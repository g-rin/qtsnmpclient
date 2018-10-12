#include "Tester.h"
#include <QCoreApplication>
#include <QThread>
#include <QDebug>
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
    if( 2 != argc ) {
        qDebug() << "Incorrect input arguments. The agent address isn't specified.";
        qDebug() << "Usage: " << argv[0] << " <agent IP address>";
        exit( 1 );
    }
    makeTester( argv[1] );
    return app.exec();
}
