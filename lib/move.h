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

#ifndef MOVE_H
#define MOVE_H

#include <QString>
#include <QVector>

#include "chess.h"
#include "square.h"

class Move {
public:
    Move() : m_data(0) { }

    Square start() const;
    void setStart(const Square &start);

    Square end() const;
    void setEnd(const Square &end);

    Chess::PieceType piece() const;
    void setPiece(Chess::PieceType piece);

    Chess::PieceType promotion() const;
    void setPromotion(Chess::PieceType promotion);

    bool isCapture() const;
    void setCapture(bool isCapture);

    bool isCheck() const;
    void setCheck(bool isCheck);

    bool isCheckMate() const;
    void setCheckMate(bool isCheckMate);

    bool isStaleMate() const;
    void setStaleMate(bool isStaleMate);

    bool isEnPassant() const;
    void setEnPassant(bool isEnPassant);

    bool isCastle() const;
    void setCastle(bool isCastle);

    Chess::Castle castleSide() const;
    void setCastleSide(Chess::Castle castle);

    bool isValid() const;

    void mirror();

    quint32 data() const { return m_data; }

private:
    // Minimize footprint of structure
    // bits 0..5 start square
    // bits 6..11 end square
    // bit 12 is start square valid
    // bit 13 is end quare valid
    // bits 14..16 is piece
    // bits 17..19 promotion type
    // bit 20 is capture flag
    // bit 21 is check flag
    // bit 22 is checkmate flag
    // bit 23 is stalemate flag
    // bit 24 is enpassant flag
    // bit 25 is castle flag
    // bit 26 is castle side
    enum Masks : quint32 {
        StartMask       = 0b00000000000000000000000000111111,
        EndMask         = 0b00000000000000000000111111000000,
        ValidStartMask  = 0b00000000000000000001000000000000,
        ValidEndMask    = 0b00000000000000000010000000000000,
        PieceMask       = 0b00000000000000011100000000000000,
        PromotionMask   = 0b00000000000011100000000000000000,
        CaptureMask     = 0b00000000000100000000000000000000,
        CheckMask       = 0b00000000001000000000000000000000,
        CheckMateMask   = 0b00000000010000000000000000000000,
        StaleMateMask   = 0b00000000100000000000000000000000,
        EnPassantMask   = 0b00000001000000000000000000000000,
        CastleMask      = 0b00000010000000000000000000000000,
        CastleSideMask  = 0b00000100000000000000000000000000,
    };

    quint32 m_data;
};

inline Square Move::start() const
{
    return m_data & StartMask;
}

inline void Move::setStart(const Square &start)
{
    m_data = (m_data & ~StartMask) | quint32(start.data());
    m_data = (m_data & ~ValidStartMask) | quint32(true << 12);
}

inline Square Move::end() const
{
    return (m_data & EndMask) >> 6;
}

inline void Move::setEnd(const Square &end)
{
    m_data = (m_data & ~EndMask) | quint32(end.data() << 6);
    m_data = (m_data & ~ValidEndMask) | quint32(true << 13);
}

inline Chess::PieceType Move::piece() const
{
    return Chess::PieceType((m_data & PieceMask) >> 14);
}

inline void Move::setPiece(Chess::PieceType piece)
{
    m_data = (m_data & ~PieceMask) | quint32(piece << 14);
}

inline Chess::PieceType Move::promotion() const
{
    return Chess::PieceType((m_data & PromotionMask) >> 17);
}

inline void Move::setPromotion(Chess::PieceType promotion)
{
    m_data = (m_data & ~PromotionMask) | quint32(promotion << 17);
}

inline bool Move::isCapture() const
{
    return (m_data & CaptureMask) != 0;
}

inline void Move::setCapture(bool isCapture)
{
    m_data = (m_data & ~CaptureMask) | quint32(isCapture << 20);
}

inline bool Move::isCheck() const
{
    return (m_data & CheckMask) != 0;
}

inline void Move::setCheck(bool isCheck)
{
    m_data = (m_data & ~CheckMask) | quint32(isCheck << 21);
}

inline bool Move::isCheckMate() const
{
    return (m_data & CheckMateMask) != 0;
}

inline void Move::setCheckMate(bool isCheckMate)
{
    m_data = (m_data & ~CheckMateMask) | quint32(isCheckMate << 22);
}

inline bool Move::isStaleMate() const
{
    return (m_data & StaleMateMask) != 0;
}

inline void Move::setStaleMate(bool isStaleMate)
{
    m_data = (m_data & ~StaleMateMask) | quint32(isStaleMate << 23);
}

inline bool Move::isEnPassant() const
{
    return (m_data & EnPassantMask) != 0;
}

inline void Move::setEnPassant(bool isEnPassant)
{
    m_data = (m_data & ~EnPassantMask) | quint32(isEnPassant << 24);
}

inline bool Move::isCastle() const
{
    return (m_data & CastleMask) != 0;
}

inline void Move::setCastle(bool isCastle)
{
    m_data = (m_data & ~CastleMask) | quint32(isCastle << 25);
}

inline Chess::Castle Move::castleSide() const
{
    return Chess::Castle((m_data & CastleSideMask) >> 26);
}

inline void Move::setCastleSide(Chess::Castle castle)
{
    m_data = (m_data & ~CastleSideMask) | quint32(castle << 26);
}

inline bool Move::isValid() const
{
    return (piece() != Chess::Unknown || (m_data & ValidStartMask) != 0) && (m_data & ValidEndMask) != 0;
}

inline void Move::mirror()
{
    m_data ^= 0b111000111000;
}

inline bool operator==(const Move &a, const Move &b) { return a.data() == b.data(); }

typedef QVector<Move> MoveList;

QDebug operator<<(QDebug, const Move &);

#endif
