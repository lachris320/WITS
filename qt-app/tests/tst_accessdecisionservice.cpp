#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include "capturingnam.h"
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/accessdecisionservice.h"

using namespace AccessControl;

// A reply that never finishes on its own — only abort() (fired by the
// service's timeout timer) completes it, with OperationCanceledError.
class HangingReply : public QNetworkReply
{
    Q_OBJECT
public:
    explicit HangingReply(QObject *parent = nullptr) : QNetworkReply(parent)
    {
        open(QIODevice::ReadOnly);
    }
    void abort() override
    {
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted"));
        setFinished(true);
        emit errorOccurred(QNetworkReply::OperationCanceledError);
        emit finished();
    }
protected:
    qint64 readData(char *, qint64) override { return -1; }
};

class HangingNam : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &, QIODevice *) override
    {
        return new HangingReply(this);
    }
};

class TestAccessDecisionService : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void decodeGrantedResponse();
    void decodeGrantedWithoutSubjectIsError();
    void decodeDeniedResponse();
    void decodeInvalidBodyIsError();
    void verifyGrantedEmitsDecidedGrantedAndSendsRequest();
    void verifyTransportFailureEmitsError();
    void verifyTimeoutEmitsError();   // the required invariant
    void cancelDropsPendingDecision();
};

void TestAccessDecisionService::decodeGrantedResponse()
{
    const AccessDecision d = AccessDecisionService::decodeResponse(
        QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"S-1\"}"),
        QStringLiteral("corr-x"));
    QCOMPARE(d.result, AccessDecision::Result::Granted);
    QCOMPARE(d.subjectId, QStringLiteral("S-1"));
    QCOMPARE(d.correlationId, QStringLiteral("corr-x"));
}

void TestAccessDecisionService::decodeGrantedWithoutSubjectIsError()
{
    // A 200 that says "granted" but carries no subject_id must NOT authorize.
    const AccessDecision d = AccessDecisionService::decodeResponse(
        QByteArrayLiteral("{\"decision\":\"granted\"}"), QStringLiteral("corr-n"));
    QCOMPARE(d.result, AccessDecision::Result::Error);
    QVERIFY(d.result != AccessDecision::Result::Granted);
    // An empty-string subject_id is equally unacceptable.
    const AccessDecision d2 = AccessDecisionService::decodeResponse(
        QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"\"}"),
        QStringLiteral("corr-n2"));
    QCOMPARE(d2.result, AccessDecision::Result::Error);
}

void TestAccessDecisionService::decodeDeniedResponse()
{
    const AccessDecision d = AccessDecisionService::decodeResponse(
        QByteArrayLiteral("{\"decision\":\"denied\",\"message\":\"expired card\"}"),
        QStringLiteral("corr-y"));
    QCOMPARE(d.result, AccessDecision::Result::Denied);
    QCOMPARE(d.reason, QStringLiteral("expired card"));
}

void TestAccessDecisionService::decodeInvalidBodyIsError()
{
    // Malformed / non-JSON body is infrastructure trouble, not a policy denial.
    const AccessDecision d = AccessDecisionService::decodeResponse(
        QByteArrayLiteral("<html>502</html>"), QStringLiteral("corr-z"));
    QCOMPARE(d.result, AccessDecision::Result::Error);
}

void TestAccessDecisionService::verifyGrantedEmitsDecidedGrantedAndSendsRequest()
{
    CapturingNam nam(QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"S-9\"}"));
    AccessDecisionService svc(&nam);
    QSignalSpy spy(&svc, &AccessDecisionService::decided);
    Credential c;
    c.kind = CredentialKind::Rfid;
    c.raw = QStringLiteral("CARD-9");
    c.gateId = QStringLiteral("gate-a");
    svc.verify(c);
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    const auto d = qvariant_cast<AccessDecision>(spy.at(0).at(0));
    QCOMPARE(d.result, AccessDecision::Result::Granted);
    QCOMPARE(d.subjectId, QStringLiteral("S-9"));

    // Assert the request the service actually assembled (CapturingNam records it).
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QVERIFY(nam.lastUrl.toString().endsWith(QStringLiteral("access_verify.php")));
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    const QString body = QString::fromUtf8(nam.lastBody);
    QVERIFY(body.contains(QStringLiteral("raw=CARD-9")));
    QVERIFY(body.contains(QStringLiteral("gate_id=gate-a")));
    QVERIFY(body.contains(QStringLiteral("kind=")));   // CredentialKind encoded
}

void TestAccessDecisionService::verifyTransportFailureEmitsError()
{
    // A transport failure (connection refused) carries no decodable decision:
    // it is Error, never a fabricated Denied — and its reason is NOT the timeout
    // message (this proves the transport branch, distinct from the timeout branch).
    CapturingNam nam(QByteArrayLiteral(""), QNetworkReply::ConnectionRefusedError, 0);
    AccessDecisionService svc(&nam);
    QSignalSpy spy(&svc, &AccessDecisionService::decided);
    Credential c;
    c.raw = QStringLiteral("CARD-X");
    svc.verify(c);
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    const auto d = qvariant_cast<AccessDecision>(spy.at(0).at(0));
    QCOMPARE(d.result, AccessDecision::Result::Error);
    QVERIFY(d.result != AccessDecision::Result::Denied);
    QVERIFY(d.reason != QStringLiteral("Verification timed out"));
}

void TestAccessDecisionService::verifyTimeoutEmitsError()
{
    HangingNam nam;
    AccessDecisionService svc(&nam);
    svc.setTimeoutMs(20);   // tiny timeout; reply never finishes on its own
    QSignalSpy spy(&svc, &AccessDecisionService::decided);
    Credential c;
    c.raw = QStringLiteral("CARD-STUCK");
    svc.verify(c);
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    const auto d = qvariant_cast<AccessDecision>(spy.at(0).at(0));
    // A timeout is Error, NEVER a fabricated Denied — and the reason proves it was
    // the timeout/abort path, not some other transport error masquerading as one.
    QCOMPARE(d.result, AccessDecision::Result::Error);
    QVERIFY(d.result != AccessDecision::Result::Denied);
    QCOMPARE(d.reason, QStringLiteral("Verification timed out"));
}

void TestAccessDecisionService::cancelDropsPendingDecision()
{
    // CapturingNam finishes its reply on the event loop (not synchronously), so a
    // cancel() issued before we spin the loop invalidates the in-flight verify: the
    // finished handler sees a newer generation and emits nothing. This is the
    // contract a capture adapter relies on when it stops mid-verify.
    CapturingNam nam(QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"S-1\"}"));
    AccessDecisionService svc(&nam);
    QSignalSpy spy(&svc, &AccessDecisionService::decided);
    Credential c;
    c.raw = QStringLiteral("CARD-1");
    svc.verify(c);
    svc.cancel();          // invalidate before the reply is delivered
    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);   // the late decision was dropped, not emitted
}

QTEST_MAIN(TestAccessDecisionService)
#include "tst_accessdecisionservice.moc"
