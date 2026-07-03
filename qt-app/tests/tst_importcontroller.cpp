#include <QtTest>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include "importcontroller.h"
#include "importdata.h"
#include "xlsxdocument.h"

class TestImportController : public QObject
{
    Q_OBJECT
private slots:
    // normalizeHeader
    void normalizeHeaderTrimsLowersStrips();
    void normalizeHeaderIdempotent();

    // mapHeaders
    void mapHeadersSchoolIdFamily();
    void mapHeadersNameFamily();
    void mapHeadersCodeFamily();
    void mapHeadersCourse();
    void mapHeadersYearLevel();
    void mapHeadersDepartmentFamily();
    void mapHeadersGender();
    void mapHeadersStatus();
    void mapHeadersVisits();
    void mapHeadersUnrecognizedFallsBackToColN();
    void mapHeadersFullRealisticRow();

    // parseCsv
    void parseCsvHeaderAndRows();
    void parseCsvSkipsEmptyLine();
    void parseCsvRaggedRowKeptAsIs();
    void parseCsvEmptyTextReturnsEmptyTable();
    void parseCsvColNFallbackEndToEnd();

    // parseDuplicateResponse
    void parseDuplicateResponseSuccessWithDuplicates();
    void parseDuplicateResponseSuccessEmpty();
    void parseDuplicateResponseStatusNotSuccess();
    void parseDuplicateResponseNotAnObject();

    // parseUploadResponse
    void parseUploadResponseSuccess();
    void parseUploadResponseStatusNotSuccess();
    void parseUploadResponsePlainTextFallback();

    // parseExcel
    void parseExcelRoundTrip();
    void parseExcelHeaderOnlyNoDataRows();
    void parseExcelOpenFailedOnBadPath();
};

void TestImportController::normalizeHeaderTrimsLowersStrips()
{
    QCOMPARE(ImportController::normalizeHeader(" School ID "), QString("schoolid"));
    QCOMPARE(ImportController::normalizeHeader("Full_Name"),   QString("fullname"));
    QCOMPARE(ImportController::normalizeHeader("Year-Level"),  QString("yearlevel"));
}

// NOTE: idempotence is trivially satisfied by the Step 3 stub ("" -> "") —
// this slot is expected to PASS in the red phase; it earns its keep only
// once the real implementation lands.
void TestImportController::normalizeHeaderIdempotent()
{
    const QString already = ImportController::normalizeHeader("Full_Name");
    QCOMPARE(ImportController::normalizeHeader(already), already);
}

void TestImportController::mapHeadersSchoolIdFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"School ID", "Student ID", "id"}, idx);
    QCOMPARE(idx.value("school_id"), 2);   // last match wins (same column key)
}

void TestImportController::mapHeadersNameFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Name", "Full Name"}, idx);
    QCOMPARE(idx.value("name"), 1);
}

void TestImportController::mapHeadersCodeFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Code", "Student Code"}, idx);
    QCOMPARE(idx.value("code"), 1);
}

// NOTE: the single-alias tests below deliberately place the target header at
// a NON-ZERO column (behind an unrecognized "Notes" header) and assert
// contains() first — QMap::value() returns 0 for a missing key, so asserting
// index 0 on an empty map would vacuously pass against the Step 3 no-op stub.
void TestImportController::mapHeadersCourse()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Course"}, idx);
    QVERIFY(idx.contains("course"));
    QCOMPARE(idx.value("course"), 1);
}

void TestImportController::mapHeadersYearLevel()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Year Level"}, idx);
    QVERIFY(idx.contains("year_level"));
    QCOMPARE(idx.value("year_level"), 1);
}

void TestImportController::mapHeadersDepartmentFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Department", "Dept"}, idx);
    QCOMPARE(idx.value("department"), 1);
}

void TestImportController::mapHeadersGender()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Gender"}, idx);
    QVERIFY(idx.contains("gender"));
    QCOMPARE(idx.value("gender"), 1);
}

void TestImportController::mapHeadersStatus()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Status"}, idx);
    QVERIFY(idx.contains("status"));
    QCOMPARE(idx.value("status"), 1);
}

void TestImportController::mapHeadersVisits()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Visits"}, idx);
    QVERIFY(idx.contains("visits"));
    QCOMPARE(idx.value("visits"), 1);
}

void TestImportController::mapHeadersUnrecognizedFallsBackToColN()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Course", "Notes"}, idx);
    QVERIFY(idx.contains("col_1"));   // unrecognized header at NON-ZERO column
    QCOMPARE(idx.value("col_1"), 1);
    QVERIFY(!idx.contains("notes"));
}

void TestImportController::mapHeadersFullRealisticRow()
{
    QMap<QString, int> idx;
    const QStringList headers = {"School ID", "Full Name", "Course", "Year Level",
                                 "Department", "Gender", "Status", "Visits", "Notes"};
    ImportController::mapHeaders(headers, idx);
    QCOMPARE(idx.value("school_id"),   0);
    QCOMPARE(idx.value("name"),        1);
    QCOMPARE(idx.value("course"),      2);
    QCOMPARE(idx.value("year_level"),  3);
    QCOMPARE(idx.value("department"),  4);
    QCOMPARE(idx.value("gender"),      5);
    QCOMPARE(idx.value("status"),      6);
    QCOMPARE(idx.value("visits"),      7);
    QCOMPARE(idx.value("col_8"),       8);
}

void TestImportController::parseCsvHeaderAndRows()
{
    const QString text =
        "School ID,Full Name,Course\n"
        "2023-00123,Juan Dela Cruz,BSIT\n"
        "2023-00456,Maria Clara,BSCS\n";

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.headers, QStringList({"School ID", "Full Name", "Course"}));
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz", "BSIT"}));
    QCOMPARE(table.rows[1], QStringList({"2023-00456", "Maria Clara", "BSCS"}));
    QCOMPARE(table.headerIndex.value("school_id"), 0);
    QCOMPARE(table.headerIndex.value("name"),      1);
    QCOMPARE(table.headerIndex.value("course"),    2);
}

void TestImportController::parseCsvSkipsEmptyLine()
{
    const QString text =
        "School ID,Full Name\n"
        "2023-00123,Juan Dela Cruz\n"
        "\n"
        "2023-00456,Maria Clara\n";

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz"}));
    QCOMPARE(table.rows[1], QStringList({"2023-00456", "Maria Clara"}));
}

void TestImportController::parseCsvRaggedRowKeptAsIs()
{
    const QString text =
        "School ID,Full Name,Course\n"
        "2023-00123,Juan Dela Cruz\n";   // missing the Course cell

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.rows.size(), 1);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz"}));   // natural length 2, not padded to 3
}

// NOTE: an empty stub ParsedTable{} already satisfies all three isEmpty()
// checks — this slot is expected to PASS in the red phase; it guards the
// empty-input contract once the real implementation lands.
void TestImportController::parseCsvEmptyTextReturnsEmptyTable()
{
    const ParsedTable table = ImportController::parseCsv(QString());
    QVERIFY(table.headers.isEmpty());
    QVERIFY(table.rows.isEmpty());
    QVERIFY(table.headerIndex.isEmpty());
}

void TestImportController::parseCsvColNFallbackEndToEnd()
{
    const QString text =
        "School ID,Notes\n"
        "2023-00123,Some note\n";

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.headerIndex.value("school_id"), 0);
    QCOMPARE(table.headerIndex.value("col_1"), 1);
    QVERIFY(!table.headerIndex.contains("notes"));
}

void TestImportController::parseDuplicateResponseSuccessWithDuplicates()
{
    const QByteArray json = R"({
        "status": "success",
        "duplicates": ["2023-00123", "2023-00456"]
    })";

    QStringList duplicates;
    QString errorMsg;
    QVERIFY(ImportController::parseDuplicateResponse(json, &duplicates, &errorMsg));
    QCOMPARE(duplicates, QStringList({"2023-00123", "2023-00456"}));
}

void TestImportController::parseDuplicateResponseSuccessEmpty()
{
    const QByteArray json = R"({"status": "success", "duplicates": []})";

    QStringList duplicates;
    QString errorMsg;
    QVERIFY(ImportController::parseDuplicateResponse(json, &duplicates, &errorMsg));
    QVERIFY(duplicates.isEmpty());
}

void TestImportController::parseDuplicateResponseStatusNotSuccess()
{
    const QByteArray json = R"({"status": "error"})";

    QStringList duplicates;
    QString errorMsg;
    QVERIFY(!ImportController::parseDuplicateResponse(json, &duplicates, &errorMsg));
    QCOMPARE(errorMsg, QString("Duplicate check failed."));
}

void TestImportController::parseDuplicateResponseNotAnObject()
{
    QStringList duplicates;
    QString errorMsg;
    QVERIFY(!ImportController::parseDuplicateResponse(QByteArray("[1,2,3]"), &duplicates, &errorMsg));
    QCOMPARE(errorMsg, QString("Invalid duplicate check response."));

    errorMsg.clear();
    QVERIFY(!ImportController::parseDuplicateResponse(QByteArray("not json at all"), &duplicates, &errorMsg));
    QCOMPARE(errorMsg, QString("Invalid duplicate check response."));
}

void TestImportController::parseUploadResponseSuccess()
{
    const QByteArray json = R"({
        "status": "success",
        "message": "All good.",
        "success_count": 10,
        "error_count": 1
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(result.ok);
    QVERIFY(!result.plainText);
    QCOMPARE(result.message, QString("All good."));
    QCOMPARE(result.successCount, 10);
    QCOMPARE(result.errorCount, 1);
}

void TestImportController::parseUploadResponseStatusNotSuccess()
{
    const QByteArray json = R"({
        "status": "error",
        "message": "Some rows were invalid."
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(!result.ok);
    QVERIFY(!result.plainText);
    QCOMPARE(result.message, QString("Some rows were invalid."));
}

void TestImportController::parseUploadResponsePlainTextFallback()
{
    const QByteArray raw = "Upload finished (legacy plain-text handler).";

    const UploadResult result = ImportController::parseUploadResponse(raw);
    QVERIFY(result.plainText);
    QCOMPARE(result.rawText, QString::fromUtf8(raw));
}

void TestImportController::parseExcelRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("import_test.xlsx"));

    {
        QXlsx::Document doc;
        doc.write("A1", "School ID");
        doc.write("B1", "Full Name");
        doc.write("C1", "Course");
        doc.write("A2", "2023-00123");
        doc.write("B2", "Juan Dela Cruz");
        doc.write("C2", "BSIT");
        doc.write("A3", "2023-00456");
        doc.write("B3", "Maria Clara");
        doc.write("C3", "BSCS");
        QVERIFY(doc.saveAs(filePath));
    }

    ImportController controller(nullptr);   // parseExcel never touches the manager
    ExcelParseError err = ExcelParseError::OpenFailed;
    const ParsedTable table = controller.parseExcel(filePath, &err);

    QCOMPARE(err, ExcelParseError::None);
    QCOMPARE(table.headers, QStringList({"School ID", "Full Name", "Course"}));
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz", "BSIT"}));
    QCOMPARE(table.rows[1], QStringList({"2023-00456", "Maria Clara", "BSCS"}));
    QCOMPARE(table.headerIndex.value("school_id"), 0);
    QCOMPARE(table.headerIndex.value("name"),      1);
    QCOMPARE(table.headerIndex.value("course"),    2);
}

void TestImportController::parseExcelHeaderOnlyNoDataRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("import_header_only.xlsx"));

    {
        QXlsx::Document doc;
        doc.write("A1", "School ID");
        doc.write("B1", "Full Name");
        QVERIFY(doc.saveAs(filePath));
    }

    ImportController controller(nullptr);
    ExcelParseError err = ExcelParseError::OpenFailed;
    const ParsedTable table = controller.parseExcel(filePath, &err);

    QCOMPARE(err, ExcelParseError::None);
    QCOMPARE(table.headers, QStringList({"School ID", "Full Name"}));
    QVERIFY(table.rows.isEmpty());
}

void TestImportController::parseExcelOpenFailedOnBadPath()
{
    ImportController controller(nullptr);
    ExcelParseError err = ExcelParseError::None;
    const ParsedTable table = controller.parseExcel(
        QStringLiteral("nonexistent_path_does_not_exist.xlsx"), &err);

    QCOMPARE(err, ExcelParseError::OpenFailed);
    QVERIFY(table.headers.isEmpty());
    QVERIFY(table.rows.isEmpty());
}

QTEST_MAIN(TestImportController)
#include "tst_importcontroller.moc"
