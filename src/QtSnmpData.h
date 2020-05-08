#pragma once

#include <QByteArray>
#include <QVariant>
#include <QMap>
#include <QMetaType>
#include <QDataStream>
#include <QDebug>
#include <vector>
#include "win_export.h"

class QtSnmpData;

WIN_EXPORT QDataStream& operator<<( QDataStream&, const QtSnmpData& );
WIN_EXPORT QDataStream& operator>>( QDataStream&, QtSnmpData& );
WIN_EXPORT QDebug operator<<( QDebug, const QtSnmpData& );
WIN_EXPORT bool operator==( const QtSnmpData&, const QtSnmpData& );
WIN_EXPORT bool operator!=( const QtSnmpData&, const QtSnmpData& );

class WIN_EXPORT QtSnmpData {
    friend WIN_EXPORT QDataStream& ::operator<<( QDataStream&, const QtSnmpData& );
    friend WIN_EXPORT QDataStream& ::operator>>( QDataStream&, QtSnmpData& );
    friend WIN_EXPORT bool ::operator==( const QtSnmpData&, const QtSnmpData& );
    friend WIN_EXPORT bool ::operator!=( const QtSnmpData&, const QtSnmpData& );
public:
    enum {
        INVALID_TYPE = -1,
        INTEGER_TYPE = 0x02,
        STRING_TYPE = 0x04,
        NULL_DATA_TYPE = 0x05,
        OBJECT_TYPE = 0x06,
        SEQUENCE_TYPE = 0x30,
        IP_ADDR_TYPE = 0x40,
        COUNTER_TYPE = 0x41,
        GAUGE_TYPE = 0x42,
        TIME_TICKS_TYPE = 0x43,
        GET_REQUEST_TYPE = 0xA0,
        GET_NEXT_REQUEST_TYPE = 0xA1,
        GET_RESPONSE_TYPE = 0xA2,
        SET_REQUEST_TYPE = 0xA3,
    };

public:
    QtSnmpData() = default;
    QtSnmpData( const QtSnmpData& ) = default;
    QtSnmpData& operator=( const QtSnmpData& ) = default;
    QtSnmpData( const int type, const QByteArray data = {} );

    int type() const;
    QString typeDescription() const;

    QByteArray data() const;
    int intValue() const;
    unsigned int uintValue() const;
    qint64 longLongValue() const;
    QString textValue() const;

    QByteArray address() const;
    void setAddress( const QByteArray& );

    const std::vector< QtSnmpData >& children() const;
    void addChild( const QtSnmpData& );

    bool isValid() const;

    QByteArray makeSnmpChunk() const;

    QVariant toVariant() const;

    static QtSnmpData integer( const qint32 value );
    static QtSnmpData null();
    static QtSnmpData string( const QByteArray& value );
    static QtSnmpData sequence();
    static QtSnmpData oid( const QByteArray& );
    static void parseData( const QByteArray& data_for_parsing,
                           std::vector< QtSnmpData >*const parsed_data_list );

private:
    qint32 m_type = INVALID_TYPE;
    qint32 m_padding = 0;
    QByteArray m_data;
    std::vector< QtSnmpData > m_children;
    QByteArray m_address;
};

typedef std::vector< QtSnmpData > QtSnmpDataList;
typedef QHash< QByteArray, QtSnmpData > QtSnmpDataMap;

Q_DECLARE_METATYPE( QtSnmpData )
Q_DECLARE_METATYPE( QtSnmpDataList )
Q_DECLARE_METATYPE( QtSnmpDataMap )
