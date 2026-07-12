#include <QtTest>
#include <QSignalSpy>
#include "GuestViewModel.h"

class TestGuestViewModel : public QObject
{
    Q_OBJECT
private slots:
    void validateRequiresNameCompanyPurpose();
    void applyResponseSuccessEmitsSucceeded();
    void applyResponseFailureEmitsFailed();
    void submitWithMissingFieldsEmitsFailedNoNetwork();
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
    GuestViewModel vm;
    QSignalSpy ok(&vm, &GuestViewModel::guestSucceeded);
    vm.applyGuestResponse(R"({"status":"success","message":"Welcome"})");
    QCOMPARE(ok.count(), 1);
    QCOMPARE(ok.at(0).at(0).toString(), QStringLiteral("Welcome"));
}

void TestGuestViewModel::applyResponseFailureEmitsFailed()
{
    GuestViewModel vm;
    QSignalSpy bad(&vm, &GuestViewModel::guestFailed);
    vm.applyGuestResponse(R"({"status":"error","message":"Nope"})");
    QCOMPARE(bad.count(), 1);
}

void TestGuestViewModel::submitWithMissingFieldsEmitsFailedNoNetwork()
{
    GuestViewModel vm;
    QSignalSpy bad(&vm, &GuestViewModel::guestFailed);
    vm.submitGuest("Ana", "0917", "", "Meeting");   // company missing
    QCOMPARE(bad.count(), 1);                         // rejected before any POST
}

QTEST_MAIN(TestGuestViewModel)
#include "tst_guestviewmodel.moc"
