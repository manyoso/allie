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

#include "options.h"

#include "hash.h"
#include "nn.h"
#include "tb.h"

class MyOptions : public Options { };
Q_GLOBAL_STATIC(MyOptions, OptionsInstance)
Options* Options::globalInstance()
{
    return OptionsInstance();
}

Options::Options()
{
    UciOption hash;
    hash.m_name = QLatin1Literal("Hash");
    hash.m_type = UciOption::Spin;
    hash.m_default = QLatin1Literal("20");
    hash.m_value = hash.m_default;
    hash.m_min = QLatin1Literal("0");
    hash.m_max = QLatin1Literal("65536");
    hash.m_description = QLatin1String("Size of the hash in MB");
    insertOption(hash);

    UciOption treeSize;
    treeSize.m_name = QLatin1Literal("TreeSize");
    treeSize.m_type = UciOption::Spin;
    treeSize.m_default = QLatin1Literal("0");
    treeSize.m_value = treeSize.m_default;
    treeSize.m_min = QLatin1Literal("0");
    treeSize.m_max = QLatin1Literal("65536");
    treeSize.m_description = QLatin1String("Limit the size of the tree in MB");
    insertOption(treeSize);

    UciOption moveOverhead;
    moveOverhead.m_name = QLatin1Literal("MoveOverhead");
    moveOverhead.m_type = UciOption::Spin;
    moveOverhead.m_default = QLatin1Literal("300");
    moveOverhead.m_value = moveOverhead.m_default;
    moveOverhead.m_min = QLatin1Literal("0");
    moveOverhead.m_max = QLatin1Literal("5000");
    moveOverhead.m_description = QLatin1String("Overhead to avoid timing out");
    insertOption(moveOverhead);

    UciOption ponder;
    ponder.m_name = QLatin1Literal("Ponder");
    ponder.m_type = UciOption::Check;
    ponder.m_default = QLatin1Literal("false");
    ponder.m_value = ponder.m_default;
    ponder.m_description = QLatin1String("Whether to ponder");
    insertOption(ponder);

    UciOption useHalfFloatingPoint;
    useHalfFloatingPoint.m_name = QLatin1Literal("UseFP16");
    useHalfFloatingPoint.m_type = UciOption::Check;
    useHalfFloatingPoint.m_default = QLatin1Literal("false");
    useHalfFloatingPoint.m_value = useHalfFloatingPoint.m_default;
    useHalfFloatingPoint.m_description = QLatin1String("Use half floating point on GPU");
    insertOption(useHalfFloatingPoint);

    UciOption GPUCores;
    GPUCores.m_name = QLatin1Literal("GPUCores");
    GPUCores.m_type = UciOption::Spin;
    GPUCores.m_default = QLatin1Literal("1");
    GPUCores.m_value = GPUCores.m_default;
    GPUCores.m_min = QLatin1Literal("0");
    GPUCores.m_max = QLatin1Literal("256");
    GPUCores.m_description = QLatin1String("Number of GPU cores to use");
    insertOption(GPUCores);

    UciOption threads;
    threads.m_name = QLatin1Literal("Threads");
    threads.m_type = UciOption::Spin;
    threads.m_default = QLatin1Literal("1");
    threads.m_value = threads.m_default;
    threads.m_min = QLatin1Literal("0");
    threads.m_max = QLatin1Literal("256");
    threads.m_description = QLatin1String("Number of threads to use");
    insertOption(threads);

    UciOption maxBatchSize;
    maxBatchSize.m_name = QLatin1Literal("MaxBatchSize");
    maxBatchSize.m_type = UciOption::Spin;
    maxBatchSize.m_default = QLatin1Literal("256");
    maxBatchSize.m_value = maxBatchSize.m_default;
    maxBatchSize.m_min = QLatin1Literal("0");
    maxBatchSize.m_max = QLatin1Literal("65536");
    maxBatchSize.m_description = QLatin1String("Largest batch to send to GPU");
    insertOption(maxBatchSize);

    UciOption tb;
    tb.m_name = QLatin1Literal("SyzygyPath");
    tb.m_type = UciOption::String;
    tb.m_default = QLatin1Literal("");
    tb.m_value = tb.m_default;
    tb.m_description = QLatin1String("Path to the syzygy tablebase");
    insertOption(tb);
}

Options::~Options()
{
}

bool Options::contains(const QString &name) const
{
    return m_options.contains(name);
}

UciOption Options::option(const QString &name) const
{
    Q_ASSERT(contains(name));
    return m_options.value(name);
}

void Options::setOption(const QString &name, const QString &value)
{
    // FIXME: Need some validation!
    Q_ASSERT(contains(name));
    UciOption o = m_options.value(name);
    o.setValue(value);
    m_options.insert(name, o);
}

void Options::insertOption(const UciOption &option)
{
    m_options.insert(option.optionName(), option);
}

QVector<UciOption> Options::options() const
{
    return m_options.values().toVector();
}
