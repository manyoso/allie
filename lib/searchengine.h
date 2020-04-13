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

private:
    bool m_stop = false;
    int m_processing = 0;
    BatchQueue m_inQueue;
    BatchQueue m_outQueue;
    QMutex m_mutex;
    QWaitCondition m_condition;
};

class GPUWorker : public QThread {
    Q_OBJECT
public:
    GPUWorker(GuardedBatchQueue *queue,
        QObject *parent = nullptr);
    ~GPUWorker();

    void run() override;

private:
    GuardedBatchQueue *m_queue;
};

class SearchWorker : public QObject {
    Q_OBJECT
public:
    SearchWorker(QObject *parent = nullptr);
    ~SearchWorker();

    // These are thread safe
    void stopSearch();
    void setEstimatedNodes(quint32 nodes) { m_estimatedNodes = nodes; }


public Q_SLOTS:
    void startSearch(Tree *tree, int searchId);

Q_SIGNALS:
    void sendInfo(const SearchInfo &info, bool isPartial);
    void searchWorkerStopped();
    void requestStop();

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
    void processWorkerInfo(const WorkerInfo &info);

    int m_searchId;
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
    void startWorker(Tree *tree, int searchId);
};

class SearchEngine : public QObject
{
    Q_OBJECT
public:
    SearchEngine(QObject *parent = nullptr);
    ~SearchEngine() override;

    void setEstimatedNodes(quint32 nodes);
    bool isStopped() const { return m_stop; }
    Tree *tree() const { return m_tree; }

public Q_SLOTS:
    void reset();
    void startSearch();
    void stopSearch();
    void searchWorkerStopped();
    void receivedSearchInfo(const SearchInfo &info, bool isPartial);
    void printTree(const QVector<QString> &node, int depth, bool printPotentials) const;
    void startPonder() {}
    void stopPonder() {}

Q_SIGNALS:
    void sendInfo(const SearchInfo &info, bool isPartial);
    void requestStop();

private:
    void resetSearch(const Search &search);
    bool tryResumeSearch(const Search &search);

    Tree *m_tree;
    int m_searchId;
    bool m_startedWorker;
    WorkerThread* m_worker;
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_stop;
};

#endif // SEARCHENGINE_H
