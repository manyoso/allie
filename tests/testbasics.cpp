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

#include "cache.h"
#include "game.h"
#include "node.h"
#include "tree.h"
#include "options.h"
#include "tests.h"

void Tests::testBasicStructures()
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

void Tests::testSizes()
{
    QCOMPARE(sizeof(Square),          ulong(1));
    QCOMPARE(sizeof(Move),            ulong(4));
    QCOMPARE(sizeof(BitBoard),        ulong(8));
    QCOMPARE(sizeof(Game),            ulong(8));
    QCOMPARE(sizeof(Node::Potential), ulong(8));
    QCOMPARE(sizeof(Game::Position),  ulong(72));
    QCOMPARE(sizeof(Node),            ulong(64));
    QCOMPARE(sizeof(Node::Position),  ulong(96));
}

void Tests::testVLDFormula()
{
    {
        float second = 0.302369982f;
        float best = std::nextafter(second, 1.0f);
        float uCoeff = 18834.3008f;
        float q = -0.973092973f;
        float p = 0.00887134857f;
        int visits = 1;
        int virtualLoss = 129;
        int n = Node::virtualLossDistance(best, second, uCoeff, q, p, visits, virtualLoss);
        QVERIFY(n >= 1);
    }

    {
        float second = 1.01385188f;
        float best = std::nextafter(second, 1.0f);
        float uCoeff = 613.978821f;
        float q = -0.476382792f;
        float p = 0.019417407f;
        int visits = 1;
        int virtualLoss = 6;
        int n = Node::virtualLossDistance(best, second, uCoeff, q, p, visits, virtualLoss);
        QVERIFY(n >= 1);
    }

    {
        float second = 1.00026727f;
        float best = 1.00945294f;
        float uCoeff = 41.92011642f;
        float q = 1.00000000f;
        float p = 0.04803130f;
        int visits = 212;
        int virtualLoss = 0;
        int n = Node::virtualLossDistance(best, second, uCoeff, q, p, visits, virtualLoss);
        QVERIFY(n >= 1);
    }
}

