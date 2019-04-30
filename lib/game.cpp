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

#include "game.h"

#include <QDebug>
#include <QStringList>

#include "bitboard.h"
#include "clock.h"
#include "movegen.h"
#include "node.h"
#include "notation.h"
#include "options.h"
#include "zobrist.h"

using namespace Chess;

static Game s_startPos = Game(QLatin1String("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));

Game::Game(const QString &fen)
    : m_halfMoveClock(0),
    m_halfMoveNumber(2),
    m_fileOfKingsRook(0),
    m_fileOfQueensRook(0),
    m_repetitions(-1),
    m_hasWhiteKingCastle(false),
    m_hasBlackKingCastle(false),
    m_hasWhiteQueenCastle(false),
    m_hasBlackQueenCastle(false),
    m_activeArmy(Chess::White)
{
    if (fen.isEmpty()) {
        *this = s_startPos;
    } else {
        setFen(fen);
    }
}

bool Game::hasPieceAt(int index, Chess::Army army) const
{
    switch (army) {
    case White:
        return m_whitePositionBoard.testBit(index);
    case Black:
        return m_blackPositionBoard.testBit(index);
    }
    Q_UNREACHABLE();
    return false;
}

PieceType Game::pieceTypeAt(int index) const
{
    // From most numerous piece type to least
    Chess::PieceType type = Pawn;
    if (BitBoard(board(type) & m_whitePositionBoard).testBit(index)
        || BitBoard(board(type) & m_blackPositionBoard).testBit(index))
        return type;

    type = Knight;
    if (BitBoard(board(type) & m_whitePositionBoard).testBit(index)
        || BitBoard(board(type) & m_blackPositionBoard).testBit(index))
        return type;

    type = Bishop;
    if (BitBoard(board(type) & m_whitePositionBoard).testBit(index)
        || BitBoard(board(type) & m_blackPositionBoard).testBit(index))
        return type;

    type = Rook;
    if (BitBoard(board(type) & m_whitePositionBoard).testBit(index)
        || BitBoard(board(type) & m_blackPositionBoard).testBit(index))
        return type;

    type = Queen;
    if (BitBoard(board(type) & m_whitePositionBoard).testBit(index)
        || BitBoard(board(type) & m_blackPositionBoard).testBit(index))
        return type;

    type = King;
    if (BitBoard(board(type) & m_whitePositionBoard).testBit(index)
        || BitBoard(board(type) & m_blackPositionBoard).testBit(index))
        return type;

    return Unknown;
}

bool Game::hasPieceTypeAt(int index, Chess::PieceType piece) const
{
    return board(piece).testBit(index);
}

bool Game::makeMove(const Move &move)
{
    Move mv = move;
    bool ok = fillOutMove(activeArmy(), &mv);
    if (!ok) {
        qDebug() << "ERROR! move is malformed";
        return false;
    }
    processMove(activeArmy(), mv);
    return true;
}

void Game::processMove(Chess::Army army, const Move &move)
{
    m_lastMove = move;
    m_enPassantTarget = Square();

    if (army == White) {
        if (move.piece() == King) {
            m_hasWhiteKingCastle = false;
            m_hasWhiteQueenCastle = false;
        } else if (move.piece() == Rook) {
            if (move.start() == Square(m_fileOfQueensRook, 0))
                m_hasWhiteQueenCastle = false;
            else if (move.start() == Square(m_fileOfKingsRook, 0))
                m_hasWhiteKingCastle = false;
        } else if (move.piece() == Pawn && qAbs(move.start().rank() - move.end().rank()) == 2) {
            m_enPassantTarget = Square(move.end().file(), move.end().rank() - 1);
        }

        int start = move.start().data();
        int end = move.end().data();

        bool capture = hasPieceAt(end, Black) || move.isEnPassant();
        if (capture) {
            m_lastMove.setCapture(true); // set the flag now that we know it
            int capturedPieceIndex = end;
            if (move.isEnPassant())
                capturedPieceIndex = Square(move.end().file(), move.end().rank() - 1).data();

            PieceType type = pieceTypeAt(capturedPieceIndex);
            Q_ASSERT(type != Unknown);
            togglePieceAt(capturedPieceIndex, Black, type, false);
            if (type == Rook) {
                if (move.end().file() == m_fileOfKingsRook)
                    m_hasBlackKingCastle = false;
                else
                    m_hasBlackQueenCastle = false;
            }
        }

        if (move.piece() != Pawn && !capture)
            ++m_halfMoveClock;
        else
            m_halfMoveClock = 0;

        if (move.isCastle()) { //have to move the rook
            if (move.castleSide() == KingSide) {
                Square rook(m_fileOfKingsRook, 0);
                togglePieceAt(BitBoard::squareToIndex(rook), White, Rook, false);
                rook = Square(5, 0); //f1
                togglePieceAt(BitBoard::squareToIndex(rook), White, Rook, true);
            } else if (move.castleSide() == QueenSide) {
                Square rook(m_fileOfQueensRook, 0);
                togglePieceAt(BitBoard::squareToIndex(rook), White, Rook, false);
                rook = Square(3, 0); //d1
                togglePieceAt(BitBoard::squareToIndex(rook), White, Rook, true);
            }
        }

        togglePieceAt(start, White, move.piece(), false);
        if (move.promotion() != Unknown)
            togglePieceAt(end, White, move.promotion(), true);
        else
            togglePieceAt(end, White, move.piece(), true);
    } else if (army == Black) {
        if (move.piece() == King) {
            m_hasBlackKingCastle = false;
            m_hasBlackQueenCastle = false;
        } else if (move.piece() == Rook) {
            if (move.start() == Square(m_fileOfQueensRook, 7))
                m_hasBlackQueenCastle = false;
            else if (move.start() == Square(m_fileOfKingsRook, 7))
                m_hasBlackKingCastle = false;
        } else if (move.piece() == Pawn && qAbs(move.start().rank() - move.end().rank()) == 2) {
            m_enPassantTarget = Square(move.end().file(), move.end().rank() + 1);
        }

        int start = move.start().data();
        int end = move.end().data();

        bool capture = hasPieceAt(end, White) || move.isEnPassant();
        if (capture) {
            m_lastMove.setCapture(true); // set the flag now that we know it
            int capturedPieceIndex = end;
            if (move.isEnPassant())
                capturedPieceIndex = Square(move.end().file(), move.end().rank() + 1).data();

            PieceType type = pieceTypeAt(capturedPieceIndex);
            Q_ASSERT(type != Unknown);
            togglePieceAt(capturedPieceIndex, White, type, false);
            if (type == Rook) {
                if (move.end().file() == m_fileOfKingsRook)
                    m_hasWhiteKingCastle = false;
                else
                    m_hasWhiteQueenCastle = false;
            }
        }

        if (move.piece() != Pawn && !capture)
            ++m_halfMoveClock;
        else
            m_halfMoveClock = 0;

        if (move.isCastle()) { //have to move the rook
            if (move.castleSide() == KingSide) {
                Square rook(m_fileOfKingsRook, 7);
                togglePieceAt(BitBoard::squareToIndex(rook), Black, Rook, false);
                rook = Square(5, 7); //f8
                togglePieceAt(BitBoard::squareToIndex(rook), Black, Rook, true);
            } else if (move.castleSide() == QueenSide) {
                Square rook(m_fileOfQueensRook, 7);
                togglePieceAt(BitBoard::squareToIndex(rook), Black, Rook, false);
                rook = Square(3, 7); //d8
                togglePieceAt(BitBoard::squareToIndex(rook), Black, Rook, true);
            }
        }

        togglePieceAt(start, Black, move.piece(), false);
        if (move.promotion() != Unknown)
            togglePieceAt(end, Black, move.promotion(), true);
        else
            togglePieceAt(end, Black, move.piece(), true);
    }

    m_repetitions = -1;
    m_halfMoveNumber++;
    m_activeArmy = m_activeArmy == White ? Black : White;
}

bool Game::fillOutMove(Chess::Army army, Move *move) const
{
    if (move->isCastle() && !move->isValid()) {
        if (move->castleSide() == KingSide)
            move->setEnd(Square(6, army == White ? 0 : 7));
        else if (move->castleSide() == QueenSide)
            move->setEnd(Square(2, army == White ? 0 : 7));
    }

    if (!move->isValid()) {
        qDebug() << "invalid move...";
        return false; //not enough info to do anything
    }

    if (move->piece() == Unknown) {
        int start = move->start().data();
        move->setPiece(pieceTypeAt(start));
    }

    if (!move->start().isValid()) {
        bool ok = fillOutStart(army, move);
        if (!ok)
            return false;
    }

    if (move->piece() == Pawn && move->promotion() == Unknown &&
        ((army == White && move->end().rank() == 7) ||
         (army == Black && move->end().rank() == 0))) {
        QString promotion = "Queen"; // FIXME
        if (promotion == "Queen")
            move->setPromotion(Queen);
        else if (promotion == "Rook")
            move->setPromotion(Rook);
        else if (promotion == "Bishop")
            move->setPromotion(Bishop);
        else if (promotion == "Knight")
            move->setPromotion(Knight);
        else
            move->setPromotion(Queen);
    }

    if (move->piece() == Pawn && move->end() == m_enPassantTarget)
        move->setEnPassant(true);

    if (move->piece() == King) {
        //FIXME ...this is different for Chess960...
        if ((move->start().rank() == 0 || move->start().rank() == 7) && move->start().file() == 4) {
            if (move->end().file() == 6) {
                move->setCastle(true);
                move->setCastleSide(KingSide);
            } else if (move->end().file() == 2) {
                move->setCastle(true);
                move->setCastleSide(QueenSide);
            }
        }
    }

    return true;
}

bool Game::fillOutStart(Chess::Army army, Move *move) const
{
    if (!move->isValid()) {
        qDebug() << "invalid move...";
        return false; //not enough info to do anything
    }

    Square square = move->start();
    if (square.isValid())
        return true;

    BitBoard positions(board(move->piece()) & board(army));
    BitBoard opposingPositions(board(army == White ? Black : White));

    qDebug() << "could not find a valid start square..."
             << "\narmy:" << (army == White ? "White" : "Black")
             << "\nmove:" << Notation::moveToString(*move)
             << "\npositions:" << positions
             << "\nopposingPositions:" << opposingPositions;

    return false;
}

void Game::setFen(const QString &fen)
{
    m_activeArmy = White;

    m_halfMoveClock = 0;
    m_halfMoveNumber = 2; // fen assumes fullmovenumber starts with 1
    m_fileOfKingsRook = 0;
    m_fileOfQueensRook = 0;

    m_enPassantTarget = Square();

    m_whitePositionBoard = BitBoard();
    m_blackPositionBoard = BitBoard();
    m_kingsBoard = BitBoard();
    m_queensBoard = BitBoard();
    m_rooksBoard = BitBoard();
    m_bishopsBoard = BitBoard();
    m_knightsBoard = BitBoard();
    m_pawnsBoard = BitBoard();

    QStringList list = fen.split(' ');
    Q_ASSERT(list.count() == 6);

    QStringList ranks = list.at(0).split('/');
    Q_ASSERT(ranks.count() == 8);

    for (int i = 0; i < ranks.size(); ++i) {
        QString rank = ranks.at(i);
        int blank = 0;
        for (int j = 0; j < rank.size(); ++j) {
            QChar c = rank.at(j);
            if (c.isLetter() && c.isUpper()) /*white*/ {
                PieceType piece = Notation::charToPiece(c);
                Square square = Square(j + blank, 7 - i);
                togglePieceAt(square.data(), White, piece, true);
            } else if (c.isLetter() && c.isLower()) /*black*/ {
                PieceType piece = Notation::charToPiece(c.toUpper());
                Square square = Square(j + blank, 7 - i);
                togglePieceAt(square.data(), Black, piece, true);
            } else if (c.isNumber()) /*blank*/ {
                blank += QString(c).toInt() - 1;
            }
        }
    }

    m_activeArmy = list.at(1) == QLatin1String("w") ? White : Black;

    const bool isChess960 = Options::globalInstance()->option("UCI_Chess960").value() == QLatin1String("true");

    //Should work for regular fen and UCI fen for chess960...
    QString castling = list.at(2);
    if (castling != "-") {
        QVector<QChar> whiteKingSide;
        whiteKingSide << 'K';
        if (isChess960) whiteKingSide << 'E' << 'F' << 'G' << 'H';
        m_hasWhiteKingCastle = false;
        for (QChar c : whiteKingSide) {
            if (castling.contains(c)) {
                m_hasWhiteKingCastle = true;
                m_fileOfKingsRook = isChess960 ? quint8(whiteKingSide.indexOf(c)) + 3 : 7;
            }
        }

        QVector<QChar> whiteQueenSide;
        whiteQueenSide << 'Q';
        if (isChess960) whiteQueenSide << 'A' << 'B' << 'C' << 'D';
        m_hasWhiteQueenCastle = false;
        for (QChar c : whiteQueenSide) {
            if (castling.contains(c)) {
                m_hasWhiteQueenCastle = true;
                m_fileOfQueensRook = isChess960 ? quint8(whiteQueenSide.indexOf(c) - 1) : 0;
            }
        }

        QVector<QChar> blackKingSide;
        blackKingSide << 'k';
        if (isChess960) blackKingSide << 'e' << 'f' << 'g' << 'h';
        m_hasBlackKingCastle = false;
        for (QChar c : blackKingSide) {
            if (castling.contains(c)) {
                m_hasBlackKingCastle = true;
                m_fileOfKingsRook = isChess960 ? quint8(blackKingSide.indexOf(c)) + 3 : 7;
            }
        }

        QVector<QChar> blackQueenSide;
        blackQueenSide << 'q';
        if (isChess960) blackQueenSide << 'a' << 'b' << 'c' << 'd';
        m_hasBlackQueenCastle = false;
        for (QChar c : blackQueenSide) {
            if (castling.contains(c)) {
                m_hasBlackQueenCastle = true;
                m_fileOfQueensRook = isChess960 ? quint8(blackQueenSide.indexOf(c) - 1) : 0;
            }
        }
    }

    QString enPassant = list.at(3);
    if (enPassant != QLatin1String("-"))
        m_enPassantTarget = Notation::stringToSquare(enPassant);

    m_halfMoveClock = quint16(list.at(4).toInt());
    m_halfMoveNumber = quint16(qCeil(list.at(5).toInt() * 2.0));
}

QString Game::stateOfGameToFen(bool includeMoveNumbers) const
{

    QStringList rankList;
    for (qint8 i = 0; i < 8; ++i) { //rank
        QString rank;
        int blank = 0;
        for (qint8 j = 0; j < 8; ++j) { //file
            Square square(j, 7 - i);
            int index = square.data();
            if (hasPieceAt(index, White)) {
                PieceType piece = pieceTypeAt(index);
                QChar ch = Notation::pieceToChar(piece);
                if (blank > 0) {
                    rank += QString::number(blank);
                    blank = 0;
                }
                if (!ch.isNull()) {
                    rank += ch.toUpper();
                } else {
                    rank += 'P';
                }
            } else if (hasPieceAt(index, Black)) {
                PieceType piece = pieceTypeAt(index);
                QChar ch = Notation::pieceToChar(piece);
                if (blank > 0) {
                    rank += QString::number(blank);
                    blank = 0;
                }
                if (!ch.isNull()) {
                    rank += ch.toLower();
                } else {
                    rank += 'p';
                }
            } else {
                blank++;
            }
        }

        if (blank > 0) {
            rank += QString::number(blank);
            blank = 0;
        }

        rankList << rank;
    }

    QString ranks = rankList.join("/");
    QString activeArmy = (m_activeArmy == White ? QLatin1String("w") : QLatin1String("b"));

    const bool isChess960 = Options::globalInstance()->option("UCI_Chess960").value() == QLatin1String("true");
    QString castling;
    if (isCastleAvailable(White, KingSide))
        isChess960 ? castling.append(Notation::fileToChar(m_fileOfKingsRook).toUpper()) : castling.append("K");
    if (isCastleAvailable(White, QueenSide))
        isChess960 ? castling.append(Notation::fileToChar(m_fileOfQueensRook).toUpper()) : castling.append("Q");
    if (isCastleAvailable(Black, KingSide))
        isChess960 ? castling.append(Notation::fileToChar(m_fileOfKingsRook)) : castling.append("k");
    if (isCastleAvailable(Black, QueenSide))
        isChess960 ? castling.append(Notation::fileToChar(m_fileOfQueensRook)) : castling.append("q");
    if (castling.isEmpty())
        castling.append("-");

    QString enPassant = enPassantTarget().isValid() ? Notation::squareToString(enPassantTarget()) : QLatin1String("-");

    QStringList fen;
    fen << ranks << activeArmy << castling << enPassant;
    if (includeMoveNumbers)
        fen << QString::number(halfMoveClock()) << QString::number(qCeil(halfMoveNumber() / 2.0));

    return fen.join(" ");
}

BitBoard Game::board(Chess::Army army, Chess::Castle castle, bool kingsSquares) const
{
    // This should only be called if it is available otherwise the boards will be out of date
    // as we don't update them when castling is not available
    Q_ASSERT(isCastleAvailable(army, castle));

    static BitBoard firstRank;
    static BitBoard eighthRank;
    static bool init = false;
    if (!init) {
        SquareList first;
        first << Square(0, 0) << Square(1, 0) << Square(2, 0) << Square(3, 0) << Square(4, 0) << Square(5, 0) << Square(6, 0) << Square(7, 0);
        firstRank.setBoard(first);

        SquareList eight;
        eight << Square(0, 7) << Square(1, 7) << Square(2, 7) << Square(3, 7) << Square(4, 7) << Square(5, 7) << Square(6, 7) << Square(7, 7);
        eighthRank.setBoard(eight);

        init = true;
    }

    return castleBoard(army, castle, kingsSquares) & BitBoard(army == White ? firstRank : eighthRank);
}

BitBoard Game::kingAttackBoard(Chess::Army army, const Movegen *gen) const
{
    BitBoard bits;
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard pieces(friends & board(King));
    BitBoard::Iterator sq = pieces.begin();
    for (int i = 0; sq != pieces.end(); ++sq, ++i) {
        Q_ASSERT(i < 1);
        bits = bits | gen->kingMoves(*sq, friends, enemies);
    }
    return bits;
}

BitBoard Game::queenAttackBoard(Chess::Army army, const Movegen *gen) const
{
    BitBoard bits;
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard pieces(friends & board(Queen));
    BitBoard::Iterator sq = pieces.begin();
    for (; sq != pieces.end(); ++sq)
        bits = bits | gen->queenMoves(*sq, friends, enemies);
    return bits;
}

BitBoard Game::rookAttackBoard(Chess::Army army, const Movegen *gen) const
{
    BitBoard bits;
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard pieces(friends & board(Rook));
    BitBoard::Iterator sq = pieces.begin();
    for (; sq != pieces.end(); ++sq)
        bits = bits | gen->rookMoves(*sq, friends, enemies);
    return bits;
}

BitBoard Game::bishopAttackBoard(Chess::Army army, const Movegen *gen) const
{
    BitBoard bits;
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
            const BitBoard pieces(friends & board(Bishop));
            BitBoard::Iterator sq = pieces.begin();
            for (; sq != pieces.end(); ++sq)
                bits = bits | gen->bishopMoves(*sq, friends, enemies);
            return bits;
}

BitBoard Game::knightAttackBoard(Chess::Army army, const Movegen *gen) const
{
    BitBoard bits;
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard pieces(friends & board(Knight));
    BitBoard::Iterator sq = pieces.begin();
    for (; sq != pieces.end(); ++sq)
        bits = bits | gen->knightMoves(*sq, friends, enemies);
    return bits;
}

BitBoard Game::pawnAttackBoard(Chess::Army army, const Movegen *gen) const
{
    BitBoard bits;
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard pieces(friends & board(Pawn));
    BitBoard::Iterator sq = pieces.begin();
    BitBoard enemiesPlusEnpassant = enemies;
    if (m_enPassantTarget.isValid())
        enemiesPlusEnpassant.setSquare(m_enPassantTarget);
    for (; sq != pieces.end(); ++sq)
        bits = bits | gen->pawnAttacks(army, *sq, friends, enemiesPlusEnpassant);
    return bits;
}

void Game::pseudoLegalMoves(Node *parent) const
{
    const Chess::Army army = activeArmy();
    const BitBoard friends = army == White ? m_whitePositionBoard : m_blackPositionBoard;
    const BitBoard enemies = army == Black ? m_whitePositionBoard : m_blackPositionBoard;
    const Movegen *gen = Movegen::globalInstance();

    {
        const BitBoard pieces(friends & board(King));
        BitBoard::Iterator sq = pieces.begin();
        for (int i = 0; sq != pieces.end(); ++sq, ++i) {
            Q_ASSERT(i < 1);
            const BitBoard moves = gen->kingMoves(*sq, friends, enemies);
            BitBoard::Iterator newSq = moves.begin();
            for (; newSq != moves.end(); ++newSq)
                generateMove(King, *sq, *newSq, parent);
        }
    }

    {
        const BitBoard pieces(friends & board(Queen));
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            const BitBoard moves = gen->queenMoves(*sq, friends, enemies);
            BitBoard::Iterator newSq = moves.begin();
            for (; newSq != moves.end(); ++newSq)
                generateMove(Queen, *sq, *newSq, parent);
        }
    }

    {
        const BitBoard pieces(friends & board(Rook));
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            const BitBoard moves = gen->rookMoves(*sq, friends, enemies);
            BitBoard::Iterator newSq = moves.begin();
            for (; newSq != moves.end(); ++newSq)
                generateMove(Rook, *sq, *newSq, parent);
        }
    }

    {
        const BitBoard pieces(friends & board(Bishop));
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            const BitBoard moves = gen->bishopMoves(*sq, friends, enemies);
            BitBoard::Iterator newSq = moves.begin();
            for (; newSq != moves.end(); ++newSq)
                generateMove(Bishop, *sq, *newSq, parent);
        }
    }

    {
        const BitBoard pieces(friends & board(Knight));
        BitBoard::Iterator sq = pieces.begin();
        for (; sq != pieces.end(); ++sq) {
            const BitBoard moves = gen->knightMoves(*sq, friends, enemies);
            BitBoard::Iterator newSq = moves.begin();
            for (; newSq != moves.end(); ++newSq)
                generateMove(Knight, *sq, *newSq, parent);
        }
    }

    {
        const BitBoard pieces(friends & board(Pawn));
        BitBoard::Iterator sq = pieces.begin();
        BitBoard enemiesPlusEnpassant = enemies;
        if (m_enPassantTarget.isValid())
            enemiesPlusEnpassant.setSquare(m_enPassantTarget);
        for (; sq != pieces.end(); ++sq) {
            {
                const BitBoard moves = gen->pawnMoves(army, *sq, friends, enemies);
                BitBoard::Iterator newSq = moves.begin();
                for (; newSq != moves.end(); ++newSq) {
                    bool forwardTwo = qAbs((*newSq).rank() - (*sq).rank()) > 1;
                    Square forwardOne = Square((*newSq).file(), army == White ? (*newSq).rank() - 1 : (*newSq).rank() + 1);
                    if (forwardTwo && BitBoard(friends | enemies).testBit(forwardOne.data()))
                        continue; // can't move through another piece
                    generateMove(Pawn, *sq, *newSq, parent);
                }
            }
            {
                const BitBoard moves = gen->pawnAttacks(army, *sq, friends, enemiesPlusEnpassant);
                BitBoard::Iterator newSq = moves.begin();
                for (; newSq != moves.end(); ++newSq)
                    generateMove(Pawn, *sq, *newSq, parent);
            }
        }
    }

    // Add castle moves
    if (isCastleLegal(army, KingSide))
        generateCastle(army, KingSide, parent);
    if (isCastleLegal(army, QueenSide))
        generateCastle(army, QueenSide, parent);
}

void Game::generateCastle(Chess::Army army, Chess::Castle castleSide, Node *parent) const
{
    Move mv;
    mv.setPiece(King);
    mv.setStart(BitBoard(board(King) & board(army)).occupiedSquares().first());

    if (castleSide == KingSide)
        mv.setEnd(army == White ? Square(6, 0) /*g1*/ : Square(6, 7) /*g8*/);
    else
        mv.setEnd(army == White ? Square(2, 0) /*c1*/ : Square(2, 7) /*c8*/);

    mv.setCastle(true);
    mv.setCastleSide(castleSide);
    Q_ASSERT(parent);
    parent->generatePotential(mv);
}

void Game::generateMove(Chess::PieceType piece, const Square &start, const Square &end, Node *parent) const
{
    const Chess::Army army = activeArmy();
    const bool isPromotion = piece == Pawn && (army == White ? end.rank() == 7 : end.rank() == 0);
    const bool isCapture = board(army == White ? Black : White).isSquareOccupied(end);

    Move mv;
    mv.setPiece(piece);
    mv.setStart(start);
    mv.setEnd(end);
    mv.setCapture(isCapture);
    Q_ASSERT(parent);
    if (!isPromotion) {
        parent->generatePotential(mv);
    } else {
        mv.setPromotion(Queen);
        parent->generatePotential(mv);
        mv.setPromotion(Knight);
        parent->generatePotential(mv);
        mv.setPromotion(Rook);
        parent->generatePotential(mv);
        mv.setPromotion(Bishop);
        parent->generatePotential(mv);
    }
}

bool Game::isChecked(Chess::Army army)
{
    const Chess::Army friends = army == White ? White : Black;
    const Chess::Army enemies = army == Black ? White : Black;
    const BitBoard kingBoard(board(friends) & board(King));
    const Movegen *gen = Movegen::globalInstance();
    {
        const BitBoard b(kingBoard & queenAttackBoard(enemies, gen));
        if (!b.isClear()) {
            m_lastMove.setCheck(true);
            return true;
        }
    }
    {
        const BitBoard b(kingBoard & rookAttackBoard(enemies, gen));
        if (!b.isClear()) {
            m_lastMove.setCheck(true);
            return true;
        }
    }
    {
        const BitBoard b(kingBoard & bishopAttackBoard(enemies, gen));
        if (!b.isClear()) {
            m_lastMove.setCheck(true);
            return true;
        }
    }
    {
        const BitBoard b(kingBoard & knightAttackBoard(enemies, gen));
        if (!b.isClear()) {
            m_lastMove.setCheck(true);
            return true;
        }
    }
    {
        // Checks for illegality...
        const BitBoard b(kingBoard & kingAttackBoard(enemies, gen));
        if (!b.isClear()) {
            m_lastMove.setCheck(true);
            return true;
        }
    }
    {
        const BitBoard b(kingBoard & pawnAttackBoard(enemies, gen));
        if (!b.isClear()) {
            m_lastMove.setCheck(true);
            return true;
        }
    }
    m_lastMove.setCheck(false);
    return false;
}

void Game::setCheckMate(bool checkMate)
{
    m_lastMove.setCheckMate(checkMate);
}

void Game::setStaleMate(bool staleMate)
{
    m_lastMove.setStaleMate(staleMate);
}

bool Game::isCastleLegal(Chess::Army army, Chess::Castle castle) const
{
    //Check if castle is available... ie, if neither king nor rook(s) have moved...
    if (!isCastleAvailable(army, castle)) {
        //qDebug() << "castle is unavailable!";
        return false;
    }

    // The board representing the squares involved in castling for given army
    BitBoard castleBoard = board(army, castle);

    // The board of kings position
    BitBoard kingBoard(board(King) & board(army));

    // The board of rooks position in castle under question
    BitBoard rookBoard(Square(castle == KingSide ? fileOfKingsRook() : fileOfQueensRook(), army == White ? 0 : 7));

    // The position of the king and rook under question regardless if they are in right position
    // for castling
    BitBoard piecesBoard(kingBoard | rookBoard);

    // The castleBoard minus the positions of king and rook under question. This board represents
    // the squares in between the king and rook. We know at this point that the king and rook have
    // not moved.
    BitBoard castleBoardMinusPieces(castleBoard ^ piecesBoard);

    //Check if all squares between king and rook(s) are unoccupied by anything other than king or rook...
    if (!BitBoard(castleBoardMinusPieces & BitBoard(board(White) | board(Black))).isClear()) {
        //qDebug() << "castle is impeded by occupied square!";
        return false;
    }

    // The board representing the squares involved in castling for given army
    castleBoard = board(army, castle, true /*kingsSquares*/);

    const Movegen *gen = Movegen::globalInstance();
    const Chess::Army attackArmy = army == White ? Black : White;
    const BitBoard atb = kingAttackBoard(attackArmy, gen) |
        queenAttackBoard(attackArmy, gen) |
        rookAttackBoard(attackArmy, gen) |
        bishopAttackBoard(attackArmy, gen) |
        knightAttackBoard(attackArmy, gen) |
        pawnAttackBoard(attackArmy, gen);

    //Check if any squares between king and kings castle position are under attack...
    if (!BitBoard(castleBoard & atb).isClear()) {
        //qDebug() << "king can't move through check!";
        return false;
    }

    return true;
}

BitBoard Game::castleBoard(Chess::Army army, Chess::Castle castle, bool kingSquares) const
{
    if (!isCastleAvailable(army, castle))
        return BitBoard();

    SquareList kings = BitBoard(board(King) & board(army)).occupiedSquares();
    Square king = !kings.isEmpty() ? kings.first() : Square();
    Q_ASSERT(king.isValid());

    BitBoard cb;
    int rank = army == White ? 0 : 7;

    if (!kingSquares) {
        int rook = castle == KingSide ? fileOfKingsRook() : fileOfQueensRook();
        for (int i= 0; i < 8; ++i) {
            if (king.file() < rook) {
                if (i >= king.file() && i <= rook) {
                    cb = cb | Square(i, rank);
                }
            } else {
                if (i >= rook && i <= king.file()) {
                    cb = cb | Square(i, rank);
                }
            }
        }
        return cb;
    }

    // Remove all squares except the ones between king position and file he ends up on
    int kingsSpot = castle == KingSide ? 6 : 2; // file king ends up on
    for (int i= 0; i < 8; ++i) {
        if (king.file() < kingsSpot) {
            if (i >= king.file() && i <= kingsSpot) {
                cb = cb | Square(i, rank);
            }
        } else {
            if (i >= kingsSpot && i <= king.file()) {
                cb = cb | Square(i, rank);
            }
        }
    }
    return cb;
}

bool Game::isSameGame(const Game &other) const
{
    return isSamePosition(other)
        && m_halfMoveClock == other.m_halfMoveClock
        && m_halfMoveNumber == other.m_halfMoveNumber
        && m_lastMove == other.m_lastMove;
}

bool Game::isSamePosition(const Game &other) const
{
    // FIXME: For purposes of 3-fold it does not matter if the queens rook and kings rook have
    // swapped places, but it does matter for purposes of hash
    return m_activeArmy == other.m_activeArmy
        && m_fileOfKingsRook == other.m_fileOfKingsRook
        && m_fileOfQueensRook == other.m_fileOfQueensRook
        && m_enPassantTarget == other.m_enPassantTarget
        && m_whitePositionBoard == other.m_whitePositionBoard
        && m_blackPositionBoard == other.m_blackPositionBoard
        && m_kingsBoard == other.m_kingsBoard
        && m_queensBoard == other.m_queensBoard
        && m_rooksBoard == other.m_rooksBoard
        && m_bishopsBoard == other.m_bishopsBoard
        && m_knightsBoard == other.m_knightsBoard
        && m_pawnsBoard == other.m_pawnsBoard
        && m_hasWhiteKingCastle == other.m_hasWhiteKingCastle
        && m_hasBlackKingCastle == other.m_hasBlackKingCastle
        && m_hasWhiteQueenCastle == other.m_hasWhiteQueenCastle
        && m_hasBlackQueenCastle ==  other.m_hasBlackQueenCastle;
}

quint64 Game::hash() const
{
    return Zobrist::globalInstance()->hash(*this);
}

int Game::materialScore(Chess::Army army) const
{
    int score = 0;

    {
        BitBoard pieces(board(army) & board(Queen));
        score += pieces.count() * 9;
    }

    {
        BitBoard pieces(board(army) & board(Rook));
        score += pieces.count() * 5;
    }

    {
        BitBoard pieces(board(army) & board(Bishop));
        score += pieces.count() * 3;
    }

    {
        BitBoard pieces(board(army) & board(Knight));
        score += pieces.count() * 3;
    }

    {
        BitBoard pieces(board(army) & board(Pawn));
        score += pieces.count() * 1;
    }

    return score;
}

bool Game::isDeadPosition() const
{
    // If queens, rooks, or pawns are on the board, then we are good
    if (!board(Queen).isClear())
        return false;

    if (!board(Rook).isClear())
        return false;

    if (!board(Pawn).isClear())
        return false;

    // If game has four or more pieces, then usually someone can still mate although it might not be
    // forcing or if bishops are opposite then it is dead (FIXME)
    if (BitBoard(board(White) | board(Black)).count() > 3)
        return false;

    // If only 3 pieces remain with none of the above, then no one can mate
    // ie, it has to be either KBvK, or KNvK, KvK
    return true;
}

QString Game::toString(NotationType type) const
{
    if (lastMove().isValid())
        return Notation::moveToString(lastMove(), type);
    else
        return "start";
}

QDebug operator<<(QDebug debug, const Game &g)
{
    debug << g.toString(Chess::Standard);
    return debug.nospace();
}
