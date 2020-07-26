#pragma once

#include "AbstractJob.h"
#include <QStringList>

namespace qtsnmpclient {

class RequestValuesJob : public AbstractJob {
    Q_DISABLE_COPY( RequestValuesJob )
public:
    explicit RequestValuesJob( Session*const,
                               const qint32 id,
                               const QStringList& oid_list,
                               const int limit );
    virtual void start() override final;
    virtual QString description() const override final;
    virtual void processData( const QtSnmpDataList&,
                              const QList< ErrorResponse >& ) override final;
private:
    void makeRequest();

private:
    const QString m_description;
    QStringList m_requests;
    QtSnmpDataList m_results;
    const int m_limit = 0;
};

} // namespace qtsnmpclient
