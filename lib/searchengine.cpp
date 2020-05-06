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
        node->setRawQValue(-computation->qVal(index));
        if (node->hasPotentials()) {
            Q_ASSERT(!node->isExact());
            Q_ASSERT(!node->isTransposition());
            computation->setPVals(index, node);
        }
    }
    NeuralNet::globalInstance()->releaseNetwork(computation);
}

void actualMinimaxTree(Tree *tree, int evaluatedCount, WorkerInfo *info)
{
    // Gather minimax scores;
    bool isExact = false;
    double newScores = 0;
    quint32 newVisits = 0;
    quint32 currentVisited = info->nodesVisited;
    Node::minimax(tree->embodiedRoot(), 0 /*depth*/, &isExact, info, &newScores, &newVisits);
#if defined(DEBUG_VALIDATE_TREE)
    Node::validateTree(tree->embodiedRoot());
#endif

    info->nodesCacheHits += currentVisited - evaluatedCount;
    info->nodesEvaluated += evaluatedCount;
    info->numberOfBatches += evaluatedCount ? 1 : 0;
}

void actualMinimaxBatch(Batch *batch, Tree *tree, WorkerInfo *info)
{
    int countNonExact = 0;
    for (int index = 0; index < batch->count(); ++index) {
        Node *node = batch->at(index);
        if (!node->isExact()) {
            Node::sortByPVals(*node->position()->potentials());
            ++countNonExact;
        }
        node->backPropagateDirty();
    }

    actualMinimaxTree(tree, countNonExact, info);
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
        m_queue->releaseOut(batch);
    }
}

SearchWorker::SearchWorker(QObject *parent)
    : QObject(parent),
      m_depthTargeted(-1),
      m_nodesTargeted(-1),
      m_moveNode(nullptr),
      m_searchId(0),
      m_maximumBatchSize(0),
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

void SearchWorker::startSearch(Tree *tree, int searchId, qint64 depthTargeted, qint64 nodesTargeted,
    const SearchInfo &info)
{
    // Reset state
    m_tree = tree;
    m_searchId = searchId;
    m_maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();
    m_currentBatchSize = m_maximumBatchSize;
    m_depthTargeted = depthTargeted;
    m_nodesTargeted = nodesTargeted;
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
        const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt() * 2;
        for (int i = 0; i < numberOfGPUCores; ++i) {
            GPUWorker *worker = new GPUWorker(&m_queue, m_maximumBatchSize);
            worker->setObjectName(QString("gpuworker %0").arg(i));
            worker->start();
            m_gpuWorkers.append(worker);
            Batch *batch = new Batch;
            batch->reserve(m_maximumBatchSize);
            m_batchPool.append(batch);
        }
    }

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
        actualMinimaxTree(m_tree, 0 /*evaluatedCount*/, &m_currentInfo.workerInfo);
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
    if (playout->checkMoveClockOrThreefold()) {
        // This can never be a transposition as it depends upon information not found in the
        // generic position, but rather depends upon game specific context
        cache->nodePositionMakeUnique(hash);
        playout->backPropagateDirty();
        return false;
    }

    // Check if we can find a transposition now
    {
        const Node *canonicalNode = playout->position()->canonicalNode();
        Q_ASSERT(canonicalNode);
        Q_ASSERT(canonicalNode->position() == playout->position());
        if (!SearchSettings::featuresOff.testFlag(SearchSettings::Transpositions)) {
            // If we are using another canonical node, then it *must* already have a qValue or it
            // would have been made unique when this playout first got its position
            Q_ASSERT(canonicalNode == playout || canonicalNode->visits());

            // We can go ahead and use the transposition iff it is not this playout OR if it is this
            // playout AND it already has legit scored other than for FPU
            if ((canonicalNode != playout) ||
                (canonicalNode == playout && playout->position()->hasRawQValue())) {
#if defined(DEBUG_PLAYOUT)
                qDebug() << "found cached playout" << playout->toString();
#endif
                // It is possible this is a transposition of a terminal node, but we only know that
                // if the position has no potentials, but we don't mark whether this is a checkmate
                // or a stalemate or TB drawn or deadPosition or what
                playout->m_isExact = !playout->hasPotentials();
                playout->backPropagateDirty();
                return false;
            }
        }
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
        if (hash->used() == hash->size()) {
            *hardExit = true;
            break;
        }

        if (exactOrCached >= m_currentBatchSize) {
            actualMinimaxTree(m_tree, 0 /*evaluatedCount*/, &m_currentInfo.workerInfo);
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
        m_currentBatchSize = qMin(m_maximumBatchSize, m_currentBatchSize + 1);
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
        }
        fetchAndMinimax(&nodes, true /*sync*/);
    }

    {
        // Fetch and minimax for children of root
        bool didWork = false;
        QVector<Node *> children;
        Node *root = m_tree->embodiedRoot();
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

QString mateDistanceOrScore(float score, int pvDepth, bool isTB) {
    QString s = QString("cp %0").arg(scoreToCP(score));
    if (isTB)
        return s;
    if (score > 1.0f || qFuzzyCompare(score, 1.0f))
        s = QString("mate %0").arg(qCeil(qreal(pvDepth - 1) / 2));
    else if (score < -1.0f || qFuzzyCompare(score, -1.0f))
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

    m_currentInfo.workerInfo.hasTarget = m_depthTargeted != -1 || m_nodesTargeted != -1;
    m_currentInfo.workerInfo.targetReached = (m_depthTargeted != -1 && m_currentInfo.depth >= m_depthTargeted)
        || (m_nodesTargeted != -1 && qint64(m_currentInfo.workerInfo.nodesVisited) >= m_nodesTargeted);
    isPartial = m_currentInfo.workerInfo.targetReached ? false : isPartial;

    // Check for an early exit
    bool shouldEarlyExit = false;
    Q_ASSERT(root->hasChildren());
    const bool onlyOneLegalMove = (!root->hasPotentials() && root->children()->count() == 1);
    if (onlyOneLegalMove) {
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
        bool isTB = false;
        m_currentInfo.pv = QString();
        QTextStream stream(&m_currentInfo.pv);
        root->principalVariation(&pvDepth, &isTB, &stream);
        m_currentInfo.score = mateDistanceOrScore(score, pvDepth, isTB);
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

void SearchEngine::startSearch(qint64 depthTargeted, qint64 nodesTargeted)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(m_stop);

    // Remove the old root if it exists
    m_tree->clearRoot();

    // Set the search parameters
    SearchSettings::cpuctF = Options::globalInstance()->option("CpuctF").value().toFloat();
    SearchSettings::cpuctInit = Options::globalInstance()->option("CpuctInit").value().toFloat();
    SearchSettings::cpuctBase = Options::globalInstance()->option("CpuctBase").value().toFloat();
    SearchSettings::tryPlayoutLimit = Options::globalInstance()->option("TryPlayoutLimit").value().toInt();
    SearchSettings::featuresOff = SearchSettings::stringToFeatures(Options::globalInstance()->option("FeaturesOff").value());

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
        info.score = mateDistanceOrScore(-dtzNode->qValue(), depth + 1, true /*isTB*/);
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
        bool isTB = false;
        info.pv = QString();
        QTextStream stream(&info.pv);
        root->principalVariation(&pvDepth, &isTB, &stream);
        float score = best->qValue();
        info.score = mateDistanceOrScore(score, pvDepth, isTB);
        emit sendInfo(info, !onlyLegalMove /*isPartial*/);
    }

    if (onlyLegalMove) {
        requestStop(true /*isEarlyExit*/);
    } else if (!m_stop) {
        // We check if we've already been requested to stop as the sendInfo above and a very low
        // clock might have already stopped the search before the worker can even get started
        Q_ASSERT(m_worker);
        m_worker->startWorker(m_tree, m_searchId, depthTargeted, nodesTargeted, info);
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
