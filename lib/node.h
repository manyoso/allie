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

class Cache;

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

        inline bool isPotential() const { return m_isPotential; }
        inline void setPotential(bool p) { m_isPotential = p; }
        inline Node *node() const { return m_node; }
        inline void setNode(Node *node) { m_node = node; }
        inline bool hasPValue() const { return !qFuzzyCompare(pValue(), -2.0f); }
        inline void setPValue(float pValue) { Q_ASSERT(isPotential()); m_pValue = pValue; }
        inline Move move() const { return m_move; }
        inline bool isValid() const { return m_move.isValid(); }
        inline bool operator==(const Child &other) const { return m_move == other.m_move; }
        QString toString() const { return Notation::moveToString(m_move, Chess::Computer); }

        // For playouts
        inline float pValue() const
        {
            if (isPotential())
                return m_pValue;
            return node()->pValue();
        }

        inline float qValue(float parentQValueDefault) const
        {
            if (isPotential() || !node()->m_visited)
                return parentQValueDefault;
            // This method already checks for visited or using parent default so this is faster
            // than using node overload
            return node()->m_qValue;
        }

        inline float uValue(float uCoeff) const
        {
            return uCoeff * pValue() / (visits() + virtualLoss() + 1);
        }

        inline quint32 visits() const
        {
            if (isPotential())
                return 0;
            return node()->visits();
        }

        inline quint32 virtualLoss() const
        {
            if (isPotential())
                return 0;
            return node()->virtualLoss();
        }

        inline void relink(Cache *cache) const
        {
            if (!isPotential() && node())
                Node::relink(node()->hash(), cache);
        }

    private:
        union {
            Node *m_node;
            float m_pValue;
        };
        Move m_move;
        bool m_isPotential : 1;
    };

    class Position {
    public:
        Position();
        ~Position();

        void initialize(Node *node, const Game::Position &position, quint64 positionHash);
        bool deinitialize(bool forcedFree);
        void addNode(Node *node, Cache *cache);
        void removeNode(Node *node, Cache *cache);
        static Node::Position *relink(quint64 positionHash, Cache *cache);
        bool nodesNotInHash() const; // only used for debugging
        inline bool hasNode(Node *node) const
        {
            return m_nodes.contains(node);
        }
        inline QVector<Node*> embodiedNodes() const
        {
            Q_ASSERT(!nodesNotInHash());
            return m_nodes;
        }

        inline const Game::Position &position() const { return m_position; }
        inline quint64 positionHash() const { return m_positionHash; }
        inline QVector<Node*> nodes() const { return m_nodes; }

    private:
        Game::Position m_position;
        quint64 m_positionHash;
        QVector<Node*> m_nodes;
        friend class Node;
        friend class Tests;
    };

    Node();
    ~Node();

    static Node *playout(Node *root, int *vldMax, int *tryPlayoutLimit, bool *hardExit, Cache *hash, QMutex *mutex);
    static float minimax(Node *, int depth, bool *isExact, WorkerInfo *info);
    static void validateTree(const Node *);
    static float uctFormula(float qValue, float uValue, quint64 visits);
    static int virtualLossDistance(float swec, float uCoeff, float q, float p, int currentVisits);
    static quint64 nextHash();

    void initialize(quint64 hash, Node *parent, const Game &game, Node::Position *nodePosition);
    bool deinitialize(bool forcedFree);
    static Node *relink(quint64 hash, Cache *cache);

    int treeDepth() const;
    bool isExact() const;
    bool isTrueTerminal() const;
    bool isTB() const;
    bool isDirty() const;
    float uCoeff() const;

    quint32 visits() const;
    quint32 virtualLoss() const;

    // parents and children
    Node::Child findChild(const Move &child) const;

    Node *bestEmbodiedChild() const;
    QVector<Node*> embodiedChildren() const; // FIXME: expensive do not use in hot paths!
    void nullifyChildRef(Node *child);
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
    enum NodeGenerationError {
        NoError,
        OutOfMemory,
        ParentPruned,
        OutOfPositions
    };

    bool checkAndGenerateDTZ(int *dtz);
    void generateChildren();
    void reserveChildren(int totalSize);
    Node::Child *generateChild(const Move &move);
    Node *generateEmbodiedChild(Node::Child *child, Cache *cache, NodeGenerationError *error);
    static Node *generateEmbodiedNode(const Move &move, float, Node *parent, Cache *cache, NodeGenerationError *error);

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

    Node *parent() const;

    QString principalVariation(int *depth, bool *isTB) const; // recursive

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

    void cloneFromTransposition(Node *firstTransposition);
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

    float uValue(const float uCoeff) const;

private:
    Game m_game;                        // 8
    Node *m_parent;                     // 8
    Node::Position *m_position;         // 8
    quint64 m_hash;                     // 8
    QVector<Node::Child> m_children;    // 8
    quint32 m_refs;                     // 4
    quint32 m_visited;                  // 4
    quint32 m_virtualLoss;              // 4
    float m_qValue;                     // 4
    float m_rawQValue;                  // 4
    float m_pValue;                     // 4
    float m_policySum;                  // 4
    float m_uCoeff;                     // 4
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
    const Node *parent = this->parent();
    while (parent) {
        parent = parent->parent();
        d++;
    }
    return d;
}

inline bool Node::isRootNode() const
{
    return m_parent == nullptr;
}

inline void Node::setAsRootNode()
{
    // Need to remove ourself from our parent's children
    if (m_parent) {
        m_parent->nullifyChildRef(this);
        --m_parent->m_refs;
    }

    // Now we have no parent
    m_parent = nullptr;
    m_isExact = false;
}

inline Node *Node::parent() const
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
    Q_ASSERT(m_visited > 0);
    return -m_qValue - SearchSettings::fpuReduction * float(qSqrt(qreal(m_policySum)));
#else
    return -1.0f;
#endif
}

inline void Node::cloneFromTransposition(Node *firstTransposition)
{
    // Clone the relevant state from first transposition
    Q_ASSERT(firstTransposition->hasRawQValue());
    if (!firstTransposition->isExact()) {
        QVector<Node::Child> childrenToClone = firstTransposition->m_children;
        m_children.reserve(childrenToClone.size());
        for (Node::Child childClone : childrenToClone) {
            Child child(childClone.move());
            if (childClone.isPotential())
                child.setPValue(childClone.pValue());
            else
                child.setPValue(childClone.node()->pValue());
            m_children.append(child);
        }
    }

    m_rawQValue = firstTransposition->m_rawQValue;
    m_isExact = firstTransposition->m_isExact;
    m_isTB = firstTransposition->m_isTB;
}

inline bool Node::hasQValue() const
{
    return !qFuzzyCompare(m_qValue, -2.0f);
}

inline float Node::qValue() const
{
    if (m_visited > 0)
        return m_qValue;
    return parent()->qValueDefault();
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

inline float Node::uValue(const float uCoeff) const
{
    return uCoeff * pValue() / (visits() + virtualLoss() + 1);
}

inline float Node::uctFormula(float qValue, float uValue, quint64 visits)
{
    Q_UNUSED(visits)
    return qValue + uValue;
}

inline int Node::virtualLossDistance(float swec, float uCoeff, float q, float p, int currentVisits)
{
    // Calculate the number of visits (or "virtual losses") necessary to drop an item below another
    // in weighted exploration score
    // We have...
    //     wec = q + ((kpuct * sqrt(N)) * p / (n + 1))
    // Solving for n...
    //     n = (q + p * kpuct * sqrt(N) - wec) / (wec - q) where wec - q != 0

    float wec = swec - std::numeric_limits<float>::epsilon();
    if (qFuzzyCompare(wec - q, 0.0f))
        return 1;
    else if (q > wec)
        return SearchSettings::vldMax;
    const qreal nf = qreal(q + p * uCoeff - wec) / qreal(wec - q);
    int n = qMax(1, qCeil(nf)) - currentVisits;
    if (n > SearchSettings::vldMax)
        return SearchSettings::vldMax;

#ifndef NDEBUG
    const float after = q + uCoeff * p / (currentVisits + n + 1);
    Q_ASSERT(after < swec);
    Q_ASSERT(n != 0);
#endif

    return n;
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
