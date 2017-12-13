#include "SetValueJob.h"
#include "Session.h"

namespace qtsnmpclient {

SetValueJob::SetValueJob( Session*const session,
                          const qint32 id,
                          const QByteArray& community,
                          const QString& oid,
                          const int type,
                          const QByteArray& value )
    : AbstractJob( session, id )
    , m_community( community )
    , m_oid( oid )
    , m_type( type )
    , m_value( value )
{
}

void SetValueJob::start() {
    m_session->sendRequestSetValue( m_community, m_oid, m_type, m_value );
}

QString SetValueJob::description() const {
    return "requestSetValue: " + m_oid;
}

} // namespace qtsnmpclient
