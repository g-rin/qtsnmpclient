#include "QtSnmpClient.h"
#include "Session.h"
#include "defines.h"

Q_DECLARE_METATYPE( QHostAddress )

QtSnmpClient::QtSnmpClient( QObject*const parent )
    : QObject( parent )
    , m_session( new qtsnmpclient::Session( this ) )
{
    static bool once = true;
    if( once ) {
        qRegisterMetaType< QtSnmpDataList >();
        once = false;
    }

    connect( m_session,
             SIGNAL(responseReceived(qint32,QtSnmpDataList)),
             this,
             SIGNAL(responseReceived(qint32,QtSnmpDataList)) );

    connect( m_session,
             SIGNAL(requestFailed(qint32)),
             this,
             SIGNAL(requestFailed(qint32)) );
}

QHostAddress QtSnmpClient::agentAddress() const {
    return m_session->agentAddress();
}

void QtSnmpClient::setAgentAddress( const QHostAddress& value ) {
    IN_QOBJECT_THREAD( value );
    m_session->setAgentAddress( value );
}

QByteArray QtSnmpClient::community() const {
    return m_session->community();
}

void QtSnmpClient::setCommunity( const QByteArray& value ) {
    IN_QOBJECT_THREAD( value );
    m_session->setCommunity( value );
}

int QtSnmpClient::responseTimeout() const {
    return m_session->responseTimeout();
}

void QtSnmpClient::setReponseTimeout( const int value ) {
    IN_QOBJECT_THREAD( value );
    m_session->setResponseTimeout( value );
}

bool QtSnmpClient::isBusy() const {
    return m_session->isBusy();
}

qint32 QtSnmpClient::requestValue( const QString& oid ) {
    return requestValues( QStringList() << oid );
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
