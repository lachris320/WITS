#ifndef ACCESSCONTROL_ACCESSTYPES_H
#define ACCESSCONTROL_ACCESSTYPES_H

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

namespace AccessControl {

enum class CredentialKind { Rfid, Qr, Nfc, Fingerprint, Face, Mobile, Pin };

enum class ConnectionState { Disconnected, Connecting, Connected, Degraded, Error };

// Raw presented credential. Captured and used ONLY inside an adapter; never
// published on the EventBus (bus currency is decided events).
struct Credential {
    CredentialKind kind = CredentialKind::Rfid;
    QString raw;
    QString gateId;
    QDateTime presentedAt;
};

// Outcome of verifying a Credential. Error (infra/timeout/unreachable) is
// deliberately distinct from Denied (a real policy rejection).
struct AccessDecision {
    enum class Result { Granted, Denied, Error };
    Result result = Result::Error;   // safe default: no answer == Error, not allow
    QString subjectId;
    QString reason;
    QString correlationId;
};

// A decided, publishable event. EntryObserved (confirmed physical entry) is a
// separate type from AccessGranted (permission), on purpose.
struct AccessEvent {
    enum class Type {
        AccessGranted, AccessDenied, AccessError, EntryObserved,
        ControllerConnected, ControllerDisconnected, HardwareError
    };
    Type type = Type::AccessError;
    QJsonObject subject;   // resolved subject/student JSON (empty until a later sub-plan resolves it)
    QString gateId;        // physical gate id — set for gate-scoped events (e.g. EntryObserved)
    QString providerId;    // emitting provider/controller id — set for Controller*/HardwareError
    CredentialKind credentialKind = CredentialKind::Rfid;
    QString reason;
    QString correlationId;
    QDateTime at;
};

// One configurable field a provider needs; drives the future admin config form.
struct ConfigFieldDescriptor {
    QString key;
    QString displayName;
    QString type;          // "string" | "int" | "bool" | "secret"
    bool required = false;
};

struct GateDescriptor {
    QString gateId;
    QString displayName;
};

struct ProviderDescriptor {
    QString providerId;
    QString displayName;
    QList<ConfigFieldDescriptor> configSchema;
};

// Registers every value type above (and ConnectionState) as a Qt metatype so
// it survives a queued signal/slot hop and is capturable by QSignalSpy.
// Idempotent — safe to call more than once (main.cpp and each test init).
void registerMetaTypes();

} // namespace AccessControl

Q_DECLARE_METATYPE(AccessControl::Credential)
Q_DECLARE_METATYPE(AccessControl::AccessDecision)
Q_DECLARE_METATYPE(AccessControl::AccessEvent)
Q_DECLARE_METATYPE(AccessControl::GateDescriptor)
Q_DECLARE_METATYPE(AccessControl::ProviderDescriptor)
Q_DECLARE_METATYPE(AccessControl::ConnectionState)

#endif // ACCESSCONTROL_ACCESSTYPES_H
