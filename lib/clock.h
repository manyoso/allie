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

#ifndef CLOCK_H
#define CLOCK_H

#include <QObject>

#include <QTimer>
#include <QElapsedTimer>

#include "chess.h"
#include "search.h"

class Clock : public QObject {
    Q_OBJECT
public:
    Clock(QObject *parent);
    ~Clock();

    qint64 time(Chess::Army army) const;
    void setTime(Chess::Army army, qint64 time);

    qint64 increment(Chess::Army army) const;
    void setIncrement(Chess::Army army, qint64 inc);

    bool isMoveTime() const;
    void setMoveTime(qint64 time);
    bool isInfinite() const;
    void setInfinite(bool infinite);

    void startDeadline(Chess::Army army);
    void updateDeadline(const SearchInfo &info, bool isPartial);

    qint64 elapsed() const;
    bool hasExpired() const;
    qint64 deadline() const { return m_deadline; }
    qint64 timeToDeadline() const;

    float extraBudgetedTime() const { return m_extraBudgetedTime; }
    void setExtraBudgetedTime(float t) { m_extraBudgetedTime = t; }

    void setMaterialScore(int score) { m_materialScore = score; }
    void setHalfMoveNumber(int half) { m_halfMoveNumber = half; }

    bool lessThanMoveOverhead() const;
    bool pastMoveOverhead() const;

    bool isActive() const { return m_isActive; }
    void stop();

    bool isExtended() const { return m_isExtended; }
    void resetExtension() { m_isExtended = false; }

Q_SIGNALS:
    void timeout();

private Q_SLOTS:
    void maybeTimeout();

private:
    int expectedHalfMovesTillEOG() const;
    void calculateDeadline(bool isPartial);

    bool m_isActive;

    qint64 m_whiteTime;
    qint64 m_whiteIncrement;

    qint64 m_blackTime;
    qint64 m_blackIncrement;

    qint64 m_moveTime;
    float m_extraBudgetedTime;
    bool m_infinite;
    bool m_isExtended;

    SearchInfo m_info;
    qint64 m_deadline;
    int m_materialScore;
    int m_halfMoveNumber;
    Chess::Army m_onTheClock;
    QElapsedTimer m_timer;
    QTimer *m_timeout;
};

#endif
