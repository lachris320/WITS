#include "accesscontrol/healthmonitor.h"

namespace AccessControl {

HealthMonitor::HealthMonitor(QObject *parent)
    : QObject(parent)
{}

HealthSnapshot &HealthMonitor::entry(const QString &providerId)
{
    auto it = m_byProvider.find(providerId);
    if (it == m_byProvider.end()) {
        HealthSnapshot fresh;
        fresh.providerId = providerId;
        it = m_byProvider.insert(providerId, fresh);
    }
    return it.value();
}

void HealthMonitor::recordState(const QString &providerId, ConnectionState state)
{
    HealthSnapshot &s = entry(providerId);
    s.state = state;
    emit healthChanged(s);
}

void HealthMonitor::recordComm(const QString &providerId, qint64 latencyMs,
                               const QDateTime &at)
{
    HealthSnapshot &s = entry(providerId);
    s.latencyMs = latencyMs;
    s.lastCommTime = at;
    emit healthChanged(s);
}

void HealthMonitor::recordCommTime(const QString &providerId, const QDateTime &at)
{
    // A comm happened but we have no latency measurement — record the time only,
    // leaving latencyMs at whatever it was (-1 == still unknown). Never fabricate.
    HealthSnapshot &s = entry(providerId);
    s.lastCommTime = at;
    emit healthChanged(s);
}

void HealthMonitor::recordRetry(const QString &providerId)
{
    HealthSnapshot &s = entry(providerId);
    ++s.retryCount;
    emit healthChanged(s);
}

HealthSnapshot HealthMonitor::snapshot(const QString &providerId) const
{
    const auto it = m_byProvider.constFind(providerId);
    if (it == m_byProvider.constEnd()) {
        HealthSnapshot fresh;
        fresh.providerId = providerId;
        return fresh;
    }
    return it.value();
}

} // namespace AccessControl
