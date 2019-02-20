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

#include "square.h"

#include <QDebug>

#include "notation.h"

QDebug operator<<(QDebug debug, const Square &square)
{
    switch (square.file()) {
    case 0:
        debug.nospace() << "a";
        break;
    case 1:
        debug.nospace() << "b";
        break;
    case 2:
        debug.nospace() << "c";
        break;
    case 3:
        debug.nospace() << "d";
        break;
    case 4:
        debug.nospace() << "e";
        break;
    case 5:
        debug.nospace() << "f";
        break;
    case 6:
        debug.nospace() << "g";
        break;
    case 7:
        debug.nospace() << "h";
        break;
    }
    debug.noquote() << QString::number(square.rank() + 1);
    return debug.space();
}
