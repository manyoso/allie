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

class Movegen;
class Node;
class Game {
public:
    class Position {
    public:
        inline Position()
            : m_fileOfKingsRook(0),
            m_fileOfQueensRook(0),
            m_hasWhiteKingCastle(false),
            m_hasBlackKingCastle(false),
            m_hasWhiteQueenCastle(false),
            m_hasBlackQueenCastle(false),
            m_activeArmy(Chess::White)
        {
        }

        inline Position(const Position& other)
            : m_whitePositionBoard(other.m_whitePositionBoard),
              m_blackPositionBoard(other.m_blackPositionBoard),
              m_kingsBoard(other.m_kingsBoard),
              m_queensBoard(other.m_queensBoard),
              m_rooksBoard(other.m_rooksBoard),
              m_bishopsBoard(other.m_bishopsBoard),
              m_knightsBoard(other.m_knightsBoard),
              m_pawnsBoard(other.m_pawnsBoard),
              m_fileOfKingsRook(other.m_fileOfKingsRook),
              m_fileOfQueensRook(other.m_fileOfQueensRook),
              m_enPassantTarget(other.m_enPassantTarget),
              m_hasWhiteKingCastle(other.m_hasWhiteKingCastle),
              m_hasBlackKingCastle(other.m_hasBlackKingCastle),
              m_hasWhiteQueenCastle(other.m_hasWhiteQueenCastle),
              m_hasBlackQueenCastle(other.m_hasBlackQueenCastle),
              m_activeArmy(other.m_activeArmy)
        {
        }

        Chess::Army activeArmy() const { return m_activeArmy; }

        Square enPassantTarget() const { return m_enPassantTarget; }

        int fileOfKingsRook() const { return m_fileOfKingsRook; }
        int fileOfQueensRook() const { return m_fileOfQueensRook; }

        bool hasPieceAt(int index, Chess::Army army) const;

        Chess::PieceType pieceTypeAt(int index) const;
        bool hasPieceTypeAt(int index, Chess::PieceType piece) const;

        QStringList stateOfPositionToFen() const; /* generates the fen of position for our current state */

        BitBoard board(Chess::Army army, Chess::Castle castle, bool kingsSquares = false) const;
        BitBoard board(Chess::Army army) const;
        BitBoard board(Chess::PieceType piece) const;
        BitBoard kingAttackBoard(const Movegen *gen,
            const BitBoard &friends) const;
        BitBoard queenAttackBoard(const Movegen *gen,
            const BitBoard &friends, const BitBoard &enemies) const;
        BitBoard rookAttackBoard(const Movegen *gen,
            const BitBoard &friends, const BitBoard &enemies) const;
        BitBoard bishopAttackBoard(const Movegen *gen,
            const BitBoard &friends, const BitBoard &enemies) const;
        BitBoard knightAttackBoard(const Movegen *gen,
            const BitBoard &friends) const;
        BitBoard pawnAttackBoard(Chess::Army army, const Movegen *gen,
            const BitBoard &friends) const;

        void pseudoLegalMoves(Node *parent) const;
        void generateCastle(Chess::Army army, Chess::Castle castleSide, Node *parent) const;
        void generateMove(Chess::PieceType piece, const Square &start, const Square &end, Node *parent) const;

        bool isCastleLegal(Chess::Army army, Chess::Castle castle) const;
        bool isCastleAvailable(Chess::Army army, Chess::Castle castle) const;

        bool isSamePosition(const Position &other) const;
        bool operator==(const Position &other) const { return isSamePosition(other); }
        bool operator!=(const Position &other) const { return !isSamePosition(other); }

        quint64 positionHash() const;

        int materialScore(Chess::Army army) const;
        bool isDeadPosition() const;
        bool isChecked(Chess::Army army) const;

    private:
        // non-const and will modify in-place
        void setFenOfPosition(const QStringList &fenOfPosition);
        bool makeMove(Move *move);
        void processMove(Chess::Army army, Move *move);

        bool fillOutMove(Chess::Army army, Move *move) const;
        bool fillOutStart(Chess::Army army, Move *move) const;

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
        quint8 m_fileOfKingsRook;
        quint8 m_fileOfQueensRook;
        Square m_enPassantTarget;
        bool m_hasWhiteKingCastle : 1;
        bool m_hasBlackKingCastle : 1;
        bool m_hasWhiteQueenCastle : 1;
        bool m_hasBlackQueenCastle : 1;
        Chess::Army m_activeArmy;
        friend class Game;
        friend class StandaloneGame;
        friend class TB;
    };

    inline Game()
        : m_halfMoveNumber(2),
        m_halfMoveClock(0),
        m_repetitions(-1)
    {
    }

    inline Game(const Game& other)
        : m_lastMove(other.m_lastMove),
         m_halfMoveNumber(other.m_halfMoveNumber),
         m_halfMoveClock(other.m_halfMoveClock),
         m_repetitions(other.m_repetitions)
    {
    }

    ~Game() {}

    int halfMoveClock() const { return m_halfMoveClock; }
    int halfMoveNumber() const { return m_halfMoveNumber; }

    Move lastMove() const { return m_lastMove; }

    /* generates the fen for our current state */
    QString stateOfGameToFen(const Position *position, bool includeMoveNumbers = true) const;

    bool isSameGame(const Game &other) const;
    bool operator==(const Game &other) const { return isSameGame(other); }
    bool operator!=(const Game &other) const { return !isSameGame(other); }

    QString toString(Chess::NotationType type) const;

    int repetitions() const { return m_repetitions; }

    // non-const and will modify in-place
    void storeMove(const Move &move);
    bool makeMove(const Move &move, Position *position);
    void setFen(const QString &fen, Position *position);

    // sets the various flags
    bool isChecked(Chess::Army army, const Position *position);
    void setCheckMate(bool checkMate);
    void setStaleMate(bool staleMate);
    void setRepetitions(int repetitions) { m_repetitions = qint8(repetitions); }

protected:
    Move m_lastMove;
    quint16 m_halfMoveNumber;
    quint8 m_halfMoveClock;
    qint8 m_repetitions;
};

inline BitBoard Game::Position::board(Chess::PieceType piece) const
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

inline BitBoard Game::Position::board(Chess::Army army) const
{
    return army == Chess::White ? m_whitePositionBoard : m_blackPositionBoard;
}

inline bool Game::Position::isCastleAvailable(Chess::Army army, Chess::Castle castle) const
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

inline void Game::Position::togglePieceAt(int index, Chess::Army army, Chess::PieceType piece, bool bit)
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

inline BitBoard *Game::Position::boardPointer(Chess::PieceType piece)
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

class StandaloneGame : public Game {
public:
    inline StandaloneGame()
        : Game()
    {
        static Position *s_startPos = nullptr;
        if (!s_startPos) {
            s_startPos = new Position;
            s_startPos->setFenOfPosition(QString("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1").split(' '));
        }

        m_standalonePosition = *s_startPos;
    }

    inline StandaloneGame(const QString &fen)
        : Game()
    {
        Game::setFen(fen, &m_standalonePosition);
    }

    inline const Position &position() const
    {
        return m_standalonePosition;
    }

    /* generates the fen for our current state */
    inline QString stateOfGameToFen(bool includeMoveNumbers = true) const
    {
        return Game::stateOfGameToFen(&m_standalonePosition, includeMoveNumbers);
    }

    // non-const and will modify in-place
    inline bool makeMove(const Move &move)
    {
        return Game::makeMove(move, &m_standalonePosition);
    }

    inline void setFen(const QString &fen)
    {
        Game::setFen(fen, &m_standalonePosition);
    }

    // sets the various flags
    inline bool isChecked(Chess::Army army)
    {
        return Game::isChecked(army, &m_standalonePosition);
    }

private:
    Position m_standalonePosition;
};

QDebug operator<<(QDebug debug, const Game &g);

#endif // GAME_H
