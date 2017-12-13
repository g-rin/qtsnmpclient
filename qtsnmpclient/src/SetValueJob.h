#pragma once

#include "AbstractJob.h"

namespace qtsnmpclient {

class SetValueJob : public AbstractJob {
    Q_DISABLE_COPY( SetValueJob )
public:
    explicit SetValueJob( Session*const,
                          const qint32 id,
                          const QByteArray& community,
                          const QString& oid,
                          const int type,
                          const QByteArray& value );
    virtual void start() override final;
    virtual QString description() const override final;

private:
    const QByteArray m_community;
    const QString m_oid;
    const int m_type;
    const QByteArray m_value;
};

} // namespace qtsnmpclient
