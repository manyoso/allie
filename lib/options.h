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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <QtGlobal>

#include "uciengine.h"

class Options {
public:
    static Options *globalInstance();

    bool contains(const QString &name) const;
    UciOption option(const QString &name) const;
    void setOption(const QString &name, const QString &value);
    QVector<UciOption> options() const;
    void addRegularOptions();
    void addBenchmarkOptions();

private:
    Options();
    ~Options();
    void insertOption(const UciOption &option);
    QVector<UciOption> m_optionsInOrder;
    QMap<QString, UciOption> m_options;
    friend class MyOptions;
};

#endif // OPTIONS_H
