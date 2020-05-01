#include "Host.h"

namespace rebus {

Host::Host(const QString& name)
{
    this->m_name = name;
}

const QString& Host::name() const
{
    return this->m_name;
}

void Host::setName(const QString &name)
{
    if (this->m_name != name) {
        this->m_name = name;
    }
}

const QString& Host::uuid() const
{
    return this->m_uuid;
}

void Host::setUuid(const QString &uuid)
{
    if (this->m_uuid != uuid) {
        this->m_uuid = uuid;
    }
}

} // namespace rebus
