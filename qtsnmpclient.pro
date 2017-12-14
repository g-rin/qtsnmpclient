include($${PWD}/../config.pri)
QT = core network
SOURCES_PATH = $${PWD}/src
HEADERS *= $${SOURCES_PATH}/*.h
SOURCES *= $${SOURCES_PATH}/*.cpp
win32 : !static : DEFINES *= BUILD_QTSNMPCLIENT_DLL
