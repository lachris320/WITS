#include "accesscontrol/mockprovider.h"

#include <QDateTime>
#include <QJsonObject>

namespace AccessControl {

MockProvider::MockProvider(ProviderDescriptor descriptor, QObject *parent)
    : IAccessProvider(parent)
    , m_descriptor(std::move(descriptor))
{}

ProviderDescriptor MockProvider::descriptor() const { return m_descriptor; }

ConnectionState MockProvider::state() const { return m_state; }

void MockProvider::setState(ConnectionState next)
{
    if (m_state == next)
        return;
    m_state = next;
    emit stateChanged(m_state);
}

void MockProvider::start()
{
    if (m_state == ConnectionState::Connected)
        return;   // idempotent: starting an already-connected provider is a no-op
    setState(ConnectionState::Connecting);
    setState(ConnectionState::Connected);
}

void MockProvider::stop()
{
    setState(ConnectionState::Disconnected);
}

void MockProvider::simulateGranted(const QString &subjectId, const QString &gateId)
{
    AccessEvent e;
    e.type = AccessEvent::Type::AccessGranted;
    e.subject = QJsonObject{ { QStringLiteral("subjectId"), subjectId } };
    e.gateId = gateId;
    e.credentialKind = CredentialKind::Rfid;
    e.at = QDateTime::currentDateTimeUtc();
    emit accessEvent(e);
}

void MockProvider::simulateDenied(const QString &subjectId, const QString &reason)
{
    AccessEvent e;
    e.type = AccessEvent::Type::AccessDenied;
    e.subject = QJsonObject{ { QStringLiteral("subjectId"), subjectId } };
    e.reason = reason;
    e.at = QDateTime::currentDateTimeUtc();
    emit accessEvent(e);
}

void MockProvider::simulateError(const QString &reason)
{
    AccessEvent e;
    e.type = AccessEvent::Type::AccessError;
    e.reason = reason;
    e.at = QDateTime::currentDateTimeUtc();
    emit accessEvent(e);
}

void MockProvider::simulateEntryObserved(const QString &gateId)
{
    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;   // distinct from AccessGranted
    e.gateId = gateId;
    e.at = QDateTime::currentDateTimeUtc();
    emit accessEvent(e);
}

void MockProvider::simulateHardwareError(const QString &message)
{
    setState(ConnectionState::Error);
    emit hardwareError(message);
}

void MockProvider::simulateDisconnect()
{
    setState(ConnectionState::Degraded);
}

void MockProvider::simulatePolled(const QDateTime &at)
{
    emit polled(at);   // synthetic per-poll freshness fact (drives service tests)
}

ProviderDescriptor MockProvider::defaultDescriptor()
{
    ProviderDescriptor d;
    d.providerId = QStringLiteral("mock");
    d.displayName = QStringLiteral("Mock Provider");
    // No config fields — the mock needs no external configuration.
    return d;
}

} // namespace AccessControl
