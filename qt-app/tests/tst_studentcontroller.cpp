#include <QtTest>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
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

QTEST_MAIN(TestStudentController)
#include "tst_studentcontroller.moc"
