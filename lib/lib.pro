TEMPLATE = lib
TARGET = margean

QT -= gui network

DESTDIR=../bin
CONFIG += staticlib c++14 console

lessThan(QT_MAJOR_VERSION, 5) {
    error("Requres at least at least Qt 5.9")
}

lessThan(QT_MINOR_VERSION, 9) {
    error("Requres at least at least Qt 5.9")
}

!win32 {
    QMAKE_CXXFLAGS += -march=native -ffast-math
    DEFINES += USE_PEXT
}

DEFINES += TB_NO_HELPER_API

include(atomic.pri)
include(zlib.pri)
PROTOS += $$PWD/proto/net.proto
include(protobuf.pri)
include(cuda.pri)

CONFIG(release, debug|release) {
  CONFIG += optimize_full
}

HEADERS += \
    $$PWD/benchmarkengine.h \
    $$PWD/bitboard.h \
    $$PWD/cache.h \
    $$PWD/chess.h \
    $$PWD/clock.h \
    $$PWD/game.h \
    $$PWD/history.h \
    $$PWD/move.h \
    $$PWD/movegen.h \
    $$PWD/nn.h \
    $$PWD/node.h \
    $$PWD/notation.h \
    $$PWD/options.h \
    $$PWD/piece.h \
    $$PWD/search.h \
    $$PWD/searchengine.h \
    $$PWD/square.h \
    $$PWD/tree.h \
    $$PWD/tb.h \
    $$PWD/uciengine.h \
    $$PWD/zobrist.h \
    $$PWD/neural/allie_common.h \
    $$PWD/neural/allie_shim.h \
    $$PWD/neural/loader.h \
    $$PWD/neural/network.h \
    $$PWD/neural/network_legacy.h \
    $$PWD/neural/nn_policy.h \
    $$PWD/neural/weights_adapter.h \
    $$PWD/neural/cuda/cuda_common.h \
    $$PWD/neural/cuda/kernels.h \
    $$PWD/neural/cuda/layers.h \
    $$PWD/neural/shared/policy_map.h \
    $$PWD/fathom/tbconfig.h \
    $$PWD/fathom/tbcore.h \
    $$PWD/fathom/tbprobe.h

SOURCES += \
    $$PWD/benchmarkengine.cpp \
    $$PWD/bitboard.cpp \
    $$PWD/cache.cpp \
    $$PWD/clock.cpp \
    $$PWD/game.cpp \
    $$PWD/history.cpp \
    $$PWD/move.cpp \
    $$PWD/movegen.cpp \
    $$PWD/nn.cpp \
    $$PWD/node.cpp \
    $$PWD/notation.cpp \
    $$PWD/options.cpp \
    $$PWD/piece.cpp \
    $$PWD/search.cpp \
    $$PWD/searchengine.cpp \
    $$PWD/square.cpp \
    $$PWD/tb.cpp \
    $$PWD/tree.cpp \
    $$PWD/uciengine.cpp \
    $$PWD/zobrist.cpp \
    $$PWD/neural/network_legacy.cpp \
    $$PWD/neural/loader.cpp \
    $$PWD/neural/nn_policy.cpp \
    $$PWD/neural/weights_adapter.cpp \
    $$PWD/neural/cuda/layers.cpp \
    $$PWD/neural/cuda/nn_cuda.cpp \
    $$PWD/fathom/tbprobe.c

!win32 {
HEADERS += \
    $$PWD/neural/shared/activation.h \
    $$PWD/neural/shared/winograd_filter.h \

SOURCES += \
    $$PWD/neural/shared/activation.cpp \
    $$PWD/neural/shared/winograd_filter.cpp \
}
