#ifndef ACCESSCONTROLHUB_H
#define ACCESSCONTROLHUB_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <memory>

#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/accesstypes.h"

class QNetworkAccessManager;
namespace AccessControl { class EventBus; class AccessControlService; }

// Application-owned composition root for Access Control. Owns EventBus +
// AccessProviderFactory + AccessControlService (the service owns HealthMonitor
// and the provider). Maps EntryObserved bus events to entryObserved(QVariantMap).
// Exposed to QML as "AccessControl" via AccessControlSingleton (QML_FOREIGN).
//
// Sub-plan 4: also the home of the app-global LIVE state the admin page binds
// to — the requested monitoring intent (accessEnabled, persisted, separate from
// the connection outcome), the env lock (enableLocked), the service's
// connection state, and the HealthMonitor's last comm time for the provider.
class AccessControlHub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accessEnabled READ isAccessEnabled NOTIFY accessEnabledChanged)
    Q_PROPERTY(bool enableLocked READ isEnableLocked CONSTANT)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QDateTime lastContactAt READ lastContactAt NOTIFY lastContactChanged)
public:
    explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr,
                              QObject *parent = nullptr);
    ~AccessControlHub() override;

    // Must run before the QML engine loads (enableLocked is CONSTANT).
    void initialize();

    bool isAccessEnabled() const;          // requested/persisted INTENT
    bool isEnableLocked() const { return m_enableLocked; }
    int connectionState() const;           // AccessControl::ConnectionState as int
    QDateTime lastContactAt() const;       // invalid until a first comm

    // Idempotent; persist-first; refuses disable when enableLocked.
    Q_INVOKABLE void setAccessEnabled(bool on);

    static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);

    static AccessControlHub *instance();
    static void setInstance(AccessControlHub *hub);

signals:
    void entryObserved(const QVariantMap &entry);
    void accessEnabledChanged();
    void connectionStateChanged();
    void lastContactChanged();

private:
    void onBusEvent(const AccessControl::AccessEvent &e);
    void onHealthChanged(const AccessControl::HealthSnapshot &s);
    static void persistEnabled(bool on);

    // Declaration order fixes teardown order: reverse destruction is
    // service -> factory -> owned NAM -> bus, so the service tears down the
    // provider (which aborts its reply) while the NAM is still alive.
    std::unique_ptr<AccessControl::EventBus> m_bus;
    std::unique_ptr<QNetworkAccessManager> m_ownedNam;   // only when self-created
    AccessControl::AccessProviderFactory m_factory;      // plain value member
    std::unique_ptr<AccessControl::AccessControlService> m_service;

    QNetworkAccessManager *m_nam = nullptr;   // owned-or-injected; non-owning ptr

    // Retained by initialize() on EVERY path (incl. flag-off) so a later
    // runtime enable has a descriptor + config to pass.
    AccessControl::ProviderDescriptor m_descriptor;
    QVariantMap m_config;                     // raw pollIntervalMs / gateId
    QString m_providerId;
    bool m_intentEnabled = false;
    bool m_enableLocked = false;
    QDateTime m_lastContactSeen;              // de-dupes lastContactChanged
};

#endif // ACCESSCONTROLHUB_H
