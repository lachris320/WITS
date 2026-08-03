#include <QtTest>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QUrlQuery>
#include "capturingnam.h"
#include "studentcontroller.h"
#include "studentdata.h"

class TestStudentController : public QObject
{
    Q_OBJECT
private slots:
    // normalizeFilter
    void normalizeFilterPlaceholdersBecomeEmpty();
    void normalizeFilterRealValuePassesThrough();

    // parseSearchResponse
    void parseSearchResults();
    void parseSearchEmptyIsEmptyOutcome();
    void parseSearchNotSuccessWithMessage();
    void parseSearchNotSuccessWithoutMessageDefaults();
    void parseSearchNonObjectIsInvalid();

    // parseSearchResponse — visits
    void parseSearchReadsVisits();
    void parseSearchDefaultsVisitsToZero();

    // parseBulkUpdateResponse
    void parseBulkSuccessWithErrors();
    void parseBulkSuccessNoErrors();
    void parseBulkNotSuccess();

    // parseDepartments
    void parseDepartmentsSkipsAllCaseInsensitive();
    void parseDepartmentsNotSuccessIsEmpty();

    // parseCourses
    void parseCoursesReturnsFullList();
    void parseCoursesNotSuccessIsEmpty();

    // parseDeleteResponse
    void parseDeleteResponse_success_returnsTrueEmptyMessage();
    void parseDeleteResponse_failure_returnsFalseWithMessage();
    void parseDeleteResponse_invalidJson_returnsFalseEmptyMessage();

    // request assembly (harmonized FORM + admin_key)
    void deleteStudents_buildsFormBodyWithAdminKey();
    void bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey();

    // deleteStudents — guard 401 must surface as deleteFinished(false, msg),
    // never deleteFailed (the whole point of Task 2's rewrite).
    void deleteStudents_guard401WithBody_emitsDeleteFinishedNotFailed();

    // deleteReplyIsServerAnswer (mirrors HttpForm::isServerAnswer)
    void deleteReplyIsServerAnswer_transportNoStatus_isFalse();
    void deleteReplyIsServerAnswer_401WithBody_isTrue();
    void deleteReplyIsServerAnswer_200Error_isTrue();
    void deleteReplyIsServerAnswer_200Success_isTrue();
    void deleteReplyIsServerAnswer_statusButEmptyBody_isFalse();

    // toCsv
    void toCsv_emptyList_headerOnlyWithBom();
    void toCsv_headerRowIsExactFieldOrder();
    void toCsv_serializesAllFieldsExceptPhoto();
    void toCsv_quotesCommaEmbeddedField();
    void toCsv_doublesEmbeddedQuote();
    void toCsv_quotesEmbeddedNewline();
    void toCsv_usesCrlfLineTerminators();

    // toCsv — CSV formula-injection neutralization (OWASP CSV injection)
    void toCsv_prefixesApostropheWhenFieldStartsWithEquals();
    void toCsv_prefixesApostropheWhenFieldStartsWithPlus();
    void toCsv_prefixesApostropheWhenFieldStartsWithMinus();
    void toCsv_prefixesApostropheWhenFieldStartsWithAt();
    void toCsv_normalNameIsUnchanged();
    void toCsv_formulaFieldWithCommaIsPrefixedAndQuoted();
};

void TestStudentController::normalizeFilterPlaceholdersBecomeEmpty()
{
    QCOMPARE(StudentController::normalizeFilter("All"), QString(""));
    QCOMPARE(StudentController::normalizeFilter("All Departments"), QString(""));
    QCOMPARE(StudentController::normalizeFilter("Select Department"), QString(""));
    QCOMPARE(StudentController::normalizeFilter("All Courses"), QString(""));
    QCOMPARE(StudentController::normalizeFilter("Select Course"), QString(""));
}

void TestStudentController::normalizeFilterRealValuePassesThrough()
{
    QCOMPARE(StudentController::normalizeFilter("College of Computing Studies"),
             QString("College of Computing Studies"));
    QCOMPARE(StudentController::normalizeFilter("BSIT"), QString("BSIT"));
}

void TestStudentController::parseSearchResults()
{
    const QByteArray raw = R"({
        "status":"success",
        "searchTerm":"cruz",
        "students":[
            {"code":"C1","school_id":"2023-00123","name":"Juan Dela Cruz",
             "course":"BSIT","department":"College of Computing Studies",
             "year_level":"3","gender":"Male","status":"Active"},
            {"code":"C2","school_id":"2023-00124","name":"Maria Santos",
             "course":"BSCS","department":"College of Computing Studies",
             "year_level":"2","gender":"Female","status":"Active"}
        ]
    })";
    QList<StudentRecord> records;
    QString message, searchTerm;
    const SearchOutcome outcome =
        StudentController::parseSearchResponse(raw, records, message, searchTerm);

    QCOMPARE(outcome, SearchOutcome::Results);
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0).code, QString("C1"));
    QCOMPARE(records.at(0).schoolId, QString("2023-00123"));
    QCOMPARE(records.at(0).name, QString("Juan Dela Cruz"));
    QCOMPARE(records.at(0).course, QString("BSIT"));
    QCOMPARE(records.at(0).department, QString("College of Computing Studies"));
    QCOMPARE(records.at(0).yearLevel, QString("3"));
    QCOMPARE(records.at(0).gender, QString("Male"));
    QCOMPARE(records.at(0).status, QString("Active"));
    QCOMPARE(records.at(1).schoolId, QString("2023-00124"));
    QCOMPARE(searchTerm, QString("cruz"));
}

void TestStudentController::parseSearchEmptyIsEmptyOutcome()
{
    const QByteArray raw = R"({"status":"success","students":[]})";
    QList<StudentRecord> records;
    QString message, searchTerm;
    QCOMPARE(StudentController::parseSearchResponse(raw, records, message, searchTerm),
             SearchOutcome::Empty);
    QVERIFY(records.isEmpty());
}

void TestStudentController::parseSearchNotSuccessWithMessage()
{
    const QByteArray raw = R"({"status":"error","message":"Bad filter"})";
    QList<StudentRecord> records;
    QString message, searchTerm;
    QCOMPARE(StudentController::parseSearchResponse(raw, records, message, searchTerm),
             SearchOutcome::NotSuccess);
    QCOMPARE(message, QString("Bad filter"));
}

void TestStudentController::parseSearchNotSuccessWithoutMessageDefaults()
{
    const QByteArray raw = R"({"status":"error"})";
    QList<StudentRecord> records;
    QString message, searchTerm;
    QCOMPARE(StudentController::parseSearchResponse(raw, records, message, searchTerm),
             SearchOutcome::NotSuccess);
    QCOMPARE(message, QString("No students found."));
}

void TestStudentController::parseSearchNonObjectIsInvalid()
{
    QList<StudentRecord> records;
    QString message, searchTerm;
    QCOMPARE(StudentController::parseSearchResponse("[1,2,3]", records, message, searchTerm),
             SearchOutcome::InvalidResponse);
    QCOMPARE(StudentController::parseSearchResponse("not json", records, message, searchTerm),
             SearchOutcome::InvalidResponse);
}

void TestStudentController::parseBulkSuccessWithErrors()
{
    const QByteArray raw = R"({
        "status":"success","updated":3,
        "errors":["Row 2 failed","Row 5 failed"]
    })";
    const BulkUpdateResult r = StudentController::parseBulkUpdateResponse(raw);
    QVERIFY(r.ok);
    QCOMPARE(r.updatedCount, 3);
    QCOMPARE(r.errors.size(), 2);
    QCOMPARE(r.errors.at(0), QString("Row 2 failed"));
    QCOMPARE(r.errors.at(1), QString("Row 5 failed"));
}

void TestStudentController::parseBulkSuccessNoErrors()
{
    const QByteArray raw = R"({"status":"success","updated":5})";
    const BulkUpdateResult r = StudentController::parseBulkUpdateResponse(raw);
    QVERIFY(r.ok);
    QCOMPARE(r.updatedCount, 5);
    QVERIFY(r.errors.isEmpty());
}

void TestStudentController::parseBulkNotSuccess()
{
    const QByteArray raw = R"({"status":"error","message":"Nothing updated"})";
    const BulkUpdateResult r = StudentController::parseBulkUpdateResponse(raw);
    QVERIFY(!r.ok);
    QCOMPARE(r.updatedCount, 0);
    QCOMPARE(r.message, QString("Nothing updated"));
}

void TestStudentController::parseDepartmentsSkipsAllCaseInsensitive()
{
    const QByteArray raw = R"({
        "status":"success",
        "departments":["all","All","College of Computing Studies","College of Engineering"]
    })";
    const QStringList depts = StudentController::parseDepartments(raw);
    QCOMPARE(depts.size(), 2);
    QCOMPARE(depts.at(0), QString("College of Computing Studies"));
    QCOMPARE(depts.at(1), QString("College of Engineering"));
}

void TestStudentController::parseDepartmentsNotSuccessIsEmpty()
{
    QVERIFY(StudentController::parseDepartments(R"({"status":"error"})").isEmpty());
}

void TestStudentController::parseCoursesReturnsFullList()
{
    const QByteArray raw = R"({"status":"success","courses":["BSIT","BSCS","BSIS"]})";
    const QStringList courses = StudentController::parseCourses(raw);
    QCOMPARE(courses.size(), 3);
    QCOMPARE(courses.at(0), QString("BSIT"));
    QCOMPARE(courses.at(2), QString("BSIS"));
}

void TestStudentController::parseCoursesNotSuccessIsEmpty()
{
    QVERIFY(StudentController::parseCourses(R"({"status":"error"})").isEmpty());
}

void TestStudentController::parseDeleteResponse_success_returnsTrueEmptyMessage()
{
    QString msg = "sentinel";
    const bool ok = StudentController::parseDeleteResponse(
        R"({"status":"success"})", msg);
    QVERIFY(ok);
    QVERIFY(msg.isEmpty());
}

void TestStudentController::parseDeleteResponse_failure_returnsFalseWithMessage()
{
    QString msg;
    const bool ok = StudentController::parseDeleteResponse(
        R"({"status":"error","message":"Student not found"})", msg);
    QVERIFY(!ok);
    QCOMPARE(msg, QStringLiteral("Student not found"));
}

void TestStudentController::parseDeleteResponse_invalidJson_returnsFalseEmptyMessage()
{
    QString msg;
    const bool ok = StudentController::parseDeleteResponse("not json", msg);
    QVERIFY(!ok);
    QVERIFY(msg.isEmpty());
}

void TestStudentController::parseSearchReadsVisits()
{
    const QByteArray raw = R"({
        "status":"success",
        "students":[{"school_id":"2023-0001","name":"Maria Santos","course":"BSCE",
                     "department":"CE","year_level":"3rd Year","gender":"Female",
                     "status":"Regular","visits":42}],
        "searchTerm":"Maria"
    })";
    QList<StudentRecord> recs; QString msg, term;
    const SearchOutcome outcome = StudentController::parseSearchResponse(raw, recs, msg, term);
    QCOMPARE(outcome, SearchOutcome::Results);
    QCOMPARE(recs.size(), 1);
    QCOMPARE(recs.at(0).visits, 42);
}

void TestStudentController::parseSearchDefaultsVisitsToZero()
{
    const QByteArray raw = R"({
        "status":"success",
        "students":[{"school_id":"2023-0002","name":"Jose Ramirez","course":"BSEE",
                     "department":"EE","year_level":"2nd Year","gender":"Male",
                     "status":"Regular"}],
        "searchTerm":"Jose"
    })";
    QList<StudentRecord> recs; QString msg, term;
    StudentController::parseSearchResponse(raw, recs, msg, term);
    QCOMPARE(recs.size(), 1);
    QCOMPARE(recs.at(0).visits, 0);   // field absent -> QJsonValue::toInt() default
}

void TestStudentController::deleteStudents_buildsFormBodyWithAdminKey()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    ctrl.deleteStudents(QStringList() << "2023-001" << "2023-002", "test-key");

    // Form-encoded POST (not JSON), carrying admin_key + repeated school_ids[].
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));

    const QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QCOMPARE(q.queryItemValue("admin_key"), QStringLiteral("test-key"));
    const QStringList ids = q.allQueryItemValues("school_ids[]", QUrl::FullyDecoded);
    QCOMPARE(ids.size(), 2);
    QVERIFY(ids.contains("2023-001"));
    QVERIFY(ids.contains("2023-002"));
    QVERIFY(!nam.lastBody.contains("{"));   // not a JSON body
}

void TestStudentController::bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    StudentRecord r;
    r.schoolId = "2023-001"; r.code = "C1"; r.name = "Juan Cruz";
    r.department = "CCS"; r.course = "BSIT"; r.yearLevel = "2";
    r.gender = "Male"; r.status = "Active";
    ctrl.bulkUpdateStudents(QList<StudentRecord>() << r, "test-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));

    const QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QCOMPARE(q.queryItemValue("admin_key"), QStringLiteral("test-key"));

    // students is a JSON string in one field; decode it back and check a field.
    const QString studentsJson = q.queryItemValue("students", QUrl::FullyDecoded);
    const QJsonArray arr = QJsonDocument::fromJson(studentsJson.toUtf8()).array();
    QCOMPARE(arr.size(), 1);
    QCOMPARE(arr.at(0).toObject().value("school_id").toString(), QStringLiteral("2023-001"));
    QCOMPARE(arr.at(0).toObject().value("year_level").toString(), QStringLiteral("2"));
}

void TestStudentController::deleteStudents_guard401WithBody_emitsDeleteFinishedNotFailed()
{
    // requireAdminAuth answers a stale/bad admin key with HTTP 401 + a JSON
    // error body. Before the Task 2 rewrite, StudentController::deleteStudents
    // treated ANY reply->error() != NoError as a transport failure and emitted
    // deleteFailed with the generic Qt errorString, never reaching
    // parseDeleteResponse. That mis-surfaces a stale admin key as a network
    // problem. This test pins the fix: the 401 body must reach deleteFinished
    // with ok==false and the server's own message, and deleteFailed must NOT
    // fire at all. It fails against the old early-return implementation.
    const QByteArray body = R"({"status":"error","message":"Invalid admin key"})";
    CapturingNam nam(body, QNetworkReply::AuthenticationRequiredError, 401);
    StudentController ctrl(&nam);

    QSignalSpy finishedSpy(&ctrl, &StudentController::deleteFinished);
    QSignalSpy failedSpy(&ctrl, &StudentController::deleteFailed);

    ctrl.deleteStudents(QStringList() << "2023-001", "stale-key");

    QVERIFY(finishedSpy.wait(1000));

    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    const QList<QVariant> args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).toBool(), false);
    QCOMPARE(args.at(1).toInt(), 1);
    QCOMPARE(args.at(2).toString(), QStringLiteral("Invalid admin key"));
}

void TestStudentController::deleteReplyIsServerAnswer_transportNoStatus_isFalse()
{
    // hadError=true, no HTTP status (attribute absent => 0): a real transport
    // failure — DNS/refused/timeout.
    QVERIFY(!StudentController::deleteReplyIsServerAnswer(true, 0, QByteArray()));
}

void TestStudentController::deleteReplyIsServerAnswer_401WithBody_isTrue()
{
    // requireAdminAuth answers a bad/expired key with 401 + a JSON body —
    // that IS the server answering and must reach parseDeleteResponse.
    const QByteArray body = R"({"status":"error","message":"Invalid admin key"})";
    QVERIFY(StudentController::deleteReplyIsServerAnswer(true, 401, body));
}

void TestStudentController::deleteReplyIsServerAnswer_200Error_isTrue()
{
    // No transport error and a normal 200 status, but the JSON payload itself
    // reports a server-side failure — still a decodable server answer.
    const QByteArray body = R"({"status":"error","message":"Invalid data"})";
    QVERIFY(StudentController::deleteReplyIsServerAnswer(false, 200, body));
}

void TestStudentController::deleteReplyIsServerAnswer_200Success_isTrue()
{
    QVERIFY(StudentController::deleteReplyIsServerAnswer(false, 200,
                                                         R"({"status":"success"})"));
}

void TestStudentController::deleteReplyIsServerAnswer_statusButEmptyBody_isFalse()
{
    // A status code with an empty body gives the decode seam nothing to
    // classify — treated as a transport failure, exactly like HttpForm.
    QVERIFY(!StudentController::deleteReplyIsServerAnswer(true, 500, QByteArray()));
}

static const QByteArray kBom = QByteArrayLiteral("\xEF\xBB\xBF");

void TestStudentController::toCsv_emptyList_headerOnlyWithBom()
{
    const QByteArray csv = StudentController::toCsv({});
    QVERIFY(csv.startsWith(kBom));
    QCOMPARE(csv, kBom + "code,school_id,name,course,department,year_level,gender,status,visits\r\n");
}

void TestStudentController::toCsv_headerRowIsExactFieldOrder()
{
    const QByteArray csv = StudentController::toCsv({});
    const QByteArray body = csv.mid(kBom.size());
    const QByteArray header = body.left(body.indexOf("\r\n"));
    QCOMPARE(header, QByteArrayLiteral(
        "code,school_id,name,course,department,year_level,gender,status,visits"));
}

void TestStudentController::toCsv_serializesAllFieldsExceptPhoto()
{
    StudentRecord r;
    r.code = "C1"; r.schoolId = "2023-001"; r.name = "Juan Cruz"; r.course = "BSIT";
    r.department = "CCS"; r.yearLevel = "2"; r.gender = "Male"; r.status = "Active";
    r.visits = 42;
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("C1,2023-001,Juan Cruz,BSIT,CCS,2,Male,Active,42\r\n"));
}

void TestStudentController::toCsv_quotesCommaEmbeddedField()
{
    StudentRecord r; r.name = "Cruz, Juan";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"Cruz, Juan\""));
}

void TestStudentController::toCsv_doublesEmbeddedQuote()
{
    StudentRecord r; r.name = "Juan \"JC\" Cruz";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"Juan \"\"JC\"\" Cruz\""));
}

void TestStudentController::toCsv_quotesEmbeddedNewline()
{
    StudentRecord r; r.name = "Line1\nLine2";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"Line1\nLine2\""));
}

void TestStudentController::toCsv_usesCrlfLineTerminators()
{
    StudentRecord r; r.name = "Ann";
    const QByteArray csv = StudentController::toCsv({r});
    // Header line + one data line, both CRLF-terminated => exactly two "\r\n".
    QCOMPARE(csv.count("\r\n"), 2);
    QVERIFY(!csv.contains("\n\n"));
}

// OWASP CSV-injection mitigation: a leading =, +, -, or @ (or a leading tab/CR)
// makes Excel/LibreOffice interpret the cell as a formula. Prefixing a single
// apostrophe forces text interpretation without altering the visible value.
void TestStudentController::toCsv_prefixesApostropheWhenFieldStartsWithEquals()
{
    StudentRecord r; r.name = "=HYPERLINK(\"http://x\")";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("'=HYPERLINK(\"\"http://x\"\")"));
}

void TestStudentController::toCsv_prefixesApostropheWhenFieldStartsWithPlus()
{
    StudentRecord r; r.name = "+1234";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains(",'+1234,"));
}

void TestStudentController::toCsv_prefixesApostropheWhenFieldStartsWithMinus()
{
    StudentRecord r; r.name = "-1234";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains(",'-1234,"));
}

void TestStudentController::toCsv_prefixesApostropheWhenFieldStartsWithAt()
{
    StudentRecord r; r.name = "@SUM(A1)";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains(",'@SUM(A1),"));
}

void TestStudentController::toCsv_normalNameIsUnchanged()
{
    StudentRecord r; r.name = "Juan Cruz";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains(",Juan Cruz,"));
    QVERIFY(!csv.contains("'Juan Cruz"));
}

void TestStudentController::toCsv_formulaFieldWithCommaIsPrefixedAndQuoted()
{
    StudentRecord r; r.name = "=A,B";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"'=A,B\""));
}

QTEST_MAIN(TestStudentController)
#include "tst_studentcontroller.moc"
