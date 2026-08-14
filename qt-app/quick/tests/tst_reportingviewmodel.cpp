#include <QtTest>
#include <QDate>
#include <QJsonObject>
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

QTEST_MAIN(TestReportingViewModel)
#include "tst_reportingviewmodel.moc"
