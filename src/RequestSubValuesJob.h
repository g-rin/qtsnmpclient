#pragma once

#include "AbstractJob.h"

namespace qtsnmpclient {

class RequestSubValuesJob : public AbstractJob {
    Q_DISABLE_COPY( RequestSubValuesJob )
public:
    explicit RequestSubValuesJob( Session*const,
                                  const qint32 id,
                                  const QString& base_oid );
    virtual void start() override final;
    virtual void processData( const QtSnmpDataList&, const QList< ErrorResponse >& ) override final;
    virtual QString description() const override final;

private:
    const QString m_base_oid;
    QtSnmpDataList m_found;
};

} // namespace qtsnmpclient
