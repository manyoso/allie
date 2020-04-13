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
#include "neural/loader.h"

#include "cache.h"
#include "node.h"
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
    // Set the weights file default
    SearchSettings::weightsFile = QString::fromStdString(lczero::DiscoverWeightsFile());

    UciOption cpuctBase;
    cpuctBase.m_name = QLatin1Literal("CpuctBase");
    cpuctBase.m_type = UciOption::String;
    cpuctBase.m_default = QString::number(double(SearchSettings::cpuctBase));
    cpuctBase.m_value = cpuctBase.m_default;
    cpuctBase.m_min = QLatin1Literal("0");
    cpuctBase.m_max = QLatin1Literal("100000");
    cpuctBase.m_description = QLatin1String("Cpuct base");
    insertOption(cpuctBase);

    UciOption cpuctF;
    cpuctF.m_name = QLatin1Literal("CpuctF");
    cpuctF.m_type = UciOption::String;
    cpuctF.m_default = QString::number(double(SearchSettings::cpuctF));
    cpuctF.m_value = cpuctF.m_default;
    cpuctF.m_min = QLatin1Literal("1");
    cpuctF.m_max = QLatin1Literal("256");
    cpuctF.m_description = QLatin1String("Cpuct growth factor");
    insertOption(cpuctF);

    UciOption cpuctInit;
    cpuctInit.m_name = QLatin1Literal("CpuctInit");
    cpuctInit.m_type = UciOption::String;
    cpuctInit.m_default = QString::number(double(SearchSettings::cpuctInit));
    cpuctInit.m_value = cpuctInit.m_default;
    cpuctInit.m_min = QLatin1Literal("1");
    cpuctInit.m_max = QLatin1Literal("256");
    cpuctInit.m_description = QLatin1String("Cpuct initial value");
    insertOption(cpuctInit);

    UciOption debugLog;
    debugLog.m_name = QLatin1Literal("DebugLog");
    debugLog.m_type = UciOption::Check;
    debugLog.m_default = QLatin1Literal("false");
    debugLog.m_value = debugLog.m_default;
    debugLog.m_description = QLatin1String("Output a debug log in binary directory");
    insertOption(debugLog);

    UciOption debugInfo;
    debugInfo.m_name = QLatin1Literal("DebugInfo");
    debugInfo.m_type = UciOption::Check;
    debugInfo.m_default = QLatin1Literal("false");
    debugInfo.m_value = debugInfo.m_default;
    debugInfo.m_description = QLatin1String("Output additional debug info");
    insertOption(debugInfo);
  
    UciOption earlyExitScalar;
    earlyExitScalar.m_name = QLatin1Literal("earlyExitScalar");
    earlyExitScalar.m_type =  UciOption::String;
    earlyExitScalar.m_default = QString::number(double(SearchSettings::earlyExitScalar));
    earlyExitScalar.m_value = earlyExitScalar.m_default;
    earlyExitScalar.m_min = QLatin1Literal("-1.5");
    earlyExitScalar.m_max = QLatin1Literal("1.5");
    earlyExitScalar.m_description = QLatin1String("Scalar for early exit where values less than"
                                                  " one make instamoves more common and larger than"
                                                  " one make instamove less common.");
    insertOption(earlyExitScalar);
  
    UciOption earlyExitFactor;
    earlyExitFactor.m_name = QLatin1Literal("EarlyExitFactor");
    earlyExitFactor.m_type =  UciOption::String;
    earlyExitFactor.m_default = QString::number(double(SearchSettings::earlyExitFactor));
    earlyExitFactor.m_value = earlyExitFactor.m_default;
    earlyExitFactor.m_min = QLatin1Literal("-1.5");
    earlyExitFactor.m_max = QLatin1Literal("1.5");
    earlyExitFactor.m_description = QLatin1String("Multiplier for early exit where values less than"
                                                  " one make instamoves more common and larger than"
                                                  " one make instamove less common.");
    insertOption(earlyExitFactor);

    UciOption earlyExitMinimum;
    earlyExitMinimum.m_name = QLatin1Literal("EarlyExitMinimum");
    earlyExitMinimum.m_type =  UciOption::Spin;
    earlyExitMinimum.m_default = QString::number(SearchSettings::earlyExitMinimumTime);
    earlyExitMinimum.m_value = earlyExitMinimum.m_default;
    earlyExitMinimum.m_min = QLatin1Literal("0");
    earlyExitMinimum.m_max = QLatin1Literal("5000");
    earlyExitMinimum.m_description = QLatin1String("Minimum time in milliseconds before any early"
                                                   " exit or instamove is allowed.");
    insertOption(earlyExitMinimum);

    UciOption featuresOff;
    featuresOff.m_name = QLatin1Literal("FeaturesOff");
    featuresOff.m_type = UciOption::String;
    featuresOff.m_default = SearchSettings::featuresToString(SearchSettings::featuresOff);
    featuresOff.m_value = featuresOff.m_default;
    featuresOff.m_description = QLatin1String("Specify features to turn off for debugging purposes"
                                              " as a comma delineated list without spaces in lower"
                                              "case including 'threading,earlyexit,transpositions,"
                                              "minimax'");
    insertOption(featuresOff);

    UciOption GPUCores;
    GPUCores.m_name = QLatin1Literal("GPUCores");
    GPUCores.m_type = UciOption::Spin;
    GPUCores.m_default = QLatin1Literal("1");
    GPUCores.m_value = GPUCores.m_default;
    GPUCores.m_min = QLatin1Literal("0");
    GPUCores.m_max = QLatin1Literal("256");
    GPUCores.m_description = QLatin1String("Number of GPU cores to use");
    insertOption(GPUCores);

    UciOption cache;
    cache.m_name = QLatin1Literal("Cache");
    cache.m_type = UciOption::Spin;
    cache.m_default = QLatin1Literal("5000000");
    cache.m_value = cache.m_default;
    cache.m_min = QLatin1Literal("100000");
    cache.m_max = QString::number(999999999);
    cache.m_description = QLatin1String("Maximum number of chess positions stored in memory");
    insertOption(cache);

    UciOption maxBatchSize;
    maxBatchSize.m_name = QLatin1Literal("MaxBatchSize");
    maxBatchSize.m_type = UciOption::Spin;
    maxBatchSize.m_default = QLatin1Literal("256");
    maxBatchSize.m_value = maxBatchSize.m_default;
    maxBatchSize.m_min = QLatin1Literal("0");
    maxBatchSize.m_max = QLatin1Literal("65536");
    maxBatchSize.m_description = QLatin1String("Largest batch to send to GPU");
    insertOption(maxBatchSize);

    UciOption moveOverhead;
    moveOverhead.m_name = QLatin1Literal("MoveOverhead");
    moveOverhead.m_type = UciOption::Spin;
    moveOverhead.m_default = QLatin1Literal("300");
    moveOverhead.m_value = moveOverhead.m_default;
    moveOverhead.m_min = QLatin1Literal("0");
    moveOverhead.m_max = QLatin1Literal("5000");
    moveOverhead.m_description = QLatin1String("Overhead to avoid timing out");
    insertOption(moveOverhead);

    UciOption openingTimeFactor;
    openingTimeFactor.m_name = QLatin1Literal("OpeningTimeFactor");
    openingTimeFactor.m_type =  UciOption::String;
    openingTimeFactor.m_default = QString::number(double(SearchSettings::openingTimeFactor));
    openingTimeFactor.m_value = openingTimeFactor.m_default;
    openingTimeFactor.m_min = QLatin1Literal("1");
    openingTimeFactor.m_max = QLatin1Literal("3");
    openingTimeFactor.m_description = QLatin1String("Time factor for extra time in opening");
    insertOption(openingTimeFactor);

    UciOption ponder;
    ponder.m_name = QLatin1Literal("Ponder");
    ponder.m_type = UciOption::Check;
    ponder.m_default = QLatin1Literal("false");
    ponder.m_value = ponder.m_default;
    ponder.m_description = QLatin1String("Whether to ponder");
    insertOption(ponder);

    UciOption tb;
    tb.m_name = QLatin1Literal("SyzygyPath");
    tb.m_type = UciOption::String;
    tb.m_default = QLatin1Literal("");
    tb.m_value = tb.m_default;
    tb.m_description = QLatin1String("Path to the syzygy tablebase");
    insertOption(tb);

    UciOption tryPlayoutLimit;
    tryPlayoutLimit.m_name = QLatin1Literal("TryPlayoutLimit");
    tryPlayoutLimit.m_type = UciOption::Spin;
    tryPlayoutLimit.m_default = QString::number(SearchSettings::tryPlayoutLimit);
    tryPlayoutLimit.m_value = tryPlayoutLimit.m_default;
    tryPlayoutLimit.m_min = QLatin1Literal("1");
    tryPlayoutLimit.m_max = QLatin1Literal("1000");
    tryPlayoutLimit.m_description = QLatin1String("Number of times that a playout with virtual loss"
                                                  " should be retried to grow the batchSize before"
                                                  " giving up.");
    insertOption(tryPlayoutLimit);

    UciOption ninesixty;
    ninesixty.m_name = QLatin1Literal("UCI_Chess960");
    ninesixty.m_type = UciOption::Check;
    ninesixty.m_default = QLatin1Literal("false");
    ninesixty.m_value = ninesixty.m_default;
    ninesixty.m_description = QLatin1String("Play Chess960");
    insertOption(ninesixty);

    UciOption useHalfFloatingPoint;
    useHalfFloatingPoint.m_name = QLatin1Literal("UseFP16");
    useHalfFloatingPoint.m_type = UciOption::Check;
    useHalfFloatingPoint.m_default = QLatin1Literal("false");
    useHalfFloatingPoint.m_value = useHalfFloatingPoint.m_default;
    useHalfFloatingPoint.m_description = QLatin1String("Use half floating point on GPU");
    insertOption(useHalfFloatingPoint);

    UciOption weightsFile;
    weightsFile.m_name = QLatin1Literal("WeightsFile");
    weightsFile.m_type = UciOption::String;
    weightsFile.m_default = SearchSettings::weightsFile;
    weightsFile.m_value = weightsFile.m_default;
    weightsFile.m_description = QLatin1String("Provides a weights file to use");
    insertOption(weightsFile);
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
