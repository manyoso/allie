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
    $$PWD/bitboard.h \
    $$PWD/chess.h \
    $$PWD/clock.h \
    $$PWD/game.h \
    $$PWD/hash.h \
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
    $$PWD/settings.h \
    $$PWD/square.h \
    $$PWD/tb.h \
    $$PWD/treeiterator.h \
    $$PWD/treeutils.h \
    $$PWD/uciengine.h \
    $$PWD/zobrist.h \
    $$PWD/neural/allie_common.h \
    $$PWD/neural/allie_shim.h \
    $$PWD/neural/loader.h \
    $$PWD/neural/network.h \
    $$PWD/neural/network_legacy.h \
    $$PWD/neural/nn_policy.h \
    $$PWD/neural/policy_map.h \
    $$PWD/neural/weights_adapter.h \
    $$PWD/neural/cuda/cuda_common.h \
    $$PWD/neural/cuda/kernels.h \
    $$PWD/neural/cuda/layers.h \
    $$PWD/fathom/tbconfig.h \
    $$PWD/fathom/tbcore.h \
    $$PWD/fathom/tbprobe.h

SOURCES += \
    $$PWD/bitboard.cpp \
    $$PWD/clock.cpp \
    $$PWD/game.cpp \
    $$PWD/hash.cpp \
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
    $$PWD/settings.cpp \
    $$PWD/square.cpp \
    $$PWD/tb.cpp \
    $$PWD/uciengine.cpp \
    $$PWD/zobrist.cpp \
    $$PWD/neural/cuda/layers.cpp \
    $$PWD/neural/cuda/nn_cuda.cpp \
    $$PWD/neural/network_legacy.cpp \
    $$PWD/neural/loader.cpp \
    $$PWD/neural/nn_policy.cpp \
    $$PWD/neural/weights_adapter.cpp \
    $$PWD/fathom/tbprobe.c
