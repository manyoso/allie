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

#ifndef BITBOARD_H
#define BITBOARD_H

#include <QDebug>

#include "square.h"

class BitBoard {
public:
    class Iterator {
    public:
        inline bool operator!=(const Iterator& other) const
        {
            return m_data != other.m_data;
        }

        inline void operator++()
        {
            m_data &= (m_data - 1);
        }

        inline Square operator*() const
        {
            return BitBoard::indexToSquare(quint8(qCountTrailingZeroBits(m_data)));
        }

    private:
        friend class BitBoard;
        Iterator(quint64 data) : m_data(data) {}
        quint64 m_data;
    };

    BitBoard() { m_data = 0; }
    BitBoard(quint64 data) { m_data = data; }
    BitBoard(const Square &square);

    inline bool isClear() const
    {
        return m_data == 0;
    }

    inline Iterator begin() const { return Iterator(m_data); }
    inline Iterator end() const { return Iterator(0); }

    inline bool isSquareOccupied(const Square &square) const
    {
        return testBit(squareToIndex(square));
    }

    SquareList occupiedSquares() const;
    void setBoard(const SquareList &squareList);
    void setSquare(const Square &square);

    inline static Square indexToSquare(quint8 bit)
    {
        return Square(bit);
    }

    inline static int squareToIndex(const Square &square)
    {
        return square.data();
    }

    // Flips the board from white <--> black perspective
    inline void mirror()
    {
        m_data = (m_data & 0x00000000FFFFFFFF) << 32 |
                 (m_data & 0xFFFFFFFF00000000) >> 32;
        m_data = (m_data & 0x0000FFFF0000FFFF) << 16 |
                 (m_data & 0xFFFF0000FFFF0000) >> 16;
        m_data = (m_data & 0x00FF00FF00FF00FF) << 8 |
                 (m_data & 0xFF00FF00FF00FF00) >> 8;
    }

    inline quint64 data() const { return m_data; }

    int count() const;

    inline bool testBit(int i) const
    {
        return m_data & (quint64(1) << i);
    }

    inline void setBit(int i)
    {
        m_data |= (quint64(1) << i);
    }

    inline void setBit(int i, bool on)
    {
        if (on)
            m_data |= (quint64(1) << i);
        else
            m_data &= ~(quint64(1) << i);
    }

    inline BitBoard operator~() const
    {
         return BitBoard(~m_data);
    }

    inline bool operator!=(const BitBoard &other) const { return m_data != other.m_data; }
    inline bool operator==(const BitBoard &other) const { return m_data == other.m_data; }

    inline friend BitBoard operator|(const BitBoard &a, const BitBoard &b)
    {
        return a.data() | b.data();
    }

    inline friend BitBoard operator^(const BitBoard &a, const BitBoard &b)
    {
        return quint64(a.data() ^ b.data());
    }

    inline friend BitBoard operator&(const BitBoard &a, const BitBoard &b)
    {
        return quint64(a.data() & b.data());
    }

private:
    quint64 m_data;
};

QDebug operator<<(QDebug, const BitBoard &);

#endif
