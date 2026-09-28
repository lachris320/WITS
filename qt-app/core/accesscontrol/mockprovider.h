#ifndef ACCESSCONTROL_MOCKPROVIDER_H
#define ACCESSCONTROL_MOCKPROVIDER_H

#include "accesscontrol/iaccessprovider.h"

namespace AccessControl {

// Descriptor-driven simulator implementing IAccessProvider with NO hardware.
// Each simulate*() fires one synthetic event so every downstream path is
// testable hardware-free. This is the engine behind all seam tests and the
// future "demo mode".
class MockProvider : public IAccessProvider
{
    Q_OBJECT
public:
    explicit MockProvider(ProviderDescriptor descriptor, QObject *parent = nullptr);

    ProviderDescriptor descriptor() const override;
    void start() override;
    void stop() override;
    ConnectionState state() const override;

    void simulateGranted(const QString &subjectId, const QString &gateId);
    void simulateDenied(const QString &subjectId, const QString &reason);
    void simulateError(const QString &reason);
    void simulateEntryObserved(const QString &gateId);
    void simulateHardwareError(const QString &message);
    void simulateDisconnect();

    static ProviderDescriptor defaultDescriptor();

private:
    void setState(ConnectionState next);

    ProviderDescriptor m_descriptor;
    ConnectionState m_state = ConnectionState::Disconnected;
};

} // namespace AccessControl

#endif // ACCESSCONTROL_MOCKPROVIDER_H
