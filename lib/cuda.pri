CUDA_FLAGS = $$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_SHLIB

CUDA_INC_DIR = $$(CUDA_INC_DIR)
!isEmpty(CUDA_INC_DIR) {
    INCLUDEPATH += $(CUDA_INC_DIR)
} else {
    win32 {
        INCLUDEPATH += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v10.0\include" \
            "C:\cache\cuda\include"
    } else {
        INCLUDEPATH += /usr/local/cuda/include
    }
}

CUDA_LIB_DIR = $$(CUDA_LIB_DIR)
!isEmpty(CUDA_LIB_DIR) {
    QMAKE_LIBDIR += $(CUDA_LIB_DIR)
} else {
    win32 {
        QMAKE_LIBDIR += "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v10.0\lib\x64" \
            "C:\cache\cuda\lib\x64"
    } else {
        QMAKE_LIBDIR += /usr/local/cuda/lib64
    }
}

NVCC_EXEC = nvcc

win32 {
    NVCC_EXEC = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v10.0\bin\nvcc"
    CONFIG(release, debug|release) {
        CUDA_FLAGS += -MD
    }
    CONFIG(debug, debug|release) {
        CUDA_FLAGS += -MDd
    }

} else {
    CUDA_FLAGS += -fno-exceptions
}

INCLUDEPATH += $$PWD

LIBS += -lcudart -lcudnn -lcublas

# CUDA COMMON
CUDA_COMMON_SOURCES += $$PWD/neural/cuda/common_kernels.cu
cuda_common.output = $${OBJECTS_DIR}${QMAKE_FILE_BASE}_cuda.obj
cuda_common.commands = $$NVCC_EXEC -c -Xcompiler $$join(CUDA_FLAGS,",") $$join(INCLUDEPATH,'" -I "','-I "','"') ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
cuda_common.input = CUDA_COMMON_SOURCES

# CUDA FP 16
CUDA_FP16_SOURCES += $$PWD/neural/cuda/fp16_kernels.cu
cuda_fp16.output = $${OBJECTS_DIR}${QMAKE_FILE_BASE}_cuda.obj
cuda_fp16.commands = $$NVCC_EXEC -arch=compute_70 -code=sm_70 -c -Xcompiler $$join(CUDA_FLAGS,",") $$join(INCLUDEPATH,'" -I "','-I "','"') ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
cuda_fp16.input = CUDA_FP16_SOURCES

QMAKE_EXTRA_UNIX_COMPILERS += cuda_common cuda_fp16
