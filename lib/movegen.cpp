/*
  This file is part of Allie Chess.
  Copyright (C) 2018, 2019 Adam Treat
  Copyright (C) Andrew Grant

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

#include "movegen.h"

#include "bitboard.h"
#include "chess.h"

// Largely from ethereal's magic move generation courtesy of Andrew Grant
static const quint64 RookMagics[64] = {
    0xA180022080400230ull, 0x0040100040022000ull, 0x0080088020001002ull, 0x0080080280841000ull,
    0x4200042010460008ull, 0x04800A0003040080ull, 0x0400110082041008ull, 0x008000A041000880ull,
    0x10138001A080C010ull, 0x0000804008200480ull, 0x00010011012000C0ull, 0x0022004128102200ull,
    0x000200081201200Cull, 0x202A001048460004ull, 0x0081000100420004ull, 0x4000800380004500ull,
    0x0000208002904001ull, 0x0090004040026008ull, 0x0208808010002001ull, 0x2002020020704940ull,
    0x8048010008110005ull, 0x6820808004002200ull, 0x0A80040008023011ull, 0x00B1460000811044ull,
    0x4204400080008EA0ull, 0xB002400180200184ull, 0x2020200080100380ull, 0x0010080080100080ull,
    0x2204080080800400ull, 0x0000A40080360080ull, 0x02040604002810B1ull, 0x008C218600004104ull,
    0x8180004000402000ull, 0x488C402000401001ull, 0x4018A00080801004ull, 0x1230002105001008ull,
    0x8904800800800400ull, 0x0042000C42003810ull, 0x008408110400B012ull, 0x0018086182000401ull,
    0x2240088020C28000ull, 0x001001201040C004ull, 0x0A02008010420020ull, 0x0010003009010060ull,
    0x0004008008008014ull, 0x0080020004008080ull, 0x0282020001008080ull, 0x50000181204A0004ull,
    0x48FFFE99FECFAA00ull, 0x48FFFE99FECFAA00ull, 0x497FFFADFF9C2E00ull, 0x613FFFDDFFCE9200ull,
    0xFFFFFFE9FFE7CE00ull, 0xFFFFFFF5FFF3E600ull, 0x0010301802830400ull, 0x510FFFF5F63C96A0ull,
    0xEBFFFFB9FF9FC526ull, 0x61FFFEDDFEEDAEAEull, 0x53BFFFEDFFDEB1A2ull, 0x127FFFB9FFDFB5F6ull,
    0x411FFFDDFFDBF4D6ull, 0x0801000804000603ull, 0x0003FFEF27EEBE74ull, 0x7645FFFECBFEA79Eull
};

static const quint64 BishopMagics[64] = {
    0xFFEDF9FD7CFCFFFFull, 0xFC0962854A77F576ull, 0x5822022042000000ull, 0x2CA804A100200020ull,
    0x0204042200000900ull, 0x2002121024000002ull, 0xFC0A66C64A7EF576ull, 0x7FFDFDFCBD79FFFFull,
    0xFC0846A64A34FFF6ull, 0xFC087A874A3CF7F6ull, 0x1001080204002100ull, 0x1810080489021800ull,
    0x0062040420010A00ull, 0x5028043004300020ull, 0xFC0864AE59B4FF76ull, 0x3C0860AF4B35FF76ull,
    0x73C01AF56CF4CFFBull, 0x41A01CFAD64AAFFCull, 0x040C0422080A0598ull, 0x4228020082004050ull,
    0x0200800400E00100ull, 0x020B001230021040ull, 0x7C0C028F5B34FF76ull, 0xFC0A028E5AB4DF76ull,
    0x0020208050A42180ull, 0x001004804B280200ull, 0x2048020024040010ull, 0x0102C04004010200ull,
    0x020408204C002010ull, 0x02411100020080C1ull, 0x102A008084042100ull, 0x0941030000A09846ull,
    0x0244100800400200ull, 0x4000901010080696ull, 0x0000280404180020ull, 0x0800042008240100ull,
    0x0220008400088020ull, 0x04020182000904C9ull, 0x0023010400020600ull, 0x0041040020110302ull,
    0xDCEFD9B54BFCC09Full, 0xF95FFA765AFD602Bull, 0x1401210240484800ull, 0x0022244208010080ull,
    0x1105040104000210ull, 0x2040088800C40081ull, 0x43FF9A5CF4CA0C01ull, 0x4BFFCD8E7C587601ull,
    0xFC0FF2865334F576ull, 0xFC0BF6CE5924F576ull, 0x80000B0401040402ull, 0x0020004821880A00ull,
    0x8200002022440100ull, 0x0009431801010068ull, 0xC3FFB7DC36CA8C89ull, 0xC3FF8A54F4CA2C89ull,
    0xFFFFFCFCFD79EDFFull, 0xFC0863FCCB147576ull, 0x040C000022013020ull, 0x2000104000420600ull,
    0x0400000260142410ull, 0x0800633408100500ull, 0xFC087E8E4BB2F736ull, 0x43FF9E4EF4CA2C89ull
};

static const quint64 RANK_8 = 0xFF00000000000000ull;
static const quint64 RANK_7 = 0x00FF000000000000ull;
static const quint64 RANK_6 = 0x0000FF0000000000ull;
static const quint64 RANK_5 = 0x000000FF00000000ull;
static const quint64 RANK_4 = 0x00000000FF000000ull;
static const quint64 RANK_3 = 0x0000000000FF0000ull;
static const quint64 RANK_2 = 0x000000000000FF00ull;
static const quint64 RANK_1 = 0x00000000000000FFull;

static const quint64 FILE_A = 0x0101010101010101ull;
static const quint64 FILE_B = 0x0202020202020202ull;
static const quint64 FILE_C = 0x0404040404040404ull;
static const quint64 FILE_D = 0x0808080808080808ull;
static const quint64 FILE_E = 0x1010101010101010ull;
static const quint64 FILE_F = 0x2020202020202020ull;
static const quint64 FILE_G = 0x4040404040404040ull;
static const quint64 FILE_H = 0x8080808080808080ull;

static const quint64 Ranks[8] = {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8};
static const quint64 Files[8] = {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H};

static int validCoordinate(int rank, int file) {
    return 0 <= rank && rank < 8
        && 0 <= file && file < 8;
}

static BitBoard sliderMoves(const Square &square, const BitBoard &occupied, const int delta[4][2]) {

    int rank, file;
    BitBoard result;

    for (int i = 0; i < 4; i++) {

        int dr = delta[i][0];
        int df = delta[i][1];

        for (rank = square.rank() + dr, file = square.file() + df;
             validCoordinate(rank, file);
             rank += dr, file += df) {

            result.setBit(BitBoard::squareToIndex(Square(qint8(file), qint8(rank))));
            if (occupied.testBit(BitBoard::squareToIndex(Square(qint8(file), qint8(rank)))))
                break;
        }
    }

    return result;
}

static void initSliderMoves(const Square &square, Magic *table, quint64 magic, const int delta[4][2]) {

    const quint64 edges = ((RANK_1 | RANK_8) & ~Ranks[square.rank()])
                         | ((FILE_A | FILE_H) & ~Files[square.file()]);

    const int sq = square.data();
    quint64 occupied = 0ull;

    table[sq].magic = magic;
    table[sq].mask  = sliderMoves(square, 0, delta).data() & ~edges;
    table[sq].shift = quint64(64 - BitBoard(table[sq].mask).count());

    if (sq != 64 - 1)
        table[sq+1].offset = table[sq].offset + (quint64(1) << BitBoard(table[sq].mask).count());

    do {
        quint64 index = sliderIndex(occupied, &table[sq]);
        table[sq].offset[index] = sliderMoves(square, occupied, delta).data();
        occupied = (occupied - table[sq].mask) & table[sq].mask;
    } while (occupied);
}

class MyMovegen : public Movegen { };
Q_GLOBAL_STATIC(MyMovegen, movegenInstance)
Movegen *Movegen::globalInstance()
{
    return movegenInstance();
}

static quint64 s_rookMoves[0x19000];
static quint64 s_bishopMoves[0x1480];

Movegen::Movegen()
{
    m_rookTable[0].offset = s_rookMoves;
    m_bishopTable[0].offset = s_bishopMoves;

    const int RookDelta[4][2]   = {{-1, 0}, { 0,-1}, { 0, 1}, { 1, 0}};
    const int BishopDelta[4][2] = {{-1,-1}, {-1, 1}, { 1,-1}, { 1, 1}};

    for (int i = 0; i < 64; i++) {
        Square sq = BitBoard::indexToSquare(i);
        Q_ASSERT(sq.data() == i);
        m_kingMoves[i] = raysForKing(sq);

        // init move tables for sliding pieces
        initSliderMoves(sq, m_rookTable, RookMagics[i], RookDelta);
        initSliderMoves(sq, m_bishopTable, BishopMagics[i], BishopDelta);

        m_knightMoves[i] = raysForKnight(sq);

        if (sq.rank() != 0) {
            m_pawnMoves[Chess::White][i] = raysForPawn(Chess::White, sq);
            m_pawnAttacks[Chess::White][i] = raysForPawnAttack(Chess::White, sq);
        }

        if (sq.rank() != 7) {
            m_pawnMoves[Chess::Black][i] = raysForPawn(Chess::Black, sq);
            m_pawnAttacks[Chess::Black][i] = raysForPawnAttack(Chess::Black, sq);
        }
    }
}

Movegen::~Movegen()
{
}

BitBoard Movegen::raysForKing(const Square &sq)
{
    BitBoard rays;
    generateRay(North, 1, sq, &rays);
    generateRay(NorthEast, 1, sq, &rays);
    generateRay(East, 1, sq, &rays);
    generateRay(SouthEast, 1, sq, &rays);
    generateRay(South, 1, sq, &rays);
    generateRay(SouthWest, 1, sq, &rays);
    generateRay(West, 1, sq, &rays);
    generateRay(NorthWest, 1, sq, &rays);
    return rays;
}

BitBoard Movegen::raysForQueen(const Square &sq)
{
    BitBoard rays;
    generateRay(North, 7, sq, &rays);
    generateRay(NorthEast, 7, sq, &rays);
    generateRay(East, 7, sq, &rays);
    generateRay(SouthEast, 7, sq, &rays);
    generateRay(South, 7, sq, &rays);
    generateRay(SouthWest, 7, sq, &rays);
    generateRay(West, 7, sq, &rays);
    generateRay(NorthWest, 7, sq, &rays);
    return rays;
}

BitBoard Movegen::raysForRook(const Square &sq)
{
    BitBoard rays;
    generateRay(North, 7, sq, &rays);
    generateRay(East, 7, sq, &rays);
    generateRay(South, 7, sq, &rays);
    generateRay(West, 7, sq, &rays);
    return rays;
}

BitBoard Movegen::raysForBishop(const Square &sq)
{
    BitBoard rays;
    generateRay(NorthEast, 7, sq, &rays);
    generateRay(SouthEast, 7, sq, &rays);
    generateRay(SouthWest, 7, sq, &rays);
    generateRay(NorthWest, 7, sq, &rays);
    return rays;
}

BitBoard Movegen::raysForKnight(const Square &sq)
{
    BitBoard rays;
    int f = sq.file();
    int r = sq.rank();

    //2 north then 1 east
    if (r + 2 < 8)
        generateRay(East, 1, Square(f, r + 2), &rays);

    //2 east then 1 north
    if (f + 2 < 8)
        generateRay(North, 1, Square(f + 2, r), &rays);

    //2 east then 1 south
    if (f + 2 < 8)
        generateRay(South, 1, Square(f + 2, r), &rays);

    //2 south then 1 east
    if (r - 2 >= 0)
        generateRay(East, 1, Square(f, r - 2), &rays);

    //2 south then 1 west
    if (r - 2 >= 0)
        generateRay(West, 1, Square(f, r - 2), &rays);

    //2 west then 1 south
    if (f - 2 >= 0)
        generateRay(South, 1, Square(f - 2, r), &rays);

    //2 west then 1 north
    if (f - 2 >= 0)
        generateRay(North, 1, Square(f - 2, r), &rays);

    //2 north then 1 west
    if (r + 2 < 8)
        generateRay(West, 1, Square(f, r + 2), &rays);

    return rays;
}

BitBoard Movegen::raysForPawn(Chess::Army army, const Square &sq)
{
    BitBoard rays;

    switch (army) {
    case Chess::White:
        {
            if (sq.rank() == 1) {
                generateRay(North, 2, sq, &rays);
            } else {
                generateRay(North, 1, sq, &rays);
            }
            break;
        }
    case Chess::Black:
        {
            if (sq.rank() == 6) {
                generateRay(South, 2, sq, &rays);
            } else {
                generateRay(South, 1, sq, &rays);
            }
            break;
        }
    }

    return rays;
}

BitBoard Movegen::raysForPawnAttack(Chess::Army army, const Square &sq)
{
    BitBoard rays;

    switch (army) {
    case Chess::White:
        {
            generateRay(NorthEast, 1, sq, &rays);
            generateRay(NorthWest, 1, sq, &rays);
            break;
        }
    case Chess::Black:
        {
            generateRay(SouthEast, 1, sq, &rays);
            generateRay(SouthWest, 1, sq, &rays);
            break;
        }
    }

    return rays;
}

void Movegen::generateRay(Direction direction, int magnitude, const Square &sq, BitBoard *rays)
{
    const int initialFile = sq.file();
    const int initialRank = sq.rank();
    int count = 0;

    switch (direction) {
    case North:
        {
            int r = initialRank + 1;
            for (; r < 8 && count != magnitude; r++, count++) {
                Square sq(initialFile, r);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case NorthEast:
        {
            int f = initialFile + 1;
            int r = initialRank + 1;
            for (; f < 8 && r < 8 && count != magnitude; f++, r++, count++) {
                Square sq(f, r);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case East:
        {
            int f = initialFile + 1;
            for (; f < 8 && count != magnitude; f++, count++) {
                Square sq(f, initialRank);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case SouthEast:
        {
            int f = initialFile + 1;
            int r = initialRank - 1;
            for (; f < 8 && r >= 0 && count != magnitude; f++, r--, count++) {
                Square sq(f, r);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case South:
        {
            int r = initialRank - 1;
            for (; r >= 0 && count != magnitude; r--, count++) {
                Square sq(initialFile, r);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case SouthWest:
        {
            int f = initialFile - 1;
            int r = initialRank - 1;
            for (; f >= 0 && r >= 0 && count != magnitude; f--, r--, count++) {
                Square sq(f, r);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case West:
        {
            int f = initialFile - 1;
            for (; f >= 0 && count != magnitude; f--, count++) {
                Square sq(f, initialRank);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    case NorthWest:
        {
            int f = initialFile - 1;
            int r = initialRank + 1;
            for (; f >= 0 && r < 8 && count != magnitude; f--, r++, count++) {
                Square sq(f, r);
                *rays = *rays | BitBoard(sq);
            }
            break;
        }
    }
}
