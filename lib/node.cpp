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
    : m_positionHash(0)
{
}

Node::Position::~Position()
{
}

void Node::Position::initialize(Node *node, const Game::Position &position, quint64 positionHash)
{
    m_position = position;
    m_positionHash = positionHash;
    m_nodes.clear();
    if (node) {
        m_nodes.append(node);
#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "ctor p ";
        stream << positionHash;
        stream << " [";
        stream << node->hash();
        stream << "]";
        qDebug().noquote() << string;
#endif
    }
}

bool Node::Position::deinitialize(bool forcedFree)
{
    Q_UNUSED(forcedFree)
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "dtor p ";
    stream << positionHash();
    stream << " [";
    int i = 0;
    for (Node *node : m_nodes) {
        if (i)
            stream << " ";
        stream << node->hash();
        ++i;
    }
    stream << "]";
    qDebug().noquote() << string;
#endif
    Q_ASSERT(nodesNotInHash());
    return nodesNotInHash();
}

void Node::Position::addNode(Node *node, Cache *cache)
{
    Q_ASSERT(!hasNode(node));
    m_nodes.append(node);
    cache->pinNodePosition(positionHash());
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "addn p ";
    stream << positionHash();
    stream << " [";
    int i = 0;
    for (Node *node : m_nodes) {
        if (i)
            stream << " ";
        stream << node->hash();
        ++i;
    }
    stream << "]";
    qDebug().noquote() << string;
#endif
}

void Node::Position::removeNode(Node *node, Cache *cache)
{
    Q_ASSERT(hasNode(node));
    m_nodes.removeAll(node);
    if (m_nodes.isEmpty())
        cache->unpinNodePosition(m_positionHash);
}

Node::Position *Node::Position::relink(quint64 positionHash, Cache *cache)
{
    // Update the reference for this position in LRU hash
    Q_ASSERT(cache->containsNodePosition(positionHash));
    Node::Position *p = cache->nodePosition(positionHash, true /*relink*/);
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "relk p ";
    stream << positionHash;
    stream << " [";
    int i = 0;
    for (Node *node : p->m_nodes) {
        if (i)
            stream << " ";
        stream << node->hash();
        ++i;
    }
    stream << "]";
    qDebug().noquote() << string;
#endif
    return p;
}

bool Node::Position::nodesNotInHash() const
{
    for (Node *node : m_nodes)
        if (Cache::globalInstance()->containsNode(node->hash()))
            return false;
    return true;
}

Node::Node()
{
    initialize(0, nullptr, Game(), nullptr);
}

Node::~Node()
{
}

quint64 Node::nextHash()
{
    static quint64 s_nextHash = 0;
    return ++s_nextHash;
}

void Node::initialize(quint64 hash, Node *parent, const Game &game, Node::Position *position)
{
    if (parent) {
        Q_ASSERT(Cache::globalInstance()->containsNode(parent->hash()));
        ++parent->m_refs;
#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "ref " << parent->m_refs << " [" << parent->hash() << "]";
        qDebug().noquote() << string;
#endif
    }
    m_game = game;
    m_parent = parent;
    m_position = position;
    m_hash = hash;
    m_children.clear();
    m_refs = 0;
    m_visited = 0;
    m_virtualLoss = 0;
    m_qValue = -2.0f;
    m_rawQValue = -2.0f;
    m_pValue = -2.0f;
    m_policySum = 0;
    m_uCoeff = -2.0f;
    m_isExact = false;
    m_isTB = false;
    m_isDirty = false;
    m_scoringOrScored.clear();
#if defined(DEBUG_CHURN)
    if (m_hash) {
        QString string;
        QTextStream stream(&string);
        stream << "ctor n ";
        stream << m_position->positionHash();
        stream << " [";
        stream << m_hash;
        stream << "]";
        qDebug().noquote() << string;
    }
#endif
}

bool Node::deinitialize(bool forcedFree)
{
    Cache *cache = Cache::globalInstance();
    if (Node *parent = this->parent()) {
        // Decrement parent's refs
        --parent->m_refs;

        // Remove ourself from parent's child list
        parent->nullifyChildRef(this);

#if defined(DEBUG_CHURN)
        QString string;
        QTextStream stream(&string);
        stream << "deref " << parent->m_refs << " [" << parent->hash() << "]";
        qDebug().noquote() << string;

#endif
        // Delete parent if it no longer has any refs and this node is being freed to make room
        // in fixed size hash
        if (!parent->m_refs && forcedFree)
            cache->unlinkNode(parent->hash());
    }

    // Unlink all children as we do not want to leave them parentless
    for (int i = 0; i < m_children.count(); ++i) {
        const Node::Child childRef = m_children.at(i);
        if (!childRef.isPotential() && childRef.node())
            cache->unlinkNode(childRef.node()->hash());
    }

    if (m_position)
        m_position->removeNode(this, cache);

#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "dtor n ";
    stream << (m_position ? m_position->positionHash() : 0xBAAAAAAD);
    stream << " [";
    stream << m_hash;
    stream << "]";
    qDebug().noquote() << string;
#endif

    m_parent = nullptr;
    m_position = nullptr;
    m_refs = 0;
    m_isDirty = false;

    return true;
}

Node *Node::relink(quint64 hash, Cache *cache)
{
    // Update the reference for this node in LRU hash
    Q_ASSERT(cache->containsNode(hash));
    Node *n = cache->node(hash, true /*relink*/);
#if defined(DEBUG_CHURN)
    QString string;
    QTextStream stream(&string);
    stream << "relk n ";
    stream << n->m_position->positionHash();
    stream << " [";
    stream << n->m_hash;
    stream << "]";
    qDebug().noquote() << string;
#else
    qt_noop();
#endif
    return n;
}

Node *Node::bestEmbodiedChild() const
{
    if (m_children.isEmpty())
        return nullptr;

    Node *bestChild = nullptr;
    for (int i = 0; i < m_children.count(); ++i) {
        const Node::Child childRef = m_children.at(i);
        if (childRef.isPotential() || !childRef.node())
            continue;
        if (!bestChild || Node::greaterThan(childRef.node(), bestChild))
            bestChild = childRef.node();
    }
    return bestChild;
}

QVector<Node*> Node::embodiedChildren() const
{
    if (m_children.isEmpty())
        return QVector<Node*>();

    QVector<Node*> result;
    for (int i = 0; i < m_children.count(); ++i) {
        const Node::Child childRef = m_children.at(i);
        if (!childRef.isPotential() && childRef.node())
            result.append(childRef.node());
    }
    return result;
}

void Node::nullifyChildRef(Node *child)
{
    for (int i = 0; i < m_children.count(); ++i) {
        Node::Child *childRef = &(m_children)[i];
        if (!childRef->isPotential() && childRef->node() == child) {
            const float pValue = child->pValue();
            childRef->setPotential(true);
            childRef->setPValue(pValue);
            break;
        }
    }
}

bool Node::allChildrenPruned() const
{
    if (m_isExact)
        return false;

    if (!m_visited)
        return false;

    for (Node::Child ref : m_children) {
        if (ref.isPotential())
            return false;

        if (ref.node())
            return false;
    }

    return true;
}

void Node::scoreMiniMax(float score, bool isExact)
{
    Q_ASSERT(!qFuzzyCompare(qAbs(score), 2.f));
    Q_ASSERT(!m_isExact || isExact);
    m_isExact = isExact;
    if (m_isExact) {
        m_rawQValue = score;
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
    qDebug() << "bp " << toString() << " n:" << m_position->m_visited
        << "v:" << v << "oq:" << currentQValue << "fq:" << m_position->m_qValue;
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
    qDebug() << "bp " << toString() << " n:" << m_position->m_visited
        << "v:" << m_position->m_rawQValue << "oq:" << 0.0 << "fq:" << m_position->m_qValue;
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

void Node::pinPrincipalVariation(QVector<quint64> *pinnedList, Cache* cache) const
{
    if (!isRootNode() && !hasPValue())
        return;

    const quint64 h = hash();
    pinnedList->append(h);
    Q_ASSERT(cache->containsNode(h));
    cache->pinNode(h);

    const Node *bestChild = bestEmbodiedChild();
    if (!bestChild)
        return;

    return bestChild->pinPrincipalVariation(pinnedList, cache);
}

QString Node::principalVariation(int *depth, bool *isTB) const
{
    if (!isRootNode() && !hasPValue()) {
        *isTB = m_isTB;
        return QString();
    }

    *depth += 1;

    const Node *bestChild = bestEmbodiedChild();
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
    int childrenScored = 0;
    for (const Node::Child &ch : node->m_children) {
        if (ch.isPotential() || !ch.node())
            continue;

        Node *child = ch.node();

        // If the child doesn't have a raw qValue then it has not been scored yet so just continue
        // or if it does have one, but not visited and is not marked dirty yet
        if (!child->hasRawQValue() || (!child->m_visited && !child->m_isDirty))
            continue;

        ++childrenScored;
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
    const bool everythingScored = childrenScored == node->m_children.count();
    const bool miniMaxComplete = everythingScored;
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
    for (const Node::Child &ch : node->m_children) {
        if (ch.isPotential() || !ch.node())
            continue;

        Node *child = ch.node();

        // If the child doesn't have a raw qValue then it has not been scored yet so just continue
        if (!child->hasRawQValue())
            continue;

        validateTree(child);
        childVisits += child->m_visited;
    }

    Q_ASSERT(node->m_visited == childVisits + 1);
}

quint64 Node::playout(Node *root, int *vldMax, int *tryPlayoutLimit, bool *hardExit, Cache *cache, QMutex *mutex)
{
start_playout:
    int vld = *vldMax;
    quint64 nhash = root->hash();
    forever {
        QMutexLocker locker(mutex);
        if (!cache->containsNode(nhash))
            goto start_playout;

        Node *n = cache->node(nhash);
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
        if (alreadyPlayingOut || n->isNotExtendable()) {
            --(*tryPlayoutLimit);
#if defined(DEBUG_PLAYOUT)
            qDebug() << "decreasing try for" << n->toString() << *tryPlayoutLimit;
#endif
            if (*tryPlayoutLimit <= 0)
                return 0;

            *vldMax -= increment;
#if defined(DEBUG_PLAYOUT)
            qDebug() << "decreasing vldMax for" << n->toString() << *vldMax;
#endif
            if (*vldMax <= 0)
                return 0;

            goto start_playout;
        }

        // Otherwise calculate the virtualLossDistance to advance past this node
        Q_ASSERT(n->hasChildren());

        Node::Child *firstNode = nullptr;
        Node::Child *secondNode = nullptr;
        float bestScore = -std::numeric_limits<float>::max();
        float secondBestScore = -std::numeric_limits<float>::max();
        float uCoeff = n->uCoeff();
        float parentQValueDefault = n->qValueDefault();
        int countPotentials = 0;

        for (Node::Child &ch : n->m_children) {
            if (countPotentials > 1)
                break;
            float score = Node::uctFormula(ch.qValue(parentQValueDefault), ch.uValue(uCoeff), ch.visits() + ch.virtualLoss());
            Q_ASSERT(score > -std::numeric_limits<float>::max());
            if (score > bestScore) {
                secondNode = firstNode;
                secondBestScore = bestScore;
                firstNode = &ch;
                bestScore = score;
            } else if (score > secondBestScore) {
                secondNode = &ch;
                secondBestScore = score;
            }
            if (ch.isPotential())
                ++countPotentials;
        }

        // Update the top two finishers to avoid them being pruned and calculate vld
        Q_ASSERT(firstNode);
        if (secondNode) {
            const int vldNew
                = virtualLossDistance(
                    secondBestScore,
                    uCoeff,
                    firstNode->qValue(parentQValueDefault),
                    firstNode->pValue(),
                    int(firstNode->visits() + firstNode->virtualLoss()));
            if (!vld)
                vld = vldNew;
            else
                vld = qMin(vld, vldNew);
            Q_ASSERT(vld >= 1);
            secondNode->relink(cache);
        }
        firstNode->relink(cache);

        // Retrieve the actual first node
        NodeGenerationError error = NoError;
        n = firstNode->isPotential() ? n->generateEmbodiedChild(firstNode, cache, &error) : firstNode->node();
        nhash = n->hash();

        if (!n) {
            if (error == OutOfMemory)
                *hardExit = true;
            break;
        }
    }

    return nhash;
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
    Node::Child *child = nullptr;
    for (int i = 0; i < m_children.count(); ++i) {
        Node::Child *childRef = &(m_children)[i];
        if (childRef->move() == move)
            child = childRef;
    }

    // If not, then create it
    if (!child)
        child = generateChild(move);

    Q_ASSERT(child);
    if (!child)
        return false;

    Node *embodiedChild = nullptr;

    // Is it just a potential?
    if (child->isPotential()) {
        NodeGenerationError error = NoError;
        embodiedChild = generateEmbodiedChild(child, Cache::globalInstance(), &error);
    } else {
        embodiedChild = child->node();
    }

    if (!embodiedChild)
        return false;

    // Set from dtz info
    // This is inverted because the probe reports from parent's perspective
    switch (result) {
    case TB::Win:
        embodiedChild->m_rawQValue = 1.0f;
        embodiedChild->m_isExact = true;
        embodiedChild->m_isTB = true;
        break;
    case TB::Loss:
        embodiedChild->m_rawQValue = -1.0f;
        embodiedChild->m_isExact = true;
        embodiedChild->m_isTB = true;
        break;
    case TB::Draw:
        embodiedChild->m_rawQValue = 0.0f;
        embodiedChild->m_isExact = true;
        embodiedChild->m_isTB = true;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    // If this root has never been scored, then do so now to prevent asserts in back propagation
    if (!m_visited) {
        m_rawQValue = 0.0f;
        backPropagateDirty();
        setQValueFromRaw();
        ++m_visited;
    }

    embodiedChild->setQValueAndPropagate();
    return true;
}

void Node::generateChildren()
{
    Q_ASSERT(!hasChildren());
    if (hasChildren())
        return;

    // Check if this is drawn by rules
    if (Q_UNLIKELY(m_game.halfMoveClock() >= 100)) {
        m_rawQValue = 0.0f;
        m_isExact = true;
        return;
    } else if (Q_UNLIKELY(m_position->position().isDeadPosition())) {
        m_rawQValue = 0.0f;
        m_isExact = true;
        return;
    } else if (Q_UNLIKELY(isThreeFold())) {
        m_rawQValue = 0.0f;
        m_isExact = true;
        return;
    }

    const TB::Probe result = isRootNode() ?
        TB::NotFound : TB::globalInstance()->probe(m_game, m_position->position());
    switch (result) {
    case TB::NotFound:
        break;
    case TB::Win:
        m_rawQValue = 1.0f;
        m_isExact = true;
        m_isTB = true;
        return;
    case TB::Loss:
        m_rawQValue = -1.0f;
        m_isExact = true;
        m_isTB = true;
        return;
    case TB::Draw:
        m_rawQValue = 0.0f;
        m_isExact = true;
        m_isTB = true;
        return;
    }

    // Otherwise try and generate potential moves
    m_position->position().pseudoLegalMoves(this);

    // Override the NN in case of checkmates or stalemates
    if (!hasChildren()) {
        const bool isChecked
            = m_game.isChecked(m_position->position().activeArmy(),
                &m_position->m_position);

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
    return;
}

void Node::reserveChildren(int totalSize)
{
    m_children.reserve(totalSize);
}

Node::Child *Node::generateChild(const Move &move)
{
    Q_ASSERT(move.isValid());
    Game::Position p = m_position->m_position; // copy
    Game g = m_game;
    if (!g.makeMove(move, &p))
        return nullptr; // illegal

    if (g.isChecked(m_position->position().activeArmy(), &p))
        return nullptr; // illegal

    m_children.append(Child(move));
    return &(m_children.last());
}

Node *Node::generateEmbodiedChild(Child *child, Cache *cache, NodeGenerationError *error)
{
    Q_ASSERT(child);
    Node *embodiedChild = Node::generateEmbodiedNode(child->move(), child->pValue(), this, cache, error);
    if (!embodiedChild)
        return nullptr;

    child->setPotential(false);
    child->setNode(embodiedChild);
    return embodiedChild;
}

Node *Node::generateEmbodiedNode(const Move &childMove, float childPValue, Node *parent, Cache *cache, NodeGenerationError *error)
{
    const quint64 childHash = Node::nextHash();

    // Get a new node from hash
    Node *embodiedChild = cache->newNode(childHash);
    if (!embodiedChild) {
        Q_ASSERT(error);
        *error = OutOfMemory;
        return nullptr;
    }

    // It is possible the hash returned an ancestor of parent as a new child in which case it is
    // possible that parent has been pruned to make way for this child!
    if (!cache->containsNode(parent->hash())) {
        // We need to backout the newly created child if it already existed in the hash and was
        // reused
        Q_ASSERT(cache->containsNode(childHash));
        embodiedChild->m_hash = childHash; // need to initialize it
        cache->unlinkNode(childHash);
        *error = ParentPruned;
        return nullptr;
    }

    // Make the child move
    Game::Position childPosition = parent->m_position->position(); // copy
    Game childGame = parent->m_game;
    const bool success = childGame.makeMove(childMove, &childPosition);
    Q_ASSERT(success);

    // Get a node position from hashpositions
    Node::Position *childNodePosition = nullptr;
    const quint64 childPositionHash = childPosition.positionHash();
    if (cache->containsNodePosition(childPositionHash)) {
        childNodePosition = Node::Position::relink(childPositionHash, cache);
        Q_ASSERT(childNodePosition);
        embodiedChild->initialize(childHash, parent, childGame, childNodePosition);
        childNodePosition->addNode(embodiedChild, cache);
    } else {
        childNodePosition = cache->newNodePosition(childPositionHash);
        if (!childNodePosition) {
            *error = OutOfPositions;
            qFatal("Fatal error: we have run out of positions in memory!");
        }
        Q_ASSERT(childNodePosition);
        embodiedChild->initialize(childHash, parent, childGame, childNodePosition);
        childNodePosition->initialize(embodiedChild, childPosition, childPositionHash);
    }

    embodiedChild->setPValue(childPValue);
    return embodiedChild;
}

const Node *Node::findEmbodiedSuccessor(const QVector<QString> &child) const
{
    const Node *n = this;
    for (QString c : child) {

        bool found = false;
        for (const Node *node : n->embodiedChildren()) {
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
        << qSetFieldWidth(4) << " q+u: " << qSetFieldWidth(8) << qSetRealNumberPrecision(5) << right << (isRootNode() ? 0.0f : Node::uctFormula(qValue(), uValue(uCoeff), visits()))
        << qSetFieldWidth(4) << " v: " << qSetFieldWidth(7) << qSetRealNumberPrecision(4) << right << rawQValue()
        << qSetFieldWidth(4) << " h: " << qSetFieldWidth(2) << right << qMax(1, treeDepth() - d)
        << qSetFieldWidth(4) << " cp: " << qSetFieldWidth(2) << right << scoreToCP(qValue());

    if (d < depth) {
        QVector<Node*> embodiedChildren = this->embodiedChildren();
        if (!embodiedChildren.isEmpty()) {
            Node::sortByScore(embodiedChildren, false /*partialSortFirstOnly*/);
            for (const Node *embodiedChild : embodiedChildren)
                stream << embodiedChild->printTree(topDepth, depth, printPotentials);
        }
        if (printPotentials) {
            QVector<Child> children = m_children;
            if (!children.isEmpty()) {
                std::stable_sort(children.begin(), children.end(),
                    [=](const Child &a, const Child &b) {
                    return a.pValue() > b.pValue();
                });
                for (Child c : children) {
                    if (!c.isPotential())
                        continue;
                    stream << "\n";
                    const int d = this->depth() + 1;
                    for (int i = 0; i < d; ++i) {
                        stream << qSetFieldWidth(7) << "      |";
                    }
                    stream << right << qSetFieldWidth(6) << c.toString()
                        << qSetFieldWidth(2) << " ("
                        << qSetFieldWidth(4) << moveToNNIndex(c.move())
                        << qSetFieldWidth(1) << ")"
                        << qSetFieldWidth(4) << left << " p: " << c.pValue() * 100 << qSetFieldWidth(1) << left << "%";
                }
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
