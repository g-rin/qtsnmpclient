#include "RequestValuesJob.h"
#include "Session.h"

namespace qtsnmpclient {

RequestValuesJob::RequestValuesJob( Session*const session,
                                    const qint32 id,
                                    const QStringList& oid_list )
    : AbstractJob( session, id )
    , m_list( oid_list )
{
}

void RequestValuesJob::start() {
    m_session->sendRequestGetValues( m_list );
}

QString RequestValuesJob::description() const {
    return "requestValues:" + m_list.join( "; " );
}

} // namespace qtsnmpclient
