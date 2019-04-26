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
#include "node.h"

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

    Game at(int index) const
    {
        Q_ASSERT(index >= 0);
        Q_ASSERT(index < m_history.count());
        return m_history.at(index);
    }

    int count() const
    {
        return m_history.count();
    }

    ~History() {}
    QVector<Game> m_history;
    friend class MyHistory;
    friend class HistoryIterator;
};

class HistoryIterator {
public:
    bool operator!=(const HistoryIterator& other) const;
    Game operator*();
    void operator++();

    static HistoryIterator begin(const Node *data);
    static HistoryIterator end();

private:
    HistoryIterator(const Node *data);
    HistoryIterator();
    const Node *node;
    int historyPosition;
};

inline HistoryIterator::HistoryIterator()
{
    node = nullptr;
    historyPosition = -1;
}

inline HistoryIterator::HistoryIterator(const Node *data)
{
    node = data;
    historyPosition = -1;
}

inline HistoryIterator HistoryIterator::begin(const Node *data)
{
    return HistoryIterator(data);
}

inline HistoryIterator HistoryIterator::end()
{
    return HistoryIterator();
}

inline bool HistoryIterator::operator!=(const HistoryIterator& other) const
{
    return node != other.node || historyPosition != other.historyPosition;
}

inline Game HistoryIterator::operator*()
{
    if (node)
        return node->game();
    else if (historyPosition != -1)
        return History::globalInstance()->at(historyPosition);
    return Game();
}

inline void HistoryIterator::operator++()
{
    if (node) {
        if (node->parent()) {
            node = node->parent();
            return;
        } else {
            node = nullptr;
            historyPosition = qMax(-1, History::globalInstance()->count() - 2);
        }
    } else if (historyPosition >= 0) {
        --historyPosition;
    }
}

#endif // HISTORY_H
