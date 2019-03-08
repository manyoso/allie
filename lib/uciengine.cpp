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

#include "uciengine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include <iostream>

#include "chess.h"
#include "clock.h"
#include "game.h"
#include "hash.h"
#include "history.h"
#include "nn.h"
#include "notation.h"
#include "options.h"
#include "searchengine.h"
#include "tb.h"

#define LOG
//#define AVERAGES
#if defined(LOG)
static bool s_firstLog = true;
#endif

//#define DEBUG_TIME

using namespace Chess;

UciOption::UciOption()
    : m_name(QString()),
      m_type(String),
      m_default(QString()),
      m_min(QString()),
      m_max(QString()),
      m_var(QVector<QString>()),
      m_value(QString())
{
}

QString UciOption::toString() const {
    QStringList list;
    list << "option" << "name" << m_name << "type";
    switch (m_type) {
    case Check:
        {
            list << "check";
            list << "default" << m_default;
            break;
        }
    case Spin:
        {
            list << "spin";
            list << "default" << m_default;
            list << "min" << m_min;
            list << "max" << m_max;

            break;
        }
    case Combo:
        {
            list << "combo";
            list << "default" << m_default;
            for (QString v : m_var) {
                list << "var" << v;
            }
            break;
        }
    case Button:
        {
            list << "button";
            break;
        }
    case String:
        {
            list << "string";
            list << "default" << m_default;
            break;
        }
    }
    return list.join(" ") +'\n';
}

QString UciOption::toCamelCase(const QString& s)
{
    return s.at(0).toLower() + s.mid(1);
}

QCommandLineOption UciOption::commandLine() const
{
    QString desc;
    if (!m_min.isEmpty() && !m_max.isEmpty())
        desc = QString("%0 (min:%1, max:%2, default:%3)").arg(m_description).arg(m_min).arg(m_max).arg(m_default);
    else if (!m_var.isEmpty())
        desc = QString("%0 (%1, default:%2)").arg(m_description).arg(m_var.toList().join(", ")).arg(m_default);
    else
        desc = QString("%0 (default:%1)").arg(m_description).arg(m_default);
    return QCommandLineOption(toCamelCase(m_name), desc, "value", m_default);
}

void UciOption::setFromCommandLine(const QCommandLineOption &line)
{
    Q_UNUSED(line);
}

Q_LOGGING_CATEGORY(UciInput, "input")
Q_LOGGING_CATEGORY(UciOutput, "output")

void g_uciMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString format;
    QTextStream out(&format);
    if (QLatin1String(context.category) == QLatin1Literal("input")) {
        out << "Input: " << msg << endl;
    } else if (QLatin1String(context.category) == QLatin1Literal("output")) {
        out << "Output: " << msg;
        fprintf(stdout, "%s", qPrintable(msg));
        fflush(stdout);
    } else {
        switch (type) {
        case QtDebugMsg:
            out << "Debug: " << msg << endl;
            break;
        case QtInfoMsg:
            out << "Info: " << msg << endl;
            break;
        case QtWarningMsg:
            out << "Warning: " << msg << endl;
            break;
        case QtCriticalMsg:
            out << "Critical: " << msg << endl;
            break;
        case QtFatalMsg:
            out << "Fatal: " << msg << endl;
            break;
        }
        fprintf(stderr, "%s", format.toLatin1().constData());
    }

#if defined(LOG)
    QString logFilePath = QCoreApplication::applicationDirPath() +
        QDir::separator() + QCoreApplication::applicationName() +
        "_debug.log";
    QFile file(logFilePath);

    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append;
    if (!file.open(mode))
        return;

    QTextStream log(&file);
    if (s_firstLog)
        log << "Output: log pid " << QCoreApplication::applicationPid() << " at "
            << QDateTime::currentDateTime().toString() << "\n";
    log << format;
    s_firstLog = false;
#endif
}

IOWorker::IOWorker(const QString &debugFile, QObject *parent)
    : QObject(parent)
{
    if (!debugFile.isEmpty()) {
        QFile file(debugFile);
        QIODevice::OpenMode mode = QIODevice::ReadOnly | QIODevice::Text;
        if (file.open(mode)) {
            QTextStream debug(&file);
            while (!debug.atEnd())
                m_debugLines.enqueue(debug.readLine());
            file.close();
        }
    }
}

void IOWorker::startDebug()
{
    while (!m_debugLines.isEmpty()) {
        QString line = m_debugLines.dequeue();
        if (line == "Input:" && !m_debugLines.isEmpty()) {
            QString input = m_debugLines.dequeue();
            QString lastLine;
            QQueue<QString>::const_iterator it = m_debugLines.begin();
            for (; it != m_debugLines.end(); ++it) {
                if (*it == "Input:")
                    break;
                lastLine = *it;
            }
            m_waitingOnOutput = lastLine;
            fprintf(stderr, "%s\n", input.toLatin1().constData());
            emit standardInput(input);
            if (!m_waitingOnOutput.isEmpty())
                return;
        }
    }
}

void IOWorker::run()
{
    if (!m_debugLines.isEmpty())
        startDebug();
    else
        readyRead();
}

void IOWorker::readyRead()
{
    std::cout.setf(std::ios::unitbuf);
    std::string line;
    while (std::getline(std::cin, line)) {
        QString ln = QString::fromStdString(line);
        emit standardInput(ln);
        if (ln == QLatin1Literal("quit"))
            return;
    }
}

void IOWorker::readyReadOutput(const QString &output)
{
    if ((output.startsWith("bestmove") && m_waitingOnOutput.startsWith("bestmove")) ||
        output == m_waitingOnOutput + "\n") {
        startDebug();
    }
}

UciEngine::UciEngine(QObject *parent, const QString &debugFile)
    : QObject(parent),
    m_debug(false),
    m_gameInitialized(false),
    m_debugFile(debugFile),
    m_searchEngine(nullptr),
    m_timeAtLastProgress(0),
    m_depthTargeted(-1),
    m_clock(new Clock(this)),
    m_ioHandler(nullptr)
{
    m_searchEngine = new SearchEngine(this);
    connect(m_searchEngine, &SearchEngine::sendInfo, this, &UciEngine::sendInfo);
    connect(m_clock, &Clock::timeout, this, [&](){ sendBestMove(false /*force*/); });
}

UciEngine::~UciEngine()
{
    m_inputThread.quit();
    m_inputThread.wait();
}

void UciEngine::readyRead(const QString &line)
{
    input(line); // log the input

    if (line == QLatin1Literal("uci")) {
        sendId();
        sendOptions();
        sendUciOk();
    } else if (line.startsWith("debug")) {
        QList<QString> debug = line.split(' ');
        if (debug.count() == 2) {
            if (debug.at(1) == "on")
                m_debug = true;
            else if (debug.at(1) == "off")
                m_debug = false;
        } else {
            m_debug = true;
        }
    } else if (line == QLatin1Literal("isready")) {
        sendReadyOk();
    } else if (line.startsWith("setoption")) {
        parseOption(line);
    } else if (line.startsWith("register")) {
        // noop
    } else if (line == QLatin1Literal("ucinewgame")) {
        uciNewGame();
    } else if (line.startsWith("position")) {
        QList<QString> position = line.split(' ');
        QString pos = position.at(1);
        if (pos == QLatin1String("fen")) {
            QList<QString> moves;
            QString fen = line.mid(13);
            int indexOfMoves = fen.indexOf(QLatin1String("moves "));
            if (indexOfMoves > 0) {
                QString m = indexOfMoves > 0 ? fen.mid(indexOfMoves + 6) : fen;
                moves = m.split(' ');
                fen.truncate(indexOfMoves - 1);
            }
            setPosition(fen, moves.toVector());
        } else {
            QList<QString> moves;
            if (position.count() >= 4 && position.at(2) == QLatin1Literal("moves"))
                moves = position.mid(3);

            setPosition(pos, moves.toVector());
        }
    } else if (line.startsWith("go")) {
        parseGo(line);
    } else if (line == QLatin1Literal("stop")) {
        stop();
    } else if (line == QLatin1Literal("ponderhit")) {
        ponderHit();
    } else if (line == QLatin1Literal("quit")) {
        quit();
    }
    // non-uci additions
    else if (line == QLatin1Literal("board")) {
        output(History::globalInstance()->currentGame().stateOfGameToFen() + "\n");
    } else if (line.startsWith("tree")) {
        int depth = 1;
        QList<QString> tree = line.split(' ');
        if (tree.count() == 2) {
            bool success;
            int d  = tree.at(1).toInt(&success);
            if (success)
                depth = d;
        }
        if (m_searchEngine)
            m_searchEngine->printTree(depth);
    }
}

void UciEngine::run()
{
    IOWorker *worker = new IOWorker(m_debugFile);
    worker->moveToThread(&m_inputThread);
    connect(worker, &IOWorker::standardInput, this, &UciEngine::readyRead);
    connect(this, &UciEngine::sendOutput, worker, &IOWorker::readyReadOutput);
    connect(&m_inputThread, &QThread::started, worker, &IOWorker::run);
    connect(&m_inputThread, &QThread::finished, worker, &QObject::deleteLater);
    m_inputThread.setObjectName("io");
    m_inputThread.start();
}

void UciEngine::sendId()
{
    QString out;
    QTextStream stream(&out);
    stream << "id name "
           << QCoreApplication::applicationName() << " "
           << QCoreApplication::applicationVersion() << endl
           << "id author "
           << QCoreApplication::organizationName() << endl;
    output(out);
}

void UciEngine::sendUciOk()
{
    QString out;
    QTextStream stream(&out);
    stream << "uciok" << endl;
    output(out);
}

void UciEngine::sendReadyOk()
{
    QString out;
    QTextStream stream(&out);
    stream << "readyok" << endl;
    output(out);
}

static int rollingAverage(int oldAvg, int newNumber, int n)
{
    // i.e. to calculate the new average after then nth number,
    // you multiply the old average by n−1, add the new number, and divide the total by n.
    return qRound(((float(oldAvg) * (n - 1)) + newNumber) / float(n));
}

void UciEngine::stopTheClock()
{
    m_clock->stop();
}

void UciEngine::startSearch(const Search &s)
{
    Q_ASSERT(m_searchEngine && m_gameInitialized);
    if (m_searchEngine)
        m_searchEngine->startSearch(s);
}

void UciEngine::stopSearch()
{
    Q_ASSERT(m_searchEngine && m_gameInitialized);
    if (m_searchEngine)
        m_searchEngine->stopSearch();
}

void UciEngine::calculateRollingAverage(const SearchInfo &info)
{
    const int n = History::globalInstance()->currentGame().halfMoveNumber() / 2;
    if (!n)
        return;

    m_averageInfo.depth             = rollingAverage(m_averageInfo.depth, info.depth, n);
    m_averageInfo.seldepth          = rollingAverage(m_averageInfo.seldepth, info.seldepth, n);
    m_averageInfo.nodes             = rollingAverage(m_averageInfo.nodes, info.nodes, n);
    m_averageInfo.nps               = rollingAverage(m_averageInfo.nps, info.nps, n);
    m_averageInfo.batchSize         = rollingAverage(m_averageInfo.batchSize, info.batchSize, n);
    m_averageInfo.rawnps            = rollingAverage(m_averageInfo.rawnps, info.rawnps, n);

    WorkerInfo &avgW = m_averageInfo.workerInfo;
    const WorkerInfo &newW = info.workerInfo;
    avgW.nodesSearched     = rollingAverage(avgW.nodesSearched, newW.nodesSearched, n);
    avgW.nodesEvaluated    = rollingAverage(avgW.nodesEvaluated, newW.nodesEvaluated, n);
    avgW.nodesCreated    = rollingAverage(avgW.nodesCreated, newW.nodesCreated, n);
    avgW.nodesTBHits    = rollingAverage(avgW.nodesTBHits, newW.nodesTBHits, n);
    avgW.nodesCacheHits    = rollingAverage(avgW.nodesCacheHits, newW.nodesCacheHits, n);
}

void UciEngine::sendBestMove(bool force)
{
    // We don't have a best move yet!
    if (m_lastInfo.bestMove.isEmpty()) {
        QString o = QString("No more time and no bestmove forced=%1!\n").arg(force ? "t" : "f");
        output(o);
        if (!force)
            return;
    }

    stopTheClock();

#if defined(DEBUG_TIME)
    qint64 t = m_clock->timeToDeadline();
    if (t < 0) {
        QString out;
        QTextStream stream(&out);
        stream << "info"
            << " deadline " << m_clock->deadline()
            << " timeBudgetExceeded " << qAbs(t)
            << endl;
        output(out);
    }
#endif

    if (Q_UNLIKELY(m_ioHandler))
        m_ioHandler->handleBestMove(m_lastInfo.bestMove);

    QString out;
    QTextStream stream(&out);
    if (m_lastInfo.ponderMove.isEmpty())
        stream << "bestmove " << m_lastInfo.bestMove << endl;
    else
        stream << "bestmove " << m_lastInfo.bestMove << " ponder " << m_lastInfo.ponderMove << endl;
    output(out);
    calculateRollingAverage(m_lastInfo);

    stopSearch(); // we block until the search has stopped
}

void UciEngine::sendInfo(const SearchInfo &info, bool isPartial)
{
    // Check if this is an expired search
    if (!m_clock->isActive())
        return;

    m_lastInfo = info;

    // Check if we've already exceeded time
    if (m_clock->hasExpired()) {
        sendBestMove(true /*force*/);
        return;
    }

    // Check if we've exceeded the ram limit for tree size
    const quint64 treeSizeLimit = Options::globalInstance()->option("TreeSize").value().toUInt() * quint64(1024) * quint64(1024);
    if (treeSizeLimit && quint64(info.workerInfo.nodesCreated) * sizeof(Node) > treeSizeLimit) {
        sendBestMove(true /*force*/);
        return;
    }

    // Otherwise begin updating info
    qint64 msecs = m_clock->elapsed();
    m_lastInfo.time = msecs;
    m_clock->updateDeadline(m_lastInfo, isPartial);

    if (isPartial && (msecs - m_timeAtLastProgress) < 2500)
        return;

    m_timeAtLastProgress = msecs;

    m_lastInfo.nps = qRound(qreal(m_lastInfo.nodes) / qMax(qint64(1), msecs) * 1000.0);
    m_lastInfo.rawnps = qRound(qreal(m_lastInfo.nodes) / qMax(qint64(1), msecs) * 1000.0);
    m_lastInfo.batchSize = 0;
    if (m_lastInfo.workerInfo.nodesEvaluated && m_lastInfo.workerInfo.numberOfBatches)
        m_lastInfo.batchSize = m_lastInfo.workerInfo.nodesEvaluated / m_lastInfo.workerInfo.numberOfBatches;

    if (Q_UNLIKELY(m_ioHandler))
        m_ioHandler->handleInfo(m_lastInfo);

    QString out;
    QTextStream stream(&out);

#if defined(DEBUG_TIME)
    stream << "info"
        << " trend " << trendToString(m_lastInfo.trend)
        << " trendDegree " << m_lastInfo.trendDegree
        << " trendFactor " << m_clock->trendFactor()
        << " deadline " << m_clock->deadline()
        << " timeToDeadline " << m_clock->timeToDeadline()
        << endl;
#endif

    if (m_debug) {
        stream << "info"
               << " isResume " << (m_lastInfo.isResume ? "true" : "false")
               << " rawnps " << m_lastInfo.rawnps
               << " efficiency " << m_lastInfo.workerInfo.nodesSearched / float(m_lastInfo.workerInfo.nodesEvaluated)
               << " nodesSearched " << m_lastInfo.workerInfo.nodesSearched
               << " nodesEvaluated " << m_lastInfo.workerInfo.nodesEvaluated
               << " nodesCreated " << m_lastInfo.workerInfo.nodesCreated
               << " nodesCacheHits " << m_lastInfo.workerInfo.nodesCacheHits
               << endl;
    }

    const Game &g = History::globalInstance()->currentGame();

    stream << "info"
           << " depth " << m_lastInfo.depth
           << " seldepth " << m_lastInfo.seldepth
           << " nodes " << m_lastInfo.nodes
           << " nps " << m_lastInfo.nps
           << " batchSize " << m_lastInfo.batchSize
           << " score " << m_lastInfo.score
           << " time " << m_lastInfo.time
           << " hashfull " << qRound(Hash::globalInstance()->percentFull(g.halfMoveNumber()) * 1000.0f)
           << " tbhits " << m_lastInfo.workerInfo.nodesTBHits
           << " pv " << m_lastInfo.pv
           << endl;

    output(out);

    // Stop at specific depth if requested
    if (m_depthTargeted != -1 && m_lastInfo.depth >= m_depthTargeted)
        sendBestMove(true /*force*/);
}

void UciEngine::sendAverages()
{
    QString out;
    QTextStream stream(&out);
    stream << "info averages"
           << " depth " << m_averageInfo.depth
           << " seldepth " << m_averageInfo.seldepth
           << " nodes " << m_averageInfo.nodes
           << " nps " << m_averageInfo.nps
           << " batchSize " << m_averageInfo.batchSize
           << " rawnps " << m_averageInfo.rawnps
           << " efficiency " << m_averageInfo.workerInfo.nodesSearched / float(m_averageInfo.workerInfo.nodesEvaluated)
           << " nodesSearched " << m_averageInfo.workerInfo.nodesSearched
           << " nodesEvaluated " << m_averageInfo.workerInfo.nodesEvaluated
           << " nodesCreated " << m_averageInfo.workerInfo.nodesCreated
           << " nodesTBHits " << m_averageInfo.workerInfo.nodesTBHits
           << " nodesCacheHits " << m_averageInfo.workerInfo.nodesCacheHits
           << endl;
    output(out);
}

void UciEngine::sendOptions()
{
    QVector<UciOption> options = Options::globalInstance()->options();
    QString out;
    QTextStream stream(&out);
    for (UciOption o : options)
        stream << o.toString();
    output(out);
}

void UciEngine::uciNewGame()
{
    //qDebug() << "uciNewGame";
    m_gameInitialized = true;

    Hash::globalInstance()->reset();
    NeuralNet::globalInstance()->reset();
    TB::globalInstance()->reset();
    m_searchEngine->reset();

    m_averageInfo = SearchInfo();
#if defined(AVERAGES)
    if (m_averageInfo.depth != -1)
        sendAverages();
#endif
}

void UciEngine::ponderHit()
{
    //qDebug() << "ponderHit";
    Q_ASSERT(m_searchEngine && m_gameInitialized);
    if (m_searchEngine)
        m_searchEngine->stopPonder();
}

void UciEngine::stop()
{
    //qDebug() << "stop";
    sendBestMove(true /*force*/);
}

void UciEngine::quit()
{
    //qDebug() << "quit";
#if defined(AVERAGES)
    sendAverages();
#endif
    Q_ASSERT(m_searchEngine && m_gameInitialized);
    if (m_searchEngine)
        m_searchEngine->stopPonder();
    QCoreApplication::instance()->quit();
}

void UciEngine::setPosition(const QString& position, const QVector<QString> &moves)
{
    History::globalInstance()->clear();

    QString fen = QLatin1String("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (position != QLatin1String("startpos"))
        fen = position;

    if (!moves.isEmpty()) {
        Game game(fen);
        QVector<QString> movesMinusLast = moves;
        for (QString move : movesMinusLast) {
            Move mv = Notation::stringToMove(move, Chess::Computer);
            bool success = game.makeMove(mv);
            History::globalInstance()->addGame(game);
            Q_ASSERT(success);
        }
    } else {
        History::globalInstance()->addGame(Game(fen));
    }
}

int getNextIntAfterSearch(const QList<QString> strings, QString search)
{
    int result = -1;
    int index = -1;
    if ((index = strings.indexOf(search)) != -1) {
        if (index++ < strings.count()) {
            bool ok;
            int num = strings.at(index).toInt(&ok);
            if (ok)
                result = num;
        }
    }
    return result;
}

void UciEngine::parseGo(const QString &line)
{
    QList<QString> goLine = line.split(' ');
    int index = -1;

    Search search;
    if ((index = goLine.indexOf("searchmoves")) != -1) {
        while (index++ < goLine.count()) {
            QString move = goLine.at(index);
            if (move.count() < 4 || !move.at(0).isLetter() || !move.at(1).isNumber())
                break;

            Move mv = Notation::stringToMove(move, Chess::Computer);
            if (mv.isValid())
                search.searchMoves << mv;
        }
    }

    if (goLine.contains("ponder")) {
        Q_ASSERT(m_searchEngine && m_gameInitialized);
        if (m_searchEngine)
            m_searchEngine->startPonder();
    }

    search.wtime = getNextIntAfterSearch(goLine, "wtime");
    search.btime = getNextIntAfterSearch(goLine, "btime");
    search.winc = getNextIntAfterSearch(goLine, "winc");
    search.binc = getNextIntAfterSearch(goLine, "binc");
    search.movestogo = getNextIntAfterSearch(goLine, "movestogo");
    search.depth = getNextIntAfterSearch(goLine, "depth");
    search.nodes = getNextIntAfterSearch(goLine, "nodes");
    search.mate = getNextIntAfterSearch(goLine, "mate");
    search.movetime = getNextIntAfterSearch(goLine, "movetime");
    search.infinite = goLine.contains("infinite");
    search.game = History::globalInstance()->currentGame();

    go(search);
}

void UciEngine::parseOption(const QString &line)
{
    QList<QString> optionLine = line.split(' ');
    if (optionLine.count() != 5)
        return;

    if (optionLine.at(1) != QLatin1String("name"))
        return;

    QString name = optionLine.at(2);
    if (!Options::globalInstance()->contains(name))
        return;

    if (optionLine.at(3) != QLatin1String("value"))
        return;

    QString value = optionLine.at(4);
    Options::globalInstance()->setOption(name, value);
}

void UciEngine::go(const Search& s)
{
    //qDebug() << "go";
    if (!m_gameInitialized)
        uciNewGame();

    // Start the clock immediately
    m_clock->setTime(Chess::White, s.wtime);
    m_clock->setTime(Chess::Black, s.btime);
    m_clock->setIncrement(Chess::White, s.winc);
    m_clock->setIncrement(Chess::Black, s.binc);
    m_clock->setMoveTime(s.movetime);
    m_clock->setInfinite(s.infinite || s.depth != -1);
    m_clock->setMaterialScore(s.game.materialScore(Chess::White) + s.game.materialScore(Chess::Black));
    m_clock->setHalfMoveNumber(s.game.halfMoveNumber());
    m_clock->startDeadline(s.game.activeArmy());
    m_timeAtLastProgress = 0;
    m_depthTargeted = s.depth;
    m_lastInfo = SearchInfo();

    startSearch(s);
}

void UciEngine::input(const QString &in)
{
    if (Q_LIKELY(!m_ioHandler))
        qCInfo(UciInput).noquote() << in;
}

void UciEngine::output(const QString &out)
{
    if (Q_LIKELY(!m_ioHandler))
        qCInfo(UciOutput).noquote() << out;
    emit sendOutput(out);
}

void IOHandler::handleInfo(const SearchInfo &) {}
void IOHandler::handleBestMove(const QString &) {}

