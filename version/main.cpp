/*
  This file is part of Allie Chess.
  Copyright (C) 2020 Adam Treat

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
#include <QDir>
#include <QFile>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString oldVersion;
    QString newVersion = QString("static constexpr const char* s_gitversion = \"%1\";").arg(a.arguments().at(1));

    {
        QFile file(QDir::currentPath() + QDir::separator() + QLatin1String("gitversion.h"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            oldVersion = file.readAll();
    }

    if (oldVersion != newVersion) {
        QFile file(QDir::currentPath() + QDir::separator() + QLatin1String("gitversion.h"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return -1;

        QTextStream out(&file);
        out << newVersion;
    }
    return 0;
}
