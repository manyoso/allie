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
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "clock.h"
#include "node.h"
#include "search.h"

namespace lczero {
class Network;
};

class SearchWorker : public QObject {
    Q_OBJECT
public:
    SearchWorker(int id, QObject *parent = nullptr);
    ~SearchWorker();

    // This is thread safe
    void stopSearch();

public Q_SLOTS:
    void startSearch(Tree *tree);
    void search();
    void printTree(int depth) const;

Q_SIGNALS:
    void sendInfo(const WorkerInfo &info);
    void searchStopped();
    void reachedMaxBatchSize();

private:
    void resetStats();
    bool checkStop() const;
    void reportProgress();
    void fetchBatch(const QVector<Node*> &batch);
    void fetchFromNN(const QVector<Node*> &fetch);
    bool fillOutTree();

    // Playout methods
    bool handlePlayout(Node *node, int depth);

    // MCTS related methods
    QVector<Node*> playoutNodesMCTS(int size, bool *didWork);

    int m_id;
    int m_sumDepths;
    int m_maxDepth;
    int m_nodesSearched;
    int m_nodesSearchedTotal;
    int m_nodesEvaluated;
    int m_nodesCacheHits;
    int m_nodesReduced;
    int m_nodesPruned;
    int m_numberOfBatches;
    bool m_reachedMaxBatchSize;
    lczero::Network *m_network;
    Tree *m_tree;
    QMutex m_sleepMutex;
    QWaitCondition m_sleepCondition;
    std::atomic<bool> m_stop;
};

class WorkerThread : public QObject {
    Q_OBJECT
public:
    WorkerThread(int id);
    ~WorkerThread();
    SearchWorker *worker;
    QThread thread;

Q_SIGNALS:
    void startWorker(Tree *tree);
};

class SearchEngine : public QObject
{
    Q_OBJECT
public:
    SearchEngine(QObject *parent = nullptr);
    ~SearchEngine() override;

    SearchInfo currentInfo() const { return m_currentInfo; }

public Q_SLOTS:
    void reset();
    void startSearch(const Search &search);
    void stopSearch();
    void searchStopped();
    void printTree(int depth);
    void receivedWorkerInfo(const WorkerInfo &info);
    void workerReachedMaxBatchSize();
    void startPonder() {}
    void stopPonder() {}

Q_SIGNALS:
    void sendInfo(const SearchInfo &info, bool isPartial);

private:
    void gcNode(Node *node) const;
    void resetSearch(const Search &search);
    bool tryResumeSearch(const Search &search);

    Tree *m_tree;
    int m_startedWorkers;
    float m_score;
    float m_trendDegree;
    Trend m_trend;
    SearchInfo m_currentInfo;
    QVector<WorkerThread*> m_workers;
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_stop;
};

#endif // SEARCHENGINE_H
