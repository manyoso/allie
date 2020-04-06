# Need to discard STDERR so get path to NULL device
win32 {
    NULL_DEVICE = NUL # Windows doesn't have /dev/null but has NUL
} else {
    NULL_DEVICE = /dev/null
}

# Need to call git with manually specified paths to repository
BASE_GIT_COMMAND = git --git-dir $$PWD/../.git rev-parse --short HEAD

versiontarget.target = $$OUT_PWD/gitversion.h
win32 {
  versiontarget.commands = $$DESTDIR/allieversion $(shell $$BASE_GIT_COMMAND)
} else {
  versiontarget.commands = $$DESTDIR/allieversion $(shell $$BASE_GIT_COMMAND)
}
versiontarget.depends = FORCE

PRE_TARGETDEPS += $$OUT_PWD/gitversion.h
QMAKE_EXTRA_TARGETS += versiontarget
