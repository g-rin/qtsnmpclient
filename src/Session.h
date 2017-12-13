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
#include "win_export.h"

namespace qtsnmpclient {

class Session : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY( Session )
public:
    Session( QObject*const parent = 0 );

    QHostAddress agentAddress() const;
    void setAgentAddress( const QHostAddress& );

    QByteArray community() const;
    void setCommunity( const QByteArray& );

    int responseTimeout() const;
    void setResponseTimeout( const int );

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

private:
    Q_SIGNAL void responseReceived( const qint32 request_id,
                                    const QtSnmpDataList& );
    Q_SIGNAL void requestFailed( const qint32 request_id );

private:
    void addWork( const JobPointer& );
    void startNextWork();
    Q_SLOT void cancelWork();
    Q_SLOT void onReadyRead();
    QtSnmpDataList getResponseData( const QByteArray& datagram );
    void sendDatagram( const QByteArray& );
    qint32 createWorkId();
    void updateRequestId();

private:
    QHostAddress m_agent_address;
    QByteArray m_community;
    QUdpSocket*const m_socket;
    QTimer*const m_response_wait_timer;
    qint32 m_work_id = 1;
    qint32 m_request_id = -1;
    SnmpJobList m_work_queue;
    JobPointer m_current_work;
};

} // namespace qtsnmpclient
