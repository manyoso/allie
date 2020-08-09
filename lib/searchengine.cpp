/*
  This file is part of Allie Chess.
  Copyright (C) 2018, 2019 Adam Treat

  Allie Chess is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Allie Chess is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Allie Chess.  If not, see <http://www.gnu.org/licenses/>.

  Additional permission under GNU GPL version 3 section 7
*/

#include "searchengine.h"

#include <QtMath>

#include "cache.h"
#include "game.h"
#include "move.h"
#include "node.h"
#include "nn.h"
#include "notation.h"
#include "options.h"
#include "tree.h"

//#define DEBUG_EVAL
//#define DEBUG_VALIDATE_TREE
//#define USE_DUMMY_NODES

void actualFetchFromNN(Batch *batch)
{
    Computation *computation = NeuralNet::globalInstance()->acquireNetwork();
    Q_ASSERT(computation);
    computation->reset();
    for (int index = 0; index < batch->count(); ++index) {
        Node *node = batch->at(index);
        computation->addPositionToEvaluate(node);
    }

#if defined(DEBUG_EVAL)
    qDebug() << "fetching batch of size" << batch->count() << QThread::currentThread()->objectName();
#endif
    computation->evaluate();

    Q_ASSERT(computation->positions() == batch->count());
    if (computation->positions() != batch->count()) {
        qCritical() << "NN index mismatch!";
        return;
    }

    for (int index = 0; index < batch->count(); ++index) {
        Node *node = batch->at(index);
        Q_ASSERT(node->hasPotentials());
        node->setPositionQValue(-computation->qVal(index));
        if (node->hasPotentials()) {
            Q_ASSERT(!node->isExact());
            Q_ASSERT(node->position()->refs() == 1);
            computation->setPVals(index, node);
        }
    }
    NeuralNet::globalInstance()->releaseNetwork(computation);
}

void actualMinimaxTree(Tree *tree, WorkerInfo *info)
{
    // Gather minimax scores;
    double newScores = 0;
    quint32 newVisits = 0;
    const quint64 originalEvaluated = info->nodesEvaluated;
    Node::minimax(tree->embodiedRoot(), 0 /*depth*/, info, &newScores, &newVisits);
#if defined(DEBUG_VALIDATE_TREE)
    Node::validateTree(tree->embodiedRoot());
#endif
    info->numberOfBatches += info->nodesEvaluated > originalEvaluated ? 1 : 0;
}

void actualMinimaxBatch(Batch *batch, Tree *tree, WorkerInfo *info)
{
    for (int index = 0; index < batch->count(); ++index) {
        Node *node = batch->at(index);
        node->backPropagateDirty();
    }

    actualMinimaxTree(tree, info);
}

Batch *GuardedBatchQueue::acquireIn()
{
    QMutexLocker locker(&m_inMutex);
    while (m_inQueue.isEmpty() && !m_stop)
        m_inCondition.wait(locker.mutex());
    if (m_stop)
        return nullptr;
    return m_inQueue.takeFirst();
}

void GuardedBatchQueue::releaseIn(Batch *batch)
{
    QMutexLocker locker(&m_inMutex);
    m_inQueue.append(batch);
    m_inCondition.wakeOne();
}

Batch *GuardedBatchQueue::acquireOut()
{
    QMutexLocker locker(&m_outMutex);
    while (m_outQueue.isEmpty())
        m_outCondition.wait(locker.mutex());
    return m_outQueue.takeFirst();
}

void GuardedBatchQueue::releaseOut(Batch *batch)
{
    QMutexLocker locker(&m_outMutex);
    m_outQueue.append(batch);
    m_outCondition.wakeOne();
}

void GuardedBatchQueue::stop()
{
    QMutexLocker locker(&m_inMutex);
    m_stop = true;
    m_inCondition.wakeAll();
}

GPUWorker::GPUWorker(GuardedBatchQueue *queue, int maximumBatchSize,
    QObject *parent)
    : QThread(parent),
    m_queue(queue)
{
    m_batchForEvaluating.reserve(maximumBatchSize);
}

GPUWorker::~GPUWorker()
{
}

void GPUWorker::run()
{
    forever {
        Batch *batch = m_queue->acquireIn(); // will block until a batch is ready
        if (!batch)
            return;

        // Clear our internal queue
        m_batchForEvaluating.clear();

        // Generate potentials
        for (int index = 0; index < batch->count(); ++index) {
            Node *node = batch->at(index);
            node->generatePotentials();
            if (!node->isExact())
                m_batchForEvaluating.append(node);
        }

        actualFetchFromNN(&m_batchForEvaluating);

        for (int index = 0; index < m_batchForEvaluating.count(); ++index) {
            Node *node = m_batchForEvaluating.at(index);
            Node::sortByPVals(*node->position()->potentials());
        }

        m_queue->releaseOut(batch);
    }
}

SearchWorker::SearchWorker(QObject *parent)
    : QObject(parent),
      m_totalPlayouts(0),
      m_moveNode(nullptr),
      m_searchId(0),
      m_currentBatchSize(0),
      m_estimatedNodes(std::numeric_limits<quint32>::max()),
      m_tree(nullptr),
      m_stop(true)
{
}

SearchWorker::~SearchWorker()
{
    m_queue.stop(); // blocks and sets the queue to stop all workers
    for (GPUWorker *w : m_gpuWorkers)
        w->wait();
    qDeleteAll(m_gpuWorkers);
    m_gpuWorkers.clear();
    qDeleteAll(m_batchPool);
    m_batchPool.clear();
}

void SearchWorker::stopSearch()
{
    m_stop = true;
}

void SearchWorker::startSearch(Tree *tree, int searchId, const Search &s, const SearchInfo &info)
{
    // Reset state
    m_tree = tree;
    m_searchId = searchId;
    m_totalPlayouts = 0;
    m_search = s;
    m_currentInfo = info;
    m_currentInfo.workerInfo.searchId = searchId;
    Node *root = m_tree->embodiedRoot();
    const Node *best = root->bestChild();
    m_moveNode = best;
    m_estimatedNodes = std::numeric_limits<quint32>::max();
    m_stop = false;

    if (m_gpuWorkers.isEmpty()) {
        // Start as many gpu worker threads as we have available networks and create a batch pool
        // to satisfy those workers
        const int maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();
        const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt() * 2;
        m_queue.setMaximumBatchSize(maximumBatchSize);
        for (int i = 0; i < numberOfGPUCores; ++i) {
            GPUWorker *worker = new GPUWorker(&m_queue, maximumBatchSize);
            worker->setObjectName(QString("gpuworker %0").arg(i));
            worker->start();
            m_gpuWorkers.append(worker);
            Batch *batch = new Batch;
            batch->reserve(maximumBatchSize);
            m_batchPool.append(batch);
        }
    }

    m_currentBatchSize = m_queue.maximumBatchSize();

    // Start the info timer
    m_timer.restart();

    // Start the search
    search();
}

void SearchWorker::minimaxBatch(Batch *batch, Tree *tree)
{
    actualMinimaxBatch(batch, tree, &m_currentInfo.workerInfo);
    processWorkerInfo();
}

void SearchWorker::waitForFetched()
{
    Q_ASSERT(m_batchPool.count() != m_gpuWorkers.count());
    Batch *batch = m_queue.acquireOut(); // blocks
    Q_ASSERT(batch);
    minimaxBatch(batch, m_tree);
    m_batchPool.append(batch);
    Q_ASSERT(!m_batchPool.isEmpty());
}

void SearchWorker::fetchFromNN(Batch *batch, bool sync)
{
    Q_ASSERT(!batch->isEmpty());
    if (SearchSettings::featuresOff.testFlag(SearchSettings::Threading) || sync) {
        // Generate potentials
        Batch batchForEvaluating;
        for (int index = 0; index < batch->count(); ++index) {
            Node *node = batch->at(index);
            node->generatePotentials();
            if (!node->isExact())
                batchForEvaluating.append(node);
        }
        actualFetchFromNN(&batchForEvaluating);

        for (int index = 0; index < batchForEvaluating.count(); ++index) {
            Node *node = batchForEvaluating.at(index);
            Node::sortByPVals(*node->position()->potentials());
        }

        minimaxBatch(batch, m_tree);
    } else {
        m_queue.releaseIn(batch);
        if (m_batchPool.isEmpty())
            waitForFetched();
    }
}

bool SearchWorker::fillOutTree()
{
    Q_ASSERT(!m_batchPool.isEmpty());
    Batch *batch = m_batchPool.takeFirst();
    batch->clear();
    bool hardExit = false;
    bool didWork = playoutNodes(batch, &hardExit);
    if (batch->isEmpty() || SearchSettings::featuresOff.testFlag(SearchSettings::Threading))
        m_batchPool.append(batch);
    if (!batch->isEmpty() || didWork)
        fetchAndMinimax(batch, false /*sync*/);
    else if (!hardExit)
        waitForFetched();
    return hardExit;
}

void SearchWorker::fetchAndMinimax(Batch *batch, bool sync)
{
    if (!batch->isEmpty()) {
        fetchFromNN(batch, sync);
    } else {
        actualMinimaxTree(m_tree, &m_currentInfo.workerInfo);
        processWorkerInfo();
    }
}

bool SearchWorker::handlePlayout(Node *playout, Cache *cache)
{
#if defined(DEBUG_PLAYOUT)
    qDebug() << "adding regular playout" << playout->toString();
#endif

    // If we *re-encounter* an exact node that overrides the NN (checkmate/stalemate/drawish...)
    // then let's just *reset* (which is noop since it is exact) the value, increment and propagate
    // which is *not* noop
    if (playout->isExact()) {
#if defined(DEBUG_PLAYOUT)
        qDebug() << "adding exact playout" << playout->toString();
#endif
        playout->backPropagateDirty();
        return false;
    }

    // If we don't have a position, we must initialize it
    quint64 hash = playout->initializePosition(cache);

    // Check if we have found a draw by move clock or threefold
    if (playout->checkMoveClockOrThreefold(hash, cache)) {
        playout->backPropagateGameContextAndDirty();
        return false;
    }

    // We can go ahead and use the transposition iff it has already been scored, this is
    // thread safe because the if the position does not have visits at this time, then it
    // will have been made unique by the cache
    if (playout->position()->hasQValue()) {
        Q_ASSERT(!playout->position()->isUnique());
        playout->setType(playout->positionType());
        if (playout->type() == Node::Win)
            playout->m_game.setCheckMate(true);
        Q_ASSERT(playout->hasPotentials() || playout->isExact());
#if defined(DEBUG_PLAYOUT)
        qDebug() << "found cached playout" << playout->toString();
#endif
        if (!playout->isExact() && playout->repetitions()) {
            playout->setContext(Node::GameCycleInTree);
            playout->backPropagateGameCycleAndDirty();
        } else
            playout->backPropagateDirty();
        return false;
    }

    return true; // Otherwise we should fetch from NN
}

bool SearchWorker::playoutNodes(Batch *batch, bool *hardExit)
{
#if defined(DEBUG_PLAYOUT)
    qDebug() << "begin playout filling" << m_currentBatchSize;
#endif

    bool didWork = false;
    int exactOrCached = 0;
    int vldMax = SearchSettings::vldMax;
    int tryPlayoutLimit = SearchSettings::tryPlayoutLimit;
    Cache *hash = Cache::globalInstance();
    while (batch->count() < m_currentBatchSize) {
        // Check if the we are out of nodes
        if (hash->used() == hash->size() || m_totalPlayouts == m_search.nodes) {
            *hardExit = true;
            break;
        }

        if (exactOrCached >= m_currentBatchSize) {
            actualMinimaxTree(m_tree, &m_currentInfo.workerInfo);
            processWorkerInfo();
            exactOrCached = 0;
            // I have not seen an infinite loop here, but I guess it is theoretically possible for
            // some position in the wild, so add an extra check here just in case.
            if (m_stop)
                break;
        }

        Node *playout = Node::playout(m_tree->embodiedRoot(), &vldMax, &tryPlayoutLimit, hardExit, hash);
        Q_ASSERT(!playout || playout->m_virtualLoss == 1);
        if (!playout)
            break;

        didWork = true;
        ++m_totalPlayouts;

        bool shouldFetchFromNN = handlePlayout(playout, hash);
        if (!shouldFetchFromNN) {
            ++exactOrCached;
            continue;
        }

        Q_ASSERT(!batch->contains(playout));
        batch->append(playout);
    }

#if defined(DEBUG_PLAYOUT)
    qDebug() << "end playout return" << batch->count();
#endif

    // Dynamically adjust batchsize based on how well we are meeting the current batchsize target
    if (batch->count() < m_currentBatchSize)
        m_currentBatchSize = qMax(1, m_currentBatchSize - 1);
    else if (batch->count() == m_currentBatchSize)
        m_currentBatchSize = qMin(m_queue.maximumBatchSize(), m_currentBatchSize + 1);
    return didWork;
}

void SearchWorker::ensureRootAndChildrenScored()
{
    Cache *hash = Cache::globalInstance();

    {
        // Fetch and minimax for root
        Node *root = m_tree->embodiedRoot();
        Batch nodes;
        if (!root->m_visited) {
            root->m_virtualLoss += 1;
            bool shouldFetchFromNN = handlePlayout(root, hash);
            if (shouldFetchFromNN)
                nodes.append(root);
            ++m_totalPlayouts;
        }
        fetchAndMinimax(&nodes, true /*sync*/);
    }

    {
        // Fetch and minimax for children of root
        bool didWork = false;
        QVector<Node *> children;
        Node *root = m_tree->embodiedRoot();

        // Filter the root children if necessary
        if (!m_search.searchMoves.isEmpty()) {
            float total = 0;
            QVector<Node::Potential> *potentials = root->position()->potentials();
            QMutableVectorIterator<Node::Potential> it(*potentials);
            while (it.hasNext()) {
                Node::Potential p = it.next();
                if (!m_search.searchMoves.contains(Notation::moveToString(p.move(), Chess::Computer)))
                    it.remove();
                else
                    total += p.pValue();
            }

            // Rescale the pVals if necessary
            const float scale = 1.0f / total;
            for (int i = 0; i < potentials->size(); ++i) {
                // We get a non-const reference to the actual value and change it in place
                const Node::Potential *potential = &(*potentials)[i];
                const_cast<Node::Potential*>(potential)->setPValue(scale * potential->pValue());
            }

            // Make it unique so the position can not be reused since we are fundamentally altering
            hash->nodePositionMakeUnique(root->position()->positionHash());
        }

        for (int i = root->m_potentialIndex; i < root->m_position->potentials()->count(); ++i) {
            Node::NodeGenerationError error = Node::NoError;
            Node *child = root->generateNextChild(Cache::globalInstance(), &error);
            Q_ASSERT(child);
            child->m_virtualLoss += 1;
            children.append(child);
            didWork = true;
        }

        Batch nodes;
        for (Node *child : children) {
            bool shouldFetchFromNN = handlePlayout(child, hash);
            if (shouldFetchFromNN)
                nodes.append(child);
            ++m_totalPlayouts;
        }

        if (didWork)
            fetchAndMinimax(&nodes, true /*sync*/);
    }
}

void SearchWorker::search()
{
    ensureRootAndChildrenScored();

    // Main iteration loop
    while (!m_stop) {
        // Fill out the tree
        bool hardExit = fillOutTree();
        if (hardExit)
            emit requestStop(m_searchId, false /*isEarlyExit*/);
    }

    // Notify stop
    while (m_batchPool.count() != m_gpuWorkers.count())
        waitForFetched();

#if defined(DEBUG_VALIDATE_TREE)
    Tree::validateTree(m_tree->embodiedRoot(), nullptr);
#endif

    emit searchWorkerStopped();
}

QString mateDistanceOrScore(float score, int pvDepth, bool isCheckMate) {
    QString s = QString("cp %0").arg(scoreToCP(score));
    if (isCheckMate && score > 0)
        s = QString("mate %0").arg(qCeil(qreal(pvDepth - 1) / 2));
    else if (isCheckMate && score < 0)
        s = QString("mate -%0").arg(qCeil(qreal(pvDepth - 1) / 2));
    return s;
}

void SearchWorker::processWorkerInfo()
{
    // Update our depth info
    const quint32 newDepth = qMax(quint32(1), quint32(m_currentInfo.workerInfo.sumDepths /
        qMax(quint64(1), m_currentInfo.workerInfo.nodesVisited)));
    bool isPartial = newDepth <= m_currentInfo.depth;
    m_currentInfo.depth = qMax(newDepth, m_currentInfo.depth);

    // Update our seldepth info
    const quint32 newSelDepth = qMax(quint32(1), m_currentInfo.workerInfo.maxDepth);
    isPartial = newSelDepth > m_currentInfo.seldepth ? false : isPartial;
    m_currentInfo.seldepth = qMax(newSelDepth, m_currentInfo.seldepth);

    // Update our node info
    m_currentInfo.nodes = qMax(quint64(1), m_currentInfo.workerInfo.nodesSearched);

    // See if root has a best child
    Node *root = m_tree->embodiedRoot();
    const Node *best = root->bestChild();
    if (!best)
        return;

    // If so, record our new bestmove
    Q_ASSERT(best);
    Q_ASSERT(best->parent());
    const bool hasNewMove = best != m_moveNode;
    isPartial = hasNewMove ? false : isPartial;

    m_currentInfo.workerInfo.hasTarget = m_search.depth != -1 || m_search.nodes != -1;
    m_currentInfo.workerInfo.targetReached = (m_search.depth != -1 && m_currentInfo.depth >= m_search.depth)
        || (m_search.nodes != -1 && qint64(m_currentInfo.workerInfo.nodesVisited) >= m_search.nodes);

#if !defined(NDEBUG)
    bool rootPlayedOut = false;
#endif

    // If we've set a target, make sure that root is not completely played out, otherwise set
    // target reached flag to true
    if (m_currentInfo.workerInfo.hasTarget && !root->hasPotentials()) {
        QVector<Node*> children = *root->children();
        bool allAreExact = true;
        for (Node *node : children)
            allAreExact = node->isExact() ? allAreExact : false;
        if (allAreExact) {
            m_currentInfo.workerInfo.targetReached = true;
#if !defined(NDEBUG)
            rootPlayedOut = true;
#endif
        }
    }

#if !defined(NDEBUG)
    // Check that fixed nodes always is exactly equal to the actual number of nodes visited
    if (m_currentInfo.workerInfo.targetReached && m_search.nodes != -1)
        Q_ASSERT(rootPlayedOut || qint64(m_currentInfo.workerInfo.nodesVisited) == m_search.nodes);
#endif

    isPartial = m_currentInfo.workerInfo.targetReached ? false : isPartial;

    // Check for an early exit
    bool shouldEarlyExit = false;
    Q_ASSERT(root->hasChildren());
    const bool onlyOneLegalMove = (!root->hasPotentials() && root->children()->count() == 1);
    if (onlyOneLegalMove && m_search.searchMoves.count() != 1) {
        shouldEarlyExit = true;
        m_currentInfo.bestIsMostVisited = true;
    } else {
        QVector<Node*> children = *root->children();
        if (children.count() > 1) {
            // Sort top two by score
            std::partial_sort(children.begin(), children.begin() + 2, children.end(),
                [](const Node *a, const Node *b) {
                return Node::greaterThan(a, b);
            });
            const Node *firstChild = children.at(0);
            const Node *secondChild = children.at(1);
            const qint64 diff = qint64(firstChild->m_visited) - qint64(secondChild->m_visited);
            const bool bestIsMostVisited = diff >= 0 || qFuzzyCompare(firstChild->qValue(), secondChild->qValue());
            shouldEarlyExit = bestIsMostVisited && diff >= m_estimatedNodes * SearchSettings::earlyExitFactor;
            m_currentInfo.bestIsMostVisited = bestIsMostVisited;
        } else {
            m_currentInfo.bestIsMostVisited = true;
            isPartial = true;
        }
    }

    const quint64 msecs = m_timer.nsecsElapsed() / 1000000;
    if (!isPartial || msecs >= 2500) {
        if (hasNewMove) {
            // Record a new best move
            m_moveNode = best;
            m_currentInfo.bestMove = Notation::moveToString(best->m_game.lastMove(), Chess::Computer);

            // Record a ponder move
            if (const Node *ponder = best->bestChild())
                m_currentInfo.ponderMove = Notation::moveToString(ponder->m_game.lastMove(), Chess::Computer);
            else
                m_currentInfo.ponderMove = QString();
        }

        // Record a pv and score
        float score = best->qValue();
        int pvDepth = 0;
        bool isCheckMate = false;
        m_currentInfo.pv = QString();
        QTextStream stream(&m_currentInfo.pv);
        root->principalVariation(&pvDepth, &isCheckMate, &stream);
        m_currentInfo.score = mateDistanceOrScore(score, pvDepth, isCheckMate);
        m_timer.restart();
        emit sendInfo(m_currentInfo, isPartial);
    }

    if (!SearchSettings::featuresOff.testFlag(SearchSettings::EarlyExit) && shouldEarlyExit)
        emit requestStop(m_searchId, true /*isEarlyExit*/);
}

WorkerThread::WorkerThread()
{
    worker = new SearchWorker;
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished,
                     worker, &SearchWorker::deleteLater);
    QObject::connect(this, &WorkerThread::startWorker,
                     worker, &SearchWorker::startSearch);
}

WorkerThread::~WorkerThread()
{
    worker->stopSearch(); // thread safe using atomic
    thread.quit();
    thread.wait();
}

SearchEngine::SearchEngine(QObject *parent)
    : QObject(parent),
    m_tree(new Tree),
    m_searchId(0),
    m_startedWorker(false),
    m_worker(nullptr),
    m_stop(true)
{
    qRegisterMetaType<Search>("Search");
    qRegisterMetaType<SearchInfo>("SearchInfo");
    qRegisterMetaType<WorkerInfo>("WorkerInfo");
}

SearchEngine::~SearchEngine()
{
    delete m_tree;
    m_tree = nullptr;
    delete m_worker;
    m_worker = nullptr;
    m_searchId = 0;
    m_startedWorker = false;
}

quint32 SearchEngine::estimatedNodes() const
{
    return m_worker->worker->estimatedNodes();
}

void SearchEngine::setEstimatedNodes(quint32 nodes)
{
    if (!m_startedWorker)
        return;

    Q_ASSERT(!m_stop);
    Q_ASSERT(m_worker && m_worker->worker);
    m_worker->worker->setEstimatedNodes(nodes);
}

void SearchEngine::reset()
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(m_stop); // we should be stopped before a reset

    // Reset the tree which assumes the cache has already been reset
    m_tree->reset();

    // Reset the search worker
    delete m_worker;
    m_worker = new WorkerThread;
    m_worker->thread.setObjectName("search main");
    m_worker->thread.start();
    m_worker->thread.setPriority(QThread::TimeCriticalPriority);
    // The search stopped *has* to be direct connection as the main thread will block
    // waiting for it to ensure that we only have one search going on at a time
    connect(m_worker->worker, &SearchWorker::searchWorkerStopped,
            this, &SearchEngine::searchWorkerStopped, Qt::DirectConnection);
    connect(m_worker->worker, &SearchWorker::sendInfo,
            this, &SearchEngine::receivedSearchInfo);
    connect(m_worker->worker, &SearchWorker::requestStop,
            this, &SearchEngine::receivedRequestStop);
}

void SearchEngine::startSearch(const Search &search)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(m_stop);

    // Set the search parameters
    SearchSettings::cpuctF = Options::globalInstance()->option("CpuctF").value().toFloat();
    SearchSettings::cpuctInit = Options::globalInstance()->option("CpuctInit").value().toFloat();
    SearchSettings::cpuctBase = Options::globalInstance()->option("CpuctBase").value().toFloat();
    SearchSettings::featuresOff = SearchSettings::stringToFeatures(Options::globalInstance()->option("FeaturesOff").value());
    SearchSettings::fpuReduction = Options::globalInstance()->option("ReduceFPU").value().toFloat();
    SearchSettings::policySoftmaxTemp = Options::globalInstance()->option("PolicySoftmaxTemp").value().toFloat();
    SearchSettings::policySoftmaxTempInverse = 1 / SearchSettings::policySoftmaxTemp;
    SearchSettings::tryPlayoutLimit = Options::globalInstance()->option("TryPlayoutLimit").value().toInt();

    // Remove the old root if it exists
    m_tree->clearRoot(!SearchSettings::featuresOff.testFlag(SearchSettings::TreeReuse));

    m_startedWorker = false;
    m_stop = false;

    bool onlyLegalMove = false;

    Node *root = m_tree->embodiedRoot();
    Q_ASSERT(root);

    // Check the DTZ and if found just use it and stop the search
    int dtz = 0;
    SearchInfo info;
    if (root->checkAndGenerateDTZ(&dtz)) {
        // We found a dtz move
        const int depth = dtz;
        info.isDTZ = true;
        info.depth = depth;
        info.seldepth = depth;
        info.nodes = depth;
        info.workerInfo.nodesSearched += 1;
        info.workerInfo.nodesVisited += 1;
        info.workerInfo.nodesTBHits += 1;
        info.workerInfo.sumDepths = depth;
        info.workerInfo.maxDepth = depth;
        const Node *dtzNode = root->bestChild();
        Q_ASSERT(dtzNode);
        info.bestMove = Notation::moveToString(dtzNode->m_game.lastMove(), Chess::Computer);
        info.pv = info.bestMove;
        info.score = mateDistanceOrScore(-dtzNode->qValue(), depth + 1, dtzNode->isCheckMate());
        emit sendInfo(info, false /*isPartial*/);
        return; // We are all done
    } else if (const Node *best = root->bestChild()) {
        // If we have a bestmove candidate, set it now
        info.depth = 1;
        info.seldepth = 1;
        info.nodes = 1;
        info.isResume = true;
        info.bestMove = Notation::moveToString(best->m_game.lastMove(), Chess::Computer);
        if (const Node *ponder = best->bestChild())
            info.ponderMove = Notation::moveToString(ponder->m_game.lastMove(), Chess::Computer);
        else
            info.ponderMove = QString();
        onlyLegalMove = !root->hasPotentials() && root->children()->count() == 1;
        int pvDepth = 0;
        bool isCheckMate = false;
        info.pv = QString();
        QTextStream stream(&info.pv);
        root->principalVariation(&pvDepth, &isCheckMate, &stream);
        float score = best->qValue();
        info.score = mateDistanceOrScore(score, pvDepth, isCheckMate);
        emit sendInfo(info, !onlyLegalMove /*isPartial*/);
    }

    if (onlyLegalMove) {
        requestStop(true /*isEarlyExit*/);
    } else if (!m_stop) {
        // We check if we've already been requested to stop as the sendInfo above and a very low
        // clock might have already stopped the search before the worker can even get started
        Q_ASSERT(m_worker);
        m_worker->startWorker(m_tree, m_searchId, search, info);
        m_startedWorker = true;
    }
}

void SearchEngine::stopSearch()
{
    // First, change our state to stop using thread safe atomic
    m_stop = true;

    // Now, increment the searchId to guard against stale info
    ++m_searchId;

    if (!m_startedWorker)
        return;

    // Now lock a mutex and stop the workers until all of them signal stopped
    QMutexLocker locker(&m_mutex);
    m_worker->worker->stopSearch(); // thread safe using atomic
    while (m_startedWorker)
        m_condition.wait(locker.mutex());
}

void SearchEngine::searchWorkerStopped()
{
    QMutexLocker locker(&m_mutex);
    m_startedWorker = false;
    m_condition.wakeAll();
}

void SearchEngine::receivedSearchInfo(const SearchInfo &info, bool isPartial)
{
    // It is possible this could have been queued before we were asked to stop
    // so ignore if so
    if (m_stop || info.workerInfo.searchId != m_searchId)
        return;

    emit sendInfo(info, isPartial);
}

void SearchEngine::receivedRequestStop(quint32 searchId, bool isEarlyExit)
{
    // It is possible this could have been queued before we were asked to stop
    // so ignore if so
    if (m_stop || searchId != m_searchId)
        return;

    emit requestStop(isEarlyExit);
}
void SearchEngine::printTree(const QVector<QString> &node, int depth, bool printPotentials) const
{
    if (!m_stop) {
        qWarning() << "We can only print the tree when the search is stopped!";
        return;
    }

    const Node *n = m_tree->embodiedRoot();
    if (n) {
        if (!node.isEmpty())
            n = n->findSuccessor(node);
        if (n) {
            qDebug() << "printing" << node.toList().join(" ") << "at depth" << depth << "with potentials" << printPotentials;
            qDebug().noquote() << n->printTree(n->depth(), depth, printPotentials);
        } else {
            qDebug() << "could not find" << node.toList().join(" ") << "in tree";
        }
    }
}
