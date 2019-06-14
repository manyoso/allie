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
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "search.h"

namespace lczero {
class Network;
};

class Node;
class Tree;
class SearchWorker : public QObject {
    Q_OBJECT
public:
    SearchWorker(int id, QObject *parent = nullptr);
    ~SearchWorker();

    // This is thread safe
    void stopSearch();

public Q_SLOTS:
    void startSearch(Tree *tree, int searchId);

Q_SIGNALS:
    void sendInfo(const WorkerInfo &info);
    void searchStopped();
    void reachedMaxBatchSize();
    void requestStop();

private Q_SLOTS:
    void search();

private:
    void fetchBatch(const QVector<quint64> &batch,
        lczero::Network *network, Tree *tree, int searchId);
    void fetchFromNN(const QVector<quint64> &fetch, bool sync);
    void fetchAndMinimax(QVector<quint64> nodes, bool sync);
    bool fillOutTree(bool *hardExit);

    // Playout methods
    bool handlePlayout(Node *node);
    QVector<quint64> playoutNodes(int size, bool *didWork, bool *hardExit);
    void ensureRootAndChildrenScored();

    int m_id;
    int m_searchId;
    bool m_reachedMaxBatchSize;
    Tree *m_tree;
    QVector<QFuture<void>> m_futures;
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
    void startWorker(Tree *tree, int searchId);
};

class SearchEngine : public QObject
{
    Q_OBJECT
public:
    SearchEngine(QObject *parent = nullptr);
    ~SearchEngine() override;

    SearchInfo currentInfo() const { return m_currentInfo; }

    quint32 estimatedNodes() const { return m_estimatedNodes; }
    void setEstimatedNodes(quint32 nodes) { m_estimatedNodes = nodes; }

    bool isStopped() const { return m_stop; }

public Q_SLOTS:
    void reset();
    void startSearch();
    void stopSearch();
    void searchStopped();
    void printTree(const QVector<QString> &node, int depth, bool printPotentials, bool lock = true) const;
    void receivedWorkerInfo(const WorkerInfo &info);
    void workerReachedMaxBatchSize();
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
    int m_startedWorkers;
    quint32 m_estimatedNodes;
    SearchInfo m_currentInfo;
    QVector<WorkerThread*> m_workers;
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_stop;
};

#endif // SEARCHENGINE_H
