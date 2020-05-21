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
    // Updated formula caps the centipawn at 25600 by using trig equation up to +1000 and then
    // just using a linear function after that
    if (qAbs(score) > 0.8392234846)
        return qRound(153007 * score + (score > 0 ? -127407 : 127407));
    else
        return qRound(111.f * qTan(1.74f * score));
}

float cpToScore(int cp)
{
    // Inverse of the above
    if (qAbs(cp) > 1000)
        return (cp + (cp > 0 ? 127407 : -127407)) / 153007.f;
    else
        return qAtan(cp / 111.f) / 1.74f;
}

Node::Position::Position()
{
    m_canonicalNode = nullptr;
    m_rawQValue = -2.0f;
    m_isUnique = false;
}

Node::Position::~Position()
{
}

void Node::Position::initialize(Node *node, const Game::Position &position)
{
    if (m_canonicalNode)
        return;

    m_position = position;
    m_canonicalNode = node;
    Q_ASSERT(m_canonicalNode->m_position == this);
    if (m_canonicalNode) {
#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "ctor p ";
        stream << positionHash();
        stream << " [";
        stream << m_canonicalNode;
        stream << "]";
        qDebug().noquote() << string;
#endif
    }
}

void Node::Position::deinitialize(bool forcedFree)
{
    Q_UNUSED(forcedFree)
    m_position = Game::Position();
    m_canonicalNode = nullptr;
    m_potentials.clear();
    m_rawQValue = -2.0f;
    m_isUnique = false;
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "dtor p ";
    stream << positionHash();
    qDebug().noquote() << string;
#endif
}

Node::Position *Node::Position::relinkOrMakeUnique(quint64 positionHash, Cache *cache, bool *madeUnique)
{
    if (!cache->containsNodePosition(positionHash))
        return nullptr;

    // Update the reference for this position in LRU hash
    Node::Position *p = cache->nodePositionRelinkOrMakeUnique(positionHash, madeUnique);
#if defined(DEBUG_CHURN)
    if (*madeUnique) {
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
    initialize(nullptr, Game());
}

Node::~Node()
{
}

void Node::initialize(Node *parent, const Game &game)
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
    m_position = nullptr;
    m_potentialIndex = 0;
    m_children.clear();
    m_visited = 0;
    m_virtualLoss = 0;
    m_qValue = -2.0f;
    m_pValue = -2.0f;
    m_policySum = 0;
    m_uCoeff = -2.0f;
    m_nodeType = NonTerminal;
    m_isDirty = false;
}

quint64 Node::initializePosition(Cache *cache)
{
    // Nothing to do if we already have a position which is true for root
    if (m_position)
        return 0;

    Game::Position childPosition = m_parent->m_position->position(); // copy
    const bool success = m_game.makeMove(m_game.lastMove(), &childPosition);
    Q_ASSERT(success);

    // Get a node position from hashpositions
    quint64 childPositionHash = childPosition.positionHash();

    // FIXME: This leaks memory because cache won't know how to erase the position as it does
    // not have access to the node's address when deinitializing the position!
    if (SearchSettings::featuresOff.testFlag(SearchSettings::Transpositions))
        childPositionHash ^= reinterpret_cast<quint64>(this);

    bool madeUnique = false;
    m_position = Node::Position::relinkOrMakeUnique(childPositionHash, cache, &madeUnique);
    if (!m_position || madeUnique) {
        m_position = cache->newNodePosition(childPositionHash);
        if (!m_position)
            qFatal("Fatal error: we have run out of positions in memory!");
    }

    Q_ASSERT(m_position);
    m_position->initialize(this, childPosition);

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
    return childPositionHash;
}

void Node::setPosition(Node::Position *position)
{
    m_position = position;

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

void Node::deinitialize(bool forcedFree)
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

    if (m_position && m_position->canonicalNode() == this)
        m_position->clearCanonicalNode();

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
}

void Node::updateTranspositions() const
{
    if (!m_position->canonicalNode())
        m_position->updateCanonicalNode(const_cast<const Node*>(this));

    for (int i = 0; i < m_children.count(); ++i)
        m_children.at(i)->updateTranspositions();
}

void Node::unwindTransposition(quint64 hash, Cache *cache)
{
    Q_ASSERT(isTransposition());
    Game::Position gamePosition = m_position->position(); // copy
    m_position = cache->newNodePosition(hash, true /*makeUnique*/);
    if (!m_position)
        qFatal("Fatal error: we have run out of positions in memory!");
    Q_ASSERT(m_position);
    m_position->initialize(this, gamePosition);
    Q_ASSERT(!isTransposition());
    Q_ASSERT(m_position->isUnique());
}

Node *Node::bestChild() const
{
    if (!hasChildren())
        return nullptr;
    QVector<Node*> children = m_children;
    sortByScore(children, true /*partialSortFirstOnly*/);
    return children.first();
}

void Node::scoreMiniMax(float score, bool isExact, double newScores, quint32 newVisits)
{
    Q_ASSERT(!qFuzzyCompare(qAbs(score), 2.f));
    Q_ASSERT(!this->isExact() || isExact);
    if (isExact) {
        m_qValue = score;
        const NodeType exactType = score > 0 ? PropagateWin : score < 0 ? PropagateLoss : PropagateDraw;
        // Iff it is a proven win or loss, then we can go ahead and update the rawQValue which will
        // be passed along to transpositions, but not for draws as they could have been threefold or
        // 50 move rule which does not pertain to a transposition with different move history
        if (exactType != PropagateDraw)
            setRawQValue(score);
        setExact(exactType);
    } else {
        if (Q_LIKELY(!SearchSettings::featuresOff.testFlag(SearchSettings::Minimax)))
            m_qValue = qBound(-1.f, float(m_visited * m_qValue + score + newScores) / float(m_visited + newVisits + 1), 1.f);
        else
            m_qValue = qBound(-1.f, float(m_visited * m_qValue + newScores) / float(m_visited + newVisits), 1.f);
    }
    incrementVisited(newVisits);
}

void Node::incrementVisited(quint32 increment)
{
    m_visited += increment;
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

void Node::setQValueAndVisit()
{
    Q_ASSERT(hasRawQValue());
    Node *parent = this->parent();
    if (parent && !m_visited)
        parent->m_policySum += pValue();
    setQValueFromRaw();
    incrementVisited(1);
#if defined(DEBUG_FETCHANDBP)
    qDebug() << "bp " << toString() << " n:" << m_visited
        << "v:" << rawQValue() << "oq:" << 0.0 << "fq:" << m_qValue;
#endif
}

void Node::backPropagateDirty()
{
    Q_ASSERT(!m_isDirty);
    Q_ASSERT(hasRawQValue());
    Q_ASSERT(!m_visited || isExact());
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

void Node::principalVariation(int *depth, bool *isCheckMate, QTextStream *stream) const
{
    if (!isRootNode() && !hasPValue()) {
        *isCheckMate = this->isCheckMate();
        return;
    }

    *depth += 1;

    const Node *bestChild = this->bestChild();
    if (!bestChild) {
        *isCheckMate = this->isCheckMate();
        *stream << Notation::moveToString(m_game.lastMove(), Chess::Computer);
        return;
    }

    if (!isRootNode()) {
        *stream << Notation::moveToString(m_game.lastMove(), Chess::Computer) << QStringLiteral(" ");
    }

    bestChild->principalVariation(depth, isCheckMate, stream);
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

bool Node::isMoveClock() const
{
    // FIXME: This isn't a moveclock draw if it delivers checkmate!
    return m_game.halfMoveClock() >= 100;
}

Node::MinimaxResult Node::minimax(Node *node, quint32 depth, WorkerInfo *info)
{
    Q_ASSERT(node);
    Q_ASSERT(node->hasRawQValue());

    // First we look to see if this node has been scored
    if (!node->m_visited) {
        // Record info
        Q_ASSERT(node->m_isDirty);
        ++(info->nodesSearched);
        ++(info->nodesVisited);
        info->sumDepths += depth;
        info->maxDepth = qMax(info->maxDepth, depth);
        if (node->isTB())
            ++(info->nodesTBHits);
        node->setQValueAndVisit();
        return MinimaxResult { node->m_qValue, node->isExact(), node->rawQValue(), 1 };
    }

    // Next look if it is a dirty terminal
    if (node->isExact() && node->m_isDirty) {
        // Record info
        ++(info->nodesSearched);
        ++(info->nodesVisited);
        if (node->isTB())
            ++(info->nodesTBHits);
        // If this node has children and was proven to be an exact node, then it is possible that
        // recently leafs have been made to we must trim the tree of any leafs
        trimUnscoredFromTree(node);
        node->setQValueAndVisit();
        return MinimaxResult { node->m_qValue, node->isExact(), node->rawQValue(), 1 };
    }

    // If we are an exact node, then we are terminal so just return the score
    if (node->isExact()) {
        return MinimaxResult { node->m_qValue, node->isExact(), 0, 0 };
    }

    // However, if the subtree is not dirty, then we can just return our score
    if (!node->m_isDirty) {
        return MinimaxResult { node->m_qValue, node->isExact(), 0, 0 };
    }

    // At this point we should have children
    Q_ASSERT(node->hasChildren());

    // Search the children
    float best = -2.0f;
    bool allAreExact = true;
    bool bestIsExact = false;
    bool allChildrenAreScored = true;
    double newScoresForChildren = 0;
    quint32 newVisitsForChildren = 0;
    for (int index = 0; index < node->m_children.count(); ++index) {
        Node *child = node->m_children.at(index);
        Q_ASSERT(child);

        // If the child is not visited and is not marked dirty then it has not been scored yet, so
        // just continue
        if (!child->m_visited && !child->m_isDirty) {
            allChildrenAreScored = false;
            continue;
        }

        Q_ASSERT(child->hasRawQValue());

        MinimaxResult result = minimax(child, depth + 1, info);
        newScoresForChildren += result.newScores;
        newVisitsForChildren += result.newVisits;
        allAreExact = result.isExact ? allAreExact : false;

        // Check if we have a new best child
        if (result.score > best) {
            bestIsExact = result.isExact;
            best = result.score;
        }
    }

    // We only propagate exact certainty if the best score from subtree is exact AND the best score
    // is a proven win OR if the subtree is complete and all nodes are exact in which case the score
    // is totally certain
    const bool shouldPropagateExact =
        ((bestIsExact && best > 0) || // proven win
         (allAreExact && allChildrenAreScored && !node->hasPotentials())) // score totally certain
        && !node->isRootNode();

    // Record info
    ++(info->nodesSearched);

    // Score the node based on minimax of children
    node->scoreMiniMax(-best, shouldPropagateExact, -newScoresForChildren, newVisitsForChildren);
    return MinimaxResult { node->m_qValue, node->isExact(), -newScoresForChildren, newVisitsForChildren };
}

void Node::validateTree(const Node *node)
{
    // Goes through the entire tree and verifies that everything that should have a score has one
    // and that nothing is marked as dirty that shouldn't be
    Q_ASSERT(node);
    Q_ASSERT(node->hasRawQValue());
    Q_ASSERT(!node->m_isDirty);
    Q_ASSERT(node->visits());
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

    Q_ASSERT(node->isRootNode() || node->isExact() || node->m_visited == childVisits + 1);
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
            if (child->m_position && child->m_position->canonicalNode() == child)
                child->m_position->clearCanonicalNode(); // Unpins the position
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
        if (firstPlayout.isPotential()) {
            // Expand the potential node, then this is our playout node
            n = n->generateNextChild(cache, &error);
            if (n) {
                ++n->m_virtualLoss;
            } else {
                Q_ASSERT(error == OutOfMemory);
                *hardExit = true;
            }
            break;
        } else {
            n = firstPlayout.node();
        }

        // If this is an exact node with no virtualloss, then this is our playout node
        if (n->isExact() && !n->virtualLoss()) {
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
        child->initializePosition(Cache::globalInstance());
    }

    Q_ASSERT(child);
    if (!child)
        return false;

    // Set from dtz info
    // This is inverted because the probe reports from parent's perspective
    switch (result) {
    case TB::Win:
        child->setRawQValue(1.0f);
        child->setExact(TBWin);
        break;
    case TB::Loss:
        child->setRawQValue(-1.0f);
        child->setExact(TBLoss);
        break;
    case TB::Draw:
        child->setRawQValue(0.0f);
        child->setExact(TBDraw);
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

    child->setQValueAndVisit();
    return true;
}

bool Node::checkMoveClockOrThreefold(quint64 hash, Cache *cache)
{
    Q_ASSERT(m_children.isEmpty());
    // Check if this is drawn by rules
    if (Q_UNLIKELY(isMoveClock())) {
        // This can never be a transposition as it depends upon information not found in the
        // generic position, but rather depends upon game specific context
        if (isTransposition())
            unwindTransposition(hash, cache);
        else
            cache->nodePositionMakeUnique(hash);
        Q_ASSERT(m_position->isUnique());
        setRawQValue(0.0f);
        setExact(FiftyMoveRuleDraw);
        return true;
    } else if (Q_UNLIKELY(isThreeFold())) {
        // This can never be a transposition as it depends upon information not found in the
        // generic position, but rather depends upon game specific context
        if (isTransposition())
            unwindTransposition(hash, cache);
        else
            cache->nodePositionMakeUnique(hash);
        Q_ASSERT(m_position->isUnique());
        setRawQValue(0.0f);
        setExact(ThreeFoldDraw);
        return true;
    }
    return false;
}

void Node::generatePotentials()
{
    Q_ASSERT(m_children.isEmpty());

    // Check if this is drawn by rules
    if (Q_UNLIKELY(m_position->position().isDeadPosition())) {
        setRawQValue(0.0f);
        setExact(Draw);
        return;
    }

    const TB::Probe result = isRootNode() ?
        TB::NotFound : TB::globalInstance()->probe(m_game, m_position->position());
    switch (result) {
    case TB::NotFound:
        break;
    case TB::Win:
        setRawQValue(1.0f);
        setExact(TBWin);
        return;
    case TB::Loss:
        setRawQValue(-1.0f);
        setExact(TBLoss);
        return;
    case TB::Draw:
        setRawQValue(0.0f);
        setExact(TBDraw);
        return;
    }

    Q_ASSERT(m_position);
    Q_ASSERT(m_position->potentials()->isEmpty());
    Q_ASSERT(m_position->canonicalNode() == this);

    m_position->position().pseudoLegalMoves(this);

    // Override the NN in case of checkmates or stalemates
    if (!hasPotentials()) {
        const bool isChecked
            = m_game.isChecked(m_position->position().activeArmy(),
                &m_position->m_position);

        if (isChecked) {
            m_game.setCheckMate(true);
            setRawQValue(1.0f);
            setExact(Win);
        } else {
            m_game.setStaleMate(true);
            setRawQValue(0.0f);
            setExact(Draw);
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

    // Store the child move
    Game childGame = parent->m_game;
    childGame.storeMove(childMove);
    child->initialize(parent, childGame);
    child->setPValue(childPValue);
    child->setQValue(parent->qValueDefault());
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

QString Node::toFen() const
{
    return m_game.stateOfGameToFen(&m_position->position());
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
