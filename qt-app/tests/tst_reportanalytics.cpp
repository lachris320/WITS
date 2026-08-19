#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "reportanalytics.h"

class TstReportAnalytics : public QObject
{
    Q_OBJECT
private slots:
    void empty_hasNoData();
    void kpis_sumUniqueAvgTopDept();
    void kpis_oneStudent();
    void kpis_blankDepartmentBucketsUnspecified();
    void kpis_rawStringVisitsIsCallerError();

private:
    // Synthetic rows only. `visits` is numeric (already normalized).
    static QJsonObject row(const QString &id, const QString &name, const QString &course,
                           const QString &dept, int visits) {
        return QJsonObject{{"school_id", id}, {"name", name}, {"course", course},
                           {"department", dept}, {"visits", visits}};
    }
};

void TstReportAnalytics::empty_hasNoData() {
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{});
    QVERIFY(!a.kpis.hasData);
    QCOMPARE(a.kpis.totalVisits, 0);
    QCOMPARE(a.kpis.uniqueVisitors, 0);
    QVERIFY(a.topStudents.isEmpty());
    QVERIFY(a.topDepartments.isEmpty());
}

void TstReportAnalytics::kpis_sumUniqueAvgTopDept() {
    const QJsonArray rows{
        row("1", "Ana",  "BSIT",  "CCS", 5),
        row("2", "Ben",  "BSCS",  "CCS", 3),
        row("3", "Cara", "BSEcE", "CoE", 4),
    };
    const ReportAnalytics a = ReportAnalytics::compute(rows);
    QVERIFY(a.kpis.hasData);
    QCOMPARE(a.kpis.totalVisits, 12);
    QCOMPARE(a.kpis.uniqueVisitors, 3);
    QCOMPARE(a.kpis.avgVisitsPerVisitor, 4.0);
    QCOMPARE(a.kpis.topDepartment, QStringLiteral("CCS")); // 8 > CoE 4
    QCOMPARE(a.kpis.topDepartmentVisits, 8);
}

void TstReportAnalytics::kpis_oneStudent() {
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{ row("1","Ana","BSIT","CCS",7) });
    QVERIFY(a.kpis.hasData);
    QCOMPARE(a.kpis.totalVisits, 7);
    QCOMPARE(a.kpis.uniqueVisitors, 1);
    QCOMPARE(a.kpis.avgVisitsPerVisitor, 7.0);
    QCOMPARE(a.kpis.topDepartment, QStringLiteral("CCS"));
}

void TstReportAnalytics::kpis_blankDepartmentBucketsUnspecified() {
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{ row("1","Ana","BSIT","",9) });
    QCOMPARE(a.kpis.topDepartment, QStringLiteral("(Unspecified)"));
    QCOMPARE(a.kpis.topDepartmentVisits, 9);
}

void TstReportAnalytics::kpis_rawStringVisitsIsCallerError() {
    // Contract: numeric visits required; a raw string counts as 0 (QJsonValue::toInt()).
    QJsonObject bad{{"school_id","1"},{"name","Ana"},{"course","BSIT"},
                    {"department","CCS"},{"visits","5"}};  // STRING, not int
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{ bad });
    QCOMPARE(a.kpis.totalVisits, 0);
}

QTEST_APPLESS_MAIN(TstReportAnalytics)
#include "tst_reportanalytics.moc"
