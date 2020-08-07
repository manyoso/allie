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

#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QFuture>
#include <QSemaphore>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "search.h"

class Cache;
class Computation;
class Node;
class Tree;

typedef QVector<Node*> Batch;
typedef QVector<Batch*> BatchQueue;

class GuardedBatchQueue {
public:
    Batch *acquireIn();
    void releaseIn(Batch *batch);

    Batch *acquireOut();
    void releaseOut(Batch *batch);
    void stop();

    int maximumBatchSize() const { return m_maximumBatchSize; }
    void setMaximumBatchSize(int maximumBatchSize) { m_maximumBatchSize = maximumBatchSize; }

private:
    int m_maximumBatchSize = 0;
    bool m_stop = false;
    BatchQueue m_inQueue;
    BatchQueue m_outQueue;
    QMutex m_inMutex;
    QMutex m_outMutex;
    QWaitCondition m_inCondition;
    QWaitCondition m_outCondition;
};

class GPUWorker : public QThread {
    Q_OBJECT
public:
    GPUWorker(GuardedBatchQueue *queue, int maximumBatchSize,
        QObject *parent = nullptr);
    ~GPUWorker();

    void run() override;

private:
    Batch m_batchForEvaluating;
    GuardedBatchQueue *m_queue;
};

class SearchWorker : public QObject {
    Q_OBJECT
public:
    SearchWorker(QObject *parent = nullptr);
    ~SearchWorker();

    // These are thread safe
    void stopSearch();
    quint32 estimatedNodes() const { return m_estimatedNodes; }
    void setEstimatedNodes(quint32 nodes) { m_estimatedNodes = nodes; }


public Q_SLOTS:
    void startSearch(Tree *tree, int searchId, const Search &search,
        const SearchInfo &info);

Q_SIGNALS:
    void sendInfo(const SearchInfo &info, bool isPartial);
    void searchWorkerStopped();
    void requestStop(int searchId, bool);

private Q_SLOTS:
    void search();

private:
    void minimaxBatch(Batch *batch, Tree *tree);
    void waitForFetched();
    void fetchFromNN(Batch *batch, bool sync);
    void fetchAndMinimax(Batch *batch, bool sync);
    bool fillOutTree();

    // Playout methods
    bool handlePlayout(Node *playout, Cache *cache);
    bool playoutNodes(Batch *batch, bool *hardExit);
    void ensureRootAndChildrenScored();

    // Reporting info
    void processWorkerInfo();

    Search m_search;
    qint64 m_totalPlayouts;
    const Node *m_moveNode;
    QElapsedTimer m_timer;
    int m_searchId;
    int m_currentBatchSize;
    std::atomic<quint32> m_estimatedNodes;
    SearchInfo m_currentInfo;
    Tree *m_tree;
    QVector<GPUWorker*> m_gpuWorkers;
    GuardedBatchQueue m_queue;
    BatchQueue m_batchPool;
    std::atomic<bool> m_stop;
};

class WorkerThread : public QObject {
    Q_OBJECT
public:
    WorkerThread();
    ~WorkerThread();
    SearchWorker *worker;
    QThread thread;

Q_SIGNALS:
    void startWorker(Tree *tree, int searchId, const Search &search, const SearchInfo &info);
};

class SearchEngine : public QObject
{
    Q_OBJECT
public:
    SearchEngine(QObject *parent = nullptr);
    ~SearchEngine() override;

    quint32 estimatedNodes() const;
    void setEstimatedNodes(quint32 nodes);

    bool isStopped() const { return m_stop; }
    Tree *tree() const { return m_tree; }

public Q_SLOTS:
    void reset();
    void startSearch(const Search &s);
    void stopSearch();
    void searchWorkerStopped();
    void receivedSearchInfo(const SearchInfo &info, bool isPartial);
    void receivedRequestStop(quint32 searchId, bool);
    void printTree(const QVector<QString> &node, int depth, bool printPotentials) const;
    void startPonder() {}
    void stopPonder() {}

Q_SIGNALS:
    void sendInfo(const SearchInfo &info, bool isPartial);
    void requestStop(bool);

private:
    void resetSearch(const Search &search);

    Tree *m_tree;
    quint32 m_searchId;
    bool m_startedWorker;
    WorkerThread* m_worker;
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_stop;
};

#endif // SEARCHENGINE_H
