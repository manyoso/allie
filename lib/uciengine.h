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

#ifndef UCIENGINE_H
#define UCIENGINE_H

#include <QLoggingCategory>
#include <QQueue>
#include <QVariant>
#include <QVector>
#include <QSocketNotifier>
#include <QCommandLineOption>

#include "game.h"
#include "searchengine.h"

// Follows http://wbec-ridderkerk.nl/html/UCIProtocol.html

class Clock;
class Move;

struct UciOption {
public:
    enum OptionType { Check, Spin, Combo, Button, String };
    enum Parameter { Name, Type, Default, Min, Max, Var };

    UciOption();

    QString value() const { return m_value; }
    void setValue(const QString &value) { m_value = value; }

    QString optionName() const { return m_name; }
    OptionType optionType() const { return m_type; }
    QString optionDefault() const { return m_default; }
    QString optionMin() const { return m_min; }
    QString optionMax() const { return m_max; }
    QVector<QString> optionVar() const { return m_var; }
    QString description() const { return m_description; }

    QString toString() const;

    static QString toCamelCase(const QString& s);

    QCommandLineOption commandLine() const;
    void setFromCommandLine(const QCommandLineOption &line);

private:
    QString m_name;
    QString m_description;
    OptionType m_type;
    QString m_default;
    QString m_min;
    QString m_max;
    QVector<QString> m_var;
    QString m_value;
    friend class Options;
};

void g_uciMessageHandler(QtMsgType, const QMessageLogContext &, const QString &);
Q_DECLARE_LOGGING_CATEGORY(UciInput);
Q_DECLARE_LOGGING_CATEGORY(UciOutput);

class IOWorker : public QObject {
    Q_OBJECT
public:
    IOWorker(const QString &debugFile, QObject *parent = nullptr);

    void startDebug();

public Q_SLOTS:
    void run();
    void readyRead();
    void readyReadOutput(const QString& line);

Q_SIGNALS:
    void standardInput(const QString &line);

private:
    QQueue<QString> m_debugLines;
    QString m_waitingOnOutput;
};

class IOHandler {
public:
    virtual ~IOHandler() {}
    virtual void handleInfo(const SearchInfo &info);
    virtual void handleBestMove(const QString &bestMove);
};

class UciEngine : public QObject {
    Q_OBJECT
public:
    UciEngine(QObject *parent, const QString &debugFile);
    ~UciEngine();

    void run();

    SearchEngine *searchEngine() const { return m_searchEngine; }

public Q_SLOTS:
    void sendId();
    void sendUciOk();
    void sendReadyOk();
    void sendBestMove(bool force = false);
    void sendInfo(const SearchInfo &info, bool isPartial);
    void sendAverages();
    void sendOptions();
    void uciNewGame();
    void ponderHit();
    void stop();
    void quit();
    void readyRead(const QString &line);
    void installIOHandler(IOHandler *io) { m_ioHandler = io; }

Q_SIGNALS:
    void sendOutput(const QString &output);

private:
    void stopTheClock();
    void startSearch(const Search &s);
    void stopSearch();
    void calculateRollingAverage(const SearchInfo &info);
    void setPosition(const QString &position, const QVector<QString> &moves);
    void parseGo(const QString &move);
    void parseOption(const QString &option);
    void go(const Search &search);

    void input(const QString &in);
    void output(const QString &out);

private:
    SearchInfo m_averageInfo;
    SearchInfo m_lastInfo;
    bool m_debug;
    bool m_gameInitialized;
    QString m_debugFile;
    QVector<UciOption> m_options;
    SearchEngine *m_searchEngine;
    QThread m_inputThread;
    qint64 m_timeAtLastProgress;
    qint64 m_depthTargeted;
    qint64 m_nodesTargeted;
    Clock *m_clock;
    IOHandler *m_ioHandler;
};

#endif
