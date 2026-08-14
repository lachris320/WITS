# LOAMS 2.0 Phase 4b-i — Reporting (Filters + Native Preview) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the "Reporting — coming soon" placeholder with a working reporting screen — a Dept→Course + duration filter panel and an on-screen native-QML preview (per-student table, ranked visits-by-course bar chart, summary stat tiles), run by an explicit Generate Report button.

**Architecture:** MVVM. A new `ReportingViewModel` (the only QML-facing C++) owns a `QNetworkAccessManager` + the already-built witscore `ReportController`, exposes filter/duration/result state as QML properties, and delegates the network to `ReportController`. A new `ReportRowsModel` (`QAbstractListModel`) feeds the table; the existing `BarsModel` feeds the chart. A new native `LDatePicker` component handles Day/Custom date input. No backend change; no new business logic (the report math already lives in `ReportController`/`get_report_data.php`).

**Tech Stack:** Qt 6 / C++17, Qt Quick / QML (URI `LOAMS`, module target `witsquickmodule`), CMake, QtTest + QuickTest under ctest.

## Global Constraints

- **Zero raw hex outside `Theme.qml`.** All colors via `Theme.<token>`; opacity variants via `Qt.alpha(Theme.<token>, a)`, never a literal color.
- **MVVM:** `ReportingViewModel` is the only new QML-facing C++. QML never calls `ReportController` directly.
- **Naming:** QML types + C++ VM/model classes are `PascalCase`; C++ members are `m_camelCase`.
- **Tests:** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add `OFFSCREEN` for any GUI/Quick test. Existing ctest targets are the regression floor and must stay green.
- **Anti-injection:** any server-echoed text rendered in QML uses `textFormat: Text.PlainText` (server data reaches the client over cleartext HTTP). `LTable`, `LComboBox` already do this internally.
- **`durationType` is a STRING on the wire** (`"day"`/`"month"`/`"semester"`/`"custom"`) — `get_report_data.php` reads it as a string; emitting the int silently returns all-time data with no error.
- **Semester ranging is server-side:** for Semester, send `durationType`+`year`+`semester` (a token the server recognizes: contains `first`/`1`, `second`/`2`, or `summer`) and let the server compute the range — do NOT send client-computed semester `start`/`end` (the client's `computeDateRange` semester math disagrees with the server's academic-calendar ranges). Day/Month/Custom send client-computed `start`/`end`.
- **Single-in-flight:** only one report fetch at a time. `generateReport()` is a no-op while `loading`; `canGenerate` is false while `loading`. There is NO request-token guard (the `reportDataReady` signal carries no token).
- **Build dir for this slice:** `C:/b/loams-4b` (short path, MAX_PATH — see the memory `wits-qtcreator-maxpath-buildir`). Qt tools are NOT on PATH — use the toolchain from the memory `wits-build-toolchain` (Ninja + `CMAKE_PREFIX_PATH`).

## Reference — build & test commands

Configure once (from the repo root), then build + run ctest. Substitute the real Qt kit path from the `wits-build-toolchain` memory if these differ.

```bash
cmake -S qt-app -B C:/b/loams-4b -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-4b
ctest --test-dir C:/b/loams-4b --output-on-failure
```

Run a single test target while iterating: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`.

## Verified facts the plan relies on (do not re-derive)

- `ReportController` (`qt-app/core/reportcontroller.h`) exposes, as **statics**: `computeDateRange(int durationType, const QDate &day, int month, int monthYear, const QString &semester, int semYear, const QDate &customStart, const QDate &customEnd) -> DateRange` (durationType 0=Day,1=Month,2=Semester,3=Custom); and async methods `loadDepartments()`, `loadYears()`, `loadCourses(const QString &department)`, `fetchReportRows(const QJsonObject &filters)` with signals `departmentsLoaded(QStringList)`, `yearsLoaded(QStringList)`, `coursesLoaded(QStringList)`, `reportDataReady(QJsonArray)`, `reportError(QString,bool)`, `loadError(QString,QString,bool)`. Its ctor is `ReportController(QNetworkAccessManager *nam, QObject *parent = nullptr)` — the NAM is injected, not owned.
- `DateRange` (`qt-app/core/reportdata.h`): `{ QString start; QString end; bool valid; }` (`start`/`end` are `"yyyy-MM-dd"`).
- `get_report_data.php` reads `department` (required), `course` (`""`/`"all"`/`"all courses"` = no filter), `durationType`, `start`, `end`, `year`, `semester`; returns `{"status":"success","data":[ {school_id,name,gender,status,course,department,year_level,visits}, ... ]}` ordered by `visits DESC`. **`visits` is a `COUNT(...)` returned by mysqli `fetch_assoc` as a JSON STRING** (e.g. `"5"`) — parse robustly (see `reportVisits` helper in Task 1/3).
- `BarsModel` (`qt-app/quick/models/BarsModel.h`): `struct Bar { QString label; double value; }`, `void setBars(const QList<Bar>&)`, roles `label`/`value`, notifying `maxValue`. Consumed by `LBarChart` (`orientation: "Horizontal"`, colors from Theme).
- `AdminScreen.qml`: the reporting `Component` currently has NO `vm` (`AdminScreen.qml:196`); the `Loader.onLoaded` gate already calls `item.vm.loadDepartments()` when present (`AdminScreen.qml:185-186`); `Navigator.Reporting` exists.
- `LCascadingSelect` props: `departments`, `courses`, `department`, `course`; signals `departmentPicked(string)`, `coursePicked(string)`; `"All"` → `""`.
- `LComboBox`: `model` (string list), `currentValue`, `placeholder`, signal `selected(string)`, function `selectValue(v)`.
- `LTable`: `columns` (list of `{key,title,weight}`), `model`, `emptyStateText`, readonly `rowCount`/`emptyVisible`.
- `LStatTile`: `label`, `value`, `caption`, `variant` (`Neutral`/`Hero`).
- QuickTest host `tst_qml_admin.qml`: fixed-geometry screens in non-overlapping y-bands; current ledger ends at `importDialog 6600..7300`, `height: 7300`. This plan adds `reporting 7300..8100` and raises `height` to `8100`.

---

### Task 1: `ReportRowsModel` — per-student rows for the preview table

**Files:**
- Create: `qt-app/quick/models/ReportRowsModel.h`
- Create: `qt-app/quick/models/ReportRowsModel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add the two sources to `witsquickmodule` `SOURCES`; add a new `tst_reportrowsmodel` test target)
- Test: `qt-app/quick/tests/tst_reportrowsmodel.cpp` (create)

**Interfaces:**
- Produces: `class ReportRowsModel : public QAbstractListModel` with roles `NameRole/CourseRole/YearLevelRole/VisitsRole` exposed as `name/course/year/visits`; `void setRows(const QJsonArray &data)`; `int count() const` (`Q_PROPERTY count NOTIFY countChanged`). Consumed by `ReportingViewModel` (Task 6) and `LTable` (Task 8).
- Produces: free helper `int reportVisits(const QJsonObject &row)` — robustly parses the `visits` field whether it arrives as a JSON number or string. Reused by Task 3.

- [ ] **Step 1: Write the failing test**

Create `qt-app/quick/tests/tst_reportrowsmodel.cpp`:

```cpp
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "ReportRowsModel.h"

class TestReportRowsModel : public QObject
{
    Q_OBJECT
private slots:
    void setRowsPopulatesRolesAndCount();
    void visitsParsesStringOrNumber();
    void setRowsEmptyClears();
};

static QJsonArray arr(const char *json)
{
    return QJsonDocument::fromJson(json).array();
}

void TestReportRowsModel::setRowsPopulatesRolesAndCount()
{
    ReportRowsModel m;
    QSignalSpy countSpy(&m, &ReportRowsModel::countChanged);
    m.setRows(arr(R"([
        {"name":"Maria Santos","course":"BSCE","year_level":"3","visits":"42"},
        {"name":"Jose Cruz","course":"BSIT","year_level":"1","visits":"7"}
    ])"));
    QCOMPARE(m.count(), 2);
    QVERIFY(countSpy.count() >= 1);
    const QModelIndex i0 = m.index(0, 0);
    QCOMPARE(m.data(i0, ReportRowsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(m.data(i0, ReportRowsModel::CourseRole).toString(), QStringLiteral("BSCE"));
    QCOMPARE(m.data(i0, ReportRowsModel::YearLevelRole).toString(), QStringLiteral("3"));
    QCOMPARE(m.data(i0, ReportRowsModel::VisitsRole).toInt(), 42);
    const QHash<int, QByteArray> roles = m.roleNames();
    QCOMPARE(roles.value(ReportRowsModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(ReportRowsModel::VisitsRole), QByteArray("visits"));
}

void TestReportRowsModel::visitsParsesStringOrNumber()
{
    QCOMPARE(reportVisits(QJsonObject{ {"visits", "13"} }), 13); // mysqli string
    QCOMPARE(reportVisits(QJsonObject{ {"visits", 9} }), 9);      // JSON number
    QCOMPARE(reportVisits(QJsonObject{ {"name", "x"} }), 0);      // missing
}

void TestReportRowsModel::setRowsEmptyClears()
{
    ReportRowsModel m;
    m.setRows(arr(R"([{"name":"A","course":"C","year_level":"1","visits":"1"}])"));
    QCOMPARE(m.count(), 1);
    m.setRows(QJsonArray());
    QCOMPARE(m.count(), 0);
}

QTEST_APPLESS_MAIN(TestReportRowsModel)
#include "tst_reportrowsmodel.moc"
```

- [ ] **Step 2: Register the target and run the test to verify it fails to build/compile**

Add to `qt-app/quick/CMakeLists.txt`, in the `witsquickmodule` `SOURCES` list (after the `models/StudentsTableModel...` line):

```cmake
        models/ReportRowsModel.h models/ReportRowsModel.cpp
```

Add a new test target near the other model tests (after `tst_studentstablemodel`):

```cmake
# --- ReportRowsModel unit test (C++ QtTest). Pure QAbstractListModel over
# JSON rows, no NAM -> QTEST_APPLESS_MAIN, no OFFSCREEN. ---
wits_add_qttest(tst_reportrowsmodel
    SOURCES tests/tst_reportrowsmodel.cpp
    LIBS witsquickmodule)
```

Run: `cmake -S qt-app -B C:/b/loams-4b -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4b -t tst_reportrowsmodel`
Expected: FAIL — `ReportRowsModel.h` not found / undefined `reportVisits`.

- [ ] **Step 3: Write the implementation**

Create `qt-app/quick/models/ReportRowsModel.h`:

```cpp
#ifndef REPORTROWSMODEL_H
#define REPORTROWSMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

// visits arrives from get_report_data.php as a COUNT(...) which mysqli
// fetch_assoc returns as a JSON STRING ("5"), not a number. Parse both shapes.
int reportVisits(const QJsonObject &row);

// Per-student rows for the Reporting preview table (spec §4.2). Mirrors the
// StudentsTableModel/BarsModel precedent: roles enum, roleNames(), typed row.
class ReportRowsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, CourseRole, YearLevelRole, VisitsRole };
    explicit ReportRowsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_rows.size(); }
    void setRows(const QJsonArray &data);

signals:
    void countChanged();

private:
    struct Row { QString name; QString course; QString year; int visits = 0; };
    QList<Row> m_rows;
};

#endif // REPORTROWSMODEL_H
```

Create `qt-app/quick/models/ReportRowsModel.cpp`:

```cpp
#include "ReportRowsModel.h"

int reportVisits(const QJsonObject &row)
{
    const QJsonValue v = row.value(QStringLiteral("visits"));
    if (v.isDouble())
        return v.toInt();
    return v.toString().toInt();   // "5" -> 5; missing/"" -> 0
}

ReportRowsModel::ReportRowsModel(QObject *parent) : QAbstractListModel(parent) {}

int ReportRowsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant ReportRowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case NameRole:      return r.name;
    case CourseRole:    return r.course;
    case YearLevelRole: return r.year;
    case VisitsRole:    return r.visits;
    default:            return {};
    }
}

QHash<int, QByteArray> ReportRowsModel::roleNames() const
{
    return { { NameRole, "name" }, { CourseRole, "course" },
             { YearLevelRole, "year" }, { VisitsRole, "visits" } };
}

void ReportRowsModel::setRows(const QJsonArray &data)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(data.size());
    for (const QJsonValue &v : data) {
        const QJsonObject o = v.toObject();
        m_rows.append({ o.value("name").toString(),
                        o.value("course").toString(),
                        o.value("year_level").toString(),
                        reportVisits(o) });
    }
    endResetModel();
    emit countChanged();
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportrowsmodel --output-on-failure`
Expected: PASS (3 test functions).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/models/ReportRowsModel.h qt-app/quick/models/ReportRowsModel.cpp \
        qt-app/quick/tests/tst_reportrowsmodel.cpp qt-app/quick/CMakeLists.txt
git commit -m "feat(reporting): ReportRowsModel for the preview table"
```

---

### Task 2: `ReportingViewModel` skeleton + `buildFilters` (pure request mapping)

**Files:**
- Create: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Create: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add sources; add `tst_reportingviewmodel` target)
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp` (create)

**Interfaces:**
- Produces: `static QJsonObject ReportingViewModel::buildFilters(const QString &department, const QString &course, int durationType, const QDate &day, int month, int monthYear, const QString &semester, int semYear, const QDate &customStart, const QDate &customEnd)` — the exact request body for `get_report_data.php`. Consumed by `generateReport()` (Task 6).
- Produces: the `ReportingViewModel` class + `QML_ELEMENT` registration + the shared test target `tst_reportingviewmodel`, extended by Tasks 3/4/6.

- [ ] **Step 1: Write the failing test**

Create `qt-app/quick/tests/tst_reportingviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QDate>
#include <QJsonObject>
#include "ReportingViewModel.h"

class TestReportingViewModel : public QObject
{
    Q_OBJECT
private slots:
    void buildFiltersDaySendsStringTypeAndRange();
    void buildFiltersMonthSendsRange();
    void buildFiltersSemesterSendsComponentsNotRange();
    void buildFiltersCustomSendsRange();
    void buildFiltersPassesDeptAndCourse();
};

void TestReportingViewModel::buildFiltersDaySendsStringTypeAndRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("day"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-08-14"));
}

void TestReportingViewModel::buildFiltersMonthSendsRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 1, QDate(), 2, 2026, "", 0, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("month"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-02-01"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-02-28"));
}

void TestReportingViewModel::buildFiltersSemesterSendsComponentsNotRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 2, QDate(), 0, 0, "First Semester", 2026, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("semester"));
    QCOMPARE(f.value("year").toInt(), 2026);
    QCOMPARE(f.value("semester").toString(), QStringLiteral("First Semester"));
    // Semester ranging is server-side: no client start/end sent.
    QVERIFY(!f.contains("start"));
    QVERIFY(!f.contains("end"));
}

void TestReportingViewModel::buildFiltersCustomSendsRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 3, QDate(), 0, 0, "", 0, QDate(2026, 1, 1), QDate(2026, 3, 31));
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("custom"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-01-01"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-03-31"));
}

void TestReportingViewModel::buildFiltersPassesDeptAndCourse()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "BSCE", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("department").toString(), QStringLiteral("CE"));
    QCOMPARE(f.value("course").toString(), QStringLiteral("BSCE"));
}

QTEST_MAIN(TestReportingViewModel)
#include "tst_reportingviewmodel.moc"
```

- [ ] **Step 2: Register + run to verify it fails**

Add to `witsquickmodule` `SOURCES` (after the `viewmodels/ImportViewModel...` line):

```cmake
        viewmodels/ReportingViewModel.h viewmodels/ReportingViewModel.cpp
```

Add a test target (after `tst_importviewmodel`):

```cmake
# --- ReportingViewModel unit test (C++ QtTest, offscreen). Builds a real
# QNetworkAccessManager + ReportController; generateReport() fires a
# fire-and-forget post() (harmless in-test), and the reply handlers are driven
# via the public on* slots — mirrors tst_databaseviewmodel. ---
wits_add_qttest(tst_reportingviewmodel
    SOURCES tests/tst_reportingviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)
```

Run: `cmake -S qt-app -B C:/b/loams-4b -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4b -t tst_reportingviewmodel`
Expected: FAIL — `ReportingViewModel.h` not found.

- [ ] **Step 3: Write the skeleton + `buildFilters`**

Create `qt-app/quick/viewmodels/ReportingViewModel.h`:

```cpp
#ifndef REPORTINGVIEWMODEL_H
#define REPORTINGVIEWMODEL_H

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <qqml.h>                 // QML_ELEMENT — AdminScreen instantiates this type
#include "BarsModel.h"
#include "ReportRowsModel.h"

class QNetworkAccessManager;
class ReportController;

// Reporting screen VM (spec 4b-i). Wraps the witscore ReportController (no new
// endpoint). Only QML-facing C++ for the reporting screen. Single-in-flight:
// generateReport() is a no-op while loading; canGenerate is false while loading.
class ReportingViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QStringList courses READ courses NOTIFY coursesChanged)
    Q_PROPERTY(QStringList years READ years NOTIFY yearsChanged)
    Q_PROPERTY(QString department READ department NOTIFY departmentChanged)
    Q_PROPERTY(QString course READ course NOTIFY courseChanged)
    Q_PROPERTY(int durationType READ durationType WRITE setDurationType NOTIFY durationTypeChanged)
    Q_PROPERTY(QString day READ day WRITE setDay NOTIFY dayChanged)
    Q_PROPERTY(int month READ month WRITE setMonth NOTIFY monthChanged)
    Q_PROPERTY(int monthYear READ monthYear WRITE setMonthYear NOTIFY monthYearChanged)
    Q_PROPERTY(QString semester READ semester WRITE setSemester NOTIFY semesterChanged)
    Q_PROPERTY(int semYear READ semYear WRITE setSemYear NOTIFY semYearChanged)
    Q_PROPERTY(QString customStart READ customStart WRITE setCustomStart NOTIFY customStartChanged)
    Q_PROPERTY(QString customEnd READ customEnd WRITE setCustomEnd NOTIFY customEndChanged)
    Q_PROPERTY(bool canGenerate READ canGenerate NOTIFY canGenerateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY resultChanged)
    Q_PROPERTY(ReportRowsModel *rows READ rows CONSTANT)
    Q_PROPERTY(BarsModel *courseBars READ courseBars CONSTANT)
    Q_PROPERTY(int totalVisits READ totalVisits NOTIFY resultChanged)
    Q_PROPERTY(int studentsShown READ studentsShown NOTIFY resultChanged)
    Q_PROPERTY(QString topCourse READ topCourse NOTIFY resultChanged)
public:
    explicit ReportingViewModel(QObject *parent = nullptr);

    // Pure statics (network-free test seams).
    static QJsonObject buildFilters(const QString &department, const QString &course,
                                    int durationType,
                                    const QDate &day,
                                    int month, int monthYear,
                                    const QString &semester, int semYear,
                                    const QDate &customStart, const QDate &customEnd);
    static QList<BarsModel::Bar> aggregateVisitsByCourse(const QJsonArray &data); // Task 3
    struct Tiles { int totalVisits = 0; int studentsShown = 0; QString topCourse; };
    static Tiles deriveTiles(const QJsonArray &data);                             // Task 3

    QStringList departments() const { return m_departments; }
    QStringList courses() const { return m_courses; }
    QStringList years() const { return m_years; }
    QString department() const { return m_department; }
    QString course() const { return m_course; }
    int durationType() const { return m_durationType; }
    QString day() const { return m_day; }
    int month() const { return m_month; }
    int monthYear() const { return m_monthYear; }
    QString semester() const { return m_semester; }
    int semYear() const { return m_semYear; }
    QString customStart() const { return m_customStart; }
    QString customEnd() const { return m_customEnd; }
    bool canGenerate() const;                    // Task 4
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }
    bool hasResult() const { return m_hasResult; }
    ReportRowsModel *rows() { return &m_rows; }
    BarsModel *courseBars() { return &m_courseBars; }
    int totalVisits() const { return m_totalVisits; }
    int studentsShown() const { return m_studentsShown; }
    QString topCourse() const { return m_topCourse; }

    Q_INVOKABLE void loadDepartments();          // bootstrap: departments + years (Task 5)
    Q_INVOKABLE void setDepartment(const QString &department);   // Task 5
    Q_INVOKABLE void setCourse(const QString &course);           // Task 5
    Q_INVOKABLE void setDurationType(int t);     // Task 4
    Q_INVOKABLE void setDay(const QString &v);
    Q_INVOKABLE void setMonth(int v);
    Q_INVOKABLE void setMonthYear(int v);
    Q_INVOKABLE void setSemester(const QString &v);
    Q_INVOKABLE void setSemYear(int v);
    Q_INVOKABLE void setCustomStart(const QString &v);
    Q_INVOKABLE void setCustomEnd(const QString &v);
    Q_INVOKABLE void generateReport();           // Task 6
    Q_INVOKABLE void retry();                     // Task 6

    // Public slots (network-free test seam) — wired to ReportController in Task 5/6.
    void onDepartmentsLoaded(const QStringList &departments);
    void onYearsLoaded(const QStringList &years);
    void onCoursesLoaded(const QStringList &courses);
    void onReportDataReady(const QJsonArray &data);
    void onReportError(const QString &message, bool critical);
    void onLoadError(const QString &title, const QString &message, bool critical);

signals:
    void departmentsChanged();
    void coursesChanged();
    void yearsChanged();
    void departmentChanged();
    void courseChanged();
    void durationTypeChanged();
    void dayChanged();
    void monthChanged();
    void monthYearChanged();
    void semesterChanged();
    void semYearChanged();
    void customStartChanged();
    void customEndChanged();
    void canGenerateChanged();
    void loadingChanged();
    void errorTextChanged();
    void resultChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);
    void applyResult(const QJsonArray &data);    // Task 6
    static QDate parseDate(const QString &s) { return QDate::fromString(s, QStringLiteral("yyyy-MM-dd")); }

    QNetworkAccessManager *m_nam = nullptr;
    ReportController *m_controller = nullptr;
    ReportRowsModel m_rows;
    BarsModel m_courseBars;

    QStringList m_departments, m_courses, m_years;
    QString m_department, m_course;
    int m_durationType = 0;      // 0=Day default
    QString m_day, m_customStart, m_customEnd;
    int m_month = 0, m_monthYear = 0, m_semYear = 0;
    QString m_semester;

    bool m_loading = false;
    QString m_errorText;
    bool m_hasResult = false;
    int m_totalVisits = 0, m_studentsShown = 0;
    QString m_topCourse = QStringLiteral("—");
};

#endif // REPORTINGVIEWMODEL_H
```

Create `qt-app/quick/viewmodels/ReportingViewModel.cpp` with the ctor + `buildFilters` only (later tasks fill the rest). Stub the not-yet-implemented members so the class links:

```cpp
#include "ReportingViewModel.h"

#include <QNetworkAccessManager>
#include "reportcontroller.h"
#include "reportdata.h"

ReportingViewModel::ReportingViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new ReportController(m_nam, this))
{
    // Signal wiring added in Task 5/6.
}

QJsonObject ReportingViewModel::buildFilters(const QString &department, const QString &course,
                                             int durationType,
                                             const QDate &day,
                                             int month, int monthYear,
                                             const QString &semester, int semYear,
                                             const QDate &customStart, const QDate &customEnd)
{
    QJsonObject f;
    f["department"] = department;
    f["course"] = course;
    switch (durationType) {
    case 0: {   // Day
        f["durationType"] = "day";
        const DateRange r = ReportController::computeDateRange(0, day, 0, 0, QString(), 0, QDate(), QDate());
        f["start"] = r.start;
        f["end"] = r.end;
        break;
    }
    case 1: {   // Month
        f["durationType"] = "month";
        const DateRange r = ReportController::computeDateRange(1, QDate(), month, monthYear, QString(), 0, QDate(), QDate());
        f["start"] = r.start;
        f["end"] = r.end;
        break;
    }
    case 2: {   // Semester — server-side ranging; send components, NOT a client range
        f["durationType"] = "semester";
        f["year"] = semYear;
        f["semester"] = semester;
        break;
    }
    case 3: {   // Custom
        f["durationType"] = "custom";
        const DateRange r = ReportController::computeDateRange(3, QDate(), 0, 0, QString(), 0, customStart, customEnd);
        f["start"] = r.start;
        f["end"] = r.end;
        break;
    }
    default:
        break;
    }
    return f;
}

// --- Stubs filled by later tasks (present so the class links now) ---
QList<BarsModel::Bar> ReportingViewModel::aggregateVisitsByCourse(const QJsonArray &) { return {}; }
ReportingViewModel::Tiles ReportingViewModel::deriveTiles(const QJsonArray &) { return {}; }
bool ReportingViewModel::canGenerate() const { return false; }
void ReportingViewModel::loadDepartments() {}
void ReportingViewModel::setDepartment(const QString &) {}
void ReportingViewModel::setCourse(const QString &) {}
void ReportingViewModel::setDurationType(int) {}
void ReportingViewModel::setDay(const QString &) {}
void ReportingViewModel::setMonth(int) {}
void ReportingViewModel::setMonthYear(int) {}
void ReportingViewModel::setSemester(const QString &) {}
void ReportingViewModel::setSemYear(int) {}
void ReportingViewModel::setCustomStart(const QString &) {}
void ReportingViewModel::setCustomEnd(const QString &) {}
void ReportingViewModel::generateReport() {}
void ReportingViewModel::retry() {}
void ReportingViewModel::onDepartmentsLoaded(const QStringList &) {}
void ReportingViewModel::onYearsLoaded(const QStringList &) {}
void ReportingViewModel::onCoursesLoaded(const QStringList &) {}
void ReportingViewModel::onReportDataReady(const QJsonArray &) {}
void ReportingViewModel::onReportError(const QString &, bool) {}
void ReportingViewModel::onLoadError(const QString &, const QString &, bool) {}
void ReportingViewModel::setLoading(bool) {}
void ReportingViewModel::setError(const QString &) {}
void ReportingViewModel::applyResult(const QJsonArray &) {}
```

> Later tasks REPLACE the corresponding stub bodies (delete the stub line, implement the real body). The stubs exist only so this task compiles and links a valid `QML_ELEMENT` type.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (5 `buildFilters` functions).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp \
        qt-app/quick/tests/tst_reportingviewmodel.cpp qt-app/quick/CMakeLists.txt
git commit -m "feat(reporting): ReportingViewModel skeleton + buildFilters request mapping"
```

---

### Task 3: `aggregateVisitsByCourse` + `deriveTiles` (pure aggregation)

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` (replace the two stub bodies)
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp` (add functions)

**Interfaces:**
- Consumes: `reportVisits(QJsonObject)` (Task 1), `BarsModel::Bar` (BarsModel.h).
- Produces: ranked `QList<BarsModel::Bar>` (by-course, descending) and `Tiles{totalVisits, studentsShown, topCourse}`. Consumed by `applyResult()` (Task 6).

- [ ] **Step 1: Add the failing tests**

Add to the `private slots:` block and body of `tst_reportingviewmodel.cpp`:

```cpp
    // add to the slots list:
    void aggregateSumsAndRanksByCourse();
    void aggregateEmptyIsEmpty();
    void deriveTilesComputesTotals();
    void deriveTilesEmptyIsZeroAndDash();
```

```cpp
void TestReportingViewModel::aggregateSumsAndRanksByCourse()
{
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"course":"BSIT","visits":"3"},
        {"course":"BSCE","visits":"10"},
        {"course":"BSIT","visits":"4"},
        {"course":"BSCE","visits":"5"}
    ])").array();
    const QList<BarsModel::Bar> bars = ReportingViewModel::aggregateVisitsByCourse(data);
    QCOMPARE(bars.size(), 2);
    // Ranked descending by total: BSCE=15 then BSIT=7.
    QCOMPARE(bars.at(0).label, QStringLiteral("BSCE"));
    QCOMPARE(bars.at(0).value, 15.0);
    QCOMPARE(bars.at(1).label, QStringLiteral("BSIT"));
    QCOMPARE(bars.at(1).value, 7.0);
}

void TestReportingViewModel::aggregateEmptyIsEmpty()
{
    QCOMPARE(ReportingViewModel::aggregateVisitsByCourse(QJsonArray()).size(), 0);
}

void TestReportingViewModel::deriveTilesComputesTotals()
{
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"course":"BSIT","visits":"3"},
        {"course":"BSCE","visits":"10"},
        {"course":"BSCE","visits":"5"}
    ])").array();
    const ReportingViewModel::Tiles t = ReportingViewModel::deriveTiles(data);
    QCOMPARE(t.totalVisits, 18);
    QCOMPARE(t.studentsShown, 3);
    QCOMPARE(t.topCourse, QStringLiteral("BSCE"));   // 15 > 3
}

void TestReportingViewModel::deriveTilesEmptyIsZeroAndDash()
{
    const ReportingViewModel::Tiles t = ReportingViewModel::deriveTiles(QJsonArray());
    QCOMPARE(t.totalVisits, 0);
    QCOMPARE(t.studentsShown, 0);
    QCOMPARE(t.topCourse, QStringLiteral("—"));
}
```

Add `#include <QJsonDocument>` at the top of the test if not already present.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build C:/b/loams-4b -t tst_reportingviewmodel && ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: FAIL — aggregate returns empty, tiles return zeros/"" (stub bodies), so the ranked/topCourse asserts fail.

- [ ] **Step 3: Implement (replace the two stub bodies)**

In `ReportingViewModel.cpp`, add includes at the top:

```cpp
#include <QMap>
#include <algorithm>
```

Replace `QList<BarsModel::Bar> ReportingViewModel::aggregateVisitsByCourse(const QJsonArray &) { return {}; }` with:

```cpp
QList<BarsModel::Bar> ReportingViewModel::aggregateVisitsByCourse(const QJsonArray &data)
{
    QMap<QString, int> byCourse;   // sorted by key; we re-sort by value below
    for (const QJsonValue &v : data) {
        const QJsonObject o = v.toObject();
        const QString course = o.value("course").toString();
        byCourse[course] += reportVisits(o);
    }
    QList<BarsModel::Bar> bars;
    bars.reserve(byCourse.size());
    for (auto it = byCourse.cbegin(); it != byCourse.cend(); ++it)
        bars.append({ it.key(), double(it.value()) });
    // Rank descending by value; stable so equal totals keep name (key) order.
    std::stable_sort(bars.begin(), bars.end(),
                     [](const BarsModel::Bar &a, const BarsModel::Bar &b) { return a.value > b.value; });
    return bars;
}
```

Replace `ReportingViewModel::Tiles ReportingViewModel::deriveTiles(const QJsonArray &) { return {}; }` with:

```cpp
ReportingViewModel::Tiles ReportingViewModel::deriveTiles(const QJsonArray &data)
{
    Tiles t;
    t.studentsShown = data.size();
    t.topCourse = QStringLiteral("—");
    if (data.isEmpty())
        return t;
    const QList<BarsModel::Bar> bars = aggregateVisitsByCourse(data);
    for (const BarsModel::Bar &b : bars)
        t.totalVisits += int(b.value);
    if (!bars.isEmpty())
        t.topCourse = bars.first().label;   // already ranked descending
    return t;
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (9 functions now).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): pure visits-by-course aggregation + stat-tile derivation"
```

---

### Task 4: Filter/duration state + `canGenerate` gate

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` (replace setter + `canGenerate` stubs)
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp` (add functions)

**Interfaces:**
- Consumes: `parseDate()` (private helper, ReportingViewModel.h).
- Produces: writable duration state + `canGenerate` semantics. Consumed by `generateReport()` (Task 6) and the screen (Task 8).

- [ ] **Step 1: Add the failing tests**

Add to the slots list + body:

```cpp
    void canGenerateFalseWithoutDepartment();
    void canGenerateDayRequiresValidDate();
    void canGenerateMonthRequiresMonthAndYear();
    void canGenerateSemesterRequiresSemesterAndYear();
    void canGenerateCustomRequiresOrderedRange();
    void settersEmitAndUpdateCanGenerate();
```

```cpp
void TestReportingViewModel::canGenerateFalseWithoutDepartment()
{
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(!vm.canGenerate());          // no department
}

void TestReportingViewModel::canGenerateDayRequiresValidDate()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");              // NOTE: in this task setDepartment only stores + fires network; see Task 5
    vm.setDurationType(0);
    QVERIFY(!vm.canGenerate());          // no day yet
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.setDay("not-a-date");
    QVERIFY(!vm.canGenerate());
}

void TestReportingViewModel::canGenerateMonthRequiresMonthAndYear()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(1);
    vm.setMonth(2);
    QVERIFY(!vm.canGenerate());          // no year
    vm.setMonthYear(2026);
    QVERIFY(vm.canGenerate());
    vm.setMonth(0);
    QVERIFY(!vm.canGenerate());          // month out of 1..12
}

void TestReportingViewModel::canGenerateSemesterRequiresSemesterAndYear()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(2);
    vm.setSemester("First Semester");
    QVERIFY(!vm.canGenerate());          // no year
    vm.setSemYear(2026);
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::canGenerateCustomRequiresOrderedRange()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(3);
    vm.setCustomStart("2026-03-31");
    vm.setCustomEnd("2026-01-01");
    QVERIFY(!vm.canGenerate());          // start > end
    vm.setCustomEnd("2026-06-30");
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::settersEmitAndUpdateCanGenerate()
{
    ReportingViewModel vm;
    QSignalSpy canGenSpy(&vm, &ReportingViewModel::canGenerateChanged);
    QSignalSpy durSpy(&vm, &ReportingViewModel::durationTypeChanged);
    vm.setDurationType(3);
    QVERIFY(durSpy.count() >= 1);
    vm.setDepartment("CE");
    vm.setCustomStart("2026-01-01");
    vm.setCustomEnd("2026-02-01");
    QVERIFY(vm.canGenerate());
    QVERIFY(canGenSpy.count() >= 1);
}
```

> `setDepartment("CE")` in these tests exercises only the state-store + `canGenerate` recompute. It ALSO fires `ReportController::loadCourses` (a fire-and-forget GET that never resolves in-test — harmless, mirrors `tst_databaseviewmodel`). The full cascade behavior is Task 5.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build C:/b/loams-4b -t tst_reportingviewmodel && ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: FAIL — `canGenerate()` returns false always (stub); setters are no-ops.

- [ ] **Step 3: Implement**

In `ReportingViewModel.cpp`, replace `bool ReportingViewModel::canGenerate() const { return false; }` with:

```cpp
bool ReportingViewModel::canGenerate() const
{
    if (m_department.isEmpty() || m_loading)
        return false;
    switch (m_durationType) {
    case 0: return parseDate(m_day).isValid();
    case 1: return m_month >= 1 && m_month <= 12 && m_monthYear > 0;
    case 2: return !m_semester.isEmpty() && m_semYear > 0;
    case 3: {
        const QDate s = parseDate(m_customStart), e = parseDate(m_customEnd);
        return s.isValid() && e.isValid() && s <= e;
    }
    default: return false;
    }
}
```

Replace the duration setter stubs with real setters. Each stores, emits its own change signal, and emits `canGenerateChanged()`. Replace the stub lines for `setDurationType/setDay/setMonth/setMonthYear/setSemester/setSemYear/setCustomStart/setCustomEnd`:

```cpp
void ReportingViewModel::setDurationType(int t)
{
    if (m_durationType == t) return;
    m_durationType = t;
    emit durationTypeChanged();
    emit canGenerateChanged();
}
void ReportingViewModel::setDay(const QString &v)
{
    if (m_day == v) return;
    m_day = v; emit dayChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setMonth(int v)
{
    if (m_month == v) return;
    m_month = v; emit monthChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setMonthYear(int v)
{
    if (m_monthYear == v) return;
    m_monthYear = v; emit monthYearChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setSemester(const QString &v)
{
    if (m_semester == v) return;
    m_semester = v; emit semesterChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setSemYear(int v)
{
    if (m_semYear == v) return;
    m_semYear = v; emit semYearChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setCustomStart(const QString &v)
{
    if (m_customStart == v) return;
    m_customStart = v; emit customStartChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setCustomEnd(const QString &v)
{
    if (m_customEnd == v) return;
    m_customEnd = v; emit customEndChanged(); emit canGenerateChanged();
}
```

Also replace the `setLoading`/`setError` stubs (needed so `canGenerate`'s `m_loading` term is reachable and Task 6 can reuse them):

```cpp
void ReportingViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
    emit canGenerateChanged();   // loading gates canGenerate
}
void ReportingViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}
```

Leave `setDepartment`/`setCourse`/bootstrap/report stubs as-is for now — Task 5/6 replace them. But so these tests link, give `setDepartment` a MINIMAL body now (store + emit + canGenerate + kick loadCourses); Task 5 will extend it with the dependent-clear. Replace the `setDepartment` stub:

```cpp
void ReportingViewModel::setDepartment(const QString &department)
{
    if (m_department == department) return;
    m_department = department;
    m_course.clear();                 // dependent-clear (finalized in Task 5)
    emit departmentChanged();
    emit courseChanged();
    emit canGenerateChanged();
    if (!department.isEmpty())
        m_controller->loadCourses(department);
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (15 functions).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): filter/duration state + canGenerate gate (loading-aware)"
```

---

### Task 5: Bootstrap + cascade wiring (departments, years, courses)

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` (ctor signal wiring; replace `loadDepartments`/`setCourse`/`onDepartmentsLoaded`/`onYearsLoaded`/`onCoursesLoaded` stubs)
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp` (add functions)

**Interfaces:**
- Consumes: `ReportController::loadDepartments/loadYears/loadCourses` + their signals.
- Produces: populated `departments`/`years`/`courses` state, driven by the controller signals; `loadDepartments()` bootstraps both departments and years.

- [ ] **Step 1: Add the failing tests**

```cpp
    void onDepartmentsLoadedPopulates();
    void onYearsLoadedPopulates();
    void onCoursesLoadedPopulates();
    void setDepartmentClearsCourse();
```

```cpp
void TestReportingViewModel::onDepartmentsLoadedPopulates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::departmentsChanged);
    vm.onDepartmentsLoaded({ "CE", "IT" });
    QCOMPARE(vm.departments(), QStringList({ "CE", "IT" }));
    QVERIFY(spy.count() >= 1);
}

void TestReportingViewModel::onYearsLoadedPopulates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::yearsChanged);
    vm.onYearsLoaded({ "2026", "2025" });
    QCOMPARE(vm.years(), QStringList({ "2026", "2025" }));
    QVERIFY(spy.count() >= 1);
}

void TestReportingViewModel::onCoursesLoadedPopulates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::coursesChanged);
    vm.onCoursesLoaded({ "BSCE", "BSEE" });
    QCOMPARE(vm.courses(), QStringList({ "BSCE", "BSEE" }));
    QVERIFY(spy.count() >= 1);
}

void TestReportingViewModel::setDepartmentClearsCourse()
{
    ReportingViewModel vm;
    vm.onCoursesLoaded({ "BSCE" });
    vm.setCourse("BSCE");
    QCOMPARE(vm.course(), QStringLiteral("BSCE"));
    vm.setDepartment("IT");
    QCOMPARE(vm.course(), QString());     // dependent-clear
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build C:/b/loams-4b -t tst_reportingviewmodel && ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: FAIL — `onDepartmentsLoaded`/`onYearsLoaded`/`onCoursesLoaded` are no-op stubs; `setCourse` is a stub.

- [ ] **Step 3: Implement**

In the ctor, wire the controller signals to the slots (replace the `// Signal wiring added in Task 5/6.` comment):

```cpp
    connect(m_controller, &ReportController::departmentsLoaded,
            this, &ReportingViewModel::onDepartmentsLoaded);
    connect(m_controller, &ReportController::yearsLoaded,
            this, &ReportingViewModel::onYearsLoaded);
    connect(m_controller, &ReportController::coursesLoaded,
            this, &ReportingViewModel::onCoursesLoaded);
    connect(m_controller, &ReportController::reportDataReady,
            this, &ReportingViewModel::onReportDataReady);
    connect(m_controller, &ReportController::reportError,
            this, &ReportingViewModel::onReportError);
    connect(m_controller, &ReportController::loadError,
            this, &ReportingViewModel::onLoadError);
```

Replace the `loadDepartments` stub:

```cpp
void ReportingViewModel::loadDepartments()
{
    m_controller->loadDepartments();
    m_controller->loadYears();
}
```

Replace the `setCourse` stub:

```cpp
void ReportingViewModel::setCourse(const QString &course)
{
    if (m_course == course) return;
    m_course = course;
    emit courseChanged();
    emit canGenerateChanged();
}
```

Replace the three loaded-slot stubs:

```cpp
void ReportingViewModel::onDepartmentsLoaded(const QStringList &departments)
{
    m_departments = departments;
    emit departmentsChanged();
}
void ReportingViewModel::onYearsLoaded(const QStringList &years)
{
    m_years = years;
    emit yearsChanged();
}
void ReportingViewModel::onCoursesLoaded(const QStringList &courses)
{
    m_courses = courses;
    emit coursesChanged();
}
```

(`setDepartment` already clears the course from Task 4 — no change needed.)

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (19 functions).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): bootstrap departments+years and Dept->Course cascade wiring"
```

---

### Task 6: Report fetch — `generateReport` (single-in-flight) + result/error slots

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` (replace `generateReport`/`retry`/`onReportDataReady`/`onReportError`/`onLoadError`/`applyResult` stubs)
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp` (add functions)

**Interfaces:**
- Consumes: `buildFilters`, `aggregateVisitsByCourse`, `deriveTiles`, `ReportController::fetchReportRows`.
- Produces: the run/result path — populated `rows`/`courseBars`/tiles + `hasResult`, error handling, single-in-flight.

- [ ] **Step 1: Add the failing tests**

```cpp
    void onReportDataReadyPopulatesPreview();
    void onReportDataReadyEmptyIsSuccessNotError();
    void onReportErrorSetsErrorClearsLoading();
    void generateWhileLoadingIsNoop();
```

```cpp
void TestReportingViewModel::onReportDataReadyPopulatesPreview()
{
    ReportingViewModel vm;
    QSignalSpy resultSpy(&vm, &ReportingViewModel::resultChanged);
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"name":"A","course":"BSCE","year_level":"1","visits":"10"},
        {"name":"B","course":"BSIT","year_level":"2","visits":"4"}
    ])").array();
    vm.onReportDataReady(data);
    QVERIFY(vm.hasResult());
    QCOMPARE(vm.rows()->count(), 2);
    QCOMPARE(vm.courseBars()->rowCount(), 2);
    QCOMPARE(vm.totalVisits(), 14);
    QCOMPARE(vm.studentsShown(), 2);
    QCOMPARE(vm.topCourse(), QStringLiteral("BSCE"));
    QVERIFY(!vm.loading());
    QVERIFY(vm.errorText().isEmpty());
    QVERIFY(resultSpy.count() >= 1);
}

void TestReportingViewModel::onReportDataReadyEmptyIsSuccessNotError()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonArray());
    QVERIFY(vm.hasResult());
    QCOMPARE(vm.rows()->count(), 0);
    QCOMPARE(vm.totalVisits(), 0);
    QCOMPARE(vm.topCourse(), QStringLiteral("—"));
    QVERIFY(vm.errorText().isEmpty());   // empty result is NOT an error
}

void TestReportingViewModel::onReportErrorSetsErrorClearsLoading()
{
    ReportingViewModel vm;
    vm.onReportError("Department is required", false);
    QCOMPARE(vm.errorText(), QStringLiteral("Department is required"));
    QVERIFY(!vm.loading());
}

void TestReportingViewModel::generateWhileLoadingIsNoop()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.generateReport();                 // fires; sets loading true
    QVERIFY(vm.loading());
    QVERIFY(!vm.canGenerate());          // gated while loading
    // A second call while loading must not clear/replace state.
    vm.generateReport();                 // no-op
    QVERIFY(vm.loading());
    // A result clears loading and re-enables generate.
    vm.onReportDataReady(QJsonArray());
    QVERIFY(!vm.loading());
    QVERIFY(vm.canGenerate());
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build C:/b/loams-4b -t tst_reportingviewmodel && ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: FAIL — result/error slots + `generateReport` are stubs.

- [ ] **Step 3: Implement**

Add includes at top of `ReportingViewModel.cpp` if not present: `#include "reportcontroller.h"` (already there). Replace the stub bodies:

```cpp
void ReportingViewModel::generateReport()
{
    if (!canGenerate())          // includes the !loading gate → single-in-flight
        return;
    setError(QString());
    setLoading(true);
    const QJsonObject filters = buildFilters(
        m_department, m_course, m_durationType,
        parseDate(m_day), m_month, m_monthYear,
        m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd));
    m_controller->fetchReportRows(filters);
}

void ReportingViewModel::retry()
{
    generateReport();            // re-runs with the current filter state
}

void ReportingViewModel::onReportDataReady(const QJsonArray &data)
{
    setLoading(false);
    applyResult(data);
}

void ReportingViewModel::onReportError(const QString &message, bool /*critical*/)
{
    setLoading(false);
    setError(message.isEmpty() ? QStringLiteral("Report failed. Please try again.") : message);
}

void ReportingViewModel::onLoadError(const QString &/*title*/, const QString &message, bool /*critical*/)
{
    // Bootstrap (departments/years/courses) failures surface in the same banner.
    setError(message.isEmpty() ? QStringLiteral("Failed to load filters. Please try again.") : message);
}

void ReportingViewModel::applyResult(const QJsonArray &data)
{
    m_rows.setRows(data);
    m_courseBars.setBars(aggregateVisitsByCourse(data));
    const Tiles t = deriveTiles(data);
    m_totalVisits = t.totalVisits;
    m_studentsShown = t.studentsShown;
    m_topCourse = t.topCourse;
    m_hasResult = true;
    setError(QString());
    emit resultChanged();
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (23 functions).

- [ ] **Step 5: Full ctest to confirm no regressions**

Run: `cmake --build C:/b/loams-4b && ctest --test-dir C:/b/loams-4b --output-on-failure`
Expected: all targets PASS (existing floor + `tst_reportrowsmodel` + `tst_reportingviewmodel`).

- [ ] **Step 6: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): generateReport single-in-flight fetch + result/error slots"
```

---

### Task 7: `LDatePicker` native calendar component

**Files:**
- Create: `qt-app/quick/qml/components/LDatePicker.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (add `qml/components/LDatePicker.qml` to `QML_FILES`)
- Test: `qt-app/quick/tests/tst_qml_components.qml` (add an `LDatePicker` fixture + test functions)

**Interfaces:**
- Produces: `LDatePicker` with `property string selectedDate` (`"yyyy-MM-dd"`, empty = unset), `property string placeholder`, signal `picked(string dateString)`, and `Q_INVOKABLE`-style QML functions `open()`, `close()`, `showMonth(year, month)`. Day cells carry `objectName: "dayCell_" + d`. Consumed by `ReportingScreen` (Task 8).

- [ ] **Step 1: Write the failing QuickTest**

In `qt-app/quick/tests/tst_qml_components.qml`, raise the host height so the new fixture gets its own band: change `width: 400; height: 3160` (line ~15) to `width: 400; height: 3600`.

Add the fixture in its OWN y-band below the last one (`chk` at `y: 3090`) — this file's convention is that every interactive fixture gets a non-overlapping band or it "silently absorbs synthetic mouse events meant for the other one". Add near the end of `host`, after the LCheckbox fixture:

```qml
    // --- LDatePicker fixture (own band below chk) ---
    LDatePicker { id: datePicker; y: 3200; width: 280; height: 44 }
    SignalSpy { id: datePickerSpy; target: datePicker; signalName: "picked" }
```

Add a NEW `TestCase` block (not folded into an existing one — the `mouseClick`-free
tests still need `when: windowShown` for the fixture window). Selection is driven
through the `selectDay()` function, NOT a click inside the Controls `Popup` (which
QuickTest cannot reliably hit — this repo has no precedent for `findChild`+`mouseClick`
reaching a Popup's reparented overlay content):

```qml
    TestCase {
        name: "LDatePicker"; when: windowShown
        function init() { datePickerSpy.clear(); datePicker.selectedDate = ""; }

        function test_selectDayEmitsIsoDateAndSetsSelection() {
            datePicker.showMonth(2026, 8);       // August 2026
            datePicker.selectDay(14);
            compare(datePicker.selectedDate, "2026-08-14");
            compare(datePickerSpy.count, 1);
            compare(datePickerSpy.signalArguments[0][0], "2026-08-14");
        }

        function test_navigatesMonths() {
            datePicker.showMonth(2026, 8);
            datePicker.nextMonth();
            compare(datePicker.displayYear, 2026);
            compare(datePicker.displayMonth, 9);
            datePicker.prevMonth();
            datePicker.prevMonth();
            compare(datePicker.displayMonth, 7);
        }

        function test_navigatesAcrossYearBoundary() {
            datePicker.showMonth(2026, 12);
            datePicker.nextMonth();
            compare(datePicker.displayYear, 2027);
            compare(datePicker.displayMonth, 1);
        }

        function test_fieldShowsSelection() {
            datePicker.selectedDate = "2026-08-14";
            var field = findChild(datePicker, "datePickerField");   // a root child, not in the popup
            verify(field, "field text element exists");
            compare(field.text, "2026-08-14");
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build C:/b/loams-4b -t tst_qml_components && ctest --test-dir C:/b/loams-4b -R tst_qml_components --output-on-failure`
Expected: FAIL — `LDatePicker` is not a type / cannot be created.

- [ ] **Step 3: Implement `LDatePicker.qml`**

Create `qt-app/quick/qml/components/LDatePicker.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Native themed date picker (Phase 4b-i). A field showing the selected ISO
// date that opens a calendar-grid popup. Colors from Theme tokens only.
// selectedDate is "yyyy-MM-dd" ("" = unset). Emits picked(dateString) on a
// day tap. displayYear/displayMonth (1..12) drive the shown grid.
Item {
    id: root
    property string selectedDate: ""
    property string placeholder: qsTr("Pick a date")
    property int displayYear: (new Date()).getFullYear()
    property int displayMonth: (new Date()).getMonth() + 1   // 1..12
    signal picked(string dateString)

    implicitWidth: 280
    implicitHeight: 44

    function open() { popup.open() }
    function close() { popup.close() }
    function showMonth(y, m) { root.displayYear = y; root.displayMonth = m }
    function nextMonth() {
        if (root.displayMonth === 12) { root.displayMonth = 1; root.displayYear++ }
        else root.displayMonth++
    }
    function prevMonth() {
        if (root.displayMonth === 1) { root.displayMonth = 12; root.displayYear-- }
        else root.displayMonth--
    }
    // Popup-independent selection seam: the cell MouseArea AND the QuickTest
    // both call this, so day-selection is testable without driving a click
    // inside the Controls Popup overlay (which QuickTest cannot reliably hit).
    function selectDay(d) {
        var iso = root.displayYear + "-" + root._pad(root.displayMonth) + "-" + root._pad(d);
        root.selectedDate = iso;
        root.picked(iso);
        root.close();
    }

    // Two-digit zero-pad without printf: "8" -> "08".
    function _pad(n) { return (n < 10 ? "0" : "") + n }

    // Leading-blank count + day count for the shown month via JS Date.
    readonly property int _firstDow: (new Date(root.displayYear, root.displayMonth - 1, 1)).getDay() // 0=Sun
    readonly property int _daysInMonth: (new Date(root.displayYear, root.displayMonth, 0)).getDate()
    readonly property var _cells: {
        var out = [];
        for (var b = 0; b < root._firstDow; ++b) out.push(0);        // blanks
        for (var d = 1; d <= root._daysInMonth; ++d) out.push(d);
        return out;
    }
    readonly property var _monthNames: ["January","February","March","April","May","June",
                                        "July","August","September","October","November","December"]

    // The field (acts as a button).
    Rectangle {
        id: field
        anchors.fill: parent
        radius: Theme.radius.sm
        color: Theme.card
        border.width: 2
        border.color: popup.opened ? Theme.brand.base : Theme.border
        Text {
            id: fieldText
            objectName: "datePickerField"
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing.md
            anchors.rightMargin: Theme.spacing.md
            verticalAlignment: Text.AlignVCenter
            text: root.selectedDate.length > 0 ? root.selectedDate : root.placeholder
            textFormat: Text.PlainText
            color: root.selectedDate.length > 0 ? Theme.text : Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            elide: Text.ElideRight
        }
        MouseArea { anchors.fill: parent; onClicked: root.open() }
    }

    Popup {
        id: popup
        y: field.height + Theme.spacing.xs
        width: 300
        padding: Theme.spacing.md
        background: Rectangle {
            radius: Theme.radius.md
            color: Theme.card
            border.width: 1
            border.color: Theme.border
        }
        ColumnLayout {
            spacing: Theme.spacing.sm
            width: parent.width

            // Header: ‹ Month Year ›
            RowLayout {
                Layout.fillWidth: true
                LButton {
                    objectName: "datePrevMonth"
                    variant: "Ghost"; compact: true; text: "‹"
                    onClicked: root.prevMonth()
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root._monthNames[root.displayMonth - 1] + " " + root.displayYear
                    color: Theme.text
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                    font.weight: Font.ExtraBold
                }
                LButton {
                    objectName: "dateNextMonth"
                    variant: "Ghost"; compact: true; text: "›"
                    onClicked: root.nextMonth()
                }
            }

            // Weekday headers.
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                columnSpacing: 0; rowSpacing: 0
                Repeater {
                    model: ["Su","Mo","Tu","We","Th","Fr","Sa"]
                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                    }
                }
            }

            // Day grid.
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                columnSpacing: 0; rowSpacing: 0
                Repeater {
                    model: root._cells
                    delegate: Item {
                        required property var modelData    // 0 = blank
                        Layout.fillWidth: true
                        implicitHeight: 34
                        Rectangle {
                            visible: modelData > 0
                            objectName: modelData > 0 ? ("dayCell_" + modelData) : ""
                            anchors.centerIn: parent
                            width: 30; height: 30
                            radius: Theme.radius.sm
                            readonly property bool isSelected:
                                root.selectedDate === (root.displayYear + "-" + root._pad(root.displayMonth) + "-" + root._pad(modelData))
                            color: isSelected ? Theme.brand.base
                                              : (cellHover.hovered ? Qt.alpha(Theme.brand.base, 0.10) : "transparent")
                            HoverHandler { id: cellHover }
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                textFormat: Text.PlainText
                                color: parent.isSelected ? Theme.brand.on : Theme.text
                                font.family: Theme.typography.sans
                                font.pixelSize: Theme.typography.body
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.selectDay(modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
```

Register the QML file — add to `qt-app/quick/CMakeLists.txt` `QML_FILES` (after `qml/components/LCheckbox.qml`):

```cmake
        qml/components/LDatePicker.qml
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build C:/b/loams-4b -t tst_qml_components && ctest --test-dir C:/b/loams-4b -R tst_qml_components --output-on-failure`
Expected: PASS (existing component tests + the 3 new date-picker functions).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/qml/components/LDatePicker.qml qt-app/quick/tests/tst_qml_components.qml qt-app/quick/CMakeLists.txt
git commit -m "feat(reporting): native themed LDatePicker calendar component"
```

---

### Task 8: `ReportingScreen.qml` — filter panel + native preview

**Files:**
- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml` (replace placeholder with the full screen)
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (raise host height to `8100`; add a reporting stub VM + fixture in the `7300..8100` band + test functions)

**Interfaces:**
- Consumes: `vm` (a `ReportingViewModel` or a plain-QML stub) with the properties/invokables from Tasks 2–6; `LCascadingSelect`, `LComboBox`, `LDatePicker`, `LButton`, `LCard`, `LStatTile`, `LBarChart`, `LTable`.
- Produces: the working screen. `objectName: "reportingPage"`; `generateButton`; `reportTable`; `reportBarChart`; error/empty blocks with the standing objectNames.

- [ ] **Step 1: Write the failing QuickTest fixture + functions**

In `tst_qml_admin.qml`: change `height: 7300` to `height: 8100`, and extend the geometry-ledger comment to add `| reporting 7300..8100`.

Add the stub VM + fixture (place after the import-dialog fixtures, near the bottom of `host`):

```qml
    // --- Reporting stub VM + screen (band y 7300..8100) ---
    ListModel { id: reportRowsStub
        ListElement { name: "Maria Santos"; course: "BSCE"; year: "3"; visits: 42 }
        ListElement { name: "Jose Cruz"; course: "BSIT"; year: "1"; visits: 7 }
    }
    ListModel { id: reportBarsStub
        property real maxValue: 42
        ListElement { label: "BSCE"; value: 42 }
        ListElement { label: "BSIT"; value: 7 }
    }
    QtObject {
        id: reportingStub
        property var departments: ["CE", "IT"]
        property var courses: ["BSCE", "BSEE"]
        property var years: ["2026", "2025"]
        property string department: ""
        property string course: ""
        property int durationType: 0
        property string day: ""
        property int month: 0
        property int monthYear: 0
        property string semester: ""
        property int semYear: 0
        property string customStart: ""
        property string customEnd: ""
        property bool canGenerate: false
        property bool loading: false
        property string errorText: ""
        property bool hasResult: true
        property var rows: reportRowsStub
        property var courseBars: reportBarsStub
        property int totalVisits: 49
        property int studentsShown: 2
        property string topCourse: "BSCE"
        property int generateCount: 0
        property int loadDepartmentsCount: 0
        function loadDepartments() { loadDepartmentsCount++ }
        function setDepartment(d) { department = d; course = ""; canGenerate = (d !== "") }
        function setCourse(c) { course = c }
        function setDurationType(t) { durationType = t }
        function setDay(v) { day = v }
        function setMonth(v) { month = v }
        function setMonthYear(v) { monthYear = v }
        function setSemester(v) { semester = v }
        function setSemYear(v) { semYear = v }
        function setCustomStart(v) { customStart = v }
        function setCustomEnd(v) { customEnd = v }
        function generateReport() { generateCount++ }
        function retry() { generateCount++ }
    }
    ReportingScreen { id: reporting; x: 0; y: 7300; width: 1100; height: 800; vm: reportingStub }

    // A vm-less instance to cover the `vm ? ... : ...` fallback path.
    ReportingScreen { id: vmlessReporting; x: 2000; y: 7300; width: 1100; height: 800 }
```

Add a NEW `TestCase` block (this file groups each screen's tests in its own
`TestCase { … when: windowShown }` — required because `test_reportingGenerateInvokesVm`
uses `mouseClick`). Place it near the other admin-screen TestCase blocks:

```qml
    TestCase {
        name: "ReportingScreen"; when: windowShown
        function init() {
            reportingStub.canGenerate = false;
            reportingStub.durationType = 0;
            reportingStub.hasResult = true;
        }

        function test_generateDisabledUntilCanGenerate() {
            reportingStub.canGenerate = false;
            var btn = findChild(reporting, "generateButton");
            verify(btn, "generate button exists");
            verify(!btn.enabled, "disabled when canGenerate false");
            reportingStub.canGenerate = true;
            verify(btn.enabled, "enabled when canGenerate true");
        }

        function test_generateInvokesVm() {
            reportingStub.canGenerate = true;
            var before = reportingStub.generateCount;
            var btn = findChild(reporting, "generateButton");
            mouseClick(btn);
            compare(reportingStub.generateCount, before + 1);
        }

        function test_previewRendersRowsAndTiles() {
            reportingStub.hasResult = true;
            var table = findChild(reporting, "reportTable");
            verify(table, "report table exists");
            compare(table.rowCount, 2);
            var tile = findChild(reporting, "totalVisitsTile");
            verify(tile, "total-visits tile exists");
            compare(tile.value, "49");
        }

        function test_durationSwapShowsCustomPickers() {
            reportingStub.durationType = 0;      // Day mode
            var startPicker = findChild(reporting, "customStartPicker");
            verify(startPicker, "picker exists as a QObject regardless of mode");
            verify(!startPicker.visible, "custom start picker hidden in Day mode");
            reportingStub.durationType = 3;      // Custom mode
            verify(startPicker.visible, "custom start picker shown in Custom mode");
        }

        function test_vmlessDoesNotCrash() {
            verify(vmlessReporting !== null, "vm-less screen instantiates");
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build C:/b/loams-4b -t tst_qml_admin && ctest --test-dir C:/b/loams-4b -R tst_qml_admin --output-on-failure`
Expected: FAIL — the placeholder `ReportingScreen` has no `generateButton`/`reportTable`/`totalVisitsTile`.

- [ ] **Step 3: Implement `ReportingScreen.qml`**

Replace the entire contents of `qt-app/quick/qml/admin/ReportingScreen.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Reporting (spec 4b-i): Dept->Course + duration filters and a native-QML
// preview (stat tiles + visits-by-course bar chart + per-student table). Run
// by an explicit Generate button. Takes `property var vm` (a ReportingViewModel
// or a plain-QML stub in QuickTests). No Component.onCompleted fetch — the
// initial bootstrap is issued by AdminScreen's Loader.onLoaded gate.
Rectangle {
    id: screen
    property var vm
    color: Theme.appBackground

    readonly property bool isLoading: vm ? vm.loading : false
    readonly property bool isError: vm ? vm.errorText.length > 0 : false
    readonly property bool showPreview: vm ? (vm.hasResult && !screen.isError) : false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // --- Filter card (self-sizing Rectangle: LCard has a fixed
        // implicitHeight of 96 and does NOT grow to fit slotted content, so
        // the two-row filter would clip — mirror DatabaseScreen's filter card). ---
        Rectangle {
            Layout.fillWidth: true
            color: Theme.card; radius: Theme.radius.card
            border.width: 2; border.color: Theme.border
            implicitHeight: filterCol.implicitHeight + Theme.spacing.xl * 2
            ColumnLayout {
                id: filterCol
                anchors.fill: parent
                anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md

                LCascadingSelect {
                    id: cascade
                    Layout.fillWidth: true
                    departments: screen.vm ? screen.vm.departments : []
                    courses: screen.vm ? screen.vm.courses : []
                    department: screen.vm ? screen.vm.department : ""
                    course: screen.vm ? screen.vm.course : ""
                    onDepartmentPicked: function(d) { if (screen.vm) screen.vm.setDepartment(d); }
                    onCoursePicked: function(c) { if (screen.vm) screen.vm.setCourse(c); }
                }

                // Duration selector + mode-specific sub-controls.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.md

                    LComboBox {
                        id: durationCombo
                        objectName: "durationCombo"
                        Layout.preferredWidth: 160
                        model: [qsTr("Day"), qsTr("Month"), qsTr("Semester"), qsTr("Custom")]
                        placeholder: qsTr("Duration")
                        currentValue: model[screen.vm ? screen.vm.durationType : 0]
                        onSelected: function(v) {
                            if (screen.vm) screen.vm.setDurationType(durationCombo.model.indexOf(v));
                        }
                    }

                    // Day mode.
                    LDatePicker {
                        objectName: "dayPicker"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 0
                        Layout.preferredWidth: 220
                        placeholder: qsTr("Pick a day")
                        selectedDate: screen.vm ? screen.vm.day : ""
                        onPicked: function(d) { if (screen.vm) screen.vm.setDay(d); }
                    }

                    // Month mode.
                    LComboBox {
                        objectName: "monthCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 1
                        Layout.preferredWidth: 150
                        model: [qsTr("January"), qsTr("February"), qsTr("March"), qsTr("April"),
                                qsTr("May"), qsTr("June"), qsTr("July"), qsTr("August"),
                                qsTr("September"), qsTr("October"), qsTr("November"), qsTr("December")]
                        placeholder: qsTr("Month")
                        onSelected: function(v) {
                            if (screen.vm) screen.vm.setMonth(model.indexOf(v) + 1);   // 1..12
                        }
                    }
                    LComboBox {
                        objectName: "monthYearCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 1
                        Layout.preferredWidth: 120
                        model: screen.vm ? screen.vm.years : []
                        placeholder: qsTr("Year")
                        onSelected: function(v) { if (screen.vm) screen.vm.setMonthYear(parseInt(v)); }
                    }

                    // Semester mode.
                    LComboBox {
                        objectName: "semesterCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 2
                        Layout.preferredWidth: 180
                        model: [qsTr("First Semester"), qsTr("Second Semester"), qsTr("Summer")]
                        placeholder: qsTr("Semester")
                        onSelected: function(v) { if (screen.vm) screen.vm.setSemester(v); }
                    }
                    LComboBox {
                        objectName: "semYearCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 2
                        Layout.preferredWidth: 120
                        model: screen.vm ? screen.vm.years : []
                        placeholder: qsTr("Year")
                        onSelected: function(v) { if (screen.vm) screen.vm.setSemYear(parseInt(v)); }
                    }

                    // Custom mode.
                    LDatePicker {
                        objectName: "customStartPicker"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 3
                        Layout.preferredWidth: 200
                        placeholder: qsTr("Start date")
                        selectedDate: screen.vm ? screen.vm.customStart : ""
                        onPicked: function(d) { if (screen.vm) screen.vm.setCustomStart(d); }
                    }
                    LDatePicker {
                        objectName: "customEndPicker"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 3
                        Layout.preferredWidth: 200
                        placeholder: qsTr("End date")
                        selectedDate: screen.vm ? screen.vm.customEnd : ""
                        onPicked: function(d) { if (screen.vm) screen.vm.setCustomEnd(d); }
                    }

                    Item { Layout.fillWidth: true }   // spacer

                    LButton {
                        objectName: "generateButton"
                        text: qsTr("Generate Report")
                        enabled: screen.vm ? screen.vm.canGenerate : false
                        onClicked: if (screen.vm) screen.vm.generateReport()
                    }
                }
            }
        }

        // --- Error + retry block ---
        RowLayout {
            Layout.fillWidth: true
            visible: screen.isError
            spacing: Theme.spacing.md
            Text {
                objectName: "reportErrorText"
                Layout.fillWidth: true
                text: screen.vm ? screen.vm.errorText : ""
                textFormat: Text.PlainText     // server-echoed over cleartext HTTP
                color: Theme.error
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                wrapMode: Text.WordWrap
            }
            LButton {
                objectName: "reportRetryButton"
                variant: "Outline"; compact: true
                text: qsTr("Retry")
                onClicked: if (screen.vm) screen.vm.retry()
            }
        }

        // --- Preview (stat tiles + chart + table) ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.lg
            visible: screen.showPreview
            opacity: screen.isLoading ? 0.4 : 1.0

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.lg
                LStatTile {
                    objectName: "totalVisitsTile"
                    Layout.fillWidth: true
                    label: qsTr("TOTAL VISITS")
                    value: screen.vm ? String(screen.vm.totalVisits) : "0"
                }
                LStatTile {
                    objectName: "studentsShownTile"
                    Layout.fillWidth: true
                    label: qsTr("STUDENTS")
                    value: screen.vm ? String(screen.vm.studentsShown) : "0"
                }
                LStatTile {
                    objectName: "topCourseTile"
                    Layout.fillWidth: true
                    label: qsTr("TOP COURSE")
                    value: screen.vm ? screen.vm.topCourse : "—"
                }
            }

            LCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                LBarChart {
                    objectName: "reportBarChart"
                    anchors.fill: parent
                    orientation: "Horizontal"
                    model: screen.vm ? screen.vm.courseBars : null
                    maxValue: (screen.vm && screen.vm.courseBars) ? screen.vm.courseBars.maxValue : 100
                }
            }

            LTable {
                objectName: "reportTable"
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: [
                    { key: "name",   title: qsTr("Name"),   weight: 3 },
                    { key: "course", title: qsTr("Course"), weight: 2 },
                    { key: "year",   title: qsTr("Year"),   weight: 1 },
                    { key: "visits", title: qsTr("Visits"), weight: 1 }
                ]
                model: screen.vm ? screen.vm.rows : null
                emptyStateText: qsTr("No visits in this range")
            }
        }
    }
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build C:/b/loams-4b -t tst_qml_admin && ctest --test-dir C:/b/loams-4b -R tst_qml_admin --output-on-failure`
Expected: PASS (existing admin tests + the 5 new reporting functions).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/qml/admin/ReportingScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(reporting): ReportingScreen filter panel + native preview"
```

---

### Task 9: Wire the VM into `AdminScreen` + full build/smoke

**Files:**
- Modify: `qt-app/quick/qml/admin/AdminScreen.qml` (add VM instance + `vm:` binding)

**Interfaces:**
- Consumes: `ReportingViewModel` (`QML_ELEMENT`, resolves as `LOAMS` type), the `Loader.onLoaded` bootstrap gate (already calls `item.vm.loadDepartments()`).
- Produces: the reporting screen live in the running app.

- [ ] **Step 1: Add the VM instance**

In `qt-app/quick/qml/admin/AdminScreen.qml`, add alongside the other VM instances (after `SettingsViewModel { id: settingsVm }` at line ~30):

```qml
    ReportingViewModel { id: reportingVm }
```

- [ ] **Step 2: Bind the VM on the reporting component**

Replace `Component { id: reportingComponent; ReportingScreen { objectName: "reportingPage" } }` (line ~196) with:

```qml
    Component { id: reportingComponent; ReportingScreen { objectName: "reportingPage"; vm: reportingVm } }
```

(No change to `Loader.onLoaded` — it already calls `item.vm.loadDepartments()` when present, which bootstraps departments + years without fetching a report.)

- [ ] **Step 3: Build both executables + full ctest**

Run:

```bash
cmake --build C:/b/loams-4b
ctest --test-dir C:/b/loams-4b --output-on-failure
```

Expected: clean build of `WITSQuick` (and `WITS`); all ctest targets PASS. No new compiler warnings.

- [ ] **Step 4: Manual GUI smoke (owner or orchestrator, live XAMPP)**

Launch `WITSQuick.exe` from the build dir with XAMPP running. Verify: Reporting nav loads; Department combo populates; picking a department loads courses; Duration → Day/Month/Semester/Custom swaps the correct sub-controls; the year combos populate; Generate is disabled until Dept + a valid duration; clicking Generate renders the stat tiles, the visits-by-course bar chart, and the per-student table; an empty range shows the table empty-state (not an error); a backend error shows the error+Retry block. Note: this is a GUI app — a clean build is necessary but not sufficient.

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/qml/admin/AdminScreen.qml
git commit -m "feat(reporting): wire ReportingViewModel into AdminScreen"
```

---

## Self-Review

**1. Spec coverage.** Every spec section maps to a task:
- §4.1 MVVM / VM-wraps-controller → Tasks 2, 5, 6, 9.
- §4.2 `ReportingViewModel` surface (filter/duration/result state; the three pure statics `buildFilters`/`aggregateVisitsByCourse`/`deriveTiles`; single-in-flight) → Tasks 2, 3, 4, 6.
- §4.2 `ReportRowsModel` → Task 1. §4.3 `LDatePicker` → Task 7.
- §4.4 screen + shell wiring → Tasks 8, 9.
- §5.1 bootstrap (departments + years, no report fetch) → Task 5. §5.2 request mapping (durationType string, semester components) → Task 2. §5.3 `canGenerate` (loading-aware, Course optional) → Task 4.
- §6.1 C++ unit tests → Tasks 1–6. §6.2 QuickTests (screen enable/swap/preview/empty/error; LDatePicker) → Tasks 7, 8.
- §7 `fetchReportRows` (not `fetchPreviewData`) → Task 6. §8 constraints (Theme tokens, PlainText, read-only) → Tasks 7, 8. §9 semester convention → Task 2.

**2. Placeholder scan.** No "TBD"/"handle edge cases"/"similar to Task N". Every code step shows complete code. The Task 2 `ReportingViewModel.cpp` intentionally ships stub bodies that later tasks REPLACE — each replacement is spelled out with the exact stub line to remove and the real body to add.

**3. Type consistency.** `buildFilters` signature is identical in Task 2's header, Task 2's impl, and every call site (Task 6's `generateReport`). `Tiles{totalVisits, studentsShown, topCourse}` matches between the header, `deriveTiles` (Task 3), and `applyResult` (Task 6). `BarsModel::Bar{label,value}` matches `aggregateVisitsByCourse`'s return and `LBarChart`'s consumption. Model role name `year` (not `year_level`) is consistent between `ReportRowsModel::roleNames()` (Task 1), the QuickTest stub `reportRowsStub` (Task 8), and the `LTable` column `key: "year"` (Task 8). Screen objectNames (`generateButton`, `reportTable`, `totalVisitsTile`, `customStartPicker`, `reportErrorText`) match between the screen (Task 8) and its tests (Task 8). VM invokable names (`loadDepartments`, `setDepartment`, `setDurationType`, `generateReport`, `retry`) match between the header (Task 2), the screen bindings (Task 8), and the stub (Task 8).

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-08-14-loams2-phase4b-i-reporting.md`.
