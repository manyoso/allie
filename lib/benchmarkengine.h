/*
  This file is part of Allie Chess.
  Copyright (C) 2020 Adam Treat

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

#ifndef BENCHMARKENGINE_H
#define BENCHMARKENGINE_H

#include <QObject>

#include "uciengine.h"

class UCIIOHandler : public QObject, public IOHandler
{
    Q_OBJECT
public:
    UCIIOHandler(QObject *parent)
        : QObject(parent) {}
    virtual ~UCIIOHandler() override {}

    void handleInfo(const SearchInfo &info, bool isPartial) override
    {
        m_lastInfo = info;
        emit receivedInfo(isPartial);
    }

    void handleBestMove(const QString &bestMove) override
    {
        Q_ASSERT(!bestMove.isEmpty());
        m_lastBestMove = bestMove;
        emit receivedBestMove();
    }

    void handleAverages(const SearchInfo &info) override
    {
        m_averageInfo = info;
        emit receivedAverages();
    }

    SearchInfo lastInfo() const { return m_lastInfo; }
    SearchInfo averageInfo() const { return m_averageInfo; }
    QString lastBestMove() const { return m_lastBestMove; }

    void clear()
    {
        m_lastInfo = SearchInfo();
        m_lastBestMove = QString();
        m_averageInfo = SearchInfo();
    }

Q_SIGNALS:
    void receivedInfo(bool);
    void receivedBestMove();
    void receivedAverages();

private:
    SearchInfo m_lastInfo;
    SearchInfo m_averageInfo;
    QString m_lastBestMove;
};

class BenchmarkEngine : public QObject {
    Q_OBJECT
public:
    BenchmarkEngine(QObject *parent);
    ~BenchmarkEngine();

    void run();

private Q_SLOTS:
    void reportInfo(bool);
    void runNextGame();

private:
    int m_nodes;
    int m_movetime;
    quint32 m_samples;
    UCIIOHandler *m_ioHandler;
    UciEngine *m_engine;
    quint64 m_timeAtLastProgress;
    SearchInfo m_totalInfo;
};

#endif
