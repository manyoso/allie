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

#ifndef ALLIE_SHIM_H
#define ALLIE_SHIM_H

#include "allie_common.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <stdio.h>

struct OptionsDict
{
    int gpuId;
    int maxBatchSize;
    bool useCustomWinograd;
};

inline std::vector<std::string> GetFileList(const std::string &dir)
{
    QDir d(QString::fromStdString(dir));
    if (!d.exists())
        return std::vector<std::string>();
    QStringList list = d.entryList(QDir::Files | QDir::Readable);
    std::vector<std::string> result;
    for (QString str : list)
        result.push_back(str.toStdString());
    return result;
}

inline uint64_t GetFileSize(const std::string &file)
{
    QFileInfo info(QString::fromStdString(file));
    Q_ASSERT(info.exists());
    return uint64_t(info.size());
}

inline time_t GetFileTime(const std::string &file)
{
    QFileInfo info(QString::fromStdString(file));
    Q_ASSERT(info.exists());
    return info.lastModified().toTime_t();
}

#endif // ALLIE_SHIM_H
