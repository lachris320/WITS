#include <QtTest>
#include <QNetworkReply>
#include <QPointer>
#include <memory>

#include "capturingnam.h"
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

class TestHttpClient : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void defaultOwnsAPolicyEnforcingNam();
    void defaultManagerCarriesCurrentPolicy();
    void eachDefaultClientHasItsOwnManager();
    void ownedManagerDiesWithClient();
    void injectedManagerIsUsedNotOwned();
    void requestsThroughInjectedManagerReachIt();
    void policySnapshotTakenAtConstruction();
    void offlineHttpNeverAnswers();
};

void TestHttpClient::cleanup()
{
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
}

void TestHttpClient::defaultOwnsAPolicyEnforcingNam()
{
    HttpClient http;
    auto *nam = qobject_cast<PolicyEnforcingNam *>(http.manager());
    QVERIFY(nam);
    QCOMPARE(nam->parent(), static_cast<QObject *>(&http));
    QVERIFY(http.ownsManager());
}

void TestHttpClient::defaultManagerCarriesCurrentPolicy()
{
    const auto policy = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(policy);
    HttpClient http;
    QCOMPARE(http.policy().get(), policy.get());
    auto *nam = qobject_cast<PolicyEnforcingNam *>(http.manager());
    QVERIFY(nam);
    QCOMPARE(nam->policy().get(), policy.get());
}

void TestHttpClient::eachDefaultClientHasItsOwnManager()
{
    // One security policy, not one physical manager (spec §2): owners keep
    // today's one-manager-per-owner topology.
    HttpClient a;
    HttpClient b;
    QVERIFY(a.manager() != b.manager());
    QCOMPARE(a.policy().get(), b.policy().get());
}

void TestHttpClient::ownedManagerDiesWithClient()
{
    QPointer<QNetworkAccessManager> nam;
    {
        HttpClient http;
        nam = http.manager();
        QVERIFY(nam);
    }
    QVERIFY(nam.isNull());
}

void TestHttpClient::injectedManagerIsUsedNotOwned()
{
    CapturingNam capturing;
    QPointer<QNetworkAccessManager> guard(&capturing);
    {
        HttpClient http(nullptr, &capturing);
        QCOMPARE(http.manager(), static_cast<QNetworkAccessManager *>(&capturing));
        QVERIFY(!http.ownsManager());
        QVERIFY(capturing.parent() == nullptr);                       // not reparented
        QVERIFY(http.findChildren<PolicyEnforcingNam *>().isEmpty());  // no stray owned manager
    }
    QVERIFY(!guard.isNull());                                         // client left it alone
}

void TestHttpClient::requestsThroughInjectedManagerReachIt()
{
    CapturingNam capturing;
    HttpClient http(nullptr, &capturing);
    const QNetworkRequest req(QUrl(QStringLiteral("http://localhost/loams_api/get_departments.php")));
    std::unique_ptr<QNetworkReply> reply(http.manager()->get(req));
    QCOMPARE(capturing.lastOp, QNetworkAccessManager::GetOperation);
    QCOMPARE(capturing.lastUrl, req.url());
}

void TestHttpClient::policySnapshotTakenAtConstruction()
{
    const auto first = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(first);
    HttpClient early;
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
    HttpClient late;
    QCOMPARE(early.policy().get(), first.get());
    QVERIFY(late.policy().get() != first.get());
}

void TestHttpClient::offlineHttpNeverAnswers()
{
    // Test-safety helper: a request through OfflineHttp is recorded and then
    // stays in flight — no backend, no canned answer — until it is aborted.
    OfflineHttp offline;
    std::unique_ptr<QNetworkReply> reply(offline.http.manager()->get(
        QNetworkRequest(QUrl(QStringLiteral("http://localhost/loams_api/get_departments.php")))));
    QCOMPARE(offline.nam.requestCount(), 1);
    QCOMPARE(offline.nam.lastUrl.path(), QStringLiteral("/loams_api/get_departments.php"));
    QTest::qWait(100);                       // negative assertion: nothing may answer
    QVERIFY(!reply->isFinished());
    reply->abort();
    QVERIFY(reply->isFinished());
    QCOMPARE(reply->error(), QNetworkReply::OperationCanceledError);
}

QTEST_MAIN(TestHttpClient)
#include "tst_httpclient.moc"
