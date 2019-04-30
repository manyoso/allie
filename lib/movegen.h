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

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include <QtGlobal>

#include "bitboard.h"
#include "chess.h"

#ifdef USE_PEXT
#include <immintrin.h>
#endif

struct Magic {
    quint64 magic = 0;
    quint64 mask = 0;
    quint64 shift = 0;
    quint64 *offset = nullptr;
};

class Movegen {
public:
    static Movegen *globalInstance();

    inline BitBoard kingMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    inline BitBoard queenMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    inline BitBoard rookMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    inline BitBoard bishopMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    inline BitBoard knightMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    inline BitBoard pawnMoves(Chess::Army army, const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    inline BitBoard pawnAttacks(Chess::Army army, const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;

private:
    Movegen();
    ~Movegen();

    enum Direction { North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest };
    static BitBoard raysForKing(const Square &sq);
    static BitBoard raysForQueen(const Square &sq);
    static BitBoard raysForRook(const Square &sq);
    static BitBoard raysForBishop(const Square &sq);
    static BitBoard raysForKnight(const Square &sq);
    static BitBoard raysForPawn(Chess::Army army, const Square &sq);
    static BitBoard raysForPawnAttack(Chess::Army army, const Square &sq);
    static void generateRay(Direction direction, int magnitude, const Square &sq, BitBoard *rays);

    BitBoard m_kingMoves[64];
    BitBoard m_knightMoves[64];
    BitBoard m_pawnMoves[2][64];
    BitBoard m_pawnAttacks[2][64];
    Magic m_rookTable[64];
    Magic m_bishopTable[64];
    friend class MyMovegen;
};

inline quint64 sliderIndex(const BitBoard &occupied, const Magic *table)
{
#ifdef USE_PEXT
    return _pext_u64(occupied.data(), table->mask);
#else
    return (((occupied.data() & table->mask) * table->magic) >> table->shift);
#endif
}

inline BitBoard Movegen::pawnMoves(Chess::Army army, const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    return m_pawnMoves[army][sq.data()] & BitBoard(~enemies.data()) & BitBoard(~friends.data());
}

inline BitBoard Movegen::pawnAttacks(Chess::Army army, const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    return (m_pawnAttacks[army][sq.data()] & enemies) & BitBoard(~friends.data());
}

inline BitBoard Movegen::knightMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    Q_UNUSED(enemies);
    return m_knightMoves[sq.data()] & BitBoard(~friends.data());
}

inline BitBoard Movegen::bishopMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    const BitBoard occupied(friends | enemies);
    const BitBoard destinations = ~occupied | enemies;
    return m_bishopTable[sq.data()].offset[sliderIndex(occupied, &m_bishopTable[sq.data()])] & destinations;
}

inline BitBoard Movegen::rookMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    const BitBoard occupied(friends | enemies);
    const BitBoard destinations = ~occupied | enemies;
    return m_rookTable[sq.data()].offset[sliderIndex(occupied, &m_rookTable[sq.data()])] & destinations;
}

inline BitBoard Movegen::queenMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    return bishopMoves(sq, friends, enemies) | rookMoves(sq, friends, enemies);
}

inline BitBoard Movegen::kingMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const
{
    Q_UNUSED(enemies);
    return m_kingMoves[sq.data()] & BitBoard(~friends.data());
}

#endif // MOVEGEN_H
