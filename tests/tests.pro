TEMPLATE = app
TARGET = allietests

DESTDIR=../bin

QT += testlib
QT -= gui network
CONFIG += c++14 console

include($$PWD/../lib/git.pri)

CONFIG(release, debug|release) {
  CONFIG += optimize_full
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/../lib

HEADERS += \
    testgames.h \
    testmath.h
    $$PWD/../lib/version.h

SOURCES += \
    main.cpp \
    testgames.cpp \
    testmath.cpp

win32 {
    PRE_TARGETDEPS += $$PWD/../lib $$DESTDIR/margean.lib
} else {
    PRE_TARGETDEPS += $$PWD/../lib $$DESTDIR/libmargean.a
    QMAKE_CXXFLAGS += -march=native -ffast-math
}

LIBS += -L$$OUT_PWD/../bin -lmargean

include($$PWD/../lib/atomic.pri)
include($$PWD/../lib/zlib.pri)
include($$PWD/../lib/protobuf.pri)
include($$PWD/../lib/cuda.pri)
