include( $${PWD}/config.pri )
TEMPLATE=lib
DESTDIR=$${LIB_PATH}
CONFIG+=shared
CONFIG-=static
QT = core network
SOURCES_PATH = $${PWD}/../src
HEADERS *= $${SOURCES_PATH}/*.h
SOURCES *= $${SOURCES_PATH}/*.cpp
win32 : !static : DEFINES *= BUILD_QTSNMPCLIENT_DLL
