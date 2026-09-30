#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/eventbus.h"
#include "accesscontrol/healthmonitor.h"
#include "accesscontrol/iaccessprovider.h"

#include <QDateTime>
#include <QTimer>

namespace AccessControl {

AccessControlService::AccessControlService(EventBus *bus,
                                           AccessProviderFactory *factory,
                                           QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_factory(factory)
    , m_health(new HealthMonitor(this))
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_enabled && m_provider) {
            m_health->recordRetry(m_provider->descriptor().providerId);
            m_provider->start();
        }
    });
}

bool AccessControlService::isEnabled() const { return m_enabled; }

ConnectionState AccessControlService::connectionState() const { return m_state; }

HealthMonitor *AccessControlService::healthMonitor() const { return m_health; }

bool AccessControlService::isReconnectPending() const
{
    return m_reconnectTimer->isActive();
}

void AccessControlService::setReconnectBaseMs(int ms)
{
    // Clamp to [1, kReconnectMaxMs]: a base of 0/negative would busy-loop, and a
    // base above the cap makes the doubling in scheduleReconnect pointless and
    // risks int overflow. Bounding the base keeps every backoff value <= the cap.
    m_reconnectBaseMs = qBound(1, ms, kReconnectMaxMs);
    m_reconnectNextMs = m_reconnectBaseMs;
}

void AccessControlService::enable(const ProviderDescriptor &descriptor,
                                  const QVariantMap &config)
{
    if (m_enabled)
        return;

    m_provider = m_factory->create(descriptor, config, this);   // parented to this
    if (!m_provider)
        return;   // unknown provider id — stay disabled

    // Capture the specific provider instance so a stale (e.g. queued) event from
    // a previous provider — after a disable()/enable() cycle — is dropped instead
    // of being republished or attributed to the new provider. Also stamp the
    // emitting provider's id onto every republished event so bus consumers always
    // know the provenance (adapters may leave AccessEvent::providerId empty).
    IAccessProvider *const p = m_provider;
    const QString providerId = m_provider->descriptor().providerId;
    connect(m_provider, &IAccessProvider::accessEvent, this,
            [this, p, providerId](AccessEvent e) {
                if (!m_enabled || p != m_provider)
                    return;
                e.providerId = providerId;   // stamp provenance on republish
                m_bus->publish(e);
            });
    connect(m_provider, &IAccessProvider::stateChanged, this,
            &AccessControlService::onProviderState);
    connect(m_provider, &IAccessProvider::hardwareError, this,
            &AccessControlService::onHardwareError);
    // Per-poll freshness (Sub-plan 4): the provider reports each validated
    // successful poll; the SERVICE owns recording it (the provider never touches
    // HealthMonitor). Pinned to this provider instance and dropped once
    // disabled, so a late callback from a torn-down provider records nothing.
    connect(m_provider, &IAccessProvider::polled, this,
            [this, p, providerId](const QDateTime &at) {
                if (!m_enabled || p != m_provider)
                    return;
                m_health->recordCommTime(providerId, at);
            });

    m_enabled = true;
    m_reconnectNextMs = m_reconnectBaseMs;
    emit enabledChanged(true);
    m_provider->start();
}

void AccessControlService::disable()
{
    if (!m_enabled)
        return;

    // Flip the flag and sever the provider's signals BEFORE stopping it, so the
    // provider's own Disconnected transition during teardown is NOT mistaken for
    // an unexpected drop (which would schedule a reconnect). Any transition already
    // queued from the provider is also ignored, because onProviderState and the
    // republish lambda both bail when !m_enabled.
    m_enabled = false;
    m_reconnectTimer->stop();
    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
        m_provider->stop();
        m_provider->deleteLater();   // parented to this, torn down cleanly
        m_provider = nullptr;
    }
    m_state = ConnectionState::Disconnected;
    emit enabledChanged(false);
    emit connectionStateChanged(m_state);
}

void AccessControlService::onProviderState(ConnectionState state)
{
    // Guard against a late/stale delivery: once disabled (provider torn down) a
    // still-queued transition must not run — it would deref a null provider or
    // emit after shutdown. sender() pins the event to the CURRENT provider, so a
    // transition from a previous provider (after a disable()/enable() cycle) is
    // dropped rather than applied to the new one. (v1 is single-threaded per the
    // affinity note; these checks also make a future cross-thread provider safe.)
    if (!m_enabled || !m_provider || sender() != m_provider)
        return;

    m_state = state;
    m_health->recordState(m_provider->descriptor().providerId, state);
    emit connectionStateChanged(state);

    switch (state) {
    case ConnectionState::Connected:
        m_reconnectNextMs = m_reconnectBaseMs;   // reset backoff on success
        m_reconnectTimer->stop();                // cancel any pending reconnect
        // A successful connect IS a real comm moment — record the time. This is
        // one of TWO service-owned freshness sources (the other is the per-poll
        // polled(at) connection in enable()); it is kept deliberately: it is the
        // only source for providers that never emit polled (MockProvider), and
        // for TurnstileProvider it coincides with the baseline's own polled
        // record (same instant, harmless overwrite). Latency stays -1 (unknown)
        // until the verify/decision path measures a real one; never fabricate.
        m_health->recordCommTime(m_provider->descriptor().providerId,
                                 QDateTime::currentDateTimeUtc());
        publishControllerEvent(AccessEvent::Type::ControllerConnected);
        break;
    case ConnectionState::Disconnected:
        // An unexpected drop while enabled (teardown severs signals first, so this
        // is never the intentional disable() path): report it and try to recover.
        publishControllerEvent(AccessEvent::Type::ControllerDisconnected);
        scheduleReconnect();
        break;
    case ConnectionState::Degraded:
    case ConnectionState::Error:
        scheduleReconnect();
        break;
    case ConnectionState::Connecting:
        break;
    }
}

void AccessControlService::onHardwareError(const QString &message)
{
    if (!m_enabled || !m_provider || sender() != m_provider)
        return;
    AccessEvent e;
    e.type = AccessEvent::Type::HardwareError;
    e.reason = message;
    e.at = QDateTime::currentDateTimeUtc();
    e.providerId = m_provider->descriptor().providerId;   // provider identity, not a gate
    m_bus->publish(e);
}

void AccessControlService::publishControllerEvent(AccessEvent::Type type)
{
    AccessEvent e;
    e.type = type;
    e.at = QDateTime::currentDateTimeUtc();
    if (m_provider)
        e.providerId = m_provider->descriptor().providerId;   // provider identity, not a gate
    m_bus->publish(e);
}

void AccessControlService::scheduleReconnect()
{
    if (!m_enabled)
        return;
    m_reconnectTimer->start(m_reconnectNextMs);
    // Exponential backoff, capped.
    m_reconnectNextMs = qMin(m_reconnectNextMs * 2, kReconnectMaxMs);
}

} // namespace AccessControl
