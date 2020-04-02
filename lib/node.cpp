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

#include "cache.h"
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

Node::Position::Position()
{
    m_transpositionNode = nullptr;
    m_rawQValue = -2.0f;
}

Node::Position::~Position()
{
}

void Node::Position::initialize(Node *node, const Game::Position &position)
{
    if (m_transpositionNode)
        return;

    m_position = position;
    m_transpositionNode = node;
    Q_ASSERT(m_transpositionNode->m_position == this);
    if (m_transpositionNode) {
#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "ctor p ";
        stream << positionHash();
        stream << " [";
        stream << m_transpositionNode;
        stream << "]";
        qDebug().noquote() << string;
#endif
    }
}

bool Node::Position::deinitialize(bool forcedFree)
{
    Q_UNUSED(forcedFree)
    m_position = Game::Position();
    m_transpositionNode = nullptr;
    m_potentials.clear();
    m_rawQValue = -2.0f;
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "dtor p ";
    stream << positionHash();
    qDebug().noquote() << string;
#endif
    return true;
}

Node::Position *Node::Position::relinkOrClone(quint64 positionHash, Cache *cache, bool *cloned)
{
    if (!cache->containsNodePosition(positionHash))
        return nullptr;

    // Update the reference for this position in LRU hash
    Node::Position *p = cache->nodePositionRelinkOrClone(positionHash, cloned);
#if defined(DEBUG_CHURN)
    if (*cloned) {
        QString string;
        QTextStream stream(&string);
        stream << "relk p ";
        stream << positionHash;
        qDebug().noquote() << string;
    }
#endif
    return p;
}

Node::Node()
{
    initialize(nullptr, Game(), nullptr);
}

Node::~Node()
{
}

void Node::initialize(Node *parent, const Game &game, Node::Position *position)
{
    if (parent) {
#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "ref " << " [" << parent << "]";
        qDebug().noquote() << string;
#endif
    }
    m_game = game;
    m_parent = parent;
    m_position = position;
    m_potentialIndex = 0;
    m_children.clear();
    m_visited = 0;
    m_virtualLoss = 0;
    m_qValue = -2.0f;
    m_pValue = -2.0f;
    m_policySum = 0;
    m_uCoeff = -2.0f;
    m_isExact = false;
    m_isTB = false;
    m_isDirty = false;
    m_scoringOrScored.clear();
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "ctor n ";
    stream << m_position->positionHash();
    stream << " [";
    stream << this;
    stream << "]";
    qDebug().noquote() << string;
#endif
}

bool Node::deinitialize(bool forcedFree)
{
    Cache *cache = Cache::globalInstance();
    if (Node *parent = this->parent()) {
        // Remove ourself from parent's child list
        if (forcedFree)
            parent->m_children.removeAll(this);

#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "deref " << " [" << parent << "]";
        qDebug().noquote() << string;

#endif
    }

    // Unlink all children as we do not want to leave them parentless
    for (int i = 0; i < m_children.count(); ++i)
        cache->unlinkNode(m_children.at(i));

    if (m_position && m_position->transposition() == this)
        m_position->clearTransposition();

#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "dtor n ";
    stream << (m_position ? m_position->positionHash() : 0xBAAAAAAD);
    stream << " [";
    stream << this;
    stream << "]";
    qDebug().noquote() << string;
#endif

    m_parent = nullptr;
    m_position = nullptr;
    m_isDirty = false;
    m_children.clear();

    return true;
}

void Node::updateTranspositions() const
{
    if (!m_position->transposition())
        m_position->updateTransposition(const_cast<const Node*>(this));

    for (int i = 0; i < m_children.count(); ++i)
        m_children.at(i)->updateTranspositions();
}

Node *Node::bestChild() const
{
    if (!hasChildren())
        return nullptr;
    QVector<Node*> children = m_children;
    sortByScore(children, true /*partialSortFirstOnly*/);
    return children.first();
}

void Node::scoreMiniMax(float score, bool isExact)
{
    Q_ASSERT(!qFuzzyCompare(qAbs(score), 2.f));
    Q_ASSERT(!m_isExact || isExact);
    m_isExact = isExact;
    if (m_isExact) {
        setRawQValue(score);
        m_qValue = score;
    } else
        m_qValue = qBound(-1.f, (m_visited * m_qValue + score) / float(m_visited + 1), 1.f);
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

void Node::backPropagateValue(float v)
{
    Q_ASSERT(hasQValue());
    Q_ASSERT(m_visited);
    Q_ASSERT(!m_isExact);
    const float currentQValue = m_qValue;
    m_qValue = qBound(-1.f, (m_visited * currentQValue + v) / float(m_visited + 1), 1.f);
    incrementVisited();
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << m_visited
        << "v:" << v << "oq:" << currentQValue << "fq:" << m_qValue;
#endif
}

void Node::backPropagateValueFull()
{
    float v = qValue();
    Node *parent = this->parent();
    while (parent) {
        v = -v; // flip
        parent->backPropagateValue(v);
        parent = parent->parent();
    }
}

void Node::setQValueAndPropagate()
{
    Q_ASSERT(hasRawQValue());
    Node *parent = this->parent();
    if (parent && !m_visited)
        parent->m_policySum += pValue();
    setQValueFromRaw();
    incrementVisited();
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << m_visited
        << "v:" << rawQValue() << "oq:" << 0.0 << "fq:" << m_qValue;
#endif
    backPropagateValueFull();
}

void Node::backPropagateDirty()
{
    Q_ASSERT(hasRawQValue());
    Q_ASSERT(!m_visited || m_isExact);
    m_isDirty = true;

    Node *parent = this->parent();
    while (parent) {
        parent->m_isDirty = true;
        parent = parent->parent();
    }
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
        result.prepend(it.game());
    }

    return result;
}

QString Node::principalVariation(int *depth, bool *isTB) const
{
    if (!isRootNode() && !hasPValue()) {
        *isTB = m_isTB;
        return QString();
    }

    *depth += 1;

    const Node *bestChild = this->bestChild();
    if (!bestChild) {
        *isTB = m_isTB;
        return Notation::moveToString(m_game.lastMove(), Chess::Computer);
    }

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
        if (m_position->position().isSamePosition(it.position()))
            ++r;

        if (r >= 2)
            break; // No sense in counting further

        if (!it.game().halfMoveClock())
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

float Node::minimax(Node *node, int depth, bool *isExact, WorkerInfo *info)
{
    Q_ASSERT(node);
    Q_ASSERT(node->hasRawQValue());

    const bool nodeIsExact = node->isExact();

    // First we look to see if this node has been scored
    if (!node->m_visited) {
        // Record info
        ++(info->nodesSearched);
        ++(info->nodesVisited);
        info->sumDepths += depth;
        info->maxDepth = qMax(info->maxDepth, depth);
        if (node->m_isTB)
            ++(info->nodesTBHits);

        Q_ASSERT(node->m_isDirty);
        *isExact = nodeIsExact;
        node->setQValueAndPropagate();
        return node->m_qValue;
    }

    // Next look if it is a dirty terminal
    if (nodeIsExact && node->m_isDirty) {
        // Record info
        ++(info->nodesSearched);
        ++(info->nodesVisited);
        if (node->m_isTB)
            ++(info->nodesTBHits);

        Q_ASSERT(node->m_isDirty);
        *isExact = nodeIsExact;
        // If this node has children and was proven to be an exact node, then it is possible that
        // recently leafs have been made to we must trim the tree of any leafs
        trimUnscoredFromTree(node);
        node->setQValueAndPropagate();
        return node->m_qValue;
    }

    // If we are an exact node, then we are terminal so just return the score
    if (nodeIsExact) {
        *isExact = nodeIsExact;
        return node->m_qValue;
    }

    // However, if the subtree is not dirty, then we can just return our score
    if (!node->m_isDirty) {
        *isExact = nodeIsExact;
        return node->m_qValue;
    }

    // At this point we should have children
    Q_ASSERT(node->hasChildren());

    // Search the children
    float best = -2.0f;
    bool bestIsExact = false;
    bool everythingScored = false;
    for (int index = 0; index < node->m_children.count(); ++index) {
        Node *child = node->m_children.at(index);
        Q_ASSERT(child);

        // If the child is not visited and is not marked dirty then it has not been scored yet, so
        // just continue
        if (!child->m_visited && !child->m_isDirty) {
            everythingScored = false;
            continue;
        }
        Q_ASSERT(child->hasRawQValue());

        bool subtreeIsExact = false;
        float score = minimax(child, depth + 1, &subtreeIsExact, info);

        // Check if we have a new best child
        if (score > best) {
            bestIsExact = subtreeIsExact;
            best = score;
        }
    }

    // We only propagate exact certainty if the best score from subtree is exact AND either the best
    // score is > 0 (a proven win) OR the potential children of this node have all been played out
    const bool miniMaxComplete = everythingScored && !node->hasPotentials();
    const bool shouldPropagateExact = bestIsExact && (best > 0 || miniMaxComplete) && !node->isRootNode();

    // Score the node based on minimax of children
    node->scoreMiniMax(-best, shouldPropagateExact);

    // Record info
    ++(info->nodesSearched);

    *isExact = node->isExact();
    return node->m_qValue;
}

void Node::validateTree(const Node *node)
{
    // Goes through the entire tree and verifies that everything that should have a score has one
    // and that nothing is marked as dirty that shouldn't be
    Q_ASSERT(node);
    Q_ASSERT(node->hasRawQValue());
    Q_ASSERT(!node->m_isDirty);
    Q_ASSERT(node->hasQValue());
    quint32 childVisits = 0;
    for (int index = 0; index < node->m_children.count(); ++index) {
        Node *child = node->m_children.at(index);
        Q_ASSERT(child);
        Q_ASSERT(child->parent() == node);

        // If the child is not visited and is not marked dirty then it has not been scored yet, so
        // just continue
        if (!child->m_visited && !child->m_isDirty)
            continue;

        validateTree(child);
        childVisits += child->m_visited;
    }

    Q_ASSERT(node->isExact() || node->m_visited == childVisits + 1);
}

void Node::trimUnscoredFromTree(Node *node)
{
    // If this is not dirty, then we don't need to trim
    if (!node->isDirty())
        return;

    QMutableVectorIterator<Node*> it(node->m_children);
    while (it.hasNext()) {
        Node *child = it.next();
        // If this child has not been scored and dirty, then it should be trimmed
        if (!child->m_visited && child->isDirty()) {
            Q_ASSERT(child->m_children.isEmpty());
            --node->m_potentialIndex;
            if (child->m_position && child->m_position->transposition() == child)
                child->m_position->clearTransposition(); // Unpins the position
            it.remove();                    // deletes ourself from our parent
            child->m_position = nullptr;    // unpins the node
            child->m_parent = nullptr;      // make sure to nullify our parent
        } else {
            trimUnscoredFromTree(child);
        }
    }

    node->m_isDirty = false;
}

Node *Node::playout(Node *root, int *vldMax, int *tryPlayoutLimit, bool *hardExit, Cache *cache)
{
start_playout:
    int vld = *vldMax;
    Node *n = root;
    forever {
        // If we've never been scored or this is an exact node, then this is our playout node
        if (!n->setScoringOrScored() || n->isExact()) {
            ++n->m_virtualLoss;
            break;
        }

        // Otherwise, increase virtual loss
        const bool alreadyPlayingOut = n->isAlreadyPlayingOut();
        const qint64 increment = alreadyPlayingOut ? vld : 1;
        if (alreadyPlayingOut) {
            if (increment > 1) {
                Node *parent = n->parent();
                while (parent) {
                    parent->m_virtualLoss += increment - 1;
                    parent = parent->parent();
                }
            }
        } else {
            n->m_virtualLoss += increment;
        }

        // If we've already calculated virtualLossDistance or we are not extendable,
        // then decrement the try and vld limits and check if we should exit
        if (alreadyPlayingOut || n->isExact()) {
            --(*tryPlayoutLimit);
#if defined(DEBUG_PLAYOUT)
            qDebug() << "decreasing try for" << n->toString() << *tryPlayoutLimit;
#endif
            if (*tryPlayoutLimit <= 0)
                return nullptr;

            *vldMax -= increment;
#if defined(DEBUG_PLAYOUT)
            qDebug() << "decreasing vldMax for" << n->toString() << *vldMax;
#endif
            if (*vldMax <= 0)
                return nullptr;

            goto start_playout;
        }

        // Otherwise calculate the virtualLossDistance to advance past this node
        Q_ASSERT(n->hasChildren() || n->hasPotentials());
        Q_ASSERT(!n->isExact());

        Node::Playout firstPlayout;
        Node::Playout secondPlayout;
        float bestScore = -std::numeric_limits<float>::max();
        float secondBestScore = -std::numeric_limits<float>::max();
        float uCoeff = n->uCoeff();
        float parentQValueDefault = n->qValueDefault();

        // First look at the actual children
        for (int i = 0; i < n->m_children.count(); ++i) {
            Node *child = n->m_children.at(i);
            float score = Node::uctFormula(child->qValue(), child->uValue(uCoeff));
            Q_ASSERT(score > -std::numeric_limits<float>::max());
            if (score > bestScore) {
                secondPlayout = firstPlayout;
                secondBestScore = bestScore;
                firstPlayout = Node::Playout(child);
                bestScore = score;
            } else if (score > secondBestScore) {
                secondPlayout = Node::Playout(child);
                secondBestScore = score;
            }
        }

        Q_ASSERT(firstPlayout.isNull() || !(firstPlayout == secondPlayout));

        // Then look at the next two potential children as they have now been sorted by pval
        for (int i = n->m_potentialIndex; i < n->m_position->m_potentials.count() && i < n->m_potentialIndex + 2; ++i) {
            // We get a non-const reference to the actual value
            Node::Potential *potential = &n->m_position->m_potentials[i];
            float score = Node::uctFormula(parentQValueDefault, uCoeff * potential->pValue());
            Q_ASSERT(score > -std::numeric_limits<float>::max());
            if (score > bestScore) {
                secondPlayout = firstPlayout;
                secondBestScore = bestScore;
                firstPlayout = Node::Playout(potential);
                bestScore = score;
            } else if (score > secondBestScore) {
                secondPlayout = Node::Playout(potential);
                secondBestScore = score;
            }
        }

        // Update the top two finishers to avoid them being pruned and calculate vld
        Q_ASSERT(!firstPlayout.isNull());
        if (!secondPlayout.isNull()) {
            const int vldNew
                = virtualLossDistance(
                    secondBestScore,
                    uCoeff,
                    firstPlayout.qValue(parentQValueDefault),
                    firstPlayout.pValue(),
                    int(firstPlayout.visits() + firstPlayout.virtualLoss()));
            if (!vld)
                vld = vldNew;
            else
                vld = qMin(vld, vldNew);
        }

        // Retrieve the actual first node
        NodeGenerationError error = NoError;
        n = firstPlayout.isPotential() ? n->generateNextChild(cache, &error) : firstPlayout.node();

        if (!n) {
            if (error == OutOfMemory)
                *hardExit = true;
            break;
        }
    }

    return n;
}

bool Node::isNoisy() const
{
    const Move mv = m_game.lastMove();
    return mv.isCapture() || mv.isCheck() || mv.promotion() != Chess::Unknown;
}

bool Node::checkAndGenerateDTZ(int *dtz)
{
    Q_ASSERT(isRootNode());
    Move move;
    TB::Probe result = TB::globalInstance()->probeDTZ(m_game, m_position->position(), &move, dtz);
    if (result == TB::NotFound)
        return false;

    // See if the child already exists
    Node *child = nullptr;
    for (int i = 0; i < m_children.count(); ++i) {
        Node *ch = (m_children)[i];
        if (ch->game().lastMove() == move)
            child = ch;
    }

    // If not, then create it
    if (!child) {
        NodeGenerationError error = NoError;
        child = Node::generateNode(move, 0.0f, this, Cache::globalInstance(), &error);
    }

    Q_ASSERT(child);
    if (!child)
        return false;

    // Set from dtz info
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
    if (!m_visited) {
        setRawQValue(0.0f);
        backPropagateDirty();
        setQValueFromRaw();
        ++m_visited;
    }

    child->setQValueAndPropagate();
    return true;
}

void Node::generatePotentials()
{
    Q_ASSERT(m_children.isEmpty());

    // Check if this is drawn by rules
    if (Q_UNLIKELY(m_game.halfMoveClock() >= 100)) {
        setRawQValue(0.0f);
        m_isExact = true;
        return;
    } else if (Q_UNLIKELY(m_position->position().isDeadPosition())) {
        setRawQValue(0.0f);
        m_isExact = true;
        return;
    } else if (Q_UNLIKELY(isThreeFold())) {
        setRawQValue(0.0f);
        m_isExact = true;
        return;
    }

    const TB::Probe result = isRootNode() ?
        TB::NotFound : TB::globalInstance()->probe(m_game, m_position->position());
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

    // Otherwise try and generate potential moves if they have not already been generated for this
    // position
    Q_ASSERT(m_position);
    if (!m_position->hasPotentials())
        m_position->position().pseudoLegalMoves(this);

    // Override the NN in case of checkmates or stalemates
    if (!hasPotentials()) {
        const bool isChecked
            = m_game.isChecked(m_position->position().activeArmy(),
                &m_position->m_position);

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
}

void Node::reservePotentials(int totalSize)
{
    Q_ASSERT(m_position);
    m_children.reserve(totalSize);
    m_position->m_potentials.reserve(totalSize);
}

Node::Potential *Node::generatePotential(const Move &move)
{
    Q_ASSERT(move.isValid());
    Q_ASSERT(m_position);
    Game::Position p = m_position->m_position; // copy
    Game g = m_game;
    if (!g.makeMove(move, &p))
        return nullptr; // illegal

    if (g.isChecked(m_position->position().activeArmy(), &p))
        return nullptr; // illegal

    m_position->m_potentials.append(Potential(move));
    return &(m_position->m_potentials.last());
}

Node *Node::generateNextChild(Cache *cache, NodeGenerationError *error)
{
    Q_ASSERT(hasPotentials());
    Node::Potential potential = m_position->m_potentials.at(m_potentialIndex);
    Node *child = Node::generateNode(potential.move(), potential.pValue(), this, cache, error);
    if (!child)
        return nullptr;
    ++m_potentialIndex;
    return child;
}

Node *Node::generateNode(const Move &childMove, float childPValue, Node *parent, Cache *cache, NodeGenerationError *error)
{
    // Get a new node from hash
    Node *child = cache->newNode();
    if (!child) {
        Q_ASSERT(error);
        *error = OutOfMemory;
        return nullptr;
    }

    // Make the child move
    Game::Position childPosition = parent->m_position->position(); // copy
    Game childGame = parent->m_game;
    const bool success = childGame.makeMove(childMove, &childPosition);
    Q_ASSERT(success);

    // Get a node position from hashpositions
    quint64 childPositionHash = childPosition.positionHash();
    if (!SearchSettings::useTranspositions)
        childPositionHash ^= reinterpret_cast<quint64>(child);

    bool cloned = false;
    Node::Position *childNodePosition = Node::Position::relinkOrClone(childPositionHash, cache, &cloned);
    if (!childNodePosition || cloned) {
        childNodePosition = cache->newNodePosition(childPositionHash);
        if (!childNodePosition) {
            *error = OutOfPositions;
            qFatal("Fatal error: we have run out of positions in memory!");
        }
    }

    Q_ASSERT(childNodePosition);
    child->initialize(parent, childGame, childNodePosition);
    childNodePosition->initialize(child, childPosition);
    child->setPValue(childPValue);
    parent->m_children.append(child);
    return child;
}

const Node *Node::findSuccessor(const QVector<QString> &child) const
{
    const Node *n = this;
    for (QString c : child) {

        bool found = false;
        for (const Node *node : n->m_children) {
            if (node->m_game.toString(Chess::Computer) == c) {
                n = node;
                found = true;
                break;
            }
        }

        if (!found) {
            n = nullptr;
            break;
        }
    }

    return n;
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

QString Node::printTree(int topDepth, int depth, bool printPotentials) const
{
    QString tree;
    QTextStream stream(&tree);
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream << "\n";
    const int d = this->depth() - topDepth;
    for (int i = 0; i < d; ++i)
        stream << qSetFieldWidth(7) << "      |";

    Move mv = m_game.lastMove();

    float uCoeff = isRootNode() ? 0.0f : parent()->uCoeff();

    QString move = QString("%1").arg(mv.isValid() ? Notation::moveToString(mv, Chess::Computer) : "start");
    QString i = QString("%1").arg(mv.isValid() ? QString::number(moveToNNIndex(mv)) : "----");
    stream
        << right << qSetFieldWidth(6) << move
        << qSetFieldWidth(2) << " ("
        << qSetFieldWidth(4) << i
        << qSetFieldWidth(1) << ")"
        << qSetFieldWidth(4) << left << " n: " << qSetFieldWidth(4) << right << m_visited + m_virtualLoss
        << qSetFieldWidth(4) << left << " p: " << qSetFieldWidth(5) << qSetRealNumberPrecision(2) << right << pValue() * 100 << qSetFieldWidth(1) << left << "%"
        << qSetFieldWidth(4) << left << " q: " << qSetFieldWidth(8) << qSetRealNumberPrecision(5) << right << qValue()
        << qSetFieldWidth(4) << " u: " << qSetFieldWidth(6) << qSetRealNumberPrecision(5) << right << uValue(uCoeff)
        << qSetFieldWidth(4) << " q+u: " << qSetFieldWidth(8) << qSetRealNumberPrecision(5) << right << (isRootNode() ? 0.0f : Node::uctFormula(qValue(), uValue(uCoeff)))
        << qSetFieldWidth(4) << " v: " << qSetFieldWidth(7) << qSetRealNumberPrecision(4) << right << rawQValue()
        << qSetFieldWidth(4) << " h: " << qSetFieldWidth(2) << right << qMax(1, treeDepth() - d)
        << qSetFieldWidth(4) << " cp: " << qSetFieldWidth(2) << right << scoreToCP(qValue());

    if (d < depth) {
        QVector<Node*> children = *this->children();
        if (!children.isEmpty()) {
            Node::sortByScore(children, false /*partialSortFirstOnly*/);
            for (const Node *child : children)
                stream << child->printTree(topDepth, depth, printPotentials);
        }
        if (printPotentials) {
            for (int i = m_potentialIndex; i < m_position->m_potentials.count(); ++i) {
                Potential p = m_position->m_potentials.at(i);
                stream << "\n";
                const int d = this->depth() + 1;
                for (int i = 0; i < d; ++i) {
                    stream << qSetFieldWidth(7) << "      |";
                }
                stream << right << qSetFieldWidth(6) << p.toString()
                    << qSetFieldWidth(2) << " ("
                    << qSetFieldWidth(4) << moveToNNIndex(p.move())
                    << qSetFieldWidth(1) << ")"
                    << qSetFieldWidth(4) << left << " p: " << p.pValue() * 100 << qSetFieldWidth(1) << left << "%";
            }
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
