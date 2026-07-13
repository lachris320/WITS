# LOAMS 2.0 Phase 3 — Admin Part 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Phase-2 admin placeholder with a real admin surface — a maroon sidebar shell + page header with live clock, and three functional screens (Dashboard, Search, Visit Logs) — driven by per-screen ViewModels over `witscore`, backed by two new read-only PHP endpoints plus a one-field extension to `search_students.php`.

**Architecture:** MVVM. ViewModels (`quick/viewmodels/`) are the only QML-facing C++; QML never calls a `witscore` controller directly. Each screen receives its VM as `property var vm` so QuickTests inject a plain-QML stub. Pure JSON parsers live in `core/` (unit-tested with captured fixtures, **no live network**); row lists are `QAbstractListModel`s following the `RecentLoginsModel` pattern. Navigation extends the existing `Navigator` singleton.

**Tech Stack:** Qt 6.11.1 / C++17, Qt Quick (QML module URI `LOAMS`, CMake target `witsquickmodule`), Qt Test + Qt Quick Test, CMake + Ninja (MinGW kit), PHP/MySQLi backend (`deliverables/loams_api/`).

**Source spec:** `docs/superpowers/specs/2026-07-13-loams2-phase3-admin-part1-design.md` (approved 2026-07-13). Section references below (§N) point into that spec.

## Global Constraints

These apply to **every** task; each task's requirements implicitly include this section.

- **MVVM boundary (CLAUDE.md):** ViewModels are the only QML-facing C++. QML never calls a `witscore` controller directly. Every screen takes `property var vm`.
- **Theming (CLAUDE.md):** ZERO raw hex outside `qt-app/quick/qml/theme/Theme.qml`. Opacity variants use `Qt.alpha(Theme.<token>, a)`, never a literal color. New tokens go in `Theme.qml` only. **Because a C++ ViewModel cannot read Theme tokens, color decisions live in QML** — models carry data (label/value/text), components/screens apply Theme colors.
- **Naming (CLAUDE.md):** QML types and C++ ViewModel/model classes are `PascalCase`; C++ members are `m_camelCase`.
- **Tests (CLAUDE.md):** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add `OFFSCREEN` for any GUI/Quick/painting test. Console QtTest exes (`WIN32_EXECUTABLE FALSE`) are handled by the helper.
- **No live network in tests (spec §5, house rule):** parsers/VMs expose a network-free `apply…`/`on…Finished` seam that tests drive with a captured/synthetic `QByteArray`. Never hit `http://localhost` in a test.
- **SQL safety (spec §5):** both new PHP endpoints build **every** query with prepared statements + `bind_param`. Never string-interpolate a parameter. Do **not** copy `get_visitors.php`'s unescaped-date pattern.
- **Endpoints via `ApiConfig::endpoint(...)` only** over the shipped base `http://localhost/loams_api/` (`qt-app/core/apiconfig.h:18-21`, unchanged). The two new endpoints are called by filename — **not** added to `api.php`'s router.
- **Security-hygiene:** error strings shown to the user must not leak backend internals; no secrets/PII/real student data in fixtures (use synthetic data).
- **Commits (workflow.md §5):** commit **only** via the `commit` skill — never raw `git add`/`git commit`. The "Commit" step in each task means "invoke the commit skill with the stated message." Never commit `qt-app/build/`.
- **Orchestration (workflow.md §0):** the main agent dispatches a subagent per task; each subagent owns its TDD cycle end-to-end.

### Standard configure / build / test commands

Tools are **not** on PATH on this machine — prepend the Qt tool dirs in the **same** PowerShell command every time (shell state does not persist):

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```

- Run a single test: `ctest --test-dir qt-app/build -R <name> --output-on-failure`
- **After editing any `CMakeLists.txt`** (new target/source/QML file), re-run the `cmake -S … -B …` **configure** step before `cmake --build`.
- Harmless warnings to ignore: `LF will be replaced by CRLF`; the pre-existing `QXlsx … GuiPrivate target` CMake warning.
- Baseline before starting: the ctest suite is **23 targets** green (spec §12). Every task keeps it green; new tests raise the count.

### File-structure overview (what each new file owns)

**Backend (`deliverables/loams_api/`)** — deployed manually to the library server; no PHP test harness in-repo. The parser unit tests are the contract check.
- `dashboard_summary.php` (NEW) — today/week/students counts + hourly + department aggregates.
- `get_library_visits.php` (NEW) — student attendance rows for range=today|week.
- `search_students.php` (MODIFY) — add `visits` to the response map.

**Core (`qt-app/core/`, compiled into `witscore`)** — pure, widget-free, network-free parsers + structs.
- `dashboarddata.h` / `dashboardparser.h`+`.cpp` — `DashboardSummary` + `DashboardParser::parse`.
- `visitlogdata.h` / `visitlogparser.h`+`.cpp` — `StudentVisitRecord` + `VisitLogParser::parse`.
- `studentdata.h` / `studentcontroller.cpp` (MODIFY) — `StudentRecord.visits` + parser reads it.

**Quick models (`qt-app/quick/models/`)** — `QAbstractListModel`s exposed via VM properties (not QML_ELEMENT).
- `BarsModel.h`+`.cpp` — bar series (label + value) for `LBarChart`.
- `VisitLogRowsModel.h`+`.cpp` — union of student + guest columns.
- `SearchResultsModel.h`+`.cpp` — search result rows incl. `visits`.

**Quick ViewModels (`qt-app/quick/viewmodels/`)** — QML_ELEMENT, the only QML-facing C++.
- `Navigator.h`+`.cpp` (MODIFY) — admin sub-page state.
- `DashboardViewModel.h`+`.cpp`, `VisitLogsViewModel.h`+`.cpp`, `SearchViewModel.h`+`.cpp`.

**Quick QML (`qt-app/quick/qml/`)**
- `components/LSideNav.qml`, `LTable.qml`, `LBarChart.qml`, `LPageHeader.qml` (all MODIFY, Phase-1 skeletons → functional).
- `admin/DashboardScreen.qml`, `admin/SearchScreen.qml`, `admin/VisitLogsScreen.qml` (NEW).
- `admin/AdminScreen.qml` (MODIFY) — placeholder → shell.

**Task order & dependencies:** 1 (Navigator) → 2/3/4 (backend+core parsers, independent) → 5 (BarsModel) → 6/7/8 (VMs) → 9/10/11/12 (components, independent of VMs) → 13/14/15 (screens, need VMs+components) → 16 (shell, needs all screens). Tasks 2/3/4 and 9/10/11/12 are internally parallelizable.

---

## Task 1: Navigator — admin sub-page state

**Files:**
- Modify: `qt-app/quick/viewmodels/Navigator.h`
- Modify: `qt-app/quick/viewmodels/Navigator.cpp`
- Test: `qt-app/quick/tests/tst_navigator.cpp`

**Interfaces:**
- Produces: `Navigator::AdminPage { Dashboard, Search, VisitLogs }` (Q_ENUM); `Q_PROPERTY(AdminPage adminPage READ adminPage NOTIFY adminPageChanged)` default `Dashboard`; `Q_INVOKABLE void showAdminPage(AdminPage)` (idempotent). `showKiosk()`/`showAdmin()` unchanged.

- [ ] **Step 1: Write the failing tests** — add to `tst_navigator.cpp`. Add these two declarations to the `private slots:` block:

```cpp
    void defaultsToDashboardPage();
    void showAdminPageSwitchesAndSignals();
```

And add the two method bodies before `QTEST_MAIN`:

```cpp
void TestNavigator::defaultsToDashboardPage()
{
    Navigator nav;
    QCOMPARE(nav.adminPage(), Navigator::Dashboard);
}

void TestNavigator::showAdminPageSwitchesAndSignals()
{
    Navigator nav;
    QSignalSpy spy(&nav, &Navigator::adminPageChanged);
    nav.showAdminPage(Navigator::VisitLogs);
    QCOMPARE(nav.adminPage(), Navigator::VisitLogs);
    QCOMPARE(spy.count(), 1);
    nav.showAdminPage(Navigator::VisitLogs);   // idempotent — no redundant signal
    QCOMPARE(spy.count(), 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_navigator --output-on-failure` (configure first)
Expected: FAIL — build error, `adminPage` / `showAdminPage` / `Navigator::Dashboard` undefined.

- [ ] **Step 3: Implement in `Navigator.h`** — replace the class body's public enum/accessor region and slots/signals/private members:

```cpp
    Q_PROPERTY(Surface currentSurface READ currentSurface NOTIFY currentSurfaceChanged)
    Q_PROPERTY(AdminPage adminPage READ adminPage NOTIFY adminPageChanged)
public:
    enum Surface { Kiosk, Admin };
    Q_ENUM(Surface)
    enum AdminPage { Dashboard, Search, VisitLogs };
    Q_ENUM(AdminPage)

    explicit Navigator(QObject *parent = nullptr);

    Surface currentSurface() const { return m_surface; }
    AdminPage adminPage() const { return m_adminPage; }

public slots:
    void showKiosk();
    void showAdmin();
    void showAdminPage(AdminPage page);

signals:
    void currentSurfaceChanged();
    void adminPageChanged();

private:
    void setSurface(Surface s);
    Surface m_surface = Kiosk;
    AdminPage m_adminPage = Dashboard;
```

- [ ] **Step 4: Implement in `Navigator.cpp`** — append after `showAdmin()`:

```cpp
void Navigator::showAdminPage(AdminPage page)
{
    if (m_adminPage == page)
        return;                      // idempotent — mirrors setSurface's guard
    m_adminPage = page;
    emit adminPageChanged();
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_navigator --output-on-failure`
Expected: PASS (5 test functions).

- [ ] **Step 6: Commit** — via the `commit` skill, message:
`feat(loams2): add admin sub-page state to Navigator (Phase 3)`

---

## Task 2: `search_students.php` visits — PHP + StudentRecord + parser (§5.3 layers 1–3)

**Files:**
- Modify: `deliverables/loams_api/search_students.php:74-84`
- Modify: `qt-app/core/studentdata.h:11-21`
- Modify: `qt-app/core/studentcontroller.cpp` (`parseSearchResponse`, ~`:57-69`)
- Test: `qt-app/tests/tst_studentcontroller.cpp`

**Interfaces:**
- Produces: `StudentRecord.visits` (`int`, default `0`); `parseSearchResponse` fills it from the JSON `visits` field (missing → `0`). Bulk-update serialize path (`studentcontroller.cpp:180-190`) is unchanged — `visits` is simply not serialized back.

- [ ] **Step 1: Write the failing tests** — add to `tst_studentcontroller.cpp`. Add to the `private slots:` block:

```cpp
    void parseSearchReadsVisits();
    void parseSearchDefaultsVisitsToZero();
```

Add the bodies before `QTEST_MAIN`:

```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: FAIL — `StudentRecord` has no member `visits`.

- [ ] **Step 3: Add the struct field** — in `studentdata.h`, inside `struct StudentRecord`, after `QString status;`:

```cpp
    QString status;
    int     visits = 0;   // lifetime library visit count (search read-only, §5.3)
```

- [ ] **Step 4: Read the field in the parser** — in `studentcontroller.cpp`, in the `parseSearchResponse` per-record loop, after `rec.status = s["status"].toString();`:

```cpp
        rec.status     = s["status"].toString();
        rec.visits     = s["visits"].toInt();
```

- [ ] **Step 5: Run test to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Add the field to the PHP response** — in `deliverables/loams_api/search_students.php`, extend the `$students[]` map (currently ends `"gender" => $row['gender']`):

```php
        $students[] = [
            "code" => isset($row['code']) ? $row['code'] : "",
            "school_id" => $row['school_id'],
            "name" => $row['name'],
            "department" => $row['department'],
            "course" => $row['course'],
            "year_level" => $row['year_level'],
            "status" => $row['status'],
            "gender" => $row['gender'],
            "visits" => isset($row['visits']) ? intval($row['visits']) : 0
        ];
```

(The query already `SELECT *`s every column, so `$row['visits']` is present; the `isset` guard keeps it safe if the column is ever absent. No SQL change — no injection surface added.)

- [ ] **Step 7: Commit** — via the `commit` skill, message:
`feat(loams2): surface lifetime visits count through student search (Phase 3)`

---

## Task 3: `dashboard_summary.php` endpoint + `DashboardParser`

**Files:**
- Create: `deliverables/loams_api/dashboard_summary.php`
- Create: `qt-app/core/dashboarddata.h`
- Create: `qt-app/core/dashboardparser.h`
- Create: `qt-app/core/dashboardparser.cpp`
- Create: `qt-app/tests/tst_dashboardparser.cpp`
- Modify: `qt-app/core/CMakeLists.txt`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces:**
- Produces: `struct DashboardSummary { int today, week, students; QList<HourBucket> hourly; QList<DeptBucket> departments; }`, `struct HourBucket { int hour, count; }`, `struct DeptBucket { QString name; int count; }`. `bool DashboardParser::parse(const QByteArray &raw, DashboardSummary &out, QString &outError)` — returns `true` on a `status=="success"` object, else `false` with a user-safe `outError`.

- [ ] **Step 1: Write the failing test** — `qt-app/tests/tst_dashboardparser.cpp`:

```cpp
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
```

- [ ] **Step 2: Create the data + parser headers.**

`qt-app/core/dashboarddata.h`:

```cpp
#ifndef DASHBOARDDATA_H
#define DASHBOARDDATA_H

#include <QList>
#include <QString>

// One hourly histogram bucket (spec §5.1). hour is 0..23 (server-local).
struct HourBucket { int hour = 0; int count = 0; };

// One department breakdown bucket (spec §5.1).
struct DeptBucket { QString name; int count = 0; };

// Decoded dashboard_summary.php response. Peak hour is NOT here — it is
// derived client-side in DashboardViewModel (spec §5.1/§6.1).
struct DashboardSummary
{
    int today = 0;
    int week = 0;
    int students = 0;
    QList<HourBucket> hourly;
    QList<DeptBucket> departments;
};

#endif // DASHBOARDDATA_H
```

`qt-app/core/dashboardparser.h`:

```cpp
#ifndef DASHBOARDPARSER_H
#define DASHBOARDPARSER_H

#include <QByteArray>
#include <QString>
#include "dashboarddata.h"

// Pure, network-free decoder for dashboard_summary.php (spec §5.1). Returns
// true and fills `out` on a status=="success" object; otherwise returns false
// and sets `outError` to a user-safe message (never a backend internal).
namespace DashboardParser {
bool parse(const QByteArray &raw, DashboardSummary &out, QString &outError);
}

#endif // DASHBOARDPARSER_H
```

- [ ] **Step 3: Create `qt-app/core/dashboardparser.cpp`:**

```cpp
#include "dashboardparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool DashboardParser::parse(const QByteArray &raw, DashboardSummary &out, QString &outError)
{
    out = DashboardSummary();
    outError.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        outError = QStringLiteral("Invalid server response.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        outError = QStringLiteral("Could not load dashboard data.");
        return false;
    }

    out.today    = obj.value(QStringLiteral("today")).toInt();
    out.week     = obj.value(QStringLiteral("week")).toInt();
    out.students = obj.value(QStringLiteral("students")).toInt();

    const QJsonArray hours = obj.value(QStringLiteral("hourly")).toArray();
    for (const QJsonValue &v : hours) {
        const QJsonObject h = v.toObject();
        out.hourly.append(HourBucket{ h.value(QStringLiteral("hour")).toInt(),
                                      h.value(QStringLiteral("count")).toInt() });
    }

    const QJsonArray depts = obj.value(QStringLiteral("departments")).toArray();
    for (const QJsonValue &v : depts) {
        const QJsonObject d = v.toObject();
        out.departments.append(DeptBucket{ d.value(QStringLiteral("name")).toString(),
                                           d.value(QStringLiteral("count")).toInt() });
    }
    return true;
}
```

- [ ] **Step 4: Register the parser in `witscore`** — in `qt-app/core/CMakeLists.txt`, add to the `add_library(witscore STATIC …)` source list (e.g. after the `studentcontroller` lines):

```cmake
    dashboarddata.h
    dashboardparser.h dashboardparser.cpp
```

- [ ] **Step 5: Register the test** — in `qt-app/tests/CMakeLists.txt`, append:

```cmake
qt_add_executable(tst_dashboardparser
    tst_dashboardparser.cpp
    ${CMAKE_SOURCE_DIR}/core/dashboardparser.cpp
    ${CMAKE_SOURCE_DIR}/core/dashboardparser.h
    ${CMAKE_SOURCE_DIR}/core/dashboarddata.h
)
set_target_properties(tst_dashboardparser PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_dashboardparser PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(tst_dashboardparser PRIVATE Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_dashboardparser COMMAND tst_dashboardparser)
set_tests_properties(tst_dashboardparser PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
```

- [ ] **Step 6: Configure, build, run — verify PASS**

Run: (configure) then `ctest --test-dir qt-app/build -R tst_dashboardparser --output-on-failure`
Expected: PASS (4 functions).

- [ ] **Step 7: Create the PHP endpoint** — `deliverables/loams_api/dashboard_summary.php`. Prepared statements only (§5). "today" = `DATE(login_time)=CURDATE()`; "week" = Monday 00:00 (inclusive) → next Monday 00:00 (exclusive), server-local (§5).

```php
<?php
header('Content-Type: application/json');
include 'db.php';

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "Database connection failed"]);
    exit;
}

// Current Mon–Sun calendar week as a half-open [start, end) datetime range
// (spec §5). Server-local time is authoritative (single on-prem server).
$weekStart = (new DateTime('monday this week'))->format('Y-m-d 00:00:00');
$weekEnd   = (new DateTime('monday next week'))->format('Y-m-d 00:00:00');

$response = ["status" => "success"];

// --- Visitors today ---
$stmt = $conn->prepare("SELECT COUNT(*) FROM library_visits WHERE DATE(login_time) = CURDATE()");
$stmt->execute();
$stmt->bind_result($today);
$stmt->fetch();
$stmt->close();
$response["today"] = intval($today);

// --- Visitors this week [weekStart, weekEnd) ---
$stmt = $conn->prepare("SELECT COUNT(*) FROM library_visits WHERE login_time >= ? AND login_time < ?");
$stmt->bind_param("ss", $weekStart, $weekEnd);
$stmt->execute();
$stmt->bind_result($week);
$stmt->fetch();
$stmt->close();
$response["week"] = intval($week);

// --- Registered students ---
$stmt = $conn->prepare("SELECT COUNT(*) FROM students");
$stmt->execute();
$stmt->bind_result($students);
$stmt->fetch();
$stmt->close();
$response["students"] = intval($students);

// --- Hourly histogram for today ---
$hourly = [];
$stmt = $conn->prepare(
    "SELECT HOUR(login_time) AS h, COUNT(*) AS c
     FROM library_visits
     WHERE DATE(login_time) = CURDATE()
     GROUP BY HOUR(login_time)
     ORDER BY h");
$stmt->execute();
$res = $stmt->get_result();
while ($row = $res->fetch_assoc()) {
    $hourly[] = ["hour" => intval($row["h"]), "count" => intval($row["c"])];
}
$stmt->close();
$response["hourly"] = $hourly;

// --- Department breakdown for the current week ---
$departments = [];
$stmt = $conn->prepare(
    "SELECT department AS name, COUNT(*) AS c
     FROM library_visits
     WHERE login_time >= ? AND login_time < ?
     GROUP BY department
     ORDER BY c DESC");
$stmt->bind_param("ss", $weekStart, $weekEnd);
$stmt->execute();
$res = $stmt->get_result();
while ($row = $res->fetch_assoc()) {
    $departments[] = ["name" => $row["name"], "count" => intval($row["c"])];
}
$stmt->close();
$response["departments"] = $departments;

echo json_encode($response);
$conn->close();
?>
```

(Deployed manually to the library server — no in-repo PHP test. If the captured live response differs from the fixture field names, update `okFixture()` + the parser together and re-run Step 6.)

- [ ] **Step 8: Commit** — via the `commit` skill, message:
`feat(loams2): add dashboard_summary endpoint + parser (Phase 3)`

---

## Task 4: `get_library_visits.php` endpoint + `VisitLogParser`

**Files:**
- Create: `deliverables/loams_api/get_library_visits.php`
- Create: `qt-app/core/visitlogdata.h`
- Create: `qt-app/core/visitlogparser.h`
- Create: `qt-app/core/visitlogparser.cpp`
- Create: `qt-app/tests/tst_visitlogparser.cpp`
- Modify: `qt-app/core/CMakeLists.txt`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces:**
- Produces: `struct StudentVisitRecord { QString date, name, course, department, timeIn; }` (no time-out — login-only, §5.2). `bool VisitLogParser::parse(const QByteArray &raw, QList<StudentVisitRecord> &out, int &outCount, QString &outError)`.

- [ ] **Step 1: Write the failing test** — `qt-app/tests/tst_visitlogparser.cpp`:

```cpp
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
```

- [ ] **Step 2: Create the headers.**

`qt-app/core/visitlogdata.h`:

```cpp
#ifndef VISITLOGDATA_H
#define VISITLOGDATA_H

#include <QString>

// One student attendance row from get_library_visits.php (spec §5.2). No
// time-out field: the system is login-only (library_visits has no logout
// column). The screen renders a literal "—" for time-out (spec §6.3).
struct StudentVisitRecord
{
    QString date;         // "yyyy-MM-dd"
    QString name;
    QString course;
    QString department;
    QString timeIn;       // "HH:mm"
};

#endif // VISITLOGDATA_H
```

`qt-app/core/visitlogparser.h`:

```cpp
#ifndef VISITLOGPARSER_H
#define VISITLOGPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include "visitlogdata.h"

// Pure, network-free decoder for get_library_visits.php (spec §5.2). Returns
// true + fills out/outCount on status=="success"; else false + user-safe error.
namespace VisitLogParser {
bool parse(const QByteArray &raw, QList<StudentVisitRecord> &out,
           int &outCount, QString &outError);
}

#endif // VISITLOGPARSER_H
```

- [ ] **Step 3: Create `qt-app/core/visitlogparser.cpp`:**

```cpp
#include "visitlogparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool VisitLogParser::parse(const QByteArray &raw, QList<StudentVisitRecord> &out,
                           int &outCount, QString &outError)
{
    out.clear();
    outCount = 0;
    outError.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        outError = QStringLiteral("Invalid server response.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        outError = QStringLiteral("Could not load visit logs.");
        return false;
    }

    const QJsonArray visits = obj.value(QStringLiteral("visits")).toArray();
    for (const QJsonValue &v : visits) {
        const QJsonObject o = v.toObject();
        StudentVisitRecord rec;
        rec.date       = o.value(QStringLiteral("date")).toString();
        rec.name       = o.value(QStringLiteral("name")).toString();
        rec.course     = o.value(QStringLiteral("course")).toString();
        rec.department = o.value(QStringLiteral("department")).toString();
        rec.timeIn     = o.value(QStringLiteral("time_in")).toString();
        out.append(rec);
    }
    // Trust the array length for the count; fall back to the "count" field only
    // if the array is absent but a count was supplied.
    outCount = obj.contains(QStringLiteral("visits"))
                   ? out.size()
                   : obj.value(QStringLiteral("count")).toInt();
    return true;
}
```

- [ ] **Step 4: Register in `witscore`** — in `qt-app/core/CMakeLists.txt` source list:

```cmake
    visitlogdata.h
    visitlogparser.h visitlogparser.cpp
```

- [ ] **Step 5: Register the test** — in `qt-app/tests/CMakeLists.txt`:

```cmake
qt_add_executable(tst_visitlogparser
    tst_visitlogparser.cpp
    ${CMAKE_SOURCE_DIR}/core/visitlogparser.cpp
    ${CMAKE_SOURCE_DIR}/core/visitlogparser.h
    ${CMAKE_SOURCE_DIR}/core/visitlogdata.h
)
set_target_properties(tst_visitlogparser PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_visitlogparser PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(tst_visitlogparser PRIVATE Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_visitlogparser COMMAND tst_visitlogparser)
set_tests_properties(tst_visitlogparser PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
```

- [ ] **Step 6: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_visitlogparser --output-on-failure`
Expected: PASS (4 functions).

- [ ] **Step 7: Create the PHP endpoint** — `deliverables/loams_api/get_library_visits.php`. Prepared statements only; `range=today|week` (default today) or explicit `start`/`end` as app-generated ISO dates (§5.2).

```php
<?php
header('Content-Type: application/json');
include 'db.php';

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "Database connection failed"]);
    exit;
}

$range = isset($_GET['range']) ? strtolower(trim($_GET['range'])) : 'today';
$start = isset($_GET['start']) ? trim($_GET['start']) : '';
$end   = isset($_GET['end'])   ? trim($_GET['end'])   : '';

// Resolve a half-open [start, end) datetime window (spec §5 date semantics).
if ($range === 'week') {
    $startDt = (new DateTime('monday this week'))->format('Y-m-d 00:00:00');
    $endDt   = (new DateTime('monday next week'))->format('Y-m-d 00:00:00');
} elseif ($start !== '' && $end !== '') {
    $startDt = $start . ' 00:00:00';
    $endDt   = $end   . ' 23:59:59';
} else { // today (default)
    $startDt = (new DateTime('today'))->format('Y-m-d 00:00:00');
    $endDt   = (new DateTime('tomorrow'))->format('Y-m-d 00:00:00');
}

$stmt = $conn->prepare(
    "SELECT DATE(login_time) AS date, name, course, department,
            TIME_FORMAT(login_time, '%H:%i') AS time_in
     FROM library_visits
     WHERE login_time >= ? AND login_time < ?
     ORDER BY login_time DESC");
$stmt->bind_param("ss", $startDt, $endDt);
$stmt->execute();
$res = $stmt->get_result();

$visits = [];
while ($row = $res->fetch_assoc()) {
    $visits[] = $row;
}
$stmt->close();

echo json_encode(["status" => "success", "count" => count($visits), "visits" => $visits]);
$conn->close();
?>
```

- [ ] **Step 8: Commit** — via the `commit` skill, message:
`feat(loams2): add get_library_visits endpoint + parser (Phase 3)`

---

## Task 5: `BarsModel` — bar-series list model

**Files:**
- Create: `qt-app/quick/models/BarsModel.h`
- Create: `qt-app/quick/models/BarsModel.cpp`
- Create: `qt-app/quick/tests/tst_barsmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Produces: `class BarsModel : public QAbstractListModel` with `struct Bar { QString label; double value; }`, roles `LabelRole`(`"label"`)/`ValueRole`(`"value"`), `void setBars(const QList<Bar> &)`, `double maxValue() const`. No color role — colors are applied in QML from Theme (Global Constraints). Used by `DashboardViewModel` (Task 6) and `LBarChart` (Task 11).

- [ ] **Step 1: Write the failing test** — `qt-app/quick/tests/tst_barsmodel.cpp`:

```cpp
#include <QtTest>
#include <QAbstractItemModelTester>
#include "BarsModel.h"

class TestBarsModel : public QObject
{
    Q_OBJECT
private slots:
    void emptyByDefault();
    void setBarsPopulatesRowsAndRoles();
    void maxValueTracksLargestBar();
    void resetClearsWhenEmpty();
};

void TestBarsModel::emptyByDefault()
{
    BarsModel m;
    QAbstractItemModelTester tester(&m);   // sanity-checks model contract
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.maxValue(), 0.0);
}

void TestBarsModel::setBarsPopulatesRowsAndRoles()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("8"), 12.0}, {QStringLiteral("9"), 34.0} });
    QCOMPARE(m.rowCount(), 2);
    const QModelIndex i0 = m.index(0, 0);
    QCOMPARE(m.data(i0, BarsModel::LabelRole).toString(), QStringLiteral("8"));
    QCOMPARE(m.data(i0, BarsModel::ValueRole).toDouble(), 12.0);
}

void TestBarsModel::maxValueTracksLargestBar()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("a"), 5.0}, {QStringLiteral("b"), 41.0}, {QStringLiteral("c"), 9.0} });
    QCOMPARE(m.maxValue(), 41.0);
}

void TestBarsModel::resetClearsWhenEmpty()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("a"), 5.0} });
    m.setBars({});
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.maxValue(), 0.0);
}

QTEST_MAIN(TestBarsModel)
#include "tst_barsmodel.moc"
```

- [ ] **Step 2: Create `qt-app/quick/models/BarsModel.h`:**

```cpp
#ifndef BARSMODEL_H
#define BARSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

// Generic bar-series model for LBarChart (hourly + department, spec §6.1).
// Carries only label + numeric value; the bar COLOR is chosen in QML from
// Theme tokens (Global Constraints: no color decisions in C++). Follows the
// RecentLoginsModel pattern (roles enum, roleNames(), a typed Row struct).
class BarsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Bar { QString label; double value = 0.0; };
    enum Roles { LabelRole = Qt::UserRole + 1, ValueRole };

    explicit BarsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setBars(const QList<Bar> &bars);
    double maxValue() const { return m_maxValue; }

private:
    QList<Bar> m_bars;
    double m_maxValue = 0.0;
};

#endif // BARSMODEL_H
```

- [ ] **Step 3: Create `qt-app/quick/models/BarsModel.cpp`:**

```cpp
#include "BarsModel.h"

BarsModel::BarsModel(QObject *parent) : QAbstractListModel(parent) {}

int BarsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_bars.size();
}

QVariant BarsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_bars.size())
        return {};
    const Bar &b = m_bars.at(index.row());
    switch (role) {
    case LabelRole: return b.label;
    case ValueRole: return b.value;
    default:        return {};
    }
}

QHash<int, QByteArray> BarsModel::roleNames() const
{
    return { { LabelRole, "label" }, { ValueRole, "value" } };
}

void BarsModel::setBars(const QList<Bar> &bars)
{
    beginResetModel();
    m_bars = bars;
    m_maxValue = 0.0;
    for (const Bar &b : m_bars)
        if (b.value > m_maxValue)
            m_maxValue = b.value;
    endResetModel();
}
```

- [ ] **Step 4: Register model + test in `qt-app/quick/CMakeLists.txt`.** Add to the `qt_add_qml_module(witsquickmodule … SOURCES …)` list (after the `models/RecentLoginsModel` line):

```cmake
        models/BarsModel.h models/BarsModel.cpp
```

Then append a test target (after `tst_httpform`):

```cmake
# --- BarsModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_barsmodel
    SOURCES tests/tst_barsmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)
```

- [ ] **Step 5: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_barsmodel --output-on-failure`
Expected: PASS (4 functions).

- [ ] **Step 6: Commit** — via the `commit` skill, message:
`feat(loams2): add BarsModel bar-series list model (Phase 3)`

---

## Task 6: `DashboardViewModel`

**Files:**
- Create: `qt-app/quick/viewmodels/DashboardViewModel.h`
- Create: `qt-app/quick/viewmodels/DashboardViewModel.cpp`
- Create: `qt-app/quick/tests/tst_dashboardviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Consumes: `DashboardParser::parse` + `DashboardSummary` (Task 3); `BarsModel` (Task 5); `ApiConfig::endpoint`.
- Produces (QML-facing): `statToday`/`statWeek`/`statStudents` (int), `peakHourLabel` (QString), `peakHourIndex` (int, −1 when no data), `hourlyModel`/`departmentModel` (`BarsModel*`), `loading` (bool), `errorText` (QString); `Q_INVOKABLE void refresh()`; network-free seam `void applySummary(const QByteArray &)`; pure static `QString formatPeakHour(int hour)`.

- [ ] **Step 1: Write the failing test** — `qt-app/quick/tests/tst_dashboardviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "DashboardViewModel.h"
#include "BarsModel.h"

class TestDashboardViewModel : public QObject
{
    Q_OBJECT
private slots:
    void formatPeakHourMapsTo12Hour();
    void applySummaryPopulatesStatsAndModels();
    void applySummaryDerivesPeak();
    void applyInvalidSetsErrorText();
};

void TestDashboardViewModel::formatPeakHourMapsTo12Hour()
{
    QCOMPARE(DashboardViewModel::formatPeakHour(0),  QStringLiteral("12 AM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(8),  QStringLiteral("8 AM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(12), QStringLiteral("12 PM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(14), QStringLiteral("2 PM"));
    QCOMPARE(DashboardViewModel::formatPeakHour(-1), QStringLiteral("—"));
}

void TestDashboardViewModel::applySummaryPopulatesStatsAndModels()
{
    DashboardViewModel vm;
    QSignalSpy spy(&vm, &DashboardViewModel::dataChanged);
    vm.applySummary(R"({
        "status":"success","today":128,"week":812,"students":3450,
        "hourly":[{"hour":8,"count":12},{"hour":9,"count":34}],
        "departments":[{"name":"CE","count":210},{"name":"IT","count":180}]
    })");
    QVERIFY(spy.count() >= 1);
    QCOMPARE(vm.statToday(), 128);
    QCOMPARE(vm.statWeek(), 812);
    QCOMPARE(vm.statStudents(), 3450);
    QCOMPARE(vm.hourlyModel()->rowCount(), 2);
    QCOMPARE(vm.departmentModel()->rowCount(), 2);
    QVERIFY(vm.errorText().isEmpty());
}

void TestDashboardViewModel::applySummaryDerivesPeak()
{
    DashboardViewModel vm;
    vm.applySummary(R"({
        "status":"success","today":1,"week":1,"students":1,
        "hourly":[{"hour":8,"count":12},{"hour":10,"count":41},{"hour":9,"count":34}]
    })");
    // Max count is 41 at hour 10, which is index 1 in array order.
    QCOMPARE(vm.peakHourIndex(), 1);
    QCOMPARE(vm.peakHourLabel(), QStringLiteral("10 AM"));
}

void TestDashboardViewModel::applyInvalidSetsErrorText()
{
    DashboardViewModel vm;
    vm.applySummary(R"({"status":"error"})");
    QVERIFY(!vm.errorText().isEmpty());
    QCOMPARE(vm.peakHourIndex(), -1);
}

QTEST_MAIN(TestDashboardViewModel)
#include "tst_dashboardviewmodel.moc"
```

- [ ] **Step 2: Create `qt-app/quick/viewmodels/DashboardViewModel.h`:**

```cpp
#ifndef DASHBOARDVIEWMODEL_H
#define DASHBOARDVIEWMODEL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <qqml.h>
#include "BarsModel.h"

class QNetworkAccessManager;

// Dashboard screen's only QML-facing object (spec §6.1/§7.3). Fetches
// dashboard_summary.php once on navigation (manual refresh available; no
// polling). Peak hour is derived here from the hourly array (spec §5.1).
class DashboardViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int statToday READ statToday NOTIFY dataChanged)
    Q_PROPERTY(int statWeek READ statWeek NOTIFY dataChanged)
    Q_PROPERTY(int statStudents READ statStudents NOTIFY dataChanged)
    Q_PROPERTY(QString peakHourLabel READ peakHourLabel NOTIFY dataChanged)
    Q_PROPERTY(int peakHourIndex READ peakHourIndex NOTIFY dataChanged)
    Q_PROPERTY(BarsModel *hourlyModel READ hourlyModel CONSTANT)
    Q_PROPERTY(BarsModel *departmentModel READ departmentModel CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    explicit DashboardViewModel(QObject *parent = nullptr);

    int statToday() const { return m_today; }
    int statWeek() const { return m_week; }
    int statStudents() const { return m_students; }
    QString peakHourLabel() const { return m_peakHourLabel; }
    int peakHourIndex() const { return m_peakHourIndex; }
    BarsModel *hourlyModel() { return &m_hourly; }
    BarsModel *departmentModel() { return &m_department; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    Q_INVOKABLE void refresh();

    // Network-free seam (tests + reply handler).
    void applySummary(const QByteArray &raw);

    // Pure: 0..23 -> "8 AM" / "2 PM"; out-of-range -> "—".
    static QString formatPeakHour(int hour);

signals:
    void dataChanged();
    void loadingChanged();
    void errorTextChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);

    QNetworkAccessManager *m_nam = nullptr;
    BarsModel m_hourly;
    BarsModel m_department;
    int m_today = 0;
    int m_week = 0;
    int m_students = 0;
    QString m_peakHourLabel = QStringLiteral("—");
    int m_peakHourIndex = -1;
    bool m_loading = false;
    QString m_errorText;
};

#endif // DASHBOARDVIEWMODEL_H
```

- [ ] **Step 3: Create `qt-app/quick/viewmodels/DashboardViewModel.cpp`:**

```cpp
#include "DashboardViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "apiconfig.h"
#include "dashboardparser.h"

DashboardViewModel::DashboardViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString DashboardViewModel::formatPeakHour(int hour)
{
    if (hour < 0 || hour > 23)
        return QStringLiteral("—");
    const int h12 = (hour % 12 == 0) ? 12 : (hour % 12);
    const QString ap = hour < 12 ? QStringLiteral("AM") : QStringLiteral("PM");
    return QStringLiteral("%1 %2").arg(h12).arg(ap);
}

void DashboardViewModel::refresh()
{
    setError(QString());
    setLoading(true);
    QNetworkReply *reply = m_nam->get(
        QNetworkRequest(ApiConfig::endpoint(QStringLiteral("dashboard_summary.php"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool netErr = reply->error() != QNetworkReply::NoError;
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        setLoading(false);
        if (netErr) {
            setError(QStringLiteral("Network error. Please try again."));
            return;
        }
        applySummary(body);
    });
}

void DashboardViewModel::applySummary(const QByteArray &raw)
{
    DashboardSummary s;
    QString err;
    if (!DashboardParser::parse(raw, s, err)) {
        setError(err);
        m_peakHourIndex = -1;
        m_peakHourLabel = QStringLiteral("—");
        emit dataChanged();
        return;
    }

    m_today = s.today;
    m_week = s.week;
    m_students = s.students;

    QList<BarsModel::Bar> hourly;
    hourly.reserve(s.hourly.size());
    int peakIdx = -1;
    int peakCount = -1;
    int peakHour = -1;
    for (int i = 0; i < s.hourly.size(); ++i) {
        const HourBucket &h = s.hourly.at(i);
        hourly.append({ formatPeakHour(h.hour), double(h.count) });
        if (h.count > peakCount) {
            peakCount = h.count;
            peakIdx = i;
            peakHour = h.hour;
        }
    }
    m_hourly.setBars(hourly);
    m_peakHourIndex = peakIdx;
    m_peakHourLabel = formatPeakHour(peakHour);

    QList<BarsModel::Bar> depts;
    depts.reserve(s.departments.size());
    for (const DeptBucket &d : s.departments)
        depts.append({ d.name, double(d.count) });
    m_department.setBars(depts);

    setError(QString());
    emit dataChanged();
}

void DashboardViewModel::setLoading(bool v)
{
    if (m_loading == v)
        return;
    m_loading = v;
    emit loadingChanged();
}

void DashboardViewModel::setError(const QString &e)
{
    if (m_errorText == e)
        return;
    m_errorText = e;
    emit errorTextChanged();
}
```

- [ ] **Step 4: Register VM + test in `qt-app/quick/CMakeLists.txt`.** Add to the module `SOURCES` list (after the `viewmodels/GuestViewModel` line):

```cmake
        viewmodels/DashboardViewModel.h viewmodels/DashboardViewModel.cpp
```

Append a test target:

```cmake
# --- DashboardViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_dashboardviewmodel
    SOURCES tests/tst_dashboardviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)
```

- [ ] **Step 5: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_dashboardviewmodel --output-on-failure`
Expected: PASS (4 functions).

- [ ] **Step 6: Commit** — via the `commit` skill, message:
`feat(loams2): add DashboardViewModel (Phase 3)`

---

## Task 7: `VisitLogsViewModel` + `VisitLogRowsModel`

**Files:**
- Create: `qt-app/quick/models/VisitLogRowsModel.h`
- Create: `qt-app/quick/models/VisitLogRowsModel.cpp`
- Create: `qt-app/quick/viewmodels/VisitLogsViewModel.h`
- Create: `qt-app/quick/viewmodels/VisitLogsViewModel.cpp`
- Create: `qt-app/quick/tests/tst_visitlogsviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Consumes: `VisitLogParser::parse` + `StudentVisitRecord` (Task 4); `VisitorController::parseVisitorsResponse` + `VisitorRecord` (`qt-app/core/visitorcontroller.h:31`, `visitordata.h`); `HttpForm::submit` + `ApiConfig::endpoint`.
- Produces: `VisitLogRowsModel` (roles `date`/`name`/`course`/`department`/`timeIn`/`timeOut`/`company`/`purpose`). `VisitLogsViewModel` with `Mode { Student, Guest }` (Q_ENUM), `Range { Today, Week }` (Q_ENUM), `mode`/`range` (read+write), `count` (int), `rangeLabel` (QString), `rows` (`VisitLogRowsModel*`), `loading`/`errorText`; `Q_INVOKABLE void refresh()`; seams `applyStudentVisits(QByteArray)` / `applyGuestVisits(QByteArray)`; pure static `QString formatWeekLabel(const QDate &monday)`.

- [ ] **Step 1: Write the failing test** — `qt-app/quick/tests/tst_visitlogsviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QDate>
#include "VisitLogsViewModel.h"
#include "VisitLogRowsModel.h"

class TestVisitLogsViewModel : public QObject
{
    Q_OBJECT
private slots:
    void defaultsToStudentToday();
    void applyStudentVisitsPopulatesRowsWithDashTimeOut();
    void applyGuestVisitsPopulatesGuestColumns();
    void applyInvalidSetsErrorText();
    void setModeEmitsAndChanges();
    void formatWeekLabelSpansMonToSun();
};

void TestVisitLogsViewModel::defaultsToStudentToday()
{
    VisitLogsViewModel vm;
    QCOMPARE(vm.mode(), VisitLogsViewModel::Student);
    QCOMPARE(vm.range(), VisitLogsViewModel::Today);
}

void TestVisitLogsViewModel::applyStudentVisitsPopulatesRowsWithDashTimeOut()
{
    VisitLogsViewModel vm;
    QSignalSpy spy(&vm, &VisitLogsViewModel::dataChanged);
    vm.applyStudentVisits(R"({
        "status":"success","count":1,
        "visits":[{"date":"2026-07-13","name":"Maria Santos","course":"BSCE",
                   "department":"CE","time_in":"08:14"}]
    })");
    QVERIFY(spy.count() >= 1);
    QCOMPARE(vm.count(), 1);
    VisitLogRowsModel *rows = vm.rows();
    const QModelIndex i0 = rows->index(0, 0);
    QCOMPARE(rows->data(i0, VisitLogRowsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::CourseRole).toString(), QStringLiteral("BSCE"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::TimeInRole).toString(), QStringLiteral("08:14"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::TimeOutRole).toString(), QStringLiteral("—"));
    QVERIFY(vm.errorText().isEmpty());
}

void TestVisitLogsViewModel::applyGuestVisitsPopulatesGuestColumns()
{
    VisitLogsViewModel vm;
    vm.setMode(VisitLogsViewModel::Guest);
    vm.applyGuestVisits(R"({
        "status":"success","count":1,
        "visitors":[{"name":"Ana Cruz","company":"Acme","purpose":"Research",
                     "time_in":"2026-07-13 09:20:00"}]
    })");
    QCOMPARE(vm.count(), 1);
    VisitLogRowsModel *rows = vm.rows();
    const QModelIndex i0 = rows->index(0, 0);
    QCOMPARE(rows->data(i0, VisitLogRowsModel::NameRole).toString(), QStringLiteral("Ana Cruz"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::CompanyRole).toString(), QStringLiteral("Acme"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::PurposeRole).toString(), QStringLiteral("Research"));
}

void TestVisitLogsViewModel::applyInvalidSetsErrorText()
{
    VisitLogsViewModel vm;
    vm.applyStudentVisits(R"({"status":"error"})");
    QVERIFY(!vm.errorText().isEmpty());
    QCOMPARE(vm.count(), 0);
}

void TestVisitLogsViewModel::setModeEmitsAndChanges()
{
    VisitLogsViewModel vm;
    QSignalSpy spy(&vm, &VisitLogsViewModel::modeChanged);
    vm.setMode(VisitLogsViewModel::Guest);
    QCOMPARE(vm.mode(), VisitLogsViewModel::Guest);
    QVERIFY(spy.count() >= 1);
}

void TestVisitLogsViewModel::formatWeekLabelSpansMonToSun()
{
    // 2026-07-13 is a Monday.
    QCOMPARE(VisitLogsViewModel::formatWeekLabel(QDate(2026, 7, 13)),
             QStringLiteral("Jul 13 – Jul 19, 2026"));
}

QTEST_MAIN(TestVisitLogsViewModel)
#include "tst_visitlogsviewmodel.moc"
```

- [ ] **Step 2: Create `qt-app/quick/models/VisitLogRowsModel.h`:**

```cpp
#ifndef VISITLOGROWSMODEL_H
#define VISITLOGROWSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

// Visit Logs row model (spec §6.3/§7.3). Roles are the UNION of the student
// and guest column sets; unused roles are empty per mode. The screen selects
// which columns to show from VisitLogsViewModel.mode.
class VisitLogRowsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        DateRole = Qt::UserRole + 1,
        NameRole,
        CourseRole,
        DepartmentRole,
        TimeInRole,
        TimeOutRole,
        CompanyRole,
        PurposeRole,
    };
    struct Row {
        QString date, name, course, department, timeIn, timeOut, company, purpose;
    };

    explicit VisitLogRowsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QList<Row> &rows);

private:
    QList<Row> m_rows;
};

#endif // VISITLOGROWSMODEL_H
```

- [ ] **Step 3: Create `qt-app/quick/models/VisitLogRowsModel.cpp`:**

```cpp
#include "VisitLogRowsModel.h"

VisitLogRowsModel::VisitLogRowsModel(QObject *parent) : QAbstractListModel(parent) {}

int VisitLogRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant VisitLogRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case DateRole:       return r.date;
    case NameRole:       return r.name;
    case CourseRole:     return r.course;
    case DepartmentRole: return r.department;
    case TimeInRole:     return r.timeIn;
    case TimeOutRole:    return r.timeOut;
    case CompanyRole:    return r.company;
    case PurposeRole:    return r.purpose;
    default:             return {};
    }
}

QHash<int, QByteArray> VisitLogRowsModel::roleNames() const
{
    return {
        { DateRole, "date" }, { NameRole, "name" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { TimeInRole, "timeIn" },
        { TimeOutRole, "timeOut" }, { CompanyRole, "company" }, { PurposeRole, "purpose" },
    };
}

void VisitLogRowsModel::setRows(const QList<Row> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}
```

- [ ] **Step 4: Create `qt-app/quick/viewmodels/VisitLogsViewModel.h`:**

```cpp
#ifndef VISITLOGSVIEWMODEL_H
#define VISITLOGSVIEWMODEL_H

#include <QByteArray>
#include <QDate>
#include <QObject>
#include <QString>
#include <qqml.h>
#include "VisitLogRowsModel.h"

class QNetworkAccessManager;

// Visit Logs screen VM (spec §6.3/§7.3). Student attendance is the primary
// view (get_library_visits.php); Guest is a secondary toggle (get_visitors.php
// reused, driven only with app-generated ISO dates, §5.4).
class VisitLogsViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(Range range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(int count READ count NOTIFY dataChanged)
    Q_PROPERTY(QString rangeLabel READ rangeLabel NOTIFY dataChanged)
    Q_PROPERTY(VisitLogRowsModel *rows READ rows CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    enum Mode { Student, Guest };
    Q_ENUM(Mode)
    enum Range { Today, Week };
    Q_ENUM(Range)

    explicit VisitLogsViewModel(QObject *parent = nullptr);

    Mode mode() const { return m_mode; }
    Range range() const { return m_range; }
    int count() const { return m_count; }
    QString rangeLabel() const { return m_rangeLabel; }
    VisitLogRowsModel *rows() { return &m_rows; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    void setMode(Mode m);
    void setRange(Range r);

    Q_INVOKABLE void refresh();

    // Network-free seams (tests + reply handlers).
    void applyStudentVisits(const QByteArray &raw);
    void applyGuestVisits(const QByteArray &raw);

    // Pure: Monday date -> "Jul 13 – Jul 19, 2026".
    static QString formatWeekLabel(const QDate &monday);

signals:
    void modeChanged();
    void rangeChanged();
    void dataChanged();
    void loadingChanged();
    void errorTextChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);
    void setCount(int c);
    QString computeRangeLabel() const;   // uses current date + m_range

    QNetworkAccessManager *m_nam = nullptr;
    VisitLogRowsModel m_rows;
    Mode m_mode = Student;
    Range m_range = Today;
    int m_count = 0;
    QString m_rangeLabel;
    bool m_loading = false;
    QString m_errorText;
};

#endif // VISITLOGSVIEWMODEL_H
```

- [ ] **Step 5: Create `qt-app/quick/viewmodels/VisitLogsViewModel.cpp`:**

```cpp
#include "VisitLogsViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include "apiconfig.h"
#include "HttpForm.h"
#include "visitlogparser.h"
#include "visitorcontroller.h"
#include "visitordata.h"

VisitLogsViewModel::VisitLogsViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_rangeLabel = computeRangeLabel();
}

QString VisitLogsViewModel::formatWeekLabel(const QDate &monday)
{
    const QDate sunday = monday.addDays(6);
    return QStringLiteral("%1 – %2")
        .arg(monday.toString(QStringLiteral("MMM d")),
             sunday.toString(QStringLiteral("MMM d, yyyy")));
}

QString VisitLogsViewModel::computeRangeLabel() const
{
    const QDate today = QDate::currentDate();
    if (m_range == Week) {
        // Qt: dayOfWeek() Monday==1. Step back to this week's Monday.
        const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
        return formatWeekLabel(monday);
    }
    return QStringLiteral("Today, %1").arg(today.toString(QStringLiteral("MMM d, yyyy")));
}

void VisitLogsViewModel::setMode(Mode m)
{
    if (m_mode == m)
        return;
    m_mode = m;
    emit modeChanged();
    refresh();
}

void VisitLogsViewModel::setRange(Range r)
{
    if (m_range == r)
        return;
    m_range = r;
    emit rangeChanged();
    refresh();
}

void VisitLogsViewModel::refresh()
{
    setError(QString());
    setLoading(true);

    if (m_mode == Student) {
        QUrl url = ApiConfig::endpoint(QStringLiteral("get_library_visits.php"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("range"),
                       m_range == Week ? QStringLiteral("week") : QStringLiteral("today"));
        url.setQuery(q);
        QNetworkReply *reply = m_nam->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            const bool netErr = reply->error() != QNetworkReply::NoError;
            const QByteArray body = reply->readAll();
            reply->deleteLater();
            setLoading(false);
            if (netErr) { setError(QStringLiteral("Network error. Please try again.")); return; }
            applyStudentVisits(body);
        });
        return;
    }

    // Guest (§5.4): POST get_visitors.php with APP-GENERATED ISO dates only —
    // never free-text on the date path. range -> date_type + start/end.
    const QDate today = QDate::currentDate();
    QString dateType, startDate, endDate;
    if (m_range == Week) {
        const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
        dateType  = QStringLiteral("date range");
        startDate = monday.toString(Qt::ISODate);
        endDate   = monday.addDays(6).toString(Qt::ISODate);
    } else {
        dateType  = QStringLiteral("specific day");
        startDate = today.toString(Qt::ISODate);
        endDate   = today.toString(Qt::ISODate);
    }
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("date_type"), dateType},
        {QStringLiteral("start_date"), startDate},
        {QStringLiteral("end_date"), endDate},
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("get_visitors.php")),
                     fields, this,
        [this](const QByteArray &body) { setLoading(false); applyGuestVisits(body); },
        [this]() { setLoading(false); setError(QStringLiteral("Network error. Please try again.")); });
}

void VisitLogsViewModel::applyStudentVisits(const QByteArray &raw)
{
    QList<StudentVisitRecord> parsed;
    int count = 0;
    QString err;
    if (!VisitLogParser::parse(raw, parsed, count, err)) {
        setError(err);
        m_rows.setRows({});
        setCount(0);
        emit dataChanged();
        return;
    }
    QList<VisitLogRowsModel::Row> rows;
    rows.reserve(parsed.size());
    for (const StudentVisitRecord &r : parsed) {
        VisitLogRowsModel::Row row;
        row.date       = r.date;
        row.name       = r.name;
        row.course     = r.course;
        row.department = r.department;
        row.timeIn     = r.timeIn;
        row.timeOut    = QStringLiteral("—");   // login-only (§6.3)
        rows.append(row);
    }
    m_rows.setRows(rows);
    setCount(rows.size());
    m_rangeLabel = computeRangeLabel();
    setError(QString());
    emit dataChanged();
}

void VisitLogsViewModel::applyGuestVisits(const QByteArray &raw)
{
    QList<VisitorRecord> parsed;
    int total = 0;
    QString err;
    if (!VisitorController::parseVisitorsResponse(raw, &parsed, &total, &err)) {
        setError(err.isEmpty() ? QStringLiteral("Could not load guest logs.") : err);
        m_rows.setRows({});
        setCount(0);
        emit dataChanged();
        return;
    }
    QList<VisitLogRowsModel::Row> rows;
    rows.reserve(parsed.size());
    for (const VisitorRecord &v : parsed) {
        VisitLogRowsModel::Row row;
        row.name    = v.name;
        row.company = v.company;
        row.purpose = v.purpose;
        row.date    = v.date;
        row.timeIn  = v.time;
        // timeOut/course/department stay empty — not in the guest column set.
        rows.append(row);
    }
    m_rows.setRows(rows);
    setCount(rows.size());
    m_rangeLabel = computeRangeLabel();
    setError(QString());
    emit dataChanged();
}

void VisitLogsViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void VisitLogsViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}

void VisitLogsViewModel::setCount(int c)
{
    m_count = c;   // emitted via dataChanged by callers
}
```

> **Note on `VisitorController::parseVisitorsResponse` signature** — confirm it against `qt-app/core/visitorcontroller.h:31-34`: `static bool parseVisitorsResponse(const QByteArray &json, QList<VisitorRecord> *out, int *totalCount, QString *errorMsg)`. `VisitorRecord` fields are `name/company/purpose/date/time` (`visitordata.h:7-14`). If the fixture in Step 1 does not populate `date`/`time` as expected, adjust the fixture to whatever `parseVisitorsResponse` actually reads from `time_in` — the guest parser is reused verbatim, so the test must match its real behavior.

- [ ] **Step 6: Register in `qt-app/quick/CMakeLists.txt`.** Add to module `SOURCES`:

```cmake
        models/VisitLogRowsModel.h models/VisitLogRowsModel.cpp
        viewmodels/VisitLogsViewModel.h viewmodels/VisitLogsViewModel.cpp
```

Append the test target (VM pulls `witscore` via the module, so link `Network`):

```cmake
# --- VisitLogsViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_visitlogsviewmodel
    SOURCES tests/tst_visitlogsviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)
```

- [ ] **Step 7: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_visitlogsviewmodel --output-on-failure`
Expected: PASS (6 functions). If the guest test fails on `date`/`time`, reconcile the fixture with `parseVisitorsResponse` per the note above, then re-run.

- [ ] **Step 8: Commit** — via the `commit` skill, message:
`feat(loams2): add VisitLogsViewModel + row model (Phase 3)`

---

## Task 8: `SearchViewModel` + `SearchResultsModel`

**Files:**
- Create: `qt-app/quick/models/SearchResultsModel.h`
- Create: `qt-app/quick/models/SearchResultsModel.cpp`
- Create: `qt-app/quick/viewmodels/SearchViewModel.h`
- Create: `qt-app/quick/viewmodels/SearchViewModel.cpp`
- Create: `qt-app/quick/tests/tst_searchviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Consumes: `StudentController` (`searchStudents`, `searchFinished`/`searchFailed`, `loadCourses`/`coursesLoaded`, `qt-app/core/studentcontroller.h`); `StudentRecord` incl. `visits` (Task 2).
- Produces: `SearchResultsModel` (roles incl. `visits` → rendered as "Total Visits: N"). `SearchViewModel` with `results` (`SearchResultsModel*`), `courses` (QStringList), `loading`/`errorText`; `Q_INVOKABLE void search(const QString &search, const QString &course)`; public slots `onSearchFinished`/`onSearchFailed`/`onCoursesLoaded` (network-free test seams).

- [ ] **Step 1: Write the failing test** — `qt-app/quick/tests/tst_searchviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "SearchViewModel.h"
#include "SearchResultsModel.h"
#include "studentdata.h"

class TestSearchViewModel : public QObject
{
    Q_OBJECT
private slots:
    void resultsPopulateWithVisits();
    void searchFailedSetsErrorText();
    void coursesLoadedExposesChips();
};

void TestSearchViewModel::resultsPopulateWithVisits()
{
    SearchViewModel vm;
    StudentRecord r;
    r.schoolId = "2023-0001";
    r.name = "Maria Santos";
    r.course = "BSCE";
    r.department = "CE";
    r.visits = 42;
    vm.onSearchFinished(SearchOutcome::Results, { r }, QString(), QStringLiteral("Maria"));

    QCOMPARE(vm.results()->rowCount(), 1);
    const QModelIndex i0 = vm.results()->index(0, 0);
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::VisitsRole).toInt(), 42);
    QVERIFY(vm.errorText().isEmpty());
}

void TestSearchViewModel::searchFailedSetsErrorText()
{
    SearchViewModel vm;
    vm.onSearchFailed(QStringLiteral("boom"));
    QVERIFY(!vm.errorText().isEmpty());
    // The user-facing string must not leak the raw transport error.
    QVERIFY(!vm.errorText().contains(QStringLiteral("boom")));
}

void TestSearchViewModel::coursesLoadedExposesChips()
{
    SearchViewModel vm;
    QSignalSpy spy(&vm, &SearchViewModel::coursesChanged);
    vm.onCoursesLoaded({ QStringLiteral("BSCE"), QStringLiteral("BSEE") });
    QVERIFY(spy.count() >= 1);
    QCOMPARE(vm.courses().size(), 2);
    QCOMPARE(vm.courses().at(0), QStringLiteral("BSCE"));
}

QTEST_MAIN(TestSearchViewModel)
#include "tst_searchviewmodel.moc"
```

- [ ] **Step 2: Create `qt-app/quick/models/SearchResultsModel.h`:**

```cpp
#ifndef SEARCHRESULTSMODEL_H
#define SEARCHRESULTSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "studentdata.h"

// Search results row model (spec §6.2). The `visits` role is rendered by the
// result card as the explicit "Total Visits: N" label (spec §5.3 layer 4).
class SearchResultsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SchoolIdRole,
        CourseRole,
        DepartmentRole,
        YearLevelRole,
        StatusRole,
        VisitsRole,
    };

    explicit SearchResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRecords(const QList<StudentRecord> &records);

private:
    QList<StudentRecord> m_records;
};

#endif // SEARCHRESULTSMODEL_H
```

- [ ] **Step 3: Create `qt-app/quick/models/SearchResultsModel.cpp`:**

```cpp
#include "SearchResultsModel.h"

SearchResultsModel::SearchResultsModel(QObject *parent) : QAbstractListModel(parent) {}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};
    const StudentRecord &r = m_records.at(index.row());
    switch (role) {
    case NameRole:       return r.name;
    case SchoolIdRole:   return r.schoolId;
    case CourseRole:     return r.course;
    case DepartmentRole: return r.department;
    case YearLevelRole:  return r.yearLevel;
    case StatusRole:     return r.status;
    case VisitsRole:     return r.visits;
    default:             return {};
    }
}

QHash<int, QByteArray> SearchResultsModel::roleNames() const
{
    return {
        { NameRole, "name" }, { SchoolIdRole, "schoolId" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { YearLevelRole, "yearLevel" },
        { StatusRole, "status" }, { VisitsRole, "visits" },
    };
}

void SearchResultsModel::setRecords(const QList<StudentRecord> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
}
```

- [ ] **Step 4: Create `qt-app/quick/viewmodels/SearchViewModel.h`:**

```cpp
#ifndef SEARCHVIEWMODEL_H
#define SEARCHVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <qqml.h>
#include "SearchResultsModel.h"
#include "studentdata.h"

class QNetworkAccessManager;
class StudentController;

// Search screen VM (spec §6.2). Wraps StudentController: no new query endpoint,
// but exposes the lifetime `visits` count per result as "Total Visits: N".
class SearchViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(SearchResultsModel *results READ results CONSTANT)
    Q_PROPERTY(QStringList courses READ courses NOTIFY coursesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    explicit SearchViewModel(QObject *parent = nullptr);

    SearchResultsModel *results() { return &m_results; }
    QStringList courses() const { return m_courses; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    // Empty course => no course filter. department is left empty in Phase 3.
    Q_INVOKABLE void search(const QString &search, const QString &course);

public slots:
    // Wired to StudentController; public so tests drive them network-free.
    void onSearchFinished(SearchOutcome outcome,
                          const QList<StudentRecord> &records,
                          const QString &message,
                          const QString &searchTerm);
    void onSearchFailed(const QString &errorString);
    void onCoursesLoaded(const QStringList &courses);

signals:
    void coursesChanged();
    void loadingChanged();
    void errorTextChanged();
    void resultsChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);

    QNetworkAccessManager *m_nam = nullptr;
    StudentController *m_controller = nullptr;
    SearchResultsModel m_results;
    QStringList m_courses;
    bool m_loading = false;
    QString m_errorText;
};

#endif // SEARCHVIEWMODEL_H
```

- [ ] **Step 5: Create `qt-app/quick/viewmodels/SearchViewModel.cpp`:**

```cpp
#include "SearchViewModel.h"

#include <QNetworkAccessManager>
#include "studentcontroller.h"

SearchViewModel::SearchViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new StudentController(m_nam, this))
{
    connect(m_controller, &StudentController::searchFinished,
            this, &SearchViewModel::onSearchFinished);
    connect(m_controller, &StudentController::searchFailed,
            this, &SearchViewModel::onSearchFailed);
    connect(m_controller, &StudentController::coursesLoaded,
            this, &SearchViewModel::onCoursesLoaded);
    m_controller->loadCourses(QString());   // populate the filter chips
}

void SearchViewModel::search(const QString &search, const QString &course)
{
    setError(QString());
    setLoading(true);
    m_controller->searchStudents(search, QString(), course);
}

void SearchViewModel::onSearchFinished(SearchOutcome outcome,
                                       const QList<StudentRecord> &records,
                                       const QString &message,
                                       const QString &searchTerm)
{
    Q_UNUSED(searchTerm)
    setLoading(false);
    m_results.setRecords(records);
    emit resultsChanged();
    if (outcome == SearchOutcome::InvalidResponse) {
        setError(QStringLiteral("Invalid server response."));
    } else if (outcome == SearchOutcome::NotSuccess) {
        setError(message.isEmpty() ? QStringLiteral("No students found.") : message);
    } else {
        setError(QString());   // Results or Empty — empty state handled by the screen
    }
}

void SearchViewModel::onSearchFailed(const QString & /*errorString*/)
{
    // Do NOT surface the raw transport string (security-hygiene).
    setLoading(false);
    setError(QStringLiteral("Network error. Please try again."));
}

void SearchViewModel::onCoursesLoaded(const QStringList &courses)
{
    m_courses = courses;
    emit coursesChanged();
}

void SearchViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void SearchViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}
```

- [ ] **Step 6: Register in `qt-app/quick/CMakeLists.txt`.** Add to module `SOURCES`:

```cmake
        models/SearchResultsModel.h models/SearchResultsModel.cpp
        viewmodels/SearchViewModel.h viewmodels/SearchViewModel.cpp
```

Append the test target:

```cmake
# --- SearchViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_searchviewmodel
    SOURCES tests/tst_searchviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)
```

- [ ] **Step 7: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_searchviewmodel --output-on-failure`
Expected: PASS (3 functions).

- [ ] **Step 8: Commit** — via the `commit` skill, message:
`feat(loams2): add SearchViewModel + results model (Phase 3)`

---

## Task 9: `LSideNav` build-out

**Files:**
- Modify: `qt-app/quick/qml/components/LSideNav.qml`
- Test: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Produces: `LSideNav` with `items` = list of `{page, label, enabled}`, `currentPage` (matched against `page`), `signal pageActivated(var page)`, `footer` (an `Item` slot at the bottom). Active row: soft brand highlight + gold dot; disabled rows: muted + a "soon" badge, no click. Consumed by `AdminScreen.qml` (Task 16).

- [ ] **Step 1: Add the failing assertions** — in `tst_qml_components.qml`, replace the `LSideNav { id: sn }` line with a configured instance carrying items, and add a `TestCase` after the existing one:

```qml
    LSideNav {
        id: sn
        width: 240; height: 400
        currentPage: "dashboard"
        items: [
            { page: "dashboard", label: "Dashboard", enabled: true },
            { page: "search",    label: "Search",    enabled: true },
            { page: "database",  label: "Database",  enabled: false }
        ]
    }

    TestCase {
        name: "LSideNavInteraction"
        when: windowShown

        function test_activeItemMatchesCurrentPage() {
            compare(sn.currentPage, "dashboard");
        }
        function test_emitsPageActivatedForEnabledItem() {
            var got = null;
            sn.pageActivated.connect(function(p) { got = p; });
            sn.activate("search");            // test hook: same path a click takes
            compare(got, "search");
        }
        function test_disabledItemDoesNotActivate() {
            var count = 0;
            sn.pageActivated.connect(function(p) { count++; });
            sn.activate("database");          // disabled -> ignored
            compare(count, 0);
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: FAIL — `sn.activate` is not a function / `pageActivated` undefined.

- [ ] **Step 3: Implement `LSideNav.qml`:**

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Persistent left sidebar (§11). Brand-gradient background reads the brand
// roles directly (§12.5). items: list of {page, label, enabled}.
Rectangle {
    id: nav
    property var items: []
    property string currentPage: ""
    property Item footer: null
    signal pageActivated(var page)

    // Test/logic hook: the single path both a click and a test call take, so
    // the disabled-guard lives in exactly one place.
    function activate(page) {
        for (var i = 0; i < items.length; i++) {
            if (items[i].page === page) {
                if (items[i].enabled === false)
                    return;
                nav.pageActivated(page);
                return;
            }
        }
    }

    color: Theme.brand.admin
    implicitWidth: 240
    implicitHeight: 600

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.lg
        spacing: Theme.spacing.sm

        Repeater {
            model: nav.items
            delegate: Rectangle {
                id: row
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 40
                radius: Theme.radius.sm2
                readonly property bool isActive: row.modelData.page === nav.currentPage
                readonly property bool isEnabled: row.modelData.enabled !== false
                color: isActive ? Qt.alpha(Theme.brand.onPrimary, 0.14) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.md
                    anchors.rightMargin: Theme.spacing.md
                    spacing: Theme.spacing.sm

                    // Gold active-dot.
                    Rectangle {
                        implicitWidth: 6; implicitHeight: 6; radius: 3
                        visible: row.isActive
                        color: Theme.secondary
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.label !== undefined ? row.modelData.label : row.modelData
                        color: row.isEnabled ? Theme.brand.onPrimary
                                             : Qt.alpha(Theme.brand.onPrimary, 0.45)
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        Accessible.role: Accessible.ListItem
                    }
                    // "soon" badge for disabled (Phase 4) items.
                    Text {
                        visible: !row.isEnabled
                        text: qsTr("soon")
                        color: Qt.alpha(Theme.brand.onPrimary, 0.55)
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: row.isEnabled
                    cursorShape: row.isEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: nav.activate(row.modelData.page)
                }
            }
        }

        Item { Layout.fillHeight: true }   // push footer to the bottom

        // Footer slot (e.g. Back-to-Kiosk), reparented into the column.
        Item {
            Layout.fillWidth: true
            implicitHeight: nav.footer ? nav.footer.implicitHeight : 0
            children: nav.footer ? [nav.footer] : []
        }
    }

    Accessible.role: Accessible.List
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): build out LSideNav (active state, soon items, footer) (Phase 3)`

---

## Task 10: `LTable` build-out

**Files:**
- Modify: `qt-app/quick/qml/components/LTable.qml`
- Test: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Produces: `LTable` with `columns` = list of `{key, title}` (optional `weight`, default 1), `model` = a `QAbstractListModel` (or QML `ListModel`) whose role names match column `key`s, `emptyStateText`. Renders a header row of titles, body rows reading `model[key]` per column, per-row hover highlight, and the empty state when the row count is 0. Consumed by `VisitLogsScreen` (Task 15).

- [ ] **Step 1: Add failing assertions** — in `tst_qml_components.qml`, replace `LTable { id: t }` with a configured instance + a fixture model, and add a `TestCase`:

```qml
    ListModel {
        id: tableFixture
        ListElement { date: "2026-07-13"; name: "Maria Santos"; timeIn: "08:14" }
        ListElement { date: "2026-07-13"; name: "Jose Ramirez"; timeIn: "08:31" }
    }
    LTable {
        id: t
        width: 480; height: 240
        columns: [
            { key: "date",   title: "Date" },
            { key: "name",   title: "Name" },
            { key: "timeIn", title: "Time In" }
        ]
        model: tableFixture
    }

    TestCase {
        name: "LTableRendersRows"
        when: windowShown
        function test_rowCountMatchesModel() {
            compare(t.rowCount, 2);
        }
        function test_showsEmptyStateWhenNoRows() {
            t.model = null;
            compare(t.rowCount, 0);
            verify(t.emptyVisible === true);
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: FAIL — `t.rowCount` / `t.emptyVisible` undefined.

- [ ] **Step 3: Implement `LTable.qml`:**

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Row-grid primitive (§11). columns: list of {key, title, weight?}; model:
// a QAbstractListModel whose roles match the column keys.
Rectangle {
    id: table
    property var columns: []
    property var model: null
    property bool selectable: false
    property string emptyStateText: qsTr("No records")

    // Introspection for tests + the empty state.
    readonly property int rowCount: list.count
    readonly property bool emptyVisible: list.count === 0

    color: Theme.card
    radius: Theme.radius.card
    border.width: 2
    border.color: Theme.border
    implicitWidth: 320
    implicitHeight: 200
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 36
            color: Theme.tableHeaderBg
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing.lg
                anchors.rightMargin: Theme.spacing.lg
                spacing: Theme.spacing.md
                Repeater {
                    model: table.columns
                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: modelData.weight !== undefined ? modelData.weight : 1
                        text: modelData.title !== undefined ? modelData.title : ""
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Body.
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: table.model
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: rowDelegate
                required property var model
                required property int index
                width: ListView.view ? ListView.view.width : 0
                implicitHeight: 40
                color: hover.hovered ? Qt.alpha(Theme.brand.admin, 0.06)
                                     : (index % 2 === 0 ? Theme.card : Theme.rowHairline)

                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.lg
                    anchors.rightMargin: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Repeater {
                        model: table.columns
                        delegate: Text {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: modelData.weight !== undefined ? modelData.weight : 1
                            text: {
                                var v = rowDelegate.model[modelData.key];
                                return v !== undefined && v !== null ? v : "";
                            }
                            color: Theme.text
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // Empty state.
    Text {
        anchors.centerIn: parent
        visible: table.emptyVisible
        text: table.emptyStateText
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
    }

    Accessible.role: Accessible.Table
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): build out LTable (columns, rows, hover, empty state) (Phase 3)`

---

## Task 11: `LBarChart` build-out (horizontal variant + per-bar labels)

**Files:**
- Modify: `qt-app/quick/qml/components/LBarChart.qml`
- Test: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Produces: `LBarChart` consuming a `BarsModel` via `model` (roles `label`/`value`); `orientation` = `"Vertical"` | `"Horizontal"`; `maxValue`; `barColor` (default `Theme.brand.admin`); `highlightIndex` (default −1) + `highlightColor` (default `Theme.secondary`) to gold-highlight one bar; per-bar `label` text. Consumed by `DashboardScreen` (Task 13).

- [ ] **Step 1: Add failing assertions** — in `tst_qml_components.qml`, replace `LBarChart { id: bc }` with a configured instance + fixture, and add a `TestCase`:

```qml
    ListModel {
        id: barsFixture
        ListElement { label: "8"; value: 12 }
        ListElement { label: "9"; value: 34 }
        ListElement { label: "10"; value: 41 }
    }
    LBarChart {
        id: bc
        width: 320; height: 160
        model: barsFixture
        maxValue: 41
        highlightIndex: 2
    }

    TestCase {
        name: "LBarChartRenders"
        when: windowShown
        function test_barCountMatchesModel() {
            compare(bc.barCount, 3);
        }
        function test_orientationDefaultsVertical() {
            compare(bc.orientation, "Vertical");
        }
        function test_horizontalOrientationAccepted() {
            bc.orientation = "Horizontal";
            compare(bc.orientation, "Horizontal");
            compare(bc.barCount, 3);
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: FAIL — `bc.barCount` / `highlightIndex` undefined.

- [ ] **Step 3: Implement `LBarChart.qml`:**

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Custom bar visualization (§11) — NOT QtCharts (Risk 2). Consumes a BarsModel
// (roles label/value). Colors come from Theme (Global Constraints): all bars
// use barColor, except the bar at highlightIndex which uses highlightColor.
Item {
    id: chart
    property string orientation: "Vertical"     // Vertical | Horizontal
    property var model: null                     // BarsModel / ListModel {label,value}
    property real maxValue: 100
    property int barRadius: Theme.radius.xs
    property color barColor: Theme.brand.admin
    property color highlightColor: Theme.secondary
    property int highlightIndex: -1

    readonly property int barCount: repeaterV.model ? repeaterV.count
                                    : (repeaterH.model ? repeaterH.count : 0)

    implicitWidth: 320
    implicitHeight: 160

    function colorFor(i) { return i === chart.highlightIndex ? chart.highlightColor : chart.barColor }

    // --- Vertical (hourly): bottom-aligned columns + a label under each bar. ---
    RowLayout {
        id: vertical
        anchors.fill: parent
        visible: chart.orientation === "Vertical"
        spacing: Theme.spacing.sm
        Repeater {
            id: repeaterV
            model: chart.orientation === "Vertical" ? chart.model : null
            delegate: ColumnLayout {
                required property var model
                required property int index
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacing.xs
                Item { Layout.fillHeight: true }   // spacer so the bar sits at the bottom
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: chart.maxValue > 0
                        ? ((chart.height - 18) * (model.value / chart.maxValue)) : 0
                    radius: chart.barRadius
                    color: chart.colorFor(index)
                    Accessible.role: Accessible.StaticText
                    Accessible.name: (model.label !== undefined ? model.label : "") + " " + model.value
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: model.label !== undefined ? model.label : ""
                    color: Theme.mutedTextCaption
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                }
            }
        }
    }

    // --- Horizontal (departments): label + width-proportional track. ---
    ColumnLayout {
        id: horizontal
        anchors.fill: parent
        visible: chart.orientation === "Horizontal"
        spacing: Theme.spacing.sm
        Repeater {
            id: repeaterH
            model: chart.orientation === "Horizontal" ? chart.model : null
            delegate: RowLayout {
                required property var model
                required property int index
                Layout.fillWidth: true
                spacing: Theme.spacing.md
                Text {
                    Layout.preferredWidth: 80
                    text: model.label !== undefined ? model.label : ""
                    color: Theme.text
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                    elide: Text.ElideRight
                }
                Rectangle {   // track
                    Layout.fillWidth: true
                    implicitHeight: 14
                    radius: chart.barRadius
                    color: Qt.alpha(Theme.brand.admin, 0.10)
                    Rectangle {   // fill
                        height: parent.height
                        width: chart.maxValue > 0 ? (parent.width * (model.value / chart.maxValue)) : 0
                        radius: chart.barRadius
                        color: chart.colorFor(index)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: (model.label !== undefined ? model.label : "") + " " + model.value
                    }
                }
            }
        }
    }

    Accessible.role: Accessible.Pane
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): build out LBarChart (horizontal variant, labels, highlight) (Phase 3)`

---

## Task 12: `LPageHeader` build-out (subtitle AND clock together)

**Files:**
- Modify: `qt-app/quick/qml/components/LPageHeader.qml`
- Test: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Produces: `LPageHeader` showing `title`, `subtitle` (under the title) and a right-aligned `clockText` **simultaneously**. Consumed by `AdminScreen` (Task 16).

- [ ] **Step 1: Add failing assertions** — in `tst_qml_components.qml`, replace `LPageHeader{ id: ph; title: "P" }` with:

```qml
    LPageHeader {
        id: ph
        width: 480
        title: "Dashboard"
        subtitle: "Library activity overview"
        clockText: "8:04:11 AM"
    }
```

Add a `TestCase`:

```qml
    TestCase {
        name: "LPageHeaderShowsBoth"
        when: windowShown
        function test_subtitleAndClockBothPresent() {
            verify(findText(ph, "Library activity overview") !== null);
            verify(findText(ph, "8:04:11 AM") !== null);
        }
        function findText(root, s) {
            if (root.text !== undefined && root.text !== null && root.text.indexOf(s) !== -1)
                return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findText(root.children[i], s);
                if (f !== null) return f;
            }
            return null;
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: FAIL — the current header shows clock **or** subtitle, never both, so the subtitle node is absent.

- [ ] **Step 3: Implement `LPageHeader.qml`:**

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Admin page-header region (§11): title + subtitle on the left, live clock on
// the right — all shown together.
Item {
    id: hdr
    property string title: ""
    property string subtitle: ""
    property string clockText: ""
    property Item actions: null

    implicitHeight: 64
    implicitWidth: 480

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing.lg

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacing.xs
            Text {
                text: hdr.title
                color: Theme.text
                font.family: Theme.typography.serif
                font.pixelSize: Theme.typography.pageTitle
                font.weight: Font.Bold
                Accessible.role: Accessible.StaticText
            }
            Text {
                visible: hdr.subtitle.length > 0
                text: hdr.subtitle
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                Accessible.role: Accessible.StaticText
            }
        }

        Text {
            visible: hdr.clockText.length > 0
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            text: hdr.clockText
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.cardTitle
            Accessible.role: Accessible.StaticText
        }
    }
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): build out LPageHeader (subtitle + clock together) (Phase 3)`

---

## Task 13: `DashboardScreen.qml`

**Files:**
- Create: `qt-app/quick/qml/admin/DashboardScreen.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (`QML_FILES`)
- Create/Modify: `qt-app/quick/tests/tst_qml_admin.qml` + `qt-app/quick/tests/tst_qml_admin.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (test target — created here, reused by Tasks 14–16)

**Interfaces:**
- Consumes: a VM exposing `statToday`/`statWeek`/`statStudents` (int), `peakHourLabel` (string), `peakHourIndex` (int), `hourlyModel`/`departmentModel` (models with `maxValue`), `loading` (bool), `errorText` (string), `refresh()`.
- Produces: `DashboardScreen` with `property var vm`; introspection `readonly property string peakShown` for tests.

- [ ] **Step 1: Create the QuickTest harness + failing test.**

`qt-app/quick/tests/tst_qml_admin.cpp`:

```cpp
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_admin, Setup)
#include "tst_qml_admin.moc"
```

`qt-app/quick/tests/tst_qml_admin.qml`:

```qml
import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    width: 1100; height: 760

    // --- Dashboard stub VM ---
    ListModel { id: hourlyStub
        ListElement { label: "8"; value: 12 }
        ListElement { label: "10"; value: 41 }
    }
    ListModel { id: deptStub
        ListElement { label: "CE"; value: 210 }
        ListElement { label: "IT"; value: 180 }
    }
    QtObject {
        id: dashStub
        property int statToday: 128
        property int statWeek: 812
        property int statStudents: 3450
        property string peakHourLabel: "10 AM"
        property int peakHourIndex: 1
        property var hourlyModel: hourlyStub
        property var departmentModel: deptStub
        property bool loading: false
        property string errorText: ""
        function refresh() {}
    }
    DashboardScreen { id: dash; anchors.fill: parent; vm: dashStub }

    TestCase {
        name: "DashboardScreen"
        when: windowShown
        function test_showsPeakHourLabel() {
            compare(dash.peakShown, "10 AM");
        }
        function test_loadsWithStubVm() {
            verify(dash !== null);
        }
    }
}
```

- [ ] **Step 2: Create `qt-app/quick/qml/admin/DashboardScreen.qml`:**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Dashboard (spec §6.1). Fetches once on load; manual refresh; states dimmed
// while loading, inline error+retry, empty-state when there is no data.
Rectangle {
    id: screen
    property var vm

    // Introspection for tests.
    readonly property string peakShown: vm ? vm.peakHourLabel : ""

    color: Theme.appBackground

    Component.onCompleted: if (vm) vm.refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.xl
        opacity: (vm && vm.loading) ? 0.5 : 1.0

        // Stat cards.
        GridLayout {
            Layout.fillWidth: true
            columns: screen.width < 900 ? 2 : 4
            columnSpacing: Theme.spacing.lg
            rowSpacing: Theme.spacing.lg
            LStatTile { Layout.fillWidth: true; label: qsTr("Visitors Today")
                        value: vm ? vm.statToday.toString() : "0"; variant: "Hero" }
            LStatTile { Layout.fillWidth: true; label: qsTr("This Week")
                        value: vm ? vm.statWeek.toString() : "0" }
            LStatTile { Layout.fillWidth: true; label: qsTr("Registered Students")
                        value: vm ? vm.statStudents.toString() : "0" }
            LStatTile { Layout.fillWidth: true; label: qsTr("Peak Hour")
                        value: vm ? vm.peakHourLabel : "—" }
        }

        // Charts.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.xl
            LCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("Visitors by Hour"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    LBarChart {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        orientation: "Vertical"
                        model: vm ? vm.hourlyModel : null
                        maxValue: vm && vm.hourlyModel ? vm.hourlyModel.maxValue : 1
                        highlightIndex: vm ? vm.peakHourIndex : -1
                    }
                }
            }
            LCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("By Department"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    LBarChart {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        orientation: "Horizontal"
                        model: vm ? vm.departmentModel : null
                        maxValue: vm && vm.departmentModel ? vm.departmentModel.maxValue : 1
                    }
                }
            }
        }
    }

    // Error + retry (spec §7.3): a failed fetch never renders as empty.
    ColumnLayout {
        anchors.centerIn: parent
        visible: vm && vm.errorText.length > 0
        spacing: Theme.spacing.md
        Text {
            text: vm ? vm.errorText : ""
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }
        LButton { text: qsTr("Retry"); onClicked: if (vm) vm.refresh() }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: vm && vm.loading
        visible: running
    }
}
```

> **`LButton` interface check:** confirm `LButton` exposes `text` + an `onClicked`/`clicked` signal (`qt-app/quick/qml/components/LButton.qml`). If its click signal has a different name, adjust the `onClicked` handler here and in Tasks 14/16.

- [ ] **Step 3: Register the QML file + test target** in `qt-app/quick/CMakeLists.txt`. Add to `QML_FILES` (after `qml/admin/AdminScreen.qml`):

```cmake
        qml/admin/DashboardScreen.qml
        qml/admin/SearchScreen.qml
        qml/admin/VisitLogsScreen.qml
```

(Listing all three now avoids re-editing this block in Tasks 14/15; the files for Search/VisitLogs are created there. **Do not build until those files exist** — complete Step 4's build only after this task's file is created, and expect the Search/VisitLogs QML-not-found only if you build between tasks. If executing tasks strictly one-at-a-time, add only `DashboardScreen.qml` here and add the other two lines in Tasks 14/15.)

Append the admin QuickTest target (reused by Tasks 14–16):

```cmake
# --- Admin screens Quick test (offscreen). "import LOAMS" lives only in the
# QUICK_TEST_MAIN data file, so link the plugin + "_init" object explicitly.
wits_add_qttest(tst_qml_admin
    SOURCES tests/tst_qml_admin.cpp
    LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
         Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
    DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    OFFSCREEN)
```

- [ ] **Step 4: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS (DashboardScreen cases). (If you added all three QML_FILES lines but Search/VisitLogs don't exist yet, the module fails to build — add only `DashboardScreen.qml` for now.)

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): add DashboardScreen (Phase 3)`

---

## Task 14: `SearchScreen.qml`

**Files:**
- Create: `qt-app/quick/qml/admin/SearchScreen.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (`QML_FILES` — add `qml/admin/SearchScreen.qml` if not already added in Task 13)
- Modify: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: a VM exposing `results` (model with roles `name`/`schoolId`/`course`/`department`/`visits`), `courses` (list), `loading`, `errorText`, `search(search, course)`.
- Produces: `SearchScreen` with `property var vm`; introspection `readonly property int resultCount`; each result card shows the explicit label `"Total Visits: N"`.

- [ ] **Step 1: Add the failing test** — in `tst_qml_admin.qml`, add a stub + screen + `TestCase`:

```qml
    ListModel { id: searchStub
        ListElement { name: "Maria Santos"; schoolId: "2023-0001"; course: "BSCE"; department: "CE"; visits: 42 }
    }
    QtObject {
        id: searchVmStub
        property var results: searchStub
        property var courses: ["BSCE", "BSEE"]
        property bool loading: false
        property string errorText: ""
        property string lastSearch: ""
        function search(s, c) { lastSearch = s }
    }
    SearchScreen { id: search; width: 900; height: 700; vm: searchVmStub }

    TestCase {
        name: "SearchScreen"
        when: windowShown
        function test_showsResultCount() {
            compare(search.resultCount, 1);
        }
        function test_rendersTotalVisitsLabel() {
            verify(findAny(search, "Total Visits: 42") !== null);
        }
        function findAny(root, s) {
            if (root.text !== undefined && root.text !== null && root.text.indexOf(s) !== -1)
                return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findAny(root.children[i], s);
                if (f !== null) return f;
            }
            return null;
        }
    }
```

- [ ] **Step 2: Create `qt-app/quick/qml/admin/SearchScreen.qml`:**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Search (spec §6.2). Name/ID/course search + course filter chips. Each result
// card shows the lifetime count as an explicit "Total Visits: N" (never bare).
Rectangle {
    id: screen
    property var vm
    property string selectedCourse: ""

    readonly property int resultCount: vm && vm.results ? vm.results.count : 0

    color: Theme.appBackground

    function runSearch() { if (vm) vm.search(queryField.text, screen.selectedCourse) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // Query row.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            TextField {
                id: queryField
                Layout.fillWidth: true
                placeholderText: qsTr("Search by name, ID, or course")
                onAccepted: screen.runSearch()
            }
            LButton { text: qsTr("Search"); onClicked: screen.runSearch() }
        }

        // Course filter chips.
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            Repeater {
                model: vm ? vm.courses : []
                delegate: Rectangle {
                    required property var modelData
                    radius: Theme.radius.pill
                    implicitHeight: 28
                    implicitWidth: chipText.implicitWidth + Theme.spacing.xl
                    readonly property bool active: screen.selectedCourse === modelData
                    color: active ? Theme.brand.admin : Theme.card
                    border.width: 2
                    border.color: active ? Theme.brand.admin : Theme.border
                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: modelData
                        color: parent.active ? Theme.brand.onPrimary : Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.control
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            screen.selectedCourse = (screen.selectedCourse === modelData) ? "" : modelData;
                            screen.runSearch();
                        }
                    }
                }
            }
        }

        // Results.
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacing.md
            model: vm ? vm.results : null
            delegate: LCard {
                required property var model
                width: ListView.view ? ListView.view.width : 0
                implicitHeight: 96
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.xs
                    Text { text: model.name; color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    Text { text: model.course + " · " + model.department; color: Theme.mutedText
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
                    Text {
                        text: qsTr("Total Visits: %1").arg(model.visits)
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }

    // Error + retry.
    ColumnLayout {
        anchors.centerIn: parent
        visible: vm && vm.errorText.length > 0
        spacing: Theme.spacing.md
        Text { text: vm ? vm.errorText : ""; color: Theme.error
               font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
        LButton { text: qsTr("Retry"); onClicked: screen.runSearch() }
    }

    BusyIndicator { anchors.centerIn: parent; running: vm && vm.loading; visible: running }
}
```

- [ ] **Step 3: Register QML file** — ensure `qml/admin/SearchScreen.qml` is in `QML_FILES` (added in Task 13, or add now).

- [ ] **Step 4: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS (Dashboard + Search cases).

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): add SearchScreen with Total Visits label (Phase 3)`

---

## Task 15: `VisitLogsScreen.qml`

**Files:**
- Create: `qt-app/quick/qml/admin/VisitLogsScreen.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (`QML_FILES` — add `qml/admin/VisitLogsScreen.qml` if not already added in Task 13)
- Modify: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: a VM exposing `mode` (0=Student, 1=Guest), `range` (0=Today, 1=Week), `count` (int), `rangeLabel` (string), `rows` (model with union roles), `loading`, `errorText`, `refresh()`, and writable `mode`/`range`. Enum literals: `VisitLogsViewModel.Student`/`Guest`/`Today`/`Week`.
- Produces: `VisitLogsScreen` with `property var vm`; the `LTable` column set switches on `vm.mode`; time-out column is always `"—"` (student view only). Introspection `readonly property int activeColumnCount`.

- [ ] **Step 1: Add the failing test** — in `tst_qml_admin.qml`, add a stub + screen + `TestCase`:

```qml
    ListModel { id: visitRowsStub
        ListElement { date: "2026-07-13"; name: "Maria Santos"; course: "BSCE"; department: "CE"; timeIn: "08:14"; timeOut: "—"; company: ""; purpose: "" }
    }
    QtObject {
        id: visitVmStub
        property int mode: 0        // 0 == VisitLogsViewModel.Student
        property int range: 0
        property int count: 1
        property string rangeLabel: "Today, Jul 13, 2026"
        property var rows: visitRowsStub
        property bool loading: false
        property string errorText: ""
        function refresh() {}
    }
    VisitLogsScreen { id: logs; width: 1000; height: 700; vm: visitVmStub }

    TestCase {
        name: "VisitLogsScreen"
        when: windowShown
        function test_studentModeHasSixColumns() {
            compare(logs.vm.mode, VisitLogsViewModel.Student);
            compare(logs.activeColumnCount, 6);
        }
        function test_guestModeHasFourColumns() {
            logs.vm.mode = VisitLogsViewModel.Guest;
            compare(logs.activeColumnCount, 4);
        }
    }
```

- [ ] **Step 2: Create `qt-app/quick/qml/admin/VisitLogsScreen.qml`:**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Visit Logs (spec §6.3). Student attendance is the primary/default view; Guest
// is a secondary toggle. The LTable column set switches on vm.mode; the student
// Time Out column is always "—" (login-only).
Rectangle {
    id: screen
    property var vm

    readonly property var studentColumns: [
        { key: "date",       title: qsTr("Date") },
        { key: "name",       title: qsTr("Name") },
        { key: "course",     title: qsTr("Course") },
        { key: "department", title: qsTr("Department") },
        { key: "timeIn",     title: qsTr("Time In") },
        { key: "timeOut",    title: qsTr("Time Out") }
    ]
    readonly property var guestColumns: [
        { key: "name",    title: qsTr("Name") },
        { key: "company", title: qsTr("Company") },
        { key: "purpose", title: qsTr("Purpose") },
        { key: "timeIn",  title: qsTr("Time In") }
    ]
    readonly property bool isStudent: !vm || vm.mode === VisitLogsViewModel.Student
    readonly property var activeColumns: isStudent ? studentColumns : guestColumns
    readonly property int activeColumnCount: activeColumns.length

    color: Theme.appBackground

    Component.onCompleted: if (vm) vm.refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // Controls row: Student/Guest mode (student primary) + Today/Week range.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.lg
            LSegmented {
                options: [ { value: VisitLogsViewModel.Student, label: qsTr("Students") },
                           { value: VisitLogsViewModel.Guest,   label: qsTr("Guests") } ]
                currentValue: vm ? vm.mode : VisitLogsViewModel.Student
                onSelectionChanged: function(value) { if (vm) vm.mode = value }
            }
            Item { Layout.fillWidth: true }
            Text {
                text: vm ? vm.rangeLabel : ""
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
            }
            LSegmented {
                options: [ { value: VisitLogsViewModel.Today, label: qsTr("Today") },
                           { value: VisitLogsViewModel.Week,  label: qsTr("This Week") } ]
                currentValue: vm ? vm.range : VisitLogsViewModel.Today
                onSelectionChanged: function(value) { if (vm) vm.range = value }
            }
        }

        LTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: screen.activeColumns
            model: vm ? vm.rows : null
            emptyStateText: qsTr("No visits in this range")
        }
    }

    // Error + retry.
    ColumnLayout {
        anchors.centerIn: parent
        visible: vm && vm.errorText.length > 0
        spacing: Theme.spacing.md
        Text { text: vm ? vm.errorText : ""; color: Theme.error
               font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
        LButton { text: qsTr("Retry"); onClicked: if (vm) vm.refresh() }
    }

    BusyIndicator { anchors.centerIn: parent; running: vm && vm.loading; visible: running }
}
```

- [ ] **Step 3: Register QML file** — ensure `qml/admin/VisitLogsScreen.qml` is in `QML_FILES`.

- [ ] **Step 4: Configure, build, run — verify PASS**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS (Dashboard + Search + VisitLogs cases).

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): add VisitLogsScreen with mode-switched columns (Phase 3)`

---

## Task 16: `AdminScreen` shell + back-to-kiosk + wiring

**Files:**
- Modify: `qt-app/quick/qml/admin/AdminScreen.qml`
- Modify: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: `Navigator` (`adminPage`, `showAdminPage`, `showKiosk`, `Navigator.Dashboard`/`Search`/`VisitLogs`); the three real VMs (`DashboardViewModel`/`SearchViewModel`/`VisitLogsViewModel`); the three screens; `LSideNav`/`LPageHeader`.
- Produces: the functional admin surface — sidebar shell + header with a live clock + a `Loader` keyed on `Navigator.adminPage`, plus a Back-to-Kiosk footer affordance.

- [ ] **Step 1: Add the failing test** — in `tst_qml_admin.qml`, add a real shell instance + `TestCase`. (The shell uses the singleton `Navigator`, so drive it directly.)

```qml
    AdminScreen { id: shell; width: 1100; height: 720 }

    TestCase {
        name: "AdminScreenShell"
        when: windowShown

        function init() { Navigator.showAdminPage(Navigator.Dashboard); }

        function test_sidebarSwitchesActivePage() {
            shell.activatePage(Navigator.VisitLogs);   // test hook -> Navigator.showAdminPage
            compare(Navigator.adminPage, Navigator.VisitLogs);
        }
        function test_backToKioskReturnsToKiosk() {
            Navigator.showAdmin();
            shell.backToKiosk();                        // test hook -> Navigator.showKiosk
            compare(Navigator.currentSurface, Navigator.Kiosk);
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: FAIL — `shell.activatePage`/`backToKiosk` undefined (still the placeholder).

- [ ] **Step 3: Implement `qt-app/quick/qml/admin/AdminScreen.qml`:**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Admin shell (spec §7.2): sidebar + page header (with live clock) + a Loader
// keyed on Navigator.adminPage. Replaces the Phase-2 placeholder.
Rectangle {
    id: admin
    color: Theme.appBackground

    // Test hooks (mirror the click/footer paths).
    function activatePage(page) { Navigator.showAdminPage(page) }
    function backToKiosk() { Navigator.showKiosk() }

    // One VM per screen, instantiated once and reused across page switches.
    DashboardViewModel { id: dashboardVm }
    SearchViewModel    { id: searchVm }
    VisitLogsViewModel { id: visitLogsVm }

    // Live clock for the header.
    property string clockText: ""
    function tickClock() { clockText = Qt.formatDateTime(new Date(), "h:mm:ss AP") }
    Component.onCompleted: tickClock()
    Timer { interval: 1000; running: true; repeat: true; onTriggered: admin.tickClock() }

    readonly property string pageTitle: {
        switch (Navigator.adminPage) {
        case Navigator.Search:     return qsTr("Search");
        case Navigator.VisitLogs:  return qsTr("Visit Logs");
        default:                   return qsTr("Dashboard");
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        LSideNav {
            id: sideNav
            Layout.fillHeight: true
            Layout.preferredWidth: 240
            currentPage: Navigator.adminPage
            items: [
                { page: Navigator.Dashboard, label: qsTr("Dashboard"),  enabled: true },
                { page: Navigator.Search,    label: qsTr("Search"),     enabled: true },
                { page: Navigator.VisitLogs, label: qsTr("Visit Logs"), enabled: true },
                { page: "database",          label: qsTr("Database"),   enabled: false },
                { page: "reporting",         label: qsTr("Reporting"),  enabled: false },
                { page: "settings",          label: qsTr("Settings"),   enabled: false }
            ]
            onPageActivated: function(page) { Navigator.showAdminPage(page) }
            footer: LButton {
                text: qsTr("← Back to Kiosk")
                onClicked: Navigator.showKiosk()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            LPageHeader {
                Layout.fillWidth: true
                Layout.margins: Theme.spacing.xxl
                Layout.bottomMargin: 0
                title: admin.pageTitle
                clockText: admin.clockText
            }

            Loader {
                id: pageLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: {
                    switch (Navigator.adminPage) {
                    case Navigator.Search:    return searchComponent;
                    case Navigator.VisitLogs: return visitLogsComponent;
                    default:                  return dashboardComponent;
                    }
                }
            }
        }
    }

    Component { id: dashboardComponent; DashboardScreen { vm: dashboardVm } }
    Component { id: searchComponent;    SearchScreen    { vm: searchVm } }
    Component { id: visitLogsComponent; VisitLogsScreen { vm: visitLogsVm } }
}
```

> **Note:** `LSideNav.footer` is assigned an `LButton` whose default parent is the `LSideNav`; the footer slot reparents it (Task 9's `children: [nav.footer]`). Confirm the button renders at the sidebar bottom in the visual walkthrough (Step 6).

- [ ] **Step 4: Run to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS (all admin cases). Also run the full suite: `ctest --test-dir qt-app/build --output-on-failure` — expect all prior targets green + the new ones (≥ 30 targets).

- [ ] **Step 5: Commit** — via the `commit` skill, message:
`feat(loams2): wire admin shell, sidebar nav, and back-to-kiosk (Phase 3)`

- [ ] **Step 6: Manual visual walkthrough (spec §12 gate).** Build and run `WITSQuick` (see `wits-qtcreator-maxpath-buildir` — use a short build dir in Qt Creator, or run the Ninja-built `qt-app/build/quick/WITSQuick.exe` with `C:\Qt\6.11.1\mingw_64\bin` on PATH). Enter admin via the kiosk admin-key login. Verify against `../Library System UI Modernization/Admin Dashboard.dc.html`:
  - Sidebar: Dashboard/Search/Visit Logs active-highlight + gold dot; Database/Reporting/Settings show "soon" and are inert; Back-to-Kiosk returns to the kiosk.
  - Header clock ticks every second; subtitle/title correct per page.
  - Dashboard: 4 stat cards, vertical hourly chart with the peak bar gold-highlighted, horizontal department bars; refresh works; error+retry on a killed backend.
  - Visit Logs: Students is the default; Today/This-Week toggle; switching to Guests changes the column set; Time Out shows "—".
  - Search: query + course chips; each result card shows "Total Visits: N".

---

## Phase-completion gates (spec §12)

After Task 16, before finishing the branch (workflow.md §3–4):

- [ ] **Full build clean** (Debug; then a Release configure/build) — no new warnings/errors.
- [ ] **Full ctest green:** `ctest --test-dir qt-app/build --output-on-failure` — all targets pass (baseline 23 + the new parser/model/VM/QuickTest targets).
- [ ] **`/claude-review`** (Skill tool) — fix Critical/Important, resubmit until APPROVE or 3 rounds.
- [ ] **create-pr review trio** (dry-checker / security-reviewer / general-code-reviewer, diff-scoped) via the `create-pr` skill — fix Critical/Important.
- [ ] **Manual visual walkthrough** signed off (Task 16 Step 6).
- [ ] Open the PR against `master` via `create-pr` (see `wits-pr-base-branch`). **Ask before merging** — PR-open approval ≠ merge approval.

---

## Self-review notes (spec coverage)

- §5.1 dashboard_summary + client-side peak → Tasks 3, 6. §5.2 get_library_visits → Task 4. §5.3 four-layer visits (PHP→struct→parser→VM/screen) → Tasks 2 (layers 1–3) + 8 + 14 (layer 4). §5.4 guest reuse with app-generated ISO dates → Task 7 (`applyGuestVisits` + ISO-only date fields). §5 date semantics (today / Mon–Sun week, half-open range) → Tasks 3, 4 PHP + Task 7 `formatWeekLabel`/`computeRangeLabel`.
- §6.1 Dashboard (stat tiles, vertical hourly + horizontal department, peak) → Tasks 11, 13. §6.2 Search (Total Visits label, course chips) → Tasks 8, 14. §6.3 Visit Logs (student primary, guest secondary, mode-switched columns, "—" time-out, Today/Week segmented) → Tasks 7, 15.
- §7.1 Navigator extension → Task 1. §7.2 shell + 3 screens → Tasks 13–16. §7.3 per-screen VMs, first-class fetch state (loading/errorText), pure parsers in core/, QAbstractListModel rows, ApiConfig-only endpoints → Tasks 3–8.
- §8 component build-outs: LSideNav → 9, LTable → 10, LBarChart → 11, LPageHeader → 12 (LSegmented/LStatTile/LCard reused as-is).
- §9 a11y slice: `Accessible.*` roles carried on built-out components (Tasks 9–12). §10 Theme compliance: no hex outside Theme.qml; colors chosen in QML (Global Constraints). §12 verification: new test targets registered per task; gates above.
- §11 deferred (Database/Reporting/Settings, configurable week, Ctrl+K, backend auth, time-out) — explicitly out of scope; disabled "soon" sidebar items only (Task 16).
