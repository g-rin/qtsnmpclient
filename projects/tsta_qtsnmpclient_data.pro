include( $${PWD}/config.pri )
TEMPLATE=app
DESTDIR=$${BIN_PATH}
QT = core network testlib
SOURCES_PATH = $${PWD}/../test/auto
SOURCES *= $${PWD}/../test/auto/tsta_qtsnmpclient_data.cpp
INCLUDEPATH *= $${PWD}/../include
LIBS *= -L$${LIB_PATH} -lqtsnmpclient
