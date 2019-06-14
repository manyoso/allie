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

#include "game.h"
#include "node.h"
#include "hash.h"
#include "history.h"

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
    quint64 rootHash();
    void clearRoot();
    void constructPrincipalVariations();
    QVector<PrincipalVariation> multiPV();

private:
    QMutex m_treeMutex;
    quint64 m_rootHash;
    QVector<PrincipalVariation> m_pv;
    bool m_resumePreviousPositionIfPossible;
};

inline Tree::Tree(bool resumePreviousPositionIfPossible)
    : m_rootHash(0),
    m_resumePreviousPositionIfPossible(resumePreviousPositionIfPossible)
{
}

inline Node *Tree::embodiedRoot()
{
    // This function should *always* return a valid and initialized pointer
    const StandaloneGame rootGame = History::globalInstance()->currentGame();
    Hash &hash = *Hash::globalInstance();
    if (m_rootHash && hash.containsNode(m_rootHash))
        return Hash::globalInstance()->node(m_rootHash);

    m_rootHash = Node::nextHash();
    Node *root = hash.newNode(m_rootHash);
    Q_ASSERT(root);

    Node::Position *rootPosition = nullptr;
    if (hash.containsNodePosition(rootGame.position().positionHash())) {
        rootPosition = hash.nodePosition(rootGame.position().positionHash());
        if (!m_resumePreviousPositionIfPossible)
            rootPosition->initialize(m_rootHash, rootGame.position());
    } else {
        rootPosition = hash.newNodePosition(rootGame.position().positionHash());
        rootPosition->initialize(m_rootHash, rootGame.position());
    }

    root->initialize(m_rootHash, 0, rootGame, rootPosition);
    Q_ASSERT(m_rootHash == root->hash());

    return root;
}

inline QMutex *Tree::treeMutex()
{
    return &m_treeMutex;
}

inline void Tree::clearRoot()
{
    m_rootHash = 0;
}

inline void Tree::constructPrincipalVariations()
{
    Node *root = embodiedRoot();
    Q_ASSERT(root);
    Hash &hash = *Hash::globalInstance();

    // Clear the old pinned nodes and pv
    hash.clearPinnedNodes();
    m_pv.clear();

    // Pin root
    hash.pinNode(root->hash());

    QVector<Node*> children = root->embodiedChildren();
    Node::sortByScore(children, true /*partialSortFirstOnly*/);
    for (Node *child : children) {
        int pvDepth = 0;
        bool isTB = false;
        QString pv = child->principalVariation(&pvDepth, &isTB, true /*pin*/);
        m_pv.append(PrincipalVariation { pv, pvDepth, isTB });
    }
}

#endif // TREE_H
