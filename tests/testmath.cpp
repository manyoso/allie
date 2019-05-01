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

#include <sys/time.h>
#include <math.h>

#include "testmath.h"

#include "fastapprox/fastonebigheader.h"

#define USE_EXACT

static void test_fastlog_once(double* erracc,
                              float* max,
                              float* argmax)
{
    float x = float(1e-10 + 10.0 * drand48());
    float exact = logf(x);
#if defined(USE_EXACT)
    float est = logf(x);
#else
    float est = fastlog(x);
#endif
    float err = fabsf (est - exact) /
            (fabsf (1e-4f) + fabsf (est) + fabsf (exact));
    if (err > *max) { *max = err; *argmax = x; }
    *erracc += double(err);
}

static double test_fastlog()
{
    unsigned int i;
    double err = 0;
    float argmax = 0;
    float max = 0;
    for (i = 0; i < 100000; ++i)
        test_fastlog_once(&err, &max, &argmax);
    err /= i;
    return err;
}

static void test_fastpow_once(double* erracc,
                              float* max,
                              float*  argmaxx,
                              float*  argmaxy)
{
    float x = float(3.0 * drand48()); /* ah ... the generation gap ... */
    float y = float(-3.0 + 6.0 * drand48());
    float exact = powf(x, y);
#if defined(USE_EXACT)
    float est = powf(x, y);
#else
    float est = fastpow(x, y);
#endif
    float err = fabsf (est - exact) /
            (fabsf (1e-4f) + fabsf (est) + fabsf (exact));

    if (err > *max) { *max = err; *argmaxx = x; *argmaxy = y; }
    *erracc += double(err);
}

static double test_fastpow()
{
    unsigned int i;
    double err = 0;
    float argmaxx = 0;
    float argmaxy = 0;
    float max = 0;
    for (i = 0; i < 100000; ++i)
        test_fastpow_once(&err, &max, &argmaxx, &argmaxy);
    err /= i;
    return err;
}

void TestMath::testFastLog()
{
    double error = test_fastlog();
    QVERIFY(error < 1e-4);

    QElapsedTimer timer;
    timer.start();
    unsigned int i;
    float sum = 0;
    volatile float xd = 1.0f;
    for (i = 0; i < 1000000000; ++i)
        sum += fastlog(xd);
//    qDebug() << "fastlog elapsed" << timer.elapsed() << "error" << error;
}

void TestMath::testFastPow()
{
    double error = test_fastpow();
    QVERIFY(error < 1e-4);

    QElapsedTimer timer;
    timer.start();
    unsigned int i;
    float sum = 0;
    volatile float xd = 1.0f;
    volatile float yd = 1.0f;
    for (i = 0; i < 1000000000; ++i)
        sum += fastpow(xd, yd);
//    qDebug() << "fastpow elapsed" << timer.elapsed() << "error" << error;
}
