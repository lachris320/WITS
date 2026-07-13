#include <QtTest>
#include <QSignalSpy>
#include "DashboardViewModel.h"
#include "BarsModel.h"

class TestDashboardViewModel : public QObject
{
    Q_OBJECT
private slots:
    void formatPeakHourMapsTo12Hour();
    void applySummaryPopulatesStatsAndModels();
    void applySummaryDerivesPeak();
    void applyInvalidSetsErrorText();
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

QTEST_MAIN(TestDashboardViewModel)
#include "tst_dashboardviewmodel.moc"
