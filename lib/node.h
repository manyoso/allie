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
    class Potential {
    public:
        Potential()
        {
            m_pValue = -2.0f;
            Q_ASSERT(!m_move.isValid());
        }

        Potential(const Move &move)
        {
            m_pValue = -2.0f;
            m_move = move;
            Q_ASSERT(m_move.isValid());
        }

        inline bool hasPValue() const { return !qFuzzyCompare(m_pValue, -2.0f); }
        inline float pValue() const { return m_pValue; }
        inline void setPValue(float pValue) { m_pValue = pValue; }
        inline Move move() const { return m_move; }
        inline bool isValid() const { return m_move.isValid(); }

        inline QString toString() const { return Notation::moveToString(m_move, Chess::Computer); }
        bool operator==(const Potential &other) const { return m_move == other.m_move; }

    private:
        Move m_move;
        float m_pValue;
    };

    class Playout {
    public:
        Playout()
            : m_potential(nullptr)
            , m_isPotential(true)
        { }

        Playout(Node *node)
            : m_node(node)
            , m_isPotential(false)
        { }

        Playout(Potential *potential)
            : m_potential(potential)
            , m_isPotential(true)
        { }

        inline bool isNull() const { return m_isPotential && !m_potential; }
        inline Node *node() const { return m_node; }
        inline Potential *potential() const { return m_potential; }
        inline bool isPotential() const { return m_isPotential; }
        inline bool hasPValue() const { return !qFuzzyCompare(pValue(), -2.0f); }

        inline bool operator==(const Playout &other) const
        {
            return m_isPotential == other.m_isPotential && (m_isPotential ? m_potential == other.m_potential : m_node == other.m_node);
        }

        // For playouts
        inline float pValue() const
        {
            if (isPotential())
                return potential()->pValue();
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

    private:
        union {
            Node *m_node;
            Potential *m_potential;
        };
        bool m_isPotential : 1;
    };

    class Position {
    public:
        Position();
        ~Position();

        void initialize(Node *node, const Game::Position &position);
        bool deinitialize(bool forcedFree);
        void addNode(Node *node);
        void removeNode(Node *node);
        static Node::Position *relink(quint64 positionHash, Cache *cache);
        inline bool hasNode(Node *node) const { return m_nodes.contains(node); }
        inline const QVector<Node*>& nodes() const { return m_nodes; }
        inline bool hasPotentials() const { return !m_potentials.isEmpty(); }
        inline QVector<Potential> *potentials() { return &m_potentials; }
        inline const QVector<Potential> *potentials() const { return &m_potentials; }
        inline const Game::Position &position() const { return m_position; }
        inline quint64 positionHash() const { return m_position.positionHash(); }

    private:
        Game::Position m_position;
        QVector<Node*> m_nodes;
        QVector<Potential> m_potentials;
        friend class Node;
        friend class Tests;
    };

    Node();
    ~Node();

    static Node *playout(Node *root, int *vldMax, int *tryPlayoutLimit, bool *hardExit, Cache *hash);
    static float minimax(Node *, int depth, bool *isExact, WorkerInfo *info);
    static void validateTree(const Node *);
    static float uctFormula(float qValue, float uValue, quint64 visits);
    static int virtualLossDistance(float swec, float uCoeff, float q, float p, int currentVisits);

    void initialize(Node *parent, const Game &game, Node::Position *nodePosition);
    bool deinitialize(bool forcedFree);

    int treeDepth() const;
    bool isExact() const;
    bool isTrueTerminal() const;
    bool isTB() const;
    bool isDirty() const;
    float uCoeff() const;

    quint32 visits() const;
    quint32 virtualLoss() const;

    // parents and children
    Node *bestChild() const;
    bool hasPotentials() const;

    inline bool hasChildren() const { return !m_children.isEmpty(); }
    inline QVector<Node*> *children() { return &m_children; }
    inline const QVector<Node*> *children() const { return &m_children; }

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
    void generatePotentials();
    void reservePotentials(int totalSize);
    Node::Potential *generatePotential(const Move &move);
    Node *generateNextChild(Cache *cache, NodeGenerationError *error);
    static Node *generateNode(const Move &move, float, Node *parent, Cache *cache, NodeGenerationError *error);

    // children
    const Node *findSuccessor(const QVector<QString> &child) const;

    inline const Game &game() const { return m_game; }

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
    static void sortByPVals(QVector<Node::Potential> &potentials);

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

    float uValue(const float uCoeff) const;

private:
    Game m_game;                        // 8
    Node *m_parent;                     // 8
    Node::Position *m_position;         // 8
    QVector<Node*> m_children;          // 8
    quint32 m_visited;                  // 4
    quint32 m_virtualLoss;              // 4
    float m_qValue;                     // 4
    float m_rawQValue;                  // 4
    float m_pValue;                     // 4
    float m_policySum;                  // 4
    float m_uCoeff;                     // 4
    quint8 m_potentialIndex;            // 2
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
    for (Node *n : m_children)
        c += n->count();
    return c;
}

inline bool Node::isNotExtendable() const
{
    // If we don't have children or potentials (either exact or haven't generated them yet)
    // or if our children or potentials don't have pValues then we are not extendable
    Q_ASSERT(m_position);
    return (!hasChildren() || !m_children.first()->hasPValue())
        && (!hasPotentials() || !m_position->potentials()->first().hasPValue());
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

inline bool Node::hasPotentials() const
{
    Q_ASSERT(m_position);
    return m_potentialIndex != m_position->m_potentials.count();
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
    if (m_parent)
        m_parent->m_children.removeAll(this);

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
            [](const Node *a, const Node *b) {
            return greaterThan(a, b);
        });
    } else {
        std::stable_sort(nodes.begin(), nodes.end(),
            [](const Node *a, const Node *b) {
            return greaterThan(a, b);
        });
    }
}

inline void Node::sortByPVals(QVector<Node::Potential> &potentials)
{
    std::stable_sort(potentials.begin(), potentials.end(),
        [](const Node::Potential &a, const Node::Potential &b) {
        return a.pValue() > b.pValue();
    });
}

inline Node::Position *Node::position() const
{
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

inline quint64 fixedHash(const Node::Position &position)
{
    return position.positionHash();
}

inline bool isPinned(const Node::Position &position)
{
    return !position.nodes().isEmpty();
}

inline bool isPinned(Node *node)
{
    return node->position();
}

QDebug operator<<(QDebug debug, const Node &node);

#endif // NODE_H
