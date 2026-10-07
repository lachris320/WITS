#include <QtTest>
#include <QSignalSpy>
#include "GuestViewModel.h"
#include <QUrlQuery>
#include "apiconfig.h"
#include "capturingnam.h"
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"

class TestGuestViewModel : public QObject
{
    Q_OBJECT
private slots:
    void validateRequiresNameCompanyPurpose();
    void applyResponseSuccessEmitsSucceeded();
    void applyResponseFailureEmitsFailed();
    void submitWithMissingFieldsEmitsFailedNoNetwork();
    void defaultConstructionUsesTransportSeam();
    void guestSubmitGoesThroughInjectedClient();
};

void TestGuestViewModel::validateRequiresNameCompanyPurpose()
{
    QVERIFY(GuestViewModel::validate("Ana", "Acme", "Meeting"));
    QVERIFY(!GuestViewModel::validate("", "Acme", "Meeting"));
    QVERIFY(!GuestViewModel::validate("Ana", "", "Meeting"));
    QVERIFY(!GuestViewModel::validate("Ana", "Acme", ""));
}

void TestGuestViewModel::applyResponseSuccessEmitsSucceeded()
{
    OfflineHttp vmHttp;
    GuestViewModel vm(nullptr, &vmHttp.http);
    QSignalSpy ok(&vm, &GuestViewModel::guestSucceeded);
    vm.applyGuestResponse(R"({"status":"success","message":"Welcome"})");
    QCOMPARE(ok.count(), 1);
    QCOMPARE(ok.at(0).at(0).toString(), QStringLiteral("Welcome"));
}

void TestGuestViewModel::applyResponseFailureEmitsFailed()
{
    OfflineHttp vmHttp;
    GuestViewModel vm(nullptr, &vmHttp.http);
    QSignalSpy bad(&vm, &GuestViewModel::guestFailed);
    vm.applyGuestResponse(R"({"status":"error","message":"Nope"})");
    QCOMPARE(bad.count(), 1);
}

void TestGuestViewModel::submitWithMissingFieldsEmitsFailedNoNetwork()
{
    OfflineHttp vmHttp;
    GuestViewModel vm(nullptr, &vmHttp.http);
    QSignalSpy bad(&vm, &GuestViewModel::guestFailed);
    vm.submitGuest("Ana", "0917", "", "Meeting");   // company missing
    QCOMPARE(bad.count(), 1);                         // rejected before any POST
}

void TestGuestViewModel::defaultConstructionUsesTransportSeam()
{
    GuestViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
}

void TestGuestViewModel::guestSubmitGoesThroughInjectedClient()
{
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"success\",\"message\":\"Welcome\"}"));
    HttpClient http(nullptr, &nam);
    GuestViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    QSignalSpy ok(&vm, &GuestViewModel::guestSucceeded);

    vm.submitGuest(QStringLiteral("Test Guest"), QStringLiteral("0000"),
                   QStringLiteral("Example Org"), QStringLiteral("Visit"));
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("guest_login.php")));
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    const QUrlQuery body(QString::fromUtf8(nam.lastBody));
    QCOMPARE(body.queryItemValue(QStringLiteral("name")), QStringLiteral("Test Guest"));
    QCOMPARE(body.queryItemValue(QStringLiteral("company")), QStringLiteral("Example Org"));
    QTRY_COMPARE(ok.count(), 1);
}

QTEST_MAIN(TestGuestViewModel)
#include "tst_guestviewmodel.moc"
