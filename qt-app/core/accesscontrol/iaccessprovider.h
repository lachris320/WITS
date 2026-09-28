#ifndef ACCESSCONTROL_IACCESSPROVIDER_H
#define ACCESSCONTROL_IACCESSPROVIDER_H

#include <QObject>
#include <QString>
#include "accesscontrol/accesstypes.h"

namespace AccessControl {

// QObject interface every access-control adapter implements. It is a QObject
// (not a pure C++ interface) because it must emit signals; concrete providers
// subclass it. A provider captures credentials internally and emits DECIDED
// AccessEvents — it never hands a raw Credential to its owner.
class IAccessProvider : public QObject
{
    Q_OBJECT
public:
    explicit IAccessProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~IAccessProvider() override = default;

    virtual ProviderDescriptor descriptor() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual ConnectionState state() const = 0;

signals:
    void accessEvent(const AccessControl::AccessEvent &event);
    void stateChanged(AccessControl::ConnectionState state);
    void hardwareError(const QString &message);
};

} // namespace AccessControl

#endif // ACCESSCONTROL_IACCESSPROVIDER_H
