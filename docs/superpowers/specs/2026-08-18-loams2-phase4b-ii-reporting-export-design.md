---
title: "LOAMS 2.0 Phase 4b-ii — Reporting: Export (PDF / Excel / Print) — Design Spec"
phase: 4b-ii
status: draft
depends_on: 4b-i (merged, PR #40, squash ea363c9)
author: brainstorming (owner-approved decisions 2026-08-18)
---

# LOAMS 2.0 Phase 4b-ii — Reporting: Export (PDF / Excel / Print) (Design Spec)

> Successor slice to **4b-i** (filters + native preview, merged via PR #40). 4b-ii turns the
> already-fetched report into shareable output — a themed PDF, an Excel workbook, and a printout —
> reusing the rows 4b-i fetched and the stateless `ReportRenderer` that already exists and is
> unit-tested in `witscore` (`qt-app/tests/tst_reportrenderer.cpp`, run under the legacy Widgets
> test harness — see §6 for why that harness masks the application-object issue this slice fixes).

## 1. Summary

The Reporting screen already fetches rows and shows a native preview (table + ranked bar chart +
stat tiles). 4b-ii adds an **export bar** to that screen: pick a **palette** and a **chart type**,
then **Export PDF**, **Export Excel**, or **Print**. All three drive the existing
`ReportRenderer` (`qt-app/core/reportrenderer.{h,cpp}`) — no new endpoint, no refetch. The rows the
user is looking at are the rows that get exported (fetch once → preview + export).

Export runs **on the GUI thread** behind a **busy overlay** — see §6 for why off-thread is not
viable with this renderer. Chart output is **Bar or Pie only**; the line chart is intentionally
excluded because the aggregated endpoint returns no per-visit timestamps to back it.

## 2. Scope

### 2.1 In scope (4b-ii)

- **Export bar** on `ReportingScreen.qml`, visible only when a report has been generated
  (`hasResult` and the result is non-empty):
  - **Palette** picker — `Default` (multi-color), `Blue`, `Green`, `Red` (exactly what
    `ReportController::getPalette` supports).
  - **Chart type** picker — `Bar`, `Pie`.
  - **Export PDF** — native "Save As" (`*.pdf`) → `QPdfWriter` → `ReportRenderer::paintReport`.
  - **Export Excel** — native "Save As" (`*.xlsx`) → `QXlsx::Document` →
    `ReportRenderer::writeReportToXlsx` → `saveAs`.
  - **Print** — `QPrintDialog` → `QPrinter` → `ReportRenderer::paintReport`.
- A **busy overlay** during export (blocks input, shows "Exporting…") and **success/failure
  feedback** (toast or status line).
- `ReportingViewModel` export surface: palette/chart-type state, `canExport` gate, `exporting`
  flag, `exportPdf(url)`, `exportExcel(url)`, `printReport()`, plus a pure `buildExportFilters(...)`
  static seam and a private shared `renderToDevice(...)` used by both PDF and Print.
- **CMake:** add `Qt::PrintSupport` to `witsquickmodule` (the one Qt component the VM's new code
  needs that `witscore` does not already PUBLIC-propagate).
- Tests: pure-static filter-builder unit tests; OFFSCREEN export-to-temp-file tests (PDF header +
  size, Excel round-trip cell read); QuickTest for the export bar (buttons gate on `canExport`,
  overlay toggles on `exporting`, pickers bind).

### 2.2 Out of scope — deferred

- **Line / peak-hours chart** and any "All charts" mode — unbacked by `get_report_data.php`
  (no per-row `login_time`; see §3). A future timeseries endpoint would be its own phase.
- **Off-GUI-thread rendering** — not viable with QtCharts/`QPixmap` (see §6). If a real-world
  report ever janks perceptibly, revisit with the `paintReport`-refactor option then (YAGNI).
- **New backend work** — endpoints are read-only and unchanged.
- The **4b-i deferred cleanups** (extract shared `LFilterCard`, `LComboBox` dropdown PlainText,
  reset-on-nav, first-load spinner, `LDatePicker` month-sync, `applyResult` double-aggregate,
  namespace `reportVisits`) — remain separate follow-ups; not folded into this slice.

### 2.3 Design OS UX contract (binding)

The behavior below is a hard contract for this slice — the architecture in §4 must satisfy it, and
review checks against it:

1. **Generate is the commitment point.** Exports always use the **last successfully generated
   result** (`m_exportRows`). Changing filters after Generate does **not** change what exports until
   the user presses Generate again. This guarantees the export equals what the user is viewing.
2. **Preview / export parity.** Export uses the same data, palette semantics, and chart-type
   semantics as the on-screen preview. No export-only visualizations. Chart types stay **Bar + Pie**;
   palettes stay **Default / Blue / Green / Red**.
3. **Export controls appear only when an exportable result exists** (§4.3).
4. **Familiar native dialogs.** Standard "Save As" (`FileDialog`) and Print (`QPrintDialog`) — no
   custom file/print UI.
5. **Small surface area.** The export bar is exactly: Palette, Chart type, PDF, Excel, Print. Nothing
   else in 4b-ii.
6. **Always communicate state and failures** (§4.5 feedback hierarchy).
7. **Preserve reporting context.** An export (success or failure) never navigates away and never
   resets filters, generated results, preview, palette, or chart selection.
8. **Accessible and keyboard-usable** by default (§4.5).
9. **Never offer unsupported or misleading visualizations** (no empty line chart; see §3).
10. **Avoid premature complexity.** GUI-thread rendering is retained unless real profiling shows a
    problem (§6).

**Explicitly NOT in 4b-ii** (would add complexity without demonstrated value): export confirmation
or "Are you sure?" prompts; progress percentages; cancellation infrastructure; worker-thread
rendering; advanced formatting panels; additional chart types; an export/preview-of-export dialog;
custom print-settings UI.

## 3. Data reality that shapes the design (findings)

Confirmed by reading `deliverables/loams_api/get_report_data.php` and `reportrenderer.cpp`:

- The endpoint returns, per student, the **8 columns** the PDF/Excel tables draw:
  `school_id, name, gender, status, course, department, year_level, visits` — where `visits` is a
  `COUNT(v.id)` over a `GROUP BY` on the student. **PDF and Excel tables are fully backable.**
- Because the query **groups visits away into a count**, there is **no per-row `login_time`**.
  `ReportRenderer::makeLineChartImage` / `aggregateVisitsByCourseHour` key off `login_time`, so a
  line chart would render **empty**. → **Bar + Pie only** (owner decision), consistent with 4b-i
  dropping the on-screen line chart for the same reason.
- `visits` arrives as a mysqli JSON **string** ("5"); the renderer's `obj["visits"].toInt()` yields
  0 for a string. 4b-i already normalizes visits via `reportVisits()` in `ReportRowsModel`. The
  **rows the VM stores for export must carry a numeric `visits`** so `paintReport`/`writeReportToXlsx`
  aggregate and print correctly (see §4.2, "Row normalization for export").

## 4. Architecture

### 4.1 MVVM & the dialog seam

`ReportingViewModel` stays the only QML-facing C++ for this screen. The view owns file-picking; the
VM owns rendering:

- **Save-path dialogs** use QML-native `FileDialog` (`QtQuick.Dialogs`, already linked via
  `QuickDialogs2`). QML passes the chosen `QUrl` to `vm.exportPdf(url)` / `vm.exportExcel(url)`.
  This keeps the file-picker a view concern — clean MVVM.
- **Print dialog:** Qt Quick has **no native print dialog**, so `printReport()` constructs a
  `QPrinter` + `QPrintDialog` (Widgets) inside the VM. This is a deliberate, **isolated** MVVM
  exception — the process already links Widgets, and it is confined to this one method. Flagged for
  spec review (§9).

### 4.2 `ReportingViewModel` additions (the only C++ unit changed)

**Retain the raw rows for export.** `applyResult(const QJsonArray &data)` currently aggregates into
models/tiles. Add storage of a **normalized** copy of the rows:

- New member `QJsonArray m_exportRows;` populated in `applyResult` by copying `data` and rewriting
  each row's `visits` to a numeric value via the shared `reportVisits()` helper (so the renderer's
  `toInt()` works). Cleared when a new fetch starts and on error.

**Row normalization for export (pure, testable).**
```cpp
// Returns a copy of `data` with each object's "visits" coerced to a JSON number,
// so ReportRenderer's obj["visits"].toInt() aggregates correctly. Other fields pass through.
static QJsonArray normalizeExportRows(const QJsonArray &data);
```

**Export filters (pure, testable).** Builds exactly the keys `paintReport` and `writeReportToXlsx`
read: `department, course, start, end, schoolYear, chartType`.
```cpp
static QJsonObject buildExportFilters(const QString &department, const QString &course,
                                      const QString &start, const QString &end,
                                      const QString &schoolYear, const QString &chartType);
```
- `department` / `course`: current state; empty department is rendered as `"All Departments"`,
  empty course as `"All Courses"` for human-readable header text.
- `start` / `end`: the **Period** printed in the PDF/Excel header. **Source per duration:**
  - **Day / Month / Custom** — the `DateRange` `computeDateRange` already produces for that mode.
  - **Semester** — `computeDateRange`'s semester branch does **not** match the data: `buildFilters`
    sends only `year`+`semester` and lets the server range it, and the server's Philippine-calendar
    windows (`get_report_data.php`: First = Jun 1–Oct 31, Second = Nov 1–(year+1) Mar 31, Summer =
    Apr 1–May 31) differ from `computeDateRange`'s Jan–Jun/Jul–Dec, whose `semester.contains("1")`
    test also never matches the QML labels ("First Semester"/"Second Semester"/"Summer"). So the
    Period for Semester **must be derived from the server's windows**, via a new pure helper matched
    to them (labels matched on `"first"`/`"second"`/`"summer"`, case-insensitive — never a digit):
    ```cpp
    // Display-only Period for a semester, matching get_report_data.php's server windows,
    // so the printed range equals the data the server actually returned.
    static DateRange semesterWindow(const QString &semester, int year);
    ```
    (This is display parity, not a second query — the server still ranges the actual rows.)
- `schoolYear`: `monthYear` (Month), `semYear` (Semester), else the year of `start` (Day/Custom),
  as a string; `""` if indeterminate.
- `chartType`: `"Bar"` or `"Pie"` from state.

**Header info from settings.** Gather `ReportHeaderInfo` (school name/address/logoPath, admin
name/position, library open/close hours) — the same keys Phase 4c Settings writes (`school/*`,
`admin/*`, `library/*`). **Read them through the project's `AppSettings` seam
(`qt-app/core/appsettings.h`), NOT a bare `QSettings`.** `AppSettings` is the mandated wrapper (Qt 6's
`QSettings(org, app)` ignores `setDefaultFormat`/`setPath` and hits the real registry); every test
isolates settings via that scope (`testsupport/settingsisolation`). A bare `QSettings` here would
read the wrong hive (mismatching what 4c wrote) and break test isolation. Encapsulated in a small
private helper `ReportHeaderInfo headerInfo() const;` that constructs an `AppSettings`.

**Shared render seam (private).**
```cpp
// Gathers m_exportRows + buildExportFilters(...) + getPalette(palette) + headerInfo(),
// then calls ReportRenderer::paintReport(dev, resolution, ...). Both PDF and Print use this.
bool renderToDevice(QPagedPaintDevice *dev, int resolution);
```

**New Q_PROPERTYs**
| property | type | notes |
|---|---|---|
| `palettes` | `QStringList` CONSTANT | `["Default","Blue","Green","Red"]` — feeds the combo |
| `palette` | `QString` R/W NOTIFY | selected palette name; default `"Default"` |
| `chartTypes` | `QStringList` CONSTANT | `["Bar","Pie"]` |
| `chartType` | `QString` R/W NOTIFY | default `"Bar"` |
| `exporting` | `bool` NOTIFY | true while an export/print is in flight |
| `canExport` | `bool` NOTIFY | `hasResult && !loading && !exporting && rows.count() > 0` |
| `exportStatus` | `QString` NOTIFY | last success message (e.g. "Saved report.pdf") |
| `exportError` | `QString` NOTIFY | last failure message (PlainText; may echo a path) |

**New Q_INVOKABLEs**
- `void setPalette(const QString&)`, `void setChartType(const QString&)`
- `void exportPdf(const QUrl &fileUrl)` — `QPdfWriter writer(localPath); writer.setPageSize(A4);
  renderToDevice(&writer, writer.resolution());` set status/error.
- `void exportExcel(const QUrl &fileUrl)` — `QXlsx::Document doc; writeReportToXlsx(doc, m_exportRows,
  buildExportFilters(...), headerInfo()); doc.saveAs(localPath);` set status/error.
- `void printReport()` — `QPrinter printer; QPrintDialog dlg(&printer);
  if (dlg.exec() == Accepted) renderToDevice(&printer, printer.resolution());`

**Authoritative validation at export (Design OS #3).** `canExport` governs UI availability, but each
of `exportPdf` / `exportExcel` / `printReport` **independently re-validates** before it renders or
writes: `m_exportRows` non-empty; for PDF/Excel the target must yield a non-empty local path
(`url.toLocalFile()` is empty for a non-file URL — reject it); device opened. If any check fails, it
sets `exportError` and returns **without** claiming success. **Success is emitted
only after the actual write/render succeeds** — `saveAs()` returning true for Excel, `paintReport()`
returning true and the file existing for PDF. Stale or invalid state fails safely, never producing
misleading output.

**Print-state semantics (Design OS #10).** Opening `QPrintDialog` is **not** "exporting": the normal
UI stays interactive while the native dialog is up, and `exporting` becomes true **only after the
user accepts** the dialog and rendering begins — then back to false on completion. A cancelled print
dialog is a no-op (no error, no busy state).

**Single-in-flight + overlay-paints-first.** All three entry points no-op when `exporting` is
already true. Once actual rendering is to begin (for print, *after* the dialog is accepted) they set
`exporting = true`, then **defer the blocking render one event-loop turn**
(`QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)` or `QTimer::singleShot(0, ...)`) so
QML paints the busy overlay before the synchronous `paintReport` freezes the loop; on completion set
`exporting = false` and emit status/error. This is **best-effort, not a hard guarantee** — posting
the property change ahead of the blocking render makes the overlay paint first in practice, but does
not strictly guarantee the frame is on screen before the freeze. That is an accepted tradeoff of
GUI-thread rendering (§6); the plan should not over-promise it. The spinner won't animate *during*
the freeze either — inherent to GUI-thread rendering — but the user gets a clear "working" state and
cannot double-fire.

**Context preservation (Design OS #7).** No export path mutates filters, duration, `m_exportRows`,
preview models/tiles, `palette`, or `chartType`, and none triggers navigation. Only
`exporting` / `exportStatus` / `exportError` change across an export.

### 4.3 QML — `ReportingScreen.qml` export bar

Added below the existing preview, wrapped so it only appears once there is a non-empty result
(`visible: screen.vm && screen.vm.hasResult && screen.vm.rows.count > 0`):

- **Palette** `LComboBox` (`objectName: "paletteCombo"`) bound to `vm.palettes` / `vm.palette`.
- **Chart type** `LComboBox` (`objectName: "chartTypeCombo"`) bound to `vm.chartTypes` / `vm.chartType`.
- **Export PDF** `LButton` (`objectName: "exportPdfButton"`) → opens `exportPdfDialog`.
- **Export Excel** `LButton` (`objectName: "exportExcelButton"`) → opens `exportExcelDialog`.
- **Print** `LButton` (`objectName: "printButton"`) → `vm.printReport()`.
- All three buttons `enabled: screen.vm ? screen.vm.canExport : false`.
- `FileDialog` × 2 (`QtQuick.Dialogs`): PDF (`nameFilters: ["PDF (*.pdf)"]`, `defaultSuffix: "pdf"`)
  and Excel (`*.xlsx`), `fileMode: FileDialog.SaveFile`, suggested name
  `LOAMS_Report_<Dept>_<start>_to_<end>` (sanitized). `onAccepted` → `vm.exportPdf(selectedFile)` /
  `vm.exportExcel(selectedFile)`.
- **Busy overlay** (`objectName: "exportBusyOverlay"`): a full-screen themed `Rectangle` +
  `BusyIndicator` + "Exporting…" label, `visible: screen.vm && screen.vm.exporting`, catching input
  with a `MouseArea`.
- **Empty-state affordance (Design OS #4).** When a report has been generated but has zero rows,
  show a short PlainText line near the export bar — **"No data to export. Adjust the filters and
  generate a report with results."** — instead of relying on the disabled buttons alone. No modal.
- **Feedback hierarchy (Design OS #5/#6):**
  - **Success** → a **transient** toast/status ("Saved `<file>`", or "Sent to printer"), driven by
    `exportStatus`.
  - **Failure** → a **persistent** inline error/status (`exportError`) that stays until the next
    export replaces it or the user dismisses it. Messages are actionable (e.g. "Couldn't write
    `<path>` — choose a different location").

All colors via `Theme.qml` tokens; all `exportStatus` / `exportError` and any server- or
path-derived text rendered `Text.PlainText`.

### 4.5 Accessibility & keyboard (Design OS #7/#8)

- **Accessible names** on every export control (`Accessible.name` / `Accessible.role`): the two
  combos ("Report palette", "Chart type") and the three buttons ("Export PDF", "Export Excel",
  "Print report"). `LButton` already exposes an `accessibleName` property — reuse it. **`LComboBox`
  does not** currently pass an accessible name through to its inner `ComboBox` (its root is a bare
  `Item`); the plan must either add an `Accessible.name` passthrough to `LComboBox` or set it on the
  inner control, and the QuickTest must assert it.
- **Disabled state is exposed**, not just styled — bind `Accessible.description`/enabled so a screen
  reader reports *why* (e.g. "unavailable until a report with results is generated"); the empty-state
  line above carries the same reason visibly. State is never communicated by color/spinner/disabled
  styling **alone**.
- **Busy overlay announces progress** — the overlay exposes an accessible "Exporting…" name so the
  in-progress state is perceivable without seeing the spinner.
- **Keyboard:** export controls are in normal tab order; Enter/Space activate the focused button;
  combos are arrow-key navigable. Native Save/Print dialogs keep their standard keyboard behavior.
  **No custom shortcuts** unless an existing LOAMS component already defines them.
- **Focus is predictable** across dialog open/close — after a Save/Print dialog closes, focus returns
  to the control that opened it (or a sensible sibling), never lost to the window root.

### 4.4 Application object (Critical) & CMake delta

**WITSQuick must run a `QApplication`, not a `QGuiApplication`.** Today `qt-app/quick/main.cpp:13`
constructs a **`QGuiApplication`** — WITSQuick is a pure Qt Quick app that never makes a `QWidget`.
But `ReportRenderer::renderChartToImage` builds a **`QChartView` (a `QWidget`)** for every Bar/Pie
page, and the Print path uses **`QPrintDialog` (also a `QWidget`)**. Constructing a `QWidget` under a
`QGuiApplication` is a `qFatal` ("Cannot create a QWidget without QApplication") → the shipped
`WITSQuick.exe` **aborts on export**. This is invisible to tests (see §6/§7), so it is called out
here as a hard prerequisite.

- **`qt-app/quick/main.cpp`:** change `QGuiApplication` → `QApplication` (`#include <QApplication>`).
  `QApplication` *is-a* `QGuiApplication`; Qt Quick runs unchanged under it. This is the minimal
  remedy and is required regardless of charts, because `QPrintDialog` needs it too. (The deferred
  alternative — refactor `paintReport` to take pre-rendered `QImage`s and swap the logo `QPixmap`→
  `QImage` — is *not* taken in this slice; the one-line app-object switch is simpler and lower-risk.)
- **CMake:**
  - `qt-app/quick/CMakeLists.txt`: add `Qt${QT_VERSION_MAJOR}::PrintSupport` to
    `target_link_libraries(witsquickmodule PUBLIC …)`. (Charts, Widgets, QXlsx already arrive PUBLIC
    from `witscore`; `QPdfWriter`/`QPagedPaintDevice` are Qt::Gui, already linked. `QApplication`
    comes from Qt::Widgets, likewise already PUBLIC-propagated — so `WITSQuick` needs no new link,
    only the include; add `Qt::Widgets` explicitly to the `WITSQuick` target for clarity.)
  - The new OFFSCREEN export test target links `Qt::PrintSupport` (QPrinter-adjacent include) and
    `Qt::Gui` (already used).

## 5. Data flow (export)

```
Generate (4b-i) ──► applyResult(data): m_exportRows = normalizeExportRows(data); models/tiles updated
                                   │
User picks palette + chartType     │  (canExport true once hasResult && rows>0 && !busy)
                                   ▼
Export PDF ─► FileDialog(save,*.pdf) ─► vm.exportPdf(url):
      exporting=true → (defer) → QPdfWriter(A4) → renderToDevice → status/error → exporting=false
Export Excel ─► FileDialog(save,*.xlsx) ─► vm.exportExcel(url):
      exporting=true → (defer) → QXlsx::Document → writeReportToXlsx → saveAs → status/error
Print ─► vm.printReport(): QPrintDialog → (if accepted) exporting=true → (defer) →
      renderToDevice(QPrinter) → status/error → exporting=false
```

`renderToDevice` = `getPalette(palette)` + `buildExportFilters(...)` + `headerInfo()` +
`ReportRenderer::paintReport(dev, resolution, m_exportRows, filters, palette, info)`.

## 6. Threading & responsiveness (the pivotal constraint)

`ReportRenderer::renderChartToImage` builds a **`QChartView` (a `QWidget`)** and calls
`view.show()` / `view.render()`; `paintReport` also draws the logo through **`QPixmap`**. In Qt,
`QWidget` and `QPixmap` operations are **GUI-thread-only**. Therefore **PDF and Print cannot run on
a worker thread** without refactoring `witscore` (render charts to `QImage` on the GUI thread, pass
them in, swap the logo to `QImage`). `writeReportToXlsx` is widget-free and *could* run off-thread,
but threading a single format in isolation adds machinery for little gain.

**Decision (owner):** run **all exports on the GUI thread** behind a **busy overlay**, deferring the
blocking render one event-loop turn so the overlay paints first (§4.2). Typical library datasets
render sub-second; the overlay + input-blocking is sufficient UX. The `paintReport`-refactor remains
a documented future option if a real report ever janks perceptibly.

**Because these are `QWidget` operations, the app object must be a `QApplication` (§4.4).** This is
also why the constraint is easy to miss: the existing `tst_reportrenderer` and every QtTest target
run under `QTEST_MAIN`, which instantiates a **`QApplication`** whenever `Qt::Widgets` is linked
(it is, PUBLIC from `witscore`). So chart rendering "works in tests" and any OFFSCREEN `exportPdf`
test passes — while the real `WITSQuick.exe` (a `QGuiApplication`) would `qFatal`. The test harness
**cannot** catch this mismatch; the guards are (a) making the `main.cpp` switch part of this slice's
deliverables (§10), and (b) a mandatory manual GUI smoke of a real PDF/Print export on
`WITSQuick.exe` (§7.4) before the branch is finished.

## 7. Testing & gates (TDD, ctest)

### 7.1 C++ pure-static unit tests (extend `tst_reportingviewmodel`)
- `buildExportFilters(...)`: correct keys/values for Day, Month, Semester, Custom; empty
  department → `"All Departments"`; empty course → `"All Courses"`; `schoolYear` selection per mode;
  `chartType` passthrough.
- `normalizeExportRows(...)`: string `"5"` → numeric `5`; already-numeric passthrough; other fields
  preserved; empty array → empty.
- `canExport` truth table: false while `!hasResult`, while `loading`, while `exporting`, and when
  `rows.count()==0`; true otherwise.
- Single-in-flight: second export call while `exporting` no-ops.

### 7.2 OFFSCREEN export-to-temp-file tests (extend `tst_reportingviewmodel`, add `OFFSCREEN`)
Feed the VM a synthetic result via the existing `onReportDataReady` seam (no live network), then:
- `exportPdf(tmp.pdf)`: file exists, size > 0, first bytes are `"%PDF"`; `exportStatus` set,
  `exporting` returns to false.
- `exportExcel(tmp.xlsx)`: reopen with `QXlsx::Document` and assert a known header/data cell reads
  back (e.g. a course name and its visits count).
- Error path: `exportPdf` to an unwritable path → `exportError` set, no crash, `exporting` false.
- `printReport()` is **not** auto-tested (modal dialog); its render path is covered by the shared
  `renderToDevice` exercised via `exportPdf`.

### 7.3 QuickTest (offscreen, plain-QML stub `vm`) — extend `tst_qml_admin`
- Export bar hidden when `hasResult` false / rows empty; visible when a stub result is present.
- PDF/Excel/Print buttons disabled when `canExport` false, enabled when true.
- `exportBusyOverlay` visible iff `vm.exporting`.
- Palette / chart-type combos reflect and write `vm.palette` / `vm.chartType`.
- **Empty-state message** (Design OS #4) shows when a stub reports `hasResult` true with zero rows.
- **Feedback hierarchy** (Design OS #5): a stub `exportStatus` shows transient success; a stub
  `exportError` shows a persistent inline error that survives until replaced/dismissed.
- **Accessibility**: each export control exposes an `Accessible.name`; disabled buttons expose their
  disabled/why state (assert `Accessible.name` / enabled).
- **Context preservation** (Design OS #7): toggling `exporting` on the stub does not change
  `palette` / `chartType` / preview bindings.

### 7.4 Gates
Full `ctest` green (the current suite count — **verify with `ctest` at plan time**, don't hard-code a
number — plus the new tests). Then project **`create-pr` 3-agent gate** (dry-checker,
security-reviewer, general-code-reviewer — **no** api-checker). Whole-branch Claude review before
finishing.

**Mandatory manual GUI smoke on the real `WITSQuick.exe` (not tests) — this is a release gate, not
optional:** because the test harness runs under `QApplication` and cannot catch the
`QGuiApplication`→`QApplication` fix (§6), the branch is not done until an owner has, on the running
`WITSQuick.exe`: generated a report, then **Export PDF** (save + open the file), **Export Excel**
(save + open), and **Print** (open the dialog, print or preview) — each completing without the app
aborting. A crash here is the exact failure the automated suite is blind to.

## 8. Security & constraints

- **PlainText everywhere** server/path-derived text is shown (`exportError`, status). No markup
  injection over cleartext HTTP — same hardening 4b-i applied to tiles/bars.
- **No secrets / PII** in the spec, tests, or fixtures — synthetic rows only.
- **File writes** go only where the user's Save dialog points (or the chosen printer). No silent
  writes to fixed paths.
- Reuses fetched rows; **no refetch**, no new network surface, no new endpoint.
- `logoPath` from `AppSettings` is read as an image by `paintReport` (`QPixmap`); a missing/invalid
  path degrades gracefully (renderer already null-checks). No path is taken from network data.

## 9. Open items to resolve in the plan / review

1. **Print-dialog MVVM exception** (§4.1): VM owning `QPrintDialog` — accept as an isolated
   exception, or route through a tiny Widgets "print service" helper? (Lean: accept; it is one
   method and Widgets is already linked.)
2. **`schoolYear` for Day/Custom**: derive from `start`'s year vs. leave blank — confirm the header
   text reads sensibly for a single-day report.
3. **Empty-result export** — **resolved by Design OS #4**: `canExport` is false when
   `rows.count() == 0`, and the screen shows the "No data to export…" line explaining why. No
   empty-with-headers export.
4. **Deferred-render turn**: `QTimer::singleShot(0,…)` vs `invokeMethod(Qt::QueuedConnection)` — pick
   one in the plan; both paint the overlay first.

## 10. Deliverables

- **`qt-app/quick/main.cpp`** — `QGuiApplication` → `QApplication` (the §4.4 Critical prerequisite;
  without it the app aborts on export).
- `qt-app/quick/viewmodels/ReportingViewModel.{h,cpp}` — export state, `buildExportFilters` +
  `normalizeExportRows` + `semesterWindow` (pure), `headerInfo()` (via `AppSettings`),
  `renderToDevice()`, `exportPdf/exportExcel/printReport`.
- `qt-app/quick/qml/admin/ReportingScreen.qml` — export bar, two `FileDialog`s, busy overlay, feedback.
- `qt-app/quick/qml/components/LComboBox.qml` — `Accessible.name` passthrough (if not already present).
- `qt-app/quick/CMakeLists.txt` — `Qt::PrintSupport` on `witsquickmodule`, `Qt::Widgets` explicit on
  `WITSQuick` (+ the export test target).
- `qt-app/quick/tests/tst_reportingviewmodel.cpp` — pure + OFFSCREEN export tests.
- `qt-app/quick/tests/tst_qml_admin.qml` (+ `.cpp` band) — export-bar QuickTest.
- This spec; then a `/writing-plans` TDD plan; both `/claude-review`ed before build.

## 11. Non-goals recap

No new endpoint, no off-thread rendering, no line/peak-hours chart, no 4b-i cleanup folding. This
slice is strictly: reuse the fetched rows + the existing `ReportRenderer` to produce a themed PDF,
an Excel workbook, and a printout, gated behind a clear busy state.
