#pragma once

#include <QByteArray>
#include <QVariant>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QDataStream>
#include <QDebug>
#include "win_export.h"

class QtSnmpData;

QDataStream& operator<<( QDataStream&, const QtSnmpData& );
QDataStream& operator>>( QDataStream&, QtSnmpData& );
QDebug operator<<( QDebug, const QtSnmpData& );
bool operator==( const QtSnmpData&, const QtSnmpData& );
bool operator!=( const QtSnmpData&, const QtSnmpData& );

class WIN_EXPORT QtSnmpData {
    friend QDataStream& ::operator<<( QDataStream&, const QtSnmpData& );
    friend QDataStream& ::operator>>( QDataStream&, QtSnmpData& );
    friend bool ::operator==( const QtSnmpData&, const QtSnmpData& );
    friend bool ::operator!=( const QtSnmpData&, const QtSnmpData& );
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
    QtSnmpData( const int type, const QByteArray& data = QByteArray() );

    int type() const;
    QString typeDescription() const;

    QByteArray data() const;
    int intValue() const;
    unsigned int uintValue() const;
    qint64 longLongValue() const;
    QString textValue() const;

    QByteArray address() const;
    void setAddress( const QByteArray& );

    QList< QtSnmpData > children() const;
    void addChild( const QtSnmpData& );

    bool isValid() const;

    QByteArray makeSnmpChunk() const;

    QVariant toVariant() const;

    static QtSnmpData integer( const int value );
    static QtSnmpData null();
    static QtSnmpData string( const QByteArray& value );
    static QtSnmpData sequence();
    static QtSnmpData oid( const QByteArray& );
    static QList< QtSnmpData > parseData( const QByteArray& );

private:
    qint32 m_type = INVALID_TYPE;
    qint32 m_padding = 0;
    QByteArray m_data;
    QList< QtSnmpData > m_childs;
    QByteArray m_address;
};

typedef QList< QtSnmpData > QtSnmpDataList;
typedef QHash< QByteArray, QtSnmpData > QtSnmpDataMap;

Q_DECLARE_METATYPE( QtSnmpData )
Q_DECLARE_METATYPE( QtSnmpDataList )
Q_DECLARE_METATYPE( QtSnmpDataMap )
