# Need to discard STDERR so get path to NULL device
win32 {
    NULL_DEVICE = NUL # Windows doesn't have /dev/null but has NUL
} else {
    NULL_DEVICE = /dev/null
}

# Need to call git with manually specified paths to repository
BASE_GIT_COMMAND = git --git-dir $$PWD/../.git rev-parse --short HEAD

# Get the short git sha
GIT_SHA = $$system($$BASE_GIT_COMMAND 2> $$NULL_DEVICE)

# Adding C preprocessor #DEFINE so we can use it in C++ code
DEFINES += GIT_SHA=\\\"$$GIT_SHA\\\"
