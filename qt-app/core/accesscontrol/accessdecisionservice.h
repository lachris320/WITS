#ifndef ACCESSCONTROL_ACCESSDECISIONSERVICE_H
#define ACCESSCONTROL_ACCESSDECISIONSERVICE_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include "accesscontrol/accesstypes.h"

class QNetworkAccessManager;

namespace AccessControl {

// Verifies a Credential against the backend and emits a decided()
// AccessDecision. Follows the repo idiom: an injected QNetworkAccessManager
// (not owned) so it is testable with CapturingNam, and it decodes the reply
// through a pure static so decode logic is unit-testable with no network.
// A timeout or transport failure yields Result::Error — NEVER a fabricated
// Denied (Denied is reserved for a well-formed policy rejection).
//
// Lifecycle: verify() is fire-and-forget. cancel() invalidates every verify still
// in flight (via a generation counter) so a capture adapter can call it from its
// own stop() and be sure no late decided() fires after shutdown. Reply cleanup is
// bound to the reply itself, so replies never leak against the injected NAM even
// if this service is destroyed mid-request.
class AccessDecisionService : public QObject
{
    Q_OBJECT
public:
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owner's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    explicit AccessDecisionService(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }

    void setTimeoutMs(int ms);
    void verify(const Credential &credential);
    void cancel();

    static AccessDecision decodeResponse(const QByteArray &raw,
                                         const QString &correlationId);

signals:
    void decided(const AccessControl::AccessDecision &decision);

private:
    QNetworkAccessManager *m_nam;   // injected, not owned
    int m_timeoutMs = 5000;
    quint64 m_generation = 0;       // bumped by cancel(); a verify from an older gen is dropped
};

} // namespace AccessControl

#endif // ACCESSCONTROL_ACCESSDECISIONSERVICE_H
