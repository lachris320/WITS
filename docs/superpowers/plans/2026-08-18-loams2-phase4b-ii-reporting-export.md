# LOAMS 2.0 Phase 4b-ii — Reporting Export (PDF / Excel / Print) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an export bar to the Reporting screen — pick a palette + chart type, then Export PDF / Export Excel / Print — driving the existing stateless `ReportRenderer` over the rows 4b-i already fetched (no new endpoint, no refetch).

**Architecture:** `ReportingViewModel` (the only QML-facing C++ for this screen) gains export state, three pure static seams (`normalizeExportRows`, `semesterWindow`, `buildExportFilters`), a shared `renderToDevice()`, and `exportPdf/exportExcel/printReport`. Rendering runs on the GUI thread behind a busy overlay. **Prerequisite:** WITSQuick must run a `QApplication` (not `QGuiApplication`) because `ReportRenderer` draws charts via `QChartView` and print uses `QPrintDialog` — both `QWidget`s.

**Tech Stack:** Qt 6.11 (Quick + Widgets + Charts + PrintSupport), QXlsx (vendored), QPdfWriter (Qt::Gui), CMake + Ninja, QtTest + Qt Quick Test under ctest.

## Global Constraints

- **Spec:** `docs/superpowers/specs/2026-08-18-loams2-phase4b-ii-reporting-export-design.md` (claude-review APPROVED). Every task's requirements implicitly include the spec's §2.3 Design OS UX contract.
- **MVVM:** ViewModels are the ONLY QML-facing C++; QML never calls a `witscore` controller directly. The one accepted exception is `printReport()` owning `QPrintDialog` (no Quick-native print dialog).
- **Theming:** every color via `Theme.qml` tokens — **ZERO raw hex** outside `Theme.qml`; opacity variants use `Qt.alpha(Theme.<token>, a)`.
- **Security:** all server- or path-derived text renders `Text.PlainText` (cleartext HTTP).
- **Charts:** Bar + Pie only (line is unbacked by the endpoint). **Palettes:** Default / Blue / Green / Red only.
- **Settings:** read report header info through the `AppSettings` seam (`qt-app/core/appsettings.h`), NEVER a bare `QSettings`.
- **Tests:** register via `wits_add_qttest()`; the two touched targets (`tst_reportingviewmodel`, `tst_qml_admin`) are already `OFFSCREEN`.
- **The automated suite CANNOT catch the QGuiApplication→QApplication crash** (QtTest runs under `QApplication` when Widgets is linked). A manual export smoke on the real `WITSQuick.exe` is a **release gate** — carried in "Finishing" below, not skippable.

**Build & test commands (Qt tools are NOT on PATH — prefix every invocation).** In PowerShell:

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
# Configure once (short build dir avoids the Windows MAX_PATH overflow on the QML module):
cmake -S qt-app -B C:/b/loams-4b -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
# Build:
cmake --build C:/b/loams-4b
# Run one test target:
ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure
# Full suite:
ctest --test-dir C:/b/loams-4b --output-on-failure
```

---

## File Structure

- **Modify** `qt-app/quick/main.cpp` — `QGuiApplication` → `QApplication` (Task 1).
- **Modify** `qt-app/quick/CMakeLists.txt` — `Qt::Widgets` explicit on `WITSQuick` (Task 1); `Qt::PrintSupport` on `witsquickmodule` (Task 7).
- **Modify** `qt-app/quick/viewmodels/ReportingViewModel.h` / `.cpp` — export statics, state, methods (Tasks 2–7).
- **Modify** `qt-app/quick/qml/components/LComboBox.qml` — `Accessible.name` passthrough (Task 8).
- **Modify** `qt-app/quick/qml/admin/ReportingScreen.qml` — export bar (Task 9).
- **Modify** `qt-app/quick/tests/tst_reportingviewmodel.cpp` — pure + OFFSCREEN export tests (Tasks 2–7).
- **Modify** `qt-app/quick/tests/tst_qml_admin.qml` — export-bar QuickTest + stub props (Tasks 8–9).

No `AdminScreen.qml` change: it already instantiates `ReportingViewModel` and passes it as `vm` (from 4b-i).

---

## Task 1: WITSQuick runs a QApplication (Critical prerequisite)

Without this, the shipped app aborts the moment an export renders a chart or opens the print dialog. There is no unit test (QtTest already runs under `QApplication`); this task is build- and launch-verified, and underwrites the manual smoke gate.

**Files:**
- Modify: `qt-app/quick/main.cpp:1,13`
- Modify: `qt-app/quick/CMakeLists.txt:138-142`

**Interfaces:**
- Consumes: nothing.
- Produces: a running `WITSQuick` whose `qApp` is a `QApplication`, so `QWidget`-based rendering (`QChartView`, `QPrintDialog`) is legal at runtime.

- [ ] **Step 1: Switch the include and the application object**

In `qt-app/quick/main.cpp`, change line 1 from `#include <QGuiApplication>` to `#include <QApplication>`, and line 13 from `QGuiApplication app(argc, argv);` to `QApplication app(argc, argv);`. Leave everything else (QuickStyle, software-backend gate, engine load, fullscreen gate) untouched — `QApplication` *is-a* `QGuiApplication`, so the Quick setup is unchanged.

- [ ] **Step 2: Link Qt::Widgets explicitly on WITSQuick (clarity)**

In `qt-app/quick/CMakeLists.txt`, extend the `WITSQuick` link list so the dependency is explicit rather than only transitive:

```cmake
target_link_libraries(WITSQuick PRIVATE
    witsquickmodule
    witsquickmoduleplugin
    witsquickmoduleplugin_init
    Qt${QT_VERSION_MAJOR}::Widgets
)
```

- [ ] **Step 3: Configure + build WITSQuick**

Run: `cmake -S qt-app -B C:/b/loams-4b -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"` then `cmake --build C:/b/loams-4b --target WITSQuick`
Expected: builds with no new warnings/errors.

- [ ] **Step 4: Launch to confirm no regression**

Run: `C:/b/loams-4b/qt-app/quick/WITSQuick.exe` (close after the shell paints).
Expected: the app opens and the admin shell renders exactly as before — QApplication has not changed any behavior.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/main.cpp qt-app/quick/CMakeLists.txt
git commit -m "fix(quick): run WITSQuick under QApplication for widget-based export

- ReportRenderer draws charts via QChartView and Print uses QPrintDialog,
  both QWidgets; under the previous QGuiApplication they would qFatal at
  export time. QApplication is-a QGuiApplication so Quick setup is unchanged.
- Link Qt::Widgets explicitly on WITSQuick (was only transitive via witscore)."
```

---

## Task 2: `normalizeExportRows` pure static (visits string → number)

The endpoint returns `visits` as a JSON string ("5"); `paintReport`/`writeReportToXlsx` call `obj["visits"].toInt()`, which returns 0 for a string. Store a normalized copy for export.

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h:56` (add declaration near the other statics)
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` (add after `aggregateVisitsByCourse`)
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `reportVisits(const QJsonObject&)` (existing free helper in `ReportRowsModel.h`, already used by the VM).
- Produces: `static QJsonArray ReportingViewModel::normalizeExportRows(const QJsonArray &data);` — a deep copy of `data` with each object's `visits` rewritten to a JSON number; all other fields untouched.

- [ ] **Step 1: Write the failing test**

Add the slot `void normalizeExportRowsCoercesVisitsToNumber();` to the `private slots:` block, and the body:

```cpp
void TestReportingViewModel::normalizeExportRowsCoercesVisitsToNumber()
{
    const QJsonArray in = QJsonDocument::fromJson(R"([
        {"name":"Ana","course":"BSIT","visits":"5","year_level":"3"},
        {"name":"Ben","course":"BSCE","visits":8}
    ])").array();
    const QJsonArray out = ReportingViewModel::normalizeExportRows(in);
    QCOMPARE(out.size(), 2);
    // String "5" becomes numeric 5 (toInt now works for the renderer).
    QVERIFY(out.at(0).toObject().value("visits").isDouble());
    QCOMPARE(out.at(0).toObject().value("visits").toInt(), 5);
    // Already-numeric passes through.
    QCOMPARE(out.at(1).toObject().value("visits").toInt(), 8);
    // Other fields preserved.
    QCOMPARE(out.at(0).toObject().value("course").toString(), QStringLiteral("BSIT"));
    // Empty in -> empty out.
    QCOMPARE(ReportingViewModel::normalizeExportRows(QJsonArray()).size(), 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure` (after `cmake --build C:/b/loams-4b --target tst_reportingviewmodel`)
Expected: compile error — `normalizeExportRows` is not a member.

- [ ] **Step 3: Declare and implement**

In `ReportingViewModel.h`, under the "Pure statics" group (near line 57), add:

```cpp
static QJsonArray normalizeExportRows(const QJsonArray &data);   // visits string -> number
```

In `ReportingViewModel.cpp`, add after `aggregateVisitsByCourse(...)`:

```cpp
QJsonArray ReportingViewModel::normalizeExportRows(const QJsonArray &data)
{
    QJsonArray out;
    for (const QJsonValue &v : data) {
        QJsonObject o = v.toObject();
        o["visits"] = reportVisits(o);   // robust string-or-number -> int
        out.append(o);
    }
    return out;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): normalizeExportRows coerces visits to numeric for export"
```

---

## Task 3: `semesterWindow` pure static (server-matched display range)

For Semester, `buildFilters` sends only `year`+`semester` and the server ranges the data. The export header still needs a printed Period; derive it from the server's Philippine-calendar windows so the printed range equals the data.

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h` (declaration near the statics)
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `DateRange` (from `reportdata.h`, already included transitively).
- Produces: `static DateRange ReportingViewModel::semesterWindow(const QString &semester, int year);` — First = `year-06-01`..`year-10-31`, Second = `year-11-01`..`(year+1)-03-31`, Summer = `year-04-01`..`year-05-31`. Matched on the words "first"/"second"/"summer" (case-insensitive), never a digit. Unknown/invalid → `{ .valid = false }`.

- [ ] **Step 1: Write the failing test**

Add slot `void semesterWindowMatchesServerRanges();` and body:

```cpp
void TestReportingViewModel::semesterWindowMatchesServerRanges()
{
    DateRange first = ReportingViewModel::semesterWindow("First Semester", 2026);
    QVERIFY(first.valid);
    QCOMPARE(first.start, QStringLiteral("2026-06-01"));
    QCOMPARE(first.end,   QStringLiteral("2026-10-31"));

    DateRange second = ReportingViewModel::semesterWindow("Second Semester", 2026);
    QCOMPARE(second.start, QStringLiteral("2026-11-01"));
    QCOMPARE(second.end,   QStringLiteral("2027-03-31"));   // crosses the year

    DateRange summer = ReportingViewModel::semesterWindow("Summer", 2026);
    QCOMPARE(summer.start, QStringLiteral("2026-04-01"));
    QCOMPARE(summer.end,   QStringLiteral("2026-05-31"));

    QVERIFY(!ReportingViewModel::semesterWindow("", 2026).valid);
    QVERIFY(!ReportingViewModel::semesterWindow("First Semester", 0).valid);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel`
Expected: compile error — `semesterWindow` is not a member.

- [ ] **Step 3: Declare and implement**

In `ReportingViewModel.h`, add near the statics:

```cpp
// Display-only Period for a semester, matching get_report_data.php's server windows.
static DateRange semesterWindow(const QString &semester, int year);
```

In `ReportingViewModel.cpp`, add (near `buildFilters`):

```cpp
DateRange ReportingViewModel::semesterWindow(const QString &semester, int year)
{
    DateRange r;
    if (year <= 0)
        return r;   // invalid
    const QString s = semester.toLower();
    if (s.contains(QStringLiteral("first"))) {
        r.start = QStringLiteral("%1-06-01").arg(year);
        r.end   = QStringLiteral("%1-10-31").arg(year);
        r.valid = true;
    } else if (s.contains(QStringLiteral("second"))) {
        r.start = QStringLiteral("%1-11-01").arg(year);
        r.end   = QStringLiteral("%1-03-31").arg(year + 1);
        r.valid = true;
    } else if (s.contains(QStringLiteral("summer"))) {
        r.start = QStringLiteral("%1-04-01").arg(year);
        r.end   = QStringLiteral("%1-05-31").arg(year);
        r.valid = true;
    }
    return r;   // unknown label -> valid stays false
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): semesterWindow derives display Period from server calendar"
```

---

## Task 4: `buildExportFilters` pure static (the keys the renderer reads)

`paintReport`/`writeReportToXlsx` read `department, course, start, end, schoolYear, chartType`. Build exactly those, mirroring `buildFilters`' input shape plus `chartType`.

> **Intentional refinement of the spec (§4.2).** The spec sketched a 6-arg form `(department, course, start, end, schoolYear, chartType)`. This plan uses the 11-arg form (the raw duration inputs + `chartType`) instead, so the date/semester math stays **inside** the pure, unit-tested static rather than being computed in `renderToDevice`. Same output keys; better testability. Not scope drift.

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `ReportController::computeDateRange(...)`, `semesterWindow(...)` (Task 3).
- Produces:
  ```cpp
  static QJsonObject ReportingViewModel::buildExportFilters(
      const QString &department, const QString &course, int durationType,
      const QDate &day, int month, int monthYear,
      const QString &semester, int semYear,
      const QDate &customStart, const QDate &customEnd,
      const QString &chartType);
  ```
  Keys: `department` (empty → `"All Departments"`), `course` (empty → `"All Courses"`), `start`, `end`, `schoolYear`, `chartType`.

- [ ] **Step 1: Write the failing test**

Add slots and bodies:

```cpp
void TestReportingViewModel::buildExportFiltersDayHasRangeAndLabels()
{
    const QJsonObject f = ReportingViewModel::buildExportFilters(
        "", "", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate(), "Bar");
    QCOMPARE(f.value("department").toString(), QStringLiteral("All Departments"));
    QCOMPARE(f.value("course").toString(),     QStringLiteral("All Courses"));
    QCOMPARE(f.value("start").toString(),      QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("end").toString(),        QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("schoolYear").toString(), QStringLiteral("2026"));
    QCOMPARE(f.value("chartType").toString(),  QStringLiteral("Bar"));
}

void TestReportingViewModel::buildExportFiltersSemesterUsesServerWindow()
{
    const QJsonObject f = ReportingViewModel::buildExportFilters(
        "CE", "BSCE", 2, QDate(), 0, 0, "Second Semester", 2026, QDate(), QDate(), "Pie");
    QCOMPARE(f.value("department").toString(), QStringLiteral("CE"));
    QCOMPARE(f.value("course").toString(),     QStringLiteral("BSCE"));
    QCOMPARE(f.value("start").toString(),      QStringLiteral("2026-11-01"));
    QCOMPARE(f.value("end").toString(),        QStringLiteral("2027-03-31"));
    QCOMPARE(f.value("schoolYear").toString(), QStringLiteral("2026"));
    QCOMPARE(f.value("chartType").toString(),  QStringLiteral("Pie"));
}

void TestReportingViewModel::buildExportFiltersMonthSchoolYear()
{
    const QJsonObject f = ReportingViewModel::buildExportFilters(
        "IT", "", 1, QDate(), 2, 2025, "", 0, QDate(), QDate(), "Bar");
    QCOMPARE(f.value("start").toString(),      QStringLiteral("2025-02-01"));
    QCOMPARE(f.value("end").toString(),        QStringLiteral("2025-02-28"));
    QCOMPARE(f.value("schoolYear").toString(), QStringLiteral("2025"));
}
```

(Declare the three slots in `private slots:`.)

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel`
Expected: compile error — `buildExportFilters` is not a member.

- [ ] **Step 3: Declare and implement**

In `ReportingViewModel.h`, add the declaration (signature above). In `ReportingViewModel.cpp`, add:

```cpp
QJsonObject ReportingViewModel::buildExportFilters(
    const QString &department, const QString &course, int durationType,
    const QDate &day, int month, int monthYear,
    const QString &semester, int semYear,
    const QDate &customStart, const QDate &customEnd,
    const QString &chartType)
{
    QJsonObject f;
    f["department"] = department.isEmpty() ? QStringLiteral("All Departments") : department;
    f["course"]     = course.isEmpty()     ? QStringLiteral("All Courses")     : course;
    f["chartType"]  = chartType;

    DateRange r;
    QString schoolYear;
    switch (durationType) {
    case 0:  // Day
        r = ReportController::computeDateRange(0, day, 0, 0, QString(), 0, QDate(), QDate());
        schoolYear = QString::number(day.year());
        break;
    case 1:  // Month
        r = ReportController::computeDateRange(1, QDate(), month, monthYear, QString(), 0, QDate(), QDate());
        schoolYear = QString::number(monthYear);
        break;
    case 2:  // Semester — display range from the server-matched window
        r = semesterWindow(semester, semYear);
        schoolYear = QString::number(semYear);
        break;
    case 3:  // Custom
        r = ReportController::computeDateRange(3, QDate(), 0, 0, QString(), 0, customStart, customEnd);
        schoolYear = QString::number(customStart.year());
        break;
    default:
        break;
    }
    f["start"]      = r.valid ? r.start : QString();
    f["end"]        = r.valid ? r.end   : QString();
    f["schoolYear"] = schoolYear;
    return f;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): buildExportFilters emits renderer keys incl. semester window"
```

---

## Task 5: Export state — palette/chartType, `canExport`, and stored rows

Adds the QML-facing export state and stores the normalized rows in `applyResult` so export reuses them.

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `normalizeExportRows` (Task 2), `m_rows` (existing `ReportRowsModel`, exposes `count()`).
- Produces (new members / API):
  - Props: `palettes` (CONSTANT `{"Default","Blue","Green","Red"}`), `palette` (R/W, default `"Default"`), `chartTypes` (CONSTANT `{"Bar","Pie"}`), `chartType` (R/W, default `"Bar"`), `exporting` (r/o), `canExport` (r/o), `exportStatus` (r/o), `exportError` (r/o).
  - `Q_INVOKABLE void setPalette(const QString&)`, `void setChartType(const QString&)`.
  - `bool canExport() const` = `m_hasResult && !m_loading && !m_exporting && m_errorText.isEmpty() && m_rows.count() > 0`. The `m_errorText.isEmpty()` term ties export to a clean, viewable result (mirrors the screen's `showPreview = hasResult && !isError`), so a **failed refetch** — which leaves `m_hasResult`/`m_rows` populated but shows an error banner — correctly disables export (Design OS #1: export equals what you're viewing).
  - `m_exportRows` is **cleared at fetch start** (`setLoading(true)`), so a stale prior result can never be exported after a failed refetch (spec §4.2: "cleared when a new fetch starts and on error"; the start-clear covers both, since an error never repopulates it).
  - Private setters `setExporting/setExportStatus/setExportError`; member `QJsonArray m_exportRows`.
  - Signals `paletteChanged/chartTypeChanged/exportingChanged/canExportChanged/exportStatusChanged/exportErrorChanged`.

- [ ] **Step 1: Write the failing tests**

Add slots + bodies:

```cpp
void TestReportingViewModel::canExportTruthTable()
{
    ReportingViewModel vm;
    QVERIFY(!vm.canExport());                       // no result yet
    const QJsonArray data = QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array();
    vm.onReportDataReady(data);                     // hasResult + rows>0, not loading/exporting
    QVERIFY(vm.canExport());
    vm.onReportDataReady(QJsonArray());             // hasResult true but zero rows
    QVERIFY(!vm.canExport());
}

void TestReportingViewModel::paletteAndChartTypeSettersEmit()
{
    ReportingViewModel vm;
    QCOMPARE(vm.palette(), QStringLiteral("Default"));
    QCOMPARE(vm.chartType(), QStringLiteral("Bar"));
    QSignalSpy pSpy(&vm, &ReportingViewModel::paletteChanged);
    QSignalSpy cSpy(&vm, &ReportingViewModel::chartTypeChanged);
    vm.setPalette("Blue");
    vm.setChartType("Pie");
    QCOMPARE(vm.palette(), QStringLiteral("Blue"));
    QCOMPARE(vm.chartType(), QStringLiteral("Pie"));
    QCOMPARE(pSpy.count(), 1);
    QCOMPARE(cSpy.count(), 1);
    QCOMPARE(vm.palettes().size(), 4);
    QCOMPARE(vm.chartTypes().size(), 2);
}

void TestReportingViewModel::applyResultStoresNormalizedExportRows()
{
    ReportingViewModel vm;
    const QJsonArray data = QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array();
    vm.onReportDataReady(data);
    QVERIFY(vm.canExport());   // rows stored + hasResult
}

void TestReportingViewModel::failedRefetchDisablesExportAndClearsRows()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array());
    QVERIFY(vm.canExport());                 // a clean result

    // Change filters, then fire a new fetch that fails.
    vm.setDurationType(0);
    vm.setDay(QStringLiteral("2026-08-14")); // filtersComplete
    vm.generateReport();                     // setLoading(true): clears m_exportRows, loading
    QVERIFY(!vm.canExport());                // gated while loading
    vm.onReportError(QStringLiteral("Server error"), false);   // loading=false, errorText set
    QVERIFY(!vm.canExport());                // errorText non-empty AND rows cleared

    // Export must refuse — no stale prior result.
    QTemporaryDir dir;
    vm.exportPdf(QUrl::fromLocalFile(dir.filePath("stale.pdf")));
    QVERIFY(vm.exportError().contains(QStringLiteral("No data")));
}
```

(Add `#include <QSignalSpy>`, `#include <QTemporaryDir>`, `#include <QUrl>` at the top if not present. Declare the four slots, incl. `failedRefetchDisablesExportAndClearsRows`.)

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel`
Expected: compile error — `canExport`, `setPalette`, etc. are not members.

- [ ] **Step 3: Add the properties, getters, setters, and state**

In `ReportingViewModel.h`, add to the `Q_PROPERTY` block:

```cpp
Q_PROPERTY(QStringList palettes READ palettes CONSTANT)
Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY paletteChanged)
Q_PROPERTY(QStringList chartTypes READ chartTypes CONSTANT)
Q_PROPERTY(QString chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
Q_PROPERTY(bool canExport READ canExport NOTIFY canExportChanged)
Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStatusChanged)
Q_PROPERTY(QString exportError READ exportError NOTIFY exportErrorChanged)
```

Add getters (public):

```cpp
QStringList palettes() const { return { QStringLiteral("Default"), QStringLiteral("Blue"),
                                        QStringLiteral("Green"), QStringLiteral("Red") }; }
QString palette() const { return m_palette; }
QStringList chartTypes() const { return { QStringLiteral("Bar"), QStringLiteral("Pie") }; }
QString chartType() const { return m_chartType; }
bool exporting() const { return m_exporting; }
bool canExport() const;
QString exportStatus() const { return m_exportStatus; }
QString exportError() const { return m_exportError; }

Q_INVOKABLE void setPalette(const QString &p);
Q_INVOKABLE void setChartType(const QString &c);
```

Add signals:

```cpp
void paletteChanged();
void chartTypeChanged();
void exportingChanged();
void canExportChanged();
void exportStatusChanged();
void exportErrorChanged();
```

Add private helpers + members:

```cpp
void setExporting(bool v);
void setExportStatus(const QString &s);
void setExportError(const QString &e);

QString m_palette = QStringLiteral("Default");
QString m_chartType = QStringLiteral("Bar");
bool m_exporting = false;
QString m_exportStatus;
QString m_exportError;
QJsonArray m_exportRows;
```

In `ReportingViewModel.cpp`, implement:

```cpp
bool ReportingViewModel::canExport() const
{
    return m_hasResult && !m_loading && !m_exporting
           && m_errorText.isEmpty() && m_rows.count() > 0;
}
void ReportingViewModel::setPalette(const QString &p)
{
    if (m_palette == p) return;
    m_palette = p; emit paletteChanged();
}
void ReportingViewModel::setChartType(const QString &c)
{
    if (m_chartType == c) return;
    m_chartType = c; emit chartTypeChanged();
}
void ReportingViewModel::setExporting(bool v)
{
    if (m_exporting == v) return;
    m_exporting = v; emit exportingChanged(); emit canExportChanged();
}
void ReportingViewModel::setExportStatus(const QString &s)
{
    m_exportStatus = s; emit exportStatusChanged();
}
void ReportingViewModel::setExportError(const QString &e)
{
    m_exportError = e; emit exportErrorChanged();
}
```

Then wire `canExportChanged` into the existing state transitions:

- In `setLoading(bool v)`: after `emit loadingChanged();` add `emit canExportChanged();`, and **clear the export rows when a fetch starts** so a stale result can't survive a failed refetch:
  ```cpp
  void ReportingViewModel::setLoading(bool v)
  {
      if (m_loading == v) return;
      m_loading = v;
      if (v) m_exportRows = QJsonArray();   // new fetch starting -> drop the previous export rows
      emit loadingChanged();
      emit canGenerateChanged();   // loading gates canGenerate
      emit canExportChanged();     // loading (and cleared rows) gate canExport
  }
  ```
- In `setError(const QString &e)`: after `emit errorTextChanged();` add `emit canExportChanged();` (an error banner disables export via the `m_errorText.isEmpty()` term). Change its body to:
  ```cpp
  void ReportingViewModel::setError(const QString &e)
  {
      if (m_errorText == e) return;
      m_errorText = e;
      emit errorTextChanged();
      emit canExportChanged();
  }
  ```
- In `applyResult(...)` store the normalized rows and signal export availability — change the body to:

```cpp
void ReportingViewModel::applyResult(const QJsonArray &data)
{
    m_rows.setRows(data);
    m_exportRows = normalizeExportRows(data);   // reused by export (numeric visits, all 8 cols)
    m_courseBars.setBars(aggregateVisitsByCourse(data));
    const Tiles t = deriveTiles(data);
    m_totalVisits = t.totalVisits;
    m_studentsShown = t.studentsShown;
    m_topCourse = t.topCourse;
    m_hasResult = true;
    m_validationError = false;
    setError(QString());
    emit resultChanged();
    emit canExportChanged();
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): export state (palette/chartType/canExport) + stored export rows"
```

---

## Task 6: `headerInfo` + `renderToDevice` + `exportPdf` + `exportExcel`

The file-export path: render the stored rows to a PDF (via `QPdfWriter`, Qt::Gui) and an Excel workbook (via `QXlsx`). No PrintSupport needed yet.

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `ReportRenderer::paintReport`, `ReportRenderer::writeReportToXlsx`, `ReportController::getPalette`, `buildExportFilters` (Task 4), `AppSettings` (`appsettings.h`), `QPdfWriter`, `QXlsx::Document`.
- Produces:
  - `Q_INVOKABLE void exportPdf(const QUrl &fileUrl);`
  - `Q_INVOKABLE void exportExcel(const QUrl &fileUrl);`
  - private `ReportHeaderInfo headerInfo() const;`
  - private `bool renderToDevice(QPagedPaintDevice *dev, int resolution);`

- [ ] **Step 1: Write the failing OFFSCREEN tests**

Add includes at the top of the test file: `#include <QUrl>`, `#include <QTemporaryDir>`, `#include <QFileInfo>`, `#include "xlsxdocument.h"`. Add slots + bodies:

```cpp
void TestReportingViewModel::exportPdfWritesFile()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","gender":"F","status":"Regular",
             "course":"BSIT","department":"IT","year_level":"3","visits":"5"}])").array());
    QTemporaryDir dir;
    const QString path = dir.filePath("report.pdf");
    vm.exportPdf(QUrl::fromLocalFile(path));
    QTRY_VERIFY(!vm.exporting());                 // queued render completes
    QVERIFY(QFileInfo::exists(path));
    QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.read(4), QByteArray("%PDF"));
    QVERIFY(!vm.exportStatus().isEmpty());
    QVERIFY(vm.exportError().isEmpty());
}

void TestReportingViewModel::exportExcelWritesReadableCell()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","gender":"F","status":"Regular",
             "course":"BSIT","department":"IT","year_level":"3","visits":"5"}])").array());
    QTemporaryDir dir;
    const QString path = dir.filePath("report.xlsx");
    vm.exportExcel(QUrl::fromLocalFile(path));
    QTRY_VERIFY(!vm.exporting());
    QVERIFY(QFileInfo::exists(path));
    QXlsx::Document doc(path);
    QVERIFY(doc.load());
    // The student name lands somewhere in the sheet's table body.
    bool foundName = false;
    for (int r = 1; r <= 40 && !foundName; ++r)
        for (int c = 1; c <= 8; ++c)
            if (doc.read(r, c).toString() == QStringLiteral("Ana")) { foundName = true; break; }
    QVERIFY(foundName);
}

void TestReportingViewModel::exportPdfEmptyRowsShowsNoDataError()
{
    ReportingViewModel vm;   // never generated -> m_exportRows empty
    QTemporaryDir dir;
    vm.exportPdf(QUrl::fromLocalFile(dir.filePath("x.pdf")));
    QVERIFY(vm.exportError().contains(QStringLiteral("No data")));
    QVERIFY(!vm.exporting());
}

void TestReportingViewModel::exportPdfInvalidUrlShowsError()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array());
    vm.exportPdf(QUrl("https://example.com/x.pdf"));   // non-file URL -> empty local path
    QVERIFY(!vm.exportError().isEmpty());
    QVERIFY(!vm.exporting());
}

void TestReportingViewModel::exportWhileExportingIsNoop()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array());
    QTemporaryDir dir;
    vm.exportPdf(QUrl::fromLocalFile(dir.filePath("a.pdf")));   // sets exporting=true (queued)
    QVERIFY(vm.exporting());
    const QString before = vm.exportError();
    vm.exportExcel(QUrl::fromLocalFile(dir.filePath("b.xlsx")));   // must no-op while exporting
    QCOMPARE(vm.exportError(), before);
    QTRY_VERIFY(!vm.exporting());                               // first export drains
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel`
Expected: compile error — `exportPdf`/`exportExcel` are not members.

- [ ] **Step 3: Declare + implement**

In `ReportingViewModel.h`, add the public invokables (near `generateReport()`):

```cpp
Q_INVOKABLE void exportPdf(const QUrl &fileUrl);
Q_INVOKABLE void exportExcel(const QUrl &fileUrl);
```

and the private helpers:

```cpp
ReportHeaderInfo headerInfo() const;
bool renderToDevice(QPagedPaintDevice *dev, int resolution);
```

Add the include of `<QUrl>` to the header (for the parameter type).

In `ReportingViewModel.cpp`, add includes at the top:

```cpp
#include <QFileInfo>
#include <QPageSize>
#include <QPdfWriter>
#include <QUrl>
#include "appsettings.h"
#include "reportrenderer.h"
#include "xlsxdocument.h"
```

Implement:

```cpp
ReportHeaderInfo ReportingViewModel::headerInfo() const
{
    AppSettings s;   // mandated scope — matches what Phase 4c Settings wrote
    ReportHeaderInfo info;
    info.schoolName = s.value(QStringLiteral("school/name"), QStringLiteral("Your School Name")).toString();
    info.address    = s.value(QStringLiteral("school/address"), QStringLiteral("Your Address")).toString();
    info.logoPath   = s.value(QStringLiteral("school/logoPath"), QString()).toString();
    info.librarian  = s.value(QStringLiteral("admin/name"), QString()).toString();
    info.position   = s.value(QStringLiteral("admin/position"), QString()).toString();
    info.openHour   = s.value(QStringLiteral("library/openHour"), 7).toInt();
    info.closeHour  = s.value(QStringLiteral("library/closeHour"), 21).toInt();
    return info;
}

bool ReportingViewModel::renderToDevice(QPagedPaintDevice *dev, int resolution)
{
    const QJsonObject filters = buildExportFilters(
        m_department, m_course, m_durationType,
        parseDate(m_day), m_month, m_monthYear,
        m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd),
        m_chartType);
    const ReportPalette pal = ReportController::getPalette(m_palette);
    return ReportRenderer::paintReport(dev, resolution, m_exportRows, filters, pal, headerInfo());
}

void ReportingViewModel::exportPdf(const QUrl &fileUrl)
{
    if (m_exporting) return;
    if (m_exportRows.isEmpty()) {
        setExportError(tr("No data to export. Adjust the filters and generate a report with results."));
        return;
    }
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        setExportError(tr("Couldn't export — choose a local file location."));
        return;
    }
    setExportError(QString());
    setExporting(true);
    // Defer one turn so the busy overlay paints before the blocking render.
    QMetaObject::invokeMethod(this, [this, path]() {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
        if (ok)
            setExportStatus(tr("Saved %1").arg(QFileInfo(path).fileName()));
        else
            setExportError(tr("Couldn't write %1 — choose a different location.").arg(QFileInfo(path).fileName()));
        setExporting(false);
    }, Qt::QueuedConnection);
}

void ReportingViewModel::exportExcel(const QUrl &fileUrl)
{
    if (m_exporting) return;
    if (m_exportRows.isEmpty()) {
        setExportError(tr("No data to export. Adjust the filters and generate a report with results."));
        return;
    }
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        setExportError(tr("Couldn't export — choose a local file location."));
        return;
    }
    setExportError(QString());
    setExporting(true);
    QMetaObject::invokeMethod(this, [this, path]() {
        QXlsx::Document doc;
        const QJsonObject filters = buildExportFilters(
            m_department, m_course, m_durationType,
            parseDate(m_day), m_month, m_monthYear,
            m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd),
            m_chartType);
        const bool ok = ReportRenderer::writeReportToXlsx(doc, m_exportRows, filters, headerInfo())
                        && doc.saveAs(path);
        if (ok)
            setExportStatus(tr("Saved %1").arg(QFileInfo(path).fileName()));
        else
            setExportError(tr("Couldn't write %1 — choose a different location.").arg(QFileInfo(path).fileName()));
        setExporting(false);
    }, Qt::QueuedConnection);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (all export-to-temp-file tests green under the offscreen platform).

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): PDF + Excel export via ReportRenderer over stored rows"
```

---

## Task 7: `printReport` + link Qt::PrintSupport

Print reuses `renderToDevice`. `QPrintDialog` is the interaction beat, so after Accept the render runs synchronously (the dialog already gave the user a moment; `exporting` is true only for that render).

**Files:**
- Modify: `qt-app/quick/CMakeLists.txt:103-110` (add `Qt::PrintSupport` to `witsquickmodule`)
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `renderToDevice` (Task 6), `QPrinter`, `QPrintDialog`.
- Produces: `Q_INVOKABLE void printReport();` — validates rows, shows the dialog, renders on Accept.

- [ ] **Step 1: Write the failing test (the only headless-safe path: the no-data guard)**

```cpp
void TestReportingViewModel::printReportEmptyRowsShowsNoDataError()
{
    ReportingViewModel vm;   // no rows -> must not open a dialog
    vm.printReport();
    QVERIFY(vm.exportError().contains(QStringLiteral("No data")));
    QVERIFY(!vm.exporting());
}
```

(A successful print opens a modal dialog and cannot run headlessly; its render path is the same `renderToDevice` already covered by `exportPdfWritesFile`. This is documented, not a coverage gap.)

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel`
Expected: compile error — `printReport` is not a member (and, once declared, a link error for `QPrintDialog` until Step 3's CMake change).

- [ ] **Step 3: Link PrintSupport, declare + implement**

In `qt-app/quick/CMakeLists.txt`, add PrintSupport to the module link list:

```cmake
target_link_libraries(witsquickmodule PUBLIC
    witscore
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::QuickDialogs2
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::PrintSupport
)
```

In `ReportingViewModel.h`, add `Q_INVOKABLE void printReport();` (near the other export invokables).

In `ReportingViewModel.cpp`, add includes `#include <QPrinter>` and `#include <QPrintDialog>`, then:

```cpp
void ReportingViewModel::printReport()
{
    if (m_exporting) return;
    if (m_exportRows.isEmpty()) {
        setExportError(tr("No data to export. Adjust the filters and generate a report with results."));
        return;
    }
    // Opening the dialog is NOT "exporting" — the normal UI stays live.
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer);
    if (dlg.exec() != QDialog::Accepted)
        return;   // cancelled -> no-op, no error, no busy state
    setExportError(QString());
    setExporting(true);   // now rendering begins
    const bool ok = renderToDevice(&printer, printer.resolution());
    if (ok)
        setExportStatus(tr("Sent to printer"));
    else
        setExportError(tr("Couldn't print the report."));
    setExporting(false);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4b -R tst_reportingviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/CMakeLists.txt qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): Print export via QPrintDialog; link Qt::PrintSupport"
```

---

## Task 8: `LComboBox` accessible-name passthrough

The palette/chart-type combos need an accessible name. `LComboBox`'s root is a bare `Item`; add a passthrough.

**Files:**
- Modify: `qt-app/quick/qml/components/LComboBox.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (a small case in the existing components/admin file; asserted fully in Task 9's screen test — here we prove the passthrough on the component alone).

**Interfaces:**
- Produces: `LComboBox` gains `property string accessibleName: ""`, applied to the inner `ComboBox` via `Accessible.name`.

- [ ] **Step 1: Write the failing test**

In `tst_qml_admin.qml`, inside the `ReportingScreen` `TestCase` (added fully in Task 9), the palette combo's `Accessible.name` is asserted. For this task, add a focused check by giving the export palette combo `accessibleName: qsTr("Report palette")` (Task 9) and asserting:

```qml
function test_paletteComboHasAccessibleName() {
    var combo = findChild(reporting, "paletteCombo");
    verify(combo !== null);
    compare(combo.accessibleName, "Report palette");
}
```

(If Task 9 is not yet in place when running this task standalone, temporarily assert against any `LComboBox` instance with `accessibleName` set; the durable assertion lives in Task 9.)

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build C:/b/loams-4b --target tst_qml_admin`
Expected: FAIL — `accessibleName` is undefined on `LComboBox` (property does not exist).

- [ ] **Step 3: Add the passthrough**

In `qt-app/quick/qml/components/LComboBox.qml`, add a root property (near `placeholder`):

```qml
property string accessibleName: ""
```

and on the inner `ComboBox` (inside the `ComboBox { id: combo ... }` block), add:

```qml
Accessible.role: Accessible.ComboBox
Accessible.name: root.accessibleName
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build C:/b/loams-4b --target tst_qml_admin` then `ctest --test-dir C:/b/loams-4b -R tst_qml_admin --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add qt-app/quick/qml/components/LComboBox.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(components): LComboBox accessibleName passthrough"
```

---

## Task 9: ReportingScreen export bar + QuickTest

The UI: palette + chart-type combos, PDF/Excel/Print buttons, two `FileDialog`s, a busy overlay, an empty-state line, and success/error feedback — all gated per the Design OS contract.

**Files:**
- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (extend `reportingStub` + the `ReportingScreen` `TestCase`)

**Interfaces:**
- Consumes (from `vm`): `palettes`, `palette`, `chartTypes`, `chartType`, `canExport`, `exporting`, `exportStatus`, `exportError`, `hasResult`, `rows.count`, `setPalette`, `setChartType`, `exportPdf(url)`, `exportExcel(url)`, `printReport()`.
- Produces: objectNames `paletteCombo`, `chartTypeCombo`, `exportPdfButton`, `exportExcelButton`, `printButton`, `exportBusyOverlay`, `exportEmptyState`, `exportFeedback`.

- [ ] **Step 1: Write the failing QuickTests + extend the stub**

Extend `reportingStub` (in `tst_qml_admin.qml`) with export props/functions:

```qml
property var palettes: ["Default", "Blue", "Green", "Red"]
property string palette: "Default"
property var chartTypes: ["Bar", "Pie"]
property string chartType: "Bar"
property bool canExport: true
property bool exporting: false
property string exportStatus: ""
property string exportError: ""
property int exportPdfCount: 0
property int exportExcelCount: 0
property int printCount: 0
function setPalette(p) { palette = p }
function setChartType(c) { chartType = c }
function exportPdf(u) { exportPdfCount++ }
function exportExcel(u) { exportExcelCount++ }
function printReport() { printCount++ }
```

The screen reads emptiness via `screen.vm.rows.count`, and the stub's `rows` is `reportRowsStub` — a real `ListModel` (`tst_qml_admin.qml:2505`) whose `count` is **read-only**. Do NOT add or assign a `count` property to it and do NOT introduce a `rowsCount` prop the screen never binds. Drive the empty case with `reportRowsStub.clear()` and restore it by re-appending the two original `ListElement`s (the preview/table tests need `rows` to stay a real model, so it can't be swapped for a plain `QtObject`).

Add these test functions to the `ReportingScreen` `TestCase` (raise the fixture height in Step 3 so the bar is laid out and hit-testable). First add a `cleanup()` that always restores the shared `reportRowsStub` to its two original rows, so a failing assertion in the empty-state test can't leave the model empty for later tests in the case:

```qml
function cleanup() {
    if (reportRowsStub.count === 0) {
        reportRowsStub.append({ name: "Maria Santos", course: "BSCE", year: "3", visits: 42 });
        reportRowsStub.append({ name: "Jose Cruz", course: "BSIT", year: "1", visits: 7 });
    }
}

function test_exportButtonsDisabledWhenCannotExport() {
    reportingStub.canExport = false;
    var pdf = findChild(reporting, "exportPdfButton");
    var xls = findChild(reporting, "exportExcelButton");
    var prn = findChild(reporting, "printButton");
    verify(pdf && xls && prn);
    verify(!pdf.enabled); verify(!xls.enabled); verify(!prn.enabled);
}

function test_exportButtonsFireVmMethods() {
    reportingStub.canExport = true;
    var prn = findChild(reporting, "printButton");
    var before = reportingStub.printCount;
    mouseClick(prn);
    compare(reportingStub.printCount, before + 1);
}

function test_busyOverlayVisibleWhileExporting() {
    var overlay = findChild(reporting, "exportBusyOverlay");
    verify(overlay);
    reportingStub.exporting = false; verify(!overlay.visible);
    reportingStub.exporting = true;  verify(overlay.visible);
    reportingStub.exporting = false;
}

function test_emptyStateShownWhenResultHasNoRows() {
    reportingStub.hasResult = true;
    reportRowsStub.clear();                 // count -> 0 (read-only: mutate the model, not the prop)
    var empty = findChild(reporting, "exportEmptyState");
    verify(empty);
    verify(empty.visible);
    // Restoration happens in cleanup() below, so a failed assert can't leave
    // the model empty for later (alphabetically-ordered) tests.
}

function test_paletteComboHasAccessibleNameAndWrites() {
    var combo = findChild(reporting, "paletteCombo");
    verify(combo);
    compare(combo.accessibleName, "Report palette");
    combo.selectValue("Blue");
    compare(reportingStub.palette, "Blue");
    reportingStub.palette = "Default";
}

function test_exportErrorPersistsAsFeedback() {
    reportingStub.exportError = "Couldn't write report.pdf";
    var fb = findChild(reporting, "exportFeedback");
    verify(fb);
    verify(fb.visible);
    verify(fb.text.indexOf("Couldn't write") >= 0);
    reportingStub.exportError = "";
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build C:/b/loams-4b --target tst_qml_admin`
Expected: FAIL — the export controls (objectNames) don't exist yet.

- [ ] **Step 3: Build the export bar**

In `ReportingScreen.qml`, add a `readonly property bool canExport: vm ? vm.canExport : false` near the other readonly props, and — inside the preview `ColumnLayout` visibility model — insert the export bar as a sibling **after** the error/retry block and before (or after) the preview, wrapped so it appears only with a real, non-empty result. Insert this block (uses only `Theme` tokens):

```qml
import QtQuick.Dialogs   // add to the top imports alongside QtQuick / Layouts / LOAMS

// --- Export bar (visible only when there is a non-empty result) ---
Rectangle {
    Layout.fillWidth: true
    visible: screen.vm ? (screen.vm.hasResult && screen.vm.rows && screen.vm.rows.count > 0) : false
    color: Theme.card; radius: Theme.radius.card
    border.width: 2; border.color: Theme.border
    implicitHeight: exportRow.implicitHeight + Theme.spacing.xl * 2

    RowLayout {
        id: exportRow
        anchors.fill: parent
        anchors.margins: Theme.spacing.xl
        spacing: Theme.spacing.md

        LComboBox {
            id: paletteCombo
            objectName: "paletteCombo"
            accessibleName: qsTr("Report palette")
            Layout.preferredWidth: 150
            model: screen.vm ? screen.vm.palettes : []
            placeholder: qsTr("Palette")
            currentValue: screen.vm ? screen.vm.palette : "Default"
            onSelected: function(v) { if (screen.vm) screen.vm.setPalette(v); }
        }
        LComboBox {
            id: chartTypeCombo
            objectName: "chartTypeCombo"
            accessibleName: qsTr("Chart type")
            Layout.preferredWidth: 150
            model: screen.vm ? screen.vm.chartTypes : []
            placeholder: qsTr("Chart")
            currentValue: screen.vm ? screen.vm.chartType : "Bar"
            onSelected: function(v) { if (screen.vm) screen.vm.setChartType(v); }
        }

        Item { Layout.fillWidth: true }   // spacer

        // Design OS §4.5: disabled controls expose WHY via Accessible.description.
        LButton {
            objectName: "exportPdfButton"
            text: qsTr("Export PDF")
            accessibleName: qsTr("Export PDF")
            enabled: screen.canExport
            Accessible.description: screen.canExport ? "" : qsTr("Generate a report with results to enable export")
            onClicked: exportPdfDialog.open()
        }
        LButton {
            objectName: "exportExcelButton"
            text: qsTr("Export Excel")
            accessibleName: qsTr("Export Excel")
            variant: "Outline"
            enabled: screen.canExport
            Accessible.description: screen.canExport ? "" : qsTr("Generate a report with results to enable export")
            onClicked: exportExcelDialog.open()
        }
        LButton {
            objectName: "printButton"
            text: qsTr("Print")
            accessibleName: qsTr("Print report")
            variant: "Outline"
            enabled: screen.canExport
            Accessible.description: screen.canExport ? "" : qsTr("Generate a report with results to enable export")
            onClicked: if (screen.vm) screen.vm.printReport()
        }
    }
}

// Empty-state affordance (Design OS #4): a report was generated but has no rows.
Text {
    objectName: "exportEmptyState"
    Layout.fillWidth: true
    visible: screen.vm ? (screen.vm.hasResult && (!screen.vm.rows || screen.vm.rows.count === 0)) : false
    text: qsTr("No data to export. Adjust the filters and generate a report with results.")
    textFormat: Text.PlainText
    color: Theme.mutedTextCaption
    font.family: Theme.typography.sans
    font.pixelSize: Theme.typography.body
    wrapMode: Text.WordWrap
}

// Export feedback: transient success / persistent error (Design OS #5).
Text {
    objectName: "exportFeedback"
    Layout.fillWidth: true
    visible: text.length > 0
    text: screen.vm ? (screen.vm.exportError.length > 0 ? screen.vm.exportError : screen.vm.exportStatus) : ""
    textFormat: Text.PlainText
    color: (screen.vm && screen.vm.exportError.length > 0) ? Theme.error : Theme.text
    font.family: Theme.typography.sans
    font.pixelSize: Theme.typography.body
    wrapMode: Text.WordWrap
}

FileDialog {
    id: exportPdfDialog
    fileMode: FileDialog.SaveFile
    nameFilters: [qsTr("PDF document (*.pdf)")]
    defaultSuffix: "pdf"
    onAccepted: if (screen.vm) screen.vm.exportPdf(selectedFile)
}
FileDialog {
    id: exportExcelDialog
    fileMode: FileDialog.SaveFile
    nameFilters: [qsTr("Excel workbook (*.xlsx)")]
    defaultSuffix: "xlsx"
    onAccepted: if (screen.vm) screen.vm.exportExcel(selectedFile)
}
```

Add the busy overlay as the **last** child of the root `Rectangle` (so it sits above everything and catches input):

```qml
// Export busy overlay — blocks input; announces progress (Design OS #7/a11y).
Rectangle {
    objectName: "exportBusyOverlay"
    anchors.fill: parent
    visible: screen.vm ? screen.vm.exporting : false
    color: Qt.alpha(Theme.appBackground, 0.7)
    z: 100
    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Exporting report")
    MouseArea { anchors.fill: parent }   // swallow clicks
    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.spacing.md
        BusyIndicator { running: parent.visible; Layout.alignment: Qt.AlignHCenter }
        Text {
            text: qsTr("Exporting…")
            textFormat: Text.PlainText
            color: Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }
    }
}
```

(The `BusyIndicator` needs `import QtQuick.Controls`; add it to the top imports.)

- [ ] **Step 4: Raise the QuickTest fixture height + run**

In `tst_qml_admin.qml`, bump the two reporting fixtures so the export bar lays out within the fixture band (the stat tiles + chart + table + export bar are taller than 800):

```qml
ReportingScreen { id: reporting; x: 0; y: 7300; width: 1100; height: 1000; vm: reportingStub }
ReportingScreen { id: vmlessReporting; x: 2000; y: 7300; width: 1100; height: 1000 }
```

Also update the band-map comment near the top of the file (`tst_qml_admin.qml:13-15`): the reporting band is now `7300..8300` (was `7300..8100`) — reporting is the last band, so nothing sits below it, but keep the comment accurate.

Run: `cmake --build C:/b/loams-4b --target tst_qml_admin` then `ctest --test-dir C:/b/loams-4b -R tst_qml_admin --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Full suite + commit**

Run: `cmake --build C:/b/loams-4b` then `ctest --test-dir C:/b/loams-4b --output-on-failure`
Expected: entire suite green.

```powershell
git add qt-app/quick/qml/admin/ReportingScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(reporting): export bar (palette/chart, PDF/Excel/Print, busy overlay, feedback)"
```

---

## Finishing (after all tasks)

1. **Full build + suite green:** `cmake --build C:/b/loams-4b` then `ctest --test-dir C:/b/loams-4b --output-on-failure`.
2. **MANDATORY manual GUI smoke on the real `WITSQuick.exe` — release gate (§7.4).** The automated suite runs under `QApplication` and cannot catch the app-object crash, so on the running app: generate a report, then **Export PDF** (save + open the file), **Export Excel** (save + open), and **Print** (open the dialog, print/preview) — each must complete without the app aborting. Also confirm the empty-state line appears for a zero-row range and the export bar hides when there is no result.
3. **`/claude-review`** (whole-branch) → fix Critical/Important → re-review to APPROVE.
4. **Project `create-pr`** — 3-agent gate (dry-checker, security-reviewer, general-code-reviewer; **no** api-checker). Stop at PR-open; the owner merges.

---

## Self-Review

**Spec coverage:** §2.1 export bar (T9), palette picker (T5/T9), chart-type Bar/Pie (T5/T9), PDF (T6), Excel (T6), Print (T7), busy overlay (T9), success/failure feedback (T9), `canExport`/`exporting` (T5), pure statics `buildExportFilters`/`normalizeExportRows`/`semesterWindow` (T2–T4), `renderToDevice`/`headerInfo` (T6), CMake PrintSupport (T7), QApplication prerequisite (T1), LComboBox a11y (T8), OFFSCREEN export tests (T6), QuickTests (T9), manual smoke gate (Finishing). §2.3 Design OS: Generate=commitment via reused `m_exportRows` (T5/T6); authoritative validation incl. `toLocalFile()` empty-check (T6); print-state semantics (T7); empty-state message (T9); feedback hierarchy (T9); context preservation — no export path mutates filters/preview/palette/chart (T6/T7); accessibility (T8/T9). All covered.

**Placeholder scan:** every code step carries full code; no TBD/TODO. The one soft spot (Task 8's standalone assertion) is explicitly bridged to Task 9's durable test.

**Type consistency:** `normalizeExportRows`→`m_exportRows` (QJsonArray) fed to `renderToDevice`→`paintReport`; `buildExportFilters` signature identical across T4/T6/T7 call sites; `semesterWindow` returns `DateRange` used by `buildExportFilters`; `canExport` reads `m_rows.count()` (existing `ReportRowsModel::count()`); export invokables take `const QUrl&` matching the `FileDialog.selectedFile` handoff (proven by `DatabaseViewModel::exportCsv`). Consistent.

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-08-18-loams2-phase4b-ii-reporting-export.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — a fresh subagent per task with two-stage review between tasks.

**2. Inline Execution** — batch execution in this session with checkpoints.

**Which approach?**
