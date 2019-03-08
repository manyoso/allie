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

#include <QtCore>

#include "game.h"
#include "hash.h"
#include "history.h"
#include "nn.h"
#include "node.h"
#include "notation.h"
#include "searchengine.h"
#include "testgames.h"
#include "treeiterator.h"
#include "uciengine.h"

void TestGames::testBasicStructures()
{
    Square s;
    QVERIFY(!s.isValid());
    s = Square(64);
    QVERIFY(!s.isValid());

    s = Square(0, 8);
    QVERIFY(!s.isValid());

    s = Square(0, 0);
    QVERIFY(s.isValid());
    QCOMPARE(s.data(), quint8(0));

    s = Square(7, 7);
    QVERIFY(s.isValid());
    QCOMPARE(s.data(), quint8(63));

    // e4
    s = Square(4, 3);
    QVERIFY(s.isValid());
    QCOMPARE(s.file(), 4);
    QCOMPARE(s.rank(), 3);

    // reverse to e5
    s.mirror();
    QVERIFY(s.isValid());
    QCOMPARE(s.file(), 4);
    QCOMPARE(s.rank(), 4);

    // e1e4
    Move mv;
    QVERIFY(!mv.isValid());
    mv.setStart(Square(4, 1));
    mv.setEnd(Square(4, 3));
    QCOMPARE(mv.start(), Square(4, 1));
    QCOMPARE(mv.end(), Square(4, 3));
    QVERIFY(mv.isValid());

    QCOMPARE(mv.piece(), Chess::Unknown);
    mv.setPiece(Chess::Pawn);
    QCOMPARE(mv.piece(), Chess::Pawn);

    QCOMPARE(mv.promotion(), Chess::Unknown);
    mv.setPromotion(Chess::Queen);
    QCOMPARE(mv.promotion(), Chess::Queen);

    QVERIFY(!mv.isCapture());
    mv.setCapture(true);
    QVERIFY(mv.isCapture());
    mv.setCapture(false);
    QVERIFY(!mv.isCapture());

    QVERIFY(!mv.isCheck());
    mv.setCheck(true);
    QVERIFY(mv.isCheck());

    QVERIFY(!mv.isCheckMate());
    mv.setCheckMate(true);
    QVERIFY(mv.isCheckMate());

    QVERIFY(!mv.isStaleMate());
    mv.setStaleMate(true);
    QVERIFY(mv.isStaleMate());

    QVERIFY(!mv.isEnPassant());
    mv.setEnPassant(true);
    QVERIFY(mv.isEnPassant());

    QVERIFY(!mv.isCastle());
    mv.setCastle(true);
    QVERIFY(mv.isCastle());

    QVERIFY(mv.castleSide() == Chess::KingSide);
    mv.setCastleSide(Chess::QueenSide);
    QVERIFY(mv.castleSide() == Chess::QueenSide);
}

void TestGames::testSizes()
{
    QCOMPARE(sizeof(Square), ulong(1));
    QCOMPARE(sizeof(Move), ulong(4));
    QCOMPARE(sizeof(BitBoard), ulong(8));
    QCOMPARE(sizeof(PotentialNode), ulong(8));
    QCOMPARE(sizeof(Game), ulong(80));
    QCOMPARE(sizeof(Node), ulong(136));
}

void TestGames::testStartingPosition()
{
    Game g;
    QCOMPARE(g.stateOfGameToFen(), QLatin1String("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    QCOMPARE(g.activeArmy(), Chess::White);

    Node *n = new Node(nullptr, g);
    n->generatePotentials();

    QCOMPARE(n->potentials().count(), 20);

    QVector<Node*> gc;
    TreeIterator<PreOrder> it = n->begin<PreOrder>();
    for (; it != n->end<PreOrder>(); ++it)
        gc.append(*it);
    qDeleteAll(gc);
}

void TestGames::testStartingPositionBlack()
{
    Game g("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    QCOMPARE(g.stateOfGameToFen(), QLatin1String("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"));
    QCOMPARE(g.activeArmy(), Chess::Black);

    Node *n = new Node(nullptr, g);
    n->generatePotentials();

    QCOMPARE(n->potentials().count(), 20);

    QVector<Node*> gc;
    TreeIterator<PreOrder> it = n->begin<PreOrder>();
    for (; it != n->end<PreOrder>(); ++it)
        gc.append(*it);
    qDeleteAll(gc);
}

void TestGames::testSearchForMateInOne()
{
    const QLatin1String mateInOne = QLatin1String("8/8/5K2/3P3k/2P5/8/6Q1/8 w - - 12 68");
    Game g(mateInOne);
    QCOMPARE(g.stateOfGameToFen(), mateInOne);

    const QLatin1String mateInOneMoves = QLatin1String("position startpos moves d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 f8d6 g1f3 e8g8 e2e4 e6d5 e4d5 b8a6 f1e2 f8e8 e1g1 b7b6 c1g5 h7h6 g5h4 d6f4 e2d3 a6b4 d3f5 c8a6 b2b3 a6c8 g2g3 f4e5 f3e5 e8e5 g3g4 d7d6 f5c8 a8c8 a2a3 b4a6 a1a2 c8c7 d1c1 e5e8 f2f3 g7g5 h4g3 f6h7 c3e4 c7d7 h2h4 g5h4 g3e1 h7g5 a2e2 e8e5 e1c3 f7f6 c3e5 f6e5 g1h2 d7f7 f3f4 g5e4 e2e4 d8f6 c1e1 a6b8 h2g1 b8d7 f4e5 f6f1 e1f1 f7f1 g1f1 d7e5 f1g2 g8f8 g2h3 e5g6 e4e3 f8f7 e3f3 f7g7 f3f1 g6h8 h3h4 h8f7 h4h5 f7g5 f1e1 g7f8 h5h6 g5f7 h6g6 f7e5 e1e5 d6e5 g6f5 e5e4 f5e4 a7a6 a3a4 b6b5 a4b5 a6b5 c4b5 f8e7 e4e5 c5c4 b3c4 e7e8 e5e6 e8d8 e6d6 d8e8 b5b6 e8f7 b6b7 f7f6 b7b8q f6g5 d6e5 g5g4 b8b3 g4g5 b3g3 g5h5 e5f5 h5h6 g3g2 h6h7 f5e5 h7h6 e5f6 h6h5");
    UciEngine engine(this, QString());
    UCIIOHandler handler(this);
    engine.installIOHandler(&handler);

    QSignalSpy bestMoveSpy(&handler, &UCIIOHandler::receivedBestMove);
    engine.readyRead(mateInOneMoves);
    engine.readyRead(QLatin1String("go depth 2"));
    bool receivedSignal = bestMoveSpy.wait();
    if (!receivedSignal) {
        QString message = QString("Did not receive signal for %1").arg(mateInOneMoves);
        QWARN(message.toLatin1().constData());
        engine.readyRead(QLatin1String("stop"));
    }
    QVERIFY(receivedSignal);
    QVERIFY2(handler.lastBestMove() == QLatin1String("g2h3")
        || handler.lastBestMove() == QLatin1String("g2g5"), QString("Result is %1")
        .arg(handler.lastBestMove()).toLatin1().constData());
    QCOMPARE(handler.lastInfo().score, QLatin1String("mate 1"));
}

void TestGames::testThreeFold()
{
    History::globalInstance()->clear();

    QVector<QString> moves = QString("g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8").split(" ").toVector();
    Game g;
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Node n(nullptr, g);
    QVERIFY(n.isThreeFold());
}

void TestGames::testThreeFold2()
{
    History::globalInstance()->clear();

    QVector<QString> moves = QString("g1f3 d7d5 d2d4 e7e6 c1f4 f8d6 f4d6 d8d6 c2c4 g8f6 e2e3 d5c4 d1a4 c7c6 a4c4 b8d7 f1e2 d6e7 b1d2 e6e5 c4c2 e5d4 f3d4 d7e5 a2a3 c6c5 e2b5 e8f8 d4f3 e5f3 d2f3 g7g6 c2c3 f8g7 h2h4 c8g4 b5c4 a8d8 f3g5 h8f8 f2f3 g4d7 h4h5 h7h6 g5f7 f8f7 c4f7 g7f7 h5g6 f7g6 e1c1 d8e8 e3e4 h6h5 d1d2 e7e5 c3e5 e8e5 d2d6 g6f7 c1d2 f7e7 d6d3 c5c4 d3c3 d7e6 d2e3 e5b5 c3c2 b5b3 e3f4 e6f7 g2g4 h5g4 f3g4 f6d7 g4g5 d7f8 h1h6 f8g6 f4f5 b3f3 f5g4 g6e5 g4h4 f7e6 h6h7 e7d6 g5g6 e5g6 h4g5 f3g3 g5f6 g6e5 c2d2 g3d3 d2g2 e5d7 f6g5 d6e5 h7e7 d7c5 g5h6 d3b3 h6g7 c5e4 g2e2 b3g3 g7h6 e5f6 e7e6 f6e6 e2e4 e6d5 e4e7 g3b3 e7e2 c4c3 b2c3 b3a3 e2e7 b7b5 e7c7 a7a5 h6g5 a5a4 c7c8 d5e4 c8e8 e4d3 g5f5 a3b3 e8h8 a4a3 h8e8 d3c3 f5e5 c3b4 e8h8 b3b1 h8c8 a3a2 e5d4 b4a4 d4e4 b5b4 e4d3 a4a3 c8c1 b4b3 d3d2 a3a4 d2d3 a4b5 d3c3 b3b2 c1h1 a2a1q h1b1 a1b1 c3d2 b1a2 d2c3 b5c5 c3d2 b2b1q d2e3 a2a1 e3f4 a1a2 f4e5 b1g1 e5f5 g1h1 f5g6 h1g1 g6f6 g1h1 f6f5 h1h5 f5e4 a2a1 e4f4 h5h6 f4g3 h6h5 g3g2 h5h4 g2f3 a1a2 f3e3 h4h3 e3f4 h3h2 f4g5 a2a1 g5g6 h2h4 g6f7 h4h6 f7e7 h6h5 e7d8 a1b1 d8d7 b1a1 d7e7 a1a2 e7d7 a2a1 d7e7 h5h7 e7e6 h7h6 e6d7 h6h5").split(" ").toVector();
    Game g;
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Node n(nullptr, g);
    QVERIFY(n.isThreeFold());
}

void TestGames::testThreeFold3()
{
    History::globalInstance()->clear();

    QVector<QString> moves = QString("g1f3 d7d5 d2d4 g8f6 c1f4 c8f5 c2c4 e7e6 b1c3 f8b4 d1a4 b8c6 f3e5 e8g8 e5c6 b4c3 b2c3 d8d7 f2f3 h7h5 e2e3 b7c6 f1e2 f8b8 e1g1 b8b2 f1f2 h5h4 e2f1 b2f2 g1f2 a7a5 f2g1 f5g6 h2h3 f6h5 f4g5 h5g3 c4d5 g3f1 a1f1 e6d5 g5h4 d7d6 h4g5 g6d3 g5f4 d6e7 f1f2 d3b5 a4c2 a5a4 c2f5 a4a3 g1h2 b5c4 f5b1 e7d8 b1b7 a8b8 b7c6 b8b2 f2b2 a3b2 c6b7 g7g5 f4g3 c4a6 b7b2 d8e8 g3c7 e8e3 c7e5 f7f6 b2b8 g8f7 b8a7 f7e8 a7a6 f6e5 a6e6 e8f8 e6e5 e3d2 e5g3 d2c3 g3d6 f8g7 d6d5 c3c1 d5e5 g7f7 e5g3 c1d2 a2a4 d2d4 a4a5 d4f6 g3c7 f7f8 c7b8 f8f7 b8b7 f7f8 b7b4 f8f7 b4c4 f7g7 c4c7 g7g8 c7c4 g8g7 c4c7 g7g8 c7c4 g8g7").split(" ").toVector();
    Game g;
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Node n(nullptr, g);
    QVERIFY(n.isThreeFold());
}

void TestGames::testThreeFold4()
{
    History::globalInstance()->clear();

    QLatin1String fen = QLatin1String("4k3/8/8/8/8/1R6/8/4K3 b - - 0 40");
    QVector<QString> moves = QString("e8d7 e1f1 d7d6 b3b2 d6c6 b2b8 c6d6 b8b7 d6c6 b7b3 c6d7 b3a3 d7c7 a3a6 c7c8 a6a1 c8d7 f1g1 d7c6 a1a8 c6b7 a8d8 b7a7 d8d3 a7b8 d3a3 b8c7 a3a6 c7b7 a6f6 b7b8 f6f2 b8c7 f2a2 c7b6 a2a3 b6b7 a3a2 b7c7 a2a6 c7b7 a6a5 b7b8 a5a4 b8c7 a4b4 c7d7 b4b6 d7d8 b6b5 d8c7 b5b4 c7d7 b4b6 d7c7 b6f6 c7d7 f6f2 d7e8 f2a2 e8d7 a2a6 d7c7 a6a5 c7b8 a5a4 b8c7").split(" ").toVector();
    Game g(fen);
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Node n(nullptr, g);
    QVERIFY(!n.isThreeFold());
    n.generatePotentials();
    QVector<PotentialNode*> potentials = n.potentials();
    QVERIFY(!potentials.isEmpty());
    bool found = false;
    for (PotentialNode *p : potentials) {
        if (QLatin1String("a4b4") == Notation::moveToString(p->move(), Chess::Computer)) {
            found = true;
            Node *threeFold = n.generateChild(p);
            threeFold->generatePotentials();
            QVERIFY(threeFold->isThreeFold());
            delete threeFold;
        }
    }
    QVERIFY(found);
}

void TestGames::checkGame(const QString &fen, const QVector<QString> &mv)
{
    QVector<QString> moves = mv;
    UciEngine engine(this, QString());
    UCIIOHandler engineHandler(this);
    engine.installIOHandler(&engineHandler);
    QSignalSpy bestMoveSpy(&engineHandler, &UCIIOHandler::receivedBestMove);

    enum Result { CheckMate, StaleMate, HalfMoveClock, ThreeFold, DeadPosition, NoResult };
    Result r = NoResult;

    QString position;
    for (int i = 0; i < 100; ++i) {
        position = QLatin1String("position fen ") + fen;
        if (!moves.isEmpty())
            position.append(QLatin1String(" moves ") + moves.toList().join(' '));
        engine.readyRead(position);

        Game g = History::globalInstance()->currentGame();
        if (g.halfMoveClock() >= 100) {
            r = HalfMoveClock;
            break;
        }

        if (g.isDeadPosition()) {
            r = DeadPosition;
            break;
        }

        Node n(nullptr, g);
        n.generatePotentials();

        if (n.isThreeFold()) {
            r = ThreeFold;
            break;
        }

        if (n.isCheckMate()) {
            r = CheckMate;
            break;
        }

        if (n.isStaleMate()) {
            r = StaleMate;
            break;
        }

        engineHandler.clear();
        engine.readyRead(QLatin1String("go wtime 2000 btime 100"));
        bool receivedSignal = bestMoveSpy.wait();
        if (!receivedSignal) {
            QString message = QString("Did not receive signal for %1").arg(position);
            QWARN(message.toLatin1().constData());
            engine.readyRead(QLatin1String("stop"));
        }
        QString bestMove = engineHandler.lastBestMove();
        QVERIFY(!bestMove.isEmpty());
        moves.append(bestMove);
    }

    QString resultString;
    switch (r) {
    case CheckMate:
        resultString = "CheckMate"; break;
    case StaleMate:
        resultString = "StaleMate"; break;
    case HalfMoveClock:
        resultString = "HalfMoveClock"; break;
    case ThreeFold:
        resultString = "ThreeFold"; break;
    case DeadPosition:
        resultString = "DeadPosition"; break;
    case NoResult:
        resultString = "NoResult"; break;
    }
    QVERIFY2(r == CheckMate, QString("Result is %1 at %2")
        .arg(resultString).arg(position).toLatin1().constData());
}

void TestGames::testMateWithKRvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1R6/8/4K3 b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void TestGames::testMateWithKQvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1Q6/8/4K3 b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void TestGames::testMateWithKBNvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1N6/8/4K2B b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void TestGames::testMateWithKBBvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1B6/8/4K1B1 b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void TestGames::testMateWithKQQvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1Q6/8/4K2Q b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void TestGames::testHashInsertAndRetrieve()
{
    // Create a position
    Game game;
    const QString move = QLatin1String("d2d4");
    Move mv = Notation::stringToMove(move, Chess::Computer);
    QVERIFY(game.makeMove(mv));

    // Create a node
    Node *node1 = new Node(nullptr, game);
    node1->generatePotentials();
    QCOMPARE(node1->potentials().count(), 20);

    // Go to the NN for evaluation
    Computation computation;
    computation.addPositionToEvaluate(node1);
    computation.evaluate();
    QCOMPARE(computation.positions(), 1);

    // Retrieve the qVal and pVal from NN and set the values in the node
    node1->setRawQValue(-computation.qVal(0));
    computation.setPVals(0, node1);
    node1->setQValueAndPropagate();

    // Insert node1 into the hash
    Hash::globalInstance()->insert(node1);

    // Create a new node with the same position
    Node *node2 = new Node(nullptr, game);
    node2->generatePotentials();
    QCOMPARE(node2->potentials().count(), 20);

    QVERIFY(Hash::globalInstance()->contains(node2));

    // Go to the Hash to fill out
    Hash::globalInstance()->fillOut(node2);

    QCOMPARE(node1->potentials().count(), node2->potentials().count());
    QCOMPARE(node1->rawQValue(), node2->rawQValue());

    QVector<PotentialNode*> p1 = node1->potentials();
    QVector<PotentialNode*> p2 = node2->potentials();
    for (int i = 0; i < p1.count(); ++i) {
        PotentialNode *potential1 = p1.at(i);
        PotentialNode *potential2 = p2.at(i);
        QCOMPARE(potential1->move(), potential2->move());
        QCOMPARE(potential1->pValue(), potential2->pValue());
    }
}
