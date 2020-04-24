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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <stdio.h>
#include <iostream>

#include "benchmarkengine.h"
#include "cache.h"
#include "movegen.h"
#include "nn.h"
#include "options.h"
#include "searchengine.h"
#include "uciengine.h"
#include "version.h"
#include "zobrist.h"

#define APP_NAME "Allie"

int main(int argc, char *argv[])
{
    qInstallMessageHandler(g_uciMessageHandler);

    QCoreApplication a(argc, argv);
    a.setApplicationName(APP_NAME);
    a.setApplicationVersion(versionString());
    a.setOrganizationName("Allie Chess Authors");

    enum Mode {
        UNKNOWN,
        UCI,
        BENCHMARK,
        DEBUGFILE
    };

    QCommandLineParser parser;
    parser.setApplicationDescription("A uci compliant chess engine.");
    parser.addVersionOption();
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    QCommandLineOption customHelp(QStringList{"h", "help"}, "Displays this help.");
    parser.addOption(customHelp);
    parser.addPositionalArgument("", "");
    parser.addPositionalArgument("mode", "uci\t\tRegular uci chess engine (default)\n\t"
                                         "benchmark\tBenchmarking mode\n\t"
                                         "debugfile\tReplay a debug log file\n");

    QCommandLineParser modeParser;
    modeParser.setApplicationDescription("mode");
    QCommandLineOption customModeHelp(QStringList{"h", "help"}, "Displays mode help.");
    customModeHelp.setFlags(QCommandLineOption::HiddenFromHelp);
    modeParser.addOption(customModeHelp);

    // Provide own parse of first parser
    QStringList args = a.arguments();

    // First value is always the app name
    args.pop_front();

    // Parse the help
    bool help = false;
    if (!args.isEmpty() && (args.first() == "--help" || args.first() == "--h")) {
        help = true;
        args.pop_front();
    }

    // Check the mode
    Mode mode = UCI;
    bool modeIsEmpty = args.isEmpty();
    QString modeString = modeIsEmpty ? QString() : args.first();
    if (modeString == QLatin1String("uci")) {
        mode = UCI;
    } else if (modeString == QLatin1String("benchmark")) {
        mode = BENCHMARK;
    } else if (modeString == QLatin1String("debugfile")) {
        mode = DEBUGFILE;
    } else {
        // Assume uci as that is default way of interpreting mode
        mode = UCI;
        modeIsEmpty = true;
        modeString = QLatin1String("uci");
    }

    // Add options depending upon mode
    switch (mode) {
    case BENCHMARK:
        Options::globalInstance()->addBenchmarkOptions();
        [[fallthrough]];
    case DEBUGFILE:
        modeParser.addPositionalArgument("filepath", "\t<filepath>\tThe filepath of the debug file to load");
        [[fallthrough]];
    case UCI:
        {
            Options::globalInstance()->addRegularOptions();
            QVector<UciOption> options = Options::globalInstance()->options();
            for (UciOption o : options)
                modeParser.addOption(o.commandLine());
            break;
        }
    case UNKNOWN:
        break;
    default:
        Q_UNREACHABLE();
    }

    // Fixup the mode help
    QString modeHelp = modeParser.helpText();
    modeHelp.remove(0, modeHelp.indexOf("mode\n\n") + 6);
    modeHelp.prepend("Mode ");

    QString fullHelp = QString("%0%1").arg(parser.helpText()).arg(modeHelp);

    // If we've requested --help as first arg display it now
    if (help) {
        std::cerr << fullHelp.toLatin1().constData();
        return -1;
    }

    // If we had a mode, then pop it so we can parse the rest of the options
    if (!modeIsEmpty)
        args.pop_front();

    // Need at least one positional argument which usually is the process name
    args.prepend("mode");

    // Process the mode args
    modeParser.process(args);

    // Check if we've invoked mode help
    if (modeParser.isSet("help")) {
        std::cerr << fullHelp.toLatin1().constData();
        return -1;
    }

    // Process the rest of our arguments
    QVector<UciOption> options = Options::globalInstance()->options();
    for (UciOption o : options) {
        QString option = UciOption::toCamelCase(o.optionName());
        if (modeParser.isSet(option))
            Options::globalInstance()->setOption(o.optionName(), modeParser.value(option));
    }

    // Is this debug mode?
    QString debugFile;
    QStringList modePositionalArgs = modeParser.positionalArguments();
    if (mode == DEBUGFILE && modePositionalArgs.count() == 1) {
            debugFile = modePositionalArgs.first();
    } else if (mode == DEBUGFILE || !modePositionalArgs.isEmpty()) {
        std::cerr << fullHelp.toLatin1().constData();
        return -1;
    }

    // Display our logo
    QString ascii = QLatin1String("       _ _ _       \n"
                                  "  __ _| | (_) ___  \n"
                                  " / _` | | | |/ _ \\\n"
                                  "| (_| | | | |  __/ \n"
                                  " \\__,_|_|_|_|\\___|");
    fprintf(stderr, "%s %s built on %s at %s\n", ascii.toLatin1().constData(),
        QString("%0").arg(a.applicationVersion()).toLatin1().constData(),
        QString("%0").arg(__DATE__).toLatin1().constData(),
        QString("%0").arg(__TIME__).toLatin1().constData());

    Zobrist::globalInstance();
    Movegen::globalInstance();

    // Is this benchmark mode?
    if (mode == BENCHMARK) {
        BenchmarkEngine engine(&a);
        engine.run();
        return a.exec();
    } else {
        UciEngine engine(&a, debugFile);
        engine.run();
        return a.exec();
    }
}
