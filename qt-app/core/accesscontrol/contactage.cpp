#include "accesscontrol/contactage.h"

#include <QtGlobal>

namespace AccessControl {

QString formatContactAge(bool monitoringOn, const QDateTime &lastContact, const QDateTime &now)
{
    if (!monitoringOn)
        return QStringLiteral("Monitoring off");
    if (!lastContact.isValid() || !now.isValid())
        return QStringLiteral("No contact yet");
    const qint64 secs = qMax<qint64>(0, lastContact.secsTo(now));   // skew clamps to 0
    if (secs < 60)
        return QStringLiteral("Last contact %1 s ago").arg(secs);
    return QStringLiteral("Last contact %1 min ago").arg(secs / 60);
}

} // namespace AccessControl
