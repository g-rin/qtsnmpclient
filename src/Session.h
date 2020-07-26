#pragma once

#include "AbstractJob.h"
#include <QObject>
#include <QByteArray>
#include <QSharedPointer>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUdpSocket>
#include <QSharedPointer>
#include <QPair>
#include <QTimer>
#include <QHostAddress>
#include <QQueue>
#include <atomic>
#include "win_export.h"

namespace qtsnmpclient {

class Session : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY( Session )
public:
    Session( QObject*const parent = nullptr );

    QHostAddress agentAddress() const;
    void setAgentAddress( const QHostAddress& );

    quint16 agentPort() const;
    Q_SLOT void setAgentPort( const quint16 );

    int protocolVersion() const;
    Q_SLOT void setProtocolVersion( const int );

    QByteArray community() const;
    void setCommunity( const QByteArray& );

    int responseTimeout() const;
    void setResponseTimeout( const int );

    int getRequestLimit() const;
    void setGetRequestLimit( const int );

    bool isBusy() const;

    qint32 requestValues( const QStringList& oid_list );

    qint32 requestSubValues( const QString& oid );

    qint32 setValue( const QByteArray& community,
                     const QString& oid,
                     const int type,
                     const QByteArray& value );

    void sendRequestGetValues( const QStringList& names );
    void sendRequestGetNextValue( const QString& name );
    void sendRequestSetValue( const QByteArray& community,
                              const QString& name,
                              const int type,
                              const QByteArray& value );
    void completeWork( const QtSnmpDataList& );
    void failWork();

private:
    Q_SIGNAL void responseReceived( const qint32 request_id,
                                    const QtSnmpDataList& );
    Q_SIGNAL void requestFailed( const qint32 request_id );

private:
    void addWork( const JobPointer& );
    void startNextWork();
    void finishWork();
    Q_SLOT void onResponseTimeExpired();
    void cancelWork();
    Q_SLOT void onReadyRead();
    void processIncommingDatagram( const QByteArray& );
    bool writeDatagram( const QByteArray& );
    void sendRequest( const QtSnmpData& );
    qint32 createWorkId();
    void updateRequestId();

private:
    QHostAddress m_agent_address;
    quint16 m_agent_port = 161; // default SNMP port
    int m_protocol_version = 1; // v2c is default protocol version
    QByteArray m_community;
    QUdpSocket m_socket;
    QTimer m_response_wait_timer;
    qint32 m_work_id = 1;
    qint32 m_request_id = -1;
    QQueue< qint32 > m_request_history_queue;
    QtSnmpData m_last_request_data;
    QByteArray m_last_request_datagram;
    SnmpJobList m_work_queue;
    JobPointer m_current_work;
    int m_timeout_cnt = 0;
    std::atomic_int m_get_limit = {0};
};

} // namespace qtsnmpclient
