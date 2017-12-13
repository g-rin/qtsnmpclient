include( $${PWD}/config.pri )
TEMPLATE=app
DESTDIR=$${BIN_PATH}
QT = core network
SOURCES_PATH = $${PWD}/../test/manual_test
HEADERS *= $${SOURCES_PATH}/*.h
SOURCES *= $${SOURCES_PATH}/*.cpp
INCLUDEPATH *= $${PWD}/../include
LIBS *= -L$${LIB_PATH} -lqtsnmpclient
