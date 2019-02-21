win32 {
    INCLUDEPATH += "C:/cache/zlib_build/include"
    LIBS += -L"C:/cache/zlib_build/lib" -lzlibstatic
} else {
    LIBS += -lz
}
