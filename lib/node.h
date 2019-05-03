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

#ifndef NODE_H
#define NODE_H

#include <QString>
#include <QVector>
#include <QtMath>
#include <QMutex>

#include "fastapprox/fastlog.h"
#include "game.h"
#include "move.h"
#include "notation.h"
#include "search.h"
#include "treeutils.h"

#define MAX_DEPTH 127
#define USE_PARENT_QVALUE
#define USE_CPUCT_SCALING

//#define DEBUG_FETCHANDBP
//#define DEBUG_PLAYOUT_MCTS

extern int scoreToCP(float score);
extern float cpToScore(int cp);

template<Traversal>
class TreeIterator;

class Node;
struct Tree {
    Node *root = nullptr;
    QMutex mutex;
};

class PotentialNode {
public:
    PotentialNode(const Move &move)
        : m_move(move),
        m_pValue(-2.0f)
    {
    }

    bool hasPValue() const { return !qFuzzyCompare(m_pValue, -2.0f); }
    float pValue() const { return m_pValue; }
    void setPValue(float pValue) { m_pValue = pValue; }
    Move move() const { return m_move; }

    QString toString() const
    {
        return Notation::moveToString(m_move, Chess::Computer);
    }

private:
    Move m_move;
    float m_pValue;
};

class Node {
public:
    Node(Node *parent, const Game &game);
    ~Node();

    Game game() const { return m_game; }

    QVector<Game> previousMoves(bool fullHistory) const; // slow

    template<Traversal t>
    TreeIterator<t> begin() { return TreeIterator<t>(this); }
    template<Traversal t>
    TreeIterator<t> end() { return TreeIterator<t>(); }

    bool isFirstChild() const;
    bool isSecondChild() const;

    int depth() const;
    int treeDepth() const;
    bool isExact() const;
    float uCoeff() const;
    float uValue() const;
    float weightedExplorationScore() const;

    Node *rootNode(); // recursive
    const Node *rootNode() const; // recursive
    bool isRootNode() const;
    void setAsRootNode();
    Node *parent() const { return m_parent; }

    // children and potentials
    inline bool hasChildren() const { return !m_children.isEmpty(); }
    inline bool hasPotentials() const { return !m_potentials.isEmpty(); }
    const QVector<Node*> children() const { return m_children; }
    const QVector<PotentialNode*> potentials() const { return m_potentials; }
    bool isNotExtendable() const;

    // traversal
    bool isChildOf(const Node *node) const;
    Node *leftChild() const;
    Node *leftMostChild() const;
    Node *nextSibling() const;
    Node *nextAncestorSibling() const;
    Node *bestChild() const;

    QString principalVariation(int *depth) const; // recursive

    bool hasQValue() const;
    float qValueDefault() const;
    float qValue() const;
    void setQValueFromRaw();
    bool hasRawQValue() const;
    float rawQValue() const { return m_rawQValue; }
    void setRawQValue(float qValue);
    void backPropagateValue(float qValue);
    void backPropagateValueFull();
    void setQValueAndPropagate();
    bool isAlreadyPlayingOut() const;
    Node *playout(int *depth, bool *createdNode);

    bool hasPValue() const;
    float pValue() const { return m_pValue; }
    void setPValue(float pValue) { m_pValue = pValue; }

    int count() const;

    void incrementVisited();
    static bool greaterThan(const Node *a, const Node *b);
    static void sortByScore(QVector<Node*> &nodes, bool partialSortFirstOnlyy);

    QString toString(Chess::NotationType = Chess::Computer) const;
    QString printTree(int depth) /*const*/; // recursive

    bool isCheckMate() const { return m_game.lastMove().isCheckMate(); }
    bool isStaleMate() const { return m_game.lastMove().isStaleMate(); }
    int repetitions() const;
    bool isThreeFold() const;
    bool isNoisy() const;

    // children and potential generation
    bool hasNoisyChildren() const;
    bool checkAndGenerateDTZ(int *dtz);
    void generatePotentials();
    void generatePotential(const Move &move);
    Node *generateChild(PotentialNode *potential);

    // flag saying we are in midst of scoring
    bool setScoringOrScored()
    {
        return m_scoringOrScored.test_and_set(); // atomic
    }

private:
    Game m_game;
    Node *m_parent;
    QVector<Node*> m_children;
    QVector<PotentialNode*> m_potentials;
    quint32 m_visited;
    quint32 m_virtualLoss;
    float m_qValue;
    float m_rawQValue;
    float m_pValue;
    float m_policySum;
    mutable float m_uCoeff;
    bool m_isExact: 1;
    bool m_isTB: 1;
    std::atomic_flag m_scoringOrScored;
    template<Traversal t>
    friend class TreeIterator;
    friend class SearchWorker;
    friend class SearchEngine;
};

inline bool Node::isFirstChild() const
{
    if (isRootNode())
        return false;
    return m_parent->m_children.first() == this;
}

inline int Node::depth() const
{
    int d = 0;
    Node *parent = m_parent;
    while (parent) {
        parent = parent->m_parent;
        d++;
    }
    return d;
}

inline int Node::treeDepth() const
{
    int d = 0;
    const Node *n = this;
    while (n && n->hasChildren()) {
        QVector<Node*> children = n->m_children;
        sortByScore(children, true /*partialSortFirstOnly*/);
        n = children.first();
        ++d;
    }
    return d;
}

inline bool Node::isExact() const
{
    return m_isExact;
}

inline bool Node::isRootNode() const
{
    return m_parent == nullptr;
}

inline int Node::count() const
{
    int c = isRootNode() ? 0 : 1; // me, myself, and I
    for (Node *n : m_children)
        c += n->count();
    return c;
}

inline bool Node::isNotExtendable() const
{
    // If we don't have children or potentials (either exact or haven't generated them yet)
    // or if our children or potentials don't have pValues then we are not extendable
    return (!hasChildren() || !m_children.first()->hasPValue())
        && (!hasPotentials() || !m_potentials.first()->hasPValue());
}

inline bool Node::isChildOf(const Node *node) const
{
    return node->m_children.contains(const_cast<Node*>(this));
}

inline Node *Node::leftChild() const
{
    if (!hasChildren())
        return nullptr;
    return m_children.first();
}

inline Node *Node::bestChild() const
{
    if (!hasChildren())
        return nullptr;
    QVector<Node*> children = m_children;
    sortByScore(children, true /*partialSortFirstOnly*/);
    return children.first();
}

inline Node *Node::leftMostChild() const
{
    Node *next = nullptr;
    Node *leftChild = this->leftChild();
    while (leftChild) {
        next = leftChild;
        leftChild = next->leftChild();
    }
    return next;
}

inline Node *Node::nextSibling() const
{
    if (isRootNode())
        return nullptr;

    const QVector<Node*> &parentsChildren = m_parent->m_children;
    const int index = parentsChildren.indexOf(const_cast<Node*>(this)) + 1;

    // If we are at the end, return nullptr
    if (index >= parentsChildren.count())
        return nullptr;

    return parentsChildren.at(index);
}

inline Node *Node::nextAncestorSibling() const
{
    for (Node* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (Node* sibling = ancestor->nextSibling())
            return sibling;
    }
    return nullptr;
}

inline float Node::qValueDefault() const
{
#if defined(USE_PARENT_QVALUE)
    return -qValue() - SearchSettings::fpuReduction * float(qSqrt(qreal(m_policySum)));
#else
    return -1.0f;
#endif
}

inline float Node::qValue() const
{
    if (isRootNode() || m_visited > 0)
        return m_qValue;

    if (m_parent->isRootNode())
        return 1.0f;
    return m_parent->qValueDefault();
}

inline float Node::uCoeff() const
{
    if (qFuzzyCompare(m_uCoeff, -2.0f)) {
        const quint32 N = qMax(quint32(1), m_visited);
#if defined(USE_CPUCT_SCALING)
        // From Deepmind's A0 paper
        // log ((1 + N(s) + cbase)/cbase) + cini
        const float growth = SearchSettings::cpuctF * fastlog((1 + N + SearchSettings::cpuctBase) / SearchSettings::cpuctBase);
#else
        const float growth = 0.0f;
#endif
        m_uCoeff = (SearchSettings::cpuctInit + growth) * float(qSqrt(N));
    }
    return m_uCoeff;
}

inline float Node::uValue() const
{
    if (isRootNode())
        return 100.f;

    const qint64 n = m_visited + m_virtualLoss;
    const float p = m_pValue;
    return m_parent->uCoeff() * p / (n + 1);
}

inline float Node::weightedExplorationScore() const
{
    if (isRootNode())
        return 1.0f + uValue();
    const float q = qValue();
    return q + uValue();
}

inline bool Node::greaterThan(const Node *a, const Node *b)
{
    if (a->m_visited == b->m_visited) {
        if (!a->m_visited)
            return a->pValue() > b->pValue();
        else
            return a->qValue() > b->qValue();
    }
    return a->m_visited > b->m_visited;
}

inline void Node::sortByScore(QVector<Node*> &nodes, bool partialSortFirstOnly)
{
    if (Q_LIKELY(partialSortFirstOnly)) {
        std::partial_sort(nodes.begin(), nodes.begin() + 1, nodes.end(),
            [=](const Node *a, const Node *b) {
            return greaterThan(a, b);
        });
    } else {
        std::stable_sort(nodes.begin(), nodes.end(),
            [=](const Node *a, const Node *b) {
            return greaterThan(a, b);
        });
    }
}

inline bool Node::isAlreadyPlayingOut() const
{
    return m_virtualLoss > 0 && !hasQValue();
}

inline bool Node::hasQValue() const
{
    return !qFuzzyCompare(m_qValue, -2.0f);
}

inline bool Node::hasRawQValue() const
{
    return !qFuzzyCompare(m_rawQValue, -2.0f);
}

inline bool Node::hasPValue() const
{
    return !qFuzzyCompare(pValue(), -2.0f);
}

QDebug operator<<(QDebug debug, const Node &node);

#endif // NODE_H
