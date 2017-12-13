#include <QtCore/QtGlobal>

#ifdef WIN_EXPORT
    #undef WIN_EXPORT
#endif

#ifdef BUILD_SNMPCLIENT_DLL
    #define WIN_EXPORT Q_DECL_EXPORT
#elif defined( IMPORT_SNMPCLIENT_DLL )
    #define WIN_EXPORT Q_DECL_IMPORT
#else
    #define WIN_EXPORT
#endif
