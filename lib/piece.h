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

#ifndef PIECE_H
#define PIECE_H

#include <QVector>
#include <QString>

#include "chess.h"
#include "square.h"

class Piece {
public:
    Piece();
    Piece(Chess::Army army, Chess::PieceType piece, const Square &square);
    ~Piece();

    Chess::Army army() const { return m_army; }

    Chess::PieceType piece() const { return m_piece; }
    void setPiece(Chess::PieceType piece) { m_piece = piece; }

    Square square() const { return m_square; }
    void setSquare(const Square &square) { m_square = square; }

private:
    Chess::Army m_army;
    Chess::PieceType m_piece;
    Square m_square;
};

inline uint qHash(const Piece &key) { return uint(key.square().data() + key.army() + key.piece()); }
inline bool operator==(const Piece &a, const Piece &b)
{ return a.army() == b.army() && a.piece() == b.piece() && a.square() == b.square(); }

typedef QVector<Piece> PieceList;

QDebug operator<<(QDebug, const Piece &);

#endif
