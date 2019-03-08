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

SearchWorker::SearchWorker(int id, QObject *parent)
    : QObject(parent),
      m_id(id),
      m_reachedMaxBatchSize(false),
      m_tree(nullptr),
      m_stop(false)
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

void SearchWorker::startSearch(Tree *tree)
{
    // Reset state
    m_reachedMaxBatchSize = false;
    m_tree = tree;
    m_stop = false;

    // Start the search
    search();
}

void SearchWorker::fetchBatch(const QVector<Node*> &batch,
    lczero::Network *network, Tree *tree, const WorkerInfo &info)
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

    NeuralNet::globalInstance()->releaseNetwork(network);

    Q_ASSERT(computation.positions() == batch.count());
    if (computation.positions() != batch.count()) {
        qCritical() << "NN index mismatch!";
        return;
    }

    for (int index = 0; index < batch.count(); ++index) {
        Node *node = batch.at(index);
        Q_ASSERT((node->hasPotentials()) || node->isCheckMate() || node->isStaleMate());

        {
            QMutexLocker locker(&tree->mutex);
            node->setRawQValue(-computation.qVal(index));
            if (node->hasPotentials()) {
                computation.setPVals(index, node);
            }
            if (!node->isPrefetch()) {
                node->setQValueAndPropagate();
            }
            Hash::globalInstance()->insert(node);
        }
    }

    WorkerInfo myInfo = info;
    myInfo.nodesEvaluated += batch.count();
    myInfo.numberOfBatches += 1;
    myInfo.threadId = QThread::currentThread()->objectName();
    emit sendInfo(myInfo);
}

void SearchWorker::fetchFromNN(const QVector<Node*> &nodesToFetch, const WorkerInfo &info)
{
    Q_ASSERT(!nodesToFetch.isEmpty());
    const int maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();

    if (!m_reachedMaxBatchSize && nodesToFetch.count() >= maximumBatchSize) {
        m_reachedMaxBatchSize = true;
        emit reachedMaxBatchSize();
    }

    QVector<QVector<Node*>> batches;
    int batchSize = maximumBatchSize;
    if (nodesToFetch.count() > batchSize) {
        div_t divresult;
        divresult = div(nodesToFetch.count(), batchSize);
        int buckets = divresult.quot + (divresult.rem ? 1 : 0);
        for (int i = 0; i < buckets; ++i)
            batches.append(nodesToFetch.mid(i * batchSize, batchSize));
    } else
        batches.append(nodesToFetch);

    for (QVector<Node*> batch : batches) {
        lczero::Network *network = NeuralNet::globalInstance()->acquireNetwork(); // blocks
        Q_ASSERT(network);
        std::function<void()> fetchBatch = std::bind(&SearchWorker::fetchBatch, this,
            batch, network, m_tree, info);
        m_futures.append(QtConcurrent::run(fetchBatch));
    }
}

bool SearchWorker::fillOutTree()
{
    const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt();
    const int maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();
    const int maxSize = (numberOfGPUCores * maximumBatchSize);

    // Scale the fetchSize by depth
    const int fetchSize = maxSize;

    bool didWork = false;
    WorkerInfo info;
    QVector<Node*> playouts = playoutNodesMCTS(fetchSize, &didWork, &info);
    if (!playouts.isEmpty())
        fetchFromNN(playouts, info);
    else if (didWork)
        emit sendInfo(info);
    return didWork;
}

bool SearchWorker::handlePlayout(Node *playout, int depth, WorkerInfo *info)
{
    info->nodesSearched += 1;
    info->nodesSearchedTotal += playout->m_virtualLoss;
    info->sumDepths += depth * int(playout->m_virtualLoss);
    info->maxDepth = qMax(info->maxDepth, depth);

#if defined(DEBUG_PLAYOUT_MCTS) || defined(DEBUG_PLAYOUT_AB)
    qDebug() << "adding regular playout" << playout->toString() << "depth" << depth;
#endif

    // If we *re-encounter* an exact node that overrides the NN (checkmate/stalemate/drawish...)
    // then let's just *reset* (which is noop since it is exact) the value, increment and propagate
    // which is *not* noop
    if (playout->isExact()) {
#if defined(DEBUG_PLAYOUT_MCTS)
        qDebug() << "adding exact playout" << playout->toString();
#endif
        QMutexLocker locker(&m_tree->mutex);
        playout->setQValueAndPropagate();
        return false;
    }

    // If we encounter a playout that already has a rawQValue perhaps from a resumed search,
    // or from a prefetch, then all we need to do is back propagate the value and continue
    if (playout->hasRawQValue()) {
#if defined(DEBUG_PLAYOUT_MCTS)
        qDebug() << "found resumed playout" << playout->toString();
#endif
        info->nodesCacheHits += 1;
        QMutexLocker locker(&m_tree->mutex);
        playout->setPrefetch(false);
        playout->setQValueAndPropagate();
        return false;
    }

    // Generate potential moves of the node if possible
    m_tree->mutex.lock();
    const bool isTbHit = playout->generatePotentials();
    if (isTbHit)
        info->nodesTBHits += 1;
    m_tree->mutex.unlock();

    // If we *newly* discovered a playout that can override the NN (checkmate/stalemate/drawish...),
    // then let's just set the value and propagate
    if (playout->isExact()) {
#if defined(DEBUG_PLAYOUT_MCTS)
        qDebug() << "adding exact playout 2" << playout->toString();
#endif
        QMutexLocker locker(&m_tree->mutex);
        playout->setQValueAndPropagate();
        return false;
    }

    // If this playout is in cache, retrieve the values and back propagate and continue
    if (Hash::globalInstance()->contains(playout)) {
#if defined(DEBUG_PLAYOUT_MCTS)
        qDebug() << "found cached playout" << playout->toString();
#endif
        info->nodesCacheHits += 1;
        QMutexLocker locker(&m_tree->mutex);
        Hash::globalInstance()->fillOut(playout);
        playout->setQValueAndPropagate();
        return false;
    }

    return true; // Otherwise we should fetch from NN
}

QVector<Node*> SearchWorker::playoutNodesMCTS(int size, bool *didWork, WorkerInfo *info)
{
#if defined(DEBUG_PLAYOUT_MCTS)
    qDebug() << "begin MCTS playout filling" << size;
#endif

    int exactOrCached = 0;
    QVector<Node*> nodes;
    while (nodes.count() < size && exactOrCached < size) {
        int depth = 0;

        m_tree->mutex.lock();
        bool createdNode = false;
        Node *playout = m_tree->root->playout(&depth, &createdNode);
        if (createdNode)
            info->nodesCreated += 1;
        m_tree->mutex.unlock();

        if (!playout)
            break;

        *didWork = true;

        bool shouldFetchFromNN = handlePlayout(playout, depth, info);
        if (!shouldFetchFromNN) {
            ++exactOrCached;
            continue;
        }

        Q_ASSERT(!nodes.contains(playout));
        Q_ASSERT(!playout->hasQValue());
        nodes.append(playout);
    }

#if defined(DEBUG_PLAYOUT_MCTS)
    qDebug() << "end MCTS playout return" << nodes.count();
#endif

    return nodes;
}

void SearchWorker::search()
{
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

void SearchWorker::printTree(int depth) const
{
    if (m_tree->root)
        qDebug().noquote() << m_tree->root->printTree(depth);
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
    m_startedWorkers(0),
    m_score(0),
    m_trendDegree(0.0f),
    m_trend(Better),
    m_stop(false)
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
    m_startedWorkers = 0;
}

void SearchEngine::reset()
{
    QMutexLocker locker(&m_mutex);
    const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt();
    const int numberOfThreads = Options::globalInstance()->option("Threads").value().toInt();
    const int numberOfSearchThreads = qMax(1, numberOfGPUCores * numberOfThreads);
    if (m_workers.count() != numberOfThreads) {
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
    for (; it != node->end<PreOrder>(); ++it)
        gc.append(*it);
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
            if (grandChild->m_game.isSamePosition(s.game) && !grandChild->isExact()) {
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

void SearchEngine::startSearch(const Search &s)
{
    QMutexLocker locker(&m_mutex);

    // Try to resume where we left off
    bool resumeSearch = tryResumeSearch(s);
    if (!resumeSearch)
        resetSearch(s);

    m_startedWorkers = 0;
    m_score = 0;
    m_trendDegree = 0.0f;
    m_trend = Better;
    m_currentInfo = SearchInfo();
    m_stop = false;

    if (m_tree->root) {
        // If we have a bestmove candidate, set it now
        if (Node *best = m_tree->root->bestChild(Node::MCTS)) {
            m_currentInfo.isResume = true;
            m_currentInfo.bestMove = Notation::moveToString(best->m_game.lastMove(), Chess::Computer);
            if (Node *ponder = best->bestChild(Node::MCTS))
                m_currentInfo.ponderMove = Notation::moveToString(ponder->m_game.lastMove(), Chess::Computer);
            emit sendInfo(m_currentInfo, true /*isPartial*/);
        }
    }

    Q_ASSERT(!m_workers.isEmpty());
    m_workers.first()->startWorker(m_tree);
    ++m_startedWorkers;
}

void SearchEngine::stopSearch()
{
    // First, change our state to stop using thread safe atomic
    m_stop = true;

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

void SearchEngine::printTree(int depth)
{
    if (m_tree->root)
        qDebug().noquote() << m_tree->root->printTree(depth);
}

void SearchEngine::receivedWorkerInfo(const WorkerInfo &info)
{
    // It is possible this could have been queued before we were asked to stop
    // so ignore if so
    if (m_stop)
        return;

    // Sum the worker infos
    m_currentInfo.workerInfo.sumDepths += info.sumDepths;
    m_currentInfo.workerInfo.maxDepth = qMax(m_currentInfo.workerInfo.maxDepth, info.maxDepth);
    m_currentInfo.workerInfo.nodesSearched += info.nodesSearched;
    m_currentInfo.workerInfo.nodesSearchedTotal += info.nodesSearchedTotal;
    m_currentInfo.workerInfo.nodesEvaluated += info.nodesEvaluated;
    m_currentInfo.workerInfo.nodesCreated += info.nodesCreated;
    m_currentInfo.workerInfo.numberOfBatches += info.numberOfBatches;
    m_currentInfo.workerInfo.nodesTBHits += info.nodesTBHits;
    m_currentInfo.workerInfo.nodesCacheHits += info.nodesCacheHits;

    // Update our depth info
    const int newDepth = m_currentInfo.workerInfo.sumDepths / qMax(1, m_currentInfo.workerInfo.nodesSearched);
    bool isPartial = newDepth <= m_currentInfo.depth;
    m_currentInfo.depth = qMax(newDepth, m_currentInfo.depth);

    // Update our seldepth info
    const int newSelDepth = m_currentInfo.workerInfo.maxDepth;
    isPartial = newSelDepth > m_currentInfo.seldepth ? false : isPartial;
    m_currentInfo.seldepth = qMax(newSelDepth, m_currentInfo.seldepth);

    // Update our node info
    m_currentInfo.nodes = m_currentInfo.workerInfo.nodesSearchedTotal;

    // Lock the tree for reading
    m_tree->mutex.lock();

    // See if root has a best child
    Node *best = m_tree->root->bestChild(Node::MCTS);
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
    if (Node *ponder = best->bestChild(Node::MCTS))
        m_currentInfo.ponderMove = Notation::moveToString(ponder->m_game.lastMove(), Chess::Computer);

    // Record a pv and score
    int pvDepth = 0;
    m_currentInfo.pv = m_tree->root->principalVariation(&pvDepth, Node::MCTS);

    float score = best->hasQValue() ? best->qValue() : -best->parent()->qValue();

    // Unlock for read
    m_tree->mutex.unlock();

    m_currentInfo.score = QString("cp %0").arg(scoreToCP(score));
    if (score > 1.0f || qFuzzyCompare(score, 1.0f))
        m_currentInfo.score = QString("mate %0").arg(qCeil(qreal(pvDepth - 1) / 2));
    else if (score < -1.0f || qFuzzyCompare(score, -1.0f))
        m_currentInfo.score = QString("mate -%0").arg(qCeil(qreal(pvDepth - 1) / 2));

    // Update our trend
    Trend t;
    if (qFuzzyCompare(score, m_score))
        t = m_trend;
    else if (score < m_score)
        t = Worse;
    else
        t = Better;

    static const float scaleScore = qAbs(cpToScore(900)); // a queen
    m_trend = t;
    m_trendDegree = qAbs(score - m_score) / scaleScore;
    m_score = score;
    m_currentInfo.trend = m_trend;
    m_currentInfo.trendDegree = m_trendDegree;

    emit sendInfo(m_currentInfo, isPartial);
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
        m_workers.at(m_startedWorkers)->startWorker(m_tree);
        ++m_startedWorkers;
    }
}
