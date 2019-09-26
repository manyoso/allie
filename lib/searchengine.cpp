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
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>

#include "game.h"
#include "hash.h"
#include "move.h"
#include "node.h"
#include "nn.h"
#include "notation.h"
#include "options.h"
#include "treeiterator.h"

//#define DEBUG_EVAL
//#define DEBUG_VALIDATE_TREE
//#define USE_DUMMY_NODES

SearchWorker::SearchWorker(int id, QObject *parent)
    : QObject(parent),
      m_id(id),
      m_searchId(0),
      m_reachedMaxBatchSize(false),
      m_tree(nullptr),
      m_stop(true)
{
}

SearchWorker::~SearchWorker()
{
}

void SearchWorker::stopSearch()
{
    m_stop = true;
    QMutexLocker locker(&m_sleepMutex);
    m_sleepCondition.wakeAll();
}

void SearchWorker::startSearch(Tree *tree, int searchId)
{
    // Reset state
    m_searchId = searchId;
    m_reachedMaxBatchSize = false;
    m_tree = tree;
    m_stop = false;

    // Start the search
    search();
}

void SearchWorker::fetchBatch(const QVector<Node*> &batch,
    lczero::Network *network, Tree *tree, int searchId)
{
    {
        Computation computation(network);
        for (int index = 0; index < batch.count(); ++index) {
            Node *node = batch.at(index);
            computation.addPositionToEvaluate(node);
        }

#if defined(DEBUG_EVAL)
        qDebug() << "fetching batch of size" << batch.count() << QThread::currentThread()->objectName();
#endif
        computation.evaluate();

        Q_ASSERT(computation.positions() == batch.count());
        if (computation.positions() != batch.count()) {
            qCritical() << "NN index mismatch!";
            return;
        }

        for (int index = 0; index < batch.count(); ++index) {
            Node *node = batch.at(index);
            Q_ASSERT((node->hasPotentials()) || node->isCheckMate() || node->isStaleMate());
            node->setRawQValue(-computation.qVal(index));
            if (node->hasPotentials())
                computation.setPVals(index, node);
        }

        NeuralNet::globalInstance()->releaseNetwork(network);
    }

    WorkerInfo info;
    {
        QMutexLocker locker(&tree->mutex);
        for (int index = 0; index < batch.count(); ++index) {
            Node *node = batch.at(index);
            node->backPropagateDirty();
            Hash::globalInstance()->insert(node);
            node->sortByPVals(); // strictly after we insert into hash
        }

        // Gather minimax scores;
        bool isExact = false;
        Node::minimax(m_tree->root, 0 /*depth*/, &isExact, &info);
#if defined(DEBUG_VALIDATE_TREE)
        Node::validateTree(m_tree->root);
#endif
    }

    info.nodesCacheHits = info.nodesCreated - batch.count();
    info.nodesEvaluated += batch.count();
    info.numberOfBatches += 1;
    info.searchId = searchId;
    info.threadId = QThread::currentThread()->objectName();
    emit sendInfo(info);
}

void SearchWorker::fetchFromNN(const QVector<Node*> &nodesToFetch, bool sync)
{
    Q_ASSERT(!nodesToFetch.isEmpty());
    const int maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();

    if (!m_reachedMaxBatchSize && nodesToFetch.count() >= maximumBatchSize) {
        m_reachedMaxBatchSize = true;
        emit reachedMaxBatchSize();
    }

    lczero::Network *network = NeuralNet::globalInstance()->acquireNetwork(); // blocks
    Q_ASSERT(network);
    if (sync) {
        fetchBatch(nodesToFetch, network, m_tree, m_searchId);
    } else {
        std::function<void()> fetchBatch = std::bind(&SearchWorker::fetchBatch, this,
            nodesToFetch, network, m_tree, m_searchId);
        m_futures.append(QtConcurrent::run(fetchBatch));
    }
}

bool SearchWorker::fillOutTree()
{
    const int maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();
    bool didWork = false;
    QVector<Node*> playouts = playoutNodes(maximumBatchSize, &didWork);
    if (!playouts.isEmpty() || didWork)
        fetchAndMinimax(playouts, false /*sync*/);
    return didWork;
}

void SearchWorker::fetchAndMinimax(QVector<Node*> nodes, bool sync)
{
    if (!nodes.isEmpty()) {
        fetchFromNN(nodes, sync);
    } else {
        WorkerInfo info;
        {
            QMutexLocker locker(&m_tree->mutex);
            // Gather minimax scores;
            bool isExact = false;
            Node::minimax(m_tree->root, 0 /*depth*/, &isExact, &info);
#if defined(DEBUG_VALIDATE_TREE)
            Node::validateTree(m_tree->root);
#endif
        }

        info.nodesCacheHits = info.nodesCreated;
        info.searchId = m_searchId;
        info.threadId = QThread::currentThread()->objectName();
        emit sendInfo(info);
    }
}

bool SearchWorker::handlePlayout(Node *playout)
{
#if defined(DEBUG_PLAYOUT)
    qDebug() << "adding regular playout" << playout->toString();
#endif

    // If we *re-encounter* a true term node that overrides the NN (checkmate/stalemate/drawish...)
    // then let's just *reset* (which is noop since it is exact) the value, increment and propagate
    // which is *not* noop
    if (playout->isExact()) {
#if defined(DEBUG_PLAYOUT)
        qDebug() << "adding exact playout" << playout->toString();
#endif
        QMutexLocker locker(&m_tree->mutex);
        playout->backPropagateDirty();
        return false;
    }

    // Generate potential moves of the node if possible
    playout->generatePotentials();

    // If we *newly* discovered a playout that can override the NN (checkmate/stalemate/drawish...),
    // then let's just back propagate dirty
    if (playout->isExact()) {
#if defined(DEBUG_PLAYOUT)
        qDebug() << "adding exact playout 2" << playout->toString();
#endif
        QMutexLocker locker(&m_tree->mutex);
        playout->backPropagateDirty();
        return false;
    }

    // If this playout is in cache, retrieve the values and back propagate dirty (which will have
    // happened in the fill out)
    {
        QMutexLocker locker(&m_tree->mutex);
        if (Hash::globalInstance()->fillOut(playout)) {
#if defined(DEBUG_PLAYOUT)
            qDebug() << "found cached playout" << playout->toString();
#endif
            // Dirty flag gets set in fillOut above
            playout->sortByPVals(); // strictly after we filled from hash
            return false;
        }
    }

    return true; // Otherwise we should fetch from NN
}

QVector<Node*> SearchWorker::playoutNodes(int size, bool *didWork)
{
#if defined(DEBUG_PLAYOUT)
    qDebug() << "begin playout filling" << size;
#endif

    int exactOrCached = 0;
    QVector<Node*> nodes;
    int vldMax = SearchSettings::vldMax;
    int tryPlayoutLimit = SearchSettings::tryPlayoutLimit;
    while (nodes.count() < size && exactOrCached < size) {
        m_tree->mutex.lock();
#if defined(USE_DUMMY_NODES)
        static Node* currentLeaf = m_tree->root;
        Node *playout = nullptr;
        if (!currentLeaf->setScoringOrScored()) {
            playout = currentLeaf;
        } else {
            playout = new Node(currentLeaf, Game());
            playout->setScoringOrScored();
            currentLeaf->m_children.append(playout);
        }
        ++playout->m_virtualLoss;
        if (currentLeaf->m_children.count() > 20)
            currentLeaf = playout;
#else
        Node *playout = Node::playout(m_tree->root, &vldMax, &tryPlayoutLimit);
        const bool isExistingPlayout = playout && playout->m_virtualLoss > 1;
#endif

        m_tree->mutex.unlock();

        if (!playout)
            break;

        *didWork = true;

        if (isExistingPlayout) {
            ++exactOrCached;
            continue;
        }

        bool shouldFetchFromNN = handlePlayout(playout);
        if (!shouldFetchFromNN) {
            ++exactOrCached;
            continue;
        }

        exactOrCached = 0;
        Q_ASSERT(!nodes.contains(playout));
        Q_ASSERT(!playout->hasQValue());
        nodes.append(playout);
    }

#if defined(DEBUG_PLAYOUT)
    qDebug() << "end playout return" << nodes.count();
#endif

    return nodes;
}

void SearchWorker::ensureRootAndChildrenScored()
{
    Node *root = m_tree->root;
    {
        // Fetch and minimax for root
        QVector<Node*> nodes;
        if (!root->setScoringOrScored()) {
            m_tree->mutex.lock();
            root->m_virtualLoss += 1;
            m_tree->mutex.unlock();
            bool shouldFetchFromNN = handlePlayout(root);
            if (shouldFetchFromNN)
                nodes.append(root);
            fetchAndMinimax(nodes, true /*sync*/);
        }
    }

    {
        // Fetch and minimax for children of root
        m_tree->mutex.lock();
        bool didWork = false;
        QVector<Node *> children;
        QVector<PotentialNode> potentials = root->m_potentials; // copy
        for (int i = 0; i < potentials.count(); ++i) {
            PotentialNode *potential = &potentials[i];
            Node *child = root->generateChild(potential);
            child->m_virtualLoss += 1;
            child->setScoringOrScored();
            children.append(child);
            didWork = true;
        }
        m_tree->mutex.unlock();

        QVector<Node*> nodes;
        for (Node *child : children) {
            bool shouldFetchFromNN = handlePlayout(child);
            if (shouldFetchFromNN)
                nodes.append(child);
        }

        if (didWork)
            fetchAndMinimax(nodes, true /*sync*/);
    }
}

void SearchWorker::search()
{
    ensureRootAndChildrenScored();

    // Main iteration loop
    while (!m_stop) {

        // Clear out any finished futures
        QMutableVectorIterator<QFuture<void>> it(m_futures);
        while (it.hasNext()) {
            QFuture<void> f = it.next();
            if (f.isFinished())
                it.remove();
        }

        // Fill out the tree
        const bool didWork = fillOutTree();
        if (!didWork) {
#if defined(DEBUG_EVAL)
            qDebug() << QThread::currentThread()->objectName() << "sleeping";
#endif
            QMutexLocker locker(&m_sleepMutex);
            m_sleepCondition.wait(locker.mutex(), 10);
        }
    }

    // Notify stop
    for (QFuture<void> f : m_futures)
        f.waitForFinished();
    m_futures.clear();

    emit searchStopped();
}

WorkerThread::WorkerThread(int id)
{
    worker = new SearchWorker(id);
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished,
                     worker, &SearchWorker::deleteLater);
    QObject::connect(this, &WorkerThread::startWorker,
                     worker, &SearchWorker::startSearch);
    QThreadPool::globalInstance()->reserveThread();
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
    m_startedWorkers(0),
    m_estimatedNodes(std::numeric_limits<quint32>::max()),
    m_stop(true)
{
    qRegisterMetaType<Search>("Search");
    qRegisterMetaType<SearchInfo>("SearchInfo");
    qRegisterMetaType<WorkerInfo>("WorkerInfo");
}

SearchEngine::~SearchEngine()
{
    gcNode(m_tree->root);
    delete m_tree;
    m_tree = nullptr;
    qDeleteAll(m_workers);
    m_workers.clear();
    m_searchId = 0;
    m_startedWorkers = 0;
}

void SearchEngine::reset()
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(m_stop); // we should be stopped before a reset

    // Delete the old root if it exists
    if (m_tree->root) {
        gcNode(m_tree->root);
        m_tree->root = nullptr;
    }

    // Reset the search workers
    const int numberOfSearchThreads = 1;
    if (m_workers.count() != numberOfSearchThreads) {
        qDeleteAll(m_workers);
        m_workers.clear();
        for (int i = 0; i < numberOfSearchThreads; ++i) {
            WorkerThread *w = new WorkerThread(i);
            if (!i)
                w->thread.setObjectName("search main");
            else
                w->thread.setObjectName("search" + QString::number(i));
            w->thread.start();
            w->thread.setPriority(QThread::TimeCriticalPriority);
            // The search stopped *has* to be direct connection as the main thread will block
            // waiting for it to ensure that we only have one search going on at a time
            connect(w->worker, &SearchWorker::searchStopped,
                    this, &SearchEngine::searchStopped, Qt::DirectConnection);
            connect(w->worker, &SearchWorker::sendInfo,
                    this, &SearchEngine::receivedWorkerInfo);
            connect(w->worker, &SearchWorker::reachedMaxBatchSize,
                    this, &SearchEngine::workerReachedMaxBatchSize);
            m_workers.append(w);
        }
    }
}

void SearchEngine::gcNode(Node *node)
{
    if (!node) // safe to delete nullptr
        return;

    // Deletes the node and all of its children in post order traversal
    QVector<Node*> gc;
    TreeIterator<PreOrder> it = node->begin<PreOrder>();
    for (; it != node->end<PreOrder>(); ++it) {
        Q_ASSERT(!(*it)->m_isDirty);
        gc.append(*it);
    }
    qDeleteAll(gc);
}

void SearchEngine::resetSearch(const Search &s)
{
    std::function<void()> gc = std::bind(&SearchEngine::gcNode, m_tree->root);
    QtConcurrent::run(gc);
    m_tree->root = new Node(nullptr, s.game);
}

bool SearchEngine::tryResumeSearch(const Search &s)
{
    if (!m_tree->root)
        return false;

    const QVector<Node*> ch = m_tree->root->children();
    for (Node *child : ch) {
        const QVector<Node*> gch = child->children();
        for (Node *grandChild : gch) {
            if (grandChild->m_game.isSamePosition(s.game) && !grandChild->isTrueTerminal()) {
                grandChild->setAsRootNode();
                std::function<void()> gc = std::bind(&SearchEngine::gcNode, m_tree->root);
                QtConcurrent::run(gc);
                m_tree->root = grandChild;
                return true;
            }
        }
    }
    return false;
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

void SearchEngine::startSearch(const Search &s)
{
    QMutexLocker locker(&m_mutex);

    // Try to resume where we left off
    bool resumeSearch = tryResumeSearch(s);
    if (!resumeSearch)
        resetSearch(s);

    // Set the search parameters
    SearchSettings::cpuctF = Options::globalInstance()->option("CpuctF").value().toFloat();
    SearchSettings::cpuctInit = Options::globalInstance()->option("CpuctInit").value().toFloat();
    SearchSettings::cpuctBase = Options::globalInstance()->option("CpuctBase").value().toFloat();

    m_startedWorkers = 0;
    m_currentInfo = SearchInfo();
    m_stop = false;
    m_estimatedNodes = std::numeric_limits<quint32>::max();

    bool onlyLegalMove = false;

    if (m_tree->root) {
        // Check the DTZ and if found just use it and stop the search
        int dtz = 0;
        if (m_tree->root->checkAndGenerateDTZ(&dtz)) {
            // We found a dtz move
            const int depth = dtz;
            m_currentInfo.isDTZ = true;
            m_currentInfo.depth = depth;
            m_currentInfo.seldepth = depth;
            m_currentInfo.nodes = depth;
            m_currentInfo.workerInfo.nodesSearched += 1;
            m_currentInfo.workerInfo.nodesTBHits += 1;
            m_currentInfo.workerInfo.sumDepths = depth;
            m_currentInfo.workerInfo.maxDepth = depth;
            Node *dtzNode = m_tree->root->leftChild();
            Q_ASSERT(dtzNode);
            m_currentInfo.bestMove = Notation::moveToString(dtzNode->m_game.lastMove(), Chess::Computer);
            m_currentInfo.pv = m_currentInfo.bestMove;
            m_currentInfo.score = mateDistanceOrScore(-dtzNode->qValue(), depth + 1, true /*isTB*/);
            emit sendInfo(m_currentInfo, false /*isPartial*/);
            return; // We are all done
        } else if (Node *best = m_tree->root->bestChild()) {
            // If we have a bestmove candidate, set it now
            m_currentInfo.isResume = true;
            m_currentInfo.bestMove = Notation::moveToString(best->m_game.lastMove(), Chess::Computer);
            if (Node *ponder = best->bestChild())
                m_currentInfo.ponderMove = Notation::moveToString(ponder->m_game.lastMove(), Chess::Computer);
            else
                m_currentInfo.ponderMove = QString();
            onlyLegalMove = !m_tree->root->hasPotentials() && m_tree->root->children().count() == 1;
            int pvDepth = 0;
            bool isTB = false;
            m_currentInfo.pv = m_tree->root->principalVariation(&pvDepth, &isTB);
            float score = best->hasQValue() ? best->qValue() : -best->parent()->qValue();
            m_currentInfo.score = mateDistanceOrScore(score, pvDepth, isTB);
            emit sendInfo(m_currentInfo, !onlyLegalMove /*isPartial*/);
        }
    }

    if (onlyLegalMove) {
        requestStop();
    } else {
        Q_ASSERT(!m_workers.isEmpty());
        m_workers.first()->startWorker(m_tree, m_searchId);
        ++m_startedWorkers;
    }
}

void SearchEngine::stopSearch()
{
    // First, change our state to stop using thread safe atomic
    m_stop = true;

    // Now, increment the searchId to guard against stale info
    ++m_searchId;

    if (!m_startedWorkers)
        return;

    // Now lock a mutex and stop the workers until all of them signal stopped
    QMutexLocker locker(&m_mutex);
    for (WorkerThread *w : m_workers)
        w->worker->stopSearch(); // thread safe using atomic
    while (m_startedWorkers)
        m_condition.wait(locker.mutex());
}

void SearchEngine::searchStopped()
{
    QMutexLocker locker(&m_mutex);
    --m_startedWorkers;
    m_condition.wakeAll();
}

void SearchEngine::printTree(const QVector<QString> &node, int depth, bool printPotentials)
{
    m_tree->mutex.lock();
    if (m_tree->root) {
        Node *n = m_tree->root;
        if (!node.isEmpty())
            n = n->findChild(node);

        if (n) {
            qDebug() << "printing" << node.toList().join(" ") << "at depth" << depth << "with potentials" << printPotentials;
            qDebug().noquote() << n->printTree(n->depth(), depth, printPotentials);
        } else {
            qDebug() << "could not find" << node.toList().join(" ") << "in tree";
        }
    }
    m_tree->mutex.unlock();
}

void SearchEngine::receivedWorkerInfo(const WorkerInfo &info)
{
    // It is possible this could have been queued before we were asked to stop
    // so ignore if so
    if (m_stop || info.searchId != m_searchId)
        return;

    // Sum the worker infos
    m_currentInfo.workerInfo.sumDepths += info.sumDepths;
    m_currentInfo.workerInfo.maxDepth = qMax(m_currentInfo.workerInfo.maxDepth, info.maxDepth);
    m_currentInfo.workerInfo.nodesSearched += info.nodesSearched;
    m_currentInfo.workerInfo.nodesEvaluated += info.nodesEvaluated;
    m_currentInfo.workerInfo.nodesCreated += info.nodesCreated;
    m_currentInfo.workerInfo.numberOfBatches += info.numberOfBatches;
    m_currentInfo.workerInfo.nodesTBHits += info.nodesTBHits;
    m_currentInfo.workerInfo.nodesCacheHits += info.nodesCacheHits;

    // Update our depth info
    const int newDepth = m_currentInfo.workerInfo.sumDepths / qMax(1, m_currentInfo.workerInfo.nodesCreated);
    bool isPartial = newDepth <= m_currentInfo.depth;
    m_currentInfo.depth = qMax(newDepth, m_currentInfo.depth);

    // Update our seldepth info
    const int newSelDepth = m_currentInfo.workerInfo.maxDepth;
    isPartial = newSelDepth > m_currentInfo.seldepth ? false : isPartial;
    m_currentInfo.seldepth = qMax(newSelDepth, m_currentInfo.seldepth);

    // Update our node info
    m_currentInfo.nodes = m_currentInfo.workerInfo.nodesSearched;

    // Lock the tree for reading
    m_tree->mutex.lock();

    // See if root has a best child
    Node *best = m_tree->root->bestChild();
    if (!best) {
        m_tree->mutex.unlock();
        return;
    }

    // If so, record our new bestmove
    Q_ASSERT(best);
    const QString newBestMove = Notation::moveToString(best->m_game.lastMove(), Chess::Computer);
    isPartial = newBestMove != m_currentInfo.bestMove ? false : isPartial;
    m_currentInfo.bestMove = newBestMove;

    // Record a ponder move
    if (Node *ponder = best->bestChild())
        m_currentInfo.ponderMove = Notation::moveToString(ponder->m_game.lastMove(), Chess::Computer);
    else
        m_currentInfo.ponderMove = QString();

    // Record a pv and score
    int pvDepth = 0;
    bool isTB = false;
    m_currentInfo.pv = m_tree->root->principalVariation(&pvDepth, &isTB);

    float score = best->hasQValue() ? best->qValue() : -best->parent()->qValue();

    // Check for an early exit
    bool shouldEarlyExit = false;
    Q_ASSERT(m_tree->root->hasChildren());
    const bool onlyOneLegalMove = (!m_tree->root->hasPotentials() && m_tree->root->children().count() == 1);
    if (onlyOneLegalMove) {
        shouldEarlyExit = true;
        m_currentInfo.bestIsMostVisited = true;
    } else if (m_tree->root->children().count() > 1) {
        QPair<Node*, Node*> topTwoChildren = m_tree->root->topTwoChildren();
        const qint64 diff = qint64(topTwoChildren.first->m_visited) - qint64(topTwoChildren.second->m_visited);
        const bool bestIsMostVisited = diff >= 0 || qFuzzyCompare(topTwoChildren.first->qValue(), topTwoChildren.second->qValue());
        shouldEarlyExit = bestIsMostVisited && diff >= m_estimatedNodes;
        m_currentInfo.bestIsMostVisited = bestIsMostVisited;
    } else {
        m_currentInfo.bestIsMostVisited = true;
        Q_UNREACHABLE();
    }

    // Unlock for read
    m_tree->mutex.unlock();

    m_currentInfo.score = mateDistanceOrScore(score, pvDepth, isTB);

    emit sendInfo(m_currentInfo, isPartial);
    if (shouldEarlyExit)
        emit requestStop();
}

void SearchEngine::workerReachedMaxBatchSize()
{
    QMutexLocker locker(&m_mutex);
    // It is possible this could have been queued before we were asked to stop
    // so ignore if so
    if (m_stop)
        return;

    // Try and start another worker if we have any
    if (m_startedWorkers < m_workers.count()) {
#if defined(DEBUG_EVAL)
        qDebug() << "Starting worker" << m_startedWorkers;
#endif
        m_workers.at(m_startedWorkers)->startWorker(m_tree, m_searchId);
        ++m_startedWorkers;
    }
}
