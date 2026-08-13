#include <QtTest>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrlQuery>
#include "capturingnam.h"
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

    // serializeRows
    void serializeRowsMapsSevenCoreKeys();
    void serializeRowsExcludesCodeAndVisits();
    void serializeRowsIgnoresUnrecognizedColumn();
    void serializeRowsTrimsValues();
    void serializeRowsShortRowFillsEmpty();
    void serializeRowsEmptyTableIsEmptyArray();

    // parseDuplicateResponse
    void parseDuplicateResponseSuccessWithDuplicates();
    void parseDuplicateResponseSuccessEmpty();
    void parseDuplicateResponseStatusNotSuccess();
    void parseDuplicateResponseNotAnObject();

    // parseUploadResponse
    void parseUploadResponseSuccess();
    void parseUploadResponseReadsSkippedCount();
    void parseUploadResponseStatusNotSuccess();
    void parseUploadResponsePlainTextFallback();

    // parseExcel
    void parseExcelRoundTrip();
    void parseExcelHeaderOnlyNoDataRows();
    void parseExcelOpenFailedOnBadPath();

    // checkDuplicates (request assembly + 401 routing)
    void checkDuplicatesPostsFormWithSchoolIdsAndAdminKey();
    void checkDuplicates401RoutesToAuthError();

    // uploadStudents (multipart assembly)
    void uploadStudentsPostsRowsAndAdminKeyMultipart();
    void uploadStudentsOmitsSkipIdsWhenEmpty();
    void uploadStudents401RoutesToUploadFailed();

    // validateForImport
    void validateForImportOkOnGoodTable();
    void validateForImportMissingSchoolIdColumn();
    void validateForImportMissingNameColumn();
    void validateForImportEmptySchoolIdRowsReported();
    void validateForImportIgnoresExtraColumns();

    // importTemplateCsv
    void importTemplateCsvHasHeadersAndExampleRow();
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

static QJsonArray decodeRows(const QByteArray &bytes)
{
    return QJsonDocument::fromJson(bytes).array();
}

void TestImportController::serializeRowsMapsSevenCoreKeys()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Course", "Year Level",
                 "Department", "Gender", "Status"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz", "BSIT", "1",
                          "CCS", "Male", "Active"};

    const QJsonArray arr = decodeRows(ImportController::serializeRows(t));
    QCOMPARE(arr.size(), 1);
    const QJsonObject o = arr.at(0).toObject();
    QCOMPARE(o.value("school_id").toString(),  QString("21-1-0001"));
    QCOMPARE(o.value("name").toString(),       QString("Juan Dela Cruz"));
    QCOMPARE(o.value("course").toString(),     QString("BSIT"));
    QCOMPARE(o.value("year_level").toString(), QString("1"));
    QCOMPARE(o.value("department").toString(), QString("CCS"));
    QCOMPARE(o.value("gender").toString(),     QString("Male"));
    QCOMPARE(o.value("status").toString(),     QString("Active"));
    // Exactly the 7 core keys — nothing else.
    QCOMPARE(o.keys().size(), 7);
}

void TestImportController::serializeRowsExcludesCodeAndVisits()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Code", "Visits"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // maps code + visits
    t.rows << QStringList{"21-1-0002", "Maria Clara", "RFID-9", "42"};

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QVERIFY(!o.contains("code"));
    QVERIFY(!o.contains("visits"));
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0002"));
    QCOMPARE(o.value("name").toString(),      QString("Maria Clara"));
    // Unmapped core columns still present, empty.
    QCOMPARE(o.value("course").toString(), QString());
}

void TestImportController::serializeRowsIgnoresUnrecognizedColumn()
{
    ParsedTable t;
    t.headers = {"School ID", "Notes"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // Notes -> col_1
    t.rows << QStringList{"21-1-0003", "ignore me"};

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QVERIFY(!o.contains("col_1"));
    QVERIFY(!o.contains("notes"));
    QCOMPARE(o.keys().size(), 7);
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0003"));
}

void TestImportController::serializeRowsTrimsValues()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"  21-1-0004  ", "\tAna Reyes \n"};

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0004"));
    QCOMPARE(o.value("name").toString(),      QString("Ana Reyes"));
}

void TestImportController::serializeRowsShortRowFillsEmpty()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Course"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // course -> index 2
    t.rows << QStringList{"21-1-0005", "Jose Rizal"};         // only 2 cells (ragged)

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0005"));
    QCOMPARE(o.value("name").toString(),      QString("Jose Rizal"));
    QCOMPARE(o.value("course").toString(),    QString());   // index 2 out of range -> ""
}

void TestImportController::serializeRowsEmptyTableIsEmptyArray()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    // no rows appended
    QCOMPARE(ImportController::serializeRows(t), QByteArray("[]"));
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
        "skipped_count": 2,
        "error_count": 1
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(result.ok);
    QVERIFY(!result.plainText);
    QCOMPARE(result.message, QString("All good."));
    QCOMPARE(result.successCount, 10);
    QCOMPARE(result.skippedCount, 2);
    QCOMPARE(result.errorCount, 1);
}

void TestImportController::parseUploadResponseReadsSkippedCount()
{
    // skipped_count present and non-zero is read verbatim (regression guard:
    // the field did not exist before 4a.3).
    const QByteArray json = R"({
        "status": "success",
        "message": "Partial import.",
        "success_count": 5,
        "skipped_count": 4,
        "error_count": 0
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(result.ok);
    QCOMPARE(result.successCount, 5);
    QCOMPARE(result.skippedCount, 4);
    QCOMPARE(result.errorCount, 0);
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

void TestImportController::checkDuplicatesPostsFormWithSchoolIdsAndAdminKey()
{
    CapturingNam nam(R"({"status":"success","duplicates":[]})");
    ImportController controller(&nam);

    controller.checkDuplicates({"21-1-0001", "21-1-0002"}, "s3cr3t-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QVERIFY(nam.lastUrl.toString().endsWith("check_duplicates.php"));
    QCOMPARE(nam.lastContentType, QString("application/x-www-form-urlencoded"));

    // school_ids is a JSON-array string field; admin_key is a sibling field.
    QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QVERIFY(q.hasQueryItem("school_ids"));
    QVERIFY(q.hasQueryItem("admin_key"));
    QCOMPARE(q.queryItemValue("admin_key", QUrl::FullyDecoded), QString("s3cr3t-key"));
    const QString idsField = q.queryItemValue("school_ids", QUrl::FullyDecoded);
    const QJsonArray ids = QJsonDocument::fromJson(idsField.toUtf8()).array();
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0).toString(), QString("21-1-0001"));
    QCOMPARE(ids.at(1).toString(), QString("21-1-0002"));
}

void TestImportController::checkDuplicates401RoutesToAuthError()
{
    // A guard rejection: AuthenticationRequiredError + HTTP 401 + a decodable body.
    CapturingNam nam(R"({"status":"error","message":"Admin authentication required"})",
                     QNetworkReply::AuthenticationRequiredError, 401);
    ImportController controller(&nam);

    QSignalSpy errSpy(&controller, &ImportController::importError);
    QSignalSpy okSpy(&controller, &ImportController::duplicatesResolved);

    controller.checkDuplicates({"21-1-0001"}, "bad-key");
    QVERIFY(errSpy.wait(1000));            // CannedReply finishes on the next loop turn
    QCOMPARE(okSpy.count(), 0);            // NOT reported as a normal resolve
    QCOMPARE(errSpy.count(), 1);
    const QString message = errSpy.at(0).at(1).toString();
    QVERIFY(message.contains("authentication", Qt::CaseInsensitive));
}

static ParsedTable oneRowTable()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz"};
    return t;
}

void TestImportController::uploadStudentsPostsRowsAndAdminKeyMultipart()
{
    CapturingNam nam(R"({"status":"success","success_count":1,"skipped_count":1,"error_count":0})");
    ImportController controller(&nam);
    QSignalSpy startedSpy(&controller, &ImportController::uploadStarted);

    controller.uploadStudents(oneRowTable(), QString(), {"21-1-0002"}, "s3cr3t-key");

    QCOMPARE(startedSpy.count(), 1);   // fires immediately before the POST
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QVERIFY(nam.lastUrl.toString().endsWith("upload_students_zip.php"));

    const QString body = QString::fromUtf8(nam.lastBody);
    QVERIFY(body.contains("name=\"rows\""));
    QVERIFY(body.contains("name=\"admin_key\""));
    QVERIFY(body.contains("name=\"skip_ids\""));   // skipIds non-empty -> present
    QVERIFY(body.contains("21-1-0001"));            // the serialized row payload
    QVERIFY(body.contains("s3cr3t-key"));
    QVERIFY(!body.contains("name=\"excel\""));      // raw Excel part removed
}

void TestImportController::uploadStudentsOmitsSkipIdsWhenEmpty()
{
    CapturingNam nam(R"({"status":"success","success_count":1,"skipped_count":0,"error_count":0})");
    ImportController controller(&nam);

    controller.uploadStudents(oneRowTable(), QString(), QStringList{}, "s3cr3t-key");

    const QString body = QString::fromUtf8(nam.lastBody);
    QVERIFY(body.contains("name=\"rows\""));
    QVERIFY(body.contains("name=\"admin_key\""));
    QVERIFY(!body.contains("name=\"skip_ids\""));   // empty -> absent
}

void TestImportController::uploadStudents401RoutesToUploadFailed()
{
    CapturingNam nam(R"({"status":"error","message":"Admin authentication required"})",
                     QNetworkReply::AuthenticationRequiredError, 401);
    ImportController controller(&nam);
    QSignalSpy failSpy(&controller, &ImportController::uploadFailed);
    QSignalSpy finSpy(&controller, &ImportController::uploadFinished);

    controller.uploadStudents(oneRowTable(), QString(), QStringList{}, "bad-key");
    QVERIFY(failSpy.wait(1000));
    QCOMPARE(finSpy.count(), 0);
    QVERIFY(failSpy.at(0).at(0).toString().contains("authentication", Qt::CaseInsensitive));
}

void TestImportController::validateForImportOkOnGoodTable()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz"};

    QStringList bad;
    QCOMPARE(ImportController::validateForImport(t, &bad), QString());   // "" == OK
    QVERIFY(bad.isEmpty());
}

void TestImportController::validateForImportMissingSchoolIdColumn()
{
    ParsedTable t;
    t.headers = {"Full Name", "Course"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // no school_id
    t.rows << QStringList{"Juan Dela Cruz", "BSIT"};

    const QString msg = ImportController::validateForImport(t, nullptr);
    QVERIFY(msg.contains("School ID"));
    QVERIFY(msg.contains("Found columns"));
}

void TestImportController::validateForImportMissingNameColumn()
{
    ParsedTable t;
    t.headers = {"School ID", "Course"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // no name
    t.rows << QStringList{"21-1-0001", "BSIT"};

    const QString msg = ImportController::validateForImport(t, nullptr);
    QVERIFY(msg.contains("Name"));
}

void TestImportController::validateForImportEmptySchoolIdRowsReported()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz"};
    t.rows << QStringList{"", "Maria Clara"};          // row 2: empty school_id

    QStringList bad;
    const QString msg = ImportController::validateForImport(t, &bad);
    QVERIFY(!msg.isEmpty());
    QCOMPARE(bad, QStringList({"Row 2"}));
}

void TestImportController::validateForImportIgnoresExtraColumns()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Notes"};   // Notes -> col_2, ignored
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz", "vip"};

    QStringList bad;
    QCOMPARE(ImportController::validateForImport(t, &bad), QString());   // extra col is fine
}

void TestImportController::importTemplateCsvHasHeadersAndExampleRow()
{
    const QString csv = QString::fromUtf8(ImportController::importTemplateCsv());
    const QStringList lines = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVERIFY(lines.size() >= 2);   // header + at least one example row

    const QString header = lines.first();
    QVERIFY(header.contains("School ID"));
    QVERIFY(header.contains("Name"));
    QVERIFY(header.contains("Course"));
    QVERIFY(header.contains("Department"));
    QVERIFY(header.contains("Year Level"));
    QVERIFY(header.contains("Gender"));
    QVERIFY(header.contains("Status"));

    // The example row uses an opaque hyphenated School ID (reinforces that it is
    // a School ID, not an admin key) and must NOT leak real PII.
    QVERIFY(lines.at(1).contains("21-1-0001"));
}

QTEST_MAIN(TestImportController)
#include "tst_importcontroller.moc"
