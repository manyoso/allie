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

#ifndef TREE_H
#define TREE_H

#include <QMutex>

#include "cache.h"
#include "game.h"
#include "node.h"
#include "history.h"

//#define DEBUG_RESUME

struct PrincipalVariation {
    QString pv;
    int depth = 0;
    bool isTB = false;
};

class Tree {
public:
    Tree(bool resumePreviousPositionIfPossible = true);

    Node *embodiedRoot();
    QMutex *treeMutex();
    void reset();
    void clearRoot();
    void constructPrincipalVariations();
    QVector<PrincipalVariation> multiPV();

private:
    QMutex m_treeMutex;
    Node *m_root;
    QVector<quint64> m_pinned;
    bool m_resumePreviousPositionIfPossible;
};

inline Tree::Tree(bool resumePreviousPositionIfPossible)
    : m_root(nullptr),
    m_resumePreviousPositionIfPossible(resumePreviousPositionIfPossible)
{
    m_pinned.reserve(1000);
}

inline void Tree::reset()
{
    // If this is called it means the cache has already been reset
    m_pinned.clear();
    m_root = nullptr;
}

inline void Tree::clearRoot()
{
    // Attempt to resume root if possible
    const StandaloneGame rootGame = History::globalInstance()->currentGame();
    Cache &cache = *Cache::globalInstance();

    // Clear the old pinned nodes and pv
    for (quint64 pin : m_pinned)
        cache.unpinNode(pin);

    m_pinned.clear();

    if (m_root) {
        cache.unpinNode(m_root->hash());

        if (!m_resumePreviousPositionIfPossible) {
            cache.unlinkNode(m_root->hash());
            m_root = nullptr;
            Q_ASSERT(!cache.used());
        } else {
            bool foundResume = false;
            const QVector<Node*> children = m_root->embodiedChildren();
            for (Node *child : children) {
                const QVector<Node*> grandChildren = child->embodiedChildren();
                for (Node *grandChild : grandChildren) {
                    if (grandChild->m_position->position().isSamePosition(rootGame.position()) && !grandChild->isTrueTerminal()) {
                        grandChild->setAsRootNode();
                        cache.unlinkNode(m_root->hash());
                        m_root = grandChild;
                        constructPrincipalVariations();
                        foundResume = true;
                        break;
                    }
                }
            }
            if (!foundResume) {
                cache.unlinkNode(m_root->hash());
                Q_ASSERT(!cache.used());
                m_root = nullptr;
            }
        }
    }

#if defined(DEBUG_RESUME)
    const int sizeAfter = cache.used();
    if (sizeAfter)
        qDebug() << "Resume resulted in" << sizeAfter << "resued nodes.";
#endif
}

inline Node *Tree::embodiedRoot()
{
    // This function should *always* return a valid and initialized pointer
    const StandaloneGame rootGame = History::globalInstance()->currentGame();
    Cache &cache = *Cache::globalInstance();
    if (m_root && cache.containsNode(m_root->hash()))
        return m_root;

    quint64 rootHash = Node::nextHash();
    m_root = cache.newNode(rootHash);
    Q_ASSERT(m_root);

    Node::Position *rootPosition = nullptr;
    quint64 rootPositionHash = rootGame.position().positionHash();
    if (m_resumePreviousPositionIfPossible && cache.containsNodePosition(rootPositionHash)) {
        rootPosition = cache.nodePosition(rootPositionHash);
        m_root->initialize(rootHash, nullptr, rootGame, rootPosition);
        rootPosition->initialize(m_root, rootGame.position(), rootPositionHash);
    } else {
        rootPosition = cache.newNodePosition(rootPositionHash);
        m_root->initialize(rootHash, nullptr, rootGame, rootPosition);
        rootPosition->initialize(m_root, rootGame.position(), rootPositionHash);
    }

    Q_ASSERT(rootHash == m_root->hash());

    return m_root;
}

inline QMutex *Tree::treeMutex()
{
    return &m_treeMutex;
}

inline void Tree::constructPrincipalVariations()
{
    Node *root = embodiedRoot();
    Q_ASSERT(root);
    Cache &cache = *Cache::globalInstance();

    // Clear the old pinned nodes and pv
    for (quint64 pin : m_pinned)
        cache.unpinNode(pin);

    m_pinned.clear();

    // Pin root
    cache.pinNode(root->hash());

    QVector<Node::Child> *children = root->children();
    for (Node::Child &ch : *children) {
        if (ch.isPotential() || !ch.node())
            continue;
        ch.node()->pinPrincipalVariation(&m_pinned, &cache);
    }
    Q_ASSERT(m_pinned.count() < 1000);
}

#endif // TREE_H
