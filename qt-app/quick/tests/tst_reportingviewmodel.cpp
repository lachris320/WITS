#include <QtTest>
#include <QDate>
#include <QJsonObject>
#include <QJsonDocument>
#include "ReportingViewModel.h"

class TestReportingViewModel : public QObject
{
    Q_OBJECT
private slots:
    void buildFiltersDaySendsStringTypeAndRange();
    void buildFiltersMonthSendsRange();
    void buildFiltersSemesterSendsComponentsNotRange();
    void buildFiltersCustomSendsRange();
    void buildFiltersPassesDeptAndCourse();
    void aggregateSumsAndRanksByCourse();
    void aggregateEmptyIsEmpty();
    void deriveTilesComputesTotals();
    void deriveTilesEmptyIsZeroAndDash();
    void canGenerateFalseWithoutDepartment();
    void canGenerateDayRequiresValidDate();
    void canGenerateMonthRequiresMonthAndYear();
    void canGenerateSemesterRequiresSemesterAndYear();
    void canGenerateCustomRequiresOrderedRange();
    void settersEmitAndUpdateCanGenerate();
};

void TestReportingViewModel::buildFiltersDaySendsStringTypeAndRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("day"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-08-14"));
}

void TestReportingViewModel::buildFiltersMonthSendsRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 1, QDate(), 2, 2026, "", 0, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("month"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-02-01"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-02-28"));
}

void TestReportingViewModel::buildFiltersSemesterSendsComponentsNotRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 2, QDate(), 0, 0, "First Semester", 2026, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("semester"));
    QCOMPARE(f.value("year").toInt(), 2026);
    QCOMPARE(f.value("semester").toString(), QStringLiteral("First Semester"));
    // Semester ranging is server-side: no client start/end sent.
    QVERIFY(!f.contains("start"));
    QVERIFY(!f.contains("end"));
}

void TestReportingViewModel::buildFiltersCustomSendsRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 3, QDate(), 0, 0, "", 0, QDate(2026, 1, 1), QDate(2026, 3, 31));
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("custom"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-01-01"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-03-31"));
}

void TestReportingViewModel::buildFiltersPassesDeptAndCourse()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "BSCE", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("department").toString(), QStringLiteral("CE"));
    QCOMPARE(f.value("course").toString(), QStringLiteral("BSCE"));
}

void TestReportingViewModel::aggregateSumsAndRanksByCourse()
{
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"course":"BSIT","visits":"3"},
        {"course":"BSCE","visits":"10"},
        {"course":"BSIT","visits":"4"},
        {"course":"BSCE","visits":"5"}
    ])").array();
    const QList<BarsModel::Bar> bars = ReportingViewModel::aggregateVisitsByCourse(data);
    QCOMPARE(bars.size(), 2);
    // Ranked descending by total: BSCE=15 then BSIT=7.
    QCOMPARE(bars.at(0).label, QStringLiteral("BSCE"));
    QCOMPARE(bars.at(0).value, 15.0);
    QCOMPARE(bars.at(1).label, QStringLiteral("BSIT"));
    QCOMPARE(bars.at(1).value, 7.0);
}

void TestReportingViewModel::aggregateEmptyIsEmpty()
{
    QCOMPARE(ReportingViewModel::aggregateVisitsByCourse(QJsonArray()).size(), 0);
}

void TestReportingViewModel::deriveTilesComputesTotals()
{
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"course":"BSIT","visits":"3"},
        {"course":"BSCE","visits":"10"},
        {"course":"BSCE","visits":"5"}
    ])").array();
    const ReportingViewModel::Tiles t = ReportingViewModel::deriveTiles(data);
    QCOMPARE(t.totalVisits, 18);
    QCOMPARE(t.studentsShown, 3);
    QCOMPARE(t.topCourse, QStringLiteral("BSCE"));   // 15 > 3
}

void TestReportingViewModel::deriveTilesEmptyIsZeroAndDash()
{
    const ReportingViewModel::Tiles t = ReportingViewModel::deriveTiles(QJsonArray());
    QCOMPARE(t.totalVisits, 0);
    QCOMPARE(t.studentsShown, 0);
    QCOMPARE(t.topCourse, QStringLiteral("—"));
}

void TestReportingViewModel::canGenerateFalseWithoutDepartment()
{
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(!vm.canGenerate());          // no department
}

void TestReportingViewModel::canGenerateDayRequiresValidDate()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");              // NOTE: in this task setDepartment only stores + fires network; see Task 5
    vm.setDurationType(0);
    QVERIFY(!vm.canGenerate());          // no day yet
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.setDay("not-a-date");
    QVERIFY(!vm.canGenerate());
}

void TestReportingViewModel::canGenerateMonthRequiresMonthAndYear()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(1);
    vm.setMonth(2);
    QVERIFY(!vm.canGenerate());          // no year
    vm.setMonthYear(2026);
    QVERIFY(vm.canGenerate());
    vm.setMonth(0);
    QVERIFY(!vm.canGenerate());          // month out of 1..12
}

void TestReportingViewModel::canGenerateSemesterRequiresSemesterAndYear()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(2);
    vm.setSemester("First Semester");
    QVERIFY(!vm.canGenerate());          // no year
    vm.setSemYear(2026);
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::canGenerateCustomRequiresOrderedRange()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(3);
    vm.setCustomStart("2026-03-31");
    vm.setCustomEnd("2026-01-01");
    QVERIFY(!vm.canGenerate());          // start > end
    vm.setCustomEnd("2026-06-30");
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::settersEmitAndUpdateCanGenerate()
{
    ReportingViewModel vm;
    QSignalSpy canGenSpy(&vm, &ReportingViewModel::canGenerateChanged);
    QSignalSpy durSpy(&vm, &ReportingViewModel::durationTypeChanged);
    vm.setDurationType(3);
    QVERIFY(durSpy.count() >= 1);
    vm.setDepartment("CE");
    vm.setCustomStart("2026-01-01");
    vm.setCustomEnd("2026-02-01");
    QVERIFY(vm.canGenerate());
    QVERIFY(canGenSpy.count() >= 1);
}

QTEST_MAIN(TestReportingViewModel)
#include "tst_reportingviewmodel.moc"
