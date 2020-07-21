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
#include "history.h"
#include "nn.h"
#include "node.h"
#include "notation.h"
#include "options.h"
#include "searchengine.h"
#include "tests.h"
#include "tree.h"
#include "uciengine.h"

void Tests::testCastlingAnd960()
{
    // begin regular positions
    {
        // Don't take away castle rights just because rook was taken
        const QLatin1String fen = QLatin1String("r3kr2/1pqb3n/p2pp1p1/4b1P1/3NP3/2N4Q/PPP1B3/2KR1R2 b q - 1 20");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        g.makeMove(Notation::stringToMove("Rxf1"));
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }


    {
        // Don't take away castle rights just because rook was taken on same rank as castling rook
        const QLatin1String fen = QLatin1String("rnb1kbnr/ppp1pppp/3q4/3p4/7P/R7/PPPPPPP1/RNBQKBN1 b Qkq - 2 3");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        g.makeMove(Notation::stringToMove("Qxa3"));
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    {
        // White rook is attacked so cannot castle
        const QLatin1String fen = QLatin1String("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q2/PPPBBPpP/1R2K2R w Kkq - 0 2");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    {
        // Black king is in check so cannot castle
        const QLatin1String fen = QLatin1String("r3k2r/8/8/1Q6/8/8/8/4K3 b kq - 0 1");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    {
        // Black can castle as check was removed
        const QLatin1String fen = QLatin1String("r3k2r/8/8/8/8/8/8/4K3 b kq - 0 1");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    {
        // White can not castle kingside as it would move through check, but can castle queenside
        // even though the rook is attacked as king does not move through check
        const QLatin1String fen = QLatin1String("4k3/6q1/8/8/8/8/8/R3K2R w KQ - 0 1");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    {
        // White can not castle either side as king would move through check
        const QLatin1String fen = QLatin1String("4k3/8/8/8/6q1/8/8/R3K2R w KQ - 0 1");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // begin 960 positions

    Options::globalInstance()->setOption("UCI_Chess960", QLatin1Literal("true"));
    SearchSettings::chess960 = true;

    // check fen
    {
        const QLatin1String fen = QLatin1String("qrknbbrn/pppppppp/8/8/8/8/PPPPPPPP/QRKNBBRN w KQkq - 0 1");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // white to castle queenside
    {
        const QLatin1String fen = QLatin1String("r3k2r/pppqbpp1/2n1pnp1/3p2B1/3P1PP1/2P1P3/PPQ2N1P/R3KB1R w KQkq - 3 14");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // black to castle queenside
    {
        const QLatin1String fen = QLatin1String("r3k2r/pppqbpp1/2n1pnp1/3p2B1/3P1PP1/2P1P3/PPQ2N1P/2KR1B1R b kq - 4 14");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // white to castle kingside
    {
        const QLatin1String fen = QLatin1String("rn2k1r1/ppp1pp1p/3p2p1/5bn1/P7/2N2B2/1PPPPP2/2BNK1RR w Gkq - 4 11");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // black to castle kingside
    {
        const QLatin1String fen = QLatin1String("qrkr4/ppp1bppb/4pnnp/8/2PP4/2NB1P2/PP1R2PP/QRK1N1B1 b Qkq - 0 10");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // white to castle queenside
    {
        const QLatin1String fen = QLatin1String("qr3rk1/2p1bppb/pp2pnnp/8/P1PP4/2NB1P2/1PNR2PP/QRK3B1 w Q - 0 13");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // white to castle queenside
    {
        const QLatin1String fen = QLatin1String("1k1q1r1b/1p1n3p/r1np2p1/p1p1P3/2P2Pb1/P2N1N2/1PQ2B1P/RK2R2B w Qk - 0 16");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // white to castle kingside
    {
        const QLatin1String fen = QLatin1String("2rkqr1n/Qp1p2pp/8/4bp2/2bB4/8/PP2P1PP/N1RK1R1N w KQkq - 0 10");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // black to castle kingside
    {
        const QLatin1String fen = QLatin1String("rb2bkr1/pp1qpppp/1n1p2n1/8/2PNB3/1Q4N1/PP2PPPP/R3BKR1 b KQkq - 4 7");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // white to castle kingside
    {
        const QLatin1String fen = QLatin1String("2r3k1/pp2p1p1/1n4np/5p2/3R4/1bB2NP1/1P2PPP1/5KR1 w K - 0 20");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::White);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));
    }

    // black to castle kingside
    {
        const QLatin1String fen = QLatin1String("1b1rqk1r/ppnpp1pp/2pn4/4Np2/2bP4/1NP2P2/PP2P1PP/1B1RQKBR b KQkq - 4 7");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));

        History::globalInstance()->addGame(g);
        Tree tree;
        Node *root = tree.embodiedRoot();
        QVERIFY(root);
        QCOMPARE(nullptr, root->parent());
        QCOMPARE(g.position().positionHash(), root->position()->positionHash());
        root->generatePotentials();

        // Make sure this is encoded as king captures rook
        bool foundCastleKingSide = false;
        for (Node::Potential potential : *root->m_position->potentials()) {
            if (potential.toString() == QLatin1String("f8h8"))
                foundCastleKingSide = true;
        }

        QCOMPARE(root->position()->potentials()->count(), 36);
        QVERIFY(foundCastleKingSide);
    }

    // black to castle kingside
    {
        const QLatin1String fen = QLatin1String("bq4kr/p3bpp1/3ppn1p/1P1n3P/P2P4/2N4R/1P3PP1/B1Q1NBK1 b k - 0 13");
        StandaloneGame g(fen);
        QCOMPARE(g.stateOfGameToFen(), fen);
        QCOMPARE(g.position().activeArmy(), Chess::Black);
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleAvailable(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleAvailable(Chess::Black, Chess::QueenSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::White, Chess::QueenSide));
        QVERIFY(g.position().isCastleLegal(Chess::Black, Chess::KingSide));
        QVERIFY(!g.position().isCastleLegal(Chess::Black, Chess::QueenSide));

        bool success = g.makeMove(Notation::stringToMove(QLatin1String("g8h8"), Chess::Computer));
        QVERIFY(success);

        const QLatin1String afterCastle = QLatin1String("bq3rk1/p3bpp1/3ppn1p/1P1n3P/P2P4/2N4R/1P3PP1/B1Q1NBK1 w - - 1 14");
        QCOMPARE(g.stateOfGameToFen(), afterCastle);
    }

    Options::globalInstance()->setOption("UCI_Chess960", QLatin1Literal("false"));
}

void Tests::testSearchForMateInOne()
{
    const QLatin1String mateInOne = QLatin1String("8/8/5K2/3P3k/2P5/8/6Q1/8 w - - 12 68");
    StandaloneGame g(mateInOne);
    QCOMPARE(g.stateOfGameToFen(), mateInOne);
    bool success = g.makeMove(Notation::stringToMove("Qxg5"));
    QVERIFY(success);
    Node node;
    Node::Position position;
    node.initialize(nullptr, g);
    node.setPosition(&position);
    position.initialize(g.position());
    node.generatePotentials();
    QVERIFY(node.m_game.lastMove().isCheckMate());

    const QLatin1String mateInOneMoves = QLatin1String("position startpos moves d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 f8d6 g1f3 e8g8 e2e4 e6d5 e4d5 b8a6 f1e2 f8e8 e1g1 b7b6 c1g5 h7h6 g5h4 d6f4 e2d3 a6b4 d3f5 c8a6 b2b3 a6c8 g2g3 f4e5 f3e5 e8e5 g3g4 d7d6 f5c8 a8c8 a2a3 b4a6 a1a2 c8c7 d1c1 e5e8 f2f3 g7g5 h4g3 f6h7 c3e4 c7d7 h2h4 g5h4 g3e1 h7g5 a2e2 e8e5 e1c3 f7f6 c3e5 f6e5 g1h2 d7f7 f3f4 g5e4 e2e4 d8f6 c1e1 a6b8 h2g1 b8d7 f4e5 f6f1 e1f1 f7f1 g1f1 d7e5 f1g2 g8f8 g2h3 e5g6 e4e3 f8f7 e3f3 f7g7 f3f1 g6h8 h3h4 h8f7 h4h5 f7g5 f1e1 g7f8 h5h6 g5f7 h6g6 f7e5 e1e5 d6e5 g6f5 e5e4 f5e4 a7a6 a3a4 b6b5 a4b5 a6b5 c4b5 f8e7 e4e5 c5c4 b3c4 e7e8 e5e6 e8d8 e6d6 d8e8 b5b6 e8f7 b6b7 f7f6 b7b8q f6g5 d6e5 g5g4 b8b3 g4g5 b3g3 g5h5 e5f5 h5h6 g3g2 h6h7 f5e5 h7h6 e5f6 h6h5");
    UciEngine engine(this, QString());
    UCIIOHandler handler(this);
    engine.installIOHandler(&handler);

    QSignalSpy bestMoveSpy(&handler, &UCIIOHandler::receivedBestMove);
    engine.readyRead(mateInOneMoves);
    engine.readyRead(QLatin1String("go depth 1"));
    const bool receivedSignal = bestMoveSpy.isEmpty() ? bestMoveSpy.wait() : true;
    if (!receivedSignal) {
        QString message = QString("Did not receive signal for %1").arg(mateInOneMoves);
        QWARN(message.toLatin1().constData());
        engine.readyRead(QLatin1String("stop"));
    }
    QVERIFY(receivedSignal);
    QVERIFY2(handler.lastBestMove() == QLatin1String("g2h3")
        || handler.lastBestMove() == QLatin1String("g2g5"), QString("Result is %1")
        .arg(handler.lastBestMove()).toLatin1().constData());
    QVERIFY(handler.lastInfo().score == QLatin1String("mate 1")
        || handler.lastInfo().score == QLatin1String("cp 25600"));
}

void Tests::testInstaMove()
{
    const QLatin1String oneLegalMove = QLatin1String("position fen rnbqk2r/pppp1p1p/4pn1p/8/1bPP4/N7/PP2PPPP/R2QKBNR w KQkq - 3 5");
    UciEngine engine(this, QString());
    UCIIOHandler handler(this);
    engine.installIOHandler(&handler);

    QSignalSpy bestMoveSpy(&handler, &UCIIOHandler::receivedBestMove);
    engine.readyRead(oneLegalMove);
    engine.readyRead(QLatin1String("go wtime 1000000 btime 1000000"));
    const bool receivedSignal = bestMoveSpy.isEmpty() ? bestMoveSpy.wait(1000000) : true;
    if (!receivedSignal) {
        QString message = QString("Did not receive signal for %1").arg(oneLegalMove);
        QWARN(message.toLatin1().constData());
        engine.readyRead(QLatin1String("stop"));
    }
    QVERIFY(receivedSignal);
    QVERIFY2(handler.lastBestMove() == QLatin1String("d1d2"), QString("Result is %1")
        .arg(handler.lastBestMove()).toLatin1().constData());
    QVERIFY(handler.lastInfo().score != QLatin1String("cp 0"));
}

void Tests::testEarlyExit()
{
    const QLatin1String oneLegalMove = QLatin1String("position fen 7k/rrrr2nr/8/8/8/8/8/5RK1 w - - 0 1");
    UciEngine engine(this, QString());
    UCIIOHandler handler(this);
    engine.installIOHandler(&handler);

    QSignalSpy bestMoveSpy(&handler, &UCIIOHandler::receivedBestMove);
    engine.readyRead(oneLegalMove);
    QElapsedTimer timer;
    timer.start();
    engine.readyRead(QLatin1String("go wtime 5000 btime 5000"));
    const bool receivedSignal = bestMoveSpy.isEmpty() ? bestMoveSpy.wait(1000000) : true;
    if (!receivedSignal) {
        QString message = QString("Did not receive signal for %1").arg(oneLegalMove);
        QWARN(message.toLatin1().constData());
        engine.readyRead(QLatin1String("stop"));
    }
    QVERIFY(receivedSignal);
    QVERIFY(timer.elapsed() < 4000);
    QVERIFY2(handler.lastBestMove() == QLatin1String("f1f8"), QString("Result is %1")
        .arg(handler.lastBestMove()).toLatin1().constData());
    QCOMPARE(handler.lastInfo().score, QLatin1String("mate 1"));
}

void Tests::testHistory()
{
    QLatin1String fen = QLatin1String("4k3/8/8/8/8/1R6/8/4K3 b - - 0 40");
    QVector<QString> moves = QString("e8d7 e1f1 d7d6 b3b2 d6c6 b2b8 c6d6 b8b7 d6c6 b7b3 c6d7 b3a3 d7c7 a3a6 c7c8").split(" ").toVector();

    History::globalInstance()->clear();

    StandaloneGame game(fen);
    for (QString move : moves) {
        Move mv = Notation::stringToMove(move, Chess::Computer);
        bool success = game.makeMove(mv);
        History::globalInstance()->addGame(game);
        QVERIFY(success);
    }

    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);

    Node *lastNode = root;
    QVector<QString> nodeMoves = QString("a6a1 c8d7 f1g1 d7c6 a1a8 c6b7 a8d8 b7a7").split(" ").toVector();
    for (QString move : nodeMoves) {
        Move mv = Notation::stringToMove(move, Chess::Computer);
        Node::Potential *potential = lastNode->generatePotential(mv);
        Node::NodeGenerationError error = Node::NoError;
        lastNode = lastNode->generateNode(potential->move(), potential->pValue(), lastNode, Cache::globalInstance(), &error);
        lastNode->initializePosition(Cache::globalInstance());
        Q_ASSERT(lastNode);
    }

    QString string;
    {
        QTextStream stream(&string);
        HistoryIterator it = HistoryIterator::begin(lastNode);
        for (; it != HistoryIterator::end(); ++it) {
            stream << it.game().toString(Chess::Computer);
            stream << QLatin1String(" ");
        }
        string = string.trimmed();
    }

    // The moves in reverse order...
    QCOMPARE(string, QLatin1String("b7a7 a8d8 c6b7 a1a8 d7c6 f1g1 c8d7 a6a1 c7c8 a3a6 d7c7 b3a3 c6d7 b7b3 d6c6 b8b7 c6d6 b2b8 d6c6 b3b2 d7d6 e1f1 e8d7"));

    // The toString method is slower, but uses history here to display the last 12 moves from the node
    QCOMPARE(lastNode->toString(), QLatin1String("b3a3 d7c7 a3a6 c7c8 a6a1 c8d7 f1g1 d7c6 a1a8 c6b7 a8d8 b7a7"));
}

void Tests::testThreeFold()
{
    History::globalInstance()->clear();

    QVector<QString> moves = QString("g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8").split(" ").toVector();
    StandaloneGame g;
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    QVERIFY(root->isThreeFold());
}

void Tests::testThreeFold2()
{
    History::globalInstance()->clear();

    QVector<QString> moves = QString("g1f3 d7d5 d2d4 e7e6 c1f4 f8d6 f4d6 d8d6 c2c4 g8f6 e2e3 d5c4 d1a4 c7c6 a4c4 b8d7 f1e2 d6e7 b1d2 e6e5 c4c2 e5d4 f3d4 d7e5 a2a3 c6c5 e2b5 e8f8 d4f3 e5f3 d2f3 g7g6 c2c3 f8g7 h2h4 c8g4 b5c4 a8d8 f3g5 h8f8 f2f3 g4d7 h4h5 h7h6 g5f7 f8f7 c4f7 g7f7 h5g6 f7g6 e1c1 d8e8 e3e4 h6h5 d1d2 e7e5 c3e5 e8e5 d2d6 g6f7 c1d2 f7e7 d6d3 c5c4 d3c3 d7e6 d2e3 e5b5 c3c2 b5b3 e3f4 e6f7 g2g4 h5g4 f3g4 f6d7 g4g5 d7f8 h1h6 f8g6 f4f5 b3f3 f5g4 g6e5 g4h4 f7e6 h6h7 e7d6 g5g6 e5g6 h4g5 f3g3 g5f6 g6e5 c2d2 g3d3 d2g2 e5d7 f6g5 d6e5 h7e7 d7c5 g5h6 d3b3 h6g7 c5e4 g2e2 b3g3 g7h6 e5f6 e7e6 f6e6 e2e4 e6d5 e4e7 g3b3 e7e2 c4c3 b2c3 b3a3 e2e7 b7b5 e7c7 a7a5 h6g5 a5a4 c7c8 d5e4 c8e8 e4d3 g5f5 a3b3 e8h8 a4a3 h8e8 d3c3 f5e5 c3b4 e8h8 b3b1 h8c8 a3a2 e5d4 b4a4 d4e4 b5b4 e4d3 a4a3 c8c1 b4b3 d3d2 a3a4 d2d3 a4b5 d3c3 b3b2 c1h1 a2a1q h1b1 a1b1 c3d2 b1a2 d2c3 b5c5 c3d2 b2b1q d2e3 a2a1 e3f4 a1a2 f4e5 b1g1 e5f5 g1h1 f5g6 h1g1 g6f6 g1h1 f6f5 h1h5 f5e4 a2a1 e4f4 h5h6 f4g3 h6h5 g3g2 h5h4 g2f3 a1a2 f3e3 h4h3 e3f4 h3h2 f4g5 a2a1 g5g6 h2h4 g6f7 h4h6 f7e7 h6h5 e7d8 a1b1 d8d7 b1a1 d7e7 a1a2 e7d7 a2a1 d7e7 h5h7 e7e6 h7h6 e6d7 h6h5").split(" ").toVector();
    StandaloneGame g;
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    QVERIFY(root->isThreeFold());
}

void Tests::testThreeFold3()
{
    History::globalInstance()->clear();

    QVector<QString> moves = QString("g1f3 d7d5 d2d4 g8f6 c1f4 c8f5 c2c4 e7e6 b1c3 f8b4 d1a4 b8c6 f3e5 e8g8 e5c6 b4c3 b2c3 d8d7 f2f3 h7h5 e2e3 b7c6 f1e2 f8b8 e1g1 b8b2 f1f2 h5h4 e2f1 b2f2 g1f2 a7a5 f2g1 f5g6 h2h3 f6h5 f4g5 h5g3 c4d5 g3f1 a1f1 e6d5 g5h4 d7d6 h4g5 g6d3 g5f4 d6e7 f1f2 d3b5 a4c2 a5a4 c2f5 a4a3 g1h2 b5c4 f5b1 e7d8 b1b7 a8b8 b7c6 b8b2 f2b2 a3b2 c6b7 g7g5 f4g3 c4a6 b7b2 d8e8 g3c7 e8e3 c7e5 f7f6 b2b8 g8f7 b8a7 f7e8 a7a6 f6e5 a6e6 e8f8 e6e5 e3d2 e5g3 d2c3 g3d6 f8g7 d6d5 c3c1 d5e5 g7f7 e5g3 c1d2 a2a4 d2d4 a4a5 d4f6 g3c7 f7f8 c7b8 f8f7 b8b7 f7f8 b7b4 f8f7 b4c4 f7g7 c4c7 g7g8 c7c4 g8g7 c4c7 g7g8 c7c4 g8g7").split(" ").toVector();
    StandaloneGame g;
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    QVERIFY(root->isThreeFold());
}

void Tests::testThreeFold4()
{
    History::globalInstance()->clear();

    QLatin1String fen = QLatin1String("4k3/8/8/8/8/1R6/8/4K3 b - - 0 40");
    QVector<QString> moves = QString("e8d7 e1f1 d7d6 b3b2 d6c6 b2b8 c6d6 b8b7 d6c6 b7b3 c6d7 b3a3 d7c7 a3a6 c7c8 a6a1 c8d7 f1g1 d7c6 a1a8 c6b7 a8d8 b7a7 d8d3 a7b8 d3a3 b8c7 a3a6 c7b7 a6f6 b7b8 f6f2 b8c7 f2a2 c7b6 a2a3 b6b7 a3a2 b7c7 a2a6 c7b7 a6a5 b7b8 a5a4 b8c7 a4b4 c7d7 b4b6 d7d8 b6b5 d8c7 b5b4 c7d7 b4b6 d7c7 b6f6 c7d7 f6f2 d7e8 f2a2 e8d7 a2a6 d7c7 a6a5 c7b8 a5a4 b8c7").split(" ").toVector();
    StandaloneGame g(fen);
    History::globalInstance()->addGame(g);
    for (QString mv : moves) {
        Move move = Notation::stringToMove(mv, Chess::Computer);
        bool success = g.makeMove(move);
        QVERIFY(success);
        History::globalInstance()->addGame(g);
    }

    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    QVERIFY(!root->isThreeFold());
    root->generatePotentials();

    bool found = false;
    QVector<Node::Potential> *potentials = root->m_position->potentials(); // not a copy
    QVERIFY(!potentials->isEmpty());
    for (int i = 0; i < potentials->count(); ++i) {
        // We get a non-const reference to the actual value and change it in place
        Node::Potential *potential = &((*potentials)[i]);
        if (QLatin1String("a4b4") == Notation::moveToString(potential->move(), Chess::Computer)) {
            found = true;
            Node::NodeGenerationError error = Node::NoError;
            Node *threeFold = root->generateNode(potential->move(), potential->pValue(), root, Cache::globalInstance(), &error);
            threeFold->initializePosition(Cache::globalInstance());
            Q_ASSERT(threeFold);
            bool success = threeFold->checkMoveClockOrThreefold(threeFold->position()->positionHash(), Cache::globalInstance());
            QVERIFY(success);
            QVERIFY(threeFold->isThreeFold());
        }
    }
    QVERIFY(found);
}

void Tests::checkGame(const QString &fen, const QVector<QString> &mv)
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

        StandaloneGame g = History::globalInstance()->currentGame();
        if (g.halfMoveClock() >= 100) {
            r = HalfMoveClock;
            break;
        }

        if (g.position().isDeadPosition()) {
            r = DeadPosition;
            break;
        }

        Tree *tree = engine.searchEngine()->tree();
        QVERIFY(tree);
        tree->clearRoot();

        Node *root = tree->embodiedRoot();
        QVERIFY(root);
        QVERIFY(!root->isThreeFold());
        if (!root->checkMoveClockOrThreefold(root->position()->positionHash(), Cache::globalInstance()))
            root->generatePotentials();

        if (root->isThreeFold()) {
            r = ThreeFold;
            break;
        }

        if (root->isCheckMate()) {
            r = CheckMate;
            break;
        }

        if (root->isStaleMate()) {
            r = StaleMate;
            break;
        }

        engineHandler.clear();
        bestMoveSpy.clear();
        engine.readyRead(QLatin1String("go depth 1"));
        const bool receivedSignal = bestMoveSpy.isEmpty() ? bestMoveSpy.wait() : true;
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

void Tests::testMateWithKRvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1R6/8/4K3 b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void Tests::testMateWithKQvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1Q6/8/4K3 b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void Tests::testMateWithKBNvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1N6/8/4K2B b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void Tests::testMateWithKBBvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1B6/8/4K1B1 b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void Tests::testMateWithKQQvK()
{
    QString fen = QLatin1String("4k3/8/8/8/8/1Q6/8/4K2Q b - - 0 40");

    QVector<QString> moves;
    moves.append(QLatin1String("e8d7"));
    checkGame(fen, moves);
}

void Tests::testTB()
{
    // TB positions tested manually
    // 8/8/1K6/2P2Q1p/P6k/1pq5/2P5/8 w - - 2 88
    QVERIFY(true);

    // 2K5/8/2P3q1/8/P4k2/7Q/8/8 w - - 3 110
    QVERIFY(true);
}

void Tests::testDoNotPropagateDrawnAsExact()
{
    // The following is a contrived position that was examined by a specific net as more or less
    // drawn. It is not important if the the net was correct and the position is theoretically drawn
    // or not. The point of the test is that if a position is reached where we score it as exactly
    // drawn due to stalemate or the threefold rule for instance, we *must not* propagate this
    // thereby allowing the net to change its mind upon further examination of sibling playouts.
    QString fen = QLatin1String("1rk2b1r/q7/1ppp4/4p1B1/2PP2nN/8/7P/R4R1K w - - 0 35");

    QVector<QString> moves = QString("a1a7 b8b7 a7a8 b7b8 a8a7 b8b7 a7a8").split(" ").toVector();

    History::globalInstance()->clear();

    StandaloneGame game(fen);
    for (QString move : moves) {
        Move mv = Notation::stringToMove(move, Chess::Computer);
        bool success = game.makeMove(mv);
        History::globalInstance()->addGame(game);
        QVERIFY(success);
    }

    Tree tree;
    Node *root = tree.embodiedRoot();
    root->setPositionQValue(0.0f);
    root->setQValueAndVisit();
    QVERIFY(root);

    Node::NodeGenerationError error = Node::NoError;
    Node *b7b8 = root->generateNode(Notation::stringToMove(QLatin1String("b7b8"), Chess::Computer), 1.0f, root, Cache::globalInstance(), &error);
    QVERIFY(b7b8);
    QCOMPARE(Node::NoError, error);
    b7b8->initializePosition(Cache::globalInstance());
    b7b8->setPositionQValue(0.0f);
    b7b8->setQValueAndVisit();
    b7b8->generatePotentials();

    Node *a8a7 = nullptr;
    Node *a8b8 = nullptr;
    for (Node::Potential potential : *b7b8->m_position->potentials()) {
        QString move = Notation::moveToString(potential.move(), Chess::Computer);
        Node::NodeGenerationError error = Node::NoError;
        Node *child = b7b8->generateNode(potential.move(), potential.pValue(), b7b8, Cache::globalInstance(), &error);
        QVERIFY(child);
        QCOMPARE(Node::NoError, error);
        child->initializePosition(Cache::globalInstance());
        ++b7b8->m_potentialIndex;
        if (move == QLatin1String("a8a7")) {
            a8a7 = child;
            QVERIFY(child->checkMoveClockOrThreefold(child->position()->positionHash(), Cache::globalInstance()));
            QVERIFY(child->isExact());
            QVERIFY(qFuzzyCompare(child->positionQValue(), 0.0f));
        } else {
            if (move == QLatin1String("a8b8"))
                a8b8 = child;

            // Initially score all the children except for a8a7 as losing
            child->setPositionQValue(-0.1f);
        }

        child->backPropagateDirty();
    }

    QVERIFY(a8a7);
    QVERIFY(a8b8);

    WorkerInfo info;
    double newScores = 0;
    quint32 newVisits = 0;
    Node::minimax(b7b8, 0, &info, &newScores, &newVisits);

    // This is the whole point. We do not want to propagate a draw here as it is possible that upon
    // further playouts of other siblings that they will no longer be losing and could be winning.
    QVERIFY(!b7b8->isExact());
}

void Tests::testContext()
{
    {
        Node node;
        QVERIFY(node.hasContext(Node::NoContext));
        node.setContext(Node::GameContextDrawInTree);
        QVERIFY(!node.hasContext(Node::NoContext));
        QVERIFY(node.hasContext(Node::GameContextDrawInTree));
    }
    {
        Node node;
        node.setContext(Node::GameCycleInTree);
        QVERIFY(!node.hasContext(Node::NoContext));
        QVERIFY(node.hasContext(Node::GameCycleInTree));
    }
    {
        Node node;
        node.setContext(Node::GameContextDrawInTree);
        node.setContext(Node::GameCycleInTree);
        QVERIFY(!node.hasContext(Node::NoContext));
        QVERIFY(node.hasContext(Node::GameContextDrawInTree));
        QVERIFY(node.hasContext(Node::GameCycleInTree));
    }
}

void Tests::testExhaustSearch()
{
    const QLatin1String mateInOne = QLatin1String("position fen 8/8/8/8/6Q1/5K2/7k/8 w - - 49 90");

    // Turn off tablebases for this one
    Options::globalInstance()->setOption("SyzygyPath", QString());

    UciEngine engine(this, QString());
    UCIIOHandler handler(this);
    engine.installIOHandler(&handler);

    QSignalSpy bestMoveSpy(&handler, &UCIIOHandler::receivedBestMove);
    engine.readyRead(mateInOne);
    engine.readyRead(QLatin1String("go nodes 100000000"));
    const bool receivedSignal = bestMoveSpy.isEmpty() ? bestMoveSpy.wait() : true;
    if (!receivedSignal) {
        QString message = QString("Did not receive signal for %1").arg(mateInOne);
        QWARN(message.toLatin1().constData());
        engine.readyRead(QLatin1String("stop"));
    }
    QVERIFY(receivedSignal);
    QVERIFY2(handler.lastBestMove() == QLatin1String("g4g2"), QString("Result is %1")
        .arg(handler.lastBestMove()).toLatin1().constData());
    QVERIFY(handler.lastInfo().score == QLatin1String("mate 1")
        || handler.lastInfo().score == QLatin1String("cp 25600"));
}
