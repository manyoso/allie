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

struct Magic {
    quint64 magic = 0;
    quint64 mask = 0;
    quint64 shift = 0;
    quint64 *offset = nullptr;
};

class Movegen {
public:
    static Movegen *globalInstance();

    BitBoard kingMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    BitBoard queenMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    BitBoard rookMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    BitBoard bishopMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    BitBoard knightMoves(const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    BitBoard pawnMoves(Chess::Army army, const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;
    BitBoard pawnAttacks(Chess::Army army, const Square &sq, const BitBoard &friends, const BitBoard &enemies) const;

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

#endif // MOVEGEN_H
