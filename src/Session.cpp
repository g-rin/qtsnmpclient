#include "Session.h"
#include "QtSnmpData.h"
#include "RequestValuesJob.h"
#include "RequestSubValuesJob.h"
#include "SetValueJob.h"
#include "defines.h"
#include <QDateTime>
#include <QHostAddress>
#include <math.h>

namespace qtsnmpclient {

namespace {
    const int default_response_timeout = 10000;
    const quint16 SnmpPort = 161;

    QString errorStatusText( const int val ) {
        switch( val ) {
        case 0:
            return "No errors";
        case 1:
            return "Too big";
        case 2:
            return "No such name";
        case 3:
            return "Bad value";
        case 4:
            return "Read only";
        case 5:
            return "Other errors";
        default:
            Q_ASSERT( false );
            break;
        }
        return QString( "Unsupported error(%1)" ).arg( val );
    }
}

Session::Session( QObject*const parent )
    : QObject( parent )
    , m_community( "public" )
    , m_socket( new QUdpSocket( this ) )
    , m_response_wait_timer( new QTimer( this ) )
{
    connect( m_socket,
             SIGNAL(readyRead()),
             SLOT(onReadyRead()) );
    connect( m_response_wait_timer,
             SIGNAL(timeout()),
             SLOT(cancelWork()) );
    m_response_wait_timer->setInterval( default_response_timeout );
}

QHostAddress Session::agentAddress() const {
    return m_agent_address;
}

void Session::setAgentAddress( const QHostAddress& value ) {
    bool ok = !value.isNull();
    ok = ok && ( QHostAddress( QHostAddress::Any ) != value );
    if( ok ) {
        m_agent_address = value;
        m_socket->close();
        m_socket->bind();
    } else {
        qWarning() << Q_FUNC_INFO << "attempt to set invalid agent address( " << value << ")";
    }
}

QByteArray Session::community() const {
    return m_community;
}

void Session::setCommunity( const QByteArray& value ) {
    m_community = value;
}

int Session::responseTimeout() const {
    return m_response_wait_timer->interval();
}

void Session::setResponseTimeout( const int value ) {
    if( value != m_response_wait_timer->interval() ) {
        m_response_wait_timer->setInterval( value );
    }
}

bool Session::isBusy() const {
    return !m_current_work.isNull() || !m_work_queue.isEmpty();
}

qint32 Session::requestValues( const QStringList& oid_list ) {
    const qint32 work_id = createWorkId();
    addWork( JobPointer( new RequestValuesJob( this,
                                               work_id,
                                               oid_list) ) );
    return work_id;
}

qint32 Session::requestSubValues( const QString& oid ) {
    const qint32 work_id = createWorkId();
    addWork( JobPointer( new RequestSubValuesJob( this,
                                                  work_id,
                                                  oid ) ) );
    return work_id;
}

qint32 Session::setValue( const QByteArray& community,
                          const QString& oid,
                          const int type,
                          const QByteArray& value )
{
    const qint32 work_id = createWorkId();
    addWork( JobPointer( new SetValueJob( this,
                                          work_id,
                                          community,
                                          oid,
                                          type,
                                          value ) ) );
    return work_id;
}

void Session::addWork( const JobPointer& work ) {
    IN_QOBJECT_THREAD( work );
    const int queue_limit = 10;
    if( m_work_queue.count() < queue_limit ) {
        m_work_queue.push_back( work );
        startNextWork();
    } else {
        qWarning() << "Warning: snmp request( " << work->description() << ") "
                   << "to " << m_agent_address.toString() << " dropped, queue if full";
    }
}

void Session::startNextWork() {
    if( m_current_work.isNull() && !m_work_queue.isEmpty() ){
        m_current_work = m_work_queue.takeFirst();
        m_current_work->start();
    }
}

void Session::completeWork( const QtSnmpDataList& values ) {
    Q_ASSERT( ! m_current_work.isNull() );
    emit responseReceived( m_current_work->id(), values );
    m_current_work.clear();
    startNextWork();
}

void Session::cancelWork() {
    if( !m_current_work.isNull() ) {
        emit requestFailed( m_current_work->id() );
        m_current_work.clear();
    }
    m_response_wait_timer->stop();
    m_request_id = -1;
    startNextWork();
}

void Session::sendRequestGetValues( const QStringList& names ) {
    Q_ASSERT( -1 == m_request_id );
    if( -1 == m_request_id ) {
        updateRequestId();
        QtSnmpData full_packet = QtSnmpData::sequence();
        full_packet.addChild( QtSnmpData::integer( 0 ) );
        full_packet.addChild( QtSnmpData::string( m_community ) );
        QtSnmpData request( QtSnmpData::GET_REQUEST_TYPE );
        request.addChild( QtSnmpData::integer( m_request_id ) );
        request.addChild( QtSnmpData::integer( 0 ) );
        request.addChild( QtSnmpData::integer( 0 ) );
        QtSnmpData seq_all_obj = QtSnmpData::sequence();
        for( const auto& oid_key : names ) {
            QtSnmpData seq_obj_info = QtSnmpData::sequence();
            seq_obj_info.addChild( QtSnmpData::oid( oid_key.toLatin1() ) );
            seq_obj_info.addChild( QtSnmpData::null() );
            seq_all_obj.addChild( seq_obj_info );
        }
        request.addChild( seq_all_obj );
        full_packet.addChild( request );
        sendDatagram( full_packet.makeSnmpChunk() );
    } else {
        qWarning() << Q_FUNC_INFO << "we already wait response";
    }
}

void Session::sendRequestGetNextValue( const QString& name ) {
    Q_ASSERT( -1 == m_request_id );
    if( -1 == m_request_id ) {
        updateRequestId();
        QtSnmpData full_packet = QtSnmpData::sequence();
        full_packet.addChild( QtSnmpData::integer( 0 ) );
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
        sendDatagram( full_packet.makeSnmpChunk() );
    } else {
        qWarning() << Q_FUNC_INFO << "we already wait response";
    }
}

void Session::sendRequestSetValue( const QByteArray& community,
                                   const QString& name,
                                   const int type,
                                   const QByteArray& value )
{
    Q_ASSERT( -1 == m_request_id );
    if( -1 == m_request_id ) {
        updateRequestId();
        auto pdu_packet = QtSnmpData::sequence();
        pdu_packet.addChild( QtSnmpData::integer( 0 ) );
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
        sendDatagram( pdu_packet.makeSnmpChunk() );
    } else {
        qWarning() << Q_FUNC_INFO << "we already wait response";
    }
}

void Session::onReadyRead() {
    const int size = static_cast< int >( m_socket->pendingDatagramSize() );
    if( size ) {
        QByteArray datagram;
        datagram.resize( size );
        m_socket->readDatagram( datagram.data(), size );

        const auto& list = getResponseData( datagram );
        if( !list.isEmpty() ) {
            Q_ASSERT( !m_current_work.isNull() );
            m_current_work->processData( list );
        }
    }
}

QtSnmpDataList Session::getResponseData( const QByteArray& datagram ) {
    QtSnmpDataList result;
    const auto list = QtSnmpData::parseData( datagram );
    Q_ASSERT( !list.isEmpty() );
    for( const auto& packet : list ) {
        const QtSnmpDataList resp_list = packet.children();
        if( 3 == resp_list.count() ) {
            const auto& resp = resp_list.at( 2 );
            Q_ASSERT( QtSnmpData::GET_RESPONSE_TYPE == resp.type() );
            if( QtSnmpData::GET_RESPONSE_TYPE != resp.type() ) {
                qWarning() << Q_FUNC_INFO << "Err: unexpected response type";
                return result;
            }

            const auto& children = resp.children();
            Q_ASSERT( 4 == children.count() );
            if( 4 != children.count() ) {
                qWarning() << Q_FUNC_INFO << "Err: unexpected child count";
                return result;
            }

            const auto& request_id_data = children.at( 0 );
            Q_ASSERT( QtSnmpData::INTEGER_TYPE == request_id_data.type() );
            if( QtSnmpData::INTEGER_TYPE != request_id_data.type() ) {
                qWarning() << Q_FUNC_INFO << "Err: request id type";
                return result;
            }

            const int response_req_id = request_id_data.intValue();
            Q_ASSERT( response_req_id == m_request_id );
            if( response_req_id == m_request_id ) {
                m_response_wait_timer->stop();
                m_request_id = -1;
            } else {
                qWarning() << Q_FUNC_INFO
                           << "Err: unexpected request_id: " << response_req_id
                           << " (expected: " << m_request_id << ")";
                return result;
            }

            const auto& error_state_data = children.at( 1 );
            Q_ASSERT( QtSnmpData::INTEGER_TYPE == error_state_data.type() );
            if( QtSnmpData::INTEGER_TYPE != error_state_data.type() ) {
                qWarning() << Q_FUNC_INFO << "Err: unexpected error state type";
                return result;
            }

            const auto& error_index_data = children.at( 2 );
            Q_ASSERT( QtSnmpData::INTEGER_TYPE == error_index_data.type() );
            if( QtSnmpData::INTEGER_TYPE != error_index_data.type() ) {
                qWarning() << Q_FUNC_INFO << "Err: unexpected error index type";
                return result;
            }
            const int err_st = error_state_data.intValue();
            const int err_in =  error_index_data.intValue();
            if( err_st || err_in ) {
                qWarning() << Q_FUNC_INFO << QString( "error [ status: %1; index: %2 ] in answer from %3 for %4" )
                                        .arg( errorStatusText( err_st ) )
                                        .arg( QString::number( err_in ) )
                                        .arg( m_agent_address.toString() )
                                        .arg( m_current_work->description() );
                cancelWork();
                return result;
            }

            const auto& variable_list_data = children.at( 3 );
            Q_ASSERT( QtSnmpData::SEQUENCE_TYPE == variable_list_data.type() );
            if( QtSnmpData::SEQUENCE_TYPE != variable_list_data.type() ) {
                qWarning() << Q_FUNC_INFO << "Err: unexpected variable list type";
                return result;
            }

            const auto& variable_list = variable_list_data.children();
            for( int i = variable_list.count() - 1; i >= 0; --i ) {
                const auto& variable = variable_list.at( i );
                if( QtSnmpData::SEQUENCE_TYPE == variable.type() ) {
                    const QtSnmpDataList& items = variable.children();
                    if( 2 == items.count() ) {
                        const auto& object = items.at( 0 );
                        if( QtSnmpData::OBJECT_TYPE == object.type() ) {
                            auto result_item = items.at( 1 );
                            result_item.setAddress( object.data() );
                            result << result_item;
                        } else {
                            qWarning() << Q_FUNC_INFO << "invalid packet content";
                            Q_ASSERT( false );
                        }
                    } else {
                        qWarning() << Q_FUNC_INFO << "invalid packet type";
                        Q_ASSERT( false );
                    }
                } else {
                    qWarning() << Q_FUNC_INFO << "invalid packet type";
                    Q_ASSERT( false );
                }
            }
        } else {
            qWarning() << Q_FUNC_INFO << "parse_get_response error 2";
        }
    }
    return result;
}

void Session::sendDatagram( const QByteArray& datagram ) {
    if( m_socket->writeDatagram( datagram, m_agent_address, SnmpPort ) ) {
        Q_ASSERT( ! m_response_wait_timer->isActive() );
        m_response_wait_timer->start();
    } else {
        Q_ASSERT( false );
        cancelWork();
    }
}

qint32 Session::createWorkId() {
    ++m_work_id;
    if( m_work_id < 1 ) {
        m_work_id = 1;
    } else if( m_work_id > 0x7FFF ) {
        m_work_id = 1;
    }
    return m_work_id;
}

void Session::updateRequestId() {
    m_request_id = 1 + abs( rand() ) % 0x7FFF;
}

} // namespace qtsnmpclient
