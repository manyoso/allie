INCLUDEPATH += ${OBJECTS_DIR}

LIBS += -lprotobuf
protobuf_decl.name = protobuf headers
protobuf_decl.input = PROTOS
protobuf_decl.output = ${OBJECTS_DIR}proto/${QMAKE_FILE_BASE}.pb.h
protobuf_decl.commands = $(MKDIR) proto && protoc --cpp_out=proto/. --proto_path=${QMAKE_FILE_IN_PATH} ${QMAKE_FILE_NAME}
protobuf_decl.variable_out = HEADERS
protobuf_decl.CONFIG += target_predeps
QMAKE_EXTRA_COMPILERS += protobuf_decl

protobuf_impl.name = protobuf sources
protobuf_impl.input = PROTOS
protobuf_impl.output = ${OBJECTS_DIR}proto/${QMAKE_FILE_BASE}.pb.cc
protobuf_impl.depends = ${OBJECTS_DIR}proto/${QMAKE_FILE_BASE}.pb.h
protobuf_impl.commands = $$escape_expand(\n)
protobuf_impl.variable_out = SOURCES
QMAKE_EXTRA_COMPILERS += protobuf_impl
