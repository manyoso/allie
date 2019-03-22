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

#include "hash.h"
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
    a.setOrganizationName("Adam Treat");

    QCommandLineParser parser;
    parser.setApplicationDescription("A uci compliant chess engine.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "An optional debug file to open.");

    QVector<UciOption> options = Options::globalInstance()->options();
    for (UciOption o : options)
        parser.addOption(o.commandLine());

    parser.process(a);

    const QStringList args = parser.positionalArguments();
    if (args.count() > 1)
        parser.showHelp();

    QString debugFile;
    if (!args.isEmpty())
        debugFile = args.first();

    for (UciOption o : options) {
        QString option = UciOption::toCamelCase(o.optionName());
        if (parser.isSet(option))
            Options::globalInstance()->setOption(o.optionName(), parser.value(option));
    }

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

    UciEngine engine(&a, debugFile);
    engine.run();

    return a.exec();
}
