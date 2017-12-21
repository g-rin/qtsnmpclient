#include "AbstractJob.h"
#include "Session.h"

namespace qtsnmpclient {

AbstractJob::AbstractJob( Session*const session,
                          const qint32 id )
    : m_session( session )
    , m_id( id )
{
    Q_ASSERT( session );
    Q_ASSERT( m_id > 0 );
}

AbstractJob::~AbstractJob() {
    // do nothing. just for virtualization
}

qint32 AbstractJob::id() const {
    return m_id;
}

void AbstractJob::processData( const QtSnmpDataList& values ) {
    m_session->completeWork( values );
}

} // namespace qtsnmpclient
