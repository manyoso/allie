TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += lib version src
src.depends = lib version

!win32 {
    SUBDIRS += tests
    tests.depends = lib version
}
