# Phase 4b-iii-b — Reporting Export Analytics — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the analytics that 4b-iii-a put on screen into the PDF and Excel exports — the same `ReportAnalytics` struct drives a KPI summary + Top-10 rankings in both documents, the chart is demoted below the rankings, and the per-student roster becomes an opt-in section governed by a new "Include detailed roster in export" checkbox (default OFF).

**Architecture:** The pure `ReportAnalytics::compute` aggregator (shipped in 4b-iii-a) is already computed once per result inside `ReportingViewModel::applyResult`. This slice threads that computed struct — plus a `bool includeRoster` flag — into `ReportRenderer::paintReport` and `ReportRenderer::writeReportToXlsx`, so **the renderer never re-aggregates** (spec §3, "no re-derivation"). The renderer reads `analytics.kpis.*` / `analytics.topStudents|topCourses|topDepartments` for the summary and rankings, and reads `rows` only for the optional roster.

**Tech Stack:** Qt 6.11 / C++17, QtTest (offscreen) under ctest, QML (URI `LOAMS`) + Qt Quick Test, QXlsx (vendored), CMake + Ninja + MinGW.

## Global Constraints

- **Spec:** `docs/superpowers/specs/2026-08-18-loams2-phase4b-iii-reporting-analytics-design.md` (owner-approved). This slice is **4b-iii-b ONLY** — the export half. It **depends on 4b-iii-a** (merged, PR #42, `9ff36f9`): `reportanalytics.{h,cpp}`, the VM's `ReportAnalytics::compute(m_exportRows)` call in `applyResult`, and the `RankingModel` all already exist on master.
- **No re-derivation (spec §3, load-bearing):** every KPI/ranking value in an export comes from the passed `const ReportAnalytics &analytics`. The renderer performs **zero** aggregation. `rows` is retained ONLY for the optional detailed roster.
- **Input contract:** `analytics` is always built from `m_exportRows`, which `applyResult` already normalized via `normalizeExportRows` (numeric `visits`). The renderer does not re-parse.
- **Roster independence (spec §9):** the export checkbox (`includeRosterInExport`, default **false**) is **independent** of the on-screen "View full roster" toggle (`screen.showRoster`). Neither follows the other.
- **PDF order (spec §8):** context (header + filters) → KPI summary → rankings (Students, Courses, Departments) → visualization (chart, DEMOTED) → optional roster → prepared-by.
- **Excel (spec §8):** a **"Summary"** sheet (KPIs + 3 rankings) always; a **"Detailed Roster"** sheet only when `includeRoster`. The Charts sheet is explicitly out of scope (QXlsx data-only).
- **MVVM:** `ReportingViewModel` is the ONLY QML-facing C++. The new checkbox writes back through a `Q_INVOKABLE setIncludeRosterInExport(bool)`.
- **Theme:** all QML visual tokens via `Theme.qml`; ZERO raw hex outside `Theme.qml`; server/name-derived text renders `Text.PlainText`.
- **No real student PII** in any test/fixture — synthetic names/IDs only.
- **Release gate (spec §7.4 / §10, carried from 4b-ii):** WITSQuick runs `QApplication` (the `QGuiApplication`→`QApplication` fix landed in 4b-ii). OFFSCREEN QtTests run under `QApplication` when `Qt::Widgets` is linked and therefore **cannot** catch a real export-time crash. A manual `WITSQuick.exe` export smoke is a mandatory release gate — see Task 6.
- **Build (PowerShell; Qt tools NOT on PATH; external short build dir avoids the Windows MAX_PATH overflow on the QML module):**
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B C:/b/loams-4biiib -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build C:/b/loams-4biiib
  ctest --test-dir C:/b/loams-4biiib --output-on-failure
  ```
  Baseline at branch start: **43/43 green** (verify at build time; do not hard-code). Ignore the "LF will be replaced by CRLF" and the pre-existing QXlsx "GuiPrivate target" warnings. Close any running `WITSQuick.exe` before rebuilding — it locks the exe against relink.

## File Structure

- **Modify** `qt-app/quick/viewmodels/ReportingViewModel.h` / `.cpp` — add the `includeRosterInExport` `Q_PROPERTY` + setter + signal + member (Task 1); pass `ReportAnalytics::compute(m_exportRows)` and `m_includeRosterInExport` into the two renderer calls (Task 2).
- **Modify** `qt-app/quick/tests/tst_reportingviewmodel.cpp` — property default + toggle tests (Task 1).
- **Modify** `qt-app/core/reportrenderer.h` — new `paintReport` / `writeReportToXlsx` signatures gaining `const ReportAnalytics &analytics, bool includeRoster`; add `#include "reportanalytics.h"` (Task 2).
- **Modify** `qt-app/core/reportrenderer.cpp` — gate the roster on `includeRoster` (Task 2); multi-sheet Excel (Task 3); PDF structural reorder with KPI + ranking layouts (Task 4).
- **Modify** `qt-app/tests/tst_reportrenderer.cpp` — update both call sites to the new arity; add roster-gating + Summary-content assertions (Tasks 2–4).
- **Modify** `qt-app/tests/CMakeLists.txt` — add `reportanalytics.cpp/.h` to the `tst_reportrenderer` `qt_add_executable` SOURCES (it compiles `reportrenderer.cpp` directly and does **not** link `witscore`, so `ReportAnalytics::compute` would be an undefined reference otherwise) (Task 2).
- **Modify** `qt-app/quick/qml/admin/ReportingScreen.qml` — add the "Include detailed roster in export" `LCheckbox` to the export bar (Task 5).
- **Modify** `qt-app/quick/tests/tst_qml_admin.qml` — extend `reportingStub` + add checkbox QuickTests (Task 5).

**No new files.** `LCheckbox.qml` already exists (`qt-app/quick/qml/components/LCheckbox.qml`: `checked` property, `toggled(bool)` signal, PlainText label, theme-token styling).

**Not touched:** legacy `qt-app/adminwindow.cpp` has its own `adminWindow::paintReport` / `exportReportToExcel` and does **not** call `ReportRenderer::*` — WITS.exe is unaffected by the signature change.

---

### Task 1: VM `includeRosterInExport` property (default OFF, independent of screen roster)

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Produces: `bool includeRosterInExport() const`; `Q_INVOKABLE void setIncludeRosterInExport(bool)`; signal `includeRosterInExportChanged()`; member `bool m_includeRosterInExport = false`. Consumed by the renderer callers (Task 2) and the QML checkbox (Task 5).

- [ ] **Step 1: Write the failing tests**

In `qt-app/quick/tests/tst_reportingviewmodel.cpp`, add two `private slots` and their bodies (place beside the other export tests; follow the file's existing `QSignalSpy` style):

```cpp
void includeRosterInExport_defaultsFalse();
void setIncludeRosterInExport_togglesAndSignals();
```

```cpp
void TstReportingViewModel::includeRosterInExport_defaultsFalse() {
    ReportingViewModel vm;
    QCOMPARE(vm.includeRosterInExport(), false);   // spec §9: default OFF
}

void TstReportingViewModel::setIncludeRosterInExport_togglesAndSignals() {
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::includeRosterInExportChanged);
    vm.setIncludeRosterInExport(true);
    QCOMPARE(vm.includeRosterInExport(), true);
    QCOMPARE(spy.count(), 1);
    vm.setIncludeRosterInExport(true);   // no-op on unchanged value
    QCOMPARE(spy.count(), 1);
    vm.setIncludeRosterInExport(false);
    QCOMPARE(vm.includeRosterInExport(), false);
    QCOMPARE(spy.count(), 2);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_reportingviewmodel --output-on-failure`
Expected: compile error / FAIL — `includeRosterInExport` and `setIncludeRosterInExport` are not declared.

- [ ] **Step 3: Add the property, accessor, setter, signal, and member**

In `ReportingViewModel.h`, add the `Q_PROPERTY` beside the other export properties (after `exportError`, ~line 65):
```cpp
    Q_PROPERTY(bool includeRosterInExport READ includeRosterInExport
               WRITE setIncludeRosterInExport NOTIFY includeRosterInExportChanged)
```
Add the getter beside `exportError()` (~line 129):
```cpp
    bool includeRosterInExport() const { return m_includeRosterInExport; }
```
Add the invokable beside `setChartType` (~line 132):
```cpp
    Q_INVOKABLE void setIncludeRosterInExport(bool v);
```
Add the signal beside `exportErrorChanged()` (~line 182):
```cpp
    void includeRosterInExportChanged();
```
Add the member beside `m_exportRows` (~line 230):
```cpp
    bool m_includeRosterInExport = false;   // spec §9: export roster is opt-in, default OFF
```

- [ ] **Step 4: Implement the setter**

In `ReportingViewModel.cpp`, add beside `setChartType` (~line 404):
```cpp
void ReportingViewModel::setIncludeRosterInExport(bool v)
{
    if (m_includeRosterInExport == v) return;
    m_includeRosterInExport = v;
    emit includeRosterInExportChanged();
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_reportingviewmodel --output-on-failure`
Expected: PASS (both new tests green; the rest of the suite unchanged).

- [ ] **Step 6: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): VM includeRosterInExport flag (default off) for export roster opt-in"
```

---

### Task 2: Renderer signatures gain `ReportAnalytics` + `includeRoster`; roster becomes conditional

**Files:**
- Modify: `qt-app/core/reportrenderer.h`
- Modify: `qt-app/core/reportrenderer.cpp`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` (the two call sites)
- Modify: `qt-app/tests/tst_reportrenderer.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `struct ReportAnalytics` from `reportanalytics.h` (4b-iii-a).
- Produces: new signatures —
  - `static bool paintReport(QPagedPaintDevice*, int resolution, const QJsonArray &data, const QJsonObject &filters, const ReportPalette&, const ReportHeaderInfo&, const ReportAnalytics &analytics, bool includeRoster)`
  - `static bool writeReportToXlsx(QXlsx::Document&, const QJsonArray &rows, const QJsonObject &filters, const ReportHeaderInfo&, const ReportAnalytics &analytics, bool includeRoster)`
- In THIS task the two new params are threaded through and `includeRoster` gates the existing roster (PDF table + Excel rows); the KPI/ranking CONTENT is added in Tasks 3–4. Intermediate state: a default (`includeRoster=false`) export omits the roster and does not yet show analytics — resolved by Tasks 3–4. Each commit still builds and its tests pass.

- [ ] **Step 1: Write the failing tests**

In `qt-app/tests/tst_reportrenderer.cpp`, add `#include "reportanalytics.h"` near the other includes, add a helper beside `sampleRows()`:
```cpp
    static ReportAnalytics sampleAnalytics() {
        return ReportAnalytics::compute(sampleRows());   // visits already numeric
    }
```
Add two slots + bodies:
```cpp
void writeReportToXlsx_rosterSheetPresentOnlyWhenIncluded();
void paintReport_writesPdfWithAndWithoutRoster();
```

```cpp
void TstReportRenderer::writeReportToXlsx_rosterSheetPresentOnlyWhenIncluded() {
    {   // includeRoster = false -> no "Detailed Roster" sheet
        QXlsx::Document xlsx;
        QVERIFY(ReportRenderer::writeReportToXlsx(
            xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false));
        QVERIFY(!xlsx.sheetNames().contains("Detailed Roster"));
    }
    {   // includeRoster = true -> "Detailed Roster" sheet present
        QXlsx::Document xlsx;
        QVERIFY(ReportRenderer::writeReportToXlsx(
            xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), true));
        QVERIFY(xlsx.sheetNames().contains("Detailed Roster"));
    }
}

void TstReportRenderer::paintReport_writesPdfWithAndWithoutRoster() {
    for (bool includeRoster : { false, true }) {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("report.pdf");
        {
            QPdfWriter pdf(path);
            pdf.setResolution(300);
            QVERIFY(ReportRenderer::paintReport(
                &pdf, 300, sampleRows(), sampleFilters(), samplePalette(),
                sampleHeaderInfo(), sampleAnalytics(), includeRoster));
        }
        QVERIFY(QFileInfo(path).size() > 0);
    }
}
```
Update the two EXISTING call sites to the new arity:
- `paintReport_writesPdf()` — change the `paintReport(...)` call to append `, sampleAnalytics(), true`.
- `writeReportToXlsx_populatesCells()` — change the `writeReportToXlsx(...)` call to append `, sampleAnalytics(), true`. (That test scans for `"2023-00001"` / `"Test Student One"`; with `includeRoster=true` the roster is present, so it still passes once the roster lands on the Detailed Roster sheet in Task 3 — for THIS task the roster stays on the current single sheet, so it also passes. Leave the test as-is beyond the arity change.)

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_reportrenderer --output-on-failure`
Expected: compile error — `paintReport` / `writeReportToXlsx` do not accept the extra arguments (and `ReportAnalytics` is undeclared until the CMake + header edits land).

- [ ] **Step 3: Change the header signatures**

In `qt-app/core/reportrenderer.h`, add the include after the existing `#include "reportdata.h"` (line 11):
```cpp
#include "reportanalytics.h"   // ReportAnalytics — passed in; renderer never re-aggregates
```
Replace the `paintReport` declaration (lines 49–52):
```cpp
    static bool paintReport(QPagedPaintDevice *device, int resolution,
                            const QJsonArray &data, const QJsonObject &filters,
                            const ReportPalette &palette,
                            const ReportHeaderInfo &info,
                            const ReportAnalytics &analytics, bool includeRoster);
```
Replace the `writeReportToXlsx` declaration (lines 54–57):
```cpp
    static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                  const QJsonArray &rows,
                                  const QJsonObject &filters,
                                  const ReportHeaderInfo &info,
                                  const ReportAnalytics &analytics, bool includeRoster);
```

- [ ] **Step 4: Update the renderer definitions to the new arity + gate the roster**

In `qt-app/core/reportrenderer.cpp`:

Update the `paintReport` definition signature (lines 318–321) to match the header (add `const ReportAnalytics &analytics, bool includeRoster`). Mark `analytics` unused for now so the compiler is quiet until Task 4 consumes it — add at the top of the body, right after `QPainter painter;`:
```cpp
    Q_UNUSED(analytics);   // consumed by the KPI/ranking layouts in Task 4
```
Gate the existing roster table: wrap the `// ===== TABLE =====` block — from the `int col1 = margin;` column-setup (line 439) through the end of the `for (auto v : data) { ... }` row loop (the closing brace at line 513) — in `if (includeRoster) { ... }`. Leave the header/filters above and the charts/prepared-by below outside the guard.

Update the `writeReportToXlsx` definition signature (lines 611–615) to match the header. Mark the new params unused for now (Task 3 consumes them) — add at the top of the body:
```cpp
    Q_UNUSED(analytics);   // consumed by the Summary sheet in Task 3
```
Gate the roster rows: wrap the `// ===== TABLE ROWS =====` loop (the `for (const auto &val : rows) { ... }` block, lines 675–692) in `if (includeRoster) { ... }`. (The header/filters/table-headers and footer stay unconditional for this task; Task 3 restructures into sheets.)

- [ ] **Step 5: Update the VM call sites to pass the analytics + flag**

In `qt-app/quick/viewmodels/ReportingViewModel.cpp` (`reportanalytics.h` is already included, line 13):

`renderToDevice` (lines 437–442) — replace the `paintReport` call:
```cpp
bool ReportingViewModel::renderToDevice(QPagedPaintDevice *dev, int resolution)
{
    const QJsonObject filters = currentExportFilters();
    const ReportPalette pal = ReportController::getPalette(m_palette);
    return ReportRenderer::paintReport(dev, resolution, m_exportRows, filters, pal,
                                       headerInfo(),
                                       ReportAnalytics::compute(m_exportRows),
                                       m_includeRosterInExport);
}
```

`exportExcel` lambda (line 518) — replace the `writeReportToXlsx` call:
```cpp
        const bool ok = ReportRenderer::writeReportToXlsx(
                            doc, m_exportRows, currentExportFilters(), headerInfo(),
                            ReportAnalytics::compute(m_exportRows), m_includeRosterInExport)
                        && doc.saveAs(path);
```

- [ ] **Step 6: Add reportanalytics to the tst_reportrenderer CMake sources**

In `qt-app/tests/CMakeLists.txt`, in the `qt_add_executable(tst_reportrenderer ...)` block (lines 155–160), add the two files after `reportrenderer.h`:
```cmake
qt_add_executable(tst_reportrenderer
    tst_reportrenderer.cpp
    ${CMAKE_SOURCE_DIR}/core/reportrenderer.cpp
    ${CMAKE_SOURCE_DIR}/core/reportrenderer.h
    ${CMAKE_SOURCE_DIR}/core/reportanalytics.cpp
    ${CMAKE_SOURCE_DIR}/core/reportanalytics.h
    ${CMAKE_SOURCE_DIR}/core/reportdata.h
)
```
(The target already has `${CMAKE_SOURCE_DIR}/core` on its include path — no include-dir change needed.)

- [ ] **Step 7: Reconfigure, build, and run the tests to verify they pass**

Run:
```
cmake -S qt-app -B C:/b/loams-4biiib -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-4biiib
ctest --test-dir C:/b/loams-4biiib -R "tst_reportrenderer|tst_reportingviewmodel" --output-on-failure
```
Expected: PASS. (CMakeLists changed → reconfigure is required.)

- [ ] **Step 8: Run the full suite to confirm no regressions**

Run: `ctest --test-dir C:/b/loams-4biiib --output-on-failure`
Expected: all green (same count as baseline).

- [ ] **Step 9: Commit**

```bash
git add qt-app/core/reportrenderer.h qt-app/core/reportrenderer.cpp qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/tests/tst_reportrenderer.cpp qt-app/tests/CMakeLists.txt
git commit -m "feat(reporting): thread ReportAnalytics + includeRoster into export renderer; roster now conditional"
```

---

### Task 3: Excel multi-sheet — "Summary" (KPIs + rankings) + optional "Detailed Roster"

**Files:**
- Modify: `qt-app/core/reportrenderer.cpp` (`writeReportToXlsx`)
- Test: `qt-app/tests/tst_reportrenderer.cpp`

**Interfaces:**
- Consumes: `analytics.kpis` (`totalVisits`, `uniqueVisitors`, `avgVisitsPerVisitor`, `topDepartment`, `topDepartmentVisits`), `analytics.topStudents`, `analytics.topCourses`, `analytics.topDepartments` (each `QList<RankingEntry>` with `rank`, `label`, `sublabel`, `visits`, `percentOfTotal`).
- Produces: a workbook whose first sheet is named `"Summary"` and, iff `includeRoster`, a second sheet `"Detailed Roster"`.

- [ ] **Step 1: Write the failing tests**

In `qt-app/tests/tst_reportrenderer.cpp`, add slots + bodies:
```cpp
void writeReportToXlsx_summarySheetHasKpisAndRankings();
void writeReportToXlsx_rosterOnSeparateSheetWhenIncluded();
```

```cpp
void TstReportRenderer::writeReportToXlsx_summarySheetHasKpisAndRankings() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false));

    QVERIFY(xlsx.sheetNames().contains("Summary"));
    QVERIFY(xlsx.selectSheet("Summary"));

    // The Summary sheet carries KPI labels and at least one ranking heading.
    // sampleRows(): BSIT=3 + BSCS=5 -> total 8 visits, 2 unique students.
    bool foundTotalLabel = false, foundTotalValue = false, foundRankingHeading = false;
    for (int r = 1; r <= 60; ++r) {
        for (int c = 1; c <= 8; ++c) {
            const QString cell = xlsx.read(r, c).toString();
            if (cell.contains("Total Visits", Qt::CaseInsensitive)) foundTotalLabel = true;
            if (cell == "8") foundTotalValue = true;
            if (cell.contains("Top 10 Students", Qt::CaseInsensitive)) foundRankingHeading = true;
        }
    }
    QVERIFY(foundTotalLabel);
    QVERIFY(foundTotalValue);
    QVERIFY(foundRankingHeading);
}

void TstReportRenderer::writeReportToXlsx_rosterOnSeparateSheetWhenIncluded() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), true));

    QVERIFY(xlsx.sheetNames().contains("Detailed Roster"));
    QVERIFY(xlsx.selectSheet("Detailed Roster"));
    bool foundSchoolId = false;
    for (int r = 1; r <= 20; ++r)
        for (int c = 1; c <= 8; ++c)
            if (xlsx.read(r, c).toString() == "2023-00001") foundSchoolId = true;
    QVERIFY(foundSchoolId);
}
```
Update `writeReportToXlsx_populatesCells()` — it currently scans the current (first) sheet for the school name in cell (1,1) and student data. After the restructure the first sheet is "Summary" (school name still in (1,1)) but the student roster moves to the "Detailed Roster" sheet. Change that test to select the roster sheet before scanning for the student id/name:
```cpp
    QCOMPARE(xlsx.read(1, 1).toString(), sampleHeaderInfo().schoolName);   // Summary title
    QVERIFY(xlsx.selectSheet("Detailed Roster"));
    // ... existing scan for "2023-00001" / "Test Student One" ...
```
(Its call already passes `sampleAnalytics(), true` from Task 2, so the roster sheet exists.)

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_reportrenderer --output-on-failure`
Expected: FAIL — no "Summary" sheet, no KPI labels, roster still on the single default sheet.

- [ ] **Step 3: Restructure `writeReportToXlsx` into sheets**

In `qt-app/core/reportrenderer.cpp`, rewrite the `writeReportToXlsx` body. Remove the `Q_UNUSED(analytics);` line added in Task 2. Rename the default sheet to "Summary", write the header + filters (as today) into it, then the KPI block and the three ranking tables from `analytics`; then, iff `includeRoster`, add a "Detailed Roster" sheet and write the existing 8-column roster there. Replace the whole body with:

```cpp
bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx,
                                       const QJsonArray &rows,
                                       const QJsonObject &filters,
                                       const ReportHeaderInfo &info,
                                       const ReportAnalytics &analytics,
                                       bool includeRoster)
{
    const int colCount = 8;

    QXlsx::Format titleFmt;
    titleFmt.setFontBold(true);
    titleFmt.setFontSize(16);
    titleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format subTitleFmt;
    subTitleFmt.setFontSize(11);
    subTitleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format sectionFmt;
    sectionFmt.setFontBold(true);
    sectionFmt.setFontSize(12);

    QXlsx::Format hdrFmt;
    hdrFmt.setFontBold(true);
    hdrFmt.setPatternBackgroundColor(QColor("#D6EAF8"));
    hdrFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    // ===== SHEET 1: SUMMARY (renamed from the default sheet) =====
    xlsx.renameSheet(xlsx.sheetNames().first(), QStringLiteral("Summary"));
    xlsx.selectSheet(QStringLiteral("Summary"));

    int row = 1;
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), titleFmt);
    xlsx.write(row++, 1, info.schoolName, titleFmt);
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, info.address, subTitleFmt);
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, QString("Library Report - %1 to %2")
                             .arg(filters["start"].toString(), filters["end"].toString()), subTitleFmt);
    row += 1;

    xlsx.write(row++, 1, QString("Department: %1 | Course: %2 | School Year: %3")
                             .arg(filters["department"].toString(),
                                  filters["course"].toString(),
                                  filters["schoolYear"].toString()));
    row += 1;

    // --- KPI block (label | value pairs) ---
    xlsx.write(row++, 1, QStringLiteral("Summary"), sectionFmt);
    xlsx.write(row, 1, QStringLiteral("Total Visits"));
    xlsx.write(row++, 2, analytics.kpis.totalVisits);
    xlsx.write(row, 1, QStringLiteral("Unique Visitors"));
    xlsx.write(row++, 2, analytics.kpis.uniqueVisitors);
    xlsx.write(row, 1, QStringLiteral("Avg. Visits / Visitor"));
    xlsx.write(row++, 2, QString::number(analytics.kpis.avgVisitsPerVisitor, 'f', 1));
    xlsx.write(row, 1, QStringLiteral("Top Department"));
    xlsx.write(row, 2, analytics.kpis.hasData ? analytics.kpis.topDepartment : QStringLiteral("—"));
    xlsx.write(row++, 3, analytics.kpis.topDepartmentVisits);
    row += 1;

    // --- Ranking tables ---
    auto writeRanking = [&](const QString &heading, const QStringList &headers,
                            const QList<RankingEntry> &entries, bool withSublabel, bool withPercent) {
        xlsx.write(row++, 1, heading, sectionFmt);
        for (int c = 0; c < headers.size(); ++c)
            xlsx.write(row, c + 1, headers[c], hdrFmt);
        row++;
        for (const RankingEntry &e : entries) {
            int c = 1;
            xlsx.write(row, c++, e.rank);
            xlsx.write(row, c++, e.label);
            if (withSublabel) xlsx.write(row, c++, e.sublabel);
            xlsx.write(row, c++, e.visits);
            if (withPercent) xlsx.write(row, c++, QString::number(e.percentOfTotal, 'f', 1) + "%");
            row++;
        }
        row += 1;
    };
    writeRanking(QStringLiteral("Top 10 Students"),
                 { "Rank", "Name", "Course", "Visits" }, analytics.topStudents, true, false);
    writeRanking(QStringLiteral("Top 10 Courses"),
                 { "Rank", "Course", "Visits", "% of Total" }, analytics.topCourses, false, true);
    writeRanking(QStringLiteral("Top 10 Departments"),
                 { "Rank", "Department", "Visits", "% of Total" }, analytics.topDepartments, false, true);

    xlsx.write(row++, 1,
               "This is a system-generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");
    row += 1;
    xlsx.write(row++, 1, QString("Prepared by: %1").arg(info.librarian));
    xlsx.write(row++, 1, info.position);

    // ===== SHEET 2: DETAILED ROSTER (only when requested) =====
    if (includeRoster) {
        xlsx.addSheet(QStringLiteral("Detailed Roster"));   // becomes current
        int rr = 1;
        const QStringList headers = {"School ID", "Name", "Gender", "Course",
                                     "Year Level", "Department", "Status", "Visits"};
        for (int c = 0; c < headers.size(); ++c)
            xlsx.write(rr, c + 1, headers[c], hdrFmt);
        rr++;
        QXlsx::Format evenFmt, oddFmt;
        evenFmt.setPatternBackgroundColor(QColor("#F9F9F9"));
        oddFmt.setPatternBackgroundColor(QColor("#FFFFFF"));
        for (const auto &val : rows) {
            const QJsonObject obj = val.toObject();
            const QStringList rowData = {
                obj["school_id"].toString(), obj["name"].toString(), obj["gender"].toString(),
                obj["course"].toString(), obj["year_level"].toString(), obj["department"].toString(),
                obj["status"].toString(), QString::number(obj["visits"].toInt())
            };
            for (int c = 0; c < rowData.size(); ++c)
                xlsx.write(rr, c + 1, rowData[c], (rr % 2 == 0) ? evenFmt : oddFmt);
            rr++;
        }
        for (int c = 0; c < headers.size(); ++c)
            xlsx.setColumnWidth(c + 1, headers[c].length() + 5);
        xlsx.selectSheet(QStringLiteral("Summary"));   // leave Summary active
    }

    return true;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_reportrenderer --output-on-failure`
Expected: PASS — Summary sheet with KPI labels + ranking headings; roster on its own sheet only when included.

- [ ] **Step 5: Commit**

```bash
git add qt-app/core/reportrenderer.cpp qt-app/tests/tst_reportrenderer.cpp
git commit -m "feat(reporting): Excel export multi-sheet — Summary (KPIs + rankings) + optional Detailed Roster"
```

---

### Task 4: PDF structural reorder — KPI summary → rankings → demoted chart → optional roster

**Files:**
- Modify: `qt-app/core/reportrenderer.cpp` (`paintReport`)
- Test: `qt-app/tests/tst_reportrenderer.cpp`

**Interfaces:**
- Consumes: the same `analytics` fields as Task 3.
- Produces: a PDF laid out header/filters → KPI summary → rankings → chart(s) → optional roster → prepared-by (spec §8). Page-break logic re-verified for the new section order.

- [ ] **Step 1: Write the failing test**

PDF text is not readable back through QtTest, so assert the invariants that are checkable: `paintReport` succeeds and writes a non-empty PDF at both the default-ish 300 DPI and the QPdfWriter default 1200 DPI, with `includeRoster` both ways, without overlap-arithmetic regressions. Add:
```cpp
void paintReport_writesAnalyticsPdfAtHighDpi();
```
```cpp
void TstReportRenderer::paintReport_writesAnalyticsPdfAtHighDpi() {
    for (int resolution : { 300, 1200 }) {
        for (bool includeRoster : { false, true }) {
            QTemporaryDir dir;
            QVERIFY(dir.isValid());
            const QString path = dir.filePath("analytics.pdf");
            {
                QPdfWriter pdf(path);
                pdf.setResolution(resolution);
                QVERIFY(ReportRenderer::paintReport(
                    &pdf, resolution, sampleRows(), sampleFilters(), samplePalette(),
                    sampleHeaderInfo(), sampleAnalytics(), includeRoster));
            }
            QVERIFY(QFileInfo(path).size() > 0);
        }
    }
}
```
(`paintReport_writesPdfWithAndWithoutRoster` from Task 2 continues to guard the roster-flag paths at 300 DPI; this adds the 1200-DPI paths.)

- [ ] **Step 2: Run the test to verify it fails / passes-trivially**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_reportrenderer --output-on-failure`
Expected: this smoke test may PASS against the Task-2 body (which already writes a PDF), so treat Step 1 as a guard and drive the reorder from the spec — the behavioral proof is the manual release gate (Task 6). Proceed to Step 3 regardless; the test must still be green after the rewrite.

- [ ] **Step 3: Reorder the paint body**

In `qt-app/core/reportrenderer.cpp`, `paintReport`:

1. Remove the `Q_UNUSED(analytics);` added in Task 2.
2. Add a reusable page-break helper near the top of the body, after the `vs` lambda (line 344) — it advances to a fresh page when the next block would overflow:
```cpp
    auto newPageIfNeeded = [&](int needed) {
        if (y > usableHeight - vs(needed)) {
            drawFooter(currentPage);
            device->newPage();
            currentPage++;
            y = margin;
            drawHeader(y);
            painter.setFont(QFont("Arial", 10));
        }
    };
```
(Declare it after `drawFooter` and `drawHeader` are defined; both are lambdas capturing `[&]`. `drawHeader` is defined at line 382 and `drawFooter` at 357 — place `newPageIfNeeded` immediately after `drawHeader(y);` at line 422, so both captured lambdas already exist.)

3. Keep the header (`drawHeader(y);`) and the FILTERS line (lines 425–434) as the first content.

4. **Insert the KPI summary** immediately after the filters line, before the roster:
```cpp
    // ===== KPI SUMMARY (spec §8: after context, before rankings) =====
    painter.setFont(QFont("Arial", 13, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(QRect(margin, y, usableWidth, vs(24)), Qt::AlignLeft, "Summary");
    y += vs(30);
    painter.setFont(QFont("Arial", 11));
    auto kpiLine = [&](const QString &label, const QString &value) {
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft,
                         QString("%1: %2").arg(label, value));
        y += vs(24);
    };
    kpiLine("Total Visits", QString::number(analytics.kpis.totalVisits));
    kpiLine("Unique Visitors", QString::number(analytics.kpis.uniqueVisitors));
    kpiLine("Avg. Visits / Visitor", QString::number(analytics.kpis.avgVisitsPerVisitor, 'f', 1));
    kpiLine("Top Department",
            QString("%1 (%2 visits)")
                .arg(analytics.kpis.hasData ? analytics.kpis.topDepartment : QStringLiteral("—"))
                .arg(analytics.kpis.topDepartmentVisits));
    y += vs(16);
```

5. **Insert the three ranking tables** after the KPI block:
```cpp
    // ===== RANKINGS (spec §8) =====
    auto drawRanking = [&](const QString &heading, const QList<RankingEntry> &entries,
                           bool withSublabel, bool withPercent) {
        newPageIfNeeded(220);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.setPen(Qt::black);
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft, heading);
        y += vs(28);
        painter.setFont(QFont("Arial", 10));
        const QFontMetrics rfm = painter.fontMetrics();
        const int pitch = qMax(vs(20), rfm.height() + vs(4));
        int idx = 0;
        for (const RankingEntry &e : entries) {
            newPageIfNeeded(80);
            const int cRank = margin;
            const int cLabel = margin + int(usableWidth * 0.10);
            const int cSub = margin + int(usableWidth * 0.55);
            const int cVisits = margin + int(usableWidth * 0.78);
            const int cPct = margin + int(usableWidth * 0.90);
            painter.fillRect(QRect(margin, y - rfm.ascent(), usableWidth, pitch),
                             (idx % 2 == 0) ? palette.rowEvenBg : palette.rowOddBg);
            painter.setPen(palette.rowText);
            painter.drawText(cRank, y, QString::number(e.rank));
            painter.drawText(cLabel, y, rfm.elidedText(e.label, Qt::ElideRight, cSub - cLabel - vs(5)));
            if (withSublabel)
                painter.drawText(cSub, y, rfm.elidedText(e.sublabel, Qt::ElideRight, cVisits - cSub - vs(5)));
            painter.drawText(cVisits, y, QString::number(e.visits));
            if (withPercent)
                painter.drawText(cPct, y, QString::number(e.percentOfTotal, 'f', 1) + "%");
            y += pitch;
            idx++;
        }
        y += vs(16);
    };
    drawRanking("Top 10 Students", analytics.topStudents, true, false);
    drawRanking("Top 10 Courses", analytics.topCourses, false, true);
    drawRanking("Top 10 Departments", analytics.topDepartments, false, true);
```

6. **Roster demoted + optional.** Keep the `if (includeRoster) { ... }`-gated roster table from Task 2, but MOVE it to run AFTER the charts block (see next), i.e. relocate the whole `// ===== TABLE =====` block to just before `// ===== PREPARED BY =====`. Precede it with `newPageIfNeeded(0);` (force the roster to start on a clean region — it internally handles its own multi-page breaks via the existing `if (y > usableHeight - vs(200))`), and add a "Detailed Roster" heading:
```cpp
    if (includeRoster) {
        newPageIfNeeded(300);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.setPen(Qt::black);
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft, "Detailed Roster");
        y += vs(30);
        // ... the existing column-setup + header-row + row loop from Task 2, unchanged ...
    }
```

7. **Charts demoted below rankings.** The existing charts block (lines 515–573: `drawFullscreenChart` lambda + the `chartChoice` branching) already runs on its own new page(s). Leave it as-is in place — since KPI + rankings now precede it and the roster now follows it, the document order becomes context → KPI → rankings → chart → roster → prepared-by. Confirm the ordering by reading the body top-to-bottom after the edit.

8. Leave the `// ===== PREPARED BY =====` block last, unchanged.

- [ ] **Step 4: Build and run the renderer tests**

Run:
```
cmake --build C:/b/loams-4biiib
ctest --test-dir C:/b/loams-4biiib -R tst_reportrenderer --output-on-failure
```
Expected: PASS (all `paintReport_*` tests green at 300 and 1200 DPI, both roster flags).

- [ ] **Step 5: Run the full suite**

Run: `ctest --test-dir C:/b/loams-4biiib --output-on-failure`
Expected: all green.

- [ ] **Step 6: Commit**

```bash
git add qt-app/core/reportrenderer.cpp qt-app/tests/tst_reportrenderer.cpp
git commit -m "feat(reporting): PDF export reorder — KPI summary + rankings, chart demoted, roster optional"
```

---

### Task 5: QML "Include detailed roster in export" checkbox (independent of screen roster toggle)

**Files:**
- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: `vm.includeRosterInExport` (bool) + `vm.setIncludeRosterInExport(bool)` (Task 1); the existing `LCheckbox` component (`checked`, `toggled(bool)`, `label`).
- Produces: an `objectName: "includeRosterCheck"` control in the export bar.

- [ ] **Step 1: Write the failing QuickTests**

In `qt-app/quick/tests/tst_qml_admin.qml`, extend `reportingStub` (after `chartType`, ~line 2562) with the flag + setter + counter:
```qml
        property bool includeRosterInExport: false
        property int setIncludeRosterCount: 0
        function setIncludeRosterInExport(v) { includeRosterInExport = v; setIncludeRosterCount++ }
```
Add to the `ReportingScreen` `TestCase` (after `test_exportErrorPersistsAsFeedback`, ~line 2707):
```qml
        function test_includeRosterCheckboxDefaultsUncheckedAndWritesVm() {
            reportingStub.includeRosterInExport = false;
            var chk = findChild(reporting, "includeRosterCheck");
            verify(chk, "include-roster checkbox exists");
            compare(chk.checked, false, "defaults to unchecked (spec §9 default OFF)");
            var before = reportingStub.setIncludeRosterCount;
            mouseClick(chk);
            compare(reportingStub.setIncludeRosterCount, before + 1);
            compare(reportingStub.includeRosterInExport, true);
            reportingStub.includeRosterInExport = false;
        }

        function test_includeRosterIsIndependentOfScreenRosterToggle() {
            // Spec §9: the export checkbox and the on-screen "View full roster"
            // toggle are independent — neither follows the other.
            reportingStub.includeRosterInExport = false;
            reporting.showRoster = false;
            var toggle = findChild(reporting, "viewRosterToggle");
            mouseClick(toggle);                       // expand the on-screen roster
            compare(reporting.showRoster, true);
            compare(reportingStub.includeRosterInExport, false,
                    "screen toggle must NOT flip the export flag");
            reporting.showRoster = false;
        }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/loams-4biiib -R tst_qml_admin --output-on-failure`
Expected: FAIL — `findChild(reporting, "includeRosterCheck")` returns null.

- [ ] **Step 3: Add the checkbox to the export bar**

In `qt-app/quick/qml/admin/ReportingScreen.qml`, the export bar is a `Rectangle` whose single child is `RowLayout { id: exportRow ... }` and whose height is `exportRow.implicitHeight + Theme.spacing.xl * 2` (lines 389–453). Wrap the row in a `ColumnLayout` so the checkbox sits on its own line above the controls:

Replace the Rectangle's child (`RowLayout { id: exportRow ... }`) with:
```qml
            ColumnLayout {
                id: exportCol
                anchors.fill: parent
                anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md

                LCheckbox {
                    objectName: "includeRosterCheck"
                    label: qsTr("Include detailed roster in export")
                    checked: screen.vm ? screen.vm.includeRosterInExport : false
                    onToggled: function(c) { if (screen.vm) screen.vm.setIncludeRosterInExport(c); }
                }

                RowLayout {
                    id: exportRow
                    Layout.fillWidth: true
                    spacing: Theme.spacing.md
                    // ... the existing paletteCombo / chartTypeCombo / spacer /
                    //     exportPdfButton / exportExcelButton / printButton, unchanged ...
                }
            }
```
Update the Rectangle's height to track the column: change `implicitHeight: exportRow.implicitHeight + Theme.spacing.xl * 2` to `implicitHeight: exportCol.implicitHeight + Theme.spacing.xl * 2`. (`exportRow` previously used `anchors.fill: parent` + `anchors.margins`; drop those two lines — it is now a `ColumnLayout` child sized by the layout, hence `Layout.fillWidth: true` and no anchors.)

- [ ] **Step 4: Build and run the QuickTests**

Run:
```
cmake --build C:/b/loams-4biiib
ctest --test-dir C:/b/loams-4biiib -R tst_qml_admin --output-on-failure
```
Expected: PASS — both new tests green; the existing reporting tests (which find `exportPdfButton` etc. via `findChild`, recursive) still pass since those objects moved but keep their objectNames.

- [ ] **Step 5: Run the full suite**

Run: `ctest --test-dir C:/b/loams-4biiib --output-on-failure`
Expected: all green.

- [ ] **Step 6: Commit**

```bash
git add qt-app/quick/qml/admin/ReportingScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(reporting): export-roster checkbox on the reporting export bar (default off, independent of screen toggle)"
```

---

### Task 6: Manual `WITSQuick.exe` export smoke — RELEASE GATE (no code)

**This is a mandatory owner-run release gate, not a TDD task.** The OFFSCREEN QtTests run under `QApplication` and cannot exercise the real export path end-to-end (spec §7.4 / §10). Do not claim the slice complete until this passes.

- [ ] **Step 1: Build and launch the real app**

```
cmake --build C:/b/loams-4biiib
```
Run `C:/b/loams-4biiib/quick/WITSQuick.exe` (close any other running `WITSQuick.exe` first — it locks the exe). Sign in to Admin → Reporting.

- [ ] **Step 2: Generate a report with results**

Pick a department (or All Departments) + a valid duration with known data, Generate. Confirm the on-screen dashboard renders (KPI band, rankings, chart, roster toggle) — that is 4b-iii-a and should already work.

- [ ] **Step 3: Export with the roster OFF (default)**

Leave "Include detailed roster in export" unchecked.
- **Export PDF** → open it. Verify section order: header/filters → **Summary (4 KPIs)** → **Top 10 Students / Courses / Departments** → **chart (below the rankings)** → **NO roster** → prepared-by. Verify no row overlap at the 1200-DPI default.
- **Export Excel** → open it. Verify a **"Summary"** sheet (KPIs + three ranking tables) and **NO "Detailed Roster"** sheet.
- **Print** → the dialog opens; print to PDF or a printer; confirm it does not crash and matches the PDF layout.

- [ ] **Step 4: Export with the roster ON**

Check "Include detailed roster in export".
- **Export PDF** → the per-student roster now appears **after the chart**, before prepared-by.
- **Export Excel** → a second **"Detailed Roster"** sheet is present with the full per-student table; the "Summary" sheet is unchanged.

- [ ] **Step 5: Confirm independence**

Toggle the on-screen "View full roster" and confirm it does **not** change what the export checkbox produces, and vice versa (spec §9).

- [ ] **Step 6: Record the result**

Note pass/fail (and any layout issues) for the PR body. On failure, route through `superpowers:systematic-debugging` (reproduce with a failing test where possible) before fixing.

---

## Post-implementation (outside the task loop)

1. **`/claude-review`** (branch mode) — fix Critical/Important, resubmit until APPROVE or 3 rounds.
2. **`create-pr`** — the project 3-agent gate (`dry-checker`, `security-reviewer`, `general-code-reviewer`; **no `api-checker`** — if a loaded `create-pr` names it, re-read `.claude/skills/create-pr/SKILL.md`).
3. Owner merges via `/merge-pr` (PR-open ≠ merge approval — ask separately).

**Deferred / forward (not this slice):** Excel "Charts" sheet (QXlsx data-only, out of scope, spec §8/§11); **4b-iv** time/"When?" analytics (needs a `get_report_data.php` change to expose a login-time breakdown, spec §11).
