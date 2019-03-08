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

#ifndef GAME_H
#define GAME_H

#include "bitboard.h"
#include "chess.h"
#include "move.h"
#include "piece.h"
#include "square.h"

class Node;
class Game {
public:
    Game(const QString &fen = QString());

    Game(const Game& other)
        : m_whitePositionBoard(other.m_whitePositionBoard),
          m_blackPositionBoard(other.m_blackPositionBoard),
          m_kingsBoard(other.m_kingsBoard),
          m_queensBoard(other.m_queensBoard),
          m_rooksBoard(other.m_rooksBoard),
          m_bishopsBoard(other.m_bishopsBoard),
          m_knightsBoard(other.m_knightsBoard),
          m_pawnsBoard(other.m_pawnsBoard),
          m_lastMove(other.m_lastMove),
          m_halfMoveClock(other.m_halfMoveClock),
          m_halfMoveNumber(other.m_halfMoveNumber),
          m_fileOfKingsRook(other.m_fileOfKingsRook),
          m_fileOfQueensRook(other.m_fileOfQueensRook),
          m_repetitions(other.m_repetitions),
          m_enPassantTarget(other.m_enPassantTarget),
          m_hasWhiteKingCastle(other.m_hasWhiteKingCastle),
          m_hasBlackKingCastle(other.m_hasBlackKingCastle),
          m_hasWhiteQueenCastle(other.m_hasWhiteQueenCastle),
          m_hasBlackQueenCastle(other.m_hasBlackQueenCastle),
          m_activeArmy(other.m_activeArmy)
    {
    }

    ~Game() {}

    Chess::Army activeArmy() const { return m_activeArmy; }

    int halfMoveClock() const { return m_halfMoveClock; }
    int halfMoveNumber() const { return m_halfMoveNumber; }

    Square enPassantTarget() const { return m_enPassantTarget; }

    int fileOfKingsRook() const { return m_fileOfKingsRook; }
    int fileOfQueensRook() const { return m_fileOfQueensRook; }

    Move lastMove() const { return m_lastMove; }

    bool hasPieceAt(int index, Chess::Army army) const;

    Chess::PieceType pieceTypeAt(int index) const;
    bool hasPieceTypeAt(int index, Chess::PieceType piece) const;

    QString stateOfGameToFen(bool includeMoveNumbers = true) const; /* generates the fen for our current state */

    BitBoard board(Chess::Army army, Chess::Castle castle, bool kingsSquares = false) const;
    BitBoard board(Chess::Army army) const;
    BitBoard attackBoard(Chess::Army army) const;
    BitBoard attackBoard(Chess::PieceType piece, Chess::Army army) const;
    BitBoard board(Chess::PieceType piece) const;

    void pseudoLegalMoves(Node *parent) const;
    void generateCastle(Chess::Army army, Chess::Castle castleSide, Node *parent) const;
    void generateMove(Chess::PieceType piece, const Square &start, const Square &end, Node *parent) const;

    bool isCastleLegal(Chess::Army army, Chess::Castle castle) const;
    bool isCastleAvailable(Chess::Army army, Chess::Castle castle) const;

    bool isSameGame(const Game &other) const;
    bool isSamePosition(const Game &other) const;
    bool operator==(const Game &other) const { return isSamePosition(other); }

    quint64 hash() const;

    int materialScore(Chess::Army army) const;
    bool isDeadPosition() const;

    QString toString(Chess::NotationType type) const;

    int repetitions() const { return m_repetitions; }

    // non-const and will modify in-place
    void setFen(const QString &fen);
    bool makeMove(const Move &move);
    bool isChecked(Chess::Army army); // sets the checked flag if we are in check
    void setCheckMate(bool checkMate);
    void setStaleMate(bool staleMate);
    void setRepetitions(int repetitions) { m_repetitions = qint8(repetitions); }

private:
    // non-const and will modify in-place
    void processMove(Chess::Army army, const Move &move);

    bool fillOutMove(Chess::Army army, Move *move) const;
    bool fillOutStart(Chess::Army army, Move *move) const;

    BitBoard castleBoard(Chess::Army army, Chess::Castle castle, bool kingSquares) const;

    void togglePieceAt(int index, Chess::Army army, Chess::PieceType piece, bool bit);
    BitBoard *boardPointer(Chess::PieceType piece);

private:
    BitBoard m_whitePositionBoard;
    BitBoard m_blackPositionBoard;
    BitBoard m_kingsBoard;
    BitBoard m_queensBoard;
    BitBoard m_rooksBoard;
    BitBoard m_bishopsBoard;
    BitBoard m_knightsBoard;
    BitBoard m_pawnsBoard;
    Move m_lastMove;
    quint16 m_halfMoveClock;
    quint16 m_halfMoveNumber;
    quint8 m_fileOfKingsRook;
    quint8 m_fileOfQueensRook;
    qint8 m_repetitions;
    Square m_enPassantTarget;
    bool m_hasWhiteKingCastle : 1;
    bool m_hasBlackKingCastle : 1;
    bool m_hasWhiteQueenCastle : 1;
    bool m_hasBlackQueenCastle : 1;
    Chess::Army m_activeArmy;
    friend class TB;
};

inline BitBoard Game::board(Chess::PieceType piece) const
{
    switch (piece) {
        case Chess::King: return m_kingsBoard;
        case Chess::Queen: return m_queensBoard;
        case Chess::Rook: return m_rooksBoard;
        case Chess::Bishop: return m_bishopsBoard;
        case Chess::Knight: return m_knightsBoard;
        case Chess::Pawn: return m_pawnsBoard;
        case Chess::Unknown:
            Q_UNREACHABLE();
    };
    return BitBoard();
}

inline BitBoard Game::board(Chess::Army army) const
{
    return army == Chess::White ? m_whitePositionBoard : m_blackPositionBoard;
}

inline BitBoard Game::attackBoard(Chess::Army army) const
{
    // FIXME Should consider wrapping this in
    return attackBoard(Chess::King, army) |
        attackBoard(Chess::Queen, army) |
        attackBoard(Chess::Rook, army) |
        attackBoard(Chess::Bishop, army) |
        attackBoard(Chess::Knight, army) |
        attackBoard(Chess::Pawn, army);
}

inline bool Game::isCastleAvailable(Chess::Army army, Chess::Castle castle) const
{
    if (army == Chess::White && castle == Chess::KingSide) {
        if (!m_hasWhiteKingCastle) {
            return false;
        }
    } else if (army == Chess::Black && castle == Chess::KingSide) {
        if (!m_hasBlackKingCastle) {
            return false;
        }
    } else if (army == Chess::White && castle == Chess::QueenSide) {
        if (!m_hasWhiteQueenCastle) {
            return false;
        }
    } else if (army == Chess::Black && castle == Chess::QueenSide) {
        if (!m_hasBlackQueenCastle) {
            return false;
        }
    }

    return true;
}

inline void Game::togglePieceAt(int index, Chess::Army army, Chess::PieceType piece, bool bit)
{
    boardPointer(piece)->setBit(index, bit);
    switch (army) {
    case Chess::White:
        m_whitePositionBoard.setBit(index, bit);
        break;
    case Chess::Black:
        m_blackPositionBoard.setBit(index, bit);
        break;
    }
}

inline BitBoard *Game::boardPointer(Chess::PieceType piece)
{
    switch (piece) {
        case Chess::King: return &m_kingsBoard;
        case Chess::Queen: return &m_queensBoard;
        case Chess::Rook: return &m_rooksBoard;
        case Chess::Bishop: return &m_bishopsBoard;
        case Chess::Knight: return &m_knightsBoard;
        case Chess::Pawn: return &m_pawnsBoard;
        case Chess::Unknown:
            Q_UNREACHABLE();
    };
    return nullptr;
}

QDebug operator<<(QDebug debug, const Game &g);

#endif
