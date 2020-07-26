#include "RequestValuesJob.h"
#include "Session.h"

namespace qtsnmpclient {

RequestValuesJob::RequestValuesJob( Session*const session,
                                    const qint32 id,
                                    const QStringList& oid_list,
                                    const int limit )
    : AbstractJob( session, id )
    , m_description( "requestValues:" + oid_list.join( "; " ) )
    , m_requests( oid_list )
    , m_limit( limit )
{
    m_results.reserve( static_cast< size_t >( oid_list.size() ) );
}

void RequestValuesJob::start() {
    makeRequest();
}

QString RequestValuesJob::description() const {
    return m_description;
}

void RequestValuesJob::processData( const QtSnmpDataList& values,
                                    const QList< ErrorResponse >& error )
{
    if ( ! error.isEmpty() ) {
        m_session->failWork();
        return;
    }

    for ( const auto& item : values ) {
        m_results.push_back( item );
    }

    if ( m_requests.isEmpty() ) {
        m_session->completeWork( m_results );
        return;
    }

    makeRequest();
}

void RequestValuesJob::makeRequest() {
    auto size = m_requests.size();
    if ( m_limit > 0 ) {
        size = std::min( m_limit, size );
    }
    const auto list = m_requests.mid( 0, size );
    m_requests = m_requests.mid( size, m_requests.size() - size );
    m_session->sendRequestGetValues( list );
}

} // namespace qtsnmpclient
