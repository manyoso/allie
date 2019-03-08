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

#include "tb.h"

#include "options.h"
#include "fathom/tbprobe.h"

#include <QDebug>

class MyTB : public TB { };
Q_GLOBAL_STATIC(MyTB, TBInstance)
TB* TB::globalInstance()
{
    return TBInstance();
}

TB::TB()
    : m_enabled(false)
{
}

TB::~TB()
{
}

void TB::reset()
{
    const QString path = Options::globalInstance()->option("SyzygyPath").value();
    bool success = tb_init(path.toLatin1().constData());
    m_enabled = success && TB_LARGEST;
    if (m_enabled)
        fprintf(stderr, "Using %d-man tablebase: %s\n", TB_LARGEST, path.toLatin1().constData());
}

TB::Probe TB::probe(const Game &game) const
{
    if (!m_enabled)
        return NotFound;

    if (game.halfMoveClock() != 0)
        return NotFound;

    if (game.m_hasWhiteKingCastle || game.m_hasBlackKingCastle
        || game.m_hasWhiteQueenCastle || game.m_hasBlackQueenCastle)
        return NotFound;

    if (unsigned(BitBoard(game.m_whitePositionBoard | game.m_blackPositionBoard).count()) > TB_LARGEST)
        return NotFound;

    const quint8 enpassant = !game.m_enPassantTarget.isValid() ? 0 : game.m_enPassantTarget.data();

    const unsigned result = tb_probe_wdl(
        game.m_whitePositionBoard.data(),
        game.m_blackPositionBoard.data(),
        game.m_kingsBoard.data(),
        game.m_queensBoard.data(),
        game.m_rooksBoard.data(),
        game.m_bishopsBoard.data(),
        game.m_knightsBoard.data(),
        game.m_pawnsBoard.data(),
        0 /*half move clock*/,
        0 /*castling rights*/,
        enpassant,
        game.m_activeArmy == Chess::White);

    switch (result) {
    case TB_RESULT_FAILED:
        return NotFound;
    case TB_LOSS:
//        qDebug() << "tb says loss" << game.stateOfGameToFen();
        return Loss;
    case TB_WIN:
//        qDebug() << "tb says win" << game.stateOfGameToFen();
        return Win;
    case TB_CURSED_WIN:
    case TB_BLESSED_LOSS:
    case TB_DRAW:
//        qDebug() << "tb says draw" << game.stateOfGameToFen();
        return Draw;
    default:
        Q_UNREACHABLE();
        return NotFound;
    }
}

