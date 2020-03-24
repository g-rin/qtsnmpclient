#include <QTest>
#include <QDebug>
#include <QtSnmpClient.h>
#include <QUdpSocket>
#include <QUuid>
#include <chrono>

using namespace std::chrono;

namespace {
    const quint16 TestPort = 65000;
    const milliseconds default_delay_ms{ 100 };

    QByteArray generateOID() {
        QByteArray oid;
        const int size = qrand() % 7;
        oid = QByteArray( ".1.3.6" );
        for ( int i = 0; i < size; ++i ) {
            oid += "." + QByteArray::number( qrand() & 0xFF );
        }
        oid += ".3";
        return oid;
    }

    bool checkIntegerData( const QtSnmpData& data,
                           const int expected_value )
    {
        if ( !data.isValid() ) {
            return false;
        }

        if ( QtSnmpData::INTEGER_TYPE != data.type() ) {
            return false;
        }

        if ( data.longLongValue() != expected_value ) {
            return false;
        }

        return true;
    }

    bool checkStringData( const QtSnmpData& data,
                          const QString& expected_text )
    {
        if ( !data.isValid() ) {
            return false;
        }

        if ( QtSnmpData::STRING_TYPE != data.type() ) {
            return false;
        }

        if ( data.textValue() != expected_text ) {
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
        for ( const auto& data : list ) {
            auto var_bind = QtSnmpData::sequence();
            var_bind.addChild( QtSnmpData::oid( data.address() ) );
            var_bind.addChild( data );
            var_bind_list.addChild( var_bind );
        }

        auto message = QtSnmpData::sequence();
        message.addChild( QtSnmpData::integer( QtSnmpClient::SNMPv2c ) );
        message.addChild( QtSnmpData::string( community ) );
        auto response_item = QtSnmpData( QtSnmpData::GET_RESPONSE_TYPE );
        response_item.addChild( QtSnmpData::integer( request_id ) );
        response_item.addChild( QtSnmpData::integer( error_status ) );
        response_item.addChild( QtSnmpData::integer( error_index ) );
        response_item.addChild( var_bind_list );
        message.addChild( response_item );
        return message;
    }

    bool checkMessage( const QtSnmpData& message,
                       const int expected_request_type,
                       const QByteArray& expected_community,
                       QtSnmpData*const internal_request_id,
                       QtSnmpDataList*const variables )
    {
        Q_ASSERT( internal_request_id && variables );
        variables->clear();

        if ( message.type() != QtSnmpData::SEQUENCE_TYPE ) { // Message started from a SEQUENCE
            return false;
        }

        const auto last_message_children = message.children();
        if ( last_message_children.size() != 3 ) {
            return false;
        }

        const int default_protocol_version = QtSnmpClient::SNMPv2c;
        const auto version_data = last_message_children.at( 0 );
        if ( !checkIntegerData( version_data, default_protocol_version ) ) {
            return false;
        }

        const auto community_data = last_message_children.at( 1 );
        if ( !checkStringData( community_data, expected_community ) ) { // community
            return false;
        }

        const auto request = last_message_children.at( 2 );
        if ( request.type() != expected_request_type ) {
            return false;
        }

        if ( request.children().size() != 4 ) {
            return false;
        }

        *internal_request_id = request.children().at( 0 );
        if ( internal_request_id->type() != QtSnmpData::INTEGER_TYPE ) { // RequestID
            return false;
        }

        const auto error_status = request.children().at( 1 );
        if ( ! checkIntegerData( error_status, 0 ) ) { // ErrorStatus - noError
            return false;
        }

        const auto error_index = request.children().at( 2 );
        if ( ! checkIntegerData( error_index, 0 ) ) { // ErrorIndex - zero for noError ErrorStatus
            return false;
        }

        const auto var_bind_list_item = request.children().at( 3 );
        if ( var_bind_list_item.type() != QtSnmpData::SEQUENCE_TYPE ) { // VarBindList is a SEQUENCE of VarBind
            return false;
        }

        for ( const auto& var_bind : var_bind_list_item.children() ) {
             // VarBind is a SEQUENCE of two fields: name and value
            if ( var_bind.type() != QtSnmpData::SEQUENCE_TYPE ) {
                return false;
            }
            if ( var_bind.children().size() != 2 ) {
                return false;
            }
            const auto name = var_bind.children().at( 0 );
            if ( name.type() != QtSnmpData::OBJECT_TYPE ) { // name as ObjectName
                return false;
            }

            // 'value' as ObjectSyntax
            auto variable = var_bind.children().at( 1 );
            variable.setAddress( name.data() );
            variables->push_back( variable );
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
        if ( ! checkMessage( message,
                            expected_request_type,
                            expected_community,
                            internal_request_id,
                            &variables ) )
        {
            return false;
        }

        if ( variables.size() != 1 ) {
            return false;
        }

        return variables.at( 0 ).address() == expected_oid;
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
            while ( m_socket->hasPendingDatagrams() ) {
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
                std::vector< QtSnmpData > list;
                QtSnmpData::parseData( datagram, &list );
                for ( const auto& item : list ) {
                    m_received_request_data_list.push_back( item );
                }
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
        qsrand( static_cast< uint >((system_clock::now() - system_clock::time_point()).count()) );

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
            QVERIFY( m_received_request_data_list.size() == 1 );
            QtSnmpData internal_request_id;
            QVERIFY( checkSingleVariableRequest( *m_received_request_data_list.rbegin(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id ) );

            // make response
            auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
            response_value.setAddress( oid );
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                { response_value } );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QVERIFY( m_received_response_list.size() == 1 );
            QCOMPARE( m_received_response_list.at( 0 ).address(), oid );
            QVERIFY( checkStringData( m_received_response_list.at( 0 ), response_value.textValue() ) );
        };

        for ( int i = 0; i < 10; ++i ) {
            checkValueRequest();
            cleanResponseData();
        }
    }

    void testGetRequestManyValues() {
        auto checkValuesRequest = [this](){
            std::vector< QByteArray > oid_list;
            QStringList string_list;
            const int variables_count = 5 + (qrand() % 5);
            for ( int i = 0; i < variables_count; ++i ) {
                oid_list.push_back( generateOID() );
                string_list.append( *oid_list.rbegin() );
            }

            QCOMPARE( m_client->isBusy(), false );
            const auto req_id = m_client->requestValues( string_list );
            QCOMPARE( m_client->isBusy(), true );
            QVERIFY( req_id > 0 );

            auto timestamp = steady_clock::now();
            while ( !m_request_count && ( steady_clock::now() - timestamp < seconds{2} ) ) {
                QTest::qWait( default_delay_ms.count() );
            }
            QCOMPARE( m_request_count, 1 );
            QVERIFY( m_received_request_data_list.size() == 1 );

            QtSnmpDataList requested_variable_list;
            QtSnmpData internal_request_id;

            QVERIFY( checkMessage( *m_received_request_data_list.rbegin(),
                                   QtSnmpData::GET_REQUEST_TYPE,
                                   m_client->community(),
                                   &internal_request_id,
                                   &requested_variable_list) );
            QVERIFY( requested_variable_list.size() == oid_list.size() );
            for ( size_t i = 0; i < oid_list.size(); ++i ) {
                const auto variable = requested_variable_list.at( i );
                QCOMPARE( variable.address(), oid_list.at( i ) );
                QVERIFY( QtSnmpData::NULL_DATA_TYPE == variable.type() );
            }

            // make response
            QtSnmpDataList expected_response_list;
            for ( const auto& oid : oid_list ) {
                auto value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
                value.setAddress( oid );
                expected_response_list.push_back( value );
            }
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                expected_response_list );

            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );

            timestamp = steady_clock::now();
            while ( !m_response_count && ( steady_clock::now() - timestamp < seconds{2} ) ) {
                QTest::qWait( default_delay_ms.count() );
            }

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QCOMPARE( m_received_response_list, expected_response_list );
        };

        for ( int i = 0; i < 10; ++i ) {
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

            for ( int i = 1; i <= 10; ++i ) {
                QCOMPARE( m_client->isBusy(), true );
                auto timestamp = steady_clock::now();
                const auto prev_request_count = m_request_count;
                while ( (m_request_count == prev_request_count ) &&
                       (steady_clock::now() - timestamp < seconds{2} ) ) {
                    QTest::qWait( default_delay_ms.count() );
                }

                // expect get next request with the previous oid and null in var bind
                QCOMPARE( m_request_count, i );
                QVERIFY( m_received_request_data_list.size() == static_cast< size_t >( i ) );
                QVERIFY( checkSingleVariableRequest( *m_received_request_data_list.rbegin(),
                                                     QtSnmpData::GET_NEXT_REQUEST_TYPE,
                                                     m_client->community(),
                                                     value_oid,
                                                     &internal_request_id ) );

                // reply by string with the next oid
                if ( i < 10 ) {
                    if ( base_oid == value_oid ) {
                        value_oid = base_oid + ".1";
                    } else {
                        value_oid = value_oid.mid( 0, value_oid.length()-1 ) +
                                   QByteArray::number(i);
                    }
                    auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
                    response_value.setAddress( value_oid );
                    expected_response_data_list.push_back( response_value );
                    const auto response_message = makeResponse( internal_request_id.intValue(),
                                                                m_client->community(),
                                                                { response_value } );
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
                                                                { response_value } );
                    QCOMPARE( m_client->isBusy(), true );
                    QCOMPARE( m_response_count, 0 );
                    m_socket->writeDatagram( response_message.makeSnmpChunk(),
                                             m_client_address,
                                             m_client_port );
                    timestamp = steady_clock::now();
                    while ( !m_response_count && ( steady_clock::now() - timestamp < seconds{2} ) ) {
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

        for ( int i = 0; i < 10; ++i ) {
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
            QVERIFY( m_received_request_data_list.size() == 1 );
            QtSnmpData internal_request_id;
            QtSnmpDataList requested_variable_list;
            QVERIFY( checkMessage( *m_received_request_data_list.rbegin(),
                                   QtSnmpData::SET_REQUEST_TYPE,
                                   rw_community,
                                   &internal_request_id,
                                   &requested_variable_list ) );
            QVERIFY( requested_variable_list.size() == 1 );
            const auto& requested_variable = *requested_variable_list.rbegin();
            QCOMPARE( requested_variable.address(), oid );
            QCOMPARE( requested_variable.data(), value.data() );

            // make response
            auto response_value = value;
            response_value.setAddress( oid );
            const auto response = makeResponse( internal_request_id.intValue(),
                                                m_client->community(),
                                                { response_value } );
            m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
            QTest::qWait( default_delay_ms.count() );

            // check response
            QCOMPARE( m_client->isBusy(), false );
            QCOMPARE( m_response_count, 1 );
            QCOMPARE( m_fail_count, 0 );
            QCOMPARE( m_received_request_id, req_id );
            QVERIFY( m_received_response_list.size() == 1 );
            const auto received_value = m_received_response_list.at( 0 );
            QCOMPARE( received_value.address(), oid );
            QCOMPARE( received_value.type(), value.type() );
            QCOMPARE( received_value.data(), value.data() );
        };

        for ( int i = 0; i < 1; ++i ) {
            checkSetValueRequest( QtSnmpData::integer( qrand() ) );
            cleanResponseData();
            checkSetValueRequest( QtSnmpData::string( QUuid::createUuid().toByteArray() ) );
            cleanResponseData();
        }
    }

    void testWaitingTimeOut() {
        // Check that client resend the same request five times after the timeout will has expired
        QVERIFY( milliseconds{m_client->responseTimeout()} >= 10*default_delay_ms );

        const auto oid = generateOID();
        const int attempt_limit = 5;
        auto last_internal_request_id = QtSnmpData::integer( 0 );
        const auto req_id = m_client->requestValue( oid );
        for ( int attempt = 0; attempt < attempt_limit; ++attempt ) {
            const auto start_waiting = steady_clock::now();
            int init_cnt = m_request_count;
            while ( true ) {
                QTest::qWait( 10 );
                if ( m_request_count > init_cnt ) {
                    break; // while
                }
                if ( (steady_clock::now() - start_waiting) > 1.1*milliseconds( m_client->responseTimeout()) ) {
                    break;
                }
            }

            QCOMPARE( m_request_count, 1 + attempt );
            QVERIFY( m_received_request_data_list.size() == static_cast< size_t >( 1 + attempt ) );
            QCOMPARE( m_client->isBusy(), true );
            QCOMPARE( m_response_count, 0 );
            QCOMPARE( m_fail_count, 0 );
            QtSnmpData internal_request_id;
            QVERIFY( checkSingleVariableRequest( *m_received_request_data_list.rbegin(),
                                                 QtSnmpData::GET_REQUEST_TYPE,
                                                 m_client->community(),
                                                 oid,
                                                 &internal_request_id ) );
            QCOMPARE( internal_request_id.type(), last_internal_request_id.type() );
            QVERIFY( internal_request_id.data() != last_internal_request_id.data() );
            last_internal_request_id = internal_request_id;
        }

        // check after one and an half response timeout
        QTest::qWait( 2 * m_client->responseTimeout());
        QCOMPARE( m_request_count, 6 );
        QVERIFY( m_received_request_data_list.size() == 6 );
        QCOMPARE( m_client->isBusy(), false );
        QCOMPARE( m_response_count, 0 );
        QCOMPARE( m_fail_count, 1 );
        QCOMPARE( m_failed_request_id, req_id );

        cleanResponseData();
    }

    void testErrorResponses() {
        // Check that client do not resend request after a valid error response

        QVERIFY( milliseconds{m_client->responseTimeout()} >= 10*default_delay_ms );

        const auto oid = generateOID();
        const auto req_id = m_client->requestValue( oid );
        QTest::qWait( default_delay_ms.count() );

        QCOMPARE( m_request_count, 1 );
        QVERIFY( m_received_request_data_list.size() == 1 );
        QCOMPARE( m_client->isBusy(), true );
        QCOMPARE( m_response_count, 0 );
        QCOMPARE( m_fail_count, 0 );
        QtSnmpData internal_request_id;
        QVERIFY( checkSingleVariableRequest( *m_received_request_data_list.rbegin(),
                                             QtSnmpData::GET_REQUEST_TYPE,
                                             m_client->community(),
                                             oid,
                                             &internal_request_id ) );
        auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
        response_value.setAddress( oid );
        const auto response = makeResponse( internal_request_id.intValue(),
                                            m_client->community(),
                                            { response_value },
                                            ErrorStatusNoSuchName,
                                            1 );
        m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );

        // check after one and an half response timeout
        QTest::qWait( 2 * m_client->responseTimeout());
        QCOMPARE( m_request_count, 1 );
        QVERIFY( m_received_request_data_list.size() == 1 );
        QCOMPARE( m_client->isBusy(), false );
        QCOMPARE( m_response_count, 0 );
        QCOMPARE( m_fail_count, 1 );
        QCOMPARE( m_failed_request_id, req_id );

        cleanResponseData();
    }

    void testProtocolVersionV1() {
        const auto oid = generateOID();
        QVERIFY( QtSnmpClient::SNMPv2c == m_client->protocolVersion() );
        m_client->setProtocolVersion( QtSnmpClient::SNMPv1 );
        QVERIFY( QtSnmpClient::SNMPv1 == m_client->protocolVersion() );
        const auto req_id = m_client->requestValue( oid );

        QTest::qWait( default_delay_ms.count() );
        QCOMPARE( m_request_count, 1 );
        QVERIFY( m_received_request_data_list.size() == 1 );
        QtSnmpData internal_request_id;

        QtSnmpDataList variables;
        const auto& request = m_received_request_data_list.rbegin();

        QVERIFY( request->type() == QtSnmpData::SEQUENCE_TYPE );
        const auto last_message_children = request->children();
        QVERIFY( last_message_children.size() == 3 );
        const auto version_data = last_message_children.at( 0 );
        QVERIFY( checkIntegerData( version_data, QtSnmpClient::SNMPv1 ) );
        const auto community_data = last_message_children.at( 1 );
        QVERIFY( checkStringData( community_data, m_client->community() ) );
        const auto get_request = last_message_children.at( 2 );
        QVERIFY( get_request.type() == QtSnmpData::GET_REQUEST_TYPE );
        QVERIFY( get_request.children().size() == 4 );
        internal_request_id = get_request.children().at( 0 );
        QVERIFY( internal_request_id.type() == QtSnmpData::INTEGER_TYPE );
        const auto error_status = get_request.children().at( 1 );
        QVERIFY( checkIntegerData( error_status, 0 ) ); // ErrorStatus - noError
        const auto error_index = get_request.children().at( 2 );
        QVERIFY( checkIntegerData( error_index, 0 ) ); // ErrorIndex - zero for noError ErrorStatus
        const auto var_bind_list_item = get_request.children().at( 3 );
        QVERIFY( var_bind_list_item.type() == QtSnmpData::SEQUENCE_TYPE );
        for ( const auto& var_bind : var_bind_list_item.children() ) {
             // VarBind is a SEQUENCE of two fields: name and value
            QVERIFY( var_bind.type() == QtSnmpData::SEQUENCE_TYPE );
            QVERIFY( var_bind.children().size() == 2 );
            const auto name = var_bind.children().at( 0 );
            QVERIFY( name.type() == QtSnmpData::OBJECT_TYPE ); // name as ObjectName
            // 'value' as ObjectSyntax
            auto variable = var_bind.children().at( 1 );
            variable.setAddress( name.data() );
            variables.push_back( variable );
        }
        QVERIFY( variables.size() == 1 );
        QCOMPARE( variables.at( 0 ).address(), oid );


        // make response
        auto response_value = QtSnmpData::string( QUuid::createUuid().toByteArray() );
        response_value.setAddress( oid );

        auto var_bind_list = QtSnmpData::sequence();
        auto var_bind = QtSnmpData::sequence();
        var_bind.addChild( QtSnmpData::oid( response_value.address() ) );
        var_bind.addChild( response_value );
        var_bind_list.addChild( var_bind );

        auto response = QtSnmpData::sequence();
        response.addChild( QtSnmpData::integer( m_client->protocolVersion()) );
        response.addChild( QtSnmpData::string( m_client->community() ) );
        auto response_item = QtSnmpData( QtSnmpData::GET_RESPONSE_TYPE );
        response_item.addChild( QtSnmpData::integer( internal_request_id.intValue() ) );
        response_item.addChild( QtSnmpData::integer( ErrorStatusNoErrors ) );
        response_item.addChild( QtSnmpData::integer( 0 ) );
        response_item.addChild( var_bind_list );
        response.addChild( response_item );

        m_socket->writeDatagram( response.makeSnmpChunk(), m_client_address, m_client_port );
        QTest::qWait( default_delay_ms.count() );

        // check response
        QCOMPARE( m_client->isBusy(), false );
        QCOMPARE( m_response_count, 1 );
        QCOMPARE( m_fail_count, 0 );
        QCOMPARE( m_received_request_id, req_id );
        QVERIFY( m_received_response_list.size() == 1 );
        QCOMPARE( m_received_response_list.at( 0 ).address(), oid );
        QVERIFY( checkStringData( m_received_response_list.at( 0 ), response_value.textValue() ) );
    }
};

QTEST_MAIN( TestQtSnmpClient )
#include "tsta_qtsnmpclient_client.moc"
