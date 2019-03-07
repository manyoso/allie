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

#include <QtTest/QtTest>

#include "uciengine.h"

class UCIIOHandler : public QObject, public IOHandler
{
    Q_OBJECT
public:
    UCIIOHandler(QObject *parent)
        : QObject(parent) {}
    virtual ~UCIIOHandler() override {}

    void handleInfo(const SearchInfo &info) override
    {
        m_lastInfo = info;
        emit receivedInfo();
    }

    void handleBestMove(const QString &bestMove) override
    {
        m_lastBestMove = bestMove;
        emit receivedBestMove();
    }

    SearchInfo lastInfo() const { return m_lastInfo; }
    QString lastBestMove() const { return m_lastBestMove; }

    void clear()
    {
        m_lastInfo = SearchInfo();
        m_lastBestMove = QString();
    }

Q_SIGNALS:
    void receivedInfo();
    void receivedBestMove();

private:
    SearchInfo m_lastInfo;
    QString m_lastBestMove;
};

class TestGames: public QObject {
    Q_OBJECT
private slots:
    void testBasicStructures();
    void testSizes();
    void testStartingPosition();
    void testStartingPositionBlack();
    void testSearchForMateInOne();
    void testThreeFold();
    void testThreeFold2();
    void testThreeFold3();
    void testThreeFold4();
    void testMateWithKRvK();
    void testMateWithKQvK();
    void testMateWithKBNvK();
    void testMateWithKBBvK();
    void testMateWithKQQvK();
    void testHashInsertAndRetrieve();

private:
    void checkGame(const QString &fen, const QVector<QString> &mv);
};
