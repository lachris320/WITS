#ifndef ACCESSCONTROL_ACCESSCONTROLSERVICE_H
#define ACCESSCONTROL_ACCESSCONTROLSERVICE_H

#include <QObject>
#include <QVariantMap>
#include "accesscontrol/accesstypes.h"

class QTimer;

namespace AccessControl {

class EventBus;
class AccessProviderFactory;
class IAccessProvider;
class HealthMonitor;

// Lifecycle owner for the active access-control provider. Disabled by default
// (accessControl.enabled == false) so the seam is zero behaviour change until
// enable() is called. enable() builds the provider via the factory, republishes
// its accessEvents onto the EventBus, and drives a per-provider connection
// state machine (Disconnected -> Connecting -> Connected -> Degraded/Error)
// with exponential backoff reconnect. Owns its HealthMonitor.
class AccessControlService : public QObject
{
    Q_OBJECT
public:
    explicit AccessControlService(EventBus *bus, AccessProviderFactory *factory,
                                  QObject *parent = nullptr);

    bool isEnabled() const;
    void enable(const ProviderDescriptor &descriptor, const QVariantMap &config);
    void disable();

    ConnectionState connectionState() const;
    HealthMonitor *healthMonitor() const;
    bool isReconnectPending() const;
    void setReconnectBaseMs(int ms);

signals:
    void enabledChanged(bool enabled);
    void connectionStateChanged(AccessControl::ConnectionState state);

private:
    void onProviderState(ConnectionState state);
    void onHardwareError(const QString &message);
    void publishControllerEvent(AccessEvent::Type type);
    void scheduleReconnect();

    EventBus *m_bus;                    // injected, not owned
    AccessProviderFactory *m_factory;   // injected, not owned
    IAccessProvider *m_provider = nullptr;  // owned (parented to this)
    HealthMonitor *m_health;            // owned (parented to this)
    QTimer *m_reconnectTimer;           // owned (parented to this)

    bool m_enabled = false;             // flag OFF by default
    ConnectionState m_state = ConnectionState::Disconnected;
    int m_reconnectBaseMs = 1000;
    int m_reconnectNextMs = 1000;
    static constexpr int kReconnectMaxMs = 30000;
};

} // namespace AccessControl

#endif // ACCESSCONTROL_ACCESSCONTROLSERVICE_H
