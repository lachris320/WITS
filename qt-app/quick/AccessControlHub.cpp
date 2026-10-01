#include "AccessControlHub.h"

#include <QNetworkAccessManager>

#include "apiconfig.h"
#include "appsettings.h"
#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/contactage.h"
#include "accesscontrol/eventbus.h"
#include "accesscontrol/healthmonitor.h"
#include "accesscontrol/turnstileprovider.h"

using namespace AccessControl;

namespace { AccessControlHub *g_instance = nullptr; }

AccessControlHub *AccessControlHub::instance() { return g_instance; }
void AccessControlHub::setInstance(AccessControlHub *hub) { g_instance = hub; }

AccessControlHub::AccessControlHub(QNetworkAccessManager *injectedNam, QObject *parent)
    : QObject(parent)
    , m_bus(std::make_unique<EventBus>())
    , m_service(std::make_unique<AccessControlService>(m_bus.get(), &m_factory))
{
    if (injectedNam) {
        m_nam = injectedNam;                 // externally owned; must outlive the hub
    } else {
        m_ownedNam = std::make_unique<QNetworkAccessManager>();
        m_nam = m_ownedNam.get();
    }
    connect(m_bus.get(), &EventBus::eventPublished, this, &AccessControlHub::onBusEvent);

    // Live connection surface (Sub-plan 4): relay the service's state and the
    // HealthMonitor's per-provider comm time to QML.
    connect(m_service.get(), &AccessControlService::connectionStateChanged, this,
            [this](AccessControl::ConnectionState) { emit connectionStateChanged(); });
    connect(m_service->healthMonitor(), &HealthMonitor::healthChanged, this,
            &AccessControlHub::onHealthChanged);
}

AccessControlHub::~AccessControlHub()
{
    // Sever the relays first: members declared after m_service (m_providerId,
    // m_lastContactSeen) are destroyed BEFORE it, so nothing the service emits
    // during its own teardown may reach this half-destroyed hub.
    disconnect(m_service->healthMonitor(), nullptr, this, nullptr);
    disconnect(m_service.get(), nullptr, this, nullptr);
    if (g_instance == this) g_instance = nullptr;
}

QVariantMap AccessControlHub::toAccessEntry(const AccessEvent &e)
{
    const bool hasStudent = !e.subject.isEmpty();
    return QVariantMap{
        {QStringLiteral("hasStudent"), hasStudent},
        {QStringLiteral("student"), hasStudent ? e.subject.toVariantMap() : QVariantMap{}},
        {QStringLiteral("eventId"), e.correlationId},
        {QStringLiteral("at"), e.at},
    };
}

void AccessControlHub::onBusEvent(const AccessEvent &e)
{
    if (e.type == AccessEvent::Type::EntryObserved)
        emit entryObserved(toAccessEntry(e));
}

void AccessControlHub::onHealthChanged(const HealthSnapshot &s)
{
    if (m_providerId.isEmpty() || s.providerId != m_providerId)
        return;
    if (s.lastCommTime == m_lastContactSeen)
        return;                              // state-only update: don't churn the view
    m_lastContactSeen = s.lastCommTime;
    emit lastContactChanged();
}

bool AccessControlHub::isAccessEnabled() const { return m_intentEnabled; }

int AccessControlHub::connectionState() const
{
    return static_cast<int>(m_service->connectionState());
}

QDateTime AccessControlHub::lastContactAt() const
{
    if (m_providerId.isEmpty())
        return {};
    return m_service->healthMonitor()->snapshot(m_providerId).lastCommTime;
}

void AccessControlHub::persistEnabled(bool on)
{
    AppSettings settings;   // through AppSettings so tests isolate
    settings.setValue(QStringLiteral("accessControl/enabled"), on);
    settings.sync();
}

void AccessControlHub::setAccessEnabled(bool on)
{
    if (on == m_intentEnabled)
        return;                              // idempotent: no churn / duplicate persist
    if (!on && m_enableLocked)
        return;                              // WITS_ACCESS_CONTROL forces on: refuse

    persistEnabled(on);                      // the user's request is persisted FIRST
    m_intentEnabled = on;
    emit accessEnabledChanged();

    // Outcome is independent of intent: a failed connect leaves intent true and
    // the service's own reconnect/backoff keeps retrying.
    if (on)
        m_service->enable(m_descriptor, m_config);
    else
        m_service->disable();
}

void AccessControlHub::initialize()
{
    AppSettings settings;   // through AppSettings so tests isolate

    // Register the provider (whether or not enabled) so a later runtime toggle
    // can enable without re-registering. Creator captures the hub's NAM+baseUrl.
    m_descriptor = TurnstileProvider::defaultDescriptor();
    m_providerId = m_descriptor.providerId;
    QNetworkAccessManager *nam = m_nam;
    const QUrl baseUrl(ApiConfig::baseUrl());
    m_factory.registerProvider(m_descriptor,
        [nam, baseUrl](const ProviderDescriptor &, const QVariantMap &cfg, QObject *parent)
            -> IAccessProvider * {
            return new TurnstileProvider(nam, baseUrl, cfg, parent);
        });

    // Config retained RAW on every path — the provider owns normalization.
    m_config = QVariantMap{
        {QStringLiteral("pollIntervalMs"),
         settings.value(QStringLiteral("accessControl/pollIntervalMs"), 1500)},
        {QStringLiteral("gateId"),
         settings.value(QStringLiteral("accessControl/gateId"), QStringLiteral("turnstile"))},
    };

    // Enablement precedence: env force-on > setting > false.
    const QString env = qEnvironmentVariable("WITS_ACCESS_CONTROL").trimmed().toLower();
    const bool forceOn = (env == QLatin1String("1") || env == QLatin1String("true"));
    m_enableLocked = forceOn;
    m_intentEnabled = forceOn
                      || settings.value(QStringLiteral("accessControl/enabled"), false).toBool();
    if (!m_intentEnabled)
        return;

    m_service->enable(m_descriptor, m_config);
}

QString AccessControlHub::contactAgeText(bool monitoringOn, const QVariant &lastContact,
                                         const QVariant &now) const
{
    return formatContactAge(monitoringOn, lastContact.toDateTime(), now.toDateTime());
}
