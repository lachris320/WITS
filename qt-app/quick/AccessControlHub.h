#ifndef ACCESSCONTROLHUB_H
#define ACCESSCONTROLHUB_H

#include <QObject>
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
class AccessControlHub : public QObject
{
    Q_OBJECT
public:
    explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr,
                              QObject *parent = nullptr);
    ~AccessControlHub() override;

    void initialize();
    bool isAccessEnabled() const;
    static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);

    static AccessControlHub *instance();
    static void setInstance(AccessControlHub *hub);

signals:
    void entryObserved(const QVariantMap &entry);

private:
    void onBusEvent(const AccessControl::AccessEvent &e);

    // Declaration order fixes teardown order: reverse destruction is
    // service -> factory -> owned NAM -> bus, so the service tears down the
    // provider (which aborts its reply) while the NAM is still alive.
    std::unique_ptr<AccessControl::EventBus> m_bus;
    std::unique_ptr<QNetworkAccessManager> m_ownedNam;   // only when self-created
    AccessControl::AccessProviderFactory m_factory;      // plain value member
    std::unique_ptr<AccessControl::AccessControlService> m_service;

    QNetworkAccessManager *m_nam = nullptr;   // owned-or-injected; non-owning ptr
};

#endif // ACCESSCONTROLHUB_H
