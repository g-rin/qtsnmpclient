#include "Session.h"
#include "QtSnmpData.h"
#include "RequestValuesJob.h"
#include "RequestSubValuesJob.h"
#include "SetValueJob.h"
#include <QDateTime>
#include <QHostAddress>
#include <QThread>
#include <math.h>

namespace qtsnmpclient {

namespace {
    const int default_response_timeout = 10000;

    QString errorStatusText( const int val ) {
        static const QHash< int, QString > map = { {0, "No errors"},
                                                   {1, "Too big"},
                                                   {2, "No such name"},
                                                   {3, "Bad value"},
                                                   {4, "Read only"},
                                                   {5, "Other errors"} };
        const auto iter = map.constFind( val );
        if ( map.constEnd() != iter ) {
            return iter.value();
        }
        return QString( "Unsupported error(%1)" ).arg( val );
    }

    QtSnmpData changeRequestId( const QtSnmpData& request,
                                const int request_id )
    {
        Q_ASSERT( QtSnmpData::SEQUENCE_TYPE == request.type() );
        Q_ASSERT( 3 == request.children().size() );

        auto message_children = request.children();
        auto request_item = message_children[2];

        auto new_request_item = QtSnmpData( request_item.type() );
        new_request_item.addChild( QtSnmpData::integer( request_id ) );

        auto request_children = request_item.children();
        Q_ASSERT( request_children.size() > 0 );

        for ( size_t i = 1; i < request_children.size(); ++i ) {
            new_request_item.addChild( request_children.at( i ) );
        }

        auto new_message = QtSnmpData::sequence();
        new_message.addChild( message_children.at(0) );
        new_message.addChild( message_children.at(1) );
        new_message.addChild( new_request_item );

        return new_message;
    }
}

Session::Session( QObject*const parent )
    : QObject( parent )
    , m_community( "public" )
{
    connect( &m_socket, SIGNAL(readyRead()), SLOT(onReadyRead()) );
    auto socket_timer = new QTimer( this );
    connect( socket_timer, SIGNAL(timeout()), SLOT(onReadyRead()) );
    socket_timer->start( 300 );

    connect( &m_response_wait_timer,
             SIGNAL(timeout()),
             SLOT(onResponseTimeExpired()) );
    m_response_wait_timer.setInterval( default_response_timeout );
}

QHostAddress Session::agentAddress() const {
    return m_agent_address;
}

void Session::setAgentAddress( const QHostAddress& value ) {
    bool ok = !value.isNull();
    ok = ok && ( QHostAddress( QHostAddress::Any ) != value );
    ok = ok && ( QHostAddress( QHostAddress::AnyIPv4 ) != value );
    ok = ok && ( QHostAddress( QHostAddress::AnyIPv6 ) != value );
    if ( ok ) {
        m_timeout_cnt = 0;
        m_agent_address = value;
        m_socket.close();
        m_socket.bind( QHostAddress::AnyIPv4 );
    } else {
        qDebug() << tr( "Attempt to set invalid agent address: %1" ).arg( value.toString() );
    }
}

quint16 Session::agentPort() const {
    return m_agent_port;
}

void Session::setAgentPort( const quint16 value ) {
    m_agent_port = value;
}

int Session::protocolVersion() const {
    return m_protocol_version;
}

void Session::setProtocolVersion( const int value ) {
    m_protocol_version = value;
}

QByteArray Session::community() const {
    return m_community;
}

void Session::setCommunity( const QByteArray& value ) {
    m_community = value;
}

int Session::responseTimeout() const {
    return m_response_wait_timer.interval();
}

void Session::setResponseTimeout( const int value ) {
    if ( value != m_response_wait_timer.interval() ) {
        m_response_wait_timer.setInterval( value );
    }
}

int Session::getRequestLimit() const {
    return m_get_limit;
}

void Session::setGetRequestLimit( const int value ) {
    m_get_limit.exchange( value );
}

bool Session::isBusy() const {
    return m_current_work || m_work_queue.size();
}

qint32 Session::requestValues( const QStringList& oid_list ) {
    const qint32 work_id = createWorkId();
    addWork( std::make_shared< RequestValuesJob >( this, work_id, oid_list, m_get_limit ) );
    return work_id;
}

qint32 Session::requestSubValues( const QString& oid ) {
    const qint32 work_id = createWorkId();
    addWork( std::make_shared< RequestSubValuesJob >( this, work_id, oid ) );
    return work_id;
}

qint32 Session::setValue( const QByteArray& community,
                          const QString& oid,
                          const int type,
                          const QByteArray& value )
{
    const qint32 work_id = createWorkId();
    addWork( std::make_shared< SetValueJob >( this, work_id, community, oid, type, value ) );
    return work_id;
}

void Session::addWork( const JobPointer& work ) {
    if ( thread() != QThread::currentThread() ) {
        QMetaObject::invokeMethod( this,
                                   "addWork",
                                   Qt::QueuedConnection,
                                   QGenericReturnArgument(),
                                   Q_ARG( qtsnmpclient::JobPointer, work ) );
        return;
    }

    Q_ASSERT( thread() == QThread::currentThread() );

    const int queue_limit = 100;
    if ( m_work_queue.size() < queue_limit ) {
        m_work_queue.push( work );
        startNextWork();
    } else {
        qDebug() << tr( "SNMP request %1 for %2 has been dropped, due to the queue is full.")
                        .arg( work->description(), m_agent_address.toString() );
    }
}

void Session::startNextWork() {
    if ( !m_current_work && m_work_queue.size() ){
        m_current_work = m_work_queue.front();
        m_work_queue.pop();
        m_current_work->start();
    }
}

void Session::finishWork() {
    m_current_work.reset();
    m_response_wait_timer.stop();
    m_timeout_cnt = 0;
}

void Session::completeWork( const QtSnmpDataList& values ) {
    Q_ASSERT( m_current_work );
    emit responseReceived( m_current_work->id(), values );
    finishWork();
    startNextWork();
}

void Session::failWork() {
    Q_ASSERT( m_current_work );
    emit requestFailed( m_current_work->id() );
    finishWork();
    startNextWork();
}

void Session::onResponseTimeExpired() {
    if ( ++m_timeout_cnt > 5 ) {
        qDebug() << tr( "Response's timeout has been expired.\n"
                        "There is no any snmp response for %1 from %2\n"
                        "Request internal id #%3." )
                        .arg( m_current_work->description(), m_agent_address.toString() )
                        .arg( m_request_id );
        cancelWork();
        return;
    }

    updateRequestId();
    const auto new_request = changeRequestId( m_last_request_data, m_request_id );
    writeDatagram( new_request.makeSnmpChunk() );
}

void Session::cancelWork() {
    if ( m_current_work ) {
        emit requestFailed( m_current_work->id() );
        m_current_work.reset();
    }
    m_response_wait_timer.stop();
    m_request_id = -1;
    m_timeout_cnt = 0;
    startNextWork();
}

void Session::sendRequestGetValues( const QStringList& names ) {
    if ( -1 != m_request_id ) {
        qDebug() << tr( "An attempt to make new request during waiting response for the previous one.\n"
                        "Agent's address: %1\n"
                        "Requested OIDS: %2" )
                        .arg( m_agent_address.toString(), names.join( "; " ) );
        return;
    }

    updateRequestId();
    QtSnmpData full_packet = QtSnmpData::sequence();
    full_packet.addChild( QtSnmpData::integer( m_protocol_version ) );
    full_packet.addChild( QtSnmpData::string( m_community ) );
    QtSnmpData request( QtSnmpData::GET_REQUEST_TYPE );
    request.addChild( QtSnmpData::integer( m_request_id ) );
    request.addChild( QtSnmpData::integer( 0 ) );
    request.addChild( QtSnmpData::integer( 0 ) );
    QtSnmpData seq_all_obj = QtSnmpData::sequence();
    for ( const auto& oid_key : names ) {
        QtSnmpData seq_obj_info = QtSnmpData::sequence();
        seq_obj_info.addChild( QtSnmpData::oid( oid_key.toLatin1() ) );
        seq_obj_info.addChild( QtSnmpData::null() );
        seq_all_obj.addChild( seq_obj_info );
    }
    request.addChild( seq_all_obj );
    full_packet.addChild( request );
    sendRequest( full_packet );
}

void Session::sendRequestGetNextValue( const QString& name ) {
    if ( -1 != m_request_id ) {
        qDebug() << tr( "An attempt to make new request during waiting response for the previous one.\n"
                        "Agent's address: %1\n"
                        "Requested OID: %2" )
                        .arg( m_agent_address.toString(), name );
        return;
    }

    updateRequestId();
    QtSnmpData full_packet = QtSnmpData::sequence();
    full_packet.addChild( QtSnmpData::integer( m_protocol_version ) );
    full_packet.addChild( QtSnmpData::string( m_community ) );
    QtSnmpData request( QtSnmpData::GET_NEXT_REQUEST_TYPE );
    request.addChild( QtSnmpData::integer( m_request_id ) );
    request.addChild( QtSnmpData::integer( 0 ) );
    request.addChild( QtSnmpData::integer( 0 ) );
    QtSnmpData seq_all_obj = QtSnmpData::sequence();
    QtSnmpData seq_obj_info = QtSnmpData::sequence();
    seq_obj_info.addChild( QtSnmpData::oid( name.toLatin1() ) );
    seq_obj_info.addChild( QtSnmpData::null() );
    seq_all_obj.addChild( seq_obj_info );
    request.addChild( seq_all_obj );
    full_packet.addChild( request );
    sendRequest( full_packet );
}

void Session::sendRequestSetValue( const QByteArray& community,
                                   const QString& name,
                                   const int type,
                                   const QByteArray& value )
{
    if ( -1 != m_request_id ) {
        qDebug() << tr( "An attempt to make new (SET) request during waiting response for the previous one.\n"
                        "Agent's address: %1\n"
                        "OID: %2\n"
                        "type: %3\n"
                        "value: %4" )
                        .arg( m_agent_address.toString(), name )
                        .arg( type )
                        .arg( value.toStdString().c_str() );
        return;
    }

    updateRequestId();
    auto pdu_packet = QtSnmpData::sequence();
    pdu_packet.addChild( QtSnmpData::integer( m_protocol_version ) );
    pdu_packet.addChild( QtSnmpData::string( community ) );
    auto request_type = QtSnmpData( QtSnmpData::SET_REQUEST_TYPE );
    request_type.addChild( QtSnmpData::integer( m_request_id ) );
    request_type.addChild( QtSnmpData::integer( 0 ) );
    request_type.addChild( QtSnmpData::integer( 0 ) );
    auto seq_all_obj = QtSnmpData::sequence();
    auto seq_obj_info = QtSnmpData::sequence();
    seq_obj_info.addChild( QtSnmpData::oid( name.toLatin1() ) );
    seq_obj_info.addChild( QtSnmpData( type, value ) );
    seq_all_obj.addChild( seq_obj_info );
    request_type.addChild( seq_all_obj );
    pdu_packet.addChild( request_type );
    sendRequest( pdu_packet );
}

void Session::onReadyRead() {
    if ( QUdpSocket::BoundState != m_socket.state() ) {
        return;
    }

    while ( m_socket.hasPendingDatagrams() ) {
        // NOTE:
        // The field size sets a theoretical limit of 65,535 bytes
        // (8 byte header + 65,527 bytes of data) for a UDP datagram.
        // However the actual limit for the data length,
        // which is imposed by the underlying IPv4 protocol,
        // is 65,507 bytes (65,535 − 8 byte UDP header − 20 byte IP header).
        const int max_datagram_size = 65507;

        const int size = static_cast< int >( m_socket.pendingDatagramSize() );
        if ( (size <= 0) ) {
            continue;
        }

        if ( size > max_datagram_size ) {
            qDebug() << tr( "Too big UDP packet has been received.\n"
                            "There was an UDP packet with declared size %1 has been received from %2." )
                            .arg( size )
                            .arg( m_agent_address.toString() );
        }


        QByteArray datagram;
        datagram.reserve( size );
        datagram.append( size, '\x0' );
        const auto read_size = m_socket.readDatagram( datagram.data(), size );
        if ( size != read_size ) {
            qDebug() << tr( "SNMP response reading error.\n"
                            "Only %1 bytes of %2 have been read from UDP packet from %3.\n"
                            "Cause: %4" )
                            .arg( read_size )
                            .arg( size )
                            .arg( m_agent_address.toString(), m_socket.errorString() );
            continue;
        }
        processIncommingDatagram( datagram );
    }
}

void Session::processIncommingDatagram( const QByteArray& datagram ) {
    QtSnmpDataList valid_list;
    valid_list.reserve( 1024 );
    QList< AbstractJob::ErrorResponse > error_list;
    QtSnmpDataList raw_list;
    QtSnmpData::parseData( datagram, &raw_list );
    for ( const auto& packet : raw_list ) {
        const auto& resp_list = packet.children();
        if ( 3 != resp_list.size() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected top packet's children count: %1 (expected 3)\n"
                            "in a response from %2" )
                            .arg( resp_list.size() )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const auto& resp = resp_list.at( 2 );
        if ( QtSnmpData::GET_RESPONSE_TYPE != resp.type() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected response's type: %1 (expected GET_RESPONSE_TYPE as %2 ) "
                            "in a response from %3" )
                            .arg( resp.type() )
                            .arg( QtSnmpData::GET_RESPONSE_TYPE )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const auto& children = resp.children();
        if ( 4 != children.size() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected child count: %1 (expected 4) "
                            "in a response from %2" )
                            .arg( children.size() )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const auto& request_id_data = children.at( 0 );
        if ( QtSnmpData::INTEGER_TYPE != request_id_data.type() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected request id's type: %1 (expected INTEGER_TYPE as %2) "
                            "in a response from %3" )
                            .arg( request_id_data.type() )
                            .arg( QtSnmpData::INTEGER_TYPE )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const int response_req_id = request_id_data.intValue();
        if ( response_req_id != m_request_id ) {
            QStringList history;
            for ( const auto item : m_request_history_queue ) {
                history << "0x" + QString::number( item, 16 );
            }

            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected request id: 0x%1 (expected 0x%2 ) in a response from %3.\n"
                            "History (request id list): %4" )
                            .arg( QString::number( response_req_id, 16 ),
                                  QString::number( m_request_id, 16 ),
                                  m_agent_address.toString(),
                                  history.join( ", " ) );
            continue;
        }

        m_request_id = -1;

        const auto& error_state_data = children.at( 1 );
        if ( QtSnmpData::INTEGER_TYPE != error_state_data.type() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected error state's type: %1 (expected INTEGER_TYPE as %2) "
                            "in a response from %3" )
                            .arg( error_state_data.type() )
                            .arg( QtSnmpData::INTEGER_TYPE )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const auto& error_index_data = children.at( 2 );
        if ( QtSnmpData::INTEGER_TYPE != error_index_data.type() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected error index's type: %1 (expected INTEGER_TYPE as %2) "
                            "in a response from %3" )
                            .arg( error_index_data.type() )
                            .arg( QtSnmpData::INTEGER_TYPE )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const int err_st = error_state_data.intValue();
        const int err_in =  error_index_data.intValue();
        if ( err_st || err_in ) {
            qDebug() << tr( "An error message received from %1.\n"
                            "Error's status: %2. Error's index: %3\n"
                            "Current job: %4" )
                            .arg( m_agent_address.toString() )
                            .arg( errorStatusText( err_st ) )
                            .arg( err_in )
                            .arg( m_current_work->description() );
            AbstractJob::ErrorResponse error;
            error.request = m_current_work->description();
            error.status = errorStatusText( err_st );
            error.index = err_in;
            error_list << error;
            continue;
        }

        const auto& variable_list_data = children.at( 3 );
        Q_ASSERT( QtSnmpData::SEQUENCE_TYPE == variable_list_data.type() );
        if ( QtSnmpData::SEQUENCE_TYPE != variable_list_data.type() ) {
            qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                        tr( "Unexpected variable list's type %1 (expected SEQUENCE_TYPE as %2) "
                            "in a response from %3" )
                            .arg( variable_list_data.type() )
                            .arg( QtSnmpData::SEQUENCE_TYPE )
                            .arg( m_agent_address.toString() );
            continue;
        }

        const auto& variable_list = variable_list_data.children();
        for ( const auto& variable : variable_list ) {
            if ( QtSnmpData::SEQUENCE_TYPE != variable.type() ) {
                qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                            tr( "Unexpected variable's type %1 (expected SEQUENCE_TYPE as %2) "
                                "in a response from %3" )
                                .arg( variable.type() )
                                .arg( QtSnmpData::SEQUENCE_TYPE )
                                .arg( m_agent_address.toString() );
                continue;
            }

            const auto& items = variable.children();
            if ( 2 != items.size() ) {
                qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                            tr( "Unexpected item count %1 (expected 2) "
                                "in a response from %3" )
                                .arg( items.size() )
                                .arg( m_agent_address.toString() );
                continue;
            }

            const auto& object = items.at( 0 );
            if ( QtSnmpData::OBJECT_TYPE != object.type() ) {
                qDebug() << tr( "An invalid SNMP response has been received.\n" ) +
                            tr( "Unexpected object's type %1 (expected OBJECT_TYPE as %2) "
                                "in a response from %3" )
                                .arg( object.type() )
                                .arg( QtSnmpData::OBJECT_TYPE )
                                .arg( m_agent_address.toString() );
                continue;
            }

            auto result_item = items.at( 1 );
            result_item.setAddress( object.data() );
            valid_list.push_back( result_item );

            if ( m_response_wait_timer.isActive() ) {
                m_response_wait_timer.stop();
            }
            m_timeout_cnt = 0;
        }
    }

    if ( m_current_work ) {
        m_current_work->processData( valid_list, error_list );
    }
}

bool Session::writeDatagram( const QByteArray& datagram ) {
    const auto res = m_socket.writeDatagram( datagram, m_agent_address, m_agent_port );
    if ( -1 == res ) {
        qDebug() << tr( "Unable to send a datagram to %1."
                        "Cause: %2" )
                        .arg( m_agent_address.toString() )
                        .arg( m_socket.errorString() );
        return false;
    }

    if ( res < datagram.size() ) {
        qDebug() << tr( "Only %1 bytes of %2 have been sent to %3.\n"
                        "Cause: %4")
                        .arg( res )
                        .arg( datagram.size() )
                        .arg( m_agent_address.toString() )
                        .arg( m_socket.errorString() );
        return false;
    }

    return ( res == datagram.size() );
}

void Session::sendRequest( const QtSnmpData& request ) {
    const auto datagram = request.makeSnmpChunk();
    if ( writeDatagram( datagram ) ) {
        m_last_request_data = request;
        Q_ASSERT( ! m_response_wait_timer.isActive() );
        m_response_wait_timer.start();
    } else {
        // NOTE: If we can't send a datagram at once,
        //       then we wont try to resend it again.
        //       We assume that the network has cirtical problem in that case,
        //       therefore sending the datagram again wont be successed.
        //       So we will cancel the current work.
        cancelWork();
    }
}

qint32 Session::createWorkId() {
    ++m_work_id;
    if ( m_work_id < 1 ) {
        m_work_id = 1;
    } else if ( m_work_id > 0x7FFF ) {
        m_work_id = 1;
    }
    return m_work_id;
}

void Session::updateRequestId() {
    const auto prev = m_request_id;
    do {
        m_request_id = 1 + abs( rand() ) % 0x7FFF;
    } while ( prev == m_request_id );

    m_request_history_queue.enqueue( m_request_id );

    while ( m_request_history_queue.count() > 10 ) {
        m_request_history_queue.dequeue();
    }
}

} // namespace qtsnmpclient
