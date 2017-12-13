#include "RequestSubValuesJob.h"
#include "Session.h"

namespace qtsnmpclient {

RequestSubValuesJob::RequestSubValuesJob( Session*const session,
                                                  const qint32 id,
                                                  const QString& base_oid )
    : AbstractJob( session, id )
    , m_base_oid( base_oid )
{
}

void RequestSubValuesJob::start() {
    m_session->sendRequestGetNextValue( m_base_oid );
}

void RequestSubValuesJob::processData( const QtSnmpDataList& values ) {
    if( values.isEmpty() ) {
        m_session->completeWork( values );
    }

    const auto& value = values.at( 0 );
    const auto& oid = value.address();
    bool request_next_value = ( 1 == values.count() );
    request_next_value = request_next_value && ( 0 == oid.indexOf( m_base_oid + "." ) );
    if( request_next_value ) {
        m_found << value;
        m_session->sendRequestGetNextValue( oid );
    } else {
        m_session->completeWork( m_found );
    }
}

QString RequestSubValuesJob::description() const {
    return "requestSubValues: " + m_base_oid ;
}

} // namespace qtsnmpclient
