#pragma once

#include <QtCore/QtGlobal>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QMetaMethod>
#include <QtCore/QStringList>

#define _pushZero(...) e,e,e,e,e,e,e,e,e,e,e,e,e,e,e,0
#define _impl_VA_NUM_ARGS_(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_N,...) _N
#define _impl_VA_NUM_ARGS(...) _impl_VA_NUM_ARGS_(__VA_ARGS__)
#define VA_NUM_ARGS(...) _impl_VA_NUM_ARGS(_pushZero __VA_ARGS__ (),f,e,d,c,b,a,9,8,7,6,5,4,3,2,1)

#define _impl_macroARGS_(m,n) m ## n
#define _impl_macroARGS(m,n) _impl_macroARGS_(m,n)
#define macroARGS(...) _impl_macroARGS(_impl_macroARGS,VA_NUM_ARGS(__VA_ARGS__))

#define _impl_macroARGS0(m,_1) _impl_ ## m ## _0(_1)
#define _impl_macroARGS1(m,_1) _impl_ ## m(_1)
#define _impl_macroARGS2(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS1(m,__VA_ARGS__)
#define _impl_macroARGS3(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS2(m,__VA_ARGS__)
#define _impl_macroARGS4(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS3(m,__VA_ARGS__)
#define _impl_macroARGS5(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS4(m,__VA_ARGS__)
#define _impl_macroARGS6(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS5(m,__VA_ARGS__)
#define _impl_macroARGS7(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS6(m,__VA_ARGS__)
#define _impl_macroARGS8(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS7(m,__VA_ARGS__)
#define _impl_macroARGS9(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS8(m,__VA_ARGS__)
#define _impl_macroARGSa(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGS9(m,__VA_ARGS__)
#define _impl_macroARGSb(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGSa(m,__VA_ARGS__)
#define _impl_macroARGSc(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGSb(m,__VA_ARGS__)
#define _impl_macroARGSd(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGSc(m,__VA_ARGS__)
#define _impl_macroARGSe(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGSd(m,__VA_ARGS__)
#define _impl_macroARGSf(m,_1,...) _impl_macroARGS1(m,_1) _impl_ ## m ## _separator _impl_macroARGSe(m,__VA_ARGS__)

#define _impl_UNUSED_0(x)
#define _impl_UNUSED_separator ,
#define _impl_UNUSED(x) (void)(x)
#define UNUSED(...) (macroARGS(__VA_ARGS__)(UNUSED,__VA_ARGS__))

static inline QMetaMethod findMethod( const QMetaObject*const metaObject,
                                      const char*const method )
{
    auto normalized = QMetaObject::normalizedSignature( method );
    int index=metaObject->indexOfMethod( normalized );
    if( index >= 0 ) {
        return metaObject->method( index );
    }

    auto signature  = QString::fromUtf8( method );
    // Remove possible class name
    signature.remove( QString::fromUtf8( metaObject->className() )  + "::" );
    normalized = QMetaObject::normalizedSignature( signature.toUtf8() );
    index = metaObject->indexOfMethod( normalized );
    if( index >= 0) {
        return metaObject->method( index );
    }

    // Remove possible return type
    auto signatureSplit=signature.split( ' ' );
    while( signatureSplit.count() > 1 ) {
        signatureSplit.removeFirst();
        signature = signatureSplit.join( " " );
        normalized = QMetaObject::normalizedSignature( signature.toUtf8() );
        index = metaObject->indexOfMethod( normalized );
        if( index >= 0 ) {
            return metaObject->method( index );
        }
    }

    const auto left = QString( "%1(" ).arg( method );
    for(int i = 0; i < metaObject->methodCount(); ++i ) {
        const auto& m = metaObject->method( i );
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
        const QString right = QString::fromUtf8( m.signature() ).left( left.length() );
#else
        const QString right = QString::fromUtf8( m.methodSignature() ).left( left.length() );
#endif
        if( 0 == left.compare( right ) ) {
            return m;
        }
    }

    for( int i = 0; i < metaObject->methodCount(); ++i ) {
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
        qWarning() << metaObject->method(i).signature();
#else
        qWarning() << metaObject->method(i).methodSignature();
#endif
    }
    qWarning() << "no match for: " << method;

    Q_ASSERT(false);
    return QMetaMethod();
}

#define ads_qtMetaMethod() findMethod( &staticMetaObject, __FUNCTION__ )

template < typename T > inline QArgument< T > ads_qarg( const T& t ) {
    return QArgument< T >( QMetaType::typeName( qMetaTypeId< T >() ), t );
}

template < typename T > inline QReturnArgument< T > ads_qrarg( T& t) {
    return QReturnArgument< T >( QMetaType::typeName( qMetaTypeId< T >() ), t );
}

template < typename T > inline QGenericReturnArgument ads_qrarg( const T& ) {
    return QGenericReturnArgument();
}

#define _impl_QARG_EXPAND_0(x) QGenericArgument()
#define _impl_QARG_EXPAND_separator ,
#define _impl_QARG_EXPAND(x) ads_qarg(x)
#define QARG_EXPAND(...) macroARGS(__VA_ARGS__)(QARG_EXPAND,__VA_ARGS__)

#define _impl__int_QRARG_EXPAND_0(x) QGenericReturnArgument(),
#define _impl__int_QRARG_EXPAND_separator
#define _impl__int_QRARG_EXPAND(x) ads_qrarg(l_returnValue),
#define _int_QRARG_EXPAND(...) macroARGS(__VA_ARGS__)(_int_QRARG_EXPAND,__VA_ARGS__)

#define _impl_IN_QOBJECT_THREAD(preInvokeStatement,returnStatement,connection,...)\
    do {\
        if( QThread::currentThread() != thread() ) {\
            static const QMetaMethod metaMethod( ads_qtMetaMethod() );\
            preInvokeStatement;\
            const bool invokeResult = metaMethod.invoke(\
                                        this, \
                                        connection, \
                                        _int_QRARG_EXPAND(  preInvokeStatement) QARG_EXPAND(__VA_ARGS__));\
            UNUSED( invokeResult );\
            Q_ASSERT( invokeResult );\
            returnStatement; \
        }\
    } while(false)

#define IN_QOBJECT_THREAD_FULL(preInvokeStatement,returnStatement,connection,...) _impl_IN_QOBJECT_THREAD(preInvokeStatement,returnStatement,connection,__VA_ARGS__)
#define IN_QOBJECT_THREAD_BL(type,...) IN_QOBJECT_THREAD_FULL(type l_returnValue,return l_returnValue,Qt::BlockingQueuedConnection,__VA_ARGS__)
#define IN_QOBJECT_THREAD_BL_void(...) IN_QOBJECT_THREAD_FULL(,return,Qt::BlockingQueuedConnection,__VA_ARGS__)
#define IN_QOBJECT_THREAD_RS(returnStatement,...) IN_QOBJECT_THREAD_FULL(,returnStatement,Qt::QueuedConnection,__VA_ARGS__)
#define IN_QOBJECT_THREAD_ASRV(returnValue,...) IN_QOBJECT_THREAD_RS(return returnValue,__VA_ARGS__)
#define IN_QOBJECT_THREAD(...) IN_QOBJECT_THREAD_ASRV(,__VA_ARGS__)
