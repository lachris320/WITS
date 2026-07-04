# Design Spec: ReportController + ReportRenderer Extraction

**Date:** 2026-07-04
**Status:** Draft (awaiting user review)
**Scope:** Step 5 (final) of the WITS admin window structural refactor

---

## Context

`adminWindow` is the last large God Object surface. Four prior steps extracted
one tab each into a controller + value types:

- Step 1 (PR #8) — Settings → `SettingsController` + `SettingsData`.
- Step 2 (PR #10) — Visitor Logs → `VisitorController` + `visitordata.h`.
- Step 3 (PR #11) — Database Import → `ImportController` + `importdata.h`.
- Step 4 (PR #12) — Student Search → `StudentController` + `studentdata.h`.

This **final** step extracts the **Reports tab** — the largest remaining chunk
(~1,500 lines of `qt-app/adminwindow.cpp`) spanning four concerns: filter/data
network, palette selection, chart-image rendering, and PDF/Excel/print export.

Unlike the prior four (clean network + JSON), Reports mixes network code with
heavy `Qt::Gui`/`QtCharts`/`QXlsx`/`QPagedPaintDevice` painting that does not
fit the "injected `QNetworkAccessManager` + pure static parser" mold in one
class. Per the user's decision, the extraction splits into **two units**:

- **`ReportController` (QObject)** — network/data/pure-logic (Core + Network
  only): filter/course/year/report-data fetches, response parsers, the pure
  duration→date-range math, and palette selection.
- **`ReportRenderer` (plain class, no QObject)** — Gui/painting/export
  (Gui/Charts/QXlsx): the three chart-image makers, the multi-page
  `paintReport`, and the QXlsx table writer. Stateless: it takes all
  environment (school info, library hours) as parameters, reads no `QSettings`
  and no `ui->`.

`adminWindow` remains the **View**: it owns all `ui->` widget reads, every
`QMessageBox`/`QFileDialog`/`QPrintDialog`, the QtCharts **live preview**
widget tree (`updateChartsPreview`/`optimizeChartForPreview`, which build
`QChartView` widgets into `chartsPreviewBox`), the `QPdfWriter`/`QPrinter`
device creation, and the `QSettings` reads that feed the renderer.

**Goal:** behavior-identical extraction — no new features, no regressions, no
wire-protocol change. Wire endpoints (`get_report_data.php`,
`get_departments.php`, `get_years.php`, `get_courses.php`,
`api.php/reports/data`), payload shapes, every dialog text, chart appearance,
PDF/Excel layout, and combo population remain byte-identical. Unit tests ship
with this step, per the project workflow rule.

### The live Reports logic today (in `qt-app/adminwindow.cpp`)

**Network / data:**
- **`postReportData(filters, onData)`** (1646–1675): POST `get_report_data.php`,
  decode `{status,data,message}`, invoke `onData(data)` on success; shows
  `QMessageBox` on network error / non-object / non-success. Live — called by
  the PDF, Excel, and Print handlers.
- **`fetchPreviewData(filters)`** (2726–2750): POST `api.php/reports/data`
  (**different endpoint** — the API router), decode, call
  `updateChartsPreview(data)` on success; **silent** on error (only `qDebug`,
  no dialog). Live — called from the `connectFilterSignals` lambdas.
- **`loadFilterDepartments()`** (1818–1853): GET `get_departments.php`,
  populate `filterDepartmentBox` with `"-- Select Department --"` + departments;
  `QMessageBox::critical` on network error, `QMessageBox::warning` on
  non-success. Live — called from the ctor (cpp:462).
- **`loadAvailableYears()`** (1855–1888): GET `get_years.php`, populate
  `yearComboBox` and `semYearComboBox` with the years; dialogs on error/
  non-success. Live — called from the ctor (cpp:471).
- **`onFilterDepartmentBoxCurrentIndexChanged(int)`** (1890–1937): on index 0,
  reset `filterCourseBox` to `["All Courses"]`; else GET
  `get_courses.php?department=<d>&include_all=true`, repopulate
  `filterCourseBox`; dialogs on error/non-success. Live — connected at cpp:777.
- **`collectReportFilters(bool validate)`** (1701–1808): reads every report
  filter widget, validates (showing `QMessageBox::warning` when `validate`),
  and builds the `filters` `QJsonObject` including the duration→start/end date
  math. **Stays View-side** (reads widgets, shows dialogs); the pure date math
  is extracted (see below).
- **`collectReportFiltersForPreview()`** (1810–1814): thin wrapper —
  `collectReportFilters(false)`. **Stays View-side.**

**Palette:**
- **`getPalette(choice)`** (2478–2508): pure `QString → ReportPalette`. Used by
  the View (`updatePalettePreview`, `updateChartsPreview`) **and** the export
  path (`paintReport`). Extracted as a pure static.
- **`updatePalettePreview(choice)`** (2511–2530): paints a 200×50 `QPixmap`
  swatch into `ui->palettePreview`. **Stays View-side** (writes `ui->`).

**Live chart preview (QtCharts widget tree — stays View-side):**
- **`updateChartsPreview(data)`** (2532–2704): builds three `QChartView`
  widgets (bar/pie/line) into `ui->chartsPreviewBox`. Inherently View (creates
  and lays out widgets).
- **`optimizeChartForPreview(chart)`** (2706–2723): tweaks a live `QChart`'s
  margins/legend. **Stays View-side** (helper of `updateChartsPreview`).
- **`connectFilterSignals()`** (2752–…): wires department/course/duration/
  palette combo changes to preview refreshes. **Stays View-side.**

**Chart images + export (→ `ReportRenderer`):**
- **`makeBarChartImage / makePieChartImage / makeLineChartImage(data, size,
  palette)`** (123–187 / 191–241 / 246–…): render a `QChart` to a `QImage`.
  `makeLineChartImage` reads `QSettings` for library open/close hours.
- **`paintReport(device, resolution, data, filters)`** (2018–2291): the
  multi-page report painter (header, filters line, table, one chart per page,
  "Prepared by" page, footers). Reads `QSettings` for school + admin info.
- **`exportReportToExcel(rows, filters)`** (2356–2474): builds a `QXlsx`
  document (header, filters, table, footer) and saves it. The `QFileDialog` +
  `QMessageBox` wrap **stays View-side**; the document-writing body moves to
  `ReportRenderer::writeReportToXlsx`.
- **`exportReportToPDF(data, filters)`** (2293–2325): `QFileDialog` +
  `QPdfWriter` setup + `paintReport` + `QMessageBox`. **Stays View-side** as a
  thin wrapper calling `m_reportRenderer->paintReport(...)`.
- **`onPrintReportBtnClicked()`** (2327–2354): `collectReportFilters` +
  `postReportData` + `QPrintDialog` + `paintReport`. **Stays View-side** as the
  print wrapper.
- **`onGeneratePDFBtnClicked` / `onGenerateExcelBtnClicked`** (1677–1698):
  `collectReportFilters` + `postReportData` with a PDF/Excel callback.
  **Stays View-side**, rewritten to the dispatch seam below.

### Dead code deleted in this step ("extract live, delete dead")

Confirmed **zero callers** (grep over `qt-app/*.cpp`):

- **`fetchReportData(filters)`** (1939–2016) — DEAD. Its only unique behavior is
  the `ReportPreviewDialog` HTML popup, which nothing else uses. Deleted, not
  extracted. Its POST of `get_report_data.php` is preserved by the live
  `postReportData` → `ReportController::fetchReportRows`.
- **`renderChartToImage(chart, size)`** (96–118) — DEAD. Deleted.
- **`expandChartPlotArea(chart, size)`** (70–93) — DEAD. Deleted.
- The now-dead declarations in `adminwindow.h`: `renderChartToImage` (h:141),
  `expandChartPlotArea` (h:142), `fetchReportData` (h:203). Removed.
- **`#include "reportpreviewdialog.h"`** (cpp:55) — removed once
  `fetchReportData` is gone. **The `reportpreviewdialog.h`/`.cpp` class files
  and their `CMakeLists.txt` entry are KEPT** (present-but-unused), exactly as
  the Student step kept the `BusyIndicator` class after removing its last use.
  Deleting the orphaned class files is a documented **follow-up**, not part of
  this branch.

---

## Architecture

```
                         adminWindow  (View only)
                              │
   ┌──────────────┬──────────┼───────────────┬─────────────────────┐
   │              │          │               │                     │
 filter combos  collectReportFilters   live chart preview     export handlers
 (dept/course/  (reads widgets,        (updateChartsPreview,   (PDF/Excel/Print
  year/duration/ validates, dialogs)    optimizeChart…,         wrappers: dialogs,
  palette)       │                       connectFilterSignals)   device creation)
   │             │ durationType + inputs        ▲                     │
   │  QStringList│                              │ QJsonArray          │ QJsonArray data
   ▼             ▼                              │ preview data        ▼ + ReportHeaderInfo
        ReportController (QObject)              │            ReportRenderer (plain)
          ├── getPalette()            → pure    │              ├── aggregateVisitsByCourse() → pure
          ├── parseDepartments()      → pure    │              ├── aggregateVisitsByHour()   → pure
          ├── parseYears()            → pure    │              ├── makeBarChartImage()   → QImage
          ├── parseCourses()          → pure    │              ├── makePieChartImage()   → QImage
          ├── parseReportData()       → pure    │              ├── makeLineChartImage()  → QImage
          ├── parsePreviewData()      → pure    │              ├── paintReport()         → bool
          ├── computeDateRange()      → pure    │              └── writeReportToXlsx()   → bool
          ├── loadDepartments()  → GET get_departments.php
          ├── loadYears()        → GET get_years.php
          ├── loadCourses()      → GET get_courses.php
          ├── fetchReportRows()  → POST get_report_data.php  ──┘ (preview path)
          └── fetchPreviewData() → POST api.php/reports/data
                    │
                    ▼
        QNetworkAccessManager (injected, shared — adminWindow owns it)
```

`getPalette` lives on `ReportController` (Core-only, pure) and is called by both
the View and the `ReportRenderer`; `ReportRenderer` links `ReportController.cpp`
in its test target too, or `getPalette` is a free function in `reportdata.h`.
**Decision: `getPalette` is a static on `ReportController`** (keeps palette logic
with the other pure report logic); the Renderer receives an already-resolved
`ReportPalette` as a parameter, so it does **not** depend on `ReportController`.

---

## New Files

| File | Purpose |
|------|---------|
| `qt-app/reportdata.h` | Plain value types — `ReportPalette` (moved out of `adminwindow.h`), `ReportHeaderInfo`, `ReportDataOutcome` enum, `DateRange` |
| `qt-app/reportcontroller.h` | QObject controller — declaration |
| `qt-app/reportcontroller.cpp` | QObject controller — implementation |
| `qt-app/reportrenderer.h` | Plain renderer class — declaration |
| `qt-app/reportrenderer.cpp` | Plain renderer class — implementation |
| `qt-app/tests/tst_reportcontroller.cpp` | Qt Test target (Core+Network) |
| `qt-app/tests/tst_reportrenderer.cpp` | Qt Test target (Gui+offscreen) |

`adminwindow.h`, `adminwindow.cpp`, `qt-app/CMakeLists.txt`, and
`qt-app/tests/CMakeLists.txt` are modified (not replaced). All new headers use
`#ifndef` include guards (`REPORTDATA_H`, `REPORTCONTROLLER_H`,
`REPORTRENDERER_H`) — project convention. **Not** `#pragma once`.

---

## `reportdata.h` — Value Types

```cpp
// Moved verbatim out of adminwindow.h (currently adminwindow.h:54–62).
struct ReportPalette {
    QColor headerBg;
    QColor headerText;
    QColor rowEvenBg;
    QColor rowOddBg;
    QColor rowText;
    QVector<QColor> chartColors;
};

// Environment the View reads from QSettings and passes into the renderer,
// so ReportRenderer stays stateless (no QSettings, no ui->).
struct ReportHeaderInfo {
    QString schoolName;   // settings "school/name"     (default "Your School Name")
    QString address;      // settings "school/address"  (default "Your Address")
    QString logoPath;     // settings "school/logoPath"  (default "")
    QString librarian;    // settings "admin/name"       (default "")
    QString position;     // settings "admin/position"   (default "")
    int     openHour = 7;  // settings "library/openHour"  (default 7)
    int     closeHour = 21;// settings "library/closeHour" (default 21)
};

// Result of decoding a get_report_data.php POST response.
enum class ReportDataOutcome {
    Success,          // status == "success"; data array in outData
    NotSuccess,       // status != "success"; message in outMessage
    InvalidResponse   // body is not a JSON object
    // NetworkError is decided by the caller from reply->error(), not here.
};

// Pure output of the duration → date-range computation.
struct DateRange {
    QString start;   // "yyyy-MM-dd"
    QString end;     // "yyyy-MM-dd"
    bool    valid = false;
};
```

`ReportHeaderInfo`'s defaults reproduce the `QSettings` defaults at
`paintReport` (cpp:2048–2049, 2078–2080) and `makeLineChartImage`
(cpp:249–250). The View fills it once from `QSettings("MyCompany","MyApp")` and
passes it into every renderer call.

---

## `ReportController` — Public API

```cpp
class ReportController : public QObject
{
    Q_OBJECT
public:
    explicit ReportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static ReportPalette getPalette(const QString &choice);
    static QStringList  parseDepartments(const QByteArray &raw);   // get_departments.php
    static QStringList  parseYears(const QByteArray &raw);         // get_years.php
    static QStringList  parseCourses(const QByteArray &raw);       // get_courses.php
    static ReportDataOutcome parseReportData(const QByteArray &raw,
                                             QJsonArray &outData,
                                             QString &outMessage); // get_report_data.php
    static QJsonArray   parsePreviewData(const QByteArray &raw);   // api.php/reports/data

    // Pure duration → date-range math, extracted from collectReportFilters.
    // durationType: 0=day, 1=month, 2=semester, 3=custom (matches durationTypeBox).
    static DateRange computeDateRange(int durationType,
                                      const QDate &day,          // case 0
                                      int month, int monthYear,  // case 1 (month 1..12)
                                      const QString &semester, int semYear, // case 2
                                      const QDate &customStart,  // case 3
                                      const QDate &customEnd);

    // Async network methods — each emits a *Loaded / *Ready signal on success
    // and (where legacy shows a dialog) loadError on failure.
    void loadDepartments();                     // GET get_departments.php
    void loadYears();                           // GET get_years.php
    void loadCourses(const QString &department);// GET get_courses.php?department=..&include_all=true
    void fetchReportRows(const QJsonObject &filters); // POST get_report_data.php
    void fetchPreviewData(const QJsonObject &filters);// POST api.php/reports/data

signals:
    void departmentsLoaded(const QStringList &departments);
    void yearsLoaded(const QStringList &years);
    void coursesLoaded(const QStringList &courses);
    void reportDataReady(const QJsonArray &data);
    void reportError(const QString &message);        // network err OR !success OR invalid
    void previewDataReady(const QJsonArray &data);   // success only; silent on error
    void loadError(const QString &title, const QString &message); // combos' dialogs

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};
```

### Key design decisions

- **`QNetworkAccessManager` is injected, not owned** — same convention as the
  four prior controllers. The ctor takes `adminWindow`'s `networkManager`.
- **The combos DO show dialogs on failure** (unlike Student's silent combos).
  `loadFilterDepartments` (cpp:1826, 1850), `loadAvailableYears` (cpp:1863,
  1885), and `onFilterDepartmentBoxCurrentIndexChanged` (cpp:1911, 1934) all
  fire `QMessageBox`. So `loadDepartments`/`loadYears`/`loadCourses` emit
  `loadError(title, message)` on the failure paths, and the View shows the
  exact legacy dialog. This mirrors `VisitorController::fetchError`. On success
  they emit the parsed `QStringList`; the View clears the combo, adds the
  placeholder(s), then appends the list.
- **`fetchReportRows` folds all three failure branches into `reportError`.**
  Legacy `postReportData` shows `QMessageBox::critical(reply->errorString())`
  on network error (cpp:1656), `QMessageBox::warning("Invalid response…")` on
  non-object (cpp:1665), and `QMessageBox::warning(message)` on non-success
  (cpp:1670). The controller emits `reportError(msg)` with the matching text on
  each; the View shows the dialog. On success it emits `reportDataReady(data)`.
- **`fetchPreviewData` has no error signal.** Legacy only `qDebug`s and returns
  on error (cpp:2736) and never calls `updateChartsPreview` on failure. The
  controller emits `previewDataReady(data)` **only** on success; on any error
  it emits nothing (View leaves the preview unchanged), byte-identical.
- **`getPalette` is pure and static** — verbatim port of cpp:2478–2508
  (Blue/Green/Red/default palettes). No behavior change.
- **`computeDateRange` is pure** — verbatim port of the duration switch's date
  math (cpp:1732–1802), minus the widget reads and `QMessageBox`. Returns
  `valid=false` for an invalid day (case 0), an invalid/reversed custom range
  (case 3), or an out-of-range `durationType`; the View maps `!valid` to the
  same `QMessageBox::warning` + early `return {}` the legacy switch produced.
  Month/year and semester/year validity (the `< 0` index guards at cpp:1748,
  1766) stay View-side (they are combo-index checks, not date math); the View
  calls `computeDateRange` only after those pass.
- **reply lifetime safety:** every `connect(reply, &QNetworkReply::finished,
  this, …)` passes the controller (`this`) as the 3rd-arg context object;
  `reply->deleteLater()` on every path (success, network error, parse error) —
  same as `VisitorController`/`StudentController`.
- **Purity:** `reportcontroller.cpp` contains no `ui->`, no `QMessageBox`, no
  `QFileDialog`, no `QChartView`/`QChart`, no `QPainter`, no `QXlsx`, no
  `QSettings`.

---

## `ReportRenderer` — Public API (plain class, Gui/Charts/QXlsx)

```cpp
class ReportRenderer
{
public:
    // Pure aggregation helpers (no Gui) — unit-testable directly.
    static QMap<QString,int> aggregateVisitsByCourse(const QJsonArray &data);
    static QMap<int,int>     aggregateVisitsByHour(const QJsonArray &data,
                                                   int openHour, int closeHour);

    // Chart images. Verbatim ports of make{Bar,Pie,Line}ChartImage, with the
    // QSettings reads in makeLineChartImage replaced by the openHour/closeHour
    // parameters (taken from ReportHeaderInfo View-side).
    static QImage makeBarChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makePieChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makeLineChartImage(const QJsonArray &data, QSize size,
                                     const ReportPalette &palette,
                                     int openHour, int closeHour);

    // Multi-page report painter. Verbatim port of paintReport, with the
    // QSettings reads (school/admin) replaced by the ReportHeaderInfo param and
    // getPalette(...) resolved by the caller into `palette`.
    static bool paintReport(QPagedPaintDevice *device, int resolution,
                            const QJsonArray &data, const QJsonObject &filters,
                            const ReportPalette &palette,
                            const ReportHeaderInfo &info);

    // Excel document body. Verbatim port of exportReportToExcel's document
    // building (cpp:2369–2467), minus the QFileDialog/QMessageBox wrap and the
    // saveAs (the View calls xlsx.saveAs and reports success/failure).
    static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                  const QJsonArray &rows,
                                  const QJsonObject &filters,
                                  const ReportHeaderInfo &info);
};
```

### Key design decisions

- **Stateless & parameter-driven.** No `QSettings`, no `ui->`, no member state.
  Every environment value (school name/address/logo, librarian/position,
  library hours) arrives via `ReportHeaderInfo`; the palette arrives already
  resolved via `ReportPalette`. This makes each method a pure function of its
  inputs and removes the hidden global-state dependency in the legacy code.
- **The View resolves inputs before calling.** `onGeneratePDFBtnClicked` (and
  friends) build the `ReportHeaderInfo` from `QSettings("MyCompany","MyApp")`
  and the `ReportPalette` via `ReportController::getPalette(filters["palette"])`,
  then call the renderer.
- **`writeReportToXlsx` does not save.** It fills the passed `QXlsx::Document`
  and returns `true`; the View owns `QFileDialog`, `xlsx.saveAs`, and the
  success/failure `QMessageBox` (cpp:2364–2367, 2469–2473).
- **`paintReport` still calls the chart makers internally** (cpp:2247–2260) —
  those are static siblings on the same class, so no cross-unit dependency.
- **No behavior change to chart appearance or PDF/Excel layout.** Fonts, colors,
  column widths, page breaks, footer text, "Prepared by" page — all verbatim.

---

## The seams (critical)

### 1. Export-dispatch seam (replaces the `postReportData` callback)

Legacy `postReportData(filters, onData)` takes a callback that differs per
action: PDF export (cpp:1683), Excel export (cpp:1695), Print (cpp:2334). In
the split, `ReportController::fetchReportRows` emits `reportDataReady(data)`
with no callback. The View tracks which action requested the fetch:

```cpp
enum class ReportAction { None, Pdf, Excel, Print };
ReportAction m_pendingReportAction = ReportAction::None;   // View member
```

Each handler sets `m_pendingReportAction` immediately before calling
`fetchReportRows`, and the View's `onReportDataReady(data)` slot dispatches:

- `Pdf`   → `exportReportToPDF(data, m_pendingReportFilters)` (View wrapper:
  QFileDialog + QPdfWriter + `renderer->paintReport` + QMessageBox).
- `Excel` → `exportReportToExcel(data, m_pendingReportFilters)` (View wrapper:
  QFileDialog + `renderer->writeReportToXlsx` + `saveAs` + QMessageBox).
- `Print` → the print wrapper (QPrinter + QPrintDialog + `renderer->paintReport`).

The View also caches `m_pendingReportFilters` (the `filters` object) between the
`fetchReportRows` call and the `onReportDataReady` slot, since the async reply
no longer carries it in a capture. `onReportError(msg)` shows the dialog and
clears the pending action. This mirrors the `m_studentSearchShowOverlay` flag
seam from Step 4.

> **Empty-data guard stays View-side.** Legacy checks `data.isEmpty()` inside
> the PDF/Print/Excel callbacks (cpp:2300, 2335, 2358) and shows the
> `"No Data"` info box. The View keeps that check at the top of each dispatch
> branch, before opening any file dialog — byte-identical.

### 2. Preview-refresh seam

Legacy `connectFilterSignals` lambdas call `collectReportFiltersForPreview()`
then `fetchPreviewData(filters)` (or `updateChartsPreview(QJsonArray())` when a
guard fails). In the split, the lambda calls
`m_reportController->fetchPreviewData(filters)`; the controller emits
`previewDataReady(data)`; the View's `onPreviewDataReady(data)` slot calls
`updateChartsPreview(data)`. The guard branches that pass an **empty** array
(`updateChartsPreview(QJsonArray())`) stay View-side and call
`updateChartsPreview` directly — they never hit the network, exactly as today.

### 3. Combo-population seam

- `loadDepartments()` → `departmentsLoaded(list)`; View clears
  `filterDepartmentBox`, adds `"-- Select Department --"`, appends the list
  (cpp:1842–1848). `loadError` → View shows the legacy dialog.
- `loadYears()` → `yearsLoaded(list)`; View clears `yearComboBox` **and**
  `semYearComboBox`, appends the years to both (cpp:1876–1883).
- `loadCourses(dept)` → `coursesLoaded(list)`; View clears `filterCourseBox`,
  appends the list (cpp:1927–1932). The index-0 reset to `["All Courses"]`
  (cpp:1892–1895) stays View-side in the rewritten
  `onFilterDepartmentBoxCurrentIndexChanged` (it never hits the network).

---

## Message / Dialog Inventory

Every user-visible dialog in the **live** Reports flow, exact text, and owner
after extraction. All are `QMessageBox` unless noted.

| # | Trigger | Exact text | Kind | Owner |
|---|---|---|---|---|
| 1 | Report data: network error | `<reply->errorString()>` (title "Error") | critical | View, from `reportError` |
| 2 | Report data: body not object | `Invalid response from server.` | warning | View, from `reportError` |
| 3 | Report data: status != success | `<message>` (title "Error") | warning | View, from `reportError` |
| 4 | Departments: network error | `<reply->errorString()>` | critical | View, from `loadError` |
| 5 | Departments: status != success | `Failed to load departments.` | warning | View, from `loadError` |
| 6 | Years: network error | `<reply->errorString()>` | critical | View, from `loadError` |
| 7 | Years: status != success | `<obj["message"]>` | warning | View, from `loadError` |
| 8 | Courses: network error | `<reply->errorString()>` | critical | View, from `loadError` |
| 9 | Courses: body not object | `Invalid response from server.` | warning | View, from `loadError` |
| 10 | Courses: status != success | `<obj["message"]>` | warning | View, from `loadError` |
| 11 | collectReportFilters: no department | `Please select a department.` (title "Invalid Input") | warning | View (`collectReportFilters`) |
| 12 | collectReportFilters: no courses | `No courses available for the selected department.` | warning | View |
| 13 | collectReportFilters: invalid date (day) | `Please select a valid date.` | warning | View (maps `computeDateRange` `!valid`) |
| 14 | collectReportFilters: no month/year | `Please select a month and year.` | warning | View |
| 15 | collectReportFilters: no semester/year | `Please select a semester and year.` | warning | View |
| 16 | collectReportFilters: invalid custom range | `Please select a valid date range.` | warning | View (maps `computeDateRange` `!valid`) |
| 17 | collectReportFilters: no duration type | `Please select a duration type.` | warning | View |
| 18 | PDF/Print/Excel: empty data | `No data available for the selected filters. …` | information | View (dispatch guard) |
| 19 | PDF: no file selected | `No file selected.` (title "Export Canceled") | information | View |
| 20 | PDF: paint failed | `Failed to open PDF for writing.` | critical | View |
| 21 | PDF: success | `Report exported to PDF successfully.` | information | View |
| 22 | Print: paint failed | `Failed to start painting to printer.` | critical | View |
| 23 | Excel: empty data | `No data available for the selected filters.` | information | View |
| 24 | Excel: save failed | `Failed to save Excel file.` | critical | View |
| 25 | Excel: success | `Report exported to Excel successfully.` | information | View |

Rows 18 and 23 differ in trailing text — row 18 (PDF/Print) includes
`"Please check your date range and department selection."`, row 23 (Excel) does
not. Preserved verbatim. The `ReportPreviewDialog` HTML popup (dead
`fetchReportData`) is **deleted**, not in the inventory.

---

## Data Flow

### Generate PDF / Excel / Print
```
onGeneratePDFBtnClicked()  [or Excel / Print]
  → filters = collectReportFilters(true)         [View: reads widgets, validates,
        calls ReportController::computeDateRange for the date math]
  → if (filters.isEmpty()) return
  → emit reportFiltersReady(filters)             [existing signal, unchanged]
  → m_pendingReportAction = Pdf | Excel | Print
  → m_pendingReportFilters = filters
  → m_reportController->fetchReportRows(filters)
        → POST get_report_data.php
        → [network err / !object / !success]  emit reportError(msg)
        → [success]                           emit reportDataReady(data)
  → View onReportError:   QMessageBox (rows 1–3); m_pendingReportAction = None
  → View onReportDataReady(data): dispatch on m_pendingReportAction:
        Pdf   → if data empty → row 18; else QFileDialog + QPdfWriter
                + info = collectHeaderInfo(); palette = getPalette(filters["palette"])
                + renderer->paintReport(&pdf, 150, data, filters, palette, info)
                + rows 19/20/21
        Excel → if data empty → row 23; else QFileDialog + QXlsx::Document
                + renderer->writeReportToXlsx(xlsx, data, filters, info)
                + xlsx.saveAs + rows 24/25
        Print → if data empty → row 18; else QPrinter + QPrintDialog
                + renderer->paintReport(&printer, res, data, filters, palette, info)
                + row 22 on failure
```

### Live chart preview (combo change)
```
filter combo changed (dept/course/duration)
  → [guard fails] updateChartsPreview(QJsonArray())     [View, no network]
  → [guard passes] filters = collectReportFiltersForPreview()  [validate=false]
  → [filters empty] updateChartsPreview(QJsonArray())
  → [else] m_reportController->fetchPreviewData(filters)
        → POST api.php/reports/data
        → [error]   emit nothing (silent)
        → [success] emit previewDataReady(data)
  → View onPreviewDataReady(data): updateChartsPreview(data)

palette combo changed
  → updatePalettePreview(text)  [View]  +  (if guards pass) fetchPreviewData(...)
```

### Startup + department change (combo population)
```
ctor → m_reportController->loadDepartments()   → departmentsLoaded / loadError
ctor → m_reportController->loadYears()          → yearsLoaded / loadError
onFilterDepartmentBoxCurrentIndexChanged(index)
  → [index <= 0] filterCourseBox = ["All Courses"]; return   [View, no network]
  → m_reportController->loadCourses(currentText())  → coursesLoaded / loadError
```

---

## Tests

### `tst_reportcontroller` (Core+Network, **no** offscreen — 9th target)

No live network — synthetic `QByteArray` payloads with synthetic data only
(e.g. `"BSIT"`, `"College of Computing Studies"`, visit counts, `2023`), per
the security-hygiene rule. Coverage:

- **`getPalette`**: `"Blue"`/`"Green"`/`"Red"` each return their headerBg + the
  right `chartColors.size()`; an unknown string returns the default palette
  (10 chart colors).
- **`parseDepartments`**: `status:"success"` with a `departments` array → the
  full list; `status != "success"` → empty list.
- **`parseYears`**: `status:"success"` with a `years` int array → the years as
  strings; `status != "success"` → empty.
- **`parseCourses`**: `status:"success"` with a `courses` array → the list;
  `status != "success"` → empty.
- **`parseReportData`**: success with a `data` array → `Success` + `outData`
  filled; `status != "success"` with a `message` → `NotSuccess` + `outMessage`;
  non-object body → `InvalidResponse`.
- **`parsePreviewData`**: success with `data` → the array; error/non-success →
  empty array.
- **`computeDateRange`**: day (valid + invalid QDate), month (year+month →
  first/last day), semester "1st"/"2nd" (Jan–Jun / Jul–Dec of the year),
  custom (valid range + reversed range → `!valid`), and an out-of-range
  durationType → `!valid`. Assert `start`/`end` strings against the legacy
  formulas (cpp:1740–1795).

### `tst_reportrenderer` (Gui+Charts+QXlsx, offscreen — 10th target)

Links `Qt::Gui`/`Qt::Charts`/`QXlsx` (like `tst_visitorcontroller`), so
`QT_QPA_PLATFORM=offscreen`. Coverage:

- **`aggregateVisitsByCourse`**: an array with repeated courses → summed visit
  counts per course.
- **`aggregateVisitsByHour`**: an array with `login_time` values → per-hour
  counts, with out-of-`[openHour,closeHour]` rows excluded and invalid
  `login_time` skipped (cpp:260–274).
- **`make{Bar,Pie,Line}ChartImage`**: return a non-null `QImage` of the
  requested size for a small synthetic dataset (integration-level assertion —
  the same convention as the QXlsx-linked targets; pixel content is not
  asserted).

**Accepted limitation (matches Import/Visitor/Student precedent):** the network
methods' emit routing and `reply->deleteLater()` are verified by the Task-4
purity/grep gates + manual QA, not `QSignalSpy`. `paintReport` /
`writeReportToXlsx` are exercised by manual PDF/Excel QA against the live
backend (they need a real `QPagedPaintDevice`/file), not automated tests.

---

## Files to Modify in CMake

### `qt-app/CMakeLists.txt`

Add to the `qt_add_executable(WITS …)` source list, next to the student files:

```cmake
reportdata.h
reportcontroller.h reportcontroller.cpp
reportrenderer.h reportrenderer.cpp
```

No new Qt modules for `WITS` — `Qt::Network`, `Qt::Charts`, `Qt::Gui`, `QXlsx`
are already linked.

### `qt-app/tests/CMakeLists.txt`

Register two new targets (9th + 10th):

```cmake
# 9th — Core+Network only, NO offscreen (pure surface, like tst_studentcontroller)
qt_add_executable(tst_reportcontroller
    tst_reportcontroller.cpp
    ${CMAKE_SOURCE_DIR}/reportcontroller.cpp
    ${CMAKE_SOURCE_DIR}/reportcontroller.h
    ${CMAKE_SOURCE_DIR}/reportdata.h
)
set_target_properties(tst_reportcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_reportcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_reportcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
)
add_test(NAME tst_reportcontroller COMMAND tst_reportcontroller)
set_tests_properties(tst_reportcontroller PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

# 10th — Gui+Charts+QXlsx, offscreen (like tst_visitorcontroller)
qt_add_executable(tst_reportrenderer
    tst_reportrenderer.cpp
    ${CMAKE_SOURCE_DIR}/reportrenderer.cpp
    ${CMAKE_SOURCE_DIR}/reportrenderer.h
    ${CMAKE_SOURCE_DIR}/reportcontroller.cpp
    ${CMAKE_SOURCE_DIR}/reportcontroller.h
    ${CMAKE_SOURCE_DIR}/reportdata.h
)
set_target_properties(tst_reportrenderer PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_reportrenderer PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_reportrenderer PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Charts
    QXlsx
)
add_test(NAME tst_reportrenderer COMMAND tst_reportrenderer)
set_tests_properties(tst_reportrenderer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

`tst_reportrenderer` links `reportcontroller.cpp` only if the renderer test
references `getPalette`; if it constructs `ReportPalette` literals directly it
can drop `reportcontroller.*`. Final call made in Task 3.

**Why no offscreen for `tst_reportcontroller`:** its tested surface is
Core+Network-only (`QString`/`QByteArray`/`QJsonDocument`/`QDate`/`QStringList`)
— same rationale as `tst_studentcontroller`. `QTEST_MAIN` on a Core+Network
target uses `QCoreApplication`, which needs no platform plugin.

---

## Task Decomposition

Four tasks (the prior 3-task shape + one for the second unit):

- **Task 1** — `reportdata.h` + `reportcontroller.h`/`.cpp` skeleton: value
  types (`ReportPalette` moved here, `ReportHeaderInfo`, `ReportDataOutcome`,
  `DateRange`) and **all** pure statics (`getPalette`, `parseDepartments`,
  `parseYears`, `parseCourses`, `parseReportData`, `parsePreviewData`,
  `computeDateRange`) implemented for real; the five network methods stubbed;
  CMake wiring for `WITS` + `tst_reportcontroller`; `tst_reportcontroller`
  red → green on the statics.
- **Task 2** — implement the five `ReportController` network methods with signal
  emissions and reply-lifetime handling (`this` as `finished` context object,
  `reply->deleteLater()` on every path).
- **Task 3** — `reportrenderer.h`/`.cpp` + `tst_reportrenderer`: the two
  aggregation statics, the three chart-image makers (line chart's `QSettings`
  reads replaced by `openHour`/`closeHour` params), `paintReport` (school/admin
  `QSettings` reads replaced by `ReportHeaderInfo`), and `writeReportToXlsx`
  (Excel body, no `saveAs`/dialogs); CMake wiring + red → green on the
  aggregation statics and non-null chart images.
- **Task 4** — wire both units into `adminWindow`: add `m_reportController`
  (child, constructed after `networkManager` alongside the other controllers)
  and `m_reportRenderer`; add the View slots (`onDepartmentsLoaded`,
  `onYearsLoaded`, `onCoursesLoaded`, `onReportDataReady`, `onReportError`,
  `onPreviewDataReady`, `onLoadError`); add `m_pendingReportAction` /
  `m_pendingReportFilters` members and a `collectHeaderInfo()` View helper;
  rewrite `collectReportFilters` to call `computeDateRange`, and rewrite
  `onGeneratePDFBtnClicked`/`onGenerateExcelBtnClicked`/`onPrintReportBtnClicked`/
  `exportReportToPDF`/`exportReportToExcel`/`loadFilterDepartments`/
  `loadAvailableYears`/`onFilterDepartmentBoxCurrentIndexChanged`/the
  `connectFilterSignals` preview lambdas to delegate. Move `ReportPalette` out
  of `adminwindow.h` (include `reportdata.h`). **Delete** the dead code
  (`fetchReportData`, `renderChartToImage`, `expandChartPlotArea`, their `.h`
  decls, and the `reportpreviewdialog.h` include). Finish with a purity grep
  gate (`reportcontroller.cpp` has no `ui->`/`QMessageBox`/`QPainter`/`QXlsx`;
  `reportrenderer.cpp` has no `ui->`/`QMessageBox`/`QFileDialog`/`QSettings`)
  and a no-dead-code grep gate (the three deleted functions are gone).

**Branch:** `feat/report-controller-extraction` (off `master`).

---

## Out of Scope (This Step)

- Any change to `get_report_data.php`, `get_departments.php`, `get_years.php`,
  `get_courses.php`, or `api.php/reports/data` or their wire protocols —
  payloads unchanged.
- Consolidating the two report-data endpoints (`get_report_data.php` for
  exports vs `api.php/reports/data` for preview) — both preserved as-is; the
  divergence is pre-existing and out of scope.
- Deleting the orphaned `reportpreviewdialog.h`/`.cpp` class files (kept
  present-but-unused after `fetchReportData` is deleted, exactly as the Student
  step kept `BusyIndicator`) — a documented follow-up.
- The live-preview QtCharts widget tree (`updateChartsPreview`,
  `optimizeChartForPreview`) — inherently View (builds `QChartView` widgets),
  stays in `adminWindow`.
- UI layout changes to the Reports tab, and any change to `adminwindow.ui` (it
  carries the user's own uncommitted edit; NEVER stage/modify it).
- Backend URL configurability (`ApiConfig` used as-is).
- The other `get_departments.php` callers from prior tabs
  (`adminWindow::loadDepartments()` at cpp:1629, `StudentController`) — untouched.

---

## Verification Criteria

Complete when:

1. The app builds with no new warnings/errors, and
   `ctest --test-dir qt-app/build --output-on-failure` is green — all 8 existing
   targets plus `tst_reportcontroller` + `tst_reportrenderer` (**10 total**).
2. The Reports tab behaves identically:
   - Startup populates the department combo (`"-- Select Department --"` +
     departments) and both year combos; failures show the same dialogs
     (rows 4–7).
   - Selecting a department loads its courses (index 0 resets to
     `["All Courses"]`); failures show rows 8–10.
   - Generate PDF / Excel / Print validate filters identically (rows 11–17),
     show the empty-data box (rows 18/23), open the same file/print dialogs, and
     produce byte-identical PDF/Excel output (header, filters line, table,
     one chart per page, "Prepared by" page, footers) and success/failure
     dialogs (rows 19–25).
   - The live chart preview updates on combo changes exactly as before
     (bar/pie/line, real vs placeholder data), and is silent on preview-fetch
     errors.
   - The palette preview swatch and chart colors match the selected palette.
3. `adminWindow` contains no `QNetworkAccessManager` POST/GET and no report
   JSON parsing (the network halves of `postReportData`, `fetchPreviewData`,
   `loadFilterDepartments`, `loadAvailableYears`,
   `onFilterDepartmentBoxCurrentIndexChanged` delegate to `ReportController`),
   and no chart-image/`paintReport`/`QXlsx` bodies (they delegate to
   `ReportRenderer`).
4. `ReportController` has no `ui->`/`QMessageBox`/`QPainter`/`QXlsx`/`QSettings`;
   `ReportRenderer` has no `ui->`/`QMessageBox`/`QFileDialog`/`QSettings`.
5. `fetchReportData`, `renderChartToImage`, `expandChartPlotArea` and their `.h`
   decls are gone; the `reportpreviewdialog.h` include is gone;
   `adminwindow.ui` is untouched.

---

## Global Constraints

- Qt 6.11, C++17, CMake+Ninja+MinGW. `#ifndef` header guards (NOT
  `#pragma once`). `m_` prefix for new members. Functor-based `connect` (no
  `SIGNAL`/`SLOT` macros). `QNetworkAccessManager` injected, not owned;
  `reply->deleteLater()` on every path; `finished` connected with the controller
  (`this`) as context object. `ReportRenderer` is stateless — no `QSettings`,
  no `ui->`, no member state; environment via `ReportHeaderInfo`, palette via
  `ReportPalette`. `tst_reportcontroller` links only `Qt::Test` + `Qt::Network`
  (no offscreen); `tst_reportrenderer` links `Qt::Gui`/`Qt::Charts`/`QXlsx`
  with `QT_QPA_PLATFORM=offscreen`.
- Security-hygiene (binding): no secrets/admin keys/credentials/backend URLs in
  source, tests, CMake, or this spec; use placeholders. No real student PII in
  fixtures — synthetic only. No hardcoded `C:\Users\<name>` personal paths in
  committed files. NEVER stage/commit `qt-app/adminwindow.ui` (carries the
  user's own uncommitted edit) or `qt-app/build/`.

---

## Notes / discrepancies (code vs. design)

1. **Two report-data endpoints, by design.** Exports POST
   `get_report_data.php` (`postReportData`); the live preview POSTs
   `api.php/reports/data` (`fetchPreviewData`). This divergence pre-dates this
   branch and is preserved exactly — `fetchReportRows` uses the former,
   `fetchPreviewData` the latter. Not consolidated (out of scope).
2. **`filterDepartmentBox::currentIndexChanged` has two live handlers** — the
   course-loader (`onFilterDepartmentBoxCurrentIndexChanged`, connected
   cpp:777) and a preview-refresh lambda in `connectFilterSignals` (cpp:2755).
   They do **different** things (load courses vs refresh preview) and both stay
   — this is **not** a duplicate to collapse (unlike the Student course-fetch).
   Noted so the implementer does not "unify" them.
3. **`getPalette` default palette has 10 chart colors; Blue/Green/Red have 8.**
   Verbatim in the port; `tst_reportcontroller` asserts the counts so a future
   edit can't silently change them.
4. **`makeLineChartImage` reads `library/openHour`/`closeHour` from QSettings**
   (cpp:248–250) with defaults 7/21; the renderer takes these as parameters via
   `ReportHeaderInfo`. The View reads the same keys/defaults, so behavior is
   identical.
