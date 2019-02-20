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

#include <QtCore>
#include <QtTest/QtTest>

#include "hash.h"
#include "movegen.h"
#include "nn.h"
#include "testgames.h"
#include "zobrist.h"

#define APP_NAME "Allie"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(APP_NAME);

    // Order is important here
    QString weightsPath = QCoreApplication::applicationDirPath() + QDir::separator() + "weights.pb";
    NeuralNet::globalInstance()->setWeights(weightsPath);
    Zobrist::globalInstance();
    Movegen::globalInstance();

    int rc = 0;
    TestGames test1;
    rc = QTest::qExec(&test1, argc, argv) == 0 ? rc : -1;

    return rc;
}
