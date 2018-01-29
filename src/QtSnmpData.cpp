#include "QtSnmpData.h"
#include <QHostAddress>
#include <math.h>

namespace {

    const char FIRST_BYTE = 0x2B;
    const auto ISO_ORG_OID = QByteArray( ".1.3" );

    QByteArray getOid( const QByteArray& data ) {
        QByteArray result;
        Q_ASSERT( data.size() > 0 );
        if( data.size() > 0 ) {
            if( FIRST_BYTE == data.at( 0 ) ) {
                result = ISO_ORG_OID;
                int current_index = 1;
                while( current_index < data.size() ) {
                    qint32 cur_val = static_cast< quint8 >( data.at( current_index ) );
                    ++current_index;
                    int big_val = 0;
                    while( cur_val >= 0x80 ) {
                        big_val *= 0x80;
                        big_val += ( cur_val - 0x80 );
                        Q_ASSERT( current_index < data.size() );
                        if( current_index < data.size() ) {
                            cur_val = static_cast< quint8 >( data.at( current_index ) );
                            ++current_index;
                        } else {
                            return QByteArray();
                        }
                    }
                    big_val *= 0x80;
                    cur_val += big_val;
                    result += "." + QByteArray::number( cur_val );
                }
            }
        }
        return result;
    }

    QByteArray packInt( int value ) {
        QByteArray result;
        if( value >= 0x80 ) {
            int max_pow = 0;

            while( pow( 0x80, max_pow ) <= value ) {
                ++max_pow;
            }

            --max_pow;

            for( int i = max_pow; i > 0; --i ) {
                const auto tmp = static_cast< int >( pow( 0x80, i ) );
                const int cur_del = ( value / tmp );
                const auto byte = static_cast< char >( cur_del + 0x80 );
                result.append( byte );
                value = value - cur_del * tmp;
            }
        }
        result.append( static_cast< char >( value ) );
        return result;
    }

    QByteArray packOid( const QByteArray& oid ) {
        QByteArray result;
        const int prefix_size = ISO_ORG_OID.size();
        const int size = oid.count();
        bool ok = (size > prefix_size);
        ok = ok && ( 0 == oid.indexOf( ISO_ORG_OID ) );
        Q_ASSERT( ok );
        if( ok ) {
            result.append( FIRST_BYTE );
            int pos = prefix_size + 1;
            int next_pos = 0;
            const auto dot = QByteArray( "." );
            while( pos < size ) {
                next_pos = oid.indexOf( dot, pos );
                if( -1 == next_pos ) {
                    next_pos = size;
                }
                const int id = oid.mid( pos, next_pos - pos ).toInt();
                result.append( packInt( id ) );
                pos = next_pos + 1;
            }
        }
        return result;
    }

    QByteArray compressLeadingZeros( const QByteArray& data ) {
        const int size = data.size();
        for( int i = 0; i < size; ++i ) {
            if( data.at( i ) ) {
                bool ok = i > 0;
                ok = ok && ( i < (size-1) );
                if( ok ) {
                    QByteArray result = data;
                    result.remove( 0, i );
                    return result;
                }
                break;
            }
        }
        return data;
    }

    QByteArray packLength( const int length ) {
        QByteArray result;
        if ( length < 0x80 ) {
            result = QByteArray( 1, static_cast< char >( length ) );
        } else {
            QByteArray ar_val;
            QDataStream( &ar_val, QIODevice::WriteOnly ) << length;
            ar_val = compressLeadingZeros( ar_val );
            const auto size = static_cast < char >( 0x80 + ar_val.size() );
            result = QByteArray( 1, size ) + ar_val;
        }
        return result;
    }

    QByteArray packContent( const int part_key, const QByteArray& content ) {
        const int count = content.count();
        QByteArray ba;
        ba.reserve( 2 + count );
        ba.append( static_cast< char >( part_key ) );
        ba.append( packLength( count ) );
        ba.append( content );
        return ba;
    }
}

QtSnmpData::QtSnmpData( const int type, const QByteArray& data )
    : m_type( type )
    , m_padding( 0 )
{
    switch( m_type ) {
    case INTEGER_TYPE:
    case GAUGE_TYPE: {
            // NOTE: RFC defines, that GAUGE_TYPE is UNSIGNED INTERGER (4 bytes)
            // But Cisco returns ifSpeed as Gauge5Bytes, with zero lead byte
            const size_t expected_size = sizeof( qint32 );
            const size_t real_size = static_cast< size_t >( data.size() );
            if( real_size < expected_size ) {
                m_data = data;
                const int size_diff = static_cast< int >( expected_size - real_size );
                m_data.prepend( QByteArray( size_diff, '\x0' ) );
            } else {
                m_data = data.right( 4 );
            }
        }
        break;
    case NULL_DATA_TYPE:
        break;
    case OBJECT_TYPE:
        m_data = getOid( data );
        break;
    case SEQUENCE_TYPE:
    case GET_REQUEST_TYPE:
    case GET_NEXT_REQUEST_TYPE:
    case GET_RESPONSE_TYPE:
    case SET_REQUEST_TYPE:
        m_childs = parseData( data );
        break;
    case COUNTER_TYPE:
    case TIME_TICKS_TYPE:
        m_data = data;
        m_data.prepend( QByteArray( static_cast< int >(sizeof( qint64 )) - m_data.size(), '\x0' ) );
        break;
    default:
        m_data = data;
        break;
    }
}

bool QtSnmpData::isValid() const {
    switch( m_type ) {
    case INTEGER_TYPE:
    case GAUGE_TYPE:
    case IP_ADDR_TYPE:
        return sizeof( qint32 ) == static_cast< unsigned >( m_data.size() );
    case TIME_TICKS_TYPE:
    case COUNTER_TYPE:
        return sizeof( quint64 ) == static_cast< unsigned >( m_data.size() );
    case NULL_DATA_TYPE:
    case SEQUENCE_TYPE:
    case GET_REQUEST_TYPE:
    case GET_NEXT_REQUEST_TYPE:
    case GET_RESPONSE_TYPE:
        return m_data.isEmpty();
    case SET_REQUEST_TYPE:
        return !m_data.isEmpty();
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
    switch( m_type ) {
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
    switch( m_type ) {
    case TIME_TICKS_TYPE:
    case COUNTER_TYPE: {
            qint64 val;
            QDataStream( m_data ) >> val;
            return val;
        }
    case IP_ADDR_TYPE:
    case GAUGE_TYPE:
    case INTEGER_TYPE: {
            qint32 val;
            QDataStream( m_data ) >> val;
            return val;
        }
    default: break;
    }
    Q_ASSERT( false );
    return 0;
}

QString QtSnmpData::textValue() const {
    if( STRING_TYPE == m_type ) {
        return QString::fromLatin1( m_data );
    }
    Q_ASSERT( false );
    return QString();
}

QByteArray QtSnmpData::address() const {
    return m_address;
}

void QtSnmpData::setAddress( const QByteArray& value ) {
    m_address = value;
}

QList< QtSnmpData > QtSnmpData::children() const {
    return m_childs;
}

void QtSnmpData::addChild( const QtSnmpData& child ) {
    m_childs << child;
}

QByteArray QtSnmpData::makeSnmpChunk() const {
    switch( m_type ) {
    case OBJECT_TYPE:
        return packContent( m_type, packOid(m_data) );
    case INTEGER_TYPE:
    case IP_ADDR_TYPE:
        return packContent( m_type, compressLeadingZeros( m_data ) );
    case GAUGE_TYPE:
    case COUNTER_TYPE:
    case TIME_TICKS_TYPE: {
            QByteArray chunk = compressLeadingZeros( m_data );
            const auto first_byte = static_cast< quint8 >( chunk.at( 0 ) );
            if( 0x7f < first_byte ) {
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
            foreach( const QtSnmpData& child, m_childs ) {
                chunk += child.makeSnmpChunk();
            }
            return packContent( m_type, chunk );
        }
    default: break;
    }
    return packContent( m_type, m_data );
}

QVariant QtSnmpData::toVariant() const {
    switch( m_type ) {
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
    QByteArray value_data;
    QDataStream stream( &value_data, QIODevice::WriteOnly );
    stream << value;
    return QtSnmpData( INTEGER_TYPE, value_data );
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
    data.m_data = oid.trimmed();
    return data;
}

QList< QtSnmpData > QtSnmpData::parseData( const QByteArray& data ) { // static
    QList< QtSnmpData > result;
    QByteArray parsed_data( data );
    while( parsed_data.size() ) {
        Q_ASSERT( parsed_data.size() >= 2 );
        if( parsed_data.size() >= 2 ) {
            int data_length = static_cast< quint8 >( parsed_data.at( 1 ) );
            int size_lengh = 1;
            if( data_length > 0x80 ) {
                size_lengh = 1 + (data_length - 0x80);
                Q_ASSERT( parsed_data.size() > size_lengh );
                if( parsed_data.size() > size_lengh ) {
                    auto length_data = parsed_data.mid( 2, size_lengh - 1 );
                    const int sizeof_int = static_cast< int >( sizeof(int) );
                    const int leading_zeros_count = sizeof_int - length_data.size();
                    Q_ASSERT( leading_zeros_count >= 0 );
                    if( leading_zeros_count > 0 ) {
                        length_data.insert( 0, QByteArray( leading_zeros_count, 0 ) );
                    }
                    QDataStream( length_data ) >> data_length;
                } else {
                    qWarning() << Q_FUNC_INFO << "invalid packet size";
                    break;
                }
            }

            const int type_lenght = 1;
            const auto packet_size = type_lenght + size_lengh + data_length;
            Q_ASSERT( parsed_data.size() >= packet_size );
            if( parsed_data.size() >= packet_size ) {
                const int type = static_cast< quint8 >( parsed_data.at( 0 ) );
                const auto data = parsed_data.mid( type_lenght + size_lengh, data_length );
                parsed_data.remove( 0, packet_size );
                const auto pack = QtSnmpData( type, data );
                if( pack.isValid() ) {
                    result << pack;
                } else {
                    qWarning() << Q_FUNC_INFO << "error in packet parsing";
                    break;
                }
            }
        } else {
            qWarning() << Q_FUNC_INFO << "invalid packet size";
        }
    }
    return result;
}

QDataStream& operator<<( QDataStream& stream, const QtSnmpData& obj ) {
    stream << obj.m_type;
    stream << obj.m_data;
    stream << obj.m_childs;
    stream << obj.m_address;
    return stream;
}

QDataStream& operator>>( QDataStream& stream, QtSnmpData& obj ) {
    stream >> obj.m_type;
    stream >> obj.m_data;
    stream >> obj.m_childs;
    stream >> obj.m_address;
    return stream;
}

QDebug operator<<( QDebug stream, const QtSnmpData& obj ) {
    stream << "SnmpData( ";
    stream << "type: " << obj.typeDescription() << ", ";
    const QByteArray data = obj.data();
    stream << "data (" << data.size() << "): " << data.toHex() << "; ";
    stream << "childes: " << obj.children() << ", ";
    stream << "address: " << obj.address() << " )";
    return stream;
}

bool operator==( const QtSnmpData& left, const QtSnmpData& right ) {
    bool res = true;
    if( &left != &right ) {
        res = res && ( left.m_type == right.m_type );
        res = res && ( left.m_data == right.m_data );
        res = res && ( left.m_childs == right.m_childs );
        res = res && ( left.m_address == right.m_address );
    }
    return res;
}

bool operator!=( const QtSnmpData& left, const QtSnmpData& right ) {
    return !( left == right );
}
