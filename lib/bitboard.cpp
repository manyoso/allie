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

#include "bitboard.h"

#include <QDebug>

#include "notation.h"

QDebug operator<<(QDebug debug, const BitBoard &b)
{
    debug.nospace();
    for (qint8 i = 7; i > -1; --i) {
        debug.nospace() << "\n";
        for (qint8 j = 0; j < 8; ++j) {
            Square square(j, i);
            if (b.isSquareOccupied(square))
                debug.nospace() << "1";
            else
                debug.nospace() << "0";
        }
    }

    return debug.space();
}
