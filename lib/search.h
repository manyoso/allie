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

#ifndef SEARCH_H
#define SEARCH_H

#include <QDebug>

#include "move.h"
#include "game.h"

struct Search {
    QVector<Move> searchMoves;
    qint64 wtime = -1;
    qint64 btime = -1;
    qint64 winc = -1;
    qint64 binc = -1;
    qint64 movestogo = -1;
    qint64 depth = -1;
    qint64 nodes = -1;
    qint64 mate = -1;
    qint64 movetime = -1;
    bool infinite = false;
};

struct SearchSettings {
    enum Feature {
        None = 0x0,
        Threading = 0x1,
        EarlyExit = 0x2,
        Transpositions = 0x4,
        Minimax = 0x8
    };
    Q_DECLARE_FLAGS(Features, Feature)

    static float cpuctF;
    static float cpuctInit;
    static float cpuctBase;
    static float fpuReduction;
    static float policySoftmaxTemp;
    static float openingTimeFactor;
    static float earlyExitFactor;
    static int tryPlayoutLimit;
    static int vldMax;
    static QString weightsFile;
    static bool debugInfo;
    static Features featuresOff;

    static Features stringToFeatures(const QString&);
    static QString featuresToString(Features f);
};

QDebug operator<<(QDebug, const Search &);

struct WorkerInfo {
    int sumDepths = 0;
    int maxDepth = 0;
    int nodesSearched = 0;
    int nodesEvaluated = 0;
    int nodesVisited = 0;
    int numberOfBatches = 0;
    int nodesCacheHits = 0;
    int nodesTBHits = 0;
    int searchId = 0;
    QString threadId;
};

struct SearchInfo {
    int depth = -1;
    int seldepth = -1;
    qint64 time = -1;
    int nodes = -1;
    QString score;
    int nps = -1;
    int batchSize = -1;
    QString pv;
    int rawnps = -1;
    int nnnps = -1;
    QString bestMove;
    QString ponderMove;
    bool isResume = false;
    bool isDTZ = false;
    bool bestIsMostVisited = true;
    WorkerInfo workerInfo;
    int games = 0;

    void calculateSpeeds(qint64 time);
    static SearchInfo nodeAndBatchDiff(const SearchInfo &a, const SearchInfo &b);
};

#endif // SEARCH_H
