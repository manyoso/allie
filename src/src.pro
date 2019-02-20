TEMPLATE = app
TARGET = allie

DESTDIR=../bin

QT -= gui network
CONFIG += c++11 console
include($$PWD/../lib/cuda.pri)

CONFIG(release, debug|release) {
  CONFIG += optimize_full
}

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/../lib

SOURCES += \
    main.cpp

PRE_TARGETDEPS += $$PWD/../lib $$DESTDIR/libmargean.a
LIBS += -L$$OUT_PWD/../bin -lmargean
LIBS += -lcudart -lcudnn -lcublas -lprotobuf -latomic -lz

target.path = ~/.local/bin
INSTALLS += target
