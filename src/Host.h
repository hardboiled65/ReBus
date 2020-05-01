#ifndef _REBUS_HOST_H
#define _REBUS_HOST_H

#include <QtCore>

namespace rebus {

class Host
{
public:
    Host(const QString& name);

    const QString& name() const;
    void setName(const QString& name);
    const QString& uuid() const;
    void setUuid(const QString& uuid);

private:
    QString m_name;
    QString m_uuid;
};

} // namespace rebus

#endif // _REBUS_HOST_H
