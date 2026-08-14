# LOAMS 2.0 Phase 4b-i — Reporting: Filters + Native Preview (Design Spec)

**Date:** 2026-08-14
**Track:** Phase 4b (Reporting), slice **i of ii**
**Predecessor:** Track 4a (Database + Import) — merged (PR #39), deployed, smoke-tested.
**Parent contract:** `docs/superpowers/specs/2026-07-19-loams2-phase4-admin-part2-design.md` §4.3.

## 1. Summary

Replace the "Reporting — coming soon" placeholder (`qt-app/quick/qml/admin/ReportingScreen.qml`)
with a working reporting screen: a **filter panel** (Department → Course cascade + a
day/month/semester/custom **duration selector**) and an on-screen **native-QML preview** of the
result (a per-student **table**, a ranked **visits-by-course bar chart**, and a row of **summary
stat tiles**). An explicit **Generate Report** button runs the query.

This is a **presentation-layer rebuild** over the already-built, unit-tested witscore
`ReportController` — **no new business logic** and **no backend change**. It is the first of two
4b slices; the second (4b-ii) adds palette/chart-type choice and the off-thread PDF/Excel/print
**export** via the Widgets-linked `ReportRenderer`.

## 2. Scope

### 2.1 In scope (4b-i)

- `ReportingViewModel` (new, the only QML-facing C++) wrapping the existing `ReportController`.
- `ReportRowsModel` (new `QAbstractListModel`) feeding the preview table.
- `LDatePicker.qml` (new themed calendar-grid component) for Day and Custom date input.
- `ReportingScreen.qml` rebuilt: filter panel + preview.
- Reuse of existing components: `LCascadingSelect`, `LComboBox`, `LBarChart`, `LTable`,
  `LStatTile`, `LButton`, `LCard`.
- Wiring in `AdminScreen.qml` (add the VM instance + `vm:` binding).
- Full TDD coverage (QtTest + QuickTest under ctest).

### 2.2 Out of scope — deferred to 4b-ii

- **Palette choice** (Blue/Green/Red/Default via `ReportController::getPalette`) — only affects the
  rendered export, not the Theme-tokened on-screen preview.
- **Chart-type choice** (bar/pie/line) — only parametrizes the exported render.
- **PDF / Excel / print export** via `ReportRenderer` and its mandated **off-GUI-thread** worker.
- `ReportController::fetchPreviewData` stays **unused** (see §7).
- **Pie chart** on screen — deliberately dropped (see §3, research finding).
- **Time-trend / by-hour line chart** — not backable by the current endpoint without new backend
  work; explicitly not attempted here (see §3 and §9).

## 3. Research basis (why this preview shape)

Academic-library assessment practice (ACRL surveys, Springshare **LibInsight**, library dashboard
toolkits, patron-counting vendors) standardizes visit/attendance reporting on three jobs:

1. **Trend over time (line)** — by hour/day/month, for peak-time identification. *The* headline
   library-usage view.
2. **Category comparison (bar, ranked)** — usage by group/department/service.
3. **The detail table** — underlying rows for drill-down and export.

**Pie charts are conspicuously absent** from modern library-analytics tooling; the field
standardizes on **line + bar + tables**. The legacy renderer's pie option is demoted.

**The endpoint constraint.** `get_report_data.php` returns **per-student rows with a single total
`visits` count each** (`school_id, name, gender, status, course, department, year_level, visits`),
scoped to a date range, ordered by `visits DESC`. It returns **no per-hour/per-day timeseries**.
Therefore the industry-favorite **line/time-trend cannot be drawn on screen** without a new backend
aggregation endpoint (which would break 4b's "no backend" promise). What the data *does* cleanly
support — and what this slice renders — is the **ranked bar (visits by course)** + the **detail
table** + **summary stat tiles**. (Note: the legacy renderer's `aggregateVisitsByCourseHour` line
chart aggregates these same hour-less rows, so the legacy "by hour" chart is almost certainly
plotting nothing meaningful; 4b-ii will confront that on the export path.)

## 4. Architecture

### 4.1 MVVM

`ReportingScreen.qml` receives a `ReportingViewModel` as `property var vm`, injected by
`AdminScreen`'s `Loader` — identical to every other admin screen. `ReportingViewModel` is the
**only** new QML-facing C++. QML never calls `ReportController` directly (project MVVM rule).

`ReportingViewModel` **owns**:
- a `QNetworkAccessManager` (parented, owned — like `DashboardViewModel`),
- a `ReportController` (the tested witscore class, constructed with that QNAM),
connects to `ReportController`'s signals, and re-exposes state as QML properties/models.

### 4.2 New C++ units

**`ReportingViewModel`** (`qt-app/quick/viewmodels/`, `QML_ELEMENT`):
- **Filter state:** `department`, `course` (both `QString`, read/write via `Q_INVOKABLE`
  setters or `Q_PROPERTY`), plus the department/course **list** models exposed for
  `LCascadingSelect` (`QStringList departments`, `QStringList courses`).
- **Duration state:** `durationType` (0=Day,1=Month,2=Semester,3=Custom — matching
  `ReportController::computeDateRange`), `day` (Day), `month`+`monthYear` (Month),
  `semester`+`semYear` (Semester), `customStart`+`customEnd` (Custom); plus `QStringList years`
  (from `get_years.php`) for the year combos.
- **Derived:** `bool canGenerate` — true iff `department != ""` **and** the active duration mode is
  fully/ validly specified (see §5.3). Course is **not** required.
- **Result state:** `ReportRowsModel *rowsModel` (table), `BarsModel *courseBarsModel`
  (visits-by-course), and stat-tile scalars (`int totalVisits`, `int studentsShown`,
  `QString topCourse`) — all `NOTIFY`ed.
- **Status:** `bool loading`, `QString errorText` (Dashboard pattern). **Concurrency —
  single-in-flight, NOT the Dashboard request-seq guard.** `DashboardViewModel`'s
  `nextRequestSeq`/`isCurrentRequest` works because that VM issues its own `QNetworkReply` and
  captures `seq` in the reply lambda; here the VM delegates to `ReportController::fetchReportRows()`
  and receives results via the **token-less** `reportDataReady(QJsonArray)` signal — it cannot tell
  a superseded emission from the current one. Since "no witscore change" and "use `fetchReportRows`"
  are locked, the resolution is to **allow only one report fetch at a time**: `generateReport()`
  is a no-op while `loading`, and `canGenerate` is false while `loading` (Generate disabled). This
  makes the out-of-order question moot without a request token. The same single-flight discipline
  covers the bootstrap fetches; the rapid `setDepartment`→`loadCourses`/`coursesLoaded` path is
  last-write-wins (a stale course list is harmless — the cascade self-clears an invalid course).
- **Invokables:**
  - `loadDepartments()` — bootstrap entry called by `AdminScreen`'s existing `Loader.onLoaded`
    gate (§4.4). Loads **both** departments and years (`ReportController::loadDepartments()` +
    `loadYears()`); issues **no** report fetch.
  - `setDepartment(QString)` — sets department, dependent-clears course, triggers
    `ReportController::loadCourses(dept)`.
  - `setCourse(QString)`, and the duration setters.
  - `generateReport()` — the **only** report fetch: builds the filters `QJsonObject` (§5.2) and
    calls `ReportController::fetchReportRows()`.
  - `retry()` — re-runs the last `generateReport()`.
- **Slots** on `ReportController` signals: `departmentsLoaded`, `yearsLoaded`, `coursesLoaded`
  → update list models; `reportDataReady(QJsonArray)` → populate rows/bars/tiles + clear loading;
  `reportError`/`loadError` → set `errorText` + clear loading (guarded by request-seq).
- **Pure, unit-testable statics** (no network, no QML):
  - `buildFilters(...)` → `QJsonObject` — maps the filter+duration state to the exact request body
    `get_report_data.php` expects (§5.2). This is the primary new test seam.
  - `aggregateVisitsByCourse(QJsonArray)` → `QList<BarsModel::Bar>` — rows → ranked by-course bars.
  - `deriveTiles(QJsonArray)` → `{totalVisits, studentsShown, topCourse}` — stat-tile values.

**`ReportRowsModel`** (`qt-app/quick/models/`, `QAbstractListModel`): roles
`name / course / year / visits` (mirrors `StudentsTableModel`/`BarsModel` precedent — roles enum,
`roleNames()`, typed row struct, `setRows(QJsonArray)`). Feeds `LTable`.

### 4.3 New QML component

**`LDatePicker.qml`** (`qt-app/quick/qml/components/`): a themed calendar-grid popup.
- A field/button showing the selected date (`yyyy-MM-dd`, or a placeholder when unset) that opens a
  popup **month grid** (7-column weekday layout) with **prev/next month** navigation and
  **day selection**; emits the chosen `QDate`/date-string.
- **Theme tokens only** (zero raw hex; opacity variants via `Qt.alpha(Theme.<token>, a)`).
- `property date selectedDate` / `signal picked(...)`, an `objectName` on the field and on day
  cells for QuickTest introspection.
- Reusable (a general LOAMS date primitive), not report-specific.

### 4.4 Screen & shell wiring

`ReportingScreen.qml` layout (top→bottom, inside the page body):
1. **Filter card** (`LCard`): `LCascadingSelect` (Dept→Course) on one row; the **duration
   selector** (`LComboBox` Day/Month/Semester/Custom) + its mode-specific sub-controls on the next;
   the **Generate Report** `LButton` (disabled unless `vm.canGenerate`).
2. **Preview area**: a row of `LStatTile`s (total visits / students shown / top course); the
   **visits-by-course** `LBarChart` (horizontal); the per-student `LTable`. Dimmed while
   `vm.loading`; inline **error + Retry** block on error; `LTable` empty-state when the result is
   empty.

`AdminScreen.qml` changes (minimal): add `ReportingViewModel { id: reportingVm }` alongside the
other VM instances, and bind it on the reporting component:
`ReportingScreen { objectName: "reportingPage"; vm: reportingVm }`. The existing `Loader.onLoaded`
gate already calls `item.vm.loadDepartments()` when present (`AdminScreen.qml:185`), so naming the
bootstrap entry `loadDepartments()` wires departments+years on navigation with **no** other shell
change. No `Component.onCompleted` fetch on the screen (keeps stub-driven QuickTests offline).

## 5. Data flow

### 5.1 Bootstrap (on navigation)

`Loader.onLoaded` → `reportingVm.loadDepartments()` → `ReportController::loadDepartments()` +
`loadYears()` → `departmentsLoaded`/`yearsLoaded` populate the Dept combo and the year combos. No
report is fetched. Selecting a department → `setDepartment()` → `loadCourses(dept)` →
`coursesLoaded` repopulates the Course combo ("All Courses" included via `include_all=true`).

### 5.2 Generate

`generateReport()` calls `buildFilters(...)` → a `QJsonObject` posted by
`ReportController::fetchReportRows()` to `get_report_data.php`. The object carries exactly the keys
the endpoint reads (`get_report_data.php:18-24`): `department` (required), `course`
(omitted/`""`/`"All Courses"` = dept-wide), `durationType`, `start`, `end`, `year`, `semester`.

**`durationType` is a STRING, not the VM's int.** `get_report_data.php:20,57-64` reads `durationType`
as the strings `"day"`/`"month"`/`"semester"`/`"custom"`. The VM stores an int (0..3, matching
`computeDateRange`); `buildFilters` **must translate int → the exact string token**. Emitting the
int (or `"0"`) makes the server silently apply **no** date filter and return all-time visits with no
error — so `buildFilters`'s test asserts the literal string per mode.

**`semester` must carry a server-recognized token.** The server only ranges when `semester` contains
`1`/`first`, `2`/`second`, or `summer` (`get_report_data.php:66-81`); anything else falls through to
all-time. `buildFilters` emits a `semester` string carrying one of those tokens (the test asserts the
literal value).

- **Day / Month / Custom:** the client computes `start`/`end` via
  `ReportController::computeDateRange(...)` and sends them; the server uses `BETWEEN start AND end`.
- **Semester:** send `durationType="semester"`, `year`, `semester`; the server computes its own
  range (see §9 — the open convention item).

On `reportDataReady(data)` (request-seq current): `rowsModel.setRows(data)`;
`courseBarsModel.setBars(aggregateVisitsByCourse(data))`; tiles ← `deriveTiles(data)`;
`loading=false`. On `reportError`/`loadError`: `errorText` set, `loading=false`. Empty `data` is a
**success** with zeroed tiles + empty table/chart (not an error).

### 5.3 `canGenerate` gate

`department != ""` **and not `loading`** **and**:
- Day → `day` valid; Month → `month`∈1..12 and `monthYear`>0; Semester → `semester` set and
  `semYear`>0; Custom → `customStart` and `customEnd` valid and `customStart <= customEnd`.

Course is optional throughout. The `not loading` term enforces single-in-flight (§4.2): Generate is
disabled while a report fetch is outstanding.

**Year-combo data note (pre-existing, not a bug):** `get_years.php` returns
`DISTINCT YEAR(login_time)` from `library_visits` (fallback: current year only). A year with **zero**
logged visits is therefore not selectable in the Month/Semester year combos. This is existing
endpoint behavior, out of scope for 4b-i — noted so QA doesn't mistake it for a defect.

## 6. Testing & gates (TDD, ctest)

Registered via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`), `OFFSCREEN` for GUI/Quick.
All existing ctest targets are the **regression floor** and must stay green.

### 6.1 C++ unit (QtTest, synthetic payloads, NO live network)

New target **`tst_reportingviewmodel`**:
- `buildFilters(...)` — correct request JSON for each duration mode (incl. computed start/end for
  Day/Month/Custom; the semester component-passing per §9); Course omitted when empty/"All".
- `aggregateVisitsByCourse(...)` — rows → ranked by-course bars (sum per course, order).
- `deriveTiles(...)` — total visits, students shown, top course; empty-data → zeros/"—".
- `canGenerate` truth table across modes + missing/invalid inputs, **including `loading` → false**.
- Single-in-flight: `generateReport()` while `loading` is a no-op (no second `fetchReportRows`);
  `loading` flips true on fetch, false on `reportDataReady`/`reportError`; `canGenerate` is false
  throughout the in-flight window.
- `durationType` int→string token per mode, and the `semester` literal token (§5.2) — asserted in
  the `buildFilters` cases (silent-all-time is the failure mode being guarded).

`ReportRowsModel` role coverage (add to an existing models test target or a small new one).
`ReportController::computeDateRange` is already covered by `tst_reportcontroller` — untouched.

### 6.2 QuickTest (offscreen, plain-QML stub `vm`)

- `ReportingScreen`: filter enable/disable, duration-mode control **swapping**, Generate gating on
  `canGenerate`, preview render on a stub result, loading dim, error+Retry, empty-state. Added to
  `tst_qml_admin.qml` with a new fixture y-band (per the house pattern — own band + raised root
  height; see the QuickTest fixture convention).
- `LDatePicker`: open popup, month nav, day selection emits the right date, closed state shows the
  selection. Added to `tst_qml_components.qml`.
- Any server-echoed text (`errorText`) rendered with `textFormat: Text.PlainText` (the standing
  injection-over-cleartext-HTTP guard from 4a).

## 7. The redundant preview endpoint (finding)

`api.php` routes `reports/data` to `require_once 'get_report_data.php'` (`api.php:95-100`) — so
`ReportController::fetchPreviewData` (POST `api.php/reports/data`) and `fetchReportRows` (POST
`get_report_data.php`) hit the **same file and return identical data**. The parent spec's claim that
they "serve different jobs, both stay, not duplication" is factually incorrect. 4b-i uses
**`fetchReportRows`** only; `fetchPreviewData` stays unused. This also means 4b-ii's export reuses
the **same fetched rows** as the preview (fetch once → preview + export), rather than a second
round-trip. Removing the dead `fetchPreviewData`/`parsePreviewData` path is a possible later
cleanup, **not** part of this slice (it is shipped, tested witscore surface).

## 8. Security & constraints

- **Read-only endpoints**, no `admin_key`, no destructive ops — the reporting path carries none of
  4a's auth surface. (Transport is still cleartext HTTP — a Phase 6 concern, unchanged here.)
- **Zero raw hex** outside `Theme.qml`; `LBarChart`/tiles/`LDatePicker` use Theme tokens; opacity
  variants via `Qt.alpha(Theme.<token>, a)`.
- **MVVM:** `ReportingViewModel` is the only QML-facing C++; QML never touches `ReportController`.
- **PascalCase** QML types + C++ VM/model classes; `m_camelCase` C++ members.
- No new network on screen construction; fetches are navigation-gated (bootstrap) or explicit
  (Generate).

## 9. Open item to resolve in the plan

**Semester date-range convention mismatch.** `ReportController::computeDateRange` case 2 uses
**Jan–Jun / Jul–Dec**; `get_report_data.php` computes its **own** semester range and **ignores the
client `start`/`end`**, using **Jun–Oct (1st) / Nov–Mar (2nd) / Apr–May (summer)** — apparently the
Philippine academic calendar. Recommendation: for Semester, **send the components
(`durationType`+`year`+`semester`) and let the server range** (matching the academic calendar), and
do **not** rely on the client's semester `computeDateRange` output; Day/Month/Custom continue to
send client-computed `start`/`end`. The plan pins the exact `buildFilters` behavior and its test.
(A deeper reconciliation — aligning `computeDateRange` to the server, or routing all modes through
one convention — is out of scope for 4b-i; note it as a follow-up.)

## 10. Deliverables

- `qt-app/quick/viewmodels/ReportingViewModel.{h,cpp}` (+ `QML_ELEMENT` registration in
  `qt-app/quick/CMakeLists.txt`).
- `qt-app/quick/models/ReportRowsModel.{h,cpp}`.
- `qt-app/quick/qml/components/LDatePicker.qml`.
- `qt-app/quick/qml/admin/ReportingScreen.qml` (rebuilt).
- `qt-app/quick/qml/admin/AdminScreen.qml` (VM instance + binding).
- `qt-app/tests/tst_reportingviewmodel.cpp` (+ CMake) and QuickTest additions to
  `tst_qml_admin.qml` / `tst_qml_components.qml`.

## 11. What 4b-ii will add (context, not built here)

Palette + chart-type selectors, and PDF/Excel/print **export** driven by the stateless
`ReportRenderer` (QtCharts offscreen `QImage` + QXlsx) run on a **worker thread** (queued signal,
verified with `QSignalSpy`), reusing the rows already fetched for the preview.
