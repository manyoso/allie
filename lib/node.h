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

#define MAX_DEPTH 127
#define USE_PARENT_QVALUE
#define USE_CPUCT_SCALING

//#define DEBUG_FETCHANDBP
//#define DEBUG_PLAYOUT
//#define DEBUG_CHURN

extern int scoreToCP(float score);
extern float cpToScore(int cp);

class Node {
public:
    class Child {
    public:
        Child()
            : m_isPotential(true)
        {
            m_pValue = -2.0f;
            Q_ASSERT(!m_move.isValid());
        }

        Child(const Move &move)
            : m_isPotential(true)
        {
            m_pValue = -2.0f;
            m_move = move;
            Q_ASSERT(m_move.isValid());
        }

        Child(quint64 hash)
            : m_hash(hash),
            m_isPotential(true)
        {
        }

        bool isPotential() const { return m_isPotential; }
        void setPotential(bool p) { m_isPotential = p; }
        quint64 hash() const { return m_hash; }
        void setHash(quint64 hash) { m_hash = hash; }
        bool hasPValue() const { return !qFuzzyCompare(m_pValue, -2.0f); }
        float pValue() const { return m_pValue; }
        void setPValue(float pValue) { m_pValue = pValue; }
        Move move() const { return m_move; }
        bool isValid() const { return m_move.isValid(); }
        QString toString() const { return Notation::moveToString(m_move, Chess::Computer); }
        bool operator==(const Child &other) const { return m_move == other.m_move; }

    private:
        union {
            quint64 m_hash;
            float m_pValue;
        };
        Move m_move;
        bool m_isPotential : 1;
    };

    class Position {
    public:
        Position();
        ~Position();

        void initialize(quint64 nodeHash, const Game::Position &position);
        bool deinitialize();
        static Node::Position *relink(quint64 positionHash);
        bool nodesNotInHash() const;
        QVector<const Node*> embodiedNodes() const;
        inline const Game::Position &position() const { return m_position; }
        inline quint64 positionHash() const { return m_position.positionHash(); }
        inline QVector<quint64> nodes() const { return m_nodes; }
        inline bool hasNode(quint64 parent) const
        {
            return m_nodes.contains(parent);
        }

        inline void addNode(quint64 parent)
        {
            Q_ASSERT(!hasNode(parent));
            m_nodes.append(parent);
#if defined(DEBUG_CHURN)
            QString string;
            QTextStream stream(&string);
            stream << "addn p ";
            stream << positionHash();
            stream << " [";
            int i = 0;
            for (quint64 node : m_nodes) {
                if (i)
                    stream << " ";
                stream << node;
                ++i;
            }
            stream << "]";
            qDebug().noquote() << string;
#endif
        }

    private:
        Game::Position m_position;
        QVector<quint64> m_nodes;
        friend class Node;
        friend class Tests;
    };

    Node();
    ~Node();

    static Node *playout(Node *root, int *vldMax, int *tryPlayoutLimit, bool *hardExit);
    static float minimax(Node *, int depth, bool *isExact, WorkerInfo *info);
    static void validateTree(const Node *);
    static quint64 nextHash();

    void initialize(quint64 hash, quint64 parentHash, const Game &game, Node::Position *nodePosition);
    bool deinitialize();
    static Node *relink(quint64 hash);

    int treeDepth() const;
    bool isExact() const;
    bool isTrueTerminal() const;
    bool isTB() const;
    bool isDirty() const;
    float uCoeff() const;

    quint32 visits() const;
    quint32 virtualLoss() const;

    // parents and children
    QVector<quint64> parents() const;
    void addParent(quint64 parent);
    bool hasParent(quint64 parent) const;

    QVector<const Node*> embodiedParents() const;

    Node::Child findChild(const Move &child) const;

    Node *embodiedBestChild() const;
    QVector<Node*> embodiedChildren() const;
    bool hasEmbodiedChild(Node::Child *childRef) const;
    Node* embodiedChild(Node::Child *childRef) const;
    bool allChildrenPruned() const;

    inline bool hasChildren() const { return !m_children.isEmpty(); }
    inline QVector<Node::Child> *children() { return &m_children; }
    inline const QVector<Node::Child> *children() const { return &m_children; }

    bool isNotExtendable() const;

    void scoreMiniMax(float score, bool isExact);
    bool isAlreadyPlayingOut() const;

    int count() const;

    void incrementVisited();

    // flag saying we are in midst of scoring
    bool setScoringOrScored()
    {
        return m_scoringOrScored.test_and_set(); // atomic
    }

    // child generation
    bool checkAndGenerateDTZ(int *dtz);
    void generateChildren();
    void reserveChildren(int totalSize);
    Node::Child *generateChild(const Move &move);
    Node *generateEmbodiedChild(Node::Child *child);
    static Node *generateEmbodiedNode(const Move &move, float, Node *parent);

    // children
    const Node *findEmbodiedSuccessor(const QVector<QString> &child) const;

    inline const Game &game() const { return m_game; }
    inline quint64 hash() const { return m_hash; }

    // back propagation
    void backPropagateValue(float qValue);
    void backPropagateValueFull();
    void setQValueAndPropagate();
    void backPropagateDirty();

    QVector<Game> previousMoves(bool fullHistory) const; // slow

    int depth() const;

    bool isRootNode() const;
    void setAsRootNode();

    quint64 parent() const;
    bool hasEmbodiedParent() const;
    Node* embodiedParent() const;

    QString principalVariation(int *depth, bool *isTB, bool pin) const; // recursive

    QString toString(Chess::NotationType = Chess::Computer) const;
    QString printTree(int topDepth, int depth, bool printPotentials) const; // recursive

    bool isCheckMate() const { return m_game.lastMove().isCheckMate(); }
    bool isStaleMate() const { return m_game.lastMove().isStaleMate(); }
    int repetitions() const;
    bool isThreeFold() const;
    bool isNoisy() const;

    static bool greaterThan(const Node *a, const Node *b);
    static void sortByScore(QVector<Node*> &nodes, bool partialSortFirstOnlyy);
    static void sortByPVals(QVector<Node::Child> &children);

    Node::Position *position() const;

    bool hasQValue() const;
    float qValueDefault() const;
    float qValue() const;
    void setQValueFromRaw();

    bool hasRawQValue() const;
    float rawQValue() const;
    void setRawQValue(float qValue);

    bool hasPValue() const;
    float pValue() const;
    void setPValue(float pValue);

    float uValue() const;
    float weightedExplorationScore() const;

private:
    Game m_game;                        // 8
    quint64 m_parent;                   // 8
    Node::Position *m_position;         // 8
    quint64 m_hash;                     // 8
    QVector<Node::Child> m_children;    // 8
    quint32 m_visited;                  // 4
    quint32 m_virtualLoss;              // 4
    float m_qValue;                     // 4
    float m_rawQValue;                  // 4
    float m_pValue;                     // 4
    float m_policySum;                  // 4
    float m_uCoeff;                     // 4
    quint16 m_pruned;                   // 2
    bool m_isExact: 1;                  // 1
    bool m_isTB: 1;                     // 1
    bool m_isDirty: 1;                  // 1
    std::atomic_flag m_scoringOrScored; // 1
    friend class SearchWorker;
    friend class SearchEngine;
    friend class Tests;
    friend class Tree;
};

inline int Node::treeDepth() const
{
    int d = 0;
    const Node *n = this;
    while (n) {
        QVector<Node*> children = n->embodiedChildren();
        if (children.isEmpty())
            break;
        Node::sortByScore(children, true /*partialSortFirstOnly*/);
        n = children.first();
        ++d;
    }
    return d;
}

inline bool Node::isExact() const
{
    return m_isExact;
}

inline bool Node::isTrueTerminal() const
{
    return m_isExact && m_children.isEmpty();
}

inline bool Node::isTB() const
{
    return m_isTB;
}

inline bool Node::isDirty() const
{
    return m_isDirty;
}

inline int Node::count() const
{
    int c = isRootNode() ? 0 : 1; // me, myself, and I
    QVector<Node*> children = embodiedChildren();
    for (const Node *n : children)
        c += n->count();
    return c;
}

inline bool Node::isNotExtendable() const
{
    // If we don't have children (either exact or haven't generated them yet)
    // or if our children don't have pValues then we are not extendable
    return !hasChildren() || !m_children.first().hasPValue();
}

inline float Node::uCoeff() const
{
    return m_uCoeff;
}

inline quint32 Node::visits() const
{
    return m_visited;
}

inline quint32 Node::virtualLoss() const
{
    return m_virtualLoss;
}

inline bool Node::isAlreadyPlayingOut() const
{
    return !m_visited && m_virtualLoss > 0;
}

inline int Node::depth() const
{
    int d = 0;
    const Node *parent = hasEmbodiedParent() ? embodiedParent() : nullptr;
    while (parent) {
        parent = parent->hasEmbodiedParent() ? parent->embodiedParent() : nullptr;
        d++;
    }
    return d;
}

inline bool Node::isRootNode() const
{
    return m_parent == 0;
}

inline void Node::setAsRootNode()
{
    m_parent = 0;
}

inline quint64 Node::parent() const
{
    return m_parent;
}

inline bool Node::greaterThan(const Node *a, const Node *b)
{
    if (!a->m_visited)
        return a->pValue() > b->pValue();
    else
        return a->qValue() > b->qValue();
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

inline void Node::sortByPVals(QVector<Node::Child> &children)
{
    std::stable_sort(children.begin(), children.end(),
        [=](const Node::Child &a, const Node::Child &b) {
        return a.pValue() > b.pValue();
    });
}

inline Node::Position *Node::position() const
{
    Q_ASSERT(m_position);
    return m_position;
}

inline float Node::qValueDefault() const
{
#if defined(USE_PARENT_QVALUE)
    return -qValue() - SearchSettings::fpuReduction * float(qSqrt(qreal(m_policySum)));
#else
    return -1.0f;
#endif
}

inline bool Node::hasQValue() const
{
    return !qFuzzyCompare(m_qValue, -2.0f);
}

inline float Node::qValue() const
{
    if (m_visited > 0)
        return m_qValue;
    return embodiedParent()->qValueDefault();
}

inline void Node::setQValueFromRaw()
{
    Q_ASSERT(hasRawQValue());
    if (!m_visited)
        m_qValue = m_rawQValue;
}

inline bool Node::hasRawQValue() const
{
    return !qFuzzyCompare(m_rawQValue, -2.0f);
}

inline float Node::rawQValue() const
{
    return m_rawQValue;
}

inline void Node::setRawQValue(float rawQValue)
{
    m_rawQValue = rawQValue;
}

inline bool Node::hasPValue() const
{
    return !qFuzzyCompare(m_pValue, -2.0f);
}

inline float Node::pValue() const
{
    return m_pValue;
}

inline void Node::setPValue(float pValue)
{
    m_pValue = pValue;
}

inline float Node::uValue() const
{
    const qint64 n = m_visited + m_virtualLoss;
    const float p = m_pValue;
    Q_ASSERT(hasEmbodiedParent());
    return embodiedParent()->uCoeff() * p / (n + 1);
}

inline float Node::weightedExplorationScore() const
{
    const float q = qValue();
    return q + uValue();
}

inline quint64 fixedHash(const Node::Position &node)
{
    return node.positionHash();
}

inline quint64 fixedHash(const Node &node)
{
    return node.hash();
}

QDebug operator<<(QDebug debug, const Node &node);

#endif // NODE_H
