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

#ifndef HISTORY_H
#define HISTORY_H

#include <QtGlobal>

#include "game.h"

class History {
public:
    static History *globalInstance();

    QVector<Game> games() const { return m_history; }

    Game currentGame() const
    {
        if (m_history.isEmpty())
            return Game();
        return m_history.last();
    }

    void addGame(const Game &game);

    void clear()
    {
        m_history.clear();
    }

private:
    History()
    {
    }

    ~History() {}
    QVector<Game> m_history;
    friend class MyHistory;
};

#endif // HISTORY_H
