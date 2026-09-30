#include "AccessControlHub.h"

#include <QNetworkAccessManager>

#include "apiconfig.h"
#include "appsettings.h"
#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/eventbus.h"
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
}

AccessControlHub::~AccessControlHub() { if (g_instance == this) g_instance = nullptr; }

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

bool AccessControlHub::isAccessEnabled() const { return m_service->isEnabled(); }

void AccessControlHub::initialize()
{
    AppSettings settings;   // through AppSettings so tests isolate

    // Register the provider (whether or not enabled) so a later runtime toggle
    // can enable without re-registering. Creator captures the hub's NAM+baseUrl.
    const ProviderDescriptor descriptor = TurnstileProvider::defaultDescriptor();
    QNetworkAccessManager *nam = m_nam;
    const QUrl baseUrl(ApiConfig::baseUrl());
    m_factory.registerProvider(descriptor,
        [nam, baseUrl](const ProviderDescriptor &, const QVariantMap &cfg, QObject *parent)
            -> IAccessProvider * {
            return new TurnstileProvider(nam, baseUrl, cfg, parent);
        });

    // Enablement precedence: env force-on > setting > false.
    const QString env = qEnvironmentVariable("WITS_ACCESS_CONTROL").trimmed().toLower();
    const bool forceOn = (env == QLatin1String("1") || env == QLatin1String("true"));
    const bool enabled = forceOn
                         || settings.value(QStringLiteral("accessControl/enabled"), false).toBool();
    if (!enabled) return;

    // Pass config RAW — the provider owns pollIntervalMs/gateId normalization.
    m_service->enable(descriptor, QVariantMap{
        {QStringLiteral("pollIntervalMs"),
         settings.value(QStringLiteral("accessControl/pollIntervalMs"), 1500)},
        {QStringLiteral("gateId"),
         settings.value(QStringLiteral("accessControl/gateId"), QStringLiteral("turnstile"))},
    });
}
