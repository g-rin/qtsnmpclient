#pragma once

#include "QtSnmpData.h"
#include <QMetaType>
#include <QSharedPointer>
#include <QList>

namespace qtsnmpclient {

class Session;

class AbstractJob {
    Q_DISABLE_COPY( AbstractJob )
protected:
    explicit AbstractJob( Session*const,
                          const qint32 id );
public:
    virtual ~AbstractJob() = default;
    qint32 id() const;
    virtual void start() = 0;

    struct ErrorResponse {
        QString request;
        QString status;
        int index = 0;
    };

    virtual void processData( const QtSnmpDataList&,
                              const QList< ErrorResponse >& );
    virtual QString description() const = 0;

protected:
    Session*const m_session;
private:
    const qint32 m_id;
    const qint32 m_padding = 0;
};

typedef QSharedPointer< AbstractJob > JobPointer;
typedef QList< JobPointer > SnmpJobList;

} // namespace qtsnmpclient

Q_DECLARE_METATYPE( qtsnmpclient::JobPointer )
