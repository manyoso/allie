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

#ifndef VERSION_H
#define VERSION_H

#include <QString>

static int s_majorVersion = 0;
static int s_minorVersion = 4;
static bool s_isDev = true;

static QString versionString()
{
    const QString maj = QString::number(s_majorVersion);
    const QString min = QString::number(s_minorVersion);
#if defined(GIT_SHA)
    const QString git = QString("(%0)").arg(GIT_SHA);
#else
    const QString git = QString();
#endif
    const QString dev = s_isDev ? QLatin1String("-dev") : QLatin1String("");
    return QString("v%0.%1%2 %3").arg(maj).arg(min).arg(dev).arg(git);
}

#endif // VERSION_H
