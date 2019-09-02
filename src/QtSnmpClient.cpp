#include "QtSnmpClient.h"
#include "Session.h"
#include <QThread>
#include "defines.h"

Q_DECLARE_METATYPE( QHostAddress )

QtSnmpClient::QtSnmpClient( QObject*const parent )
    : QObject( parent )
    , m_session( new qtsnmpclient::Session( this ) )
{
    qRegisterMetaType< QtSnmpDataList >();

    connect( m_session, SIGNAL(responseReceived(qint32,QtSnmpDataList)),
             this, SIGNAL(responseReceived(qint32,QtSnmpDataList)) );

    connect( m_session, SIGNAL(requestFailed(qint32)),
             this, SIGNAL(requestFailed(qint32)) );
}

QHostAddress QtSnmpClient::agentAddress() const {
    return m_session->agentAddress();
}

void QtSnmpClient::setAgentAddress( const QHostAddress& value ) {
    if ( value.isNull() || (QHostAddress( "0.0.0.0" ) == value) ) {
        qWarning() << trUtf8( "invalid address %1 will be ignored." ).arg( value.toString() );
        return;
    }

    if ( thread() != QThread::currentThread() ) {
        QMetaObject::invokeMethod( this,
                                   "setAgentAddress",
                                   Qt::QueuedConnection,
                                   QGenericReturnArgument(),
                                   Q_ARG( QHostAddress, value ) );
        return;
    }
    Q_ASSERT( thread() == QThread::currentThread() );


    m_session->setAgentAddress( value );
}

quint16 QtSnmpClient::agentPort() const {
    return m_session->agentPort();
}

void QtSnmpClient::setAgentPort( const quint16 value ) {
    if ( thread() != QThread::currentThread() ) {
        QMetaObject::invokeMethod( this,
                                   "setAgentPort",
                                   Qt::QueuedConnection,
                                   QGenericReturnArgument(),
                                   Q_ARG( quint16, value ) );
        return;
    }
    Q_ASSERT( thread() == QThread::currentThread() );

    m_session->setAgentPort( value );
}

QByteArray QtSnmpClient::community() const {
    return m_session->community();
}

void QtSnmpClient::setCommunity( const QByteArray& value ) {
    if ( thread() != QThread::currentThread() ) {
        QMetaObject::invokeMethod( this,
                                   "setCommunity",
                                   Qt::QueuedConnection,
                                   QGenericReturnArgument(),
                                   Q_ARG( QByteArray, value ) );
        return;
    }
    Q_ASSERT( thread() == QThread::currentThread() );

    m_session->setCommunity( value );
}

int QtSnmpClient::responseTimeout() const {
    return m_session->responseTimeout();
}

void QtSnmpClient::setReponseTimeout( const int value ) {
    if ( thread() != QThread::currentThread() ) {
        QMetaObject::invokeMethod( this,
                                   "setReponseTimeout",
                                   Qt::QueuedConnection,
                                   QGenericReturnArgument(),
                                   Q_ARG( int, value ) );
        return;
    }
    Q_ASSERT( thread() == QThread::currentThread() );

    m_session->setResponseTimeout( value );
}

bool QtSnmpClient::isBusy() const {
    return m_session->isBusy();
}

qint32 QtSnmpClient::requestValue( const QString& oid ) {
    return requestValues( QStringList( oid ) );
}

qint32 QtSnmpClient::requestValues( const QStringList& oid_list ) {
    return m_session->requestValues( oid_list );
}

qint32 QtSnmpClient::requestSubValues( const QString& oid ) {
    return m_session->requestSubValues( oid );
}

qint32 QtSnmpClient::setValue( const QByteArray& community,
                               const QString& oid,
                               const int type,
                               const QByteArray& value )
{
    return m_session->setValue( community, oid, type, value );
}
