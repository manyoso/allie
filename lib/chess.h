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

#ifndef CHESS_H
#define CHESS_H

#include <QDebug>

namespace Chess {

enum Army : bool
{
    White,
    Black
};
inline QDebug operator<<(QDebug debug, const Army &a)
{
    debug << (a == White ? "White" : "Black");
    return debug.nospace();
}

enum Castle : bool
{
    KingSide,
    QueenSide
};

enum PieceType : quint8
{
    Unknown,
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn
};
inline QDebug operator<<(QDebug debug, const PieceType &p)
{
    switch (p) {
    case PieceType::Unknown:
        debug << "Unknown";
        break;
    case King:
        debug << "King";
        break;
    case Queen:
        debug << "Queen";
        break;
    case Rook:
        debug << "Rook";
        break;
    case Bishop:
        debug << "Bishop";
        break;
    case Knight:
        debug << "Knight";
        break;
    case Pawn:
        debug << "Pawn";
        break;
        }
    return debug.nospace();
}

enum NotationType : quint8
{
    Standard, /* The standard algebraic notation found in PGN. */
    Long,     /* Hyphenated long algebraic notation. */
    Computer  /* Un-hyphenated long algebraic notation. UCI uses this for example. */
};

enum Ending : quint8
{
    InProgress,
    CheckMate,
    StaleMate,
    Resignation,
    DrawAccepted,
    HalfMoveClock
};

enum Result : quint8
{
    NoResult,
    WhiteWins,
    BlackWins,
    Drawn
};

}

#endif
