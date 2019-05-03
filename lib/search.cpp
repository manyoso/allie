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
float SearchSettings::cpuctF = 2.0f;
float SearchSettings::cpuctInit = 3.4f;
float SearchSettings::cpuctBase = 10000;
float SearchSettings::fpuReduction = 1.2f;
float SearchSettings::policySoftmaxTemp = 1 / 2.2f;
int SearchSettings::tryPlayoutLimit = 32;
int SearchSettings::vldMax = 10000;

QDebug operator<<(QDebug debug, const Search &search)
{
    if (!search.searchMoves.isEmpty())
        debug << "searchmoves: " << search.searchMoves;
    if (search.wtime != -1)
        debug << "movestogo: " << search.wtime;
    if (search.btime != -1)
        debug << "movestogo: " << search.btime;
    if (search.winc != -1)
        debug << "movestogo: " << search.winc;
    if (search.binc != -1)
        debug << "movestogo: " << search.binc;
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

QString trendToString(Trend t)
{
    switch (t) {
    case Worse: return "Worse";
    case Better: return "Better";
    }
    Q_UNREACHABLE();
}
