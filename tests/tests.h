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

//#define RUN_PERFT

class UCIIOHandler : public QObject, public IOHandler
{
    Q_OBJECT
public:
    UCIIOHandler(QObject *parent)
        : QObject(parent) {}
    virtual ~UCIIOHandler() override {}

    void handleInfo(const SearchInfo &info, bool) override
    {
        m_lastInfo = info;
        emit receivedInfo();
    }

    void handleBestMove(const QString &bestMove) override
    {
        Q_ASSERT(!bestMove.isEmpty());
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

class Tests: public QObject {
    Q_OBJECT

    // helpers go here
    static void testStart(const StandaloneGame &start);
    struct PerftResult {
        QString fen;
        int depth = 0;
        quint64 nodes = 0;
        quint64 captures = 0;
        quint64 ep = 0;
        quint64 castles = 0;
        quint64 promotions = 0;
    };
    static void perft(int depth, Node *parent, PerftResult *result);
    static void generateEmbodiedChild(Node *parent, bool onlyUniquePositions, Node **generatedChild);

private slots:
    // Tests
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // TestBasics
    void testBasicStructures();
    void testSizes();
    void testCPFormula();
    void testVLDFormula();
#if defined(RUN_PERFT)
    void testPerft();
#endif

    // TestCache
    void testBasicCache();
    void testStartingPosition();
    void testStartingPositionBlack();

    // TestGames
    void testCastlingAnd960();
    void testSearchForMateInOne();
    void testInstaMove();
    void testEarlyExit();
    void testHistory();
    void testThreeFold();
    void testThreeFold2();
    void testThreeFold3();
    void testThreeFold4();
    void testMateWithKRvK();
    void testMateWithKQvK();
    void testMateWithKBNvK();
    void testMateWithKBBvK();
    void testMateWithKQQvK();
    void testTB();
    void testDoNotPropagateDrawnAsExact();
    void testContext();

    // Placed at the end because this turns off fathom and fathom is bugged
    void testExhaustSearch();

    // TestMath
    void testFastLog();
    void testFastPow();

private:
    void checkGame(const QString &fen, const QVector<QString> &mv);
};
