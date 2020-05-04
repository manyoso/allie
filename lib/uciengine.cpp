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

#include "cache.h"
#include "chess.h"
#include "clock.h"
#include "game.h"
#include "history.h"
#include "nn.h"
#include "notation.h"
#include "options.h"
#include "searchengine.h"
#include "tb.h"

static bool s_firstLog = true;

//#define DEBUG_TIME

using namespace Chess;

UciOption::UciOption()
    : m_name(QString()),
      m_type(String),
      m_default(QString()),
      m_min(QString()),
      m_max(QString()),
      m_var(QVector<QString>()),
      m_value(QString()),
      m_valueType(QString())
{
}

QString UciOption::toString() const {
    QStringList list;
    list << "option" << "name" << m_name << "type";
    switch (m_type) {
    case Check:
        {
            list << "check";
            list << "default" << m_value;
            break;
        }
    case Spin:
        {
            list << "spin";
            list << "default" << m_value;
            list << "min" << m_min;
            list << "max" << m_max;

            break;
        }
    case Combo:
        {
            list << "combo";
            list << "default" << m_value;
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
            list << "default" << m_value;
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
        desc = QString("%0\n [MIN:%1, MAX:%2, DEFAULT:%3]\n").arg(m_description).arg(m_min).arg(m_max).arg(m_default);
    else if (!m_var.isEmpty())
        desc = QString("%0\n [%1, DEFAULT:%2]\n").arg(m_description).arg(m_var.toList().join(", ")).arg(m_default);
    else
        desc = QString("%0\n [DEFAULT:%1]\n").arg(m_description).arg(m_default);
    return QCommandLineOption(toCamelCase(m_name), desc, m_valueType, m_default);
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

    const bool debugLog = Options::globalInstance()->option("DebugLog").value() == "true";
    if (debugLog) {
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
    }
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
    if (m_debugLines.isEmpty())
        readyRead();

    QVector<QString> input;

    // Either we are out of lines or first line should be input
    bool isInputMode = m_debugLines.isEmpty() || m_debugLines.first().startsWith(QLatin1String("Input: "));
    while (!m_debugLines.isEmpty()) {
        QString peek = m_debugLines.first();
        if (!isInputMode && !input.isEmpty() && peek.startsWith(QLatin1String("Input: ")))
            break;

        QString line = m_debugLines.dequeue();
        if (line.startsWith(QLatin1String("Output: "))) {
            line.remove(0, 8);
            isInputMode = false;
        } else if (line.startsWith(QLatin1String("Input: "))) {
            line.remove(0, 7);
            isInputMode = true;
        }

        if (isInputMode)
            input.append(line);
        else
            m_waitingOnOutput = line;
    }


    for (QString line : input) {
        fprintf(stderr, "%s\n", line.toLatin1().constData());
        emit standardInput(line);
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
    m_averageInfoN(0),
    m_minBatchesForAverage(0),
    m_gameInitialized(false),
    m_pendingBestMove(false),
    m_debugFile(debugFile),
    m_searchEngine(nullptr),
    m_clock(new Clock(this)),
    m_ioHandler(nullptr)
{
    m_searchEngine = new SearchEngine(this);
    connect(m_searchEngine, &SearchEngine::sendInfo, this, &UciEngine::sendInfo);
    connect(m_searchEngine, &SearchEngine::requestStop, this, &UciEngine::stopRequested);
    connect(m_clock, &Clock::timeout, this, &UciEngine::sendBestMove);
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
                SearchSettings::debugInfo = true;
            else if (debug.at(1) == "off")
                SearchSettings::debugInfo = false;
        } else {
            SearchSettings::debugInfo = true;
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
        const StandaloneGame game = History::globalInstance()->currentGame();
        output(game.stateOfGameToFen() + "\n");
    } else if (line.startsWith("tree")) {
        int depth = 1;
        QVector<QString> node;
        const bool printPotentials = line.startsWith("treep");

        QList<QString> tree = line.split(' ');
        tree.pop_front();
        for (QString arg : tree) {
            bool success;
            int d  = arg.toInt(&success);
            if (success) {
                depth = d;
                break;
            } else {
                node << arg;
            }
        }

        if (m_searchEngine)
            m_searchEngine->printTree(node, depth, printPotentials /*printPotentials*/);
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

void UciEngine::calculateRollingAverage()
{
    ++m_averageInfoN;
    int n = m_averageInfoN;

    // Don't average the first sample
    if (n < 2) {
        m_averageInfo = m_lastInfo;
        return;
    }

    m_averageInfo.depth             = rollingAverage(m_averageInfo.depth, m_lastInfo.depth, n);
    m_averageInfo.seldepth          = rollingAverage(m_averageInfo.seldepth, m_lastInfo.seldepth, n);
    m_averageInfo.nodes             = rollingAverage(m_averageInfo.nodes, m_lastInfo.nodes, n);
    m_averageInfo.batchSize         = rollingAverage(m_averageInfo.batchSize, m_lastInfo.batchSize, n);

    if (m_lastInfo.workerInfo.numberOfBatches >= m_minBatchesForAverage) {
        Q_ASSERT(m_lastInfo.rawnps > 0);
        m_averageInfo.nps               = rollingAverage(m_averageInfo.nps, m_lastInfo.nps, n);
        m_averageInfo.rawnps            = rollingAverage(m_averageInfo.rawnps, m_lastInfo.rawnps, n);
        m_averageInfo.nnnps             = rollingAverage(m_averageInfo.nnnps, m_lastInfo.nnnps, n);
    }

    WorkerInfo &avgW = m_averageInfo.workerInfo;
    const WorkerInfo &newW = m_lastInfo.workerInfo;
    avgW.nodesSearched     = rollingAverage(avgW.nodesSearched, newW.nodesSearched, n);
    avgW.nodesEvaluated    = rollingAverage(avgW.nodesEvaluated, newW.nodesEvaluated, n);
    avgW.nodesVisited      = rollingAverage(avgW.nodesVisited, newW.nodesVisited, n);
    avgW.nodesTBHits       = rollingAverage(avgW.nodesTBHits, newW.nodesTBHits, n);
    avgW.nodesCacheHits    = rollingAverage(avgW.nodesCacheHits, newW.nodesCacheHits, n);
}

void UciEngine::sendBestMove()
{
    // We don't have a best move yet!
    if (m_lastInfo.bestMove.isEmpty()) {
        m_pendingBestMove = true;
        return;
    }

    Q_ASSERT(!m_searchEngine->isStopped());
    Q_ASSERT(m_clock->isActive());

    stopTheClock();

    const qint64 extraBudgetedTime = qMax(qint64(0), m_clock->timeToDeadline());
    const qint64 deadline = qMax(qint64(0), m_clock->deadline());
    m_clock->setExtraBudgetedTime(!deadline ? 0 :
        extraBudgetedTime / float(m_clock->deadline()) /
            float(SearchSettings::openingTimeFactor));

#if defined(DEBUG_TIME)
    {
        QString out;
        QTextStream stream(&out);
        stream << "info"
               << " extraBudgetedTime " << extraBudgetedTime << " as percent " << m_clock->extraBudgetedTime()
               << endl;
        output(out);
    }
#endif

    if (Q_UNLIKELY(m_ioHandler))
        m_ioHandler->handleBestMove(m_lastInfo.bestMove);

#if defined(DEBUG_TIME)
    // This should only happen when we are completely out of time
    if (!m_lastInfo.bestIsMostVisited) {
        QString out;
        QTextStream stream(&out);
        stream << "info bestIsMostVisited " << (m_lastInfo.bestIsMostVisited ? "true" : "false") << endl;
        output(out);
    }
#endif

    QString out;
    QTextStream stream(&out);
    if (m_lastInfo.ponderMove.isEmpty())
        stream << "bestmove " << m_lastInfo.bestMove << endl;
    else
        stream << "bestmove " << m_lastInfo.bestMove << " ponder " << m_lastInfo.ponderMove << endl;
    output(out);

    stopSearch(); // we block until the search has stopped

    m_pendingBestMove = false;

    calculateRollingAverage();
    if (Q_UNLIKELY(m_ioHandler))
        m_ioHandler->handleAverages(m_averageInfo);
}

void UciEngine::sendInfo(const SearchInfo &info, bool isPartial)
{
    // Check if this is an expired search
    if (!m_clock->isActive())
        return;

    m_lastInfo = info;

    // Otherwise begin updating info
    qint64 msecs = m_clock->elapsed();
    m_lastInfo.calculateSpeeds(msecs);

    // Check if we are in extended mode and best has become most visited
    if (m_clock->isExtended() && m_lastInfo.bestIsMostVisited) {
        sendBestMove();
        return;
    }

    // Check if we've already exceeded time
    if (m_clock->hasExpired()) {
        sendBestMove();
        return;
    }

    // Check if we are pending best move that has now been met
    if (m_pendingBestMove && !m_lastInfo.bestMove.isEmpty()) {
        sendBestMove();
        return;
    }

    Q_ASSERT(!m_searchEngine->isStopped());
    m_clock->updateDeadline(m_lastInfo, isPartial);

    const bool targetReached = m_lastInfo.isDTZ || (m_lastInfo.workerInfo.hasTarget && m_lastInfo.workerInfo.targetReached);

    // Set the estimated number of nodes to be searched under deadline if we've been searching for
    // at least N msecs and we want to early exit according to following paper:
    // https://link.springer.com/chapter/10.1007/978-3-642-31866-5_4
    const bool hasTarget = m_lastInfo.workerInfo.hasTarget;

    if (!hasTarget && !m_clock->isInfinite() && !m_clock->isMoveTime() && m_averageInfo.nodes > 0 && m_averageInfo.rawnps > 0) {
        const qint64 timeToRemaining = m_clock->deadline() - msecs;
        const quint32 e = qMax(quint32(1), quint32(timeToRemaining / 1000.0f * m_averageInfo.rawnps));
        m_searchEngine->setEstimatedNodes(e);
    }

    m_lastInfo.batchSize = 0;
    if (m_lastInfo.workerInfo.nodesEvaluated && m_lastInfo.workerInfo.numberOfBatches)
        m_lastInfo.batchSize = m_lastInfo.workerInfo.nodesEvaluated / m_lastInfo.workerInfo.numberOfBatches;

    if (Q_UNLIKELY(m_ioHandler))
        m_ioHandler->handleInfo(m_lastInfo, isPartial);

    QString out;
    QTextStream stream(&out);

#if defined(DEBUG_TIME)
    stream << "info"
        << " deadline " << m_clock->deadline()
        << " timeToDeadline " << m_clock->timeToDeadline()
        << endl;
#endif

    if (SearchSettings::debugInfo) {
        stream << "info"
               << " isResume " << (m_lastInfo.isResume ? "true" : "false")
               << " batchSize " << m_lastInfo.batchSize
               << " rawnps " << m_lastInfo.rawnps
               << " nnnps " << m_lastInfo.nnnps
               << " efficiency " << m_lastInfo.workerInfo.nodesVisited / float(m_lastInfo.workerInfo.nodesEvaluated)
               << " nodesSearched " << m_lastInfo.workerInfo.nodesSearched
               << " nodesEvaluated " << m_lastInfo.workerInfo.nodesEvaluated
               << " nodesVisited " << m_lastInfo.workerInfo.nodesVisited
               << " nodesCacheHits " << m_lastInfo.workerInfo.nodesCacheHits
               << endl;
    }

    const Game g = History::globalInstance()->currentGame();

    stream << "info"
           << " depth " << m_lastInfo.depth
           << " seldepth " << m_lastInfo.seldepth
           << " nodes " << m_lastInfo.nodes
           << " nps " << m_lastInfo.nps
           << " score " << m_lastInfo.score
           << " time " << m_lastInfo.time
           << " hashfull " << qRound(Cache::globalInstance()->percentFull(g.halfMoveNumber()) * 1000.0f)
           << " tbhits " << m_lastInfo.workerInfo.nodesTBHits
           << " pv " << m_lastInfo.pv
           << endl;

    Q_ASSERT(m_lastInfo.depth > 0);
    Q_ASSERT(m_lastInfo.seldepth > 0);
    Q_ASSERT(m_lastInfo.nodes > 0);
    Q_ASSERT(m_lastInfo.time >= 0);

    Q_ASSERT(m_clock->isActive());
    output(out);

    // Stop at specific targets if requested or if we have a dtz move
    if (targetReached)
        sendBestMove();
}

void UciEngine::sendAverages()
{
    QString out;
    QTextStream stream(&out);
    stream << "info averages"
           << " games " << m_averageInfo.games
           << " depth " << m_averageInfo.depth
           << " seldepth " << m_averageInfo.seldepth
           << " nodes " << m_averageInfo.nodes
           << " nps " << m_averageInfo.nps
           << " rawnps " << m_averageInfo.rawnps
           << " nnnps " << m_averageInfo.nnnps
           << " batchSize " << m_averageInfo.batchSize
           << " efficiency " << m_averageInfo.workerInfo.nodesSearched / float(m_averageInfo.workerInfo.nodesEvaluated)
           << " nodesSearched " << m_averageInfo.workerInfo.nodesSearched
           << " nodesEvaluated " << m_averageInfo.workerInfo.nodesEvaluated
           << " nodesVisited " << m_averageInfo.workerInfo.nodesVisited
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
    m_pendingBestMove = false;

    m_clock->setExtraBudgetedTime(0.f);
    m_searchEngine->reset();
    Cache::globalInstance()->reset();
    SearchSettings::debugInfo = Options::globalInstance()->option("DebugInfo").value() == "true";
    SearchSettings::chess960 = Options::globalInstance()->option("UCI_Chess960").value() == "true";
    SearchSettings::weightsFile = Options::globalInstance()->option("WeightsFile").value();
    SearchSettings::openingTimeFactor = Options::globalInstance()->option("OpeningTimeFactor").value().toDouble();
    SearchSettings::earlyExitFactor = Options::globalInstance()->option("EarlyExitFactor").value().toDouble();
    Q_ASSERT(!SearchSettings::weightsFile.isEmpty());
    NeuralNet::globalInstance()->setWeights(SearchSettings::weightsFile);
    NeuralNet::globalInstance()->reset();

    // Don't average the nps unless we have at least two batches from each GPU
    const int numberOfGPUCores = Options::globalInstance()->option("GPUCores").value().toInt();
    m_minBatchesForAverage = numberOfGPUCores * 2;

    TB::globalInstance()->reset();
    ++m_averageInfo.games;
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
    if (m_clock->isActive() && !m_searchEngine->isStopped())
        sendBestMove();
}

void UciEngine::stopRequested(bool earlyExit)
{
#if defined(DEBUG_TIME)
    if (earlyExit) {
        QString out;
        QTextStream stream(&out);
        stream << "info"
               << " stopRequested estimatedNodes " << m_searchEngine->estimatedNodes()
               << " rawnps " << m_averageInfo.rawnps
               << endl;
        output(out);
    }
#else
    Q_UNUSED(earlyExit);
#endif

    //qDebug() << "stop";
    stop();
}

void UciEngine::quit()
{
    //qDebug() << "quit";
    Q_ASSERT(m_searchEngine);
    if (m_searchEngine && m_gameInitialized) {
        if (SearchSettings::debugInfo)
            sendAverages();
        m_searchEngine->stopSearch();
        m_searchEngine->stopPonder();
    }
    QCoreApplication::instance()->quit();
}

void UciEngine::setPosition(const QString& position, const QVector<QString> &moves)
{
    History::globalInstance()->clear();

    QString fen = QLatin1String("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (position != QLatin1String("startpos"))
        fen = position;

    if (!moves.isEmpty()) {
        StandaloneGame game(fen);
        QVector<QString> movesMinusLast = moves;
        for (QString move : movesMinusLast) {
            Move mv = Notation::stringToMove(move, Chess::Computer);
            bool success = game.makeMove(mv);
            History::globalInstance()->addGame(game);
            Q_ASSERT(success);
        }
    } else {
        History::globalInstance()->addGame(StandaloneGame(fen));
    }
}

int getNextIntAfterSearch(const QList<QString> strings, QString search)
{
    int result = -1;
    int index = -1;
    if ((index = strings.indexOf(search)) != -1) {
        if (++index < strings.count()) {
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
        while (++index < goLine.count()) {
            QString move = goLine.at(index);
            if (move.count() < 4 || !move.at(0).isLetter() || !move.at(1).isNumber())
                break;

            Move mv = Notation::stringToMove(move, Chess::Computer);
            if (mv.isValid())
                search.searchMoves << move;
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
    Q_ASSERT(m_searchEngine->isStopped());

    //qDebug() << "go";
    if (!m_gameInitialized)
        uciNewGame();

    const StandaloneGame currentGame = History::globalInstance()->currentGame();
    const Game::Position &p = currentGame.position();
    // Start the clock immediately
    m_clock->setTime(Chess::White, s.wtime);
    m_clock->setTime(Chess::Black, s.btime);
    m_clock->setIncrement(Chess::White, s.winc);
    m_clock->setIncrement(Chess::Black, s.binc);
    m_clock->setMoveTime(s.movetime);
    m_clock->setInfinite(s.infinite || s.depth != -1 || s.nodes != -1);
    m_clock->setMaterialScore(p.materialScore(Chess::White) + p.materialScore(Chess::Black));
    m_clock->setHalfMoveNumber(currentGame.halfMoveNumber());
    m_clock->resetExtension();
    m_lastInfo = SearchInfo();

    // Actually start the clock
    m_clock->startDeadline(p.activeArmy());
#if defined(DEBUG_TIME)
    QString out;
    QTextStream stream(&out);
    stream << "info"
           << " clock deadline " << m_clock->deadline() << " extra time " << m_clock->extraBudgetedTime()
           << endl;
    output(out);
#endif
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

void IOHandler::handleInfo(const SearchInfo &, bool) {}
void IOHandler::handleBestMove(const QString &) {}
void IOHandler::handleAverages(const SearchInfo &) {}

