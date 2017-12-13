#pragma once

#include "QtSnmpData.h"
#include <QObject>
#include <QHostAddress>
#include "win_export.h"

namespace qtsnmpclient { class Session; }

class WIN_EXPORT QtSnmpClient : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY( QtSnmpClient )
public:
    explicit QtSnmpClient( QObject*const parent = nullptr );

    QHostAddress agentAddress() const;
    Q_SLOT void setAgentAddress( const QHostAddress& );

    QByteArray community() const;
    Q_SLOT void setCommunity( const QByteArray& );

    int responseTimeout() const;
    Q_SLOT void setReponseTimeout( const int );

    bool isBusy() const;

    qint32 requestValue( const QString& );

    qint32 requestValues( const QStringList& oid_list );

    qint32 requestSubValues( const QString& oid );

    qint32 setValue( const QByteArray& community,
                     const QString& oid,
                     const int type,
                     const QByteArray& value );

private:
    Q_SIGNAL void responseReceived( const qint32 request_id,
                                    const QtSnmpDataList& );
    Q_SIGNAL void requestFailed( const qint32 request_id );

private:
    qtsnmpclient::Session*const m_session;
};
