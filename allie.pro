TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += lib src
src.depends = lib

!win32 {
    SUBDIRS += tests
    tests.depends = lib
}
