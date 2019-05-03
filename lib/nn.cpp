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

#include "nn.h"

#include <QDebug>
#include <QFileInfo>
#include <QGlobalStatic>

#include "bitboard.h"
#include "chess.h"
#include "game.h"
#include "history.h"
#include "neural/loader.h"
#include "neural/nn_policy.h"
#include "node.h"
#include "notation.h"
#include "options.h"
#include "fastapprox/fastpow.h"

using namespace Chess;
using namespace lczero;

const int s_moveHistory = 8;
const int s_planesPerPos = 13;
const int s_planeBase = s_planesPerPos * s_moveHistory;

void encodeGame(int i, const Game &g, InputPlanes *result, Chess::Army us, Chess::Army them,
    bool nextMoveIsBlack)
{
    BitBoard ours = us == White ? g.board(White) : g.board(Black);
    BitBoard theirs = them == White ? g.board(White) : g.board(Black);
    BitBoard pawns = g.board(Pawn);
    BitBoard knights = g.board(Knight);
    BitBoard bishops = g.board(Bishop);
    BitBoard rooks = g.board(Rook);
    BitBoard queens = g.board(Queen);
    BitBoard kings = g.board(King);

    // If we are evaluating from black's perspective we need to flip the boards...
    if (nextMoveIsBlack) {
        ours.mirror();
        theirs.mirror();
        pawns.mirror();
        knights.mirror();
        bishops.mirror();
        rooks.mirror();
        queens.mirror();
        kings.mirror();
    }

    const size_t base = size_t(i * s_planesPerPos);

    (*result)[base + 0].mask = (ours & pawns).data();
    (*result)[base + 1].mask = (ours & knights).data();
    (*result)[base + 2].mask = (ours & bishops).data();
    (*result)[base + 3].mask = (ours & rooks).data();
    (*result)[base + 4].mask = (ours & queens).data();
    (*result)[base + 5].mask = (ours & kings).data();

    (*result)[base + 6].mask = (theirs & pawns).data();
    (*result)[base + 7].mask = (theirs & knights).data();
    (*result)[base + 8].mask = (theirs & bishops).data();
    (*result)[base + 9].mask = (theirs & rooks).data();
    (*result)[base + 10].mask = (theirs & queens).data();
    (*result)[base + 11].mask = (theirs & kings).data();
    if (g.repetitions() >= 1)
        (*result)[base + 12].SetAll();

    // FIXME: Encode enpassant target
}

InputPlanes gameToInputPlanes(const Node *node)
{
    Game game = node->game();
    InputPlanes result(s_planeBase + s_moveHistory);

    // *us* refers to the perspective of whoever is next to move
    bool nextMoveIsBlack = game.activeArmy() == Black;
    Chess::Army us = nextMoveIsBlack ? Black : White;
    Chess::Army them = nextMoveIsBlack ? White : Black;

    HistoryIterator it = HistoryIterator::begin(node);
    int gamesEncoded = 0;
    Game lastGameEncoded = game;
    for (; it != HistoryIterator::end() && gamesEncoded < s_moveHistory; ++it, ++gamesEncoded) {
        encodeGame(gamesEncoded, *it, &result, us, them, nextMoveIsBlack);
        lastGameEncoded = *it;
    }

    // Add fake history by repeating the position to fill it up as long as the last position in the
    // real history is not the startpos
    if (lastGameEncoded != Game()) {
        while (gamesEncoded < s_moveHistory) {
            encodeGame(gamesEncoded, lastGameEncoded, &result, us, them, nextMoveIsBlack);
            ++gamesEncoded;
        }
    }

    if (game.isCastleAvailable(us, QueenSide)) result[s_planeBase + 0].SetAll();
    if (game.isCastleAvailable(us, KingSide)) result[s_planeBase + 1].SetAll();
    if (game.isCastleAvailable(them, QueenSide)) result[s_planeBase + 2].SetAll();
    if (game.isCastleAvailable(them, KingSide)) result[s_planeBase + 3].SetAll();
    if (us == Chess::Black) result[s_planeBase + 4].SetAll();
    result[s_planeBase + 5].Fill(game.halfMoveClock());
    // Plane s_planeBase + 6 used to be movecount plane, now it's all zeros.
    // Plane s_planeBase + 7 is all ones to help NN find board edges.
    result[s_planeBase + 7].SetAll();

    return result;
}

static WeightsFile s_weights;

class MyNeuralNet : public NeuralNet { };
Q_GLOBAL_STATIC(MyNeuralNet, nnInstance)
NeuralNet *NeuralNet::globalInstance()
{
    return nnInstance();
}

NeuralNet::NeuralNet()
    : m_weightsValid(false),
    m_usingFP16(false)
{
}

NeuralNet::~NeuralNet()
{
    qDeleteAll(m_availableNetworks);
}

Network *NeuralNet::createNewNetwork(int id, bool useFP16) const
{
    Q_ASSERT(m_weightsValid);
    if (!m_weightsValid)
        qFatal("Could not load NN weights!");

    if (useFP16)
        return createCudaFP16Network(s_weights, id);
    else
        return createCudaNetwork(s_weights, id);
}

void NeuralNet::reset()
{
    if (!m_weightsValid) {
        s_weights = LoadWeightsFromFile(DiscoverWeightsFile());
        m_weightsValid = true;
    }

    const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt();
    const bool useFP16 = Options::globalInstance()->option("UseFP16").value() == "true";
    if (numberOfGPUCores == m_availableNetworks.count()
        && useFP16 == m_usingFP16)
        return; // Nothing to do

    m_usingFP16 = useFP16;
    qDeleteAll(m_availableNetworks);
    m_availableNetworks.clear();
    for (int i = 0; i < numberOfGPUCores; ++i)
        m_availableNetworks.append(createNewNetwork(i, m_usingFP16));
}

void NeuralNet::setWeights(const QString &pathToWeights)
{
    QFileInfo info(pathToWeights);
    if (info.exists()) {
        s_weights = LoadWeightsFromFile(pathToWeights.toStdString());
        m_weightsValid = true;
    } else {
        qFatal("Could not load NN weights!");
    }
}

Network *NeuralNet::acquireNetwork()
{
    QMutexLocker locker(&m_mutex);
    while (m_availableNetworks.isEmpty())
        m_condition.wait(locker.mutex());
    return m_availableNetworks.takeFirst();
}

void NeuralNet::releaseNetwork(Network *network)
{
    QMutexLocker locker(&m_mutex);
    m_availableNetworks.append(network);
    m_condition.wakeAll();
}

Computation::Computation(Network *network)
    : m_positions(0),
    m_network(network),
    m_computation(nullptr)
{
    m_acquired = m_network != nullptr;
}

Computation::~Computation()
{
    clear();
}

int Computation::addPositionToEvaluate(const Node *node)
{
    if (!m_computation) {
        Q_ASSERT(m_acquired == (m_network != nullptr));
        if (!m_network) {
            m_network = NeuralNet::globalInstance()->acquireNetwork(); // blocks
        }
        m_computation = m_network->NewComputation().release();
    }

    const int maximumBatchSize = Options::globalInstance()->option("MaxBatchSize").value().toInt();
    Q_ASSERT(m_positions <= maximumBatchSize);
    m_computation->AddInput(gameToInputPlanes(node));
    return m_positions++;
}

void Computation::evaluate()
{
    if (!m_computation) {
        qCritical() << "Cannot evaluation position because NN is not valid!";
        return;
    }

    m_computation->ComputeBlocking();
}

void Computation::clear()
{
    m_positions = 0;
    delete m_computation;
    m_computation = nullptr;
    if (!m_acquired)
        NeuralNet::globalInstance()->releaseNetwork(m_network); // release back into the pool
    m_network = nullptr;
}

float Computation::qVal(int index) const
{
    Q_ASSERT(index < m_positions);
    return m_computation->GetQVal(index);
}

void Computation::setPVals(int index, Node *node) const
{
    Q_ASSERT(index < m_positions);
    Q_ASSERT(node->hasPotentials());
    QVector<PotentialNode*> potentials = node->potentials();
    QVector<QPair<float, PotentialNode*>> policyValues;
    policyValues.reserve(potentials.size());
    float total = 0;
    for (PotentialNode *n : potentials) {
        Move mv = n->move();
        if (node->game().activeArmy() == Chess::Black)
            mv.mirror(); // nn index expects the board to be flipped
        const float p = fastpow(m_computation->GetPVal(index, moveToNNIndex(mv)), SearchSettings::policySoftmaxTemp);
        total += p;
        policyValues.append(qMakePair(p, n));
    }

    QVector<QPair<float, PotentialNode*>>::const_iterator it = policyValues.begin();
    const float scale = 1.0f / total;
    float normalizedTotal = 0;
    for (; it != policyValues.end(); ++it) {
        float normalizedP = scale * it->first;
        it->second->setPValue(normalizedP);
        normalizedTotal += normalizedP;
    }
}
