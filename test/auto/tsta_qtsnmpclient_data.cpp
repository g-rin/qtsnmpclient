#include <QTest>
#include <QDebug>
#include <QtSnmpData.h>
#include <QUuid>
#include <chrono>

using namespace std::chrono;

namespace {
    QByteArray genOid () {
        QByteArray oid = ".1.3";
        for ( int i = 0; i < 10; ++i ) {
            oid += "." + QByteArray::number( qrand() & 0xff );
        }
        return oid;
    };

    void addChildren ( QtSnmpData& data, const int deep ) {
        const int tier_count = 2;
        for ( int i = 0; i < tier_count; ++i ) {
            data.addChild( QtSnmpData::oid( genOid() ) );
            auto child = QtSnmpData::string( QUuid::createUuid().toByteArray() );
            if ( deep > 0 ) {
                auto sequence = QtSnmpData::sequence();
                addChildren( sequence, deep - 1 );
            }
        }
    }

    void checkAddingChildren( QtSnmpData& data, const int deep ) {
        const int tier_count = 3;
        for ( int i = 0; i < tier_count; ++i ) {
            auto expected = data.children();
            auto child = QtSnmpData::null();
            if ( deep > 0 ) {
                checkAddingChildren( child, deep - 1 );
            }
            data.addChild( child );
            expected.push_back( child );
            QCOMPARE( data.children(), expected );
        }
    }

    void checkIntData( const qint32 initial_value,
                       const qint64 expected_int_value )
    {
        auto data = QtSnmpData::integer( initial_value );
        QCOMPARE( data.isValid(), true );
        QVERIFY( data.type() == QtSnmpData::INTEGER_TYPE );
        QCOMPARE( data.intValue(), static_cast< int >( expected_int_value ) );
        QCOMPARE( data.uintValue(), static_cast< unsigned int >( expected_int_value ) );
        QCOMPARE( data.longLongValue(), expected_int_value );
        QVERIFY( 0 == data.children().size() );
        checkAddingChildren( data, 3 );
    }

    void checkStringData( const QString& string ) {
        auto data = QtSnmpData::string( string.toLocal8Bit() );
        QCOMPARE( data.isValid(), true );
        QVERIFY( data.type() == QtSnmpData::STRING_TYPE );
        QVERIFY( 0 == data.children().size() );
        QCOMPARE( data.textValue(), string );
        QCOMPARE( data.toVariant(), QVariant::fromValue( string ) );
        checkAddingChildren( data, 3 );
    }

    void checkOidData( const QByteArray& oid ) {
        auto data = QtSnmpData::oid( oid );
        QCOMPARE( data.isValid(), true );
        QVERIFY( data.type() == QtSnmpData::OBJECT_TYPE );
        QVERIFY( 0 == data.children().size() );
        QCOMPARE( data.data(), oid );
        checkAddingChildren( data, 3 );
    }
}

class TestQtSnmpData : public QObject {
    Q_OBJECT
private slots:
    void init() {
        const auto _duration = system_clock::now() - system_clock::time_point();
        qsrand( static_cast< uint >( _duration.count()) );
    }

    void testEmptyData() {
        QtSnmpData data;
        QCOMPARE( data.isValid(), false );
        QVERIFY( data.type() == QtSnmpData::INVALID_TYPE );
        QCOMPARE( data.toVariant(), QVariant() );
        QVERIFY( 0 == data.children().size() );
        auto child = QtSnmpData::null();
    }

    void testIntegerData() {
        checkIntData( 0, 0 );

        checkIntData( 1, 1 );
        checkIntData( -1, -1 );

        checkIntData( 89478486, 89478486 );
        checkIntData( 178956971, 178956971 );

        checkIntData( -178956971, -178956971 );
        checkIntData( -89478486, -89478486 );

        checkIntData( 0x7FFFFFFF, 0x7FFFFFFF );
        checkIntData( static_cast< qint32 >( 0xFFFFFFFF ), -1 );
    }

    void testNullData() {
        auto data = QtSnmpData::null();
        QVERIFY( data.type() == QtSnmpData::NULL_DATA_TYPE );
        QVERIFY( data.isValid() );
        QCOMPARE( data.toVariant(), QVariant() );
        QVERIFY( 0 == data.children().size() );
        QVERIFY( data.data().isEmpty() );
    }

    void testStringData() {
        for ( int i = 0; i < 100; ++i ) {
            checkStringData( QUuid::createUuid().toString() );
        }
    }

    void testOidData() {
        for ( int i = 0; i < 100; ++i ) {
            checkOidData( genOid() );
        }
    }

    void testSequenceData() {
        auto data = QtSnmpData::sequence();
        QVERIFY( data.type() == QtSnmpData::SEQUENCE_TYPE );
        QVERIFY( data.data().isEmpty() );
        checkAddingChildren( data, 3 );
    }

    void testAddress() {
        for ( int i = 0; i < 100; ++i ) {
            auto data = QtSnmpData::null();
            const auto oid = genOid();
            data.setAddress( oid );
            QCOMPARE( data.data(), QByteArray() );
            QCOMPARE( data.address(), oid );
        }
    }

    void testParseData1() {
        auto original_item = QtSnmpData::sequence();
        original_item.addChild( QtSnmpData::oid( genOid() ) );
        original_item.addChild( QtSnmpData::string( QUuid::createUuid().toByteArray() ) );
        const auto data = original_item.makeSnmpChunk();
        std::vector< QtSnmpData > result_list;
        QtSnmpData::parseData( data, &result_list );
        QVERIFY( 1 == result_list.size() );
        const auto result_item = result_list.at( 0 );
        QCOMPARE( result_item, original_item );
    }

    void testParseData2() {
        QtSnmpDataList original_list;
        QByteArray data;
        for ( int i = 0; i < 2; ++i ) {
            auto root = QtSnmpData::sequence();
            addChildren( root, 2 );
            original_list.push_back( root );
            data.append( root.makeSnmpChunk() );
        }
        std::vector< QtSnmpData> result_list;
        QtSnmpData::parseData( data, &result_list );
        QCOMPARE( result_list, original_list );
    }

    void testSetRequestMessageSerialization() {
        auto checkSerialization = []( const QtSnmpData& value ){
            auto message = QtSnmpData::sequence();
            message.addChild( QtSnmpData::integer( 0 ) );
            message.addChild( QtSnmpData::string( QUuid::createUuid().toByteArray() ) );
            auto request = QtSnmpData( QtSnmpData::SET_REQUEST_TYPE );
            request.addChild( QtSnmpData::integer( qrand() ) );
            request.addChild( QtSnmpData::integer( 0 ) );
            request.addChild( QtSnmpData::integer( 0 ) );
            auto var_bind_list = QtSnmpData::sequence();
            auto var_bin = QtSnmpData::sequence();
            var_bin.addChild( QtSnmpData::oid( genOid() ) );
            var_bin.addChild( QtSnmpData( value.type(), value.data() ) );
            var_bind_list.addChild( var_bin );
            request.addChild( var_bind_list );
            message.addChild( request );

            const auto transfer_data = message.makeSnmpChunk();
            std::vector< QtSnmpData > restored_list;
            QtSnmpData::parseData( transfer_data, &restored_list );
            QVERIFY( 1 == restored_list.size() );
            QCOMPARE( restored_list.at( 0 ), message );

        };

        for ( int i = 0; i < 10; ++i ) {
            checkSerialization( QtSnmpData::string( QUuid::createUuid().toByteArray() ) );
            checkSerialization( QtSnmpData::integer( qrand() ) );
        }
    }
};

QTEST_MAIN( TestQtSnmpData )
#include "tsta_qtsnmpclient_data.moc"
