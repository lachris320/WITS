#ifndef ACCESSCONTROL_HEALTHMONITOR_H
#define ACCESSCONTROL_HEALTHMONITOR_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include "accesscontrol/accesstypes.h"

namespace AccessControl {

// Tracks per-provider connection health (state, last comm time, latency, retry
// count) and emits healthChanged() whenever a fact changes. Owned by
// AccessControlService; read by a future admin health panel (a later sub-plan).
class HealthMonitor : public QObject
{
    Q_OBJECT
public:
    explicit HealthMonitor(QObject *parent = nullptr);

    void recordState(const QString &providerId, ConnectionState state);
    void recordComm(const QString &providerId, qint64 latencyMs, const QDateTime &at);
    void recordCommTime(const QString &providerId, const QDateTime &at);
    void recordRetry(const QString &providerId);

    HealthSnapshot snapshot(const QString &providerId) const;

signals:
    void healthChanged(const AccessControl::HealthSnapshot &snapshot);

private:
    HealthSnapshot &entry(const QString &providerId);   // creates on first use

    QHash<QString, HealthSnapshot> m_byProvider;
};

} // namespace AccessControl

#endif // ACCESSCONTROL_HEALTHMONITOR_H
