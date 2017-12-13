#include "Tester.h"
#include <QThread>
#include <cassert>

namespace {
    const auto mib_2_OID = QString( ".1.3.6.1.2.1" );

    const auto sysDescr_OID = mib_2_OID + ".1.1.0";
    const auto sysUpTimeInstance_OID = mib_2_OID + ".1.3.0";
    const auto sysName_OID = mib_2_OID + ".1.5.0";

    const auto ifTable_OID = mib_2_OID + ".2.2";
    const auto ifEntry_OID = ifTable_OID + ".1";
    const auto ifMIBObjects_OID = mib_2_OID + ".31.1";
    const auto ifXTable = ifMIBObjects_OID + ".1";
    const auto ifXEntry_OID = ifXTable + ".1";

    const auto ifIndex_OID = ifEntry_OID + ".1";
    const auto ifName_OID = ifXEntry_OID + ".1";
    const auto ifDescr_OID = ifEntry_OID + ".2";
    const auto ifPhysAddress_OID = ifEntry_OID + ".6";
}

Tester::Tester( const QHostAddress& address,
                QObject*const parent )
    : QObject( parent )
    , m_address( address )
{
}

void Tester::start() {
    assert( QThread::currentThread() == thread() );
    assert( m_snmp_client.isNull() );
    m_snmp_client.reset( new QtSnmpClient );
    m_snmp_client->setAgentAddress( m_address );
    m_snmp_client->setCommunity( "public" );

    connect( m_snmp_client.data(),
             SIGNAL(responseReceived(qint32,QtSnmpDataList)),
             SLOT(onResponseReceived(qint32,QtSnmpDataList)) );
    connect( m_snmp_client.data(),
             SIGNAL(requestFailed(qint32)),
             SLOT(onRequestFailed(qint32)) );

    assert( m_timer.isNull() );
    m_timer.reset( new QTimer );
    connect( m_timer.data(), SIGNAL(timeout()), SLOT(makeRequest()) );
    m_timer->start( 500 + ( rand() % 1000 ) );
}

void Tester::onResponseReceived( const qint32,
                                 const QtSnmpDataList& values )
{
    assert( QThread::currentThread() == thread() );
    for( const auto& value : values ) {
        printf( "%s | %s : %s\n",
                qPrintable( m_address.toString() ),
                qPrintable( value.address() ),
                qPrintable( value.data().toHex() ) );
    }
}

void Tester::onRequestFailed( const qint32 request_id ) {
    assert( QThread::currentThread() == thread() );
    qDebug() << m_address << request_id;
    assert( !request_id );
}

void Tester::makeRequest() {
    assert( QThread::currentThread() == thread() );
    if( ! m_snmp_client->isBusy() ) {
        m_snmp_client->requestValue( sysDescr_OID );
        m_snmp_client->requestValues( QStringList() << sysUpTimeInstance_OID << sysName_OID );
        m_snmp_client->requestSubValues( ifIndex_OID );
        m_snmp_client->requestSubValues( ifName_OID );
        m_snmp_client->requestSubValues( ifDescr_OID );
        m_snmp_client->requestSubValues( ifPhysAddress_OID );
    }
}

