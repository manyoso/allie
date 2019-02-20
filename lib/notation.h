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

#ifndef NOTATION_H
#define NOTATION_H

#include <QString>

#include "chess.h"

class Move;
class Square;

/* TODO
 * Draw offer...
 * Check...
 * CheckMate...
 * Promotion...
 * Castling...
 * En Passant...
 * Disambiguation eg, FileOfDeparture, RankOfDeparture, etc...
 * Error handling and messages
 */

class Notation {
public:
    static Move stringToMove(const QString &string, Chess::NotationType notation = Chess::Standard, bool *ok = 0, QString *err = 0);
    static QString moveToString(const Move &move, Chess::NotationType notation = Chess::Standard);

    static Square stringToSquare(const QString &string, Chess::NotationType notation = Chess::Standard, bool *ok = 0, QString *err = 0);
    static QString squareToString(const Square &square, Chess::NotationType notation = Chess::Standard);

    static Chess::PieceType charToPiece(const QChar &ch, Chess::NotationType notation = Chess::Standard, bool *ok = 0, QString *err = 0);
    static QChar pieceToChar(Chess::PieceType piece, Chess::NotationType notation = Chess::Standard);

    static int charToFile(const QChar &ch, Chess::NotationType notation = Chess::Standard, bool *ok = 0, QString *err = 0);
    static QChar fileToChar(int file, Chess::NotationType notation = Chess::Standard);

    static int charToRank(const QChar &ch, Chess::NotationType notation = Chess::Standard, bool *ok = 0, QString *err = 0);
    static QChar rankToChar(int rank, Chess::NotationType notation = Chess::Standard);

private:
    Notation();
    ~Notation();
};

#endif
