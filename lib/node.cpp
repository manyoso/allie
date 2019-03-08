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

#include "node.h"

#include "history.h"
#include "notation.h"
#include "neural/nn_policy.h"
#include "tb.h"

int scoreToCP(float score)
{
    // Same formula as lc0
    return qRound(290.680623072 * qTan(1.548090806 * double(score)));
}

float cpToScore(int cp)
{
    // Inverse of the above
    return float(qAtan(double(cp) / 290.680623072) / 1.548090806);
}

Node::Node(Node *parent, const Game &game)
    : m_game(game),
    m_parent(parent),
    m_visited(0),
    m_virtualLoss(0),
    m_qValue(-2.0f),
    m_rawQValue(-2.0f),
    m_pValue(-2.0f),
    m_uCoeff(-2.0f),
    m_isExact(false),
    m_isPrefetch(false)
{
    m_scoringOrScored.clear();
}

Node::~Node()
{
    qDeleteAll(m_potentials);
    m_potentials.clear();
}

QVector<Game> Node::previousMoves(bool fullHistory) const
{
    const int previousMoveCount = 11;
    QVector<Game> result;
    Node *parent = m_parent;

    // Get our parents history
    while (parent && (fullHistory || result.count() < previousMoveCount)) {
        result.prepend(parent->game());
        parent = parent->m_parent;
    }

    // Get history from the history list
    if (fullHistory || result.count() < previousMoveCount) {
        QVector<Game> h = History::globalInstance()->games();
        if (!h.isEmpty())
            h.takeLast(); // already captured current root

        while (!h.isEmpty() && (fullHistory || result.count() < previousMoveCount)) {
            Game g = h.takeLast();
            result.prepend(g);
        }
    }

    return result;
}

bool Node::isSecondChild() const
{
    if (isRootNode())
        return false;
    const QVector<Node*> &parentsChildren = m_parent->m_children;
    if (parentsChildren.count() < 2)
        return false;
    return !isFirstChild() && (parentsChildren.at(0) == this || parentsChildren.at(1) == this);
}

Node *Node::rootNode()
{
    if (isRootNode())
        return this;
    return m_parent->rootNode();
}

const Node *Node::rootNode() const
{
    if (isRootNode())
        return this;
    return m_parent->rootNode();
}

void Node::setAsRootNode()
{
    // Need to remove ourself from our parent's children
    if (m_parent) {
        const int index = m_parent->m_children.indexOf(this);
        Q_ASSERT(index != -1);
        m_parent->m_children.remove(index);

    }
    // Now we have no parent
    m_parent = nullptr;
}

QString Node::principalVariation(int *depth, Strategy strategy) const
{
    if (!isRootNode() && !hasPValue())
        return QString();

    *depth += 1;

    if (!hasChildren())
        return Notation::moveToString(m_game.lastMove(), Chess::Computer);

    QVector<Node*> children = m_children;
    sortByScore(children, true /*partialSortFirstOnly*/, strategy);
    Node *bestChild = children.first();
    if (isRootNode())
        return bestChild->principalVariation(depth, strategy);
    else
        return Notation::moveToString(m_game.lastMove(), Chess::Computer)
            + " " + bestChild->principalVariation(depth, strategy);
}

int Node::repetitions() const
{
    if (m_game.repetitions() != -1)
        return m_game.repetitions();

    qint8 r = 0;
    const QVector<Game> previous = this->previousMoves(true /*fullHistory*/);
    QVector<Game>::const_reverse_iterator it = previous.crbegin();
    for (; it != previous.crend(); ++it) {
        if (m_game.isSamePosition(*it))
            ++r;

        if (r >= 2)
            break; // No sense in counting further

        if (!(*it).halfMoveClock())
            break;
    }
    const_cast<Node*>(this)->m_game.setRepetitions(r);
    return m_game.repetitions();
}

bool Node::isThreeFold() const
{
    // If this position has been found at least twice in the past, then this is a threefold draw
    return repetitions() >= 2;
}

void Node::setQValueFromRaw()
{
    Q_ASSERT(hasRawQValue());
    m_qValue = m_rawQValue;
}

void Node::setRawQValue(float qValue)
{
    m_rawQValue = qValue;
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "sq " << toString() << " v:" << qValue;
#endif
}

void Node::backPropagateValue(float v)
{
    const float currentQValue = hasQValue() ? m_qValue : 0.0f;
    const float n = qMax(quint32(1), m_visited);
    m_qValue = (n * currentQValue + v) / (n + 1.f);
    incrementVisited();
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << n
        << "v:" << v << "oq:" << currentQValue << "fq:" << m_qValue;
#endif
}

void Node::backPropagateValueFull()
{
    float v = qValue();
    Node *parent = m_parent;
    while (parent) {
        v = -v; // flip
        parent->backPropagateValue(v);
        parent = parent->m_parent;
    }
}

void Node::setQValueAndPropagate()
{
    Q_ASSERT(hasRawQValue());
    incrementVisited();
    setQValueFromRaw();
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << m_visited
        << "v:" << m_rawQValue << "oq:" << 0.0 << "fq:" << m_qValue;
#endif
    backPropagateValueFull();
}

class MCTSNode {
public:
    MCTSNode(Node* parent, PotentialNode* potential)
        : m_node(nullptr),
        m_parent(parent),
        m_potential(potential)
    {
    }

    MCTSNode(Node* node)
        : m_node(node),
        m_parent(nullptr),
        m_potential(nullptr)
    {
    }

    bool isPotential() const { return m_potential; }
    bool isNull() const { return !m_node && !m_potential; }

    float uCoeff() const
    {
        if (isPotential())
            return s_kpuct;
        return m_node->uCoeff();
    }

    float pValue() const
    {
        if (isPotential())
            return m_potential->pValue();
        return m_node->pValue();
    }

    float qValue() const
    {
        if (isPotential())
            return m_parent->qValueDefault();
        return m_node->qValue();
    }

    float uValue() const
    {
        if (isPotential())
            return m_parent->uCoeff() * m_potential->pValue();
        return m_node->uValue();
    }

    float weightedExplorationScore() const
    {
        if (isPotential())
            return qValue() + uValue();
        return m_node->weightedExplorationScore();
    }

    // Creates the node if necessary
    Node *actualNode(bool *created) const
    {
        if (isPotential()) {
            *created = true;
            return m_parent->generateChild(m_potential);
        }
        *created = false;
        return m_node;
    }

private:
    Node *m_node;
    Node *m_parent;
    PotentialNode *m_potential;
};

int virtualLossDistance(float wec, const MCTSNode &a, const MCTSNode &b)
{
    Q_UNUSED(a);
    // Calculate the number of visits (or "virtual losses") necessary to drop an item below another
    // in weighted exploration score
    // We have...
    //     wec = q + ((kpuct * sqrt(N)) * p / (n + 1))
    // Solving for n...
    //     n = (q + p * kpuct * sqrt(N) - wec) / (wec - q) where wec - q != 0
    const float q = b.qValue();
    const float p = b.pValue();
    const float uCoeff = b.uCoeff();
    if (qFuzzyCompare(wec - q, 0.0f))
        return 1;
    else if (q > wec)
        return 9999;
    const float nf = -(q + p * uCoeff - wec) / (wec - q);
    const int n = qMax(0, qCeil(qreal(nf)));
    return n;
}

Node *Node::playout(int *depth, bool *createdNode)
{
    int tryPlayoutLimit = 256;
    int vldMax = 9999;

start_playout:
    int d = 0;
    int vld = vldMax;
    Node *n = this;
    forever {
        ++d;

        // If we've never been scored or this is an exact node, then this is our playout node
        if (!n->setScoringOrScored() || n->isExact()) {
            ++n->m_virtualLoss;
#if defined(DEBUG_PLAYOUT_MCTS)
            qDebug() << "score hit" << n->toString() << "n" << n->m_visited
                     << "virtualLoss" << n->m_virtualLoss;
#endif
            break;
        }

        // Otherwise, increase virtual loss
        const bool alreadyPlayingOut = n->isAlreadyPlayingOut();
        const qint64 increment = alreadyPlayingOut ? qint64(vld - 1) : 1;
        n->m_virtualLoss += increment;
#if defined(DEBUG_PLAYOUT_MCTS)
        qDebug() << "increment hit" << n->toString() << "n" << n->m_visited
                 << "virtualLoss" << n->m_virtualLoss;
#endif

        // If we've already calculated virtualLossDistance or we are not extendable,
        // then decrement the try and vld limits and check if we should exit
        if (alreadyPlayingOut || n->isNotExtendable()) {
            --tryPlayoutLimit;
#if defined(DEBUG_PLAYOUT_MCTS)
            qDebug() << "decreasing try for" << n->toString() << tryPlayoutLimit;
#endif
            if (tryPlayoutLimit <= 0)
                return nullptr;

            vldMax -= n->m_virtualLoss;
#if defined(DEBUG_PLAYOUT_MCTS)
            qDebug() << "decreasing vldMax for" << n->toString() << vldMax;
#endif
            if (vldMax <= 0)
                return nullptr;

            goto start_playout;
        }

        // Otherwise calculate the virtualLossDistance to advance past this node
        Q_ASSERT(hasChildren() || hasPotentials());

        MCTSNode firstNode = n->leftChild();
        MCTSNode secondNode = nullptr;
        float bestScore = -1.0f;
        float secondBestScore = -1.0f;

        // First look at the actual children
        for (Node *child : n->m_children) {
            MCTSNode mctsNode(child);
            float score = mctsNode.weightedExplorationScore();
            if (firstNode.isNull() || score > bestScore) {
                secondNode = firstNode;
                secondBestScore = bestScore;
                firstNode = mctsNode;
                bestScore = score;
            } else if (secondNode.isNull() || score > secondBestScore) {
                secondNode = mctsNode;
                secondBestScore = score;
            }
        }

        // Then look for potential children
        for (PotentialNode *potential : n->m_potentials) {
            MCTSNode mctsNode(n, potential);
            float score = mctsNode.weightedExplorationScore();
            if (firstNode.isNull() || score > bestScore) {
                secondNode = firstNode;
                secondBestScore = bestScore;
                firstNode = mctsNode;
                bestScore = score;
            } else if (secondNode.isNull() || score > secondBestScore) {
                secondNode = mctsNode;
                secondBestScore = score;
            }
        }

        Q_ASSERT(!firstNode.isNull());
        if (!secondNode.isNull()) {
            const int vldNew
                = virtualLossDistance(bestScore, firstNode, secondNode);
            if (!vld)
                vld = vldNew;
            else
                vld = qMin(vld, vldNew);
        }

        // Retrieve the actual first node
        bool created = false;
        n = firstNode.actualNode(&created);

        // If we created any nodes, then update to indicate
        if (created)
            *createdNode = true;
    }

    *depth = d;
    return n;
}

void Node::incrementVisited()
{
    m_uCoeff = -2.0f;
    m_virtualLoss = 0;
    ++m_visited;
}

bool Node::isNoisy() const
{
    const Move mv = m_game.lastMove();
    return mv.isCapture() || mv.isCheck() || mv.promotion() != Chess::Unknown;
}

bool Node::hasNoisyChildren() const
{
    for (Node *node : m_children)
        if (node->isNoisy())
            return true;
    return false;
}

bool Node::generatePotentials()
{
    Q_ASSERT(!hasPotentials());
    if (hasPotentials())
        return false;

    // Check if this is drawn by rules
    if (Q_UNLIKELY(m_game.halfMoveClock() >= 100)) {
        m_rawQValue = 0.0f;
        m_isExact = true;
        return false;
    } else if (Q_UNLIKELY(m_game.isDeadPosition())) {
        m_rawQValue = 0.0f;
        m_isExact = true;
        return false;
    } else if (Q_UNLIKELY(isThreeFold())) {
        m_rawQValue = 0.0f;
        m_isExact = true;
        return false;
    }

    const TB::Probe result = isRootNode() ? TB::NotFound : TB::globalInstance()->probe(m_game);
    switch (result) {
    case TB::NotFound:
        break;
    case TB::Win:
        m_rawQValue = 1.0f;
        m_isExact = true;
        return true;
    case TB::Loss:
        m_rawQValue = -1.0f;
        m_isExact = true;
        return true;
    case TB::Draw:
        m_rawQValue = 0.0f;
        m_isExact = true;
        return true;
    }

    // Otherwise try and generate potential moves
    m_game.pseudoLegalMoves(this);

    // Override the NN in case of checkmates or stalemates
    if (!hasPotentials()) {
        bool isChecked = m_game.isChecked(m_game.activeArmy());
        if (isChecked) {
            m_game.setCheckMate(true);
            m_rawQValue = 1.0f + (MAX_DEPTH * 0.0001f) - (depth() * 0.0001f);
            m_isExact = true;
        } else {
            m_game.setStaleMate(true);
            m_rawQValue = 0.0f;
            m_isExact = true;
        }
        Q_ASSERT(isCheckMate() || isStaleMate());
    }
    return false;
}

void Node::generatePotential(const Move &move)
{
    Q_ASSERT(move.isValid());
    Game g = m_game;
    if (!g.makeMove(move))
        return; // illegal

    if (g.isChecked(m_game.activeArmy()))
        return; // illegal

    m_potentials.append(new PotentialNode(move));
}

Node *Node::generateChild(PotentialNode *potential)
{
    Q_ASSERT(potential);
    Game g = m_game;
    const bool success = g.makeMove(potential->move());
    Q_ASSERT(success);
    Node *child = new Node(this, g);
    child->setPValue(potential->pValue());
    m_children.append(child);
    m_potentials.removeAll(potential);
    delete potential;
    return child;
}

QString Node::toString(Chess::NotationType notation) const
{
    QString string;
    QTextStream stream(&string);
    QVector<Game> games = previousMoves(false /*fullHistory*/);
    games << m_game;
    QVector<Game>::const_iterator it = games.begin();
    for (int i = 0; it != games.end(); ++it, ++i) {
        stream << (*it).toString(notation);
        stream << (i != games.count() - 1 ? " " : "");
    }
    return string;
}

QString Node::printTree(int depth) /*const*/
{
    const Strategy strategy = MCTS;
    QString tree;
    QTextStream stream(&tree);
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream << "\n";
    const int d = this->depth();
    for (int i = 0; i < d; ++i)
        stream << qSetFieldWidth(7) << "      |";

    Move mv = m_game.lastMove();

    QString move = QString("%1").arg(mv.isValid() ? Notation::moveToString(mv) : "start");
    QString i = QString("%1").arg(mv.isValid() ? QString::number(moveToNNIndex(mv)) : "----");
    stream
        << right << qSetFieldWidth(6) << move
        << qSetFieldWidth(2) << " ("
        << qSetFieldWidth(4) << i
        << qSetFieldWidth(1) << ")"
        << qSetFieldWidth(4) << left << " n: " << qSetFieldWidth(4) << right << m_visited + m_virtualLoss
        << qSetFieldWidth(4) << left << " p: " << qSetFieldWidth(5) << qSetRealNumberPrecision(2) << right << pValue() * 100 << qSetFieldWidth(1) << left << "%"
        << qSetFieldWidth(4) << left << " q: " << qSetFieldWidth(8) << qSetRealNumberPrecision(5) << right << qValue()
        << qSetFieldWidth(4) << " u: " << qSetFieldWidth(6) << qSetRealNumberPrecision(5) << right << uValue()
        << qSetFieldWidth(4) << " q+u: " << qSetFieldWidth(8) << qSetRealNumberPrecision(5) << right << weightedExplorationScore()
        << qSetFieldWidth(4) << " v: " << qSetFieldWidth(7) << qSetRealNumberPrecision(4) << right << rawQValue()
        << qSetFieldWidth(4) << " h: " << qSetFieldWidth(2) << right << qMax(1, treeDepth(strategy) - d)
        << qSetFieldWidth(4) << " cp: " << qSetFieldWidth(2) << right << scoreToCP(qValue());

    if (d < depth) {
        QVector<Node*> children = m_children;
        if (!children.isEmpty()) {
            sortByScore(children, false /*partialSortFirstOnly*/, strategy);
            for (Node *child : children)
                stream << child->printTree(depth);
        }
    }

    return tree;
}

QDebug operator<<(QDebug debug, const Node &node)
{
    QVector<Game> games = node.previousMoves(false /*fullHistory*/);
    games << node.game();
    debug.nospace() << "Node(\"";
    debug.noquote() << node.toString();
    debug << "\", qVal:" << node.qValue() << ", pVal:" << node.pValue() << ")";
    return debug.space();
}
