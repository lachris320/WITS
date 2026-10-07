#ifndef ACCESSCONTROL_TURNSTILEPROVIDER_H
#define ACCESSCONTROL_TURNSTILEPROVIDER_H

#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include "accesscontrol/iaccessprovider.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace AccessControl {

// Server-observed provider: polls turnstile_display.php (oldest-next cursor)
// through an INJECTED, not-owned NAM and emits EntryObserved AccessEvents.
// Baselines the cursor to latest_id on the FIRST start only; reconnect starts
// preserve the cursor. Failures emit Degraded (the service owns reconnect).
class TurnstileProvider : public IAccessProvider
{
    Q_OBJECT
public:
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is AccessControlHub's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                      const QVariantMap &config, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }

    ~TurnstileProvider() override;

    static ProviderDescriptor defaultDescriptor();
    static int clampPollMs(int raw);   // <=0 -> 1500; positive -> max(raw, 250)

    ProviderDescriptor descriptor() const override;
    void start() override;
    void stop() override;
    ConnectionState state() const override { return m_state; }

    void setTimeoutMs(int ms) { m_timeoutMs = ms; }

private:
    void sendPoll();
    void onFinished(QNetworkReply *reply, quint64 gen);
    void armTimer();
    void setState(ConnectionState s);
    void fail();

    QNetworkAccessManager *m_nam;   // injected, not owned
    QUrl m_baseUrl;
    QString m_gateId;
    int m_pollIntervalMs = 1500;
    int m_timeoutMs = 5000;

    qint64 m_since = 0;
    bool m_baselined = false;
    ConnectionState m_state = ConnectionState::Disconnected;
    QTimer *m_pollTimer;            // single-shot, parented to this
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation = 0;
};

} // namespace AccessControl

#endif // ACCESSCONTROL_TURNSTILEPROVIDER_H
