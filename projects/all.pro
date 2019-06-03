TEMPLATE = subdirs
CONFIG = ordered

SUBDIRS *= qtsnmpclient
qtsnmpclient.file = $${PWD}/qtsnmpclient.pro

SUBDIRS *= manual_test
manual_test.file = $${PWD}/manual_test.pro

SUBDIRS *= tsta_qtsnmpclient_data
tsta_qtsnmpclient_data.file = $${PWD}/tsta_qtsnmpclient_data.pro

SUBDIRS *= tsta_qtsnmpclient_client
tsta_qtsnmpclient_client.file = $${PWD}/tsta_qtsnmpclient_client.pro
