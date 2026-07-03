# StudentController Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract the Student-Management Search-tab network/parse logic out of the `adminWindow` god-class into a new `StudentController` (QObject + value types + pure static parsers + async network methods), leaving behavior byte-identical except for one deliberate, annotated deviation (collapsing a duplicated `get_courses.php` double-fetch into a single request).

**Architecture:** Mirror the existing `ImportController` / `VisitorController` pattern. A new `StudentController` takes `adminWindow`'s existing `QNetworkAccessManager*` by injection (does **not** own it), exposes pure static parsers for unit testing, and performs four async operations (`searchStudents`, `bulkUpdateStudents`, `loadDepartments`, `loadCourses`) whose outcomes arrive as signals. `adminWindow` keeps every widget, dialog, overlay string, table-population routine, and the edit-mode state machine — it becomes a pure View that connects controller signals to its slots. Dead Student-tab code (`deleteStudents`, `onDeleteStudentBtnClicked`, `loadAllStudents`, `showSearchOverlay`, `hideSearchOverlay`, and their orphaned members) is deleted, not extracted.

**Tech Stack:** Qt 6.11 (Widgets, Network, Test), C++17, CMake + Ninja (MinGW), ctest.

## Global Constraints

- **Source spec:** `docs/superpowers/specs/2026-07-03-student-controller-design.md` — this plan implements it; consult it for any detail not repeated here.
- **Branch:** `feat/student-controller-extraction` (already checked out, off `master`).
- **Header guards:** `#ifndef`/`#define`/`#endif` only — never `#pragma once`. Guards: `STUDENTDATA_H`, `STUDENTCONTROLLER_H`.
- **Member naming:** controller members prefixed `m_` (e.g. `m_studentController`, `m_nam`).
- **Signal/slot connects:** functor syntax with an explicit context object (`connect(obj, &T::sig, this, &U::slot)`); never `SIGNAL()`/`SLOT()` string macros.
- **Injected NAM, not owned:** `StudentController` stores `QNetworkAccessManager *m_nam` and never deletes it; `adminWindow` retains ownership.
- **`reply->deleteLater()` on every code path** (success, network error, parse error) in every network method.
- **Reply context object:** every `connect(reply, &QNetworkReply::finished, this, …)` passes `this` (the controller) as the 3rd-arg context object so the connection auto-disconnects if the controller dies while a reply is in flight.
- **Purity:** `studentcontroller.cpp`/`.h` contain **no** `ui->`, `QMessageBox`, `QFileDialog`, `QTableWidget`, or `showTemporaryOverlay` reference.
- **No wire-protocol change:** endpoints (`search_students.php`, `bulk_update_students.php`, `get_departments.php`, `get_courses.php`), payload shapes, response field names, and every overlay/dialog string (emojis verbatim) are unchanged.
- **Security-hygiene:** test fixtures use only synthetic data (e.g. `"2023-00123"`, `"Juan Dela Cruz"`, `"BSIT"`); no real student PII, no backend URLs, no machine-local paths.
- **`adminwindow.ui` is untouched** — it carries the user's own uncommitted edit. Never stage or modify it. `deleteStudentBtn` stays present-but-unwired.
- **Commits:** always via the `commit` skill (never raw `git add`/`git commit`). The step-level `git` snippets below describe the *intended* grouping; the actual commit is driven through the skill.
- **Build/test commands** (tools are not on PATH — use the configured Qt kit):
  - Configure: `cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH=<Qt6-MinGW-dir>`
  - Build: `cmake --build qt-app/build`
  - Test: `ctest --test-dir qt-app/build --output-on-failure`

---

## File Structure

| File | Status | Responsibility |
|------|--------|----------------|
| `qt-app/studentdata.h` | **Create** | Plain value types: `StudentRecord`, `BulkUpdateResult`, `SearchOutcome`. No logic. |
| `qt-app/studentcontroller.h` | **Create** | `StudentController` QObject declaration: ctor, 5 pure statics, 4 async methods, 6 signals, `m_nam`. |
| `qt-app/studentcontroller.cpp` | **Create** | Implementation of the 5 statics and 4 network methods. |
| `qt-app/tests/tst_studentcontroller.cpp` | **Create** | Qt Test target covering the 5 pure statics (Core+Network link, no offscreen platform). |
| `qt-app/CMakeLists.txt` | **Modify** | Add `studentdata.h`, `studentcontroller.{h,cpp}` to the `WITS` source list (after the import lines). |
| `qt-app/tests/CMakeLists.txt` | **Modify** | Register the `tst_studentcontroller` ctest target (no QXlsx, no `QT_QPA_PLATFORM=offscreen`). |
| `qt-app/adminwindow.h` | **Modify** | Add `m_studentController`, `m_studentSearchShowOverlay`, 6 slots; delete 5 dead decls + 2 dead members. |
| `qt-app/adminwindow.cpp` | **Modify** | Construct+wire controller; rewrite 7 functions to delegate; collapse the double course-fetch; delete 5 dead functions + 2 ctor `nullptr` lines. |

**Task boundaries:**
- **Task 1** — value types + static parsers (real) + network methods (stubbed) + both CMake edits + the red→green test target. Independently testable: `ctest -R tst_studentcontroller` goes green on the statics.
- **Task 2** — implement the 4 network methods (signals + reply lifetime). Independently testable: builds clean; statics tests still green (network methods have no automated test per the accepted limitation).
- **Task 3** — wire the controller into `adminWindow`, rewrite the 7 View functions, collapse the double-fetch, delete the dead code. Independently testable: full build + all 8 ctest targets green + manual Search-tab QA.

---

## Task 1: Value types, static parsers, test target

**Files:**
- Create: `qt-app/studentdata.h`
- Create: `qt-app/studentcontroller.h`
- Create: `qt-app/studentcontroller.cpp`
- Create: `qt-app/tests/tst_studentcontroller.cpp`
- Modify: `qt-app/CMakeLists.txt` (after line 44)
- Modify: `qt-app/tests/CMakeLists.txt` (append after line 103)

**Interfaces:**
- Consumes: nothing (first task).
- Produces (Task 2 and Task 3 rely on these exact names/types):
  - `struct StudentRecord { QString code, schoolId, name, course, department, yearLevel, gender, status; };`
  - `struct BulkUpdateResult { bool ok = false; int updatedCount = 0; QStringList errors; QString message; };`
  - `enum class SearchOutcome { Results, Empty, NotSuccess, InvalidResponse, NetworkError };`
  - `static QString StudentController::normalizeFilter(const QString &comboText);`
  - `static SearchOutcome StudentController::parseSearchResponse(const QByteArray &raw, QList<StudentRecord> &outRecords, QString &outMessage, QString &outSearchTerm);`
  - `static BulkUpdateResult StudentController::parseBulkUpdateResponse(const QByteArray &raw);`
  - `static QStringList StudentController::parseDepartments(const QByteArray &raw);`
  - `static QStringList StudentController::parseCourses(const QByteArray &raw);`
  - `explicit StudentController::StudentController(QNetworkAccessManager *nam, QObject *parent = nullptr);`
  - Async method + signal decls (bodies stubbed this task): `searchStudents`, `bulkUpdateStudents`, `loadDepartments`, `loadCourses`; signals `searchFinished`, `searchFailed`, `bulkUpdateFinished`, `bulkUpdateFailed`, `departmentsLoaded`, `coursesLoaded`.

- [ ] **Step 1: Create `qt-app/studentdata.h`**

```cpp
#ifndef STUDENTDATA_H
#define STUDENTDATA_H

#include <QString>
#include <QStringList>

// One search-result row. Field names track the JSON keys the backend
// returns/consumes: code, school_id, name, course, department, year_level,
// gender, status. A single type serves both the search-result direction
// (fill the table) and the bulk-update direction (serialize back).
struct StudentRecord
{
    QString code;
    QString schoolId;
    QString name;
    QString course;
    QString department;
    QString yearLevel;
    QString gender;
    QString status;
};

// Decoded bulk_update_students.php response.
struct BulkUpdateResult
{
    bool        ok = false;
    int         updatedCount = 0;
    QStringList errors;
    QString     message;
};

// The five distinct branches of performStudentSearch's reply handler, so the
// View can show the exact original overlay for each.
enum class SearchOutcome
{
    Results,          // status success, students non-empty
    Empty,            // status success, students empty
    NotSuccess,       // obj["status"] != "success"
    InvalidResponse,  // body is not a JSON object
    NetworkError      // reply->error() != NoError (routed via searchFailed, not searchFinished)
};

#endif // STUDENTDATA_H
```

- [ ] **Step 2: Create `qt-app/studentcontroller.h`**

```cpp
#ifndef STUDENTCONTROLLER_H
#define STUDENTCONTROLLER_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include "studentdata.h"

class QNetworkAccessManager;

class StudentController : public QObject
{
    Q_OBJECT
public:
    explicit StudentController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeFilter(const QString &comboText);
    static SearchOutcome parseSearchResponse(const QByteArray &raw,
                                             QList<StudentRecord> &outRecords,
                                             QString &outMessage,
                                             QString &outSearchTerm);
    static BulkUpdateResult parseBulkUpdateResponse(const QByteArray &raw);
    static QStringList parseDepartments(const QByteArray &raw);
    static QStringList parseCourses(const QByteArray &raw);

    // Async — result arrives via searchFinished / searchFailed.
    void searchStudents(const QString &search,
                        const QString &department,
                        const QString &course);

    // Async — result arrives via bulkUpdateFinished / bulkUpdateFailed.
    void bulkUpdateStudents(const QList<StudentRecord> &updates);

    // Async — result arrives via departmentsLoaded (empty on error/!success).
    void loadDepartments();

    // Async — result arrives via coursesLoaded (empty on error/!success).
    void loadCourses(const QString &department);

signals:
    void searchFinished(SearchOutcome outcome,
                        const QList<StudentRecord> &records,
                        const QString &message,
                        const QString &searchTerm);
    void searchFailed(const QString &errorString);
    void bulkUpdateFinished(const BulkUpdateResult &result);
    void bulkUpdateFailed(const QString &errorString);
    void departmentsLoaded(const QStringList &departments);
    void coursesLoaded(const QStringList &courses);

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // STUDENTCONTROLLER_H
```

- [ ] **Step 3: Create `qt-app/studentcontroller.cpp` with real statics + stubbed network methods**

```cpp
#include "studentcontroller.h"
#include "apiconfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

StudentController::StudentController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

// Reproduces the placeholder->"" normalization at adminwindow.cpp:3171-3172.
// The union of both legacy predicate lists; no real value equals a placeholder,
// so applying this to department reproduces 3171 and to course reproduces 3172.
QString StudentController::normalizeFilter(const QString &comboText)
{
    if (comboText == "All" || comboText == "All Departments"
        || comboText == "Select Department" || comboText == "All Courses"
        || comboText == "Select Course")
        return QString();
    return comboText;
}

// Pure port of the reply body of performStudentSearch (adminwindow.cpp:3194-3242),
// minus the overlay/table side effects (which stay View-side).
SearchOutcome StudentController::parseSearchResponse(const QByteArray &raw,
                                                     QList<StudentRecord> &outRecords,
                                                     QString &outMessage,
                                                     QString &outSearchTerm)
{
    outRecords.clear();
    outMessage.clear();
    outSearchTerm.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return SearchOutcome::InvalidResponse;         // line 3195

    const QJsonObject obj = doc.object();

    if (obj["status"].toString() != "success") {       // line 3206
        outMessage = obj.contains("message") ? obj["message"].toString()
                                             : QStringLiteral("No students found.");  // line 3207
        return SearchOutcome::NotSuccess;
    }

    const QJsonArray students = obj["students"].toArray();
    if (students.isEmpty())                            // line 3224
        return SearchOutcome::Empty;

    for (const QJsonValue &val : students) {
        const QJsonObject s = val.toObject();
        StudentRecord rec;
        rec.code       = s["code"].toString();
        rec.schoolId   = s["school_id"].toString();
        rec.name       = s["name"].toString();
        rec.course     = s["course"].toString();
        rec.department = s["department"].toString();
        rec.yearLevel  = s["year_level"].toString();
        rec.gender     = s["gender"].toString();
        rec.status     = s["status"].toString();
        outRecords.append(rec);
    }
    outSearchTerm = obj["searchTerm"].toString();      // line 3242
    return SearchOutcome::Results;
}

// Pure port of the reply body of bulkUpdateStudents (adminwindow.cpp:3017-3046).
BulkUpdateResult StudentController::parseBulkUpdateResponse(const QByteArray &raw)
{
    BulkUpdateResult result;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();

    if (obj["status"].toString() == "success") {
        result.ok = true;
        result.updatedCount = obj["updated"].toInt();          // line 3021
        if (obj.contains("errors") && !obj["errors"].toArray().isEmpty()) {  // line 3030
            const QJsonArray errs = obj["errors"].toArray();
            for (const QJsonValue &e : errs)
                result.errors << e.toString();
        }
    } else {
        result.message = obj["message"].toString();            // line 3045
    }
    return result;
}

// Pure port of the departments parse in populateFilters (adminwindow.cpp:3375-3382).
// Returns only the parsed entries (View prepends "Select Department"); skips
// "all" case-insensitively; empty on !success.
QStringList StudentController::parseDepartments(const QByteArray &raw)
{
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["departments"].toArray();
        for (const QJsonValue &val : arr) {
            const QString dept = val.toString();
            if (dept.toLower() != "all")               // line 3379
                out << dept;
        }
    }
    return out;
}

// Pure port of the courses parse in onDepartmentFilterChanged (adminwindow.cpp:3497-3503).
// Returns only the parsed entries (View prepends "Select Course"); empty on !success.
QStringList StudentController::parseCourses(const QByteArray &raw)
{
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["courses"].toArray();
        for (const QJsonValue &val : arr)
            out << val.toString();
    }
    return out;
}

// --- Network methods: stubbed this task, implemented in Task 2 ---

void StudentController::searchStudents(const QString &, const QString &, const QString &)
{
    // Implemented in Task 2.
}

void StudentController::bulkUpdateStudents(const QList<StudentRecord> &)
{
    // Implemented in Task 2.
}

void StudentController::loadDepartments()
{
    // Implemented in Task 2.
}

void StudentController::loadCourses(const QString &)
{
    // Implemented in Task 2.
}
```

- [ ] **Step 4: Create `qt-app/tests/tst_studentcontroller.cpp` (the failing test)**

```cpp
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

QTEST_MAIN(TestStudentController)
#include "tst_studentcontroller.moc"
```

- [ ] **Step 5: Add the controller sources to `qt-app/CMakeLists.txt`**

After line 44 (`importcontroller.h importcontroller.cpp`), inside the `qt_add_executable(WITS …)` source list, add:

```cmake
        studentdata.h
        studentcontroller.h studentcontroller.cpp
```

No new Qt modules — `Qt::Network` is already linked to `WITS`.

- [ ] **Step 6: Register the test target in `qt-app/tests/CMakeLists.txt`**

Append after line 103 (the end of the `tst_importcontroller` block). Note: **no `QXlsx`, no `QT_QPA_PLATFORM=offscreen`** — this target links Core+Network only, so `QTEST_MAIN` builds a `QCoreApplication`, which needs no platform plugin (matches `tst_apiconfig`).

```cmake

qt_add_executable(tst_studentcontroller
    tst_studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/studentcontroller.h
    ${CMAKE_SOURCE_DIR}/studentdata.h
)
set_target_properties(tst_studentcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_studentcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_studentcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
)
add_test(NAME tst_studentcontroller COMMAND tst_studentcontroller)
set_tests_properties(tst_studentcontroller PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
```

- [ ] **Step 7: Configure + build the test target, verify it builds and passes**

Run:
```bash
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH=<Qt6-MinGW-dir>
cmake --build qt-app/build --target tst_studentcontroller
ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure
```
Expected: build succeeds; **all 14 test slots PASS** (the statics are fully implemented this task, so this is green immediately — the "red" was the pre-implementation state where `studentcontroller.cpp` did not exist and the target failed to compile/link).

> **TDD note:** because Task 1 deliberately ships the real static implementations alongside their tests (the statics are the unit under test and are cheap/pure), the meaningful red state is "target does not compile because `studentcontroller.*` is absent." If you want a stricter red, momentarily stub one static to return a wrong value, watch that slot fail, then restore — optional.

- [ ] **Step 8: Verify the full build still links (WITS picks up the new sources but does not yet use them)**

Run:
```bash
cmake --build qt-app/build
```
Expected: `WITS` and all test targets build clean, no new warnings. `StudentController` is compiled into `WITS` but not yet referenced — that is fine.

- [ ] **Step 9: Commit (via the `commit` skill)**

Intended grouping — one commit:
```bash
git add qt-app/studentdata.h qt-app/studentcontroller.h qt-app/studentcontroller.cpp \
        qt-app/tests/tst_studentcontroller.cpp qt-app/CMakeLists.txt qt-app/tests/CMakeLists.txt
# commit message (feat): add StudentController value types + pure parsers + test target
```

---

## Task 2: Implement the four network methods

**Files:**
- Modify: `qt-app/studentcontroller.cpp` (replace the 4 stub bodies from Task 1)

**Interfaces:**
- Consumes: everything from Task 1 (`StudentRecord`, `SearchOutcome`, `BulkUpdateResult`, all 5 statics, the 6 signal decls).
- Produces (Task 3 relies on the emit contract):
  - `searchStudents(search, department, course)` → emits `searchFailed(errorString)` on network error, else `searchFinished(outcome, records, message, searchTerm)`.
  - `bulkUpdateStudents(updates)` → emits `bulkUpdateFailed(errorString)` on network error, else `bulkUpdateFinished(result)`.
  - `loadDepartments()` → emits `departmentsLoaded(list)` (empty on error/!success).
  - `loadCourses(department)` → emits `coursesLoaded(list)` (empty on error/!success).

- [ ] **Step 1: Replace `searchStudents` stub with the real implementation**

In `qt-app/studentcontroller.cpp`, replace:
```cpp
void StudentController::searchStudents(const QString &, const QString &, const QString &)
{
    // Implemented in Task 2.
}
```
with:
```cpp
void StudentController::searchStudents(const QString &search,
                                       const QString &department,
                                       const QString &course)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("search_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonObject filters;
    filters["search"]     = search;
    filters["department"] = normalizeFilter(department);   // reproduces adminwindow.cpp:3171
    filters["course"]     = normalizeFilter(course);       // reproduces adminwindow.cpp:3172

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit searchFailed(reply->errorString());       // View gates overlay row 11 on flag
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QList<StudentRecord> records;
        QString message, searchTerm;
        const SearchOutcome outcome =
            parseSearchResponse(resp, records, message, searchTerm);
        emit searchFinished(outcome, records, message, searchTerm);
    });
}
```

- [ ] **Step 2: Replace `bulkUpdateStudents` stub with the real implementation**

Replace:
```cpp
void StudentController::bulkUpdateStudents(const QList<StudentRecord> &)
{
    // Implemented in Task 2.
}
```
with (serializes the `StudentRecord` list back to the exact `updates[]` object shape at adminwindow.cpp:2942-2950):
```cpp
void StudentController::bulkUpdateStudents(const QList<StudentRecord> &updates)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("bulk_update_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonArray studentsArray;
    for (const StudentRecord &rec : updates) {
        QJsonObject student;
        student["school_id"]  = rec.schoolId;
        student["code"]       = rec.code;
        student["name"]       = rec.name;
        student["department"] = rec.department;
        student["course"]     = rec.course;
        student["year_level"] = rec.yearLevel;
        student["gender"]     = rec.gender;
        student["status"]     = rec.status;
        studentsArray.append(student);
    }

    QJsonObject data;
    data["students"] = studentsArray;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(data).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit bulkUpdateFailed(reply->errorString());   // View: overlay row 4 after UI reset
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        emit bulkUpdateFinished(parseBulkUpdateResponse(resp));
    });
}
```

- [ ] **Step 3: Replace `loadDepartments` stub with the real implementation**

Replace:
```cpp
void StudentController::loadDepartments()
{
    // Implemented in Task 2.
}
```
with:
```cpp
void StudentController::loadDepartments()
{
    QNetworkReply *reply =
        m_nam->get(QNetworkRequest(ApiConfig::endpoint(QStringLiteral("get_departments.php"))));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // Legacy shows no dialog on error or !success — an empty list plus the
        // View's always-prepended placeholder is the identical visible result.
        const QStringList departments =
            (reply->error() == QNetworkReply::NoError)
                ? parseDepartments(reply->readAll())
                : QStringList();
        reply->deleteLater();
        emit departmentsLoaded(departments);
    });
}
```

- [ ] **Step 4: Replace `loadCourses` stub with the real implementation**

Replace:
```cpp
void StudentController::loadCourses(const QString &)
{
    // Implemented in Task 2.
}
```
with (builds the `department=<dept>` query exactly as adminwindow.cpp:3483-3488):
```cpp
void StudentController::loadCourses(const QString &department)
{
    QUrl url = ApiConfig::endpoint(QStringLiteral("get_courses.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("department"), department);
    url.setQuery(query);

    QNetworkReply *reply = m_nam->get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QStringList courses =
            (reply->error() == QNetworkReply::NoError)
                ? parseCourses(reply->readAll())
                : QStringList();
        reply->deleteLater();
        emit coursesLoaded(courses);
    });
}
```

- [ ] **Step 5: Add the JSON includes the new bodies need**

`studentcontroller.cpp` already includes `<QJsonArray>`, `<QJsonDocument>`, `<QJsonObject>`, `<QNetworkAccessManager>`, `<QNetworkReply>`, `<QNetworkRequest>`, `<QUrl>`, `<QUrlQuery>` from Task 1 Step 3. Confirm all are present; add any that are missing. No new includes should be required.

- [ ] **Step 6: Build and re-run the static tests**

Run:
```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure
```
Expected: clean build, all 14 static-test slots still PASS. (The network methods have no automated test — accepted limitation per the spec, matching `tst_importcontroller`. They are exercised by manual QA in Task 3.)

- [ ] **Step 7: Purity gate**

Run:
```bash
grep -nE 'ui->|QMessageBox|QFileDialog|QTableWidget|showTemporaryOverlay' qt-app/studentcontroller.cpp
```
Expected: **no matches** (empty output). If anything matches, the logic belongs View-side — move it to Task 3.

- [ ] **Step 8: Commit (via the `commit` skill)**

```bash
git add qt-app/studentcontroller.cpp
# commit message (feat): implement StudentController network methods (search/bulk-update/departments/courses)
```

---

## Task 3: Wire the controller into adminWindow, rewrite View functions, delete dead code

**Files:**
- Modify: `qt-app/adminwindow.h` (add member + 6 slots + bool; delete 5 dead decls + 2 dead members)
- Modify: `qt-app/adminwindow.cpp` (construct+wire; rewrite 7 functions; collapse double-fetch; delete 5 dead functions + 2 ctor lines)

**Interfaces:**
- Consumes: the full `StudentController` public API + signals from Tasks 1–2.
- Produces: an `adminWindow` that owns `m_studentController`, connects its 6 signals to 6 new slots, and delegates all Student-tab network/parse work to it. No new public interface for later tasks (this is the final task).

**Reference — the 6 new slots and what each does (exact overlay strings from the Message/Dialog Inventory, rows 4–15):**

| Slot | Body summary |
|------|--------------|
| `onSearchFinished(SearchOutcome, records, message, searchTerm)` | Hide `searchLoadingBar`; branch on outcome (see Step 6). |
| `onSearchFailed(errorString)` | Hide `searchLoadingBar`; if `m_studentSearchShowOverlay`, overlay `"❌ Network Error\n%1".arg(errorString)`. |
| `onBulkUpdateFinished(result)` | Reset edit-mode UI; branch on `result.ok` (see Step 7). |
| `onBulkUpdateFailed(errorString)` | Reset edit-mode UI; overlay `"❌ Network Error\n%1".arg(errorString)`. |
| `onDepartmentsLoaded(departments)` | `searchDepartmentFilter->clear(); addItem("Select Department"); addItems(departments)`. |
| `onCoursesLoaded(courses)` | `searchCourseFilter->clear(); addItem("Select Course"); addItems(courses)`. |

- [ ] **Step 1: Add the controller member, bool, and slot declarations to `qt-app/adminwindow.h`**

After line 103 (`ImportController *m_importController;   // child of this, created in ctor`), add:
```cpp
    StudentController *m_studentController;   // child of this, created in ctor
    bool m_studentSearchShowOverlay = true;   // View-side overlay-suppression flag for silent refreshes
```

In the `private slots:` section (after line 158 region — group with the other controller slots), add:
```cpp
    void onSearchFinished(SearchOutcome outcome,
                          const QList<StudentRecord> &records,
                          const QString &message,
                          const QString &searchTerm);
    void onSearchFailed(const QString &errorString);
    void onBulkUpdateFinished(const BulkUpdateResult &result);
    void onBulkUpdateFailed(const QString &errorString);
    void onDepartmentsLoaded(const QStringList &departments);
    void onCoursesLoaded(const QStringList &courses);
```

Add the include near the other controller includes at the top of `adminwindow.h`:
```cpp
#include "studentcontroller.h"
```
(`studentcontroller.h` pulls in `studentdata.h`, so `SearchOutcome`/`StudentRecord`/`BulkUpdateResult` are visible for the slot signatures.)

- [ ] **Step 2: Delete the dead declarations from `qt-app/adminwindow.h`**

Remove these lines (they back the dead functions deleted in Step 9):
- Line 118: `void loadAllStudents();`
- Line 125: `void deleteStudents(const QStringList &schoolIds);`
- Line 136: `void showSearchOverlay();`
- Line 137: `void hideSearchOverlay();`
- Line 165: `void onDeleteStudentBtnClicked();`

And the two dead members — **delete exactly line 133 and line 135, keep line 134**:
- Line 133: `BusyIndicator *searchSpinner = nullptr;`  ← delete
- Line 134: `QLabel *overlayText = nullptr;            // optional`  ← **KEEP** (separate member, out of scope)
- Line 135: `QGraphicsOpacityEffect *overlayEffect = nullptr;`  ← delete

> Do **not** delete `performStudentSearch` (line 113) or `displaySearchResults` (line 114) — both survive (see Steps 4–5).

- [ ] **Step 3: Construct and wire `m_studentController` in the constructor**

In `qt-app/adminwindow.cpp`, after the `m_importController` wiring block (after line 442, before line 443 `chartsPreviewBoxLayout = ui->chartsPreviewBox;`), add:
```cpp
    m_studentController = new StudentController(networkManager, this);
    connect(m_studentController, &StudentController::searchFinished,
            this, &adminWindow::onSearchFinished);
    connect(m_studentController, &StudentController::searchFailed,
            this, &adminWindow::onSearchFailed);
    connect(m_studentController, &StudentController::bulkUpdateFinished,
            this, &adminWindow::onBulkUpdateFinished);
    connect(m_studentController, &StudentController::bulkUpdateFailed,
            this, &adminWindow::onBulkUpdateFailed);
    connect(m_studentController, &StudentController::departmentsLoaded,
            this, &adminWindow::onDepartmentsLoaded);
    connect(m_studentController, &StudentController::coursesLoaded,
            this, &adminWindow::onCoursesLoaded);
```

- [ ] **Step 4: Delete the two dead ctor `nullptr` lines and the redundant inline department lambda**

- Delete lines 444–445:
```cpp
    searchSpinner = nullptr;
    overlayEffect = nullptr;
```
- Delete the **entire inline lambda connect** at lines 460–494 (the `connect(ui->searchDepartmentFilter, …, this, [=](int index){ … });` block that issues its own `get_courses.php` GET). The surviving `connect` at lines 719–720 (`… this, &adminWindow::onDepartmentFilterChanged`) becomes the single handler. **This is the branch's one deliberate deviation — annotate it in the commit body.**

Leave the `loadDepartments();` call at line 457 and `populateFilters();` at line 458 in place (both fire at startup; `populateFilters` is the one being rewritten in Step 8).

> **Name-collision caution (from the spec):** `adminWindow::loadDepartments()` (adminwindow.cpp:1629) and `adminWindow::loadFilterDepartments()` (adminwindow.cpp:1837) belong to other tabs and are **out of scope — do not touch/rename/delete them**, despite `loadDepartments` sharing a name with the new `StudentController::loadDepartments()`. Only `populateFilters()` is extracted.

- [ ] **Step 5: Rewrite `performStudentSearch` as a thin View helper**

Replace the whole body of `performStudentSearch` (adminwindow.cpp:3153-3244) with the thin helper — it keeps the `searchLoadingBar` plumbing and the show-overlay flag View-side and delegates the network call. Its three call sites (`onSearchBtnClicked`→`true`; the bulk-update success refresh and `onCancelEditBtnClicked`→`false`) stay literally unchanged:
```cpp
void adminWindow::performStudentSearch(bool showOverlay)
{
    ui->searchLoadingBar->setVisible(true);
    ui->searchLoadingBar->setValue(0);
    m_studentSearchShowOverlay = showOverlay;

    m_studentController->searchStudents(
        ui->searchLineEdit->text().trimmed(),
        ui->searchDepartmentFilter->currentText(),
        ui->searchCourseFilter->currentText());
}
```
Keep `onSearchBtnClicked` (3277-3280) exactly as-is (`performStudentSearch(true);`).

- [ ] **Step 6: Add `onSearchFinished` / `onSearchFailed` slots (the table fill + overlay branching)**

`displaySearchResults` (3246-3274) stays as the table filler, but its signature changes from `QJsonArray` to `QList<StudentRecord>`. Replace its body's per-row reads to pull struct fields (byte-identical column order and cell text):
```cpp
void adminWindow::displaySearchResults(const QList<StudentRecord> &students,
                                       const QString &highlightTerm)
{
    Q_UNUSED(highlightTerm);   // legacy never used it — no highlighting exists; preserved for parity
    ui->studentSearchTable->blockSignals(true);
    ui->studentSearchTable->clearContents();
    ui->studentSearchTable->setRowCount(students.size());

    for (int row = 0; row < students.size(); ++row) {
        const StudentRecord &s = students.at(row);
        ui->studentSearchTable->setItem(row, 0, new QTableWidgetItem(""));
        ui->studentSearchTable->setItem(row, 1, new QTableWidgetItem(s.code));
        ui->studentSearchTable->setItem(row, 2, new QTableWidgetItem(s.schoolId));
        ui->studentSearchTable->setItem(row, 3, new QTableWidgetItem(s.name));
        ui->studentSearchTable->setItem(row, 4, new QTableWidgetItem(s.course));
        ui->studentSearchTable->setItem(row, 5, new QTableWidgetItem(s.department));
        ui->studentSearchTable->setItem(row, 6, new QTableWidgetItem(s.yearLevel));
        ui->studentSearchTable->setItem(row, 7, new QTableWidgetItem(s.gender));
        ui->studentSearchTable->setItem(row, 8, new QTableWidgetItem(s.status));
    }

    ui->studentSearchTable->blockSignals(false);
    ui->selectAllBtn->setText("Select All");
}
```
Update its declaration in `adminwindow.h:114` to match:
```cpp
    void displaySearchResults(const QList<StudentRecord> &students, const QString &highlightTerm);
```
Add the two search slots (place them near the other Student-tab functions in `adminwindow.cpp`). The overlay strings are rows 11–15 of the Message/Dialog Inventory, verbatim:
```cpp
void adminWindow::onSearchFailed(const QString &errorString)
{
    ui->searchLoadingBar->setVisible(false);
    if (m_studentSearchShowOverlay)
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("❌ Network Error\n%1").arg(errorString));   // row 11
}

void adminWindow::onSearchFinished(SearchOutcome outcome,
                                   const QList<StudentRecord> &records,
                                   const QString &message,
                                   const QString &searchTerm)
{
    ui->searchLoadingBar->setVisible(false);

    switch (outcome) {
    case SearchOutcome::InvalidResponse:
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage, "⚠️ Invalid server response");  // row 12
        break;
    case SearchOutcome::NotSuccess:
        ui->studentSearchTable->clearContents();          // cleared regardless of flag
        ui->studentSearchTable->setRowCount(0);
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage, QString("🔍 %1").arg(message));  // row 13
        break;
    case SearchOutcome::Empty:
        ui->studentSearchTable->clearContents();          // cleared regardless of flag
        ui->studentSearchTable->setRowCount(0);
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage,
                                 "🔍 No students found\nTry adjusting your search filters");  // row 14
        break;
    case SearchOutcome::Results:
        // Legacy guard was `showOverlay && students.size() > 0` (adminwindow.cpp:3236);
        // the size check is subsumed here because parseSearchResponse only returns
        // Results when the array is non-empty (Empty is split out upstream).
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("✅ Found %1 student%2")
                                     .arg(records.size())
                                     .arg(records.size() == 1 ? "" : "s"));   // row 15
        displaySearchResults(records, searchTerm);
        break;
    case SearchOutcome::NetworkError:
        break;   // never emitted via searchFinished; handled by onSearchFailed
    }
}
```

- [ ] **Step 7: Rewrite `bulkUpdateStudents` (collector→controller) + add `onBulkUpdateFinished`/`onBulkUpdateFailed`**

The `onEditStudentBtnClicked` update branch (2929-2953) currently builds a `QJsonArray updates`. Change it to build a `QList<StudentRecord>` and pass that to the controller. Replace the collection loop body's `QJsonObject student; … updates.append(student);` with `StudentRecord`:
```cpp
        QList<StudentRecord> updates;

        for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
            QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
            QCheckBox *checkbox = widget ? widget->findChild<QCheckBox*>() : nullptr;
            if (!checkbox || !checkbox->isChecked()) continue;

            auto getText = [&](int col) -> QString {
                QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
                return item ? item->text().trimmed() : QString();
            };

            QString schoolId = getText(2);
            if (schoolId.isEmpty()) continue;

            StudentRecord rec;
            rec.schoolId   = schoolId;
            rec.code       = getText(1);
            rec.name       = getText(3);
            rec.department = getText(5);
            rec.course     = getText(4);
            rec.yearLevel  = getText(6);
            rec.gender     = getText(7);
            rec.status     = getText(8);
            updates.append(rec);
        }
```
The color-reset loop (2955-2960) and the `updates.isEmpty()` guard (2962-2971, overlay row 3 `"⚠️ No students selected or edited."`) stay unchanged. The final call stays `bulkUpdateStudents(updates);` — but the method signature changes.

Replace the whole `adminWindow::bulkUpdateStudents(const QJsonArray &updates)` body (2986-3048) with a thin delegator, and change its declaration in `adminwindow.h:127` to `void bulkUpdateStudents(const QList<StudentRecord> &updates);`:
```cpp
void adminWindow::bulkUpdateStudents(const QList<StudentRecord> &updates)
{
    m_studentController->bulkUpdateStudents(updates);
}
```
Add the two bulk-update slots. The UI reset (2999-3005), success overlay row 5, delayed row-6 box, failure overlay row 7, and network-error overlay row 4 all move here verbatim:
```cpp
void adminWindow::onBulkUpdateFailed(const QString &errorString)
{
    ui->editStudentBtn->setText("Edit");
    ui->editStudentBtn->setEnabled(true);
    ui->cancelEditBtn->setVisible(false);
    ui->selectAllBtn->setVisible(false);
    ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clearCheckboxes();

    showTemporaryOverlay(ui->studentSearchPage,
                         QString("❌ Network Error\n%1").arg(errorString));   // row 4
}

void adminWindow::onBulkUpdateFinished(const BulkUpdateResult &result)
{
    ui->editStudentBtn->setText("Edit");
    ui->editStudentBtn->setEnabled(true);
    ui->cancelEditBtn->setVisible(false);
    ui->selectAllBtn->setVisible(false);
    ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clearCheckboxes();

    if (result.ok) {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("✅ %1 student%2 updated successfully")
                                 .arg(result.updatedCount)
                                 .arg(result.updatedCount == 1 ? "" : "s"));   // row 5

        if (!result.errors.isEmpty()) {
            QString errorList;
            for (const QString &err : result.errors)
                errorList += err + "\n";
            QTimer::singleShot(2500, this, [this, errorList]() {
                QMessageBox::warning(this, "Some Updates Failed", errorList);   // row 6
            });
        }

        performStudentSearch(false);   // silent refresh (flag=false via thin helper)
    } else {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("⚠️ Update Failed\n%1").arg(result.message));   // row 7
    }
}
```

- [ ] **Step 8: Rewrite `populateFilters` + `onDepartmentFilterChanged` and add the two combo slots**

Replace `populateFilters` (3359-3385) with a one-line delegator:
```cpp
void adminWindow::populateFilters()
{
    m_studentController->loadDepartments();
}
```
Replace `onDepartmentFilterChanged` (3478-3508) with the single collapsed handler (the up-front clear preserves the index-0 end state the deleted inline lambda used to provide):
```cpp
void adminWindow::onDepartmentFilterChanged(int)
{
    ui->searchCourseFilter->clear();
    ui->searchCourseFilter->addItem("Select Course");   // preserves the index-0 end state

    const QString dept = ui->searchDepartmentFilter->currentText();
    if (dept.isEmpty() || dept == "Select Department")
        return;                                          // placeholder: combo already = ["Select Course"]

    m_studentController->loadCourses(dept);              // single GET (was two)
}
```
Add the two combo slots (the View prepends the placeholder, matching legacy):
```cpp
void adminWindow::onDepartmentsLoaded(const QStringList &departments)
{
    ui->searchDepartmentFilter->clear();
    ui->searchDepartmentFilter->addItem("Select Department");
    ui->searchDepartmentFilter->addItems(departments);
}

void adminWindow::onCoursesLoaded(const QStringList &courses)
{
    ui->searchCourseFilter->clear();
    ui->searchCourseFilter->addItem("Select Course");
    ui->searchCourseFilter->addItems(courses);
}
```

- [ ] **Step 9: Rewrite `onCancelEditBtnClicked`'s refresh call (no other change) and delete the 5 dead functions**

`onCancelEditBtnClicked` (3051-3077) stays entirely View-side; its final `performStudentSearch(false)` is already correct (the thin helper handles the flag) — **no edit needed** beyond confirming it still compiles against the surviving helper.

Delete these five now-dead function definitions from `qt-app/adminwindow.cpp` in full:
- `deleteStudents(const QStringList&)` — 3282-3324
- `onDeleteStudentBtnClicked()` — 3326-3357
- `loadAllStudents()` — 3445-3476
- `showSearchOverlay()` — 3387-3420
- `hideSearchOverlay()` — 3422-3443

> Confirm no remaining callers before deleting: `deleteStudents` is called only by `onDeleteStudentBtnClicked`; `onDeleteStudentBtnClicked` has no `connect()` and does not match Qt's `on_<widget>_<signal>` auto-connect; `loadAllStudents`/`showSearchOverlay`/`hideSearchOverlay` have zero callers. `deleteStudentBtn` stays in `adminwindow.ui` unwired (do not touch the `.ui`).

- [ ] **Step 10: Build the full app**

Run:
```bash
cmake --build qt-app/build
```
Expected: `WITS` builds clean, no new warnings/errors. If the compiler flags a missing include for `StudentController` in `adminwindow.cpp`, confirm `adminwindow.h` includes `studentcontroller.h` (Step 1) — `adminwindow.cpp` includes `adminwindow.h`, so no separate include is needed there.

- [ ] **Step 11: Dead-code + single-fetch grep gates**

Run:
```bash
grep -nE 'deleteStudents|onDeleteStudentBtnClicked|loadAllStudents|showSearchOverlay|hideSearchOverlay|searchSpinner|overlayEffect' qt-app/adminwindow.cpp qt-app/adminwindow.h
grep -nc 'get_courses.php' qt-app/adminwindow.cpp qt-app/studentcontroller.cpp
```
Expected: first grep returns **no matches** (all dead symbols gone). Second grep: **1** occurrence in `adminwindow.cpp` — the out-of-scope Reports-tab `onFilterDepartmentBoxCurrentIndexChanged` shares the endpoint and stays — and **1** in `studentcontroller.cpp` (the Student-tab's single surviving fetch lives in the controller now). *(Correction post-implementation: this line originally said 0 for `adminwindow.cpp`, overlooking the Reports-tab caller.)*

- [ ] **Step 12: Run the full ctest suite**

Run:
```bash
ctest --test-dir qt-app/build --output-on-failure
```
Expected: **all 8 targets PASS** (7 existing + `tst_studentcontroller`).

- [ ] **Step 13: Manual QA of the Student Search tab (GUI app — build is necessary but not sufficient)**

Run the `WITS` executable and verify against the Verification Criteria in the spec:
- Search populates the table with identical columns/rows/cells; overlays rows 11–15 fire on the same conditions (gated by the flag); table clears on `NotSuccess`/`Empty` regardless of flag.
- Successful bulk update / cancel triggers a silent refresh with **no** search overlay.
- Edit mode enters/exits with rows 1–3, 8; col 2 (school_id) non-editable; checkbox/color behavior intact.
- Bulk update shows row 5 success, delayed row-6 box when `errors` non-empty, row-7 failure, row-4 network-error — identical text.
- Select All / Deselect All show rows 9–10 with identical counts/text.
- Department combo populates with `"Select Department"` + departments (skips `"all"`); selecting a department populates the course combo with `"Select Course"` + courses; selecting the placeholder resets the course combo to just `"Select Course"`. **Only one** `get_courses.php` request fires per change (confirm via backend/network log if available).

- [ ] **Step 14: Commit (via the `commit` skill)**

Intended grouping — one commit, with the deliberate deviation called out in the body:
```bash
git add qt-app/adminwindow.h qt-app/adminwindow.cpp
# commit message (refactor): wire StudentController into adminWindow; delete dead Student-tab code
#   body must note: collapses the duplicated department-filter double get_courses.php fetch to
#   a single request (2->1) — the branch's one deliberate, annotated deviation; end state preserved.
```

---

## Self-Review (completed by plan author)

**1. Spec coverage** — every spec section maps to a task:
- Value types (`studentdata.h`) → T1 S1. Public API (`studentcontroller.h`) → T1 S2. Five statics → T1 S3 + tests T1 S4. Four network methods + reply lifetime → T2. Two seams (show-overlay flag, refresh-after-mutation) → T3 S5/S7. Double-fetch collapse → T3 S4/S8. Dead-code deletion → T2/T3 S9 + T3 S2. Message/Dialog Inventory rows 1–15 → T3 S6/S7 (4–15) + preserved View functions (1–3, 8–10). CMake (both files) → T1 S5/S6. Tests → T1 S4. Verification Criteria → T3 S11–S13.
- Rows 1–3, 8, 9, 10 (edit-mode entry, cancel, select-all overlays) live in `onEditStudentBtnClicked`/`onCancelEditBtnClicked`/`on_selectAllBtn_clicked`, which stay View-side unchanged — noted in T3 as "no edit needed."

**2. Placeholder scan** — no TBD/TODO/"add error handling"/"similar to"/"write tests for the above"; every code step shows complete code and every command shows expected output.

**3. Type consistency** — `StudentRecord`/`BulkUpdateResult`/`SearchOutcome` field and enumerator names are identical across T1 (definition), T2 (`bulkUpdateStudents` serialization, `parseSearchResponse` fill), and T3 (`displaySearchResults`, collector loop, slots). Method names (`searchStudents`, `bulkUpdateStudents`, `loadDepartments`, `loadCourses`, `normalizeFilter`, `parse*`) and signal names (`searchFinished`/`searchFailed`/`bulkUpdateFinished`/`bulkUpdateFailed`/`departmentsLoaded`/`coursesLoaded`) match their T1 declarations. `displaySearchResults` and `bulkUpdateStudents` signature changes (QJsonArray→QList<StudentRecord>) are updated in both the `.h` decl and the `.cpp` def within T3.
