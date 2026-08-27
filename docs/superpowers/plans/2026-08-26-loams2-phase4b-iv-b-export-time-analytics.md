# LOAMS 2.0 — Phase 4b-iv-b: Export the "When?" Time Analytics — Implementation Plan

**Date:** 2026-08-26
**Spec (source of truth):** `docs/superpowers/specs/2026-08-26-loams2-phase4b-iv-b-export-time-analytics-design.md`
**Branch:** cut a worktree branch off `master` (main checkout is at `C:/Users/USER/OneDrive - usep.edu.ph/Documents/WITS/WITS-main`).

---

## Goal

Render the SAME "When?" time analytics already shown on the 4b-iv-a screen into the **PDF** and **Excel** exports, so an exported report carries the full question ladder — How much? → Who? → Which? → **When?** → Details — and screen↔export parity holds automatically. The export path adds only *formatting and layout*: it consumes the ViewModel's already-cached `m_timeAnalytics` (+ `m_timeError`) via a presentation-ready carrier — **no second backend fetch, no re-aggregation.**

## Architecture

Layering is unchanged from 4b-iii-b / 4b-iv-a:

- **core is presentation-agnostic.** A new plain-data carrier `ReportTimeExport` + enum `TimeAnalyticsExportState` live in `qt-app/core/reportdata.h` (the leaf header that already holds `ReportHeaderInfo`/`ReportPalette`), so they compile into `witscore` and are visible to both the ViewModel and the stateless renderer. The carrier has no methods and no project-header includes.
- **The ViewModel owns ALL data-derived presentation.** `ReportingViewModel::buildTimeExport()` assembles the carrier from cached state, formatting hour/weekday labels + peak VALUE strings with the 4b-iv-a helpers so the exported labels are byte-identical to the on-screen ones. It computes the `state` (Data / Empty / Error) and never produces `Disabled`.
- **The renderer owns fixed export-document prose + layout.** `ReportRenderer::paintReport` (PDF) and `writeReportToXlsx` (Excel) each gain one trailing `const ReportTimeExport &timeExport` parameter (no new bool). The renderer wraps the carrier's bare VALUES in its own fixed templates (`"Peak Hour: %1"` / `"Busiest Day: %1"`), draws the section title / column headers / state messages, and switches on `timeExport.state`. It performs ZERO hour/weekday string math.
- **Legacy WITS.exe (`adminwindow.cpp`) is frozen.** Its 3 renderer call sites append a default-constructed `ReportTimeExport{}` (`state == Disabled`), so the renderer omits the section and WITS.exe output is byte-for-byte unchanged.

```
 m_timeAnalytics : TimeAnalytics   m_timeError : QString   (both already cached in 4b-iv-a)
        └───────────────┬──────────────────┘
                        ▼
   ReportingViewModel::buildTimeExport() const   → ReportTimeExport (core carrier)
        │   labels via hourTick / weekdayShortName; peaks via formatHourRange / weekdayName
        │   state = Error | Empty | Data          (Disabled never produced here)
        ├──────────────► ReportRenderer::paintReport(...)        → PDF: 2 chart images (captions in titles)
        └──────────────► ReportRenderer::writeReportToXlsx(...)  → Excel: side-by-side tables
                              ▲
   adminwindow.cpp passes default ReportTimeExport{} (Disabled) → section omitted; WITS.exe unchanged
```

## Tech Stack

Qt 6.11.1 / C++17 / CMake + Ninja. QtCharts (`QChartView` → raster image), QXlsx (vendored, `write(row,col)` addressing), Qt Test. MVVM: ViewModel is the only QML-facing C++. No new source files, no new CMake targets — every change lands in existing `reportdata.h`, `reportrenderer.{h,cpp}`, `ReportingViewModel.{h,cpp}`, `adminwindow.cpp`, and the two existing test files.

---

## Global Constraints

**Build (PowerShell; Qt tools are NOT on PATH; use a SHORT external build dir to avoid the Windows MAX_PATH overflow on the QML module autogen path):**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S "<worktree>\qt-app" -B C:\b\loams-4bivb -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:\b\loams-4bivb
ctest --test-dir C:\b\loams-4bivb --output-on-failure
```

- **Baseline once at branch start:** configure + build + `ctest`, and record the green test count. Do **NOT** hard-code the number — capture whatever the baseline reports and require it to stay green (plus the new cases) at every task boundary.
- **`tst_settingsviewmodel` is a known flake** under full-suite parallel load. If it fails, re-run it alone (`ctest --test-dir C:\b\loams-4bivb -R tst_settingsviewmodel --output-on-failure`) before treating it as real.
- **A CMakeLists change ⇒ reconfigure before build.** (This slice adds none — verify, don't assume.)
- **Close any running `WITSQuick.exe` before rebuilding** — a live process locks the binary and breaks relink.
- **Ignore** CRLF warnings and the pre-existing QXlsx "GuiPrivate" deprecation warnings — they are not introduced by this slice.
- **Formatting boundary (binding):** the renderer does ZERO hour/weekday string math — it consumes VM-formatted labels and composes only its own fixed prose/templates. The VM does ZERO export-document wording (no "Peak Hour:" prefix, no section title, no state sentences) — it hands over bare VALUES + a `state`.
- **One computed truth:** consume the cached `m_timeAnalytics` (+ `m_timeError`); NO second fetch and NO duplicate aggregation in the export path. `canExport` stays rows-only; a time-fetch Error never blocks export.
- **Legacy WITS.exe byte-for-byte unchanged:** the 3 `adminwindow.cpp` call sites pass a default `ReportTimeExport{}` (= Disabled).
- **No new source files or CMake targets.** No real student PII in tests — synthetic data only.
- **The screen-safe-size → `drawFullscreenChart`-upscale path is MANDATORY** for the two new chart makers. A `QChartView` is a `QWidget` the window system clamps to the physical screen; rendering it directly at print resolution (~9000 px) lays the chart out at ~screen size in a corner with giant fonts and a blank remainder — a corrupt export. Obtain the raster size from `chartImageSize(usableWidth, /*square=*/false)` and let `drawFullscreenChart` upscale it — NEVER an arbitrary or print-resolution size. This clamp is invisible to headless/offscreen tests, which is exactly why Task 4's manual smoke is a release gate. (Project memory: "QtChart export screen clamp".)
- **Commit each task directly** with a Conventional Commit (scope `reporting` or `loams_api` as apt) via the `commit` skill. Do **NOT** `git add -A`. Project convention: **NO** Claude/Anthropic co-author trailer.
- **Line numbers drift** — locate every edit by the surrounding quoted context, not by line number.
- **Preserve the EN DASH literally.** `formatHourRange` emits ranges with U+2013 (`"2–3 PM"`, and thus `"Peak Hour: 2–3 PM"`) — every test literal and fixture string in this plan uses the en-dash, matching the already-green `captions_formattedForKnownPeaks`. Copy these strings verbatim; a hyphen-minus (`-`) substitution silently fails the T1/T2 assertions.

## Cross-task type / name consistency (use these EXACT names everywhere)

- `enum class TimeAnalyticsExportState { Disabled, Data, Empty, Error };`
- `struct ReportTimeExport` — fields per spec §5 (see Task 1).
- `ReportTimeExport ReportingViewModel::buildTimeExport() const;` — **PUBLIC** (see Task 1 rationale).
- Extracted weekday helper: `static QString ReportingViewModel::weekdayShortName(int monFirstIndex);` — used identically in `buildWeekdayBars` and `buildTimeExport`.
- `ReportRenderer::makeHourlyBarChartImage` / `ReportRenderer::makeWeekdayBarChartImage`.
- `paintReport` / `writeReportToXlsx` each gain a trailing `const ReportTimeExport &timeExport` (no default argument — every call site is updated explicitly).
- Chart-title templates: `"Peak Hour: %1"` / `"Busiest Day: %1"`.
- State messages: `"Visit-time data could not be loaded"` (Error) / `"No visit activity in this range"` (Empty).
- Section title: `"When do students visit?"`.

---

## File Structure map

```
qt-app/
  core/
    reportdata.h                         [Task 1]  + enum + carrier struct + <QStringList>/<QList>
    reportrenderer.h                     [Task 2,3] + makers decl; +param on both signatures
    reportrenderer.cpp                   [Task 2,3] Excel When-block; PDF When-section + 2 makers
  quick/
    viewmodels/
      ReportingViewModel.h               [Task 1]  + weekdayShortName decl; + buildTimeExport decl (public)
      ReportingViewModel.cpp             [Task 1,2,3] extract helper; buildTimeExport; wire 2 export calls
    tests/
      tst_reportingviewmodel.cpp         [Task 1]  + 4 buildTimeExport cases
  tests/
    tst_reportrenderer.cpp               [Task 2,3] + Excel Data/Disabled/Empty/Error; + 2 maker cases;
                                                    update existing paintReport/writeReportToXlsx calls
  adminwindow.cpp                        [Task 2,3] append ReportTimeExport{} to 3 legacy call sites
```

No `CMakeLists.txt` edits: `tst_reportrenderer` already compiles `reportrenderer.cpp` + `reportanalytics.cpp` + `reportdata.h` directly (the carrier is a header-only addition, and the makers live in the already-compiled `reportrenderer.cpp` — the carrier has no `TimeAnalytics` dependency, so no new source is pulled in). `tst_reportingviewmodel` links `witsquickmodule` (which compiles `ReportingViewModel.cpp`) and transitively sees `reportdata.h`'s new types.

---

## Task 1 — Carrier + enum, VM `buildTimeExport()`, weekday-helper extraction, VM tests

Adds the core carrier/enum and the ViewModel assembler that fills it, plus a small refactor that single-sources the weekday short-names. Renderer and export call sites are NOT touched yet.

### Files

- `qt-app/core/reportdata.h` — add enum + struct + two includes.
- `qt-app/quick/viewmodels/ReportingViewModel.h` — declare `weekdayShortName` (private static) + `buildTimeExport` (public).
- `qt-app/quick/viewmodels/ReportingViewModel.cpp` — implement `weekdayShortName`, refactor `buildWeekdayBars`, implement `buildTimeExport`.
- `qt-app/quick/tests/tst_reportingviewmodel.cpp` — 4 new cases.

### Interfaces

**Produces:**
- `enum class TimeAnalyticsExportState { Disabled, Data, Empty, Error };` (in `reportdata.h`)
- `struct ReportTimeExport { ... };` (in `reportdata.h`)
- `static QString ReportingViewModel::weekdayShortName(int monFirstIndex);` — `0..6` (Mon..Sun) → `"Mon".."Sun"`, empty string out of range.
- `ReportTimeExport ReportingViewModel::buildTimeExport() const;`

**Consumes (existing, unchanged):**
- `ReportingViewModel::hourTick(int) -> QString`, `formatHourRange(int) -> QString`, `weekdayName(int) -> QString`
- members `m_timeAnalytics` (`TimeAnalytics`), `m_timeError` (`QString`)
- `TimeAnalytics` fields: `hourly` (24), `weekdayMonFirst` (7), `peakHour`, `peakWeekdayMonFirst`, `hasData`

**`buildTimeExport()` is PUBLIC because** `tst_reportingviewmodel.cpp` constructs a bare `ReportingViewModel vm;` and calls its methods directly (it has no `friend` access and reads no private members). The VM test asserts `vm.buildTimeExport()` directly, and the two export methods also call it — so public is required for testability. (Confirmed: the test file declares no friendship and reaches state only through public `on*` slots.)

### Steps

1. **Add the enum + carrier + includes to `reportdata.h`.** Locate the existing include block:

   ```cpp
   #include <QColor>
   #include <QString>
   #include <QVector>
   ```

   Replace it with (adds `<QStringList>` and `<QList>` for the new fields):

   ```cpp
   #include <QColor>
   #include <QList>
   #include <QString>
   #include <QStringList>
   #include <QVector>
   ```

   Then, immediately **before** the closing `#endif // REPORTDATA_H`, insert:

   ```cpp
   // Presentation state of the exported "When?" block. Exactly FOUR states.
   enum class TimeAnalyticsExportState {
       Disabled,  // caller opts out entirely (legacy WITS.exe) — renderer omits the section
       Data,      // time fetch succeeded and hasData — render charts (PDF) / table (Excel)
       Empty,     // time fetch succeeded but all-zero range — render the "no activity" note
       Error      // time fetch failed — render the "could not be loaded" note (DISTINCT from Empty)
   };

   // Presentation-ready carrier for the exported time analytics. Assembled by the
   // ViewModel (the single owner of hour/weekday formatting); consumed verbatim by
   // ReportRenderer, which does NO hour/weekday math of its own. Default-constructed
   // value is state == Disabled with empty lists — the legacy WITS.exe payload.
   struct ReportTimeExport {
       TimeAnalyticsExportState state = TimeAnalyticsExportState::Disabled;

       QStringList hourLabels;      // 24 entries, VM-formatted (e.g. "12A","1A", … "11P")
       QList<int>  hourCounts;      // 24 entries, index = hour 0..23
       QStringList weekdayLabels;   // 7 entries, Mon→Sun, VM-formatted short names ("Mon".."Sun")
       QList<int>  weekdayCounts;   // 7 entries, index 0=Mon .. 6=Sun

       QString     busiestHourLabel;  // VM peak VALUE, e.g. "2–3 PM"  (empty unless state==Data)
       QString     busiestDayLabel;   // VM peak VALUE, e.g. "Wednesday" (empty unless state==Data)
   };
   ```

2. **Declare the two new VM methods.** In `ReportingViewModel.h`, find the private static helper block:

   ```cpp
       static QString hourTick(int hour);          // 0..23 -> "12A","3A",...,"9P"
       static QString formatHourRange(int hour);   // 14 -> "2–3 PM"
       static QString weekdayName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Monday".."Sunday"
   ```

   Insert directly below it (still in the private section):

   ```cpp
       static QString weekdayShortName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Mon".."Sun"
   ```

   Then find the public export invokable:

   ```cpp
       Q_INVOKABLE void printReport();
   ```

   Insert directly below it (public, NOT Q_INVOKABLE — QML never calls it):

   ```cpp

       // Assembles the presentation-ready carrier from cached time state (spec §7.1).
       // PUBLIC because tst_reportingviewmodel asserts it directly; also consumed by
       // the two export seams. Pure w.r.t. member state; performs no fetch.
       ReportTimeExport buildTimeExport() const;
   ```

3. **Write the four failing VM tests.** In `tst_reportingviewmodel.cpp`, add these slot declarations at the end of the `private slots:` list (after `hasTimeData_falseOnAllZeroShowsEmptyState();`):

   ```cpp
       void buildTimeExport_dataState_populatesLabelsCountsPeaks();
       void buildTimeExport_emptyState_listsEmpty();
       void buildTimeExport_errorState_winsOverData();
       void buildTimeExport_defensiveWrongLength_degradesToEmpty();
   ```

   Then add the four implementations just before `QTEST_MAIN(TestReportingViewModel)`:

   ```cpp
   void TestReportingViewModel::buildTimeExport_dataState_populatesLabelsCountsPeaks() {
       ReportingViewModel vm;
       vm.onTimeAnalyticsReady(denseHours(), denseWeek());   // peak hour 14, peak day Monday
       const ReportTimeExport te = vm.buildTimeExport();
       QCOMPARE(te.state, TimeAnalyticsExportState::Data);

       // 24 hour labels, byte-identical to hourTick for known hours.
       QCOMPARE(te.hourLabels.size(), 24);
       QCOMPARE(te.hourLabels.at(0), QStringLiteral("12A"));
       QCOMPARE(te.hourLabels.at(3), QStringLiteral("3A"));
       QCOMPARE(te.hourLabels.at(14), QStringLiteral("2P"));

       // Counts copied straight from the cached analytics.
       const TimeAnalytics ta = TimeAnalytics::compute(denseHours(), denseWeek());
       QCOMPARE(te.hourCounts, ta.hourly);
       QCOMPARE(te.hourCounts.at(14), 12);
       QCOMPARE(te.hourCounts.at(9), 3);

       // 7 weekday labels Mon→Sun; counts == weekdayMonFirst.
       QCOMPARE(te.weekdayLabels.size(), 7);
       QCOMPARE(te.weekdayLabels.at(0), QStringLiteral("Mon"));
       QCOMPARE(te.weekdayLabels.at(6), QStringLiteral("Sun"));
       QCOMPARE(te.weekdayCounts, ta.weekdayMonFirst);
       QCOMPARE(te.weekdayCounts.at(0), 40);

       // Peak VALUE strings == the on-screen captions (parity).
       QCOMPARE(te.busiestHourLabel, QStringLiteral("2–3 PM"));
       QCOMPARE(te.busiestDayLabel, QStringLiteral("Monday"));
   }

   void TestReportingViewModel::buildTimeExport_emptyState_listsEmpty() {
       ReportingViewModel vm;
       vm.onTimeAnalyticsReady(zeros(24), zeros(7));   // all-zero, no error
       const ReportTimeExport te = vm.buildTimeExport();
       QCOMPARE(te.state, TimeAnalyticsExportState::Empty);
       QVERIFY(te.hourLabels.isEmpty());
       QVERIFY(te.hourCounts.isEmpty());
       QVERIFY(te.weekdayLabels.isEmpty());
       QVERIFY(te.weekdayCounts.isEmpty());
       QVERIFY(te.busiestHourLabel.isEmpty());
       QVERIFY(te.busiestDayLabel.isEmpty());
   }

   void TestReportingViewModel::buildTimeExport_errorState_winsOverData() {
       ReportingViewModel vm;
       vm.onTimeAnalyticsReady(denseHours(), denseWeek());   // hasData == true
       vm.onTimeAnalyticsError(QStringLiteral("network down"));   // error set AFTER data
       const ReportTimeExport te = vm.buildTimeExport();
       QCOMPARE(te.state, TimeAnalyticsExportState::Error);   // error wins over hasData
       QVERIFY(te.hourLabels.isEmpty());
       QVERIFY(te.weekdayLabels.isEmpty());
       QVERIFY(te.busiestHourLabel.isEmpty());
       QVERIFY(te.busiestDayLabel.isEmpty());
   }

   void TestReportingViewModel::buildTimeExport_defensiveWrongLength_degradesToEmpty() {
       ReportingViewModel vm;
       vm.onTimeAnalyticsReady(zeros(10), zeros(7));   // hourly wrong length -> compute bails
       const ReportTimeExport te = vm.buildTimeExport();
       QCOMPARE(te.state, TimeAnalyticsExportState::Empty);   // degrade, no OOB
       QVERIFY(te.hourLabels.isEmpty());
       QVERIFY(te.weekdayLabels.isEmpty());
   }
   ```

4. **Run the VM test target and watch it FAIL to compile.** The target references `TimeAnalyticsExportState`, `ReportTimeExport`, and `vm.buildTimeExport()`, which do not yet exist (buildTimeExport has no definition; the header additions from steps 1–2 give the declarations but no body). Expected failure: `undefined reference to 'ReportingViewModel::buildTimeExport() const'` at link (and the enum/struct resolve from step 1).

   ```powershell
   cmake --build C:\b\loams-4bivb --target tst_reportingviewmodel
   ```

5. **Extract `weekdayShortName` and refactor `buildWeekdayBars`.** In `ReportingViewModel.cpp`, find:

   ```cpp
   QList<BarsModel::Bar> ReportingViewModel::buildWeekdayBars(const QList<int> &weekdayMonFirst)
   {
       QList<BarsModel::Bar> bars;
       if (weekdayMonFirst.size() != 7)
           return bars;
       static const char *const kShort[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
       bars.reserve(7);
       for (int d = 0; d < 7; ++d)
           bars.append({ QString::fromLatin1(kShort[d]), double(weekdayMonFirst.at(d)) });
       return bars;
   }
   ```

   Replace it with (the array is single-sourced into the new helper; pure refactor — existing `timeModels_populatedWithLabelBlanking` stays green):

   ```cpp
   QString ReportingViewModel::weekdayShortName(int monFirstIndex)
   {
       static const char *const kShort[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
       if (monFirstIndex < 0 || monFirstIndex >= 7)
           return QString();
       return QString::fromLatin1(kShort[monFirstIndex]);
   }

   QList<BarsModel::Bar> ReportingViewModel::buildWeekdayBars(const QList<int> &weekdayMonFirst)
   {
       QList<BarsModel::Bar> bars;
       if (weekdayMonFirst.size() != 7)
           return bars;
       bars.reserve(7);
       for (int d = 0; d < 7; ++d)
           bars.append({ weekdayShortName(d), double(weekdayMonFirst.at(d)) });
       return bars;
   }
   ```

6. **Implement `buildTimeExport()`.** In `ReportingViewModel.cpp`, add the definition immediately after the `buildTimeExport` sibling helpers — a good anchor is right after the `weekdayName` definition:

   ```cpp
   QString ReportingViewModel::weekdayName(int monFirstIndex)
   {
       static const char *const kNames[7] = { "Monday", "Tuesday", "Wednesday",
                                              "Thursday", "Friday", "Saturday", "Sunday" };
       if (monFirstIndex < 0 || monFirstIndex >= 7)
           return QString();
       return QString::fromLatin1(kNames[monFirstIndex]);
   }
   ```

   Insert directly below that closing brace:

   ```cpp
   ReportTimeExport ReportingViewModel::buildTimeExport() const
   {
       ReportTimeExport te;

       // State order is LOCKED (spec §7.1): failure wins over empty so a failed
       // fetch is never rendered as "no visits" (spec §9 Error≠Empty).
       if (!m_timeError.isEmpty()) {
           te.state = TimeAnalyticsExportState::Error;
           return te;   // lists + peak labels stay empty
       }

       // Defensive length guard (spec §7.1): a malformed/short array degrades to
       // Empty rather than emitting a truncated carrier — a second net beneath
       // TimeAnalytics::compute (which already returns hasData=false on bad length).
       if (m_timeAnalytics.hourly.size() != 24 || m_timeAnalytics.weekdayMonFirst.size() != 7) {
           te.state = TimeAnalyticsExportState::Empty;
           return te;
       }

       if (!m_timeAnalytics.hasData) {
           te.state = TimeAnalyticsExportState::Empty;
           return te;   // peak labels empty -> no stale "12–1 AM" default leaks
       }

       te.state = TimeAnalyticsExportState::Data;

       te.hourLabels.reserve(24);
       te.hourCounts.reserve(24);
       for (int h = 0; h < 24; ++h) {
           te.hourLabels.append(hourTick(h));                 // reused 4b-iv-a helper
           te.hourCounts.append(m_timeAnalytics.hourly.at(h));
       }

       te.weekdayLabels.reserve(7);
       te.weekdayCounts.reserve(7);
       for (int d = 0; d < 7; ++d) {
           te.weekdayLabels.append(weekdayShortName(d));       // single-sourced helper
           te.weekdayCounts.append(m_timeAnalytics.weekdayMonFirst.at(d));
       }

       te.busiestHourLabel = formatHourRange(m_timeAnalytics.peakHour);
       te.busiestDayLabel  = weekdayName(m_timeAnalytics.peakWeekdayMonFirst);
       return te;
   }
   ```

7. **Build + run the VM test to GREEN.**

   ```powershell
   cmake --build C:\b\loams-4bivb --target tst_reportingviewmodel
   ctest --test-dir C:\b\loams-4bivb -R tst_reportingviewmodel --output-on-failure
   ```

   All four new cases pass; `timeModels_populatedWithLabelBlanking` and the rest stay green (the refactor is behavior-preserving).

8. **Full build + full `ctest` to confirm no regression**, then commit.

   ```powershell
   cmake --build C:\b\loams-4bivb
   ctest --test-dir C:\b\loams-4bivb --output-on-failure
   ```

   Commit (scope `reporting`), e.g. `feat(reporting): add ReportTimeExport carrier + VM buildTimeExport() assembler`. Do not `git add -A`; stage only `reportdata.h`, `ReportingViewModel.{h,cpp}`, `tst_reportingviewmodel.cpp`.

### Deliverable

`buildTimeExport()` returns the correct carrier for Data / Empty / Error / defensive inputs; VM tests green; weekday short-names single-sourced. Renderer and export call sites NOT yet touched (they still take the old signatures — the VM's two export call sites are wired in Tasks 2 & 3).

---

## Task 2 — `writeReportToXlsx` signature + Excel When-block + wire the Excel call sites

Adds the trailing carrier parameter to the Excel writer, implements the side-by-side When-block on the Summary sheet, and wires the ViewModel + legacy Excel call sites. (The PDF path is Task 3.)

### Files

- `qt-app/core/reportrenderer.h` — append the carrier param to `writeReportToXlsx`.
- `qt-app/core/reportrenderer.cpp` — implement the Excel When-block.
- `qt-app/quick/viewmodels/ReportingViewModel.cpp` — append `, buildTimeExport()` to the `exportExcel` writer call.
- `qt-app/adminwindow.cpp` — append `, ReportTimeExport{}` to the one legacy `writeReportToXlsx` site.
- `qt-app/tests/tst_reportrenderer.cpp` — new Excel cases + update existing `writeReportToXlsx` calls.

### Interfaces

**Produces:**
- `static bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx, const QJsonArray &rows, const QJsonObject &filters, const ReportHeaderInfo &info, const ReportAnalytics &analytics, bool includeRoster, const ReportTimeExport &timeExport);`

**Consumes:**
- `ReportTimeExport` fields (Task 1); existing `sanitizeXlsxText`, `sectionFmt`, `hdrFmt`, the `row` cursor, and QXlsx `write(row, col, value[, fmt])`.
- `ReportingViewModel::buildTimeExport()` (Task 1).

### Steps

1. **Write the failing Excel tests first.** In `tst_reportrenderer.cpp`, add a Data-carrier fixture. Insert this static helper into the `private:` block (after `samplePalette()`):

   ```cpp
       // A Data-state carrier with VM-formatted labels seeded directly (this core
       // test does not link the ViewModel). Peak hour 14 -> "2–3 PM"; Monday busiest.
       static ReportTimeExport sampleTimeExportData() {
           ReportTimeExport t;
           t.state = TimeAnalyticsExportState::Data;
           static const char *const hourTicks[24] = {
               "12A","1A","2A","3A","4A","5A","6A","7A","8A","9A","10A","11A",
               "12P","1P","2P","3P","4P","5P","6P","7P","8P","9P","10P","11P" };
           for (int h = 0; h < 24; ++h) {
               t.hourLabels << QString::fromLatin1(hourTicks[h]);
               t.hourCounts << (h == 14 ? 12 : (h == 9 ? 3 : 0));
           }
           static const char *const days[7] = { "Mon","Tue","Wed","Thu","Fri","Sat","Sun" };
           const int dayCounts[7] = { 40, 8, 30, 8, 5, 1, 2 };
           for (int d = 0; d < 7; ++d) {
               t.weekdayLabels << QString::fromLatin1(days[d]);
               t.weekdayCounts << dayCounts[d];
           }
           t.busiestHourLabel = QStringLiteral("2–3 PM");
           t.busiestDayLabel  = QStringLiteral("Monday");
           return t;
       }
   ```

   Add these slot declarations after `writeReportToXlsx_sanitizesFormulaLeadingNames();`:

   ```cpp
       void writeReportToXlsx_timeBlock_dataStatePresent();
       void writeReportToXlsx_timeBlock_disabledStateAbsent();
       void writeReportToXlsx_timeBlock_emptyAndErrorNotesDiffer();
   ```

   Add the three implementations before `QTEST_MAIN(TstReportRenderer)`:

   ```cpp
   void TstReportRenderer::writeReportToXlsx_timeBlock_dataStatePresent() {
       QXlsx::Document xlsx;
       QVERIFY(ReportRenderer::writeReportToXlsx(
           xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(),
           sampleAnalytics(), false, sampleTimeExportData()));
       QVERIFY(xlsx.selectSheet("Summary"));

       bool foundTitle = false, foundPeakHour = false, foundBusiestDay = false;
       bool foundHourHdr = false, foundCountHdr = false, foundDayHdr = false;
       bool foundHourCell = false, foundDayCell = false;
       for (int r = 1; r <= 80; ++r) {
           for (int c = 1; c <= 8; ++c) {
               const QString cell = xlsx.read(r, c).toString();
               if (cell == "When do students visit?") foundTitle = true;
               if (cell == "Peak Hour: 2–3 PM") foundPeakHour = true;
               if (cell == "Busiest Day: Monday") foundBusiestDay = true;
               if (cell == "Hour") foundHourHdr = true;
               if (cell == "Count") foundCountHdr = true;
               if (cell == "Day") foundDayHdr = true;
               if (cell == "2P") foundHourCell = true;    // hourLabels[14], hourly col
               if (cell == "Mon") foundDayCell = true;    // weekdayLabels[0], weekday col
           }
       }
       QVERIFY(foundTitle);
       QVERIFY(foundPeakHour);
       QVERIFY(foundBusiestDay);
       QVERIFY(foundHourHdr);
       QVERIFY(foundCountHdr);
       QVERIFY(foundDayHdr);
       QVERIFY(foundHourCell);
       QVERIFY(foundDayCell);
   }

   void TstReportRenderer::writeReportToXlsx_timeBlock_disabledStateAbsent() {
       QXlsx::Document xlsx;
       QVERIFY(ReportRenderer::writeReportToXlsx(
           xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(),
           sampleAnalytics(), false, ReportTimeExport{}));   // default = Disabled
       QVERIFY(xlsx.selectSheet("Summary"));
       for (int r = 1; r <= 80; ++r)
           for (int c = 1; c <= 8; ++c)
               QVERIFY(xlsx.read(r, c).toString() != "When do students visit?");
   }

   void TstReportRenderer::writeReportToXlsx_timeBlock_emptyAndErrorNotesDiffer() {
       ReportTimeExport empty;  empty.state = TimeAnalyticsExportState::Empty;
       ReportTimeExport err;    err.state   = TimeAnalyticsExportState::Error;

       auto findNote = [](QXlsx::Document &x, const QString &needle) {
           x.selectSheet("Summary");
           for (int r = 1; r <= 80; ++r)
               for (int c = 1; c <= 8; ++c)
                   if (x.read(r, c).toString() == needle) return true;
           return false;
       };

       QXlsx::Document xe;
       QVERIFY(ReportRenderer::writeReportToXlsx(
           xe, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false, empty));
       QVERIFY(findNote(xe, "No visit activity in this range"));
       QVERIFY(!findNote(xe, "Visit-time data could not be loaded"));
       QVERIFY(!findNote(xe, "Hour"));   // no table headers in the Empty state

       QXlsx::Document xr;
       QVERIFY(ReportRenderer::writeReportToXlsx(
           xr, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false, err));
       QVERIFY(findNote(xr, "Visit-time data could not be loaded"));
       QVERIFY(!findNote(xr, "No visit activity in this range"));
   }
   ```

2. **Build `tst_reportrenderer` and watch it FAIL to compile.** The new calls pass 7 args to a 6-arg `writeReportToXlsx`. Expected failure: `no matching function for call to 'ReportRenderer::writeReportToXlsx(...)'` / `candidate expects 6 arguments, 7 provided`.

   ```powershell
   cmake --build C:\b\loams-4bivb --target tst_reportrenderer
   ```

3. **Add the parameter to the header.** In `reportrenderer.h`, find:

   ```cpp
       static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                     const QJsonArray &rows,
                                     const QJsonObject &filters,
                                     const ReportHeaderInfo &info,
                                     const ReportAnalytics &analytics, bool includeRoster);
   ```

   Replace with:

   ```cpp
       static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                     const QJsonArray &rows,
                                     const QJsonObject &filters,
                                     const ReportHeaderInfo &info,
                                     const ReportAnalytics &analytics, bool includeRoster,
                                     const ReportTimeExport &timeExport);
   ```

4. **Update the definition signature + implement the When-block.** In `reportrenderer.cpp`, find the definition head:

   ```cpp
   bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx,
                                          const QJsonArray &rows,
                                          const QJsonObject &filters,
                                          const ReportHeaderInfo &info,
                                          const ReportAnalytics &analytics,
                                          bool includeRoster)
   {
   ```

   Replace with:

   ```cpp
   bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx,
                                          const QJsonArray &rows,
                                          const QJsonObject &filters,
                                          const ReportHeaderInfo &info,
                                          const ReportAnalytics &analytics,
                                          bool includeRoster,
                                          const ReportTimeExport &timeExport)
   {
   ```

   Then find the tail of the ranking section, immediately after the third `writeRanking(...)` call and before the system-generated footer line:

   ```cpp
       writeRanking(QStringLiteral("Top 10 Departments"),
                    { "Rank", "Department", "Visits", "% of Total" }, analytics.topDepartments, false, true);

       xlsx.write(row++, 1,
                  "This is a system-generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");
   ```

   Insert the When-block between the third `writeRanking(...)` and the `xlsx.write(row++, 1, "This is a system-generated report...")` line:

   ```cpp
       // ===== WHEN? TIME ANALYTICS (spec 4b-iv-b §10) =====
       // Side-by-side tables on the Summary sheet, below the rankings. Every
       // label/caption/header cell runs through sanitizeXlsxText for a single
       // uniform escaping path (count cells are integers, no sanitize needed).
       switch (timeExport.state) {
       case TimeAnalyticsExportState::Disabled:
           break;   // legacy WITS.exe parity — write nothing
       case TimeAnalyticsExportState::Error:
       case TimeAnalyticsExportState::Empty: {
           xlsx.write(row++, 1, sanitizeXlsxText(QStringLiteral("When do students visit?")), sectionFmt);
           const QString note = (timeExport.state == TimeAnalyticsExportState::Error)
                                    ? QStringLiteral("Visit-time data could not be loaded")
                                    : QStringLiteral("No visit activity in this range");
           xlsx.write(row++, 1, sanitizeXlsxText(note));
           row += 1;
           break;
       }
       case TimeAnalyticsExportState::Data: {
           xlsx.write(row++, 1, sanitizeXlsxText(QStringLiteral("When do students visit?")), sectionFmt);

           // Peak-label row: hourly caption in col 1, weekday caption a few cols over.
           xlsx.write(row, 1, sanitizeXlsxText(QStringLiteral("Peak Hour: %1").arg(timeExport.busiestHourLabel)));
           xlsx.write(row, 4, sanitizeXlsxText(QStringLiteral("Busiest Day: %1").arg(timeExport.busiestDayLabel)));
           row++;

           // Two tables SIDE-BY-SIDE sharing one header row: hourly (cols 1-2, 24
           // rows) and weekday (cols 4-5, 7 rows). The single-cursor writeRanking
           // lambda can't drive two columns, so address cells directly by baseRow.
           const int baseRow = row;
           xlsx.write(baseRow, 1, sanitizeXlsxText(QStringLiteral("Hour")),  hdrFmt);
           xlsx.write(baseRow, 2, sanitizeXlsxText(QStringLiteral("Count")), hdrFmt);
           xlsx.write(baseRow, 4, sanitizeXlsxText(QStringLiteral("Day")),   hdrFmt);
           xlsx.write(baseRow, 5, sanitizeXlsxText(QStringLiteral("Count")), hdrFmt);
           for (int i = 0; i < timeExport.hourLabels.size(); ++i) {
               xlsx.write(baseRow + 1 + i, 1, sanitizeXlsxText(timeExport.hourLabels.at(i)));
               xlsx.write(baseRow + 1 + i, 2, timeExport.hourCounts.at(i));
           }
           for (int i = 0; i < timeExport.weekdayLabels.size(); ++i) {
               xlsx.write(baseRow + 1 + i, 4, sanitizeXlsxText(timeExport.weekdayLabels.at(i)));
               xlsx.write(baseRow + 1 + i, 5, timeExport.weekdayCounts.at(i));
           }
           // Advance the cursor past the TALLER (24-row hourly) table so the
           // system-generated footer that follows lands below the whole block.
           row = baseRow + 1 + timeExport.hourCounts.size();
           row += 1;
           break;
       }
       }

   ```

5. **Update the existing renderer test calls to the new signature.** In `tst_reportrenderer.cpp`, every pre-existing `writeReportToXlsx(...)` call must append the Disabled default so those cases keep asserting the pre-slice content. Update each of these calls to add `, ReportTimeExport{}` as the final argument:
   - in `writeReportToXlsx_populatesCells` (`..., sampleAnalytics(), true)` → `..., sampleAnalytics(), true, ReportTimeExport{})`)
   - both calls in `writeReportToXlsx_rosterRowsPresentOnlyWhenIncluded` (`..., false)` and `..., true)`)
   - in `writeReportToXlsx_summarySheetHasKpisAndRankings` (`..., false)`)
   - in `writeReportToXlsx_rosterOnSeparateSheetWhenIncluded` (`..., true)`)
   - in `writeReportToXlsx_sanitizesFormulaLeadingNames` (`..., analytics, true)`)

   (Each is the last argument before the closing `)`; append `, ReportTimeExport{}`.)

6. **Wire the ViewModel Excel call site.** In `ReportingViewModel.cpp`, find the `exportExcel` writer call:

   ```cpp
           const bool ok = ReportRenderer::writeReportToXlsx(
                               doc, m_exportRows, currentExportFilters(), headerInfo(),
                               m_analytics, m_includeRosterInExport)
                           && doc.saveAs(path);
   ```

   Replace with:

   ```cpp
           const bool ok = ReportRenderer::writeReportToXlsx(
                               doc, m_exportRows, currentExportFilters(), headerInfo(),
                               m_analytics, m_includeRosterInExport, buildTimeExport())
                           && doc.saveAs(path);
   ```

7. **Wire the legacy Excel call site.** In `adminwindow.cpp`, find:

   ```cpp
       m_reportRenderer.writeReportToXlsx(xlsx, rows, filters, info,
                                          ReportAnalytics::compute(rows), true);
   ```

   Replace with:

   ```cpp
       m_reportRenderer.writeReportToXlsx(xlsx, rows, filters, info,
                                          ReportAnalytics::compute(rows), true, ReportTimeExport{});
   ```

8. **Build + run `tst_reportrenderer` to GREEN**, then a full build + `ctest`.

   ```powershell
   cmake --build C:\b\loams-4bivb --target tst_reportrenderer
   ctest --test-dir C:\b\loams-4bivb -R tst_reportrenderer --output-on-failure
   cmake --build C:\b\loams-4bivb
   ctest --test-dir C:\b\loams-4bivb --output-on-failure
   ```

   The three new Excel cases pass; existing renderer/VM tests stay green; both `WITS` and `WITSQuick` still link (adminwindow + ViewModel call sites updated).

9. **Commit** (scope `reporting`), e.g. `feat(reporting): render When? time analytics into the Excel export`. Stage only the touched files (`reportrenderer.{h,cpp}`, `ReportingViewModel.cpp`, `adminwindow.cpp`, `tst_reportrenderer.cpp`).

### Deliverable

The Excel export carries the side-by-side When-block on the Summary sheet (Data), or the correct distinct note (Empty/Error), or nothing (Disabled/legacy). Renderer Excel tests green; full build green.

---

## Task 3 — `paintReport` signature + two chart makers + PDF When-section + wire the PDF call sites

Adds the two screen-safe bar-chart makers, the trailing carrier parameter to `paintReport`, the PDF When-section, and wires the ViewModel + legacy PDF/print call sites.

### Files

- `qt-app/core/reportrenderer.h` — declare the two makers; append the carrier param to `paintReport`.
- `qt-app/core/reportrenderer.cpp` — implement the two makers + the PDF When-section.
- `qt-app/quick/viewmodels/ReportingViewModel.cpp` — append `, buildTimeExport()` to the `renderToDevice` `paintReport` call.
- `qt-app/adminwindow.cpp` — append `, ReportTimeExport{}` to the two legacy `paintReport` sites.
- `qt-app/tests/tst_reportrenderer.cpp` — two maker cases + update existing `paintReport` calls.

### Interfaces

**Produces:**
- `static QImage ReportRenderer::makeHourlyBarChartImage(const ReportTimeExport &t, QSize size, const ReportPalette &palette);`
- `static QImage ReportRenderer::makeWeekdayBarChartImage(const ReportTimeExport &t, QSize size, const ReportPalette &palette);`
- `static bool ReportRenderer::paintReport(QPagedPaintDevice *device, int resolution, const QJsonArray &data, const QJsonObject &filters, const ReportPalette &palette, const ReportHeaderInfo &info, const ReportAnalytics &analytics, bool includeRoster, const ReportTimeExport &timeExport);`

**Consumes:**
- existing `chartImageSize(usableWidth, false)`, `renderChartToImage(chart, size)`, the `drawFullscreenChart` / `drawFooter` / `drawHeader` lambdas, `vs(...)`, and the `QChart`/`QBarSet`/`QBarSeries`/`QBarCategoryAxis`/`QValueAxis` headers already included.
- `ReportingViewModel::buildTimeExport()` (Task 1).

### Steps

1. **Write the two failing maker tests.** In `tst_reportrenderer.cpp`, add slot declarations after the Task-2 slots:

   ```cpp
       void makeHourlyBarChartImage_nonBlankAtScreenSafeSize();
       void makeWeekdayBarChartImage_nonBlankAtScreenSafeSize();
   ```

   Add a not-uniformly-white helper into the `private:` block (after `sampleTimeExportData()`):

   ```cpp
       static bool imageHasNonWhitePixel(const QImage &img) {
           const QImage rgb = img.convertToFormat(QImage::Format_ARGB32);
           for (int y = 0; y < rgb.height(); ++y)
               for (int x = 0; x < rgb.width(); ++x)
                   if (rgb.pixelColor(x, y) != QColor(Qt::white)) return true;
           return false;
       }
   ```

   Add the two implementations before `QTEST_MAIN(TstReportRenderer)`:

   ```cpp
   void TstReportRenderer::makeHourlyBarChartImage_nonBlankAtScreenSafeSize() {
       const QSize sz = ReportRenderer::chartImageSize(9000, false);   // screen-safe size
       const QImage img = ReportRenderer::makeHourlyBarChartImage(
           sampleTimeExportData(), sz, samplePalette());
       QVERIFY(!img.isNull());
       QCOMPARE(img.size(), sz);                    // MUST render at the screen-safe size
       QVERIFY(imageHasNonWhitePixel(img));         // real bars, not a blank raster
   }

   void TstReportRenderer::makeWeekdayBarChartImage_nonBlankAtScreenSafeSize() {
       const QSize sz = ReportRenderer::chartImageSize(9000, false);
       const QImage img = ReportRenderer::makeWeekdayBarChartImage(
           sampleTimeExportData(), sz, samplePalette());
       QVERIFY(!img.isNull());
       QCOMPARE(img.size(), sz);
       QVERIFY(imageHasNonWhitePixel(img));
   }
   ```

2. **Build `tst_reportrenderer` and watch it FAIL to compile.** `makeHourlyBarChartImage` / `makeWeekdayBarChartImage` do not exist yet. Expected failure: `'makeHourlyBarChartImage' is not a member of 'ReportRenderer'`.

   ```powershell
   cmake --build C:\b\loams-4bivb --target tst_reportrenderer
   ```

3. **Declare the two makers in the header (leave `paintReport` UNTOUCHED for now).** In `reportrenderer.h`, find:

   ```cpp
       static QImage makeLineChartImage(const QJsonArray &data, QSize size,
                                        const ReportPalette &palette,
                                        int openHour, int closeHour);
   ```

   Insert directly below it:

   ```cpp

       // "When?" bar-chart makers (spec 4b-iv-b §8.2). Structurally identical to
       // makeBarChartImage; sized from chartImageSize(usableWidth,false) and
       // upscaled by drawFullscreenChart (NEVER an arbitrary/print size — the
       // QChartView screen-clamp hazard, §8.3). Hourly shows an x-label only every
       // 3rd category; weekday shows all 7 (Mon→Sun). Peak caption rides in the title.
       static QImage makeHourlyBarChartImage(const ReportTimeExport &t, QSize size,
                                             const ReportPalette &palette);
       static QImage makeWeekdayBarChartImage(const ReportTimeExport &t, QSize size,
                                              const ReportPalette &palette);
   ```

   **Do NOT change the `paintReport` declaration in this step.** Its signature change is deferred to step 6 so it lands TOGETHER with its `.cpp` definition change. If the header declared 9-arg `paintReport` now while the `.cpp` still defined the 8-arg version, `reportrenderer.cpp` would fail to compile at step 5's maker-test build (`out-of-line definition of 'paintReport' does not match any declaration`).

4. **Implement the two makers.** In `reportrenderer.cpp`, insert both definitions immediately after `makeLineChartImage`'s closing brace (before the `paintReport` port comment block). Hourly maker:

   ```cpp
   // --- Hourly Bar Chart ("When?" — 24 bars, 3-hour x-labels) ---
   // Mirrors makeBarChartImage. The x-axis shows a label only on every 3rd
   // category (by POSITION); the maker never re-derives an hour string — it only
   // chooses which of the VM's finished labels to display (spec §5/§8.2). The peak
   // caption rides in the chart TITLE because drawFullscreenChart exposes no seam
   // to place a caption below the image (§8.4).
   QImage ReportRenderer::makeHourlyBarChartImage(const ReportTimeExport &t, QSize size,
                                                  const ReportPalette &palette) {
       const int h = size.height();
       QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
       QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

       QBarSet *set = new QBarSet("Visits");
       QStringList categories;
       for (int i = 0; i < t.hourCounts.size(); ++i) {
           *set << t.hourCounts.at(i);
           categories << ((i % 3 == 0 && i < t.hourLabels.size()) ? t.hourLabels.at(i) : QString());
       }
       set->setBrush(palette.chartColors.isEmpty() ? QBrush(palette.headerBg)
                                                   : QBrush(palette.chartColors.first()));

       QBarSeries *series = new QBarSeries();
       series->append(set);

       QChart *chart = new QChart();
       chart->addSeries(series);
       chart->setTitle(QStringLiteral("Peak Hour: %1").arg(t.busiestHourLabel));
       chart->setTitleFont(titleFont);

       QBarCategoryAxis *axisX = new QBarCategoryAxis();
       axisX->append(categories);
       axisX->setLabelsFont(labelFont);
       chart->addAxis(axisX, Qt::AlignBottom);
       series->attachAxis(axisX);

       QValueAxis *axisY = new QValueAxis();
       axisY->setTitleText("Number of Visits");
       axisY->setLabelsFont(labelFont);
       axisY->setTitleFont(labelFont);
       chart->addAxis(axisY, Qt::AlignLeft);
       series->attachAxis(axisY);

       chart->legend()->setVisible(false);
       chart->setMargins(QMargins(0, 0, 0, 0));
       chart->layout()->setContentsMargins(0, 0, 0, 0);
       chart->setBackgroundRoundness(0);

       return renderChartToImage(chart, size);
   }
   ```

   Weekday maker (all 7 labels shown, Mon→Sun; title `"Busiest Day: %1"`):

   ```cpp
   // --- Weekday Bar Chart ("When?" — 7 bars, Mon→Sun) ---
   // Mirrors makeBarChartImage; the carrier's weekdayLabels are already Mon→Sun so
   // all 7 labels are shown. Peak caption rides in the chart TITLE (§8.4).
   QImage ReportRenderer::makeWeekdayBarChartImage(const ReportTimeExport &t, QSize size,
                                                   const ReportPalette &palette) {
       const int h = size.height();
       QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
       QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

       QBarSet *set = new QBarSet("Visits");
       QStringList categories;
       for (int i = 0; i < t.weekdayCounts.size(); ++i) {
           *set << t.weekdayCounts.at(i);
           categories << (i < t.weekdayLabels.size() ? t.weekdayLabels.at(i) : QString());
       }
       set->setBrush(palette.chartColors.isEmpty() ? QBrush(palette.headerBg)
                                                   : QBrush(palette.chartColors.first()));

       QBarSeries *series = new QBarSeries();
       series->append(set);

       QChart *chart = new QChart();
       chart->addSeries(series);
       chart->setTitle(QStringLiteral("Busiest Day: %1").arg(t.busiestDayLabel));
       chart->setTitleFont(titleFont);

       QBarCategoryAxis *axisX = new QBarCategoryAxis();
       axisX->append(categories);
       axisX->setLabelsFont(labelFont);
       chart->addAxis(axisX, Qt::AlignBottom);
       series->attachAxis(axisX);

       QValueAxis *axisY = new QValueAxis();
       axisY->setTitleText("Number of Visits");
       axisY->setLabelsFont(labelFont);
       axisY->setTitleFont(labelFont);
       chart->addAxis(axisY, Qt::AlignLeft);
       series->attachAxis(axisY);

       chart->legend()->setVisible(false);
       chart->setMargins(QMargins(0, 0, 0, 0));
       chart->layout()->setContentsMargins(0, 0, 0, 0);
       chart->setBackgroundRoundness(0);

       return renderChartToImage(chart, size);
   }
   ```

5. **Build + run the two maker cases to GREEN** (they don't need `paintReport` yet).

   ```powershell
   cmake --build C:\b\loams-4bivb --target tst_reportrenderer
   ctest --test-dir C:\b\loams-4bivb -R tst_reportrenderer --output-on-failure
   ```

   Note: the existing `paintReport` test calls still pass 8 args to the (still-8-arg) `paintReport`, so the target still compiles here — the maker tests go green before the signature change. The `paintReport` signature change happens next (step 6) and breaks those calls, which step 8 fixes.

6. **Extend the `paintReport` signature — header declaration AND `.cpp` definition TOGETHER.** First, in `reportrenderer.h`, find the `paintReport` declaration:

   ```cpp
       static bool paintReport(QPagedPaintDevice *device, int resolution,
                               const QJsonArray &data, const QJsonObject &filters,
                               const ReportPalette &palette,
                               const ReportHeaderInfo &info,
                               const ReportAnalytics &analytics, bool includeRoster);
   ```

   Replace with:

   ```cpp
       static bool paintReport(QPagedPaintDevice *device, int resolution,
                               const QJsonArray &data, const QJsonObject &filters,
                               const ReportPalette &palette,
                               const ReportHeaderInfo &info,
                               const ReportAnalytics &analytics, bool includeRoster,
                               const ReportTimeExport &timeExport);
   ```

   Then, in `reportrenderer.cpp`, find the definition head:

   ```cpp
   bool ReportRenderer::paintReport(QPagedPaintDevice *device, int resolution,
                                    const QJsonArray &data, const QJsonObject &filters,
                                    const ReportPalette &palette,
                                    const ReportHeaderInfo &info,
                                    const ReportAnalytics &analytics, bool includeRoster)
   {
   ```

   Replace with:

   ```cpp
   bool ReportRenderer::paintReport(QPagedPaintDevice *device, int resolution,
                                    const QJsonArray &data, const QJsonObject &filters,
                                    const ReportPalette &palette,
                                    const ReportHeaderInfo &info,
                                    const ReportAnalytics &analytics, bool includeRoster,
                                    const ReportTimeExport &timeExport)
   {
   ```

   Change the header declaration and the `.cpp` definition in the SAME step so the class never has a 9-arg declaration without the matching definition (nor vice-versa). After this step, the existing 8-arg `paintReport` test calls no longer match and break the build — step 8 updates them.

7. **Insert the PDF When-section BEFORE the terminal `drawFooter`.** In `paintReport`, find the end of the chart if/else block and the terminal footer:

   ```cpp
       } else {
           // For bar and line charts, use rectangular size
           QSize rectSize = chartImageSize(usableWidth, false);
           if (chartChoice.contains("Bar", Qt::CaseInsensitive)) {
               drawFullscreenChart("Bar Chart", makeBarChartImage(data, rectSize, palette));
           } else if (chartChoice.contains("Line", Qt::CaseInsensitive)) {
               drawFullscreenChart("Line Chart", makeLineChartImage(data, rectSize, palette, info.openHour, info.closeHour));
           }
       }

       // Footer on the last page with current page number
       drawFooter(currentPage);
   ```

   Insert the When-section between the closing `}` of the chart if/else and the `// Footer on the last page` comment (so the terminal footer foots the last When? page):

   ```cpp
       // ===== WHEN? TIME ANALYTICS (spec 4b-iv-b §8.4) =====
       // Inserted BEFORE the terminal drawFooter so that footer foots the LAST
       // When? page. Placement note: drawFullscreenChart foots the PRIOR page at
       // ENTRY (never its own page at exit) and declares a local `y` shadowing the
       // outer one, exposing no seam for a caption — so the Data path's captions
       // ride in each chart TITLE, and the Empty/Error note (which has no
       // entry-foot of its own) must foot the current page itself before paging.
       switch (timeExport.state) {
       case TimeAnalyticsExportState::Disabled:
           break;   // legacy WITS.exe parity — draw nothing, advance no page
       case TimeAnalyticsExportState::Data: {
           const QSize whenSize = chartImageSize(usableWidth, false);   // screen-safe; upscaled by drawFullscreenChart
           drawFullscreenChart("Hourly Visits",
                               makeHourlyBarChartImage(timeExport, whenSize, palette));
           drawFullscreenChart("Visits by Day",
                               makeWeekdayBarChartImage(timeExport, whenSize, palette));
           break;
       }
       case TimeAnalyticsExportState::Empty:
       case TimeAnalyticsExportState::Error: {
           // The note can't share the last course-chart page (that page holds a
           // chart image and the outer y sits near the header). Foot the current
           // page FIRST, then open a fresh page and draw the note in normal y-flow.
           drawFooter(currentPage);
           device->newPage();
           currentPage++;
           y = margin;
           drawHeader(y);
           painter.setFont(QFont("Arial", 13, QFont::Bold));
           painter.setPen(Qt::black);
           painter.drawText(QRect(margin, y, usableWidth, vs(24)), Qt::AlignLeft,
                            "When do students visit?");
           y += vs(30);
           painter.setFont(QFont("Arial", 11));
           const QString note = (timeExport.state == TimeAnalyticsExportState::Error)
                                    ? QStringLiteral("Visit-time data could not be loaded")
                                    : QStringLiteral("No visit activity in this range");
           painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft, note);
           y += vs(24);
           break;
       }
       }

   ```

8. **Update the existing renderer `paintReport` test calls.** In `tst_reportrenderer.cpp`, append `, ReportTimeExport{}` as the final argument to each pre-existing `paintReport(...)` call so those cases keep exercising the Disabled (no-When-section) path:
   - `paintReport_writesPdf` (`..., sampleAnalytics(), true)`)
   - `paintReport_writesPdfWithAndWithoutRoster` (`..., sampleHeaderInfo(), sampleAnalytics(), includeRoster)`)
   - `paintReport_writesAnalyticsPdfAtHighDpi` (`..., sampleHeaderInfo(), sampleAnalytics(), includeRoster)`)

9. **Wire the ViewModel PDF/print call site.** In `ReportingViewModel.cpp`, find `renderToDevice`:

   ```cpp
   bool ReportingViewModel::renderToDevice(QPagedPaintDevice *dev, int resolution)
   {
       const QJsonObject filters = currentExportFilters();
       const ReportPalette pal = ReportController::getPalette(m_palette);
       return ReportRenderer::paintReport(dev, resolution, m_exportRows, filters, pal,
                                          headerInfo(), m_analytics, m_includeRosterInExport);
   }
   ```

   Replace the `return` statement with:

   ```cpp
       return ReportRenderer::paintReport(dev, resolution, m_exportRows, filters, pal,
                                          headerInfo(), m_analytics, m_includeRosterInExport,
                                          buildTimeExport());
   ```

   (Both `exportPdf` and `printReport` route through `renderToDevice`, so print output gets the section for free.)

10. **Wire the two legacy PDF/print call sites.** In `adminwindow.cpp`, find and update both `paintReport` calls:

    ```cpp
        if (!m_reportRenderer.paintReport(&pdf, 150, data, filters, palette, info,
                                          ReportAnalytics::compute(data), true)) {
    ```

    →

    ```cpp
        if (!m_reportRenderer.paintReport(&pdf, 150, data, filters, palette, info,
                                          ReportAnalytics::compute(data), true, ReportTimeExport{})) {
    ```

    and

    ```cpp
        if (!m_reportRenderer.paintReport(&printer, printer.resolution(), data, filters, palette, info,
                                          ReportAnalytics::compute(data), true)) {
    ```

    →

    ```cpp
        if (!m_reportRenderer.paintReport(&printer, printer.resolution(), data, filters, palette, info,
                                          ReportAnalytics::compute(data), true, ReportTimeExport{})) {
    ```

11. **Build + run `tst_reportrenderer` to GREEN**, then full build + `ctest`.

    ```powershell
    cmake --build C:\b\loams-4bivb --target tst_reportrenderer
    ctest --test-dir C:\b\loams-4bivb -R tst_reportrenderer --output-on-failure
    cmake --build C:\b\loams-4bivb
    ctest --test-dir C:\b\loams-4bivb --output-on-failure
    ```

    Both maker cases pass; existing renderer/VM tests stay green; both `WITS` and `WITSQuick` link.

12. **Commit** (scope `reporting`), e.g. `feat(reporting): render When? time analytics into the PDF export`. Stage only the touched files.

### Deliverable

The PDF export carries the "When do students visit?" section — two upscaled bar-chart images with peak-caption titles (Data), or a footed note page (Empty/Error), or nothing (Disabled/legacy). Renderer image-maker tests green; full build green.

---

## Task 4 — Manual `WITSQuick.exe` export smoke (RELEASE GATE, owner-run, no code)

OFFSCREEN tests cannot validate real chart appearance — the `QChartView` screen-clamp of §8.3 is invisible without a physical screen. This gate is mandatory before the slice is considered done. **No code changes.**

### Steps

1. **Build the app and CLOSE any running `WITSQuick.exe` first** (a live process locks the binary and blocks relink):

   ```powershell
   cmake --build C:\b\loams-4bivb
   ```

   Then launch `C:\b\loams-4bivb\quick\WITSQuick.exe` → open the Reporting screen.

2. **Data range** — pick a range with real visit activity, Generate, then export **both** PDF and Excel:
   - PDF: the hourly (24-bar) and weekday (7-bar) charts render correctly — **readable fonts, real bars, NOT giant-font/blank** (the screen-clamp failure mode). Titles read `Peak Hour: <…>` and `Busiest Day: <…>` matching the on-screen captions. Hourly x-axis shows ~every-3rd label.
   - Excel: the Summary sheet has the "When do students visit?" section below the rankings, with the side-by-side Hourly (Hour/Count, 24 rows) and Visits-by-Day (Day/Count, 7 rows) tables reading across, plus the two peak-label cells.

3. **Empty range** — pick a range with zero activity, export both:
   - Both show **"No visit activity in this range"**, no charts/tables, and the rest of the report (KPIs, rankings, course chart, roster) is intact.

4. **Simulated time-fetch failure** — make only the time endpoint fail (e.g. point it at a bad URL / stop the backend for the time call only), Generate, export both:
   - Both show **"Visit-time data could not be loaded"** — **distinct** from the empty-range note — while the primary report still renders fully.

5. **Legacy parity** — run `WITS.exe` (legacy widgets), export PDF + Excel, confirm there is **NO "When?" section** anywhere (byte-for-byte legacy output).

6. **On any failure, route through `superpowers:systematic-debugging`** (reproduce → isolate → fix under TDD) before re-exporting.

### Deliverable

Owner-confirmed screenshots/notes for all three ranges × both formats + the legacy-parity check. This gate closes the 4b-iv arc.

---

## Post-build gate

After Task 3 (all automated green) and before finishing the branch: run `/claude-review` on the slice per the project workflow, fix Critical/Important findings, and re-submit until APPROVE or the 3-round cap. Task 4's manual smoke is the release gate the automated suite structurally cannot replace. Then `superpowers:finishing-a-development-branch` → project `create-pr`.
