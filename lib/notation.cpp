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

#include "notation.h"

#include "move.h"
#include "options.h"
#include "square.h"

#include <QVector>
#include <QDebug>
#include <QStringBuilder>

using namespace Chess;

Move Notation::stringToMove(const QString &string, Chess::NotationType notation, bool *ok, QString *err)
{
    //qDebug() << "Notation::stringToMove:" << string;
    if (ok)
        *ok = true;
    if (err)
        *err = QString();

    Move move;
    switch (notation) {
    case Standard:
        {
            /* Some Examples of SAN moves
             * d8        //Pawn to d8
             * cxd8      //Pawn on c captures d8
             * cxd8=Q+   //Pawn on c captures d8 promotes to queen and check
             * Qcd8      //Queen on file c to d8
             * Qc8d8     //Queen on file c and rank 8 to d8
             * Qxd8      //Queen captures d8
             * Qcxd8     //Queen on file c captures d8
             * Qc8xd8    //Queen on file c and rank 8 captures d8
             * Qc8xd8+   //Queen on file c and rank 8 captures d8 check
            */

            QString str = string;
            if (str.contains('x')) {
                move.setCapture(true);
                str = str.remove('x');
            }

            if (str.contains('=')) {
                int i = str.indexOf('=');
                QChar c = str.at(i + 1);
                move.setPromotion(charToPiece(c, notation, ok, err));
                str = str.remove(i + 1, 1);
                str = str.remove('=');
            }

            if (str.contains('+')) {
                move.setCheck(true);
                str = str.remove('+');
            }
            if (str.contains('#')) {
                move.setCheckMate(true);
                str = str.remove('#');
            }

            if (str == "O-O") {
                move.setPiece(King);
                move.setCastle(true);
                move.setCastleSide(KingSide);
                break;
            } else if (str == "O-O-O") {
                move.setPiece(King);
                move.setCastle(true);
                move.setCastleSide(QueenSide);
                break;
            } else if (str == "0-1") {
                break;
            } else if (str == "1-0") {
                break;
            } else if (str == "1/2-1/2") {
                break;
            }

            if (str.length() == 2) {
                move.setPiece(Pawn);
                move.setEnd(stringToSquare(str.right(2), notation, ok, err));
            } else if (str.length() == 3) {
                QChar c = str.at(0);
                if (c.isUpper()) {
                    move.setPiece(charToPiece(c, notation, ok, err));
                } else if (c.isLower()) {
                    move.setPiece(Pawn);
                }
                move.setEnd(stringToSquare(str.right(2), notation, ok, err));
            } else if (str.length() == 4) {
                move.setPiece(charToPiece(str.at(0), notation, ok, err));
                move.setEnd(stringToSquare(str.right(2), notation, ok, err));
            } else if (str.length() == 5) {
                move.setPiece(charToPiece(str.at(0), notation, ok, err));
                move.setEnd(stringToSquare(str.right(2), notation, ok, err));
            } else {
                if (ok) *ok = false;
                if (err) *err = QObject::tr("String for SAN move is incorrect size.");
            }
            break;
        }
    case Long:
            move.setCapture(string.contains('x'));
            move.setPiece(charToPiece(string.at(0), notation, ok, err));
            move.setStart(move.piece() != Pawn ? stringToSquare(string.mid(1, 2), notation, ok, err) :
                                                 stringToSquare(string.left(2), notation, ok, err));
            move.setEnd(stringToSquare(string.right(2), notation, ok, err));
            break;
    case Computer:
        {
            if (string == QLatin1String("(none)")) //glaurang sends this...
                break;

            move.setStart(stringToSquare(string.left(2), notation, ok, err));
            move.setEnd(stringToSquare(string.mid(2, 2), notation, ok, err));
            if (string.size() == 5) { //promotion
                move.setPromotion(charToPiece(string.at(4), notation, ok, err));
            }
            break;
        }
    }

    return move;
}

QString Notation::moveToString(const Move &move, Chess::NotationType notation)
{
    QString str;
    switch (notation) {
    case Standard:
        {
            QChar piece = pieceToChar(move.piece(), notation);
            QChar capture = move.isCapture() ? 'x' : QChar();
            QChar check = move.isCheck() ? '+' : QChar();
            QChar checkMate = move.isCheckMate() ? '#' : QChar();
            QString square = squareToString(move.end(), notation);

            if (!piece.isNull()) {
                str += piece;
            }

            if (!capture.isNull()) {
                if (move.piece() == Pawn)
                    str += fileToChar(move.start().file(), notation);
                str += capture;
            }

            str += square;

            if (move.promotion() != Unknown) {
                str += QString("=%1").arg(pieceToChar(move.promotion()));
            }

            if (!checkMate.isNull()) {
                str += checkMate;
            } else if (!check.isNull()) {
                str += check;
            }

            if (move.isCastle()) {
                if (move.castleSide() == KingSide)
                    str = "O-O";
                else if (move.castleSide() == QueenSide)
                    str = "O-O-O";
            }

            break;
        }
    case Long:
        {
            QChar piece = pieceToChar(move.piece(), notation);
            QChar sep = move.isCapture() ? 'x' : '-';
            QString start = squareToString(move.start(), notation);

            int e = move.end().file();
            // All castles are encoded as king captures rook internally which is correct for 960,
            // but not for normal
            if (move.isCastle() && !SearchSettings::chess960)
                e = e == 7 ? 6 : 2;

            QString end = squareToString(Square(e, move.end().rank()), notation);
            if (!piece.isNull())
                str += piece;

            str += start;
            str += sep;
            str += end;

            break;
        }
    case Computer:
        {
            QString start = squareToString(move.start(), notation);

            int e = move.end().file();
            // All castles are encoded as king captures rook internally which is correct for 960,
            // but not for normal
            if (move.isCastle() && !SearchSettings::chess960)
                e = e == 7 ? 6 : 2;

            QString end = squareToString(Square(e, move.end().rank()), notation);
            str += start;
            str += end;

            if (move.promotion() != Unknown)
                str += pieceToChar(move.promotion(), notation).toLower();
        break;
        }
    }

    return str;
}

Square Notation::stringToSquare(const QString &string, Chess::NotationType notation, bool *ok, QString *err)
{
    int file = 0;
    int rank = 0;

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            if (string.size() != 2) {
                if (ok) *ok = false;
                if (err) *err = QObject::tr("String for square is incorrect size.");
                return Square();
            }
            file = charToFile(string.at(0), notation, ok, err);
            rank = charToRank(string.at(1), notation, ok, err);
            break;
        }
    }

    return Square(file, rank);
}

QString Notation::squareToString(const Square &square, Chess::NotationType)
{
    if (!square.isValid())
        return QString();

    return QString(fileToChar(square.file()) % rankToChar(square.rank()));
}

Chess::PieceType Notation::charToPiece(const QChar &ch, Chess::NotationType notation, bool *ok, QString *err)
{
    Q_UNUSED(ok);
    Q_UNUSED(err);
    PieceType piece = Chess::Unknown;

    static QVector<QChar> pieces = {'U', 'K', 'Q', 'R', 'B', 'N', 'P'};

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            if (pieces.contains(ch.toUpper())) {
                piece = PieceType(pieces.indexOf(ch.toUpper()));
            } else {
                piece = Pawn;
            }
            break;
        }
    }

    return piece;
}

QChar Notation::pieceToChar(Chess::PieceType piece, Chess::NotationType notation)
{
    QChar ch;

    static QVector<QChar> pieces = {'U', 'K', 'Q', 'R', 'B', 'N', 'P'};

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            ch = pieces.at(piece);
            if (ch == 'P')
                ch = QChar();
            break;
        }
    }

    return ch;
}

int Notation::charToFile(const QChar &ch, Chess::NotationType notation, bool *ok, QString *err)
{
    int file = 0;

    static QVector<QChar> files = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            if (files.contains(ch.toLower())) {
                file = files.indexOf(ch.toLower());
            } else {
                if (ok) *ok = false;
                if (err) *err = QObject::tr("Char for file is invalid.");
                return -1;
            }
            break;
        }
    }

    return file;
}

QChar Notation::fileToChar(int file, Chess::NotationType notation)
{
    QChar ch;

    Q_ASSERT_X(file >=0 && file <= 7,
               "Notation::fileToChar(int file, ...) range error",
               QString("%1").arg(QString::number(file)).toLatin1().constData());

    static QVector<QChar> files = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            ch = files.at(file);
            break;
        }
    }

    return ch;
}

int Notation::charToRank(const QChar &ch, Chess::NotationType notation, bool *ok, QString *err)
{
    int rank = 0;

    static QVector<QChar> ranks = {'1', '2', '3', '4', '5', '6', '7', '8'};

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            if (ranks.contains(ch.toLower())) {
                rank = ranks.indexOf(ch.toLower());
            } else {
                if (ok) *ok = false;
                if (err) *err = QObject::tr("Char for rank is invalid.");
                rank = -1;
            }
            break;
        }
    }

    return rank;
}

QChar Notation::rankToChar(int rank, Chess::NotationType notation)
{
    QChar ch;

    static QVector<QChar> ranks = {'1', '2', '3', '4', '5', '6', '7', '8'};

    switch (notation) {
    case Standard:
    case Long:
    case Computer:
        {
            ch = ranks.at(rank);
            break;
        }
    }

    return ch;
}

Notation::Notation()
{
}

Notation::~Notation()
{
}
