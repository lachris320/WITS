#include "accesscontrol/accessdecisionservice.h"
#include "accesscontrol/replylifecycle.h"
#include "apiconfig.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace AccessControl {

AccessDecisionService::AccessDecisionService(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

void AccessDecisionService::setTimeoutMs(int ms) { m_timeoutMs = ms; }

void AccessDecisionService::cancel()
{
    // Bump the generation: any verify still in flight captured an older generation,
    // so its finished handler will drop the result instead of emitting decided().
    ++m_generation;
}

AccessDecision AccessDecisionService::decodeResponse(const QByteArray &raw,
                                                     const QString &correlationId)
{
    AccessDecision d;
    d.correlationId = correlationId;

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        d.result = AccessDecision::Result::Error;   // malformed body == infra trouble
        d.reason = QStringLiteral("Invalid server response");
        return d;
    }

    const QJsonObject obj = doc.object();
    const QString decision = obj.value(QStringLiteral("decision")).toString();
    if (decision == QLatin1String("granted")) {
        const QJsonValue subj = obj.value(QStringLiteral("subject_id"));
        if (!subj.isString() || subj.toString().isEmpty()) {
            // A grant with no identifiable subject is a broken contract, not a
            // real allow. Fail safe to Error so a malformed 200 cannot authorize.
            d.result = AccessDecision::Result::Error;
            d.reason = QStringLiteral("Granted response missing subject_id");
            return d;
        }
        d.result = AccessDecision::Result::Granted;
        d.subjectId = subj.toString();
    } else if (decision == QLatin1String("denied")) {
        d.result = AccessDecision::Result::Denied;   // real policy rejection
        d.subjectId = obj.value(QStringLiteral("subject_id")).toString();
        d.reason = obj.value(QStringLiteral("message")).toString();
    } else {
        d.result = AccessDecision::Result::Error;     // unknown/absent decision
        d.reason = QStringLiteral("Unrecognized decision");
    }
    return d;
}

void AccessDecisionService::verify(const Credential &credential)
{
    const QString correlationId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    const quint64 gen = m_generation;   // snapshot; cancel() bumps this to invalidate

    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("access_verify.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("raw"), credential.raw);
    form.addQueryItem(QStringLiteral("gate_id"), credential.gateId);
    form.addQueryItem(QStringLiteral("kind"),
                      QString::number(static_cast<int>(credential.kind)));

    QNetworkReply *reply =
        m_nam->post(request, form.query(QUrl::FullyEncoded).toUtf8());

    // Reply self-cleanup + single-shot timeout are wired by armReplyLifecycle()
    // (see replylifecycle.h): fire-and-forget, bound to the reply itself so it is
    // always deleted on finish and a timeout aborts it into the Error branch below.
    // The decode handler below uses `this` as its context object, so Qt
    // auto-disconnects it if the service dies first; a late reply is then simply
    // dropped (no use-after-free, no spurious emit).
    armReplyLifecycle(reply, m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [this, reply, correlationId, gen]() {
        if (gen != m_generation)
            return;   // cancelled since this request began — drop the late decision
        if (reply->error() != QNetworkReply::NoError) {
            AccessDecision d;
            d.result = AccessDecision::Result::Error;   // timeout OR transport == Error
            d.correlationId = correlationId;
            d.reason = (reply->error() == QNetworkReply::OperationCanceledError)
                           ? QStringLiteral("Verification timed out")
                           : reply->errorString();
            emit decided(d);
            return;
        }
        emit decided(decodeResponse(reply->readAll(), correlationId));
    });
}

} // namespace AccessControl
