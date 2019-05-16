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
    m_policySum(0),
    m_uCoeff(-2.0f),
    m_isExact(false),
    m_isTB(false),
    m_isDirty(false)
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
    // This is slow because we build up a vector by prepending it
    const int previousMoveCount = 11;
    QVector<Game> result;
    HistoryIterator it = HistoryIterator::begin(this);
    ++it; // advance past this position

    for (; it != HistoryIterator::end() &&
         (fullHistory || result.count() < previousMoveCount);
         ++it) {
        result.prepend(*it);
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

QString Node::principalVariation(int *depth, bool *isTB) const
{
    if (!isRootNode() && !hasPValue()) {
        *isTB = m_isTB;
        return QString();
    }

    *depth += 1;

    if (!hasChildren()) {
        *isTB = m_isTB;
        return Notation::moveToString(m_game.lastMove(), Chess::Computer);
    }

    QVector<Node*> children = m_children;
    sortByScore(children, true /*partialSortFirstOnly*/);
    Node *bestChild = children.first();
    if (isRootNode())
        return bestChild->principalVariation(depth, isTB);
    else
        return Notation::moveToString(m_game.lastMove(), Chess::Computer)
            + " " + bestChild->principalVariation(depth, isTB);
}

int Node::repetitions() const
{
    if (m_game.repetitions() != -1)
        return m_game.repetitions();

    qint8 r = 0;
    HistoryIterator it = HistoryIterator::begin(this);
    ++it; // advance past this position
    for (; it != HistoryIterator::end(); ++it) {
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
    backPropagateDirty();
}

void Node::backPropagateValue(float v)
{
    Q_ASSERT(hasQValue());
    Q_ASSERT(m_visited);
    const float currentQValue = m_qValue;
    m_qValue = (m_visited * currentQValue + v) / float(m_visited + 1);
    incrementVisited();
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << m_visited
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
    if (m_parent && !m_visited)
        m_parent->m_policySum += pValue();
    incrementVisited();
    setQValueFromRaw();
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << m_visited
        << "v:" << m_rawQValue << "oq:" << 0.0 << "fq:" << m_qValue;
#endif
    backPropagateValueFull();
}

void Node::backPropagateDirty()
{
    Q_ASSERT(hasRawQValue());
    m_isDirty = true;
    Node *parent = m_parent;
    while (parent) {
        parent->m_isDirty = true;
        parent = parent->m_parent;
    }
}

void Node::scoreMiniMax(float score, bool isExact)
{
    Q_ASSERT(!qFuzzyCompare(qAbs(score), 2.f));
    m_isExact = isExact;
    if (isExact)
        m_qValue = score;
    else
        m_qValue = (m_visited * m_qValue + score) / float(m_visited + 1);
    ++m_visited;
}

float Node::minimax(Node *node, bool *isExact)
{
    Q_ASSERT(node);
    Q_ASSERT(node->hasRawQValue());

    const bool isTrueTerminal = node->isTrueTerminal();

    // First we look to see if this node has been scored or if it is a dirty terminal
    if (!node->hasQValue() || (isTrueTerminal && node->m_isDirty)) {
        Q_ASSERT(node->m_isDirty);
        *isExact = node->isTrueTerminal();
        node->setQValueAndPropagate();
        return node->m_qValue;
    }

    // If we are an exact node, then we are terminal so just return the score
    if (isTrueTerminal)
        return node->m_qValue;

    // However, if the subtree is not dirty, then we can just return our score
    if (!node->m_isDirty)
        return node->m_qValue;

    // At this point we should have children
    Q_ASSERT(node->hasChildren());

    // Search the children
    float best = -2.0f;
    bool bestIsExact = false;
    bool everythingScored = true;
    for (int index = 0; index < node->m_children.count(); ++index) {
        Node *child = node->m_children.at(index);

        // If the child doesn't have a raw qValue then it has not been scored yet so just continue
        if (!child->hasRawQValue()) {
            everythingScored = false;
            continue;
        }

        bool subtreeIsExact = false;
        float score = minimax(child, &subtreeIsExact);

        // Check if we have a new best child
        if (score > best) {
            bestIsExact = subtreeIsExact;
            best = score;
        }
    }

    // We only propagate exact certainty if the best score from subtree is exact AND either the best
    // score is > 0 (a proven win) OR the potential children of this node have all been played out
    const bool miniMaxComplete = everythingScored && node->m_potentials.isEmpty();
    const bool shouldPropagateExact = bestIsExact && (best > 0 || miniMaxComplete);

    // Score the node based on minimax of children
    node->scoreMiniMax(-best, shouldPropagateExact);

    *isExact = shouldPropagateExact;
    return node->m_qValue;
}

void Node::validateTree(Node *node)
{
    // Goes through the entire tree and verifies that everything that should have a score has one
    // and that nothing is marked as dirty that shouldn't be
    Q_ASSERT(node);
    Q_ASSERT(node->hasRawQValue());
    Q_ASSERT(!node->m_isDirty);
    Q_ASSERT(node->hasQValue());
    for (int index = 0; index < node->m_children.count(); ++index) {
        Node *child = node->m_children.at(index);

        // If the child doesn't have a raw qValue then it has not been scored yet so just continue
        if (!child->hasRawQValue())
            continue;

        validateTree(child);
    }
}

class PlayoutNode {
public:
    PlayoutNode(Node* parent, PotentialNode* potential)
        : m_node(nullptr),
        m_parent(parent),
        m_potential(potential)
    {
    }

    PlayoutNode(Node* node)
        : m_node(node),
        m_parent(nullptr),
        m_potential(nullptr)
    {
        Q_ASSERT(!m_node || !m_node->isRootNode());
    }

    bool isPotential() const { return m_potential; }
    bool isNull() const { return !m_node && !m_potential; }

    QString toString() const
    {
        if (isNull())
            return QLatin1String("Null");
        if (isPotential())
            return m_potential->toString();
        return m_node->toString();
    }

    float uCoeff() const
    {
        if (isPotential())
            return m_parent->uCoeff();
        return m_node->parent()->uCoeff();
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

    bool operator==(const PlayoutNode &other) const
    {
        return m_node == other.m_node && m_parent == other.m_parent && m_potential == other.m_potential;
    }

    bool operator!=(const PlayoutNode &other) const
    {
        return m_node != other.m_node || m_parent != other.m_parent || m_potential != other.m_potential;
    }

private:
    Node *m_node;
    Node *m_parent;
    PotentialNode *m_potential;
};

QPair<Node*, Node*> Node::topTwoChildren() const
{
    // Sort the first two children by score
    QVector<Node*> children = m_children;
    std::partial_sort(children.begin(), children.begin() + 2, children.end(),
            [=](const Node *a, const Node *b) {
            return greaterThan(a, b);
    });

    Node *firstChild = children.at(0);
    Node *secondChild = children.at(1);
    return qMakePair(firstChild, secondChild);
}

Node *Node::playout(int *depth, bool *createdNode)
{
    int tryPlayoutLimit = SearchSettings::tryPlayoutLimit;

start_playout:
    int d = 0;
    Node *n = this;
    forever {
        ++d;

        // If we've never been scored or this is an exact node, then this is our playout node
        if (!n->setScoringOrScored() || n->isTrueTerminal()) {
            ++n->m_virtualLoss;
#if defined(DEBUG_PLAYOUT)
            qDebug() << "score hit" << n->toString() << "n" << n->m_visited
                     << "virtualLoss" << n->m_virtualLoss;
#endif
            break;
        }

        // Otherwise, increase virtual loss if we are not already playing out this node
        const bool alreadyPlayingOut = n->isAlreadyPlayingOut();
        if (!alreadyPlayingOut)
            ++n->m_virtualLoss;

#if defined(DEBUG_PLAYOUT)
        qDebug() << "increment hit" << n->toString() << "n" << n->m_visited
                 << "virtualLoss" << n->m_virtualLoss;
#endif

        // If we're already playing out or we are not extendable, then decrement the try and check
        // if we should exit
        if (alreadyPlayingOut || n->isNotExtendable()) {
            --tryPlayoutLimit;
#if defined(DEBUG_PLAYOUT)
            qDebug() << "decreasing try for" << n->toString() << tryPlayoutLimit;
#endif
            if (tryPlayoutLimit <= 0)
                return nullptr;

            goto start_playout;
        }

        // Otherwise advance past this node
        Q_ASSERT(hasChildren() || hasPotentials());

        PlayoutNode firstNode = nullptr;
        float bestScore = -2.0f;
        // First look at the actual children
        for (int i = 0; i < n->m_children.count(); ++i) {
            Node *child = n->m_children.at(i);
            PlayoutNode PlayoutNode(child);
            float score = PlayoutNode.weightedExplorationScore();
            Q_ASSERT(score > -2.f);
            if (score > bestScore) {
                firstNode = PlayoutNode;
                bestScore = score;
            }
        }

        // Then look at the first potential child as they have now been sorted by pval
        if (!n->m_potentials.isEmpty()) {
            PotentialNode *potential = n->m_potentials.first();
            PlayoutNode PlayoutNode(n, potential);
            float score = PlayoutNode.weightedExplorationScore();
            Q_ASSERT(score > -2.f);
            if (score > bestScore) {
                firstNode = PlayoutNode;
                bestScore = score;
            }
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
    ++m_visited;
    const quint32 N = qMax(quint32(1), m_visited);
#if defined(USE_CPUCT_SCALING)
    // From Deepmind's A0 paper
    // log ((1 + N(s) + cbase)/cbase) + cini
    const float growth = SearchSettings::cpuctF * fastlog((1 + N + SearchSettings::cpuctBase) / SearchSettings::cpuctBase);
#else
    const float growth = 0.0f;
#endif
    m_uCoeff = (SearchSettings::cpuctInit + growth) * float(qSqrt(N));
    m_virtualLoss = 0;
    m_isDirty = false;
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

bool Node::checkAndGenerateDTZ(int *dtz)
{
    Q_ASSERT(isRootNode());
    Move move;
    TB::Probe result = TB::globalInstance()->probeDTZ(m_game, &move, dtz);
    if (result == TB::NotFound)
        return false;

    // Check move is valid
    Game g = m_game;
    const bool success = g.makeMove(move);
    Q_ASSERT(success);
    if (!success)
        return false;

    // Check move is legal
    const bool isIllegal = g.isChecked(m_game.activeArmy());
    Q_ASSERT(!isIllegal);
    if (isIllegal)
        return false;

    // Check that we enpassant is correct
    Q_ASSERT(g.lastMove().isEnPassant() == move.isEnPassant());

    // Is this checkmate?
    if (g.isChecked(g.activeArmy()))
        g.setCheckMate(true);

    // If the move is good, then we generate a real child and set it to dtz
    Node *child = new Node(this, g);
    child->setPValue(1.0f);

    // This is inverted because the probe reports from parent's perspective
    switch (result) {
    case TB::Win:
        child->setRawQValue(1.0f);
        child->m_isExact = true;
        child->m_isTB = true;
        break;
    case TB::Loss:
        child->setRawQValue(-1.0f);
        child->m_isExact = true;
        child->m_isTB = true;
        break;
    case TB::Draw:
        child->setRawQValue(0.0f);
        child->m_isExact = true;
        child->m_isTB = true;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    // If this root has never been scored, then do so now to prevent asserts in back propagation
    if (!hasQValue()) {
        setRawQValue(0.0f);
        setQValueFromRaw();
        ++m_visited;
    }

    child->setQValueAndPropagate();
    m_children.append(child);
    return true;
}

void Node::generatePotentials()
{
    Q_ASSERT(!hasPotentials());
    if (hasPotentials())
        return;

    // Check if this is drawn by rules
    if (Q_UNLIKELY(m_game.halfMoveClock() >= 100)) {
        setRawQValue(0.0f);
        m_isExact = true;
        return;
    } else if (Q_UNLIKELY(m_game.isDeadPosition())) {
        setRawQValue(0.0f);
        m_isExact = true;
        return;
    } else if (Q_UNLIKELY(isThreeFold())) {
        setRawQValue(0.0f);
        m_isExact = true;
        return;
    }

    const TB::Probe result = isRootNode() ? TB::NotFound : TB::globalInstance()->probe(m_game);
    switch (result) {
    case TB::NotFound:
        break;
    case TB::Win:
        setRawQValue(1.0f);
        m_isExact = true;
        m_isTB = true;
        return;
    case TB::Loss:
        setRawQValue(-1.0f);
        m_isExact = true;
        m_isTB = true;
        return;
    case TB::Draw:
        setRawQValue(0.0f);
        m_isExact = true;
        m_isTB = true;
        return;
    }

    // Otherwise try and generate potential moves
    m_game.pseudoLegalMoves(this);

    // Override the NN in case of checkmates or stalemates
    if (!hasPotentials()) {
        bool isChecked = m_game.isChecked(m_game.activeArmy());
        if (isChecked) {
            m_game.setCheckMate(true);
            setRawQValue(1.0f + (MAX_DEPTH * 0.0001f) - (depth() * 0.0001f));
            m_isExact = true;
        } else {
            m_game.setStaleMate(true);
            setRawQValue(0.0f);
            m_isExact = true;
        }
        Q_ASSERT(isCheckMate() || isStaleMate());
    }
    return;
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
        << qSetFieldWidth(4) << " u: " << qSetFieldWidth(6) << qSetRealNumberPrecision(5) << right << (isRootNode() ? 0.0f : uValue())
        << qSetFieldWidth(4) << " q+u: " << qSetFieldWidth(8) << qSetRealNumberPrecision(5) << right << (isRootNode() ? 0.0f : weightedExplorationScore())
        << qSetFieldWidth(4) << " v: " << qSetFieldWidth(7) << qSetRealNumberPrecision(4) << right << rawQValue()
        << qSetFieldWidth(4) << " h: " << qSetFieldWidth(2) << right << qMax(1, treeDepth() - d)
        << qSetFieldWidth(4) << " cp: " << qSetFieldWidth(2) << right << scoreToCP(qValue());

    if (d < depth) {
        QVector<Node*> children = m_children;
        if (!children.isEmpty()) {
            sortByScore(children, false /*partialSortFirstOnly*/);
            for (Node *child : children)
                stream << child->printTree(depth);
        }
#if 0
        QVector<PotentialNode*> potentials = m_potentials;
        if (!potentials.isEmpty()) {
            std::stable_sort(potentials.begin(), potentials.end(),
                [=](const PotentialNode *a, const PotentialNode *b) {
                return a->pValue() > b->pValue();
            });
            for (PotentialNode *p : potentials) {
                stream << "\n";
                const int d = this->depth() + 1;
                for (int i = 0; i < d; ++i)
                    stream << qSetFieldWidth(7) << "      |";
                stream << right << qSetFieldWidth(6) << p->toString()
                    << qSetFieldWidth(2) << " ("
                    << qSetFieldWidth(4) << moveToNNIndex(p->move())
                    << qSetFieldWidth(1) << ")"
                    << qSetFieldWidth(4) << left << " p: " << p->pValue() * 100 << qSetFieldWidth(1) << left << "%";
            }
        }
#endif
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
