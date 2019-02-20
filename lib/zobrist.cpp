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

#include "zobrist.h"

#include <QGlobalStatic>
#include <random>

using namespace Chess;

class MyZobrist : public Zobrist { };
Q_GLOBAL_STATIC(MyZobrist, zobristInstance)
Zobrist *Zobrist::globalInstance()
{
    return zobristInstance();
}

Zobrist::Zobrist()
{
    // Make the keys deterministic
    std::default_random_engine generator(128612482);
    std::uniform_int_distribution<quint64> distribution;

    // https://en.wikipedia.org/wiki/Zobrist_hashing
    for (int i = 0; i < 64; ++i) {
        QVector<quint64> keys;
        for (int i = 0; i < 12; ++i) {
            quint64 a = distribution(generator);
            keys.append(a);
        }
        m_pieceKeys.append(keys);
    }

    // activearmy
    m_otherKeys.append(distribution(generator));
    // enpassant
    m_otherKeys.append(distribution(generator));
    // white kingside castle
    m_otherKeys.append(distribution(generator));
    // black kingside castle
    m_otherKeys.append(distribution(generator));
    // white queenside castle
    m_otherKeys.append(distribution(generator));
    // black queenside castle
    m_otherKeys.append(distribution(generator));
}

quint64 Zobrist::hash(const Game &game) const
{
    quint64 h = 0;

    {
        BitBoard pieces = game.board(King);
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            int squareIndex = BitBoard::squareToIndex(*sq);
            int pieceIndex = game.board(White).testBit(squareIndex) ? 0 : 1;
            h ^= m_pieceKeys[squareIndex][pieceIndex];
        }
    }

    {
        BitBoard pieces = game.board(Queen);
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            int squareIndex = BitBoard::squareToIndex(*sq);
            int pieceIndex = game.board(White).testBit(squareIndex) ? 2 : 3;
            h ^= m_pieceKeys[squareIndex][pieceIndex];
        }
    }

    {
        BitBoard pieces = game.board(Rook);
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            int squareIndex = BitBoard::squareToIndex(*sq);
            int pieceIndex = game.board(White).testBit(squareIndex) ? 4 : 5;
            h ^= m_pieceKeys[squareIndex][pieceIndex];
        }
    }

    {
        BitBoard pieces = game.board(Bishop);
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            int squareIndex = BitBoard::squareToIndex(*sq);
            int pieceIndex = game.board(White).testBit(squareIndex) ? 6 : 7;
            h ^= m_pieceKeys[squareIndex][pieceIndex];
        }
    }

    {
        BitBoard pieces = game.board(Knight);
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            int squareIndex = BitBoard::squareToIndex(*sq);
            int pieceIndex = game.board(White).testBit(squareIndex) ? 8 : 9;
            h ^= m_pieceKeys[squareIndex][pieceIndex];
        }
    }

    {
        BitBoard pieces = game.board(Pawn);
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            int squareIndex = BitBoard::squareToIndex(*sq);
            int pieceIndex = game.board(White).testBit(squareIndex) ? 10 : 11;
            h ^= m_pieceKeys[squareIndex][pieceIndex];
        }
    }

    // activearmy
    if (game.activeArmy() == Black)
        h ^= m_otherKeys[0];
    // enpassant
    if (game.enPassantTarget().isValid()) {
        Square sq = game.enPassantTarget();
        h ^= quint64(sq.file()) ^ quint64(sq.rank()) ^ m_otherKeys[1];
    }
    // white kingside castle
    if (game.isCastleAvailable(White, KingSide))
        h ^= m_otherKeys[2];
    // black kingside castle
    if (game.isCastleAvailable(Black, KingSide))
        h ^= m_otherKeys[3];
    // white queenside castle
    if (game.isCastleAvailable(White, QueenSide))
        h ^= m_otherKeys[4];
    // black queenside castle
    if (game.isCastleAvailable(Black, QueenSide))
        h ^= m_otherKeys[5];

    return h;
}
