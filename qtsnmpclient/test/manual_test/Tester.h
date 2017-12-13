#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QMap>
#include <QtSnmpClient.h>
#include <QTimer>

class Tester : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY( Tester )
public:
    explicit Tester( const QHostAddress&,
                     QObject*const parent = 0 );
    Q_SLOT void start();

private:
    Q_SLOT void onResponseReceived( const qint32 request_id,
                                    const QtSnmpDataList& );
    Q_SLOT void onRequestFailed( const qint32 request_id );
    Q_SLOT void makeRequest();

private:
    const QHostAddress m_address;
    QScopedPointer< QtSnmpClient > m_snmp_client;
    QScopedPointer< QTimer > m_timer;

};
