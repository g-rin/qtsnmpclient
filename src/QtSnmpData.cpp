#include "QtSnmpData.h"
#include <QHostAddress>
#include <math.h>
#include <inttypes.h>

namespace {

    const auto ISO_ORG_OID = QByteArray( ".1.3" );

    template< typename T >
    T swapBytes( const T& val ) {
        T res;
        char* res_ptr = reinterpret_cast< char* >( &res );
        const char* src_ptr = reinterpret_cast< const char* >( &val );
        for ( int i = sizeof(T) - 1; i >= 0; --i ) {
            *(res_ptr + i) = *(src_ptr + sizeof(T) - i - 1 );
        }
        return res;
    }

    QByteArray packOid( const QByteArray& oid ) {
        const int prefix_size = ISO_ORG_OID.size();
        const int size = oid.size();
        bool ok = (size > prefix_size);
        ok = ok && ( 0 == oid.indexOf( ISO_ORG_OID ) );
        Q_ASSERT( ok );
        if ( not ok ) {
            return {};
        }

        QByteArray result;
        result.reserve( 2*oid.size() );
        const char first_byte = 0x2B;
        result.append( first_byte );
        int pos = prefix_size + 1;
        int next_pos = 0;
        const char dot = '.';

        qint32 value;
        qint32 tmp;
        qint32 cur_del;
        qint8 byte;
        int max_pow;

        while ( pos < size ) {
            next_pos = oid.indexOf( dot, pos );
            if ( -1 == next_pos ) {
                next_pos = size;
            }

            value = QByteArray::fromRawData( oid.constData() + pos, next_pos - pos ).toInt();
            if ( value >= 0x80 ) {
                max_pow = 0;

                while ( pow( 0x80, max_pow ) <= value ) {
                    ++max_pow;
                }
                --max_pow;

                for ( int i = max_pow; i > 0; --i ) {
                    tmp = static_cast< int >( pow( 0x80, i ) );
                    cur_del = ( value / tmp );
                    byte = static_cast< qint8 >( cur_del + 0x80 );
                    result.append( byte );
                    value = value - cur_del * tmp;
                }
            }
            result.append( static_cast< char >( value ) );
            pos = next_pos + 1;
        }
        return result;
    }

    QByteArray packContent( const int part_key, const QByteArray& content ) {
        const int32_t content_size = content.size();
        const int size_of_size = sizeof( int32_t );
        QByteArray result;

        // NOTE: 4 is a gap for type, size of size and size itsefl (mostly less then 255 as 0xFF)
        result.reserve( content_size + 4 );
        result.append( static_cast< char >( part_key ) );
        if ( content_size < 0x80 ) {
            result.append( static_cast< char >( content_size ) );
        } else {
            const int32_t tmp = swapBytes( content_size );
            const char* ptr = reinterpret_cast< const char* >( &tmp );
            for ( int i = 0; i < size_of_size; ++i ) {
                if ( 0 == *( ptr + i ) ) {
                    continue;
                }
                result.append( static_cast< char >( 0x80 + size_of_size - i ) );
                result.append( ptr + i, size_of_size - i );
                break;
            }
        }
        result.append( content );
        return result;
    }
}

QtSnmpData::QtSnmpData( const int type, const QByteArray data )
    : m_type( type )
{
    switch ( m_type ) {
    case OBJECT_TYPE:
        Q_ASSERT( data.size() > 0 );
        m_data.reserve( 2*data.size() );
        m_data.append( ISO_ORG_OID );
        for ( int current_index = 1; current_index < data.size(); ) {
            int32_t cur_val = static_cast< uint8_t >( data.at( current_index ) );
            ++current_index;
            int big_val = 0;
            while ( cur_val >= 0x80 ) {
                big_val *= 0x80;
                big_val += ( cur_val - 0x80 );
                Q_ASSERT( current_index < data.size() );
                if ( current_index < data.size() ) {
                    cur_val = static_cast< uint8_t >( data.at( current_index ) );
                    ++current_index;
                } else {
                    m_data = {};
                    return;
                }
            }

            big_val *= 0x80;
            cur_val += big_val;
            m_data.append( "." + QByteArray::number( cur_val ) );
        }
        m_data.squeeze();
        return;
    case SEQUENCE_TYPE:
    case GET_REQUEST_TYPE:
    case GET_NEXT_REQUEST_TYPE:
    case GET_RESPONSE_TYPE:
    case SET_REQUEST_TYPE:
        parseData( data, &m_children );
        return;
    case TIME_TICKS_TYPE:
        m_data.reserve( sizeof( int64_t ) );
        m_data.append( m_data.capacity() - data.size(), 0 );
        m_data.append( data.right( m_data.capacity() ) );
        return;
    default: break;
    }

    m_data.append( data );
}

bool QtSnmpData::isValid() const {
    switch ( m_type ) {
    case INTEGER_TYPE:
    case GAUGE_TYPE:
    case COUNTER_TYPE:
        // NOTE: according to BER (Basic Encoding Rules for ASN.1)
        //       a correctly encoded integer could not have all
        //       of the first 9 bits are set to the same value.

        if ( 1 == m_data.size() ) {
            return true;
        } else if ( m_data.size() > 1 ) {
            if ( ( static_cast< char >( 0xFF ) == m_data.at( 0 ) ) &&
                 ( static_cast< char >( 0x80 ) & m_data.at( 1 ) ) )
            {
                return false;
            }

            if ( ( 0 == m_data.at( 0 ) ) &&
                 ( static_cast< char >( 0x80 ) & ~m_data.at( 1 ) ) )
            {
                return false;
            }

            return true;
        }
        return false;
    case IP_ADDR_TYPE:
        return 4 == m_data.size();
    case TIME_TICKS_TYPE:
        return 8 == m_data.size();
    case NULL_DATA_TYPE:
    case SEQUENCE_TYPE:
    case GET_REQUEST_TYPE:
    case GET_NEXT_REQUEST_TYPE:
    case GET_RESPONSE_TYPE:
    case SET_REQUEST_TYPE:
        return 0 == m_data.size();
    case OBJECT_TYPE:
    case STRING_TYPE:
        return true;
    default: break;
    }
    return false;
}

int QtSnmpData::type() const {
    return m_type;
}

QString QtSnmpData::typeDescription() const {
    switch ( m_type ) {
    case INVALID_TYPE:
        return "INVALID_TYPE";
    case INTEGER_TYPE:
        return "INTEGER_TYPE";
    case STRING_TYPE:
        return "STRING_TYPE";
    case NULL_DATA_TYPE:
        return "NULL_DATA_TYPE";
    case OBJECT_TYPE:
        return "OBJECT_TYPE";
    case SEQUENCE_TYPE:
        return "SEQUENCE_TYPE";
    case IP_ADDR_TYPE:
        return "IP_ADDR_TYPE";
    case COUNTER_TYPE:
        return "COUNTER_TYPE";
    case GAUGE_TYPE:
        return "GAUGE_TYPE";
    case TIME_TICKS_TYPE:
        return "TIME_TICKS_TYPE";
    case GET_REQUEST_TYPE:
        return "GET_REQUEST_TYPE";
    case GET_NEXT_REQUEST_TYPE:
        return "GET_NEXT_REQUEST_TYPE";
    case GET_RESPONSE_TYPE:
        return "GET_RESPONSE_TYPE";
    case SET_REQUEST_TYPE:
        return "SET_REQUEST_TYPE";
    default: break;
    }
    return QString( "Unsupported Type (%1)" ).arg( m_type );
}

QByteArray QtSnmpData::data() const {
    return m_data;
}

int QtSnmpData::intValue() const {
    return static_cast< int >( longLongValue() );
}

unsigned int QtSnmpData::uintValue() const {
    return static_cast< unsigned int >( longLongValue() );
}

qint64 QtSnmpData::longLongValue() const {
    switch ( m_type ) {
    case TIME_TICKS_TYPE: {
            int64_t val;
            memcpy( &val, m_data.constData(), 8 );
            return  swapBytes( val );
        }
    case IP_ADDR_TYPE: {
            int32_t val;
            memcpy( &val, m_data.constData(), 4 );
            return  swapBytes( val );
        }
    case GAUGE_TYPE:
    case COUNTER_TYPE:
    case INTEGER_TYPE: {

        // NOTE: According to BER (Basic Encoding Rules for ASN.1)
        //       a negative number has the first bit is set to 1 ( -4 as 0xFC ).
        //       The analogous code positive number has 'zero' (0x00)
        //       before the most significant byte if it has 1 at the most
        //       significant byte ( 252 as 0x00FC )

            const bool is_negative = m_data.at( 0 ) & 0x80;
            QByteArray buf;
            buf.reserve( 8 );
            for ( int i = 0; i < m_data.size(); ++i ) {
                buf.prepend( m_data.at( i ) );
            }

            while ( buf.size() < 8 ) {
                buf.append(  is_negative ? static_cast< char >( 0xFF ) : 0 );
            }
            qint64 res = *(reinterpret_cast< qint64* >( buf.data() ));
            return res;
        }
    default: break;
    }
    Q_ASSERT( false );
    return 0;
}

QString QtSnmpData::textValue() const {
    if ( STRING_TYPE == m_type ) {
        return m_data.constData();
    }
    Q_ASSERT( false );
    return {};
}

QByteArray QtSnmpData::address() const {
    return m_address;
}

void QtSnmpData::setAddress( const QByteArray& value ) {
    m_address = value;
    int capacity = 2;
    while ( capacity < m_address.size() ) {
        capacity *= 2;
    }
    if ( capacity > m_address.size() ) {
        m_address.reserve( capacity );
    }
}

const std::vector< QtSnmpData >& QtSnmpData::children() const {
    return m_children;
}

void QtSnmpData::addChild( const QtSnmpData& child ) {
    if ( 0 == m_children.capacity() ) {
        m_children.reserve( 2 );
    } else if ( m_children.capacity() == m_children.size() ) {
        m_children.reserve( 2*m_children.capacity() );
    }
    m_children.push_back( child );
}

QByteArray QtSnmpData::makeSnmpChunk() const {
    switch ( m_type ) {
    case OBJECT_TYPE:
        return packContent( m_type, packOid(m_data) );
    case INTEGER_TYPE:
    case IP_ADDR_TYPE:
        return packContent( m_type, m_data );
    case GAUGE_TYPE:
    case COUNTER_TYPE:
    case TIME_TICKS_TYPE: {
            QByteArray chunk;
            // Expected size of 14 bytes consist of:
            //   8 for data (uint64);
            //   1 + additional byte if the first byte is less then 0x7f
            //   1 for data's size
            //   4 for type
            chunk.reserve( 32 );
            Q_ASSERT( m_data.size() <= 8 );
            chunk.append( m_data );
            const auto first_byte = static_cast< quint8 >( chunk.at( 0 ) );
            if ( 0x7f < first_byte ) {
                chunk.prepend( '\x0' );
            }
            Q_ASSERT( chunk.count() <= 127 );
            const char count = static_cast< char >( chunk.count() );
            chunk.prepend( count );
            chunk.prepend( static_cast< char >( m_type ) );
            return chunk;
        }
    case SEQUENCE_TYPE:
    case GET_REQUEST_TYPE:
    case GET_NEXT_REQUEST_TYPE:
    case GET_RESPONSE_TYPE:
    case SET_REQUEST_TYPE:
        {
            QByteArray chunk;
            // NOTE: we don't know how much is needed exactly, but is
            //       it must fit into standard UDP datagram (512 bytes) at any case.
            chunk.reserve( 512 );

            for ( const auto& child : m_children ) {
                chunk.append( child.makeSnmpChunk() );
            }
            return packContent( m_type, chunk );
        }
    default: break;
    }
    return packContent( m_type, m_data );
}

QVariant QtSnmpData::toVariant() const {
    switch ( m_type ) {
    case INTEGER_TYPE:
    case GAUGE_TYPE:
    case COUNTER_TYPE:
    case TIME_TICKS_TYPE:
        return intValue();
    case STRING_TYPE:
        return QString::fromLocal8Bit( m_data );
    case IP_ADDR_TYPE:
        return QHostAddress( m_data.toUInt() ).toString();
    default: break;
    }
    return QVariant();
}

QtSnmpData QtSnmpData::integer( const int value ) { // static
    // NOTE: According to BER (Basic Encoding Rules for ASN.1)
    //       a negative number must has the first bit is set to 1 ( -4 as 0xFC ).
    //       The analogous code positive number must has 'zero' (0x00)
    //       before the most significant byte if it has 1 at the most
    //       significant byte ( 252 as 0x00FC ).
    //       Also redundant bytes (consequantly leading 0x00 for positive number,
    //       or consequantly leading 0xFF for negative number) shall be truncated.

    const int64_t tmp = swapBytes( static_cast< int64_t >( value ) );
    QByteArray data( reinterpret_cast< const char* >( &tmp ), sizeof(tmp) );
    for ( int i = 0; i < data.size() - 1; ++i ) {
        switch ( data.at( i ) ) {
        case static_cast< char >( 0xFF ):
            if ( data.at( i + 1 ) & 0x80 ) {
                continue;
            }
            return QtSnmpData( INTEGER_TYPE, data.mid( i ) );
        case 0:
            if ( ~data.at( i + 1 ) & 0x80 ) {
                continue;
            }
            return QtSnmpData( INTEGER_TYPE, data.mid( i ) );
        default:
            return QtSnmpData( INTEGER_TYPE, data.mid( i ) );
        }
    }

    return QtSnmpData( INTEGER_TYPE, data.right( 1 ) );
}

QtSnmpData QtSnmpData::null() { // static
    return QtSnmpData( NULL_DATA_TYPE );
}

QtSnmpData QtSnmpData::string( const QByteArray& value ) { // static
    return QtSnmpData( STRING_TYPE, value );
}

QtSnmpData QtSnmpData::sequence() { // static
    return QtSnmpData( SEQUENCE_TYPE );
}

QtSnmpData QtSnmpData::oid( const QByteArray& oid ) { // static
    QtSnmpData data;
    data.m_type = OBJECT_TYPE;
    data.m_data.append( oid );
    return data;
}

void QtSnmpData::parseData( const QByteArray& data_for_parsing,
                            std::vector< QtSnmpData >*const parsed_data_list) // static
{
    Q_ASSERT( parsed_data_list && ( 0 == parsed_data_list->size() ) );
    parsed_data_list->reserve( 2 );

    const int size_of_size= 4; // sizeof int32

    const int total_size = data_for_parsing.size();
    int read_bytes = 0;

    while ( read_bytes < total_size ) {
        if ( (total_size - read_bytes) < 2 ) {
            qWarning() << "Invalid size of a chunk: " << (total_size - read_bytes);
            break;
        }

        const uint8_t type = static_cast< uint8_t >( data_for_parsing.at( read_bytes++ ) );

        int32_t data_length = static_cast< uint8_t >( data_for_parsing.at( read_bytes ) );
        const int size_length = 1 + std::max( 0, data_length - 0x80);
        if ( size_length > 1 ) {
            if ( (total_size - read_bytes) <= size_length ) {
                qWarning() << "Invalid packet's size";
                break;
            }
            data_length = 0;
            memcpy( reinterpret_cast< char* >( &data_length ) + size_of_size - size_length + 1,
                    data_for_parsing.constData() + read_bytes + 1,
                    static_cast< size_t >( size_length - 1 ) );
            data_length = swapBytes( data_length );
        }
        read_bytes += size_length;

        if ( (total_size - read_bytes) < data_length ) {
            qWarning() << "Error #2 during parsing a packet";
            break;
        }

        const QtSnmpData item( type, QByteArray::fromRawData( data_for_parsing.constData() + read_bytes, data_length ) );
        read_bytes += data_length;

        if ( parsed_data_list->capacity() == parsed_data_list->size() ) {
            parsed_data_list->reserve( 2*parsed_data_list->capacity() );
        }

        parsed_data_list->push_back( item );
    }
}

QDataStream& operator<<( QDataStream& stream, const QtSnmpData& obj ) {
    const auto prev_version = stream.version();
    stream.setVersion( QDataStream::Qt_4_5 );
    stream << obj.m_type;
    stream << obj.m_data;
    stream << static_cast< qint32 >( obj.m_children.size() );
    for ( const auto& child : obj.m_children ) {
        stream << child;
    }
    stream << obj.m_address;
    stream.setVersion( prev_version );
    return stream;
}

QDataStream& operator>>( QDataStream& stream, QtSnmpData& obj ) {
    const auto prev_version = stream.version();
    stream.setVersion( QDataStream::Qt_4_5 );
    stream >> obj.m_type;
    stream >> obj.m_data;
    qint32 tmp;
    stream >> tmp;
    const size_t count = static_cast< size_t >( tmp );
    obj.m_children.resize( count );
    for ( size_t i = 0; i < count; ++i ) {
        stream >> obj.m_children[ i ];
    }
    stream >> obj.m_address;
    stream.setVersion( prev_version );
    return stream;
}

QDebug operator<<( QDebug stream, const QtSnmpData& obj ) {
    stream << "SnmpData( ";
    stream << "type: " << obj.typeDescription() << "; ";
    const QByteArray data = obj.data();
    stream << "data (" << data.size() << "): " << data.toHex() << "; ";
    stream << "address: " << obj.address() << "; ";
    stream << "children ( count: " << obj.children().size();
    for ( const auto& child : obj.children() ) {
        stream << child << " ";
    }
    stream << ")";
    return stream;
}

bool operator==( const QtSnmpData& left, const QtSnmpData& right ) {
    bool res = true;
    if ( &left != &right ) {
        res = res && ( left.m_type == right.m_type );
        res = res && ( left.m_data == right.m_data );
        res = res && ( left.m_children == right.m_children );
        res = res && ( left.m_address == right.m_address );
    }
    return res;
}

bool operator!=( const QtSnmpData& left, const QtSnmpData& right ) {
    return !( left == right );
}
