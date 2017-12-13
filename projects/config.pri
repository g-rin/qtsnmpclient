CONFIG(debug, debug|release) {
    BUILD_PATH = $${PWD}/../build/debug
} else {
    BUILD_PATH = $${PWD}/../build/release
}
GENERATED_FILES_DIR=$${BUILD_PATH}/.obj/$${TARGET}
OBJECTS_DIR = $${GENERATED_FILES_DIR}/o
MOC_DIR = $${GENERATED_FILES_DIR}/moc
RCC_DIR = $${GENERATED_FILES_DIR}/rcc
UI_DIR = $${GENERATED_FILES_DIR}/ui

BIN_PATH = $${BUILD_PATH}/bin
LIB_PATH = $${BUILD_PATH}/lib

QMAKE_LIBDIR += $${LIB_PATH}
unix: QMAKE_RPATHDIR += $${LIB_PATH}
