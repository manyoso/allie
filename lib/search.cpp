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
