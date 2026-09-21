#include <QtTest>
#include <QSignalSpy>
#include <QUrlQuery>
#include "DashboardViewModel.h"
#include "BarsModel.h"
#include "AdminSession.h"
#include "capturingnam.h"

class TestDashboardViewModel : public QObject
{
    Q_OBJECT
private slots:
    void formatPeakHourMapsTo12Hour();
    void applySummaryPopulatesStatsAndModels();
    void applySummaryDerivesPeak();
    void applyInvalidSetsErrorText();
    void supersededRequestSeqIsNotCurrent();
    void refresh_postsWithAdminKeyInBody();
    void refresh_guard401_setsError();
};

void TestDashboardViewModel::formatPeakHourMapsTo12Hour()
{
    QCOMPARE(DashboardViewModel::formatPeakHour(0),  QStringLiteral("12 AM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(8),  QStringLiteral("8 AM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(12), QStringLiteral("12 PM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(14), QStringLiteral("2 PM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(-1), QStringLiteral("—"));
}

void TestDashboardViewModel::applySummaryPopulatesStatsAndModels()
{
    DashboardViewModel vm;
    QSignalSpy spy(&vm, &DashboardViewModel::dataChanged);
    vm.applySummary(R"({
        "status":"success","today":128,"week":812,"students":3450,
        "hourly":[{"hour":8,"count":12},{"hour":9,"count":34}],
        "departments":[{"name":"CE","count":210},{"name":"IT","count":180}]
    })");
    QVERIFY(spy.count() >= 1);
    QCOMPARE(vm.statToday(), 128);
    QCOMPARE(vm.statWeek(), 812);
    QCOMPARE(vm.statStudents(), 3450);
    QCOMPARE(vm.hourlyModel()->rowCount(), 2);
    QCOMPARE(vm.departmentModel()->rowCount(), 2);
    QVERIFY(vm.errorText().isEmpty());
}

void TestDashboardViewModel::applySummaryDerivesPeak()
{
    DashboardViewModel vm;
    vm.applySummary(R"({
        "status":"success","today":1,"week":1,"students":1,
        "hourly":[{"hour":8,"count":12},{"hour":10,"count":41},{"hour":9,"count":34}]
    })");
    // Max count is 41 at hour 10, which is index 1 in array order.
    QCOMPARE(vm.peakHourIndex(), 1);
    QCOMPARE(vm.peakHourLabel(), QStringLiteral("10 AM"));
}

void TestDashboardViewModel::applyInvalidSetsErrorText()
{
    DashboardViewModel vm;
    vm.applySummary(R"({"status":"error"})");
    QVERIFY(!vm.errorText().isEmpty());
    QCOMPARE(vm.peakHourIndex(), -1);
}

// Pins the in-flight request-generation guard's increment/compare arithmetic
// (DashboardViewModel::nextRequestSeq / isCurrentRequest) — see the identical
// seam/test in tst_visitlogsviewmodel.cpp for the full rationale and the
// scope of what this test does and does not prove. Short version: this
// confirms a superseded seq stops comparing as "current" once a newer
// request is issued; it does not exercise refresh()'s real QNetworkReply
// wiring, which was verified by inspection instead of a network-hitting test.
void TestDashboardViewModel::supersededRequestSeqIsNotCurrent()
{
    DashboardViewModel vm;
    const quint64 first = vm.nextRequestSeq();     // e.g. first navigation to Dashboard
    QVERIFY(vm.isCurrentRequest(first));

    const quint64 second = vm.nextRequestSeq();    // Retry mashed before the GET returns
    QVERIFY(!vm.isCurrentRequest(first));
    QVERIFY(vm.isCurrentRequest(second));
}

void TestDashboardViewModel::refresh_postsWithAdminKeyInBody()
{
    AdminSession::instance().setKey("test-key");
    CapturingNam nam;
    DashboardViewModel vm(nullptr, &nam);
    vm.refresh();

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    const QUrlQuery form(QString::fromUtf8(nam.lastBody));
    QCOMPARE(form.queryItemValue("admin_key"), QStringLiteral("test-key"));
    QVERIFY(!QUrlQuery(nam.lastUrl).hasQueryItem("admin_key"));
    AdminSession::instance().clear();
}

void TestDashboardViewModel::refresh_guard401_setsError()
{
    AdminSession::instance().setKey("");
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"error\"}"),
                     QNetworkReply::AuthenticationRequiredError, 401);
    DashboardViewModel vm(nullptr, &nam);
    QSignalSpy err(&vm, &DashboardViewModel::errorTextChanged);
    vm.refresh();
    QVERIFY(err.wait(1000));
    QVERIFY(!vm.errorText().isEmpty());   // 401 lands in the error state, not empty success
}

QTEST_MAIN(TestDashboardViewModel)
#include "tst_dashboardviewmodel.moc"
