#include <QtTest>
#include "visitlogparser.h"

// Frozen contract fixture (spec §5.2). Synthetic data.
static QByteArray okFixture()
{
    return R"({
        "status":"success",
        "count":2,
        "visits":[
            {"date":"2026-07-13","name":"Maria Santos","course":"BSCE",
             "department":"CE","time_in":"08:14"},
            {"date":"2026-07-13","name":"Jose Ramirez","course":"BSEE",
             "department":"EE","time_in":"08:31"}
        ]
    })";
}

class TestVisitLogParser : public QObject
{
    Q_OBJECT
private slots:
    void parsesRows();
    void rejectsNonObject();
    void rejectsNonSuccessStatus();
    void emptyVisitsIsSuccessWithZeroCount();
};

void TestVisitLogParser::parsesRows()
{
    QList<StudentVisitRecord> rows; int count = -1; QString err;
    QVERIFY(VisitLogParser::parse(okFixture(), rows, count, err));
    QVERIFY(err.isEmpty());
    QCOMPARE(count, 2);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).name, QStringLiteral("Maria Santos"));
    QCOMPARE(rows.at(0).course, QStringLiteral("BSCE"));
    QCOMPARE(rows.at(0).department, QStringLiteral("CE"));
    QCOMPARE(rows.at(0).timeIn, QStringLiteral("08:14"));
    QCOMPARE(rows.at(0).date, QStringLiteral("2026-07-13"));
}

void TestVisitLogParser::rejectsNonObject()
{
    QList<StudentVisitRecord> rows; int count = -1; QString err;
    QVERIFY(!VisitLogParser::parse("<html>", rows, count, err));
    QVERIFY(!err.isEmpty());
}

void TestVisitLogParser::rejectsNonSuccessStatus()
{
    QList<StudentVisitRecord> rows; int count = -1; QString err;
    QVERIFY(!VisitLogParser::parse(R"({"status":"error"})", rows, count, err));
    QVERIFY(!err.isEmpty());
}

void TestVisitLogParser::emptyVisitsIsSuccessWithZeroCount()
{
    QList<StudentVisitRecord> rows; int count = -1; QString err;
    QVERIFY(VisitLogParser::parse(
        R"({"status":"success","count":0,"visits":[]})", rows, count, err));
    QCOMPARE(count, 0);
    QVERIFY(rows.isEmpty());
}

QTEST_MAIN(TestVisitLogParser)
#include "tst_visitlogparser.moc"
