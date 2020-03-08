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

#include "search.h"

#include <QMetaType>

// Various constants and settings used by search
float SearchSettings::cpuctF = 2.817f;
float SearchSettings::cpuctInit = 1.9f;
float SearchSettings::cpuctBase = 15000;
float SearchSettings::fpuReduction = 0.443f;
float SearchSettings::policySoftmaxTemp = 1 / 1.607f;
float SearchSettings::openingTimeFactor = 1.75;
qint64 SearchSettings::earlyExitMinimumTime = 500;
int SearchSettings::tryPlayoutLimit = 32;
int SearchSettings::vldMax = 10000;
QString SearchSettings::weightsFile = QString();

void SearchInfo::calculateSpeeds(qint64 t)
{
    time = t;
    nps = qRound(qreal(nodes) / qMax(qint64(1), t) * 1000.0);
    rawnps = qRound(qreal(workerInfo.nodesVisited) / qMax(qint64(1), t) * 1000.0);
    nnnps = qRound(qreal(workerInfo.nodesEvaluated) / qMax(qint64(1), t) * 1000.0);
}

SearchInfo SearchInfo::nodeAndBatchDiff(const SearchInfo &a, const SearchInfo &b)
{
    // The instant diff looks at the nodes and batches
    SearchInfo diff = b;
    diff.nodes = a.nodes - b.nodes;
    diff.workerInfo.nodesSearched = a.workerInfo.nodesSearched - b.workerInfo.nodesSearched;
    diff.workerInfo.nodesEvaluated = a.workerInfo.nodesEvaluated - b.workerInfo.nodesEvaluated;
    diff.workerInfo.nodesVisited = a.workerInfo.nodesVisited - b.workerInfo.nodesVisited;
    diff.workerInfo.numberOfBatches = a.workerInfo.numberOfBatches - b.workerInfo.numberOfBatches;
    diff.workerInfo.nodesCacheHits = a.workerInfo.nodesCacheHits - b.workerInfo.nodesCacheHits;
    diff.workerInfo.nodesTBHits = a.workerInfo.nodesTBHits - b.workerInfo.nodesTBHits;
    return diff;
}

QDebug operator<<(QDebug debug, const Search &search)
{
    if (!search.searchMoves.isEmpty())
        debug << "searchmoves: " << search.searchMoves;
    if (search.wtime != -1)
        debug << "wtime: " << search.wtime;
    if (search.btime != -1)
        debug << "btime: " << search.btime;
    if (search.winc != -1)
        debug << "winc: " << search.winc;
    if (search.binc != -1)
        debug << "binc: " << search.binc;
    if (search.movestogo != -1)
        debug << "movestogo: " << search.movestogo;
    if (search.depth != -1)
        debug << "depth: " << search.depth;
    if (search.nodes != -1)
        debug << "nodes: " << search.nodes;
    if (search.mate != -1)
        debug << "mate: " << search.mate;
    if (search.movetime != -1)
        debug << "movetime: " << search.movetime;
    if (search.infinite)
        debug << "infinite: " << search.infinite;

    return debug.space();
}
