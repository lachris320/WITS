#include <QtTest>
#include <QByteArray>
#include <QList>
#include <QPair>
#include "visitorcontroller.h"
#include "visitordata.h"

class TestVisitorController : public QObject
{
    Q_OBJECT
private slots:
    // parseVisitorsResponse
    void parseSuccessPayload();
    void parseCountPropagation();
    void parseInvalidJson();
    void parseStatusNotSuccess();
    void parseMalformedTimeIn();
    void parseMissingFields();

    // wireDateType
    void wireDateTypeAllVariants();

    // monthRange
    void monthRangeJune2026();
    void monthRangeFebruaryLeapYear();
    void monthRangeDecember();

    // defaultExportFileName
    void fileNameAllNoSearch();
    void fileNameSpecificDay();
    void fileNameMonth();
    void fileNameDateRange();
    void fileNameWithSearchSpacesToUnderscores();
    void fileNameSearchWithoutSpaces();
};

void TestVisitorController::parseSuccessPayload()
{
    const QByteArray json = R"({
        "status": "success",
        "count": 2,
        "visitors": [
            {"name": "Juan Dela Cruz", "company": "Acme Corp",
             "purpose": "Research", "time_in": "2026-06-15 14:30:00"},
            {"name": "Maria Clara", "company": "Globex Inc",
             "purpose": "Book Donation", "time_in": "2026-06-16 09:15:00"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 2);
    QCOMPARE(records[0].name,    QString("Juan Dela Cruz"));
    QCOMPARE(records[0].company, QString("Acme Corp"));
    QCOMPARE(records[0].purpose, QString("Research"));
    QCOMPARE(records[0].date,    QString("2026-06-15"));
    QCOMPARE(records[0].time,    QString("02:30 PM"));
    QCOMPARE(records[1].date,    QString("2026-06-16"));
    QCOMPARE(records[1].time,    QString("09:15 AM"));
    QCOMPARE(totalCount, 2);
}

void TestVisitorController::parseCountPropagation()
{
    // The server "count" field is propagated even when it differs from the
    // number of returned rows (e.g. a capped result set).
    const QByteArray json = R"({
        "status": "success",
        "count": 25,
        "visitors": [
            {"name": "Juan Dela Cruz", "company": "Acme Corp",
             "purpose": "Research", "time_in": "2026-06-15 14:30:00"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 1);
    QCOMPARE(totalCount, 25);
}

void TestVisitorController::parseInvalidJson()
{
    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(!VisitorController::parseVisitorsResponse(QByteArray("<html>502 Bad Gateway</html>"),
                                                      &records, &totalCount, &errorMsg));
    QCOMPARE(errorMsg, QString("Invalid server response."));
}

void TestVisitorController::parseStatusNotSuccess()
{
    const QByteArray json = R"({"status": "error", "message": "No database connection."})";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(!VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(errorMsg, QString("No database connection."));
}

void TestVisitorController::parseMalformedTimeIn()
{
    const QByteArray json = R"({
        "status": "success",
        "count": 1,
        "visitors": [
            {"name": "Juan Dela Cruz", "company": "Acme Corp",
             "purpose": "Research", "time_in": "15/06/2026 at 2pm"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].date, QString("N/A"));
    QCOMPARE(records[0].time, QString("N/A"));
}

void TestVisitorController::parseMissingFields()
{
    const QByteArray json = R"({
        "status": "success",
        "count": 1,
        "visitors": [
            {"time_in": "2026-06-15 08:00:00"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].name,    QString(""));
    QCOMPARE(records[0].company, QString(""));
    QCOMPARE(records[0].purpose, QString(""));
    QCOMPARE(records[0].date,    QString("2026-06-15"));
    QCOMPARE(records[0].time,    QString("08:00 AM"));
}

void TestVisitorController::wireDateTypeAllVariants()
{
    QCOMPARE(VisitorController::wireDateType(DateFilterType::All),         QString("all"));
    QCOMPARE(VisitorController::wireDateType(DateFilterType::SpecificDay), QString("Specific Day"));
    QCOMPARE(VisitorController::wireDateType(DateFilterType::Month),       QString("Month"));
    QCOMPARE(VisitorController::wireDateType(DateFilterType::DateRange),   QString("Date Range"));
}

void TestVisitorController::monthRangeJune2026()
{
    const QPair<QString, QString> range = VisitorController::monthRange(6, 2026);
    QCOMPARE(range.first,  QString("2026-06-01"));
    QCOMPARE(range.second, QString("2026-06-30"));
}

void TestVisitorController::monthRangeFebruaryLeapYear()
{
    const QPair<QString, QString> range = VisitorController::monthRange(2, 2024);
    QCOMPARE(range.first,  QString("2024-02-01"));
    QCOMPARE(range.second, QString("2024-02-29"));
}

void TestVisitorController::monthRangeDecember()
{
    const QPair<QString, QString> range = VisitorController::monthRange(12, 2025);
    QCOMPARE(range.first,  QString("2025-12-01"));
    QCOMPARE(range.second, QString("2025-12-31"));
}

void TestVisitorController::fileNameAllNoSearch()
{
    QCOMPARE(VisitorController::defaultExportFileName(VisitorFilter{}),
             QString("VisitorLogs_All.xlsx"));
}

void TestVisitorController::fileNameSpecificDay()
{
    VisitorFilter f;
    f.dateType  = DateFilterType::SpecificDay;
    f.startDate = "2026-06-15";
    f.endDate   = "2026-06-15";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_2026-06-15.xlsx"));
}

void TestVisitorController::fileNameMonth()
{
    VisitorFilter f;
    f.dateType  = DateFilterType::Month;
    f.startDate = "2026-01-01";
    f.endDate   = "2026-01-31";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_January_2026.xlsx"));
}

void TestVisitorController::fileNameDateRange()
{
    VisitorFilter f;
    f.dateType  = DateFilterType::DateRange;
    f.startDate = "2026-06-01";
    f.endDate   = "2026-06-15";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_Range_2026-06-01_to_2026-06-15.xlsx"));
}

void TestVisitorController::fileNameWithSearchSpacesToUnderscores()
{
    VisitorFilter f;
    f.search = "Juan Dela Cruz";   // dateType stays All
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_Juan_Dela_Cruz_All.xlsx"));
}

void TestVisitorController::fileNameSearchWithoutSpaces()
{
    VisitorFilter f;
    f.search    = "Acme";
    f.dateType  = DateFilterType::Month;
    f.startDate = "2026-02-01";
    f.endDate   = "2026-02-28";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_Acme_February_2026.xlsx"));
}

QTEST_MAIN(TestVisitorController)
#include "tst_visitorcontroller.moc"
