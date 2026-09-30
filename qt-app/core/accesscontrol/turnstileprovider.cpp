#include "accesscontrol/turnstileprovider.h"
#include "accesscontrol/replylifecycle.h"
#include "loginparser.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace AccessControl {

int TurnstileProvider::clampPollMs(int raw)
{
    if (raw <= 0) return 1500;              // absent/invalid -> default
    return raw < 250 ? 250 : raw;          // clamp valid values to the floor
}

TurnstileProvider::TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                                     const QVariantMap &config, QObject *parent)
    : IAccessProvider(parent)
    , m_nam(nam)
    , m_baseUrl(std::move(baseUrl))
    , m_pollIntervalMs(clampPollMs(config.value(QStringLiteral("pollIntervalMs"), 1500).toInt()))
    , m_pollTimer(new QTimer(this))
{
    QString gate = config.value(QStringLiteral("gateId")).toString().trimmed();
    m_gateId = gate.isEmpty() ? QStringLiteral("turnstile") : gate;
    m_pollTimer->setSingleShot(true);
    connect(m_pollTimer, &QTimer::timeout, this, &TurnstileProvider::sendPoll);
}

TurnstileProvider::~TurnstileProvider() { stop(); }

ProviderDescriptor TurnstileProvider::defaultDescriptor()
{
    ProviderDescriptor d;
    d.providerId = QStringLiteral("turnstile");
    d.displayName = QStringLiteral("Turnstile (server-observed)");
    d.configSchema = {
        {QStringLiteral("pollIntervalMs"), QStringLiteral("Poll interval (ms)"),
         QStringLiteral("int"), false},
        {QStringLiteral("gateId"), QStringLiteral("Gate ID"),
         QStringLiteral("string"), false},
    };
    return d;
}

ProviderDescriptor TurnstileProvider::descriptor() const { return defaultDescriptor(); }

void TurnstileProvider::setState(ConnectionState s)
{
    if (s == m_state) return;
    m_state = s;
    emit stateChanged(s);
}

void TurnstileProvider::armTimer() { m_pollTimer->start(m_pollIntervalMs); }

void TurnstileProvider::start()
{
    ++m_generation;                 // invalidate any in-flight reply from a prior run
    m_pollTimer->stop();
    setState(ConnectionState::Connecting);
    sendPoll();
}

void TurnstileProvider::stop()
{
    ++m_generation;
    m_pollTimer->stop();
    if (m_reply) { m_reply->abort(); m_reply.clear(); }
}

void TurnstileProvider::sendPoll()
{
    if (m_reply) { m_reply->abort(); m_reply.clear(); }   // enforce one-in-flight

    QUrl url = m_baseUrl.resolved(QUrl(QStringLiteral("turnstile_display.php")));
    if (m_baselined) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("since"), QString::number(m_since));
        url.setQuery(q);
    }
    const quint64 gen = m_generation;
    QNetworkReply *reply = m_nam->get(QNetworkRequest(url));
    reply->setParent(this);           // provider owns its reply lifecycle
    m_reply = reply;

    armReplyLifecycle(reply, m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, gen]() { onFinished(reply, gen); });
}

void TurnstileProvider::fail()
{
    m_pollTimer->stop();
    m_reply.clear();
    setState(ConnectionState::Degraded);   // service owns reconnect/backoff
}

void TurnstileProvider::onFinished(QNetworkReply *reply, quint64 gen)
{
    if (gen != m_generation) return;   // stale (stopped/restarted since) — drop
    m_reply.clear();

    if (reply->error() != QNetworkReply::NoError) { fail(); return; }

    const LoginParser::EntryEventResult r =
        LoginParser::parseEntryEvent(reply->readAll(), m_baseUrl);
    if (!r.valid) { fail(); return; }

    if (!m_baselined) {                    // first start: baseline, skip history
        m_since = r.latestId;
        m_baselined = true;
        setState(ConnectionState::Connected);
        armTimer();                        // transition INTO steady polling
        return;
    }

    if (r.hasEntry) {
        if (r.eventId <= m_since) { fail(); return; }   // non-advancing: protocol anomaly
        // Confirm Connected only AFTER the anomaly guard so a bad reconnect
        // response goes Connecting -> Degraded with no transient Connected.
        setState(ConnectionState::Connected);
        if (gen != m_generation) return;   // a state subscriber stopped/restarted us
        AccessEvent e;
        e.type = AccessEvent::Type::EntryObserved;
        e.subject = r.student;             // empty => unresolved (see convention)
        e.gateId = m_gateId;
        e.credentialKind = CredentialKind::Rfid;
        e.correlationId = QString::number(r.eventId);
        e.at = r.at;
        m_since = r.eventId;               // advance cursor BEFORE the synchronous emit
        emit accessEvent(e);
        if (gen != m_generation) return;   // subscriber called stop()/start(): halt the drain
        sendPoll();                        // drain: immediately request the next
    } else {
        setState(ConnectionState::Connected);   // (re)confirm after a reconnect start
        if (gen != m_generation) return;
        armTimer();                        // empty poll: wait one interval
    }
}

} // namespace AccessControl
