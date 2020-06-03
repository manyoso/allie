/*
  This file is part of Allie Chess.
  Copyright (C) 2019 Adam Treat

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
#include "options.h"
#include "nn.h"
#include "tests.h"
#include "tree.h"

struct CacheItem {
    bool deinitialize(bool forcedFree)
    {
        Q_UNUSED(forcedFree)
        qt_noop();
        return true;
    }

    quint64 id = 0;
};

inline quint64 fixedHash(const CacheItem &item)
{
    return item.id;
}

inline quint64 isPinned(const CacheItem &item)
{
    Q_UNUSED(item)
    return false;
}

inline void setUniqueFlag(CacheItem &item)
{
    Q_UNUSED(item)
}

void Tests::testBasicCache()
{
    FixedSizeCache<CacheItem> cache;
    cache.reset(1);
    QCOMPARE(cache.used(), 0);
    QCOMPARE(cache.size(), 1);

    const quint64 id1 = 1;
    {
        CacheItem *item = cache.newObject(id1);
        item->id = id1;
        QVERIFY(cache.contains(id1));
        CacheItem *copyItem = cache.object(id1);
        QCOMPARE(copyItem, item);
        QCOMPARE(copyItem->id, item->id);
    }

    QCOMPARE(cache.used(), 1);
    QCOMPARE(cache.size(), 1);
    QVERIFY(qFuzzyCompare(cache.percentFull(0), 1.f));

    // Reuse previous
    const quint64 id2 = 2;
    {
        CacheItem *item = cache.newObject(id2);
        QVERIFY(!cache.contains(id1));   // Old id is no longer in cache
        QVERIFY(cache.contains(id2));    // Now we have new id in cache
        QCOMPARE(item->id, id1);        // But should still be set to previous id, because we do not
                                        // reset in deinitialize
        item->id = id2;                 // Update the id
        CacheItem *copyItem = cache.object(id2);
        QCOMPARE(copyItem, item);
        QCOMPARE(copyItem->id, item->id);
    }

    // Should still be full
    QCOMPARE(cache.used(), 1);
    QCOMPARE(cache.size(), 1);
    QVERIFY(qFuzzyCompare(cache.percentFull(0), 1.f));

    // Manual unlink
    {
        cache.unlink(id2);
        QVERIFY(!cache.contains(id1));   // Old id is no longer in cache
    }

    // Should be empty now
    QCOMPARE(cache.used(), 0);
    QCOMPARE(cache.size(), 1);
    QVERIFY(qFuzzyCompare(cache.percentFull(0), 0.f));

    // Reset to 5 items
    cache.reset(5);
    QCOMPARE(cache.used(), 0);
    QCOMPARE(cache.size(), 5);

    CacheItem *item1 = cache.newObject(1);
    item1->id = 1;
    CacheItem *item2 = cache.newObject(2);
    item2->id = 2;
    CacheItem *item3 = cache.newObject(3);
    item3->id = 3;
    CacheItem *item4 = cache.newObject(4);
    item4->id = 4;
    CacheItem *item5 = cache.newObject(5);
    item5->id = 5;

    // Should be full
    QCOMPARE(cache.used(), 5);
    QCOMPARE(cache.size(), 5);
    QVERIFY(qFuzzyCompare(cache.percentFull(0), 1.f));

    // Unlink third item
    cache.unlink(item3->id);
    QCOMPARE(cache.used(), 4);

    // Check contents of hash... all items except third should be there
    QVERIFY(cache.contains(1));
    QVERIFY(cache.contains(2));
    QVERIFY(!cache.contains(3));
    QVERIFY(cache.contains(4));
    QVERIFY(cache.contains(5));

    // Request a new item and should get third hold item back
    {
        CacheItem *item = cache.newObject(3);
        QCOMPARE(item3, item);
        QCOMPARE(item3->id, item->id);
        QCOMPARE(cache.used(), 5);
    }

    // Unlink second and fourth item
    cache.unlink(item2->id);
    cache.unlink(item4->id);
    QCOMPARE(cache.used(), 3);

    // Check contents of hash... all items except second and fourth should be there
    QVERIFY(cache.contains(1));
    QVERIFY(!cache.contains(2));
    QVERIFY(cache.contains(3));
    QVERIFY(!cache.contains(4));
    QVERIFY(cache.contains(5));

    // Request a new item and should get fourth item back
    {
        CacheItem *item = cache.newObject(4);
        QCOMPARE(item4, item);
        QCOMPARE(item4->id, item->id);
        QCOMPARE(cache.used(), 4);
    }

    // Check contents of hash... all items except second should be there
    QVERIFY(cache.contains(1));
    QVERIFY(!cache.contains(2));
    QVERIFY(cache.contains(3));
    QVERIFY(cache.contains(4));
    QVERIFY(cache.contains(5));

    // Request a new item and should get second item back
    {
        CacheItem *item = cache.newObject(2);
        QCOMPARE(item2, item);
        QCOMPARE(item2->id, item->id);
        QCOMPARE(cache.used(), 5);
    }

    // Check contents of cache... all items should be there
    QVERIFY(cache.contains(1));
    QVERIFY(cache.contains(2));
    QVERIFY(cache.contains(3));
    QVERIFY(cache.contains(4));
    QVERIFY(cache.contains(5));

    // Unlink first and fifth item
    cache.unlink(item1->id);
    cache.unlink(item5->id);
    QCOMPARE(cache.used(), 3);

    // Check contents of hash... all items except first and fifth should be there
    QVERIFY(!cache.contains(1));
    QVERIFY(cache.contains(2));
    QVERIFY(cache.contains(3));
    QVERIFY(cache.contains(4));
    QVERIFY(!cache.contains(5));

    // Request a new item and should get fifth item back
    {
        CacheItem *item = cache.newObject(5);
        QCOMPARE(item5, item);
        QCOMPARE(item5->id, item->id);
        QCOMPARE(cache.used(), 4);
    }

    // Request a new item and should get first item back
    {
        CacheItem *item = cache.newObject(1);
        QCOMPARE(item1, item);
        QCOMPARE(item1->id, item->id);
        QCOMPARE(cache.used(), 5);
    }

    // Unlink all items
    cache.unlink(item1->id);
    cache.unlink(item2->id);
    cache.unlink(item3->id);
    cache.unlink(item4->id);
    cache.unlink(item5->id);
    QCOMPARE(cache.used(), 0);

    // Check contents of cache... nothing should be there
    QVERIFY(!cache.contains(1));
    QVERIFY(!cache.contains(2));
    QVERIFY(!cache.contains(3));
    QVERIFY(!cache.contains(4));
    QVERIFY(!cache.contains(5));

    // Reset all the items
    item1 = cache.newObject(1);
    item1->id = 1;
    item2 = cache.newObject(2);
    item2->id = 2;
    item3 = cache.newObject(3);
    item3->id = 3;
    item4 = cache.newObject(4);
    item4->id = 4;
    item5 = cache.newObject(5);
    item5->id = 5;

    // Should be full
    QCOMPARE(cache.used(), 5);
    QCOMPARE(cache.size(), 5);
    QVERIFY(qFuzzyCompare(cache.percentFull(0), 1.f));
}

void Tests::testStart(const StandaloneGame &start)
{
    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    QCOMPARE(nullptr, root->parent());
    QCOMPARE(start.position().positionHash(), root->position()->positionHash());
    root->generatePotentials();

    QVector<Node::Potential> *potentials = root->m_position->potentials(); // not a copy
    QCOMPARE(potentials->count(), 20);
    for (int i = 0; i < potentials->count(); ++i) {
        Node::Potential *potential = &((*potentials)[i]);
        Node::NodeGenerationError error = Node::NoError;
        Node *child = root->generateNode(potential->move(), potential->pValue(), root, Cache::globalInstance(), &error);
        child->initializePosition(Cache::globalInstance());
        QVERIFY(child);
        QCOMPARE(child->parent(), root);
        QVERIFY(!child->hasChildren());
        QCOMPARE(quint32(0), child->visits());
        QCOMPARE(quint16(0), child->virtualLoss());
        QVERIFY(qFuzzyCompare(-2.f, child->positionQValue()));
        QVERIFY(qFuzzyCompare(-2.f, child->m_pValue));
        QVERIFY(!child->isExact());
        QVERIFY(!child->isTB());
        QVERIFY(!child->isDirty());

        Node::Position *childPosition = child->position();
        QVERIFY(childPosition);
        QVERIFY(Cache::globalInstance()->containsNodePosition(childPosition->positionHash()));
    }
}

void Tests::testStartingPosition()
{
    StandaloneGame start;
    QCOMPARE(start.stateOfGameToFen(), QLatin1String("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    QCOMPARE(start.position().activeArmy(), Chess::White);
    History::globalInstance()->addGame(start);
    testStart(start);
}

void Tests::testStartingPositionBlack()
{
    StandaloneGame start("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    QCOMPARE(start.stateOfGameToFen(), QLatin1String("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"));
    QCOMPARE(start.position().activeArmy(), Chess::Black);
    History::globalInstance()->addGame(start);
    testStart(start);
}

void Tests::perft(int depth, Node *node, PerftResult *result)
{
    if (!depth) {
        ++(*result).nodes;
        if (node->m_game.lastMove().isCapture())
           ++(*result).captures;
        if (node->m_game.lastMove().isEnPassant())
           ++(*result).ep;
        if (node->m_game.lastMove().isCastle())
           ++(*result).castles;
        if (node->m_game.lastMove().promotion() != Chess::Unknown)
           ++(*result).promotions;
        return;
    }

    QVERIFY(node);
    QVERIFY(node->position());
    if (!node->checkMoveClockOrThreefold(node->position()->positionHash(), Cache::globalInstance()))
        node->generatePotentials();

    QVector<Node::Potential> *potentials = node->m_position->potentials(); // not a copy
    int nodes = potentials->count();
    for (int i = 0; i < nodes; ++i) {
        Node::Potential *potential = &((*potentials)[i]);

        Node child;
        child.initialize(node, node->m_game);
        Game::Position childGamePosition = node->m_position->position(); // copy
        const bool success = child.m_game.makeMove(potential->move(), &childGamePosition);
        QVERIFY(success);
        Node::Position childPosition;
        child.setPosition(&childPosition);
        childPosition.initialize(childGamePosition);

        QCOMPARE(child.parent(), node);
        PerftResult childResult;
        perft(depth - 1, &child, &childResult);
        (*result).nodes += childResult.nodes;
        (*result).captures += childResult.captures;
        (*result).ep += childResult.ep;
        (*result).promotions += childResult.promotions;
        (*result).castles += childResult.castles;

#if 0 // divide
        if (node->isRootNode())
            qDebug() << child.toString() << childResult.nodes;
#endif
    }
}

#if defined(RUN_PERFT)
void Tests::testPerft()
{
// From https://www.chessprogramming.org/Perft_Results
QVector<PerftResult> perftList =
{
// initial position
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               1, 20,          0,          0,      0,          0           },
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               2, 400,         0,          0,      0,          0           },
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               3, 8902,        34,         0,      0,          0           },
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               4, 197281,      1576,       0,      0,          0           },
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               5, 4865609,     82719,      258,    0,          0           },
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               6, 119060324,	2812008,	5248,	0,          0           },
PerftResult { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               7, 3195901860,	108329926,	319617,	883453,     0           },

// position 2
PerftResult { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   1, 48,          8,          0,      2,          0           },
PerftResult { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   2, 2039,        351,        1,      91,         0           },
PerftResult { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   3, 97862,       17102,      45,     3162,       0           },
PerftResult { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   4, 4085603,     757163,     1929,   128013,     15172       },
PerftResult { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   5, 193690690,	35043416,	73365,	4993637,	8392        },
PerftResult { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   6, 8031647685,	1558445089,	3577504,184513607,  56627920    },

// position 3
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              1,	14,         1,          0,      0,          0           },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              2,	191,        14,         0,      0,          0           },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              3,	2812,       209,        2,      0,          0           },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              4,	43238,      3348,       123,	0,          0           },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              5,  674624,     52051,      1165,	0,          0           },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              6,	11030083,	940350,     33325,	0,          7552        },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              7,	178633661,	14519036,	294874,	0,          140024      },
PerftResult { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 0",                              8,	3009794393,	267586558,	8009239,0,          6578076     },

// position 4
PerftResult { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       1,	6,          0,          0,      0,          0           },
PerftResult { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       2,	264,        87,         0,      6,          48          },
PerftResult { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       3,	9467,       1021,       4,      0,          120         },
PerftResult { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       4,	422333,     131393,     0,      7795,       60032       },
PerftResult { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       5,	15833292,	2046173,    6512,	0,          329464      },
PerftResult { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       6,	706045033,	210369132,	212,	10882006,   81102984    }
};

//TODO: Add https://www.chessprogramming.org/Chess960_Perft_Results

    for (PerftResult r : perftList) {
        StandaloneGame start(r.fen);
        QCOMPARE(start.stateOfGameToFen(), r.fen);

        Node node;
        Node::Position position;
        node.initialize(nullptr, start);
        node.setPosition(&position);
        position.initialize(&node, start.position());

        qDebug() << "Running perft"
            << r.fen << "at depth" << r.depth << "with" << r.nodes << "positions.";

        QTime t;
        t.start();
        PerftResult result;
        perft(r.depth, &node, &result);
        qDebug("Perft took: %d ms", t.elapsed());
        QCOMPARE(result.captures, r.captures);
        QCOMPARE(result.ep, r.ep);
        QCOMPARE(result.castles, r.castles);
        QCOMPARE(result.promotions, r.promotions);
        QCOMPARE(result.nodes, r.nodes);
    }
}
#endif
