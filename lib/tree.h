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

#include "cache.h"
#include "game.h"
#include "node.h"
#include "history.h"

//#define DEBUG_RESUME

class Tree {
public:
    Tree();
    ~Tree();

    Node *embodiedRoot();
    void reset();
    void clearRoot(bool resumeIfPossible = true);
    static void validateTree(Node *node, int *total);

private:
    Node *m_root;
};

inline Tree::Tree()
    : m_root(nullptr)
{
}

inline Tree::~Tree()
{
    clearRoot(false /*resumeIfPossible*/);
}

inline void Tree::reset()
{
    m_root = nullptr;
}

inline void Tree::validateTree(Node *node, int *total)
{
    if (total)
       ++(*total);
    Q_ASSERT(node->visits());
    Q_ASSERT(node->isRootNode() || node->hasPValue());
    Q_ASSERT(node->position());
    Q_ASSERT(node->position()->refs());
    Q_ASSERT(node->position()->visits());
    const QVector<Node*> children = *node->children();
    for (Node *child : children)
        validateTree(child, total);
}

inline void Tree::clearRoot(bool resumeIfPossible)
{
    const StandaloneGame rootGame = History::globalInstance()->currentGame();
    Cache &cache = *Cache::globalInstance();

    if (m_root) {
        if (!resumeIfPossible) {
            cache.unlinkNode(m_root);
            m_root = nullptr;
        } else {
            // Attempt to resume root if possible
            bool foundResume = false;
            const QVector<Node*> children = *m_root->children();
            for (Node *child : children) {
                const QVector<Node*> grandChildren = *child->children();
                for (Node *grandChild : grandChildren) {
                    if (grandChild->m_position->position().isSamePosition(rootGame.position()) && !grandChild->isTrueTerminal()) {
                        grandChild->setAsRootNode();
                        cache.unlinkNode(m_root);
                        m_root = grandChild;
                        foundResume = true;
                        break;
                    }
                }
            }
            if (!foundResume) {
                cache.unlinkNode(m_root);
                m_root = nullptr;
            }
        }
    }

    // Resets the used/size of the node arena taking into account reused nodes
    cache.resetNodes();

#if defined(DEBUG_RESUME)
    if (m_root) {
        int total = 0;
        validateTree(m_root, &total);
        Q_ASSERT(cache.used() == total);
    } else {
        Q_ASSERT(!cache.used());
    }
    const int sizeAfter = cache.used();
    if (sizeAfter)
        qDebug() << "Resume resulted in" << sizeAfter << "reused nodes.";
#endif
}

inline Node *Tree::embodiedRoot()
{
    // This function should *always* return a valid and initialized pointer
    const StandaloneGame rootGame = History::globalInstance()->currentGame();
    if (m_root)
        return m_root;

    Cache &cache = *Cache::globalInstance();
    Q_ASSERT(!cache.used());
    m_root = cache.newNode();
    Q_ASSERT(m_root);

    Node::Position *rootPosition = nullptr;
    quint64 rootPositionHash = rootGame.position().positionHash();

    // Always generate a new position for root to ensure we do not get a previous position that was
    // a TB hit or propagated exact node for instance
    if (cache.containsNodePosition(rootPositionHash))
        cache.nodePositionMakeUnique(rootPositionHash);

    rootPosition = cache.newNodePosition(rootPositionHash);
    m_root->initialize(nullptr, rootGame);
    m_root->setPosition(rootPosition);
    rootPosition->initialize(rootGame.position());
    m_root->setAsRootNode();

    return m_root;
}

#endif // TREE_H
