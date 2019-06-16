#include <QTest>
#include <QDebug>
#include <QtSnmpClient.h>
#include <QUdpSocket>
#include <QUuid>
#include <chrono>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
    const quint16 TestPort = 65000;
    const milliseconds default_delay_ms = 100ms;

    QByteArray generateOID() {
        QByteArray oid;
        const int size = qrand() % 7;
        oid = QByteArray( ".1.3.6" );
        for( int i = 0; i < size; ++i ) {
            oid += "." + QByteArray::number( qrand() & 0xFF );
        }
        oid += ".3";
        return oid;
    }

    QStringList toStringList( const QList< QByteArray >& list ) {
        QStringList result;
        for( int i = 0; i < list.count(); ++i ) {
            result.append( QString::fromLatin1( list.at( i ) ) );
        }
        return result;
    }

    bool checkIntegerData( const QtSnmpData& data,
                           const int expected_value )
    {
        if( !data.isValid() ) {
            return false;
        }

        if( QtSnmpData::INTEGER_TYPE != data.type() ) {
            return false;
        }

        if( data.longLongValue() != expected_value ) {
            return false;
        }

        return true;
    }

    bool checkStringData( const QtSnmpData& data,
                          const QString& expected_text )
    {
        if( !data.isValid() ) {
            return false;
        }

        if( QtSnmpData::STRING_TYPE != data.type() ) {
            return false;
        }

        if( data.textValue() != expected_text ) {
            return false;
        }

        return true;
    }


    enum ErrorStatus {
        ErrorStatusNoErrors = 0,
        ErrorStatusTooBig = 1,
        ErrorStatusNoSuchName = 2,
        ErrorStatusBadValue = 3,
        ErrorStatusReadOnly = 4,
        ErrorStatusOtherErrors = 5
    };

    QtSnmpData makeResponse( const int request_id,
                             const QByteArray& community,
                             const QtSnmpDataList& list,
                             const int error_status = ErrorStatusNoErrors,
                             const int error_index = 0 )
    {
        auto var_bind_list = QtSnmpData::sequence();
        for( const auto& data : list ) {
            auto var_bind = QtSnmpData::sequence();
            var_bind.addChild( QtSnmpData::oid( data.address() ) );
            var_bind.addChild( data );
            var_bind_list.addChild( var_bind );
        }

        auto message = QtSnmpData::sequence();
        message.addChild( QtSnmpData::integer(0) ); // version is version-1(0)
        message.addChild( QtSnmpData::string( community ) );
        auto response_item = QtSnmpData( QtSnmpData::GET_RESPONSE_TYPE );
        response_item.addChild( QtSnmpData::integer( request_id ) );
        response_item.addChild( QtSnmpData::integer( error_status ) );
        response_item.addChild( QtSnmpData::integer( error_index ) );
        response_item.addChild( var_bind_list );
        message.addChild( response_item );
        return message;
    }

    QtSnmpData makeResponse( const int request_id,
                             const QByteArray& community,
                             const QtSnmpData& data,
                             const int error_status = ErrorStatusNoErrors,
                             const int error_index = 0 )
    {
        return makeResponse( request_id,
                             community,
                             QtSnmpDataList() << data,
                             error_status,
                             error_index );
    }

    bool checkMessage( const QtSnmpData& message,
                       const int expected_request_type,
                       const QByteArray& expected_community,
                       QtSnmpData*const internal_request_id,
                       QtSnmpDataList*const variables )
    {
        Q_ASSERT( internal_request_id && variables );
        variables->clear();

        if( message.type() != QtSnmpData::SEQUENCE_TYPE ) { // Message started from a SEQUENCE
            return false;
        }

        const auto last_message_children = message.children();
        if( last_message_children.count() != 3 ) {
            return false;
        }

        const auto version_data = last_message_children.at( 0 );
        if( !checkIntegerData( version_data, 0 ) ) { // version always is version-1(0)
            return false;
        }

        const auto community_data = last_message_children.at( 1 );
        if( !checkStringData( community_data, expected_community ) ) { // community
            return false;
        }

        const auto request = last_message_children.at( 2 );
        if( request.type() != expected_request_type ) {
            return false;
        }

        if( request.children().count() != 4 ) {
            return false;
        }

        *internal_request_id = request.children().at( 0 );
        if( internal_request_id->type() != QtSnmpData::INTEGER_TYPE ) { // RequestID
            return false;
        }

        const auto error_status = request.children().at( 1 );
        if( ! checkIntegerData( error_status, 0 ) ) { // ErrorStatus - noError
            return false;
        }

        const auto error_index = request.children().at( 2 );
        if( ! checkIntegerData( error_index, 0 ) ) { // ErrorIndex - zero for noError ErrorStatus
            return false;
        }

        const auto var_bind_list_item = request.children().at( 3 );
        if( var_bind_list_item.type() != QtSnmpData::SEQUENCE_TYPE ) { // VarBindList is a SEQUENCE of VarBind
            return false;
        }

        for( int i = 0; i < var_bind_list_item.children().count(); ++i ) {
            const auto var_bind = var_bind_list_item.children().at( i );

             // VarBind is a SEQUENCE of two fields: name and value
            if( var_bind.type() != QtSnmpData::SEQUENCE_TYPE ) {
                return false;
            }
            if( var_bind.children().count() != 2 ) {
                return false;
            }
            const auto name = var_bind.children().at( 0 );
            if( name.type() != QtSnmpData::OBJECT_TYPE ) { // name as ObjectName
                return false;
            }

            // 'value' as ObjectSyntax
            auto variable = var_bind.children().at( 1 );
            variable.setAddress( name.data() );
            variables->append( variable );
        }

        return true;
    }

    bool checkSingleVariableRequest( const QtSnmpData& message,
                                     const int expected_request_type,
                                     const QByteArray& expected_community,
                                     const QByteArray& expected_oid,
                                     QtSnmpData*const internal_request_id )
    {
        Q_ASSERT( internal_request_id );

        QtSnmpDataList variables;
        if( ! checkMessage( message,
                            expected_request_type,
                            expected_community,
                            internal_request_id,
                            &variables ) )
        {
            return false;
        }

        if( variables.count() != 1 ) {
            return false;
        }

        return variables.first().address() == expected_oid;
    }
}

class TestQtSnmpClient : public QObject {
    Q_OBJECT
    QScopedPointer< QUdpSocket > m_socket{ new QUdpSocket };
    QHostAddress m_client_address;
    quint16 m_client_port;
    int m_request_count = 0;
    QtSnmpDataList m_received_request_data_list;
    QScopedPointer< QtSnmpClient > m_client;
    int m_fail_count = 0;
    qint32 m_failed_request_id = 0;
    qint32 m_received_request_id = 0;
    QtSnmpDataList m_received_response_list;
    int m_response_count = 0;

    void cleanResponseData() {
        m_request_count = 0;
        m_received_request_data_list.clear();
        m_response_count = 0;
        m_fail_count = 0;
        m_failed_request_id = 0;
        m_received_request_id = 0;
        m_received_response_list.clear();
    }

private slots:
    void initTestCase() {
        QVERIFY( connect( m_socket.data(), &QUdpSocket::readyRead, [this]() {
            while( m_socket->hasPendingDatagrams() ) {
                ++m_request_count;
                const int size = static_cast< int >( m_socket->pendingDatagramSize() );
                const int max_datagram_size = 65507;
                QVERIFY(size > 0);
                QVERIFY( size <= max_datagram_size );
                QByteArray datagram;
                datagram.resize( size );
                const auto read_size = m_socket->readDatagram( datagram.data(),
                                                               size,
                                                               &m_client_address,
                                                               &m_client_port );
                QCOMPARE( size, read_size );
                m_received_request_data_list += QtSnmpData::parseData( datagram );
            }
        }) );
        QVERIFY( m_socket->bind( QHostAddress::LocalHost, TestPort ) );

        QtSnmpClient client;
        QCOMPARE( client.agentAddress(), QHostAddress() );
        QVERIFY( client.agentPort() == 161 );
        QCOMPARE( client.community(), QByteArray( "public" ) );
        QVERIFY( client.responseTimeout() == duration_cast< milliseconds >( seconds{10} ).count() );
        QCOMPARE( client.isBusy(), false );
    }

    void init() {
        qsrand( static_cast< uint >((system_clock::now() - system_clock::time_point::min()).count()) );

        m_client.reset( new QtSnmpClient );
        m_client->setAgentAddress( QHostAddress::LocalHost );
        m_client->setAgentPort( TestPort );

        m_fail_count = 0;
        m_received_request_id = 0;
        m_received_response_list.clear();
        m_response_count = 0;

        connect( m_client.data(),
                 &QtSnmpClient::requestFailed,
                 [this]( const qint32 request_id )
        {
            ++m_fail_count;
            m_failed_request_id = request_id;
        });

        connect( m_client.data(),
                 &QtSnmpClient::responseReceived,
                 [this]( const qint32 request_id,
                         const QtSnmpDataList& data_list )
        {
            ++m_response_count;
            m_received_request_id = request_id;
            m_received_response_list = data_list;
        });
    }

    void cleanup() {
        m_request_count = 0;
        m_received_request_data_list.clear();
        cleanResponseData();
    }

    void testGetRequestSingleValue() {
        auto checkValueRequest = [this](){
            const auto oid = generateOID();
            QCOMPARE( m_client->isBusy(), false );
            const auto req_id = m_client->requestValue( oid );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );

            QTest::qWait( default_delay_ms.count() );
            QCOMPARE( m_request_count, 1 );
            QCOMPARE( m_received_request_data_list.count(), 1 );
            QtSnmpData internal_request_id;
            QVERIFY( checkSingleVariableRequest( m_received_request_data_list.last(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id ) );

            // make response
            auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
            response_value.setAddress( oid );
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                response_value );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QCOMPARE( m_received_response_list.count(), 1 );
            QCOMPARE( m_received_response_list.at( 0 ).address(), oid );
            QVERIFY( checkStringData( m_received_response_list.at( 0 ), response_value.textValue() ) );
        };

        for( int i = 0; i < 10; ++i ) {
            checkValueRequest();
            cleanResponseData();
        }
    }

    void testGetRequestManyValues() {
        auto checkValuesRequest = [this](){
            QList< QByteArray > oid_list;
            const int variables_count = 5 + (qrand() % 5);
            for( int i = 0; i < variables_count; ++i ) {
                oid_list << generateOID();
            }
            QCOMPARE( m_client->isBusy(), false );
            const auto req_id = m_client->requestValues( toStringList( oid_list ) );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );

            auto timestamp = steady_clock::now();
            while( !m_request_count && ( steady_clock::now() - timestamp < seconds{2} ) ) {
                QTest::qWait( default_delay_ms.count() );
            }
            QCOMPARE( m_request_count, 1 );
            QCOMPARE( m_received_request_data_list.count(), 1 );

            QtSnmpDataList requested_variable_list;
            QtSnmpData internal_request_id;

            QVERIFY( checkMessage( m_received_request_data_list.last(),
                                   QtSnmpData::GET_REQUEST_TYPE,
                                   m_client->community(),
                                   &internal_request_id,
                                   &requested_variable_list) );
            QCOMPARE( requested_variable_list.count(), oid_list.count() );
            for( int i = 0; i < variables_count; ++i ) {
                const auto variable = requested_variable_list.at( i );
                QCOMPARE( variable.address(), oid_list.at( i ) );
                QVERIFY( QtSnmpData::NULL_DATA_TYPE == variable.type() );
            }

            // make response
            QtSnmpDataList expected_response_list;
            for( const auto& oid : oid_list ) {
                auto value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
                value.setAddress( oid );
                expected_response_list << value;
            }
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                expected_response_list );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            timestamp = steady_clock::now();
            while( !m_response_count && ( steady_clock::now() - timestamp < seconds{2} ) ) {
                QTest::qWait( default_delay_ms.count() );
            }

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QCOMPARE( m_received_response_list, expected_response_list );
        };

        for( int i = 0; i < 10; ++i ) {
            checkValuesRequest();
            cleanResponseData();
        }
    }

    void testRequestSubValues() {
        auto checkSubValuesRequest = [this]() {
            const auto base_oid = generateOID();
            QCOMPARE( m_client->isBusy(), false );
            const auto req_id = m_client->requestSubValues( QString::fromLatin1( base_oid ) );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );

            QByteArray value_oid = base_oid;
            QtSnmpData internal_request_id;

            QtSnmpDataList expected_response_data_list;

            for( int i = 1; i <= 10; ++i ) {
                QCOMPARE( m_client->isBusy(), true );
                auto timestamp = steady_clock::now();
                const auto prev_request_count = m_request_count;
                while( (m_request_count == prev_request_count ) &&
                       (steady_clock::now() - timestamp < seconds{2} ) ) {
                    QTest::qWait( default_delay_ms.count() );
                }

                // expect get next request with the previous oid and null in var bind
                QCOMPARE( m_request_count, i );
                QCOMPARE( m_received_request_data_list.count(), i );
                QVERIFY( checkSingleVariableRequest( m_received_request_data_list.last(),
                                                     QtSnmpData::GET_NEXT_REQUEST_TYPE,
                                                     m_client->community(),
                                                     value_oid,
                                                     &internal_request_id ) );

                // reply by string with the next oid
                if( i < 10 ) {
                    if( base_oid == value_oid ) {
                        value_oid = base_oid + ".1";
                    } else {
                        value_oid = value_oid.mid( 0, value_oid.length()-1 ) +
                                   QByteArray::number(i);
                    }
                    auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
                    response_value.setAddress( value_oid );
                    expected_response_data_list << response_value;
                    const auto response_message = makeResponse( internal_request_id.intValue(),
                                                                m_client->community(),
                                                                response_value );
                    QCOMPARE( m_client->isBusy(), true );
                    m_socket->writeDatagram( response_message.makeSnmpChunk(),
                                             m_client_address,
                                             m_client_port );
                } else {
                    // finally reply with the base oid with error
                    auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
                    const auto parent_oid = base_oid.mid( 0, base_oid.length() - 1 );
                    const auto base_oid_last_id = base_oid.right( 1 ).toInt();
                    const auto out_of_table_oid = parent_oid + QByteArray::number( base_oid_last_id + 1 );
                    response_value.setAddress( out_of_table_oid );
                    const auto response_message = makeResponse( internal_request_id.intValue(),
                                                                m_client->community(),
                                                                response_value );
                    QCOMPARE( m_client->isBusy(), true );
                    QCOMPARE( m_response_count, 0 );
                    m_socket->writeDatagram( response_message.makeSnmpChunk(),
                                             m_client_address,
                                             m_client_port );
                    timestamp = steady_clock::now();
                    while( !m_response_count && ( steady_clock::now() - timestamp < seconds{2} ) ) {
                        QTest::qWait( default_delay_ms.count() );
                    }
                    break;
                }
            }

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QCOMPARE( m_received_response_list, expected_response_data_list );
        };

        for( int i = 0; i < 10; ++i ) {
            checkSubValuesRequest();
            cleanResponseData();
        }
    }

    void testSetValue() {
        auto checkSetValueRequest = [this]( const QtSnmpData& value ){
            const auto oid = generateOID();
            QCOMPARE( m_client->isBusy(), false );

            const auto rw_community = QUuid::createUuid().toByteArray();
            QVERIFY( rw_community != m_client->community() );
            const int req_id = m_client->setValue( rw_community,
                                                   oid,
                                                   value.type(),
                                                   value.data() );
            QVERIFY( rw_community != m_client->community() );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );

            QTest::qWait( default_delay_ms.count() );
            QCOMPARE( m_request_count, 1 );
            QCOMPARE( m_received_request_data_list.count(), 1 );
            QtSnmpData internal_request_id;
            QtSnmpDataList requested_variable_list;
            QVERIFY( checkMessage( m_received_request_data_list.last(),
                                   QtSnmpData::SET_REQUEST_TYPE,
                                   rw_community,
                                   &internal_request_id,
                                   &requested_variable_list ) );
            QCOMPARE( requested_variable_list.count(), 1 );
            const auto& requested_variable = requested_variable_list.last();
            QCOMPARE( requested_variable.address(), oid );
            QCOMPARE( requested_variable.data(), value.data() );

            // make response
            auto response_value = value;
            response_value.setAddress( oid );
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                response_value );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QCOMPARE( m_received_response_list.count(), 1 );
            const auto received_value = m_received_response_list.first();
            QCOMPARE( received_value.address(), oid );
            QCOMPARE( received_value.type(), value.type() );
            QCOMPARE( received_value.data(), value.data() );
        };

        for( int i = 0; i < 1; ++i ) {
            checkSetValueRequest( QtSnmpData::integer( qrand() ) );
            cleanResponseData();
            checkSetValueRequest( QtSnmpData::string( QUuid::createUuid().toByteArray() ) );
            cleanResponseData();
        }
    }

    void testRepeatRequestAfterError1() {
        // Check that client will repeate the request with different id
        // until it received non-error answer.
        // We'll send 1 request via client and respond to it 2 errors and 1 (last) valid answer
        // Expected:
        //      3 request from client with different id will be received
        //      After valid answer there will not any other requests
        //      Until the client received valid answer it has to be busy
        //      Client reports about valid answer

        QVERIFY( milliseconds{m_client->responseTimeout()} >= 10*default_delay_ms );

        auto checkRepeatingRequestAfterError = [this]( const int error_status ){
            const auto oid = generateOID();
            QCOMPARE( m_client->isBusy(), false );
            const auto req_id = m_client->requestValue( oid );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );
            QtSnmpData internal_request_id[3];

            QTest::qWait( default_delay_ms.count() );

            // first request
            QCOMPARE( m_request_count, 1 );
            QCOMPARE( m_received_request_data_list.count(), 1 );
            QVERIFY( checkSingleVariableRequest( m_received_request_data_list.last(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id[0] ) );

            // first error response
            auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
            response_value.setAddress( oid );
            const auto response = makeResponse( internal_request_id[0].intValue(),
                                                m_client->community(),
                                                response_value,
                                                error_status,
                                                1 );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // second request after first error response
            QCOMPARE( m_client->isBusy(), true );
            QCOMPARE( m_response_count, 0 );
            QCOMPARE( m_fail_count, 0 );

            QCOMPARE( m_request_count, 2 );
            QCOMPARE( m_received_request_data_list.count(), 2 );
            QVERIFY( checkSingleVariableRequest( m_received_request_data_list.last(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id[1] ) );
            QVERIFY( internal_request_id[1] != internal_request_id[0] );

            // seconds error response
            const auto response2 = makeResponse( internal_request_id[1].intValue(),
                                                m_client->community(),
                                                response_value,
                                                error_status,
                                                1 );
            m_socket->writeDatagram( response2.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // third request after second error response
            QCOMPARE( m_client->isBusy(), true );
            QCOMPARE( m_response_count, 0 );
            QCOMPARE( m_fail_count, 0 );

            QCOMPARE( m_request_count, 3 );
            QCOMPARE( m_received_request_data_list.count(), 3 );
            QVERIFY( checkSingleVariableRequest( m_received_request_data_list.last(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id[2] ) );
            QVERIFY( internal_request_id[2] != internal_request_id[1] );

            // valid response
            const auto response3 = makeResponse( internal_request_id[2].intValue(),
                                                m_client->community(),
                                                response_value,
                                                ErrorStatusNoErrors,
                                                0 );
            m_socket->writeDatagram( response3.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QCOMPARE( m_received_response_list.count(), 1 );
            QCOMPARE( m_received_response_list.at( 0 ).address(), oid );
            QVERIFY( checkStringData( m_received_response_list.at( 0 ), response_value.textValue() ) );
        };

        for( int i = 0; i < 10; ++i ) {
            checkRepeatingRequestAfterError( ErrorStatusTooBig );
            cleanResponseData();
            checkRepeatingRequestAfterError( ErrorStatusNoSuchName );
            cleanResponseData();
            checkRepeatingRequestAfterError( ErrorStatusBadValue );
            cleanResponseData();
            checkRepeatingRequestAfterError( ErrorStatusReadOnly );
            cleanResponseData();
            checkRepeatingRequestAfterError( ErrorStatusOtherErrors );
            cleanResponseData();
        }
    }

    void testWaitingTimeOut() {
        // Check that client response about failure when the reponse timeout expired

        QVERIFY( milliseconds{m_client->responseTimeout()} >= 10*default_delay_ms );

        auto checkWaitingTimeOut = [this](){
            const auto oid = generateOID();
            QCOMPARE( m_client->isBusy(), false );
            const auto req_id = m_client->requestValue( oid );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );

            QTest::qWait( default_delay_ms.count() );
            QCOMPARE( m_request_count, 1 );
            QCOMPARE( m_received_request_data_list.count(), 1 );
            QtSnmpData internal_request_id;
            QVERIFY( checkSingleVariableRequest( m_received_request_data_list.last(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id ) );

            // check in half of the reponse timeout
            QTest::qWait( m_client->responseTimeout()/2 );
            QCOMPARE( m_client->isBusy(), true );
            QCOMPARE( m_response_count, 0 );
            QCOMPARE( m_fail_count, 0 );

            // check after one and half response timeout
            QTest::qWait( m_client->responseTimeout() );
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 0 );
            QCOMPARE( m_fail_count, 1 );
            QCOMPARE( m_failed_request_id, req_id );

            // make the valid response but after the response timeout
            auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
            response_value.setAddress( oid );
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                response_value );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 0 );
            QCOMPARE( m_fail_count, 1 );
        };

        for( int i = 0; i < 10; ++i ) {
            checkWaitingTimeOut();
            cleanResponseData();
        }
    }
};

QTEST_MAIN( TestQtSnmpClient )
#include "tsta_qtsnmpclient_client.moc"
