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

#ifndef SQUARE_H
#define SQUARE_H

#include <QString>
#include <QVector>

class Square {
public:
    Square()
        : m_data(64)
    {
        Q_ASSERT(!isValid());
    }

    Square(quint8 data)
        : m_data(data)
    {
    }

    Square(int file, int rank)
    {
        m_data = quint8(rank * 8 + file);
    }

    ~Square() {}

    int file() const { return m_data % 8; }
    int rank() const { return m_data / 8; }

    quint8 data() const { return m_data; }

    bool isValid() const;

    void mirror();

private:
    quint8 m_data;
};

inline uint qHash(const Square &key) { return uint(key.data()); }
inline bool operator==(const Square &a, const Square &b) { return a.file() == b.file() && a.rank() == b.rank(); }
inline bool operator!=(const Square &a, const Square &b) { return a.file() != b.file() || a.rank() != b.rank(); }

inline bool Square::isValid() const
{
    const int f = file();
    const int r = rank();
    return f >= 0 && f <= 7 && r >= 0 && r <= 7;
}

inline void Square::mirror()
{
    // flipping the rank...
    // 0 becomes 7
    // 1 becomes 6
    // 2 becomes 5
    // 3 becomes 4
    // 4 becomes 3
    // 5 becomes 2
    // 6 becomes 1
    // 7 becomes 0
    m_data = m_data ^ 0b111000;
}

QDebug operator<<(QDebug, const Square &);

typedef QVector<Square> SquareList;

#endif
