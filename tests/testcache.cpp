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
        Q_UNUSED(forcedFree);
        qt_noop();
        return true;
    }

    quint64 id = 0;
};

inline quint64 fixedHash(const CacheItem &item)
{
    return item.id;
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
        CacheItem *copyItem = cache.object(id1, false /*relink*/);
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
        CacheItem *copyItem = cache.object(id2, false /*relink*/);
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

    // Pin second and fourth item
    cache.pin(item2->id);
    cache.pin(item4->id);
    QCOMPARE(cache.used(), 5);

    // Check contents of hash everything should still be there
    QVERIFY(cache.contains(1));
    QVERIFY(cache.contains(2));
    QVERIFY(cache.contains(3));
    QVERIFY(cache.contains(4));
    QVERIFY(cache.contains(5));
}

void Tests::testStart(const StandaloneGame &start)
{
    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    QCOMPARE(nullptr, root->parent());
    QCOMPARE(start.position().positionHash(), root->position()->positionHash());
    root->generateChildren();

    QVector<Node::Child> *childRefs = root->children(); // not a copy
    QCOMPARE(childRefs->count(), 20);
    for (int i = 0; i < childRefs->count(); ++i) {
        Node::Child *childRef = &((*childRefs)[i]);
        QVERIFY(childRef->isPotential());
        Node::NodeGenerationError error = Node::NoError;
        Node *child = root->generateEmbodiedChild(childRef, Cache::globalInstance(), &error);
        QVERIFY(child);
        QVERIFY(!childRef->isPotential());
        QVERIFY(Cache::globalInstance()->containsNode(child->hash()));
        QCOMPARE(child->parent(), root);
        QVERIFY(!child->hasChildren());
        QCOMPARE(quint32(0), child->visits());
        QCOMPARE(quint32(0), child->virtualLoss());
        QVERIFY(qFuzzyCompare(-2.f, child->m_qValue));
        QVERIFY(qFuzzyCompare(-2.f, child->m_rawQValue));
        QVERIFY(qFuzzyCompare(-2.f, child->m_pValue));
        QVERIFY(!child->isExact());
        QVERIFY(!child->isTB());
        QVERIFY(!child->isDirty());

        Node::Position *childPosition = child->position();
        QVERIFY(childPosition);
        QVERIFY(Cache::globalInstance()->containsNodePosition(childPosition->positionHash()));
        QVERIFY(childPosition->nodes().contains(child));
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
void Tests::generateEmbodiedChild(Node *parent, bool onlyUniquePositions, Node **generatedChild)
{
    QVector<Node::Child> *childRefs = parent->children(); // not a copy
    for (int i = 0; i < childRefs->count(); ++i) {
        Node::Child *childRef = &((*childRefs)[i]);

        if (!childRef->isPotential())
            continue;

        if (onlyUniquePositions) {
            // Make the child move
            Game::Position childPosition = parent->position()->position(); // copy
            Game childGame = parent->game();
            const bool success = childGame.makeMove(childRef->move(), &childPosition);
            Q_ASSERT(success);

            if (Cache::globalInstance()->containsNodePosition(childPosition.positionHash()))
                continue;
        }

        QVERIFY(childRef->isPotential());
        Node::NodeGenerationError error = Node::NoError;
        Node *child = parent->generateEmbodiedChild(childRef, Cache::globalInstance(), &error);
        if (error == Node::ParentPruned) {
            *generatedChild = nullptr;
            return;
        } else if (!child) {
            qDebug() << error;
        }
        QVERIFY(child);
        QVERIFY(!childRef->isPotential());
        QVERIFY(Cache::globalInstance()->containsNode(child->hash()));
        QCOMPARE(child->parent(), parent);
        QVERIFY(!child->hasChildren());
        QCOMPARE(quint32(0), child->visits());
        QCOMPARE(quint32(0), child->virtualLoss());
        QVERIFY(qFuzzyCompare(-2.f, child->m_qValue));
        QVERIFY(qFuzzyCompare(-2.f, child->m_rawQValue));
        QVERIFY(qFuzzyCompare(-2.f, child->m_pValue));
        QVERIFY(!child->isExact());
        QVERIFY(!child->isTB());
        QVERIFY(!child->isDirty());

        Node::Position *childPosition = child->position();
        QVERIFY(childPosition);
        QVERIFY(Cache::globalInstance()->containsNodePosition(childPosition->positionHash()));
        QVERIFY(childPosition->nodes().contains(child));
        *generatedChild = child;
        return; // done
    }
}

void Tests::testCacheInsertAndRetrieve()
{
    QCOMPARE(Cache::globalInstance()->used(), 0);

    // Hold a few nodes inserted into cache
    QVector<QPair<quint64, Node*>> holdNodes;

    // A stack of nodes
    QStack<quint64> stack;

    StandaloneGame start;
    History::globalInstance()->addGame(start);
    Tree tree;
    Node *root = tree.embodiedRoot();
    QVERIFY(root);
    stack.append(root->hash());
    holdNodes.append(qMakePair(root->hash(), root));

    const int numberOfNodesToHold = 42;
    QVERIFY(numberOfNodesToHold < Cache::globalInstance()->size());

    while (Cache::globalInstance()->used() < Cache::globalInstance()->size()) {
        QVERIFY(!stack.isEmpty());
        quint64 hash = stack.pop();
        if (!Cache::globalInstance()->containsNode(hash))
            continue;

        Node *parent = Cache::globalInstance()->node(hash);
        QVERIFY(!parent->hasChildren());
        parent->generateChildren();
        if (!parent->hasChildren()) // terminal
            continue;

        while (Cache::globalInstance()->used() < Cache::globalInstance()->size()
            && Cache::globalInstance()->containsNode(hash)) {
            Node *generatedChild = nullptr;
            generateEmbodiedChild(parent, true /*onlyUniquePositions*/, &generatedChild);
            if (!generatedChild)
                break;

            stack.push(generatedChild->hash());
            if (quint64(holdNodes.count()) < numberOfNodesToHold)
                holdNodes.append(qMakePair(generatedChild->hash(), generatedChild));
        }
    }

    // Make sure cache is full
    QCOMPARE(Cache::globalInstance()->used(), Cache::globalInstance()->size());
    QVERIFY(qFuzzyCompare(Cache::globalInstance()->percentFull(0), 1.f));
    QCOMPARE(holdNodes.count(), numberOfNodesToHold);

    // Explicitly retrieve the nodes in holdNodes to update the link in LRU cache
    for (QPair<quint64, Node*> last : holdNodes) {
        QVERIFY(last.second);
        QCOMPARE(last.first, last.second->hash());
        QVERIFY(Cache::globalInstance()->containsNode(last.first));
        Node *copyLast = Cache::globalInstance()->node(last.first, true /*update*/);
        Cache::globalInstance()->nodePosition(copyLast->position()->positionHash(), true /*update*/);
        QCOMPARE(last.second, copyLast);
    }

    const int numberToEvictBeforeHolds = Cache::globalInstance()->used() - numberOfNodesToHold;

    // Now that we are full, let's keep adding until we start evicting holdNodes to check the order
    // of eviction
    int numberEvicted = 0;
    while (numberEvicted < int(Cache::globalInstance()->used())) {
        QVERIFY(!stack.isEmpty());
        quint64 hash = stack.pop();
        if (!Cache::globalInstance()->containsNode(hash))
            continue;

        Node *parent = Cache::globalInstance()->node(hash);
        QVERIFY(!parent->hasChildren());
        parent->generateChildren();
        if (!parent->hasChildren()) // terminal
            continue;

        while (numberEvicted < int(Cache::globalInstance()->used())
            && Cache::globalInstance()->containsNode(hash)) {
            // Generate *one* child which will evict something from cache now that it is full
            Node *generatedChild = nullptr;
            generateEmbodiedChild(parent, true /*onlyUniquePositions*/, &generatedChild);
            if (!generatedChild)
                break;

            QVERIFY(generatedChild);
            stack.push(generatedChild->hash());
            ++numberEvicted;

            int numberOfHoldsEvicted = qMax(0, numberEvicted - numberToEvictBeforeHolds);

            for (int j = 0; j < holdNodes.count(); ++j) {
                QPair<quint64, Node*> holdNode = holdNodes.at(j);
                QVERIFY(holdNode.second);
                const quint64 hash = holdNode.first;
                const QString string = holdNode.second->toString();
                const bool shouldBeEvicted = j < numberOfHoldsEvicted;
                if (!shouldBeEvicted) {
                    QCOMPARE(holdNode.first, holdNode.second->hash());
                    QVERIFY(Cache::globalInstance()->containsNode(hash));
                }
            }
        }
    }
}
