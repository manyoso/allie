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
float SearchSettings::cpuctInit = 2.1f;
float SearchSettings::cpuctBase = 15000;
float SearchSettings::fpuReduction = 0.443f;
float SearchSettings::policySoftmaxTemp = 1.607f;
float SearchSettings::policySoftmaxTempInverse = 1 / policySoftmaxTemp;
float SearchSettings::openingTimeFactor = 2.15f;
float SearchSettings::earlyExitFactor = 0.72f;
int SearchSettings::tryPlayoutLimit = 136;
int SearchSettings::vldMax = 10000;
QString SearchSettings::weightsFile = QString();
bool SearchSettings::debugInfo = true;
bool SearchSettings::chess960 = false;
SearchSettings::Features SearchSettings::featuresOff = SearchSettings::None;

SearchSettings::Features SearchSettings::stringToFeatures(const QString &string)
{
    Features f;
    QStringList list = string.split(',');
    for (QString s : list) {
        if (s == QLatin1String("threading"))
            f.setFlag(SearchSettings::Threading, true);
        if (s == QLatin1String("earlyexit"))
            f.setFlag(SearchSettings::EarlyExit, true);
        if (s == QLatin1String("transpositions"))
            f.setFlag(SearchSettings::Transpositions, true);
        if (s == QLatin1String("minimax"))
            f.setFlag(SearchSettings::Minimax, true);
        if (s == QLatin1String("treereuse"))
            f.setFlag(SearchSettings::TreeReuse, true);
    }
    return f;
}

QString SearchSettings::featuresToString(SearchSettings::Features f)
{
    QStringList list;
    if (f.testFlag(SearchSettings::None))
        return QLatin1String("none");
    if (f.testFlag(SearchSettings::Threading))
        list.append(QLatin1String("Threading"));
    if (f.testFlag(SearchSettings::EarlyExit))
        list.append(QLatin1String("EarlyExit"));
    if (f.testFlag(SearchSettings::Transpositions))
        list.append(QLatin1String("Transpositions"));
    if (f.testFlag(SearchSettings::Minimax))
        list.append(QLatin1String("Minimax"));
    return list.join(", ");
}

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
