#include "Tester.h"
#include <QCoreApplication>
#include <QThread>
#include <cassert>

void makeTester( const QString& address ) {
    auto*const thread = new QThread;
    thread->setObjectName( "address" );
    auto*const tester = new Tester( QHostAddress( address ) );
    tester->moveToThread( thread );
    assert( qApp );
    QObject::connect( qApp, SIGNAL( destroyed() ),
                      tester, SLOT( deleteLater() ) );
    QObject::connect( tester, SIGNAL( destroyed() ),
                      thread, SLOT( quit() ) );
    QObject::connect( thread, SIGNAL( finished() ),
                      thread, SLOT( deleteLater() ) );
    QObject::connect( thread, SIGNAL( started() ),
                      tester, SLOT( start() ) );
    thread->start();
}

int main( int argc, char** argv ) {
    QCoreApplication app( argc, argv );

    // TODO: type network address of you SNMP agent (Network router as example)
//    makeTester( "192.168.0.113" );
//    makeTester( "192.168.0.201" );
//    makeTester( "192.168.0.222" );
    makeTester( "192.168.29.2" );
    return app.exec();
}
