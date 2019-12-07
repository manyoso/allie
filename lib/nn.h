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

#ifndef NN_H
#define NN_H

#include <QMutex>
#include <QWaitCondition>

#include "game.h"

namespace lczero {
    class Network;
    class NetworkComputation;
};

class Node;
class Computation {
public:
    Computation(lczero::Network *network);
    ~Computation();

    int addPositionToEvaluate(const Node *node);
    int positions() const { return m_positions; }
    void evaluate();
    void clear();

    float qVal(int index) const;
    void setPVals(int index, Node *node) const;

private:
    int m_positions;
    lczero::Network *m_network;
    lczero::NetworkComputation *m_computation;
};

class NeuralNet {
public:
    static NeuralNet *globalInstance();

    void reset();
    void setWeights(const QString &pathToWeights);
    lczero::Network *acquireNetwork(); // will block until a network is ready
    void releaseNetwork(lczero::Network *network); // must be called when you are done

private:
    NeuralNet();
    ~NeuralNet();
    lczero::Network *createNewGPUNetwork(int id, bool fp16) const;
    QVector<lczero::Network*> m_availableNetworks;
    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_weightsValid;
    bool m_usingFP16;
    friend class Computation;
    friend class MyNeuralNet;
};

#endif // NN_H
