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

#include "history.h"

class MyHistory : public History { };
Q_GLOBAL_STATIC(MyHistory, HistoryInstance)
History* History::globalInstance()
{
    return HistoryInstance();
}

void History::addGame(const Game &game)
{
    qint8 r = 0;
    const QVector<Game> previous = m_history;
    QVector<Game>::const_reverse_iterator it = previous.crbegin();
    for (; it != previous.crend(); ++it) {
        if (game.isSamePosition(*it))
            ++r;

        if (r >= 2)
            break; // No sense in counting further

        if (!(*it).halfMoveClock())
            break;
    }

    Game g = game;
    g.setRepetitions(r);
    m_history.append(g);
}
