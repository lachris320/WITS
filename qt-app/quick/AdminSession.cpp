#include "AdminSession.h"

AdminSession &AdminSession::instance()
{
    static AdminSession s;   // Meyers singleton: constructed on first use.
    return s;
}

void AdminSession::setKey(const QString &key)
{
    if (m_key == key)
        return;                 // idempotent — no redundant changed()
    m_key = key;
    emit changed();
}

void AdminSession::clear()
{
    if (m_key.isEmpty())
        return;
    m_key.clear();
    emit changed();
}
