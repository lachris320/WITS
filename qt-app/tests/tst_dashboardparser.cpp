#include <QtTest>
#include "dashboardparser.h"

// Frozen contract fixture (spec §5.1). Synthetic data — no real PII.
static QByteArray okFixture()
{
    return R"({
        "status":"success",
        "today":128,
        "week":812,
        "students":3450,
        "hourly":[{"hour":8,"count":12},{"hour":9,"count":34},{"hour":10,"count":41}],
        "departments":[{"name":"CE","count":210},{"name":"IT","count":180}]
    })";
}

class TestDashboardParser : public QObject
{
    Q_OBJECT
private slots:
    void parsesWellFormedSummary();
    void rejectsNonObject();
    void rejectsNonSuccessStatus();
    void toleratesMissingArrays();
};

void TestDashboardParser::parsesWellFormedSummary()
{
    DashboardSummary s; QString err;
    QVERIFY(DashboardParser::parse(okFixture(), s, err));
    QVERIFY(err.isEmpty());
    QCOMPARE(s.today, 128);
    QCOMPARE(s.week, 812);
    QCOMPARE(s.students, 3450);
    QCOMPARE(s.hourly.size(), 3);
    QCOMPARE(s.hourly.at(2).hour, 10);
    QCOMPARE(s.hourly.at(2).count, 41);
    QCOMPARE(s.departments.size(), 2);
    QCOMPARE(s.departments.at(0).name, QStringLiteral("CE"));
    QCOMPARE(s.departments.at(0).count, 210);
}

void TestDashboardParser::rejectsNonObject()
{
    DashboardSummary s; QString err;
    QVERIFY(!DashboardParser::parse("not json", s, err));
    QVERIFY(!err.isEmpty());
}

void TestDashboardParser::rejectsNonSuccessStatus()
{
    DashboardSummary s; QString err;
    QVERIFY(!DashboardParser::parse(R"({"status":"error"})", s, err));
    QVERIFY(!err.isEmpty());
}

void TestDashboardParser::toleratesMissingArrays()
{
    DashboardSummary s; QString err;
    QVERIFY(DashboardParser::parse(
        R"({"status":"success","today":5,"week":9,"students":100})", s, err));
    QCOMPARE(s.today, 5);
    QVERIFY(s.hourly.isEmpty());
    QVERIFY(s.departments.isEmpty());
}

QTEST_MAIN(TestDashboardParser)
#include "tst_dashboardparser.moc"
