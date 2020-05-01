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

//#define USE_UNIFORM_BACKEND
//#define USE_FAST_UNIFORM_POLICY

const int s_moveHistory = 8;
const int s_planesPerPos = 13;
const int s_planeBase = s_planesPerPos * s_moveHistory;

inline void encodeGame(int i, const Game &g, const Game::Position &p,
    InputPlanes *result, Chess::Army us, Chess::Army them, bool nextMoveIsBlack)
{
    BitBoard ours = us == White ? p.board(White) : p.board(Black);
    BitBoard theirs = them == White ? p.board(White) : p.board(Black);
    BitBoard pawns = p.board(Pawn);
    BitBoard knights = p.board(Knight);
    BitBoard bishops = p.board(Bishop);
    BitBoard rooks = p.board(Rook);
    BitBoard queens = p.board(Queen);
    BitBoard kings = p.board(King);

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

inline void gameToInputPlanes(const Node *node, std::vector<InputPlane> *result)
{
    const Game &game = node->game();
    const Game::Position &position = node->position()->position();

    // *us* refers to the perspective of whoever is next to move
    const bool nextMoveIsBlack = position.activeArmy() == Black;
    const Chess::Army us = nextMoveIsBlack ? Black : White;
    const Chess::Army them = nextMoveIsBlack ? White : Black;

    HistoryIterator it = HistoryIterator::begin(node);
    int gamesEncoded = 0;
    Game lastGameEncoded = game;
    Game::Position lastPositionEncoded = position;
    for (; it != HistoryIterator::end() && gamesEncoded < s_moveHistory; ++it, ++gamesEncoded) {
        Game g = it.game();
        Game::Position p = it.position();
        encodeGame(gamesEncoded, g, p, result, us, them, nextMoveIsBlack);
        lastGameEncoded = g;
        lastPositionEncoded = p;
    }

    // Add fake history by repeating the position to fill it up as long as the last position in the
    // real history is not the startpos
    if (lastGameEncoded != Game()) {
        while (gamesEncoded < s_moveHistory) {
            encodeGame(gamesEncoded, lastGameEncoded, lastPositionEncoded, result, us, them, nextMoveIsBlack);
            ++gamesEncoded;
        }
    }

    if (position.isCastleAvailable(us, QueenSide)) (*result)[s_planeBase + 0].SetAll();
    if (position.isCastleAvailable(us, KingSide)) (*result)[s_planeBase + 1].SetAll();
    if (position.isCastleAvailable(them, QueenSide)) (*result)[s_planeBase + 2].SetAll();
    if (position.isCastleAvailable(them, KingSide)) (*result)[s_planeBase + 3].SetAll();
    if (us == Chess::Black) (*result)[s_planeBase + 4].SetAll();
    (*result)[s_planeBase + 5].Fill(game.halfMoveClock());
    // Plane s_planeBase + 6 used to be movecount plane, now it's all zeros.
    // Plane s_planeBase + 7 is all ones to help NN find board edges.
    (*result)[s_planeBase + 7].SetAll();
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
    m_usingFP16(false),
    m_usingCustomWinograd(false)
{
}

NeuralNet::~NeuralNet()
{
    qDeleteAll(m_availableNetworks);
}

Network *NeuralNet::createNewGPUNetwork(int id, bool useFP16, bool useCustomWinograd) const
{
    Q_ASSERT(m_weightsValid);
    if (!m_weightsValid)
        qFatal("Could not load NN weights!");

    if (useFP16)
        return createCudaFP16Network(s_weights, id, useCustomWinograd);
    else
        return createCudaNetwork(s_weights, id, useCustomWinograd);
}

void NeuralNet::reset()
{
    Q_ASSERT(m_weightsValid);
    const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt();
    const bool useFP16 = Options::globalInstance()->option("UseFP16").value() == "true";
    const bool useCustomWinograd = Options::globalInstance()->option("UseCustomWinograd").value() == "true";
    if (numberOfGPUCores == m_availableNetworks.count()
        && useFP16 == m_usingFP16
        && useCustomWinograd == m_usingCustomWinograd)
        return; // Nothing to do

    m_usingFP16 = useFP16;
    m_usingCustomWinograd = useCustomWinograd;
    qDeleteAll(m_availableNetworks);
    m_availableNetworks.clear();
    for (int i = 0; i < numberOfGPUCores; ++i) {
        QSharedPointer<lczero::Network> network(createNewGPUNetwork(i, m_usingFP16, m_usingCustomWinograd));
        m_availableNetworks.append(new Computation(network));
        m_availableNetworks.append(new Computation(network));
    }
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

Computation *NeuralNet::acquireNetwork()
{
    QMutexLocker locker(&m_mutex);
    while (m_availableNetworks.isEmpty())
        m_condition.wait(locker.mutex());
    return m_availableNetworks.takeFirst();
}

void NeuralNet::releaseNetwork(Computation *network)
{
    QMutexLocker locker(&m_mutex);
    m_availableNetworks.append(network);
    m_condition.wakeAll();
}

Computation::Computation(QSharedPointer<lczero::Network> network)
    : m_positions(0),
    m_network(network),
    m_computation(nullptr)
{
    m_inputPlanes.resize(s_planeBase + s_moveHistory);
}

Computation::~Computation()
{
    clear();
}

void Computation::reset()
{
    clear();
    m_computation = m_network->NewComputation().release();
}

int Computation::addPositionToEvaluate(const Node *node)
{
    m_inputPlanes.clear();
    m_inputPlanes.resize(s_planeBase + s_moveHistory);
    gameToInputPlanes(node, &m_inputPlanes);
    Q_ASSERT(m_computation);
    m_computation->AddInput(&m_inputPlanes);
    return m_positions++;
}

void Computation::evaluate()
{
    if (!m_computation) {
        qCritical() << "Cannot evaluation position because NN is not valid!";
        return;
    }

#if !defined(USE_UNIFORM_BACKEND)
    m_computation->ComputeBlocking();
#endif
}

void Computation::clear()
{
    m_positions = 0;
    delete m_computation;
    m_computation = nullptr;
}

float Computation::qVal(int index) const
{
    Q_ASSERT(m_computation);
    Q_ASSERT(index < m_positions);
#if !defined(USE_UNIFORM_BACKEND)
    return m_computation->GetQVal(index);
#else
    return 0.0f;
#endif
}

void Computation::setPVals(int index, Node *node) const
{
#if defined(USE_FAST_UNIFORM_POLICY)
    QVector<Node::Potential> *potentials = node->position()->potentials();
    for (int i = 0; i < potentials->count(); ++i)
        (&(*potentials)[i])->setPValue(1.0f);
#else
    Q_ASSERT(m_computation);
    Q_ASSERT(index < m_positions);
    Q_ASSERT(node);
    Q_ASSERT(node->hasPotentials());
    const Chess::Army activeArmy = node->position()->position().activeArmy();
    const QVector<Node::Potential> *potentials = node->position()->potentials();
    float total = 0;
    for (int i = 0; i < potentials->size(); ++i) {
        // We get a non-const reference to the actual value and change it in place
        const Node::Potential *potential = &(*potentials)[i];
        Move mv = potential->move();
        if (activeArmy == Chess::Black)
            mv.mirror(); // nn index expects the board to be flipped
#if !defined(USE_UNIFORM_BACKEND)
        const float p = fastpow(m_computation->GetPVal(index, moveToNNIndex(mv)), SearchSettings::policySoftmaxTempInverse);
#else
        float fakePolicy = 1.0f;
        moveToNNIndex(mv);
        const float p = fastpow(fakePolicy, SearchSettings::policySoftmaxTempInverse);
#endif
        total += p;
        const_cast<Node::Potential*>(potential)->setPValue(p);
    }

    const float scale = 1.0f / total;
    for (int i = 0; i < potentials->size(); ++i) {
        // We get a non-const reference to the actual value and change it in place
        const Node::Potential *potential = &(*potentials)[i];
        const_cast<Node::Potential*>(potential)->setPValue(scale * potential->pValue());
    }
#endif
}
