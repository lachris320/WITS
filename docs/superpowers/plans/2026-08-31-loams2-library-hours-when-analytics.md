# Library-Hours Windowing for the "When?" Hourly Analytics — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. **Line numbers in this plan are current-ish snapshots and WILL drift** — locate every edit by the surrounding code/context quoted here, not by the line number alone.

**Goal:** Crop the "When?" hourly time-analytics — on screen *and* in the PDF/Excel exports — to the librarian-set library-hours window `[openHour, closeHour]` inclusive, instead of a fixed 24 hours, so the hourly chart shows only the hours the library is actually open, with every hour labeled and the peak-hour caption naming a bar that is actually drawn. This closes open follow-up #3 from the 4b-iv-b smoke. Pure client-side crop: **no backend change, no new endpoint, no new source files, no new CMake targets.** The weekday chart, KPIs, rankings, roster, totals, and `hasData` semantics are untouched.

**Architecture:** The existing "When?" layering is preserved exactly (spec §5/§9): **core owns the windowed peak scan; the ViewModel owns hour-label formatting, the presentation arrays, and caching the window; the renderer does zero hour math.**

```
AppSettings: library/openHour (7), library/closeHour (21)
     │  read once, via headerInfo(), when analytics arrive
     ▼
ReportingViewModel::onTimeAnalyticsReady(byHour[24], byWeekday[7])
     │  caches m_openHour / m_closeHour  ── parity anchor ──┐
     ▼                                                       │
TimeAnalytics::compute(byHour, byWeekday, open, close)       │
     │  • hourly[24] = byHour  (RAW, uncropped)              │
     │  • peakHour/peakHourCount scanned over [open..close] ONLY (decision 1)
     │  • clampLibraryHours() fallback open<0|close>23|open>close → 0..23 (decision 4)
     │  • weekdayMonFirst / peakWeekday / hasData — UNCHANGED
     ▼                                                       │
m_timeAnalytics : TimeAnalytics                              │
     ├──► buildHourlyBars(hourly, open, close) ──────────────┤  screen
     │       → [open..close] bars, EVERY label (decision 2/3)
     │       hour caption gated on peakHourCount > 0 (decision 5)
     │       → ReportingScreen.qml "When do students visit?"
     └──► buildTimeExport() const  (reads cached m_openHour/close)
             → ReportTimeExport.hourLabels/hourCounts = [open..close] only
             → busiestHourLabel empty when peakHourCount == 0
                  ├──► makeHourlyBarChartImage (PDF: fewer bars, all labels — NO renderer change)
                  └──► Excel hourly table  (fewer rows; cursor advances past TALLER table)

adminwindow.cpp → ReportTimeExport{} (Disabled) → section omitted; compute never called → WITS.exe unchanged
```

**Tech Stack:** Qt 6.11.1 / C++17 / CMake + Ninja; QtCharts, QXlsx (vendored), Qt Test; MVVM (ViewModel is the only QML-facing C++).

**Spec (source of truth):** `docs/superpowers/specs/2026-08-31-loams2-library-hours-when-analytics-design.md` (owner-approved, claude-review APPROVED). Every §-decision is binding; the five locked decisions in §4 are the tie-breaker.

---

## Global Constraints

**Build (PowerShell; Qt tools are NOT on PATH; use a SHORT external build dir to avoid the Windows MAX_PATH overflow on the QML-module autogen path):**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S "<worktree>\qt-app" -B C:\b\loams-lh -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:\b\loams-lh
ctest --test-dir C:\b\loams-lh --output-on-failure
```

- **Baseline once at branch start:** configure + build + `ctest`, and record the green test count. Do **NOT** hard-code the number — capture whatever the baseline reports and require it to stay green (plus the new cases) at every task boundary.
- **`tst_settingsviewmodel` is a known flake** under full-suite parallel load. If it fails, re-run it alone (`ctest --test-dir C:\b\loams-lh -R tst_settingsviewmodel --output-on-failure`) before treating it as real.
- **No CMakeLists changes are expected** — this slice adds no files and no targets. It only extends existing signatures/bodies in files the current test targets already compile. **Verify, don't assume:** `tst_timeanalytics` already lists `core/timeanalytics.cpp` in its `SOURCES`; `tst_reportrenderer` already compiles `reportrenderer.cpp` + `reportanalytics.cpp` + `reportdata.h` directly; `tst_reportingviewmodel` links `witsquickmodule` → `witscore` (so it sees `TimeAnalytics` and `ReportingViewModel.cpp`). If a build fails for an undefined-reference reason, STOP and re-check — do not "fix" it by editing CMake unless a genuine missing-source is proven.
- **Close any running `WITSQuick.exe` before rebuilding** — a live process locks the binary and breaks relink.
- **Both `WITS.exe` and `WITSQuick.exe` must link at every task boundary.** `adminwindow.cpp` (legacy WITS.exe) passes a default `ReportTimeExport{}` and never calls `TimeAnalytics::compute`, so it needs no edit — but confirm it still links after every task.
- **Ignore** the "LF will be replaced by CRLF" warnings and the pre-existing QXlsx "GuiPrivate" deprecation warnings — they are not introduced by this slice.
- **Formatting/ownership boundary (binding, spec §9):** core receives `openHour/closeHour` as plain ints and returns indices/counts only — it never turns `14` into `"2–3 PM"`. The ViewModel owns every hour string and decides which hours to emit + the caption-suppression rule. The renderer paints exactly what the carrier holds and wraps the VM's finished peak *value* in fixed prose — zero hour math, zero window arithmetic.
- **Single-source the decision-4 clamp:** exactly ONE shared helper (`clampLibraryHours`, added in Task 1) is used by `compute`, `buildHourlyBars`, and `buildTimeExport`. Never hand-copy the `if (lo < 0 || hi > 23 || lo > hi) { lo = 0; hi = 23; }` expression into more than that one helper — a drifted copy desyncs the peak from the bars.
- **Arrival-time window caching is a CORRECTNESS requirement, not a nicety.** `compute` scans the peak using the window cached in `onTimeAnalyticsReady`; the bars that `buildHourlyBars`/`buildTimeExport` emit later MUST use that same cached window. Do NOT replace the cached `m_openHour`/`m_closeHour` members with a fresh `headerInfo()` read inside the const `buildTimeExport()` — that would desync the peak from the bars whenever Settings changed mid-session.
- **Preserve the EN DASH U+2013 verbatim** in every hour-range literal (`"2–3 PM"`, `"7 AM–…"`). A hyphen-minus (`-`) substitution silently fails the caption assertions. Copy the strings in this plan character-for-character.
- **Known/accepted asymmetry:** within one PDF, the per-course line chart (`makeLineChartImage`) reads `headerInfo()` live while the "When?" chart uses the window cached at Generate — so if the librarian changes hours between Generate and Export, their x-ranges can differ. Rare, cosmetic, documented in spec §7; do NOT "fix" it by re-reading `headerInfo()` in the export path (that reintroduces the peak/bar desync).
- **Commit each task** via the `commit` skill with a Conventional Commit (scope `reporting`). Do **NOT** `git add -A` — stage only the files the task touched. Project convention: **NO** Claude/Anthropic co-author trailer.
- **No real student PII** in any test/fixture — synthetic data only.

## Cross-task type / name consistency (use these EXACT names everywhere)

- `inline void clampLibraryHours(int &lo, int &hi);` — free function in `qt-app/core/timeanalytics.h`.
- `static TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour, const QList<int> &byWeekdaySunFirst, int openHour, int closeHour);`
- `static QList<BarsModel::Bar> ReportingViewModel::buildHourlyBars(const QList<int> &hourly, int openHour, int closeHour);`
- ViewModel members: `int m_openHour = 7;` / `int m_closeHour = 21;`
- Empty-hour-caption export fallback title: `"Hourly Visits"` (used in `peakHourCaption`).

---

## File-touched map

| File | Change | Task |
|---|---|---|
| `qt-app/core/timeanalytics.h` | add `clampLibraryHours` inline helper; `compute` signature gains `int openHour, int closeHour` | 1 |
| `qt-app/core/timeanalytics.cpp` | clamp + windowed peak-hour scan; `hourly`/weekday/`hasData` unchanged | 1 |
| `qt-app/tests/tst_timeanalytics.cpp` | existing 7 cases pass `0,23`; +5 windowed cases | 1 |
| `qt-app/quick/viewmodels/ReportingViewModel.h` | `buildHourlyBars` 3-arg; new `m_openHour`/`m_closeHour` members | 2 |
| `qt-app/quick/viewmodels/ReportingViewModel.cpp` | T1: behavior-preserving `0,23` bump at the compute call. T2: cache window in `onTimeAnalyticsReady`; window `buildHourlyBars` (drop `h%3`) + `buildTimeExport`; gate hour caption on `peakHourCount > 0` | 1, 2 |
| `qt-app/quick/tests/tst_reportingviewmodel.cpp` | T1: `0,23` bump at the `:848` reference call. T2: windowed on-screen + export assertions incl. the `:848` slice rewrite; +windowed-peak / all-out-of-hours cases | 1, 2 |
| `qt-app/core/reportrenderer.cpp` | Excel cursor `qMax`; `peakHourCaption` empty-value fallback; **no change** to `makeHourlyBarChartImage` | 3 |
| `qt-app/tests/tst_reportrenderer.cpp` | `sampleTimeExportData()` → windowed carrier; narrow-window Excel cursor test | 3 |
| `qt-app/quick/qml/admin/ReportingScreen.qml` | `busiestHourCaption` visibility gated on `busiestHourLabel.length > 0` | 4 |
| `qt-app/adminwindow.cpp` | **UNCHANGED** — still `ReportTimeExport{}`; never calls `compute` (recorded, verified) | — |

---

## Task 1 — Core: shared clamp helper + windowed `compute`; ALL call sites pass `0,23` (behavior-preserving)

Adds the capability without turning the window on anywhere. `compute` gains the window and scans the peak over it, but every production and test call site passes the FULL range `0, 23`, so at the end of this task the app behaves byte-identically to today (full 24h, peak over all 24). New unit cases prove the windowing capability by passing explicit narrow windows.

### Files

- Modify: `qt-app/core/timeanalytics.h` — add `clampLibraryHours`; extend `compute` signature.
- Modify: `qt-app/core/timeanalytics.cpp` — clamp + windowed peak scan.
- Modify: `qt-app/tests/tst_timeanalytics.cpp` — pass `0,23` to existing cases; add 5 windowed cases.
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` — Step 5 behavior-preserving `0,23` bump at the `onTimeAnalyticsReady` compute call (Task 2 swaps in the cached window).
- Modify: `qt-app/quick/tests/tst_reportingviewmodel.cpp` — Step 5 behavior-preserving `0,23` bump at the `:848` reference call (Task 2 rewrites the assertion).

### Interfaces

**Produces:**
- `inline void clampLibraryHours(int &lo, int &hi);` — if `lo < 0 || hi > 23 || lo > hi`, resets to `lo = 0; hi = 23;`. Single source of the decision-4 fallback.
- `static TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour, const QList<int> &byWeekdaySunFirst, int openHour, int closeHour);` — `hourly` still returns the RAW 24-wide array; `peakHour`/`peakHourCount` are scanned over `[clamp(openHour,closeHour)]` inclusive, earliest-bucket tie-break preserved; weekday reorder, `peakWeekday*`, and `hasData` unchanged.

**Consumes (unchanged):** `QList<int>`.

### Steps

- [ ] **Step 1: Update the existing `tst_timeanalytics` calls to the new 4-arg signature, and add the 5 windowed cases (RED).**

In `qt-app/tests/tst_timeanalytics.cpp`, every existing `TimeAnalytics::compute(...)` call passes two args. Append `, 0, 23` to each so their assertions stay equivalent (full-range peak). There are **nine** `compute(...)` calls across **seven** methods: one each in `reorder_sundayFirstToMondayFirst`, `peakHour_picksHighestIndexAndCount`, `peakWeekday_picksHighestMonFirstIndexAndCount`, `tieBreak_earliestBucketWins`, `allZero_hasDataFalse`, `hasData_trueWhenAnyCountPositive`, plus the **three** calls in `badLength_hasDataFalseNoCrash`. Update all nine.

For example, change:

```cpp
    const TimeAnalytics a = TimeAnalytics::compute(byHour, byWeekday);
```

to:

```cpp
    const TimeAnalytics a = TimeAnalytics::compute(byHour, byWeekday, 0, 23);
```

and in `badLength_hasDataFalseNoCrash`, change the three:

```cpp
    const TimeAnalytics shortHour = TimeAnalytics::compute(zeros(23), zeros(7));
    ...
    const TimeAnalytics shortWeek = TimeAnalytics::compute(zeros(24), zeros(6));
    ...
    const TimeAnalytics emptyBoth = TimeAnalytics::compute(QList<int>{}, QList<int>{});
```

to:

```cpp
    const TimeAnalytics shortHour = TimeAnalytics::compute(zeros(23), zeros(7), 0, 23);
    ...
    const TimeAnalytics shortWeek = TimeAnalytics::compute(zeros(24), zeros(6), 0, 23);
    ...
    const TimeAnalytics emptyBoth = TimeAnalytics::compute(QList<int>{}, QList<int>{}, 0, 23);
```

Add these five slot declarations to the `private slots:` block (after `badLength_hasDataFalseNoCrash();`):

```cpp
    void windowedPeak_ignoresTallerOutOfWindowBar();
    void windowedPeak_picksTallestInWindow();
    void inclusiveClose_countsCloseHourBar();
    void invertedWindow_degradesToFullRange();
    void allOutOfHours_peakZeroButHasDataTrue();
```

Add the five bodies just before `QTEST_APPLESS_MAIN(TstTimeAnalytics)`:

```cpp
void TstTimeAnalytics::windowedPeak_ignoresTallerOutOfWindowBar() {
    // Global max at hour 6 (out of [7,21]); largest in-window bar at hour 10.
    QList<int> byHour = zeros(24);
    byHour[6] = 20;   // taller, but before openHour 7
    byHour[10] = 8;   // in-window peak
    const TimeAnalytics a = TimeAnalytics::compute(byHour, weekSunFirst(), 7, 21);
    QCOMPARE(a.peakHour, 10);
    QCOMPARE(a.peakHourCount, 8);
}

void TstTimeAnalytics::windowedPeak_picksTallestInWindow() {
    // A taller bar at hour 22 is out of [7,21]; peak is the largest in-window hour.
    QList<int> byHour = zeros(24);
    byHour[9] = 5;
    byHour[14] = 12;   // in-window peak
    byHour[22] = 30;   // out of window, ignored
    const TimeAnalytics a = TimeAnalytics::compute(byHour, weekSunFirst(), 7, 21);
    QCOMPARE(a.peakHour, 14);
    QCOMPARE(a.peakHourCount, 12);
}

void TstTimeAnalytics::inclusiveClose_countsCloseHourBar() {
    // The sole positive bucket sits AT closeHour (21). The close hour is included.
    QList<int> byHour = zeros(24);
    byHour[21] = 4;
    const TimeAnalytics a = TimeAnalytics::compute(byHour, weekSunFirst(), 7, 21);
    QCOMPARE(a.peakHour, 21);
    QCOMPARE(a.peakHourCount, 4);
}

void TstTimeAnalytics::invertedWindow_degradesToFullRange() {
    // Inverted / out-of-range windows clamp to 0..23 (decision 4), so the peak is
    // the overall-24h peak — equivalent to the unwindowed result.
    QList<int> byHour = zeros(24);
    byHour[3] = 15;    // would be excluded by [7,21], included by the 0..23 fallback
    byHour[14] = 9;
    const TimeAnalytics inverted = TimeAnalytics::compute(byHour, weekSunFirst(), 21, 7);
    QCOMPARE(inverted.peakHour, 3);
    QCOMPARE(inverted.peakHourCount, 15);
    const TimeAnalytics negLow = TimeAnalytics::compute(byHour, weekSunFirst(), -1, 23);
    QCOMPARE(negLow.peakHour, 3);
    const TimeAnalytics highHi = TimeAnalytics::compute(byHour, weekSunFirst(), 0, 30);
    QCOMPARE(highHi.peakHour, 3);
}

void TstTimeAnalytics::allOutOfHours_peakZeroButHasDataTrue() {
    // Every positive bucket is outside [7,21] -> windowed peak is 0/0, but hasData
    // stays an OVERALL measure (decision 5): weekday data must not be hidden.
    QList<int> byHour = zeros(24);
    byHour[2] = 6;     // pre-open staff login
    const TimeAnalytics a = TimeAnalytics::compute(byHour, weekSunFirst(), 7, 21);
    QCOMPARE(a.peakHour, 0);
    QCOMPARE(a.peakHourCount, 0);
    QVERIFY(a.hasData);   // hour 2 count + weekday counts -> overall hasData true
}
```

- [ ] **Step 2: Build the test target and watch it FAIL to compile (RED).**

```powershell
cmake --build C:\b\loams-lh --target tst_timeanalytics
```

Expected failure: `no matching function for call to 'TimeAnalytics::compute(...)'` / `candidate expects 4 arguments, 2 provided` (the still-2-arg declaration doesn't match the new 4-arg calls, and the five new bodies pass 4 args).

- [ ] **Step 3: Add the clamp helper + extend the `compute` declaration in the header.**

In `qt-app/core/timeanalytics.h`, find the include:

```cpp
#include <QList>
```

Insert directly below it (before `struct TimeAnalytics`):

```cpp

// Clamp a library-hours window to a safe, non-empty [lo,hi] within 0..23.
// SINGLE SOURCE of the decision-4 fallback (spec §5.1): used by
// TimeAnalytics::compute AND the ViewModel's hourly-bar / export builders, so the
// window arithmetic is byte-identical everywhere and the reported peak can never
// name a bar that is not drawn. If the stored hours are nonsensical
// (openHour<0 || closeHour>23 || openHour>closeHour) fall back to the full 0..23
// range so the chart never blanks from a hand-edited settings typo.
inline void clampLibraryHours(int &lo, int &hi)
{
    if (lo < 0 || hi > 23 || lo > hi) {
        lo = 0;
        hi = 23;
    }
}
```

Then find the `compute` declaration:

```cpp
    static TimeAnalytics compute(const QList<int> &byHour,
                                 const QList<int> &byWeekdaySunFirst);
```

Replace it with (and update the doc comment above it to mention the window):

```cpp
    // byHour: dense 24-element array, index = hour 0..23.
    // byWeekdaySunFirst: dense 7-element array, [0]=Sunday .. [6]=Saturday (the wire
    // format). openHour/closeHour bound the hourly peak scan to [openHour,closeHour]
    // INCLUSIVE (decision 1/3); nonsensical windows clamp to 0..23 (decision 4).
    // hourly is still returned RAW/24-wide (the window is applied by consumers, not
    // by mutating the array). Tie-break for both peaks: earliest/first bucket wins.
    // Defensive: if either array is not the expected length, returns hasData=false
    // with no OOB access (a second net beneath ReportController's parse boundary).
    static TimeAnalytics compute(const QList<int> &byHour,
                                 const QList<int> &byWeekdaySunFirst,
                                 int openHour, int closeHour);
```

(Delete the old two-line block that this comment/declaration replaces, so there is exactly one `compute` declaration.)

- [ ] **Step 4: Extend the `compute` definition + window the peak scan.**

In `qt-app/core/timeanalytics.cpp`, find the definition head:

```cpp
TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour,
                                     const QList<int> &byWeekdaySunFirst)
{
```

Replace with:

```cpp
TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour,
                                     const QList<int> &byWeekdaySunFirst,
                                     int openHour, int closeHour)
{
```

Then find the peak-hour loop:

```cpp
    // Peak hour — highest count; earliest bucket wins ties (strictly-greater keeps
    // the first max seen).
    for (int h = 0; h < 24; ++h) {
        if (byHour.at(h) > a.peakHourCount) {
            a.peakHourCount = byHour.at(h);
            a.peakHour = h;
        }
    }
```

Replace with (window the scan to `[lo,hi]` via the shared clamp; earliest-tie preserved):

```cpp
    // Peak hour — highest count WITHIN the library-hours window [openHour,closeHour]
    // inclusive (decision 1/3); earliest bucket wins ties (strictly-greater keeps
    // the first max seen). A nonsensical window clamps to 0..23 (decision 4). When
    // the window holds no positive bucket, peakHour/peakHourCount stay 0 — the
    // sentinel decision 5 keys on. hourly stays RAW/24-wide (set above).
    int lo = openHour, hi = closeHour;
    clampLibraryHours(lo, hi);
    for (int h = lo; h <= hi; ++h) {
        if (byHour.at(h) > a.peakHourCount) {
            a.peakHourCount = byHour.at(h);
            a.peakHour = h;
        }
    }
```

Everything else in `compute` — the wrong-length gate, `a.hourly = byHour;`, the Sun→Mon reorder, the peak-weekday loop, and the `hasData` scan over the full arrays — stays exactly as-is.

- [ ] **Step 5: Point the two remaining production/test call sites at `0, 23` (behavior-preserving).**

`compute` has exactly three call sites beyond `tst_timeanalytics` (updated in Step 1):

1. `qt-app/quick/viewmodels/ReportingViewModel.cpp` — `onTimeAnalyticsReady`. Find:

   ```cpp
       m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday);
   ```

   Replace with (Task 2 swaps `0, 23` for the cached window):

   ```cpp
       m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday, 0, 23);
   ```

2. `qt-app/quick/tests/tst_reportingviewmodel.cpp` — the reference call in `buildTimeExport_dataState_populatesLabelsCountsPeaks`. Find:

   ```cpp
       const TimeAnalytics ta = TimeAnalytics::compute(denseHours(), denseWeek());
   ```

   Replace with:

   ```cpp
       const TimeAnalytics ta = TimeAnalytics::compute(denseHours(), denseWeek(), 0, 23);
   ```

   (This keeps the `QCOMPARE(te.hourCounts, ta.hourly)` assertion valid for now — `buildTimeExport` is still full-24 until Task 2. The slice-rewrite of this assertion happens in Task 2.)

   **Grounding note:** grep `TimeAnalytics::compute` across `qt-app/` to confirm these are the only production/test call sites and that `adminwindow.cpp` contains NONE (verified: its three renderer calls pass `ReportTimeExport{}` and never call `compute`).

- [ ] **Step 6: Build + run `tst_timeanalytics` to GREEN.**

```powershell
cmake --build C:\b\loams-lh --target tst_timeanalytics
ctest --test-dir C:\b\loams-lh -R tst_timeanalytics --output-on-failure
```

Expected: all 12 cases green (7 existing behavior-preserved with `0,23`, 5 new windowed).

- [ ] **Step 7: Full build + full `ctest` (no regressions), both exes link.**

```powershell
cmake --build C:\b\loams-lh
ctest --test-dir C:\b\loams-lh --output-on-failure
```

Expected: baseline green + the 5 new cases. `WITS` and `WITSQuick` both link (the VM call site now passes `0,23`; `adminwindow.cpp` untouched). If `tst_settingsviewmodel` flakes, re-run it alone.

- [ ] **Step 8: Commit** (scope `reporting`). Stage only `qt-app/core/timeanalytics.h`, `qt-app/core/timeanalytics.cpp`, `qt-app/tests/tst_timeanalytics.cpp`, `qt-app/quick/viewmodels/ReportingViewModel.cpp`, `qt-app/quick/tests/tst_reportingviewmodel.cpp`. Subject e.g.:

  `feat(reporting): add library-hours window to TimeAnalytics::compute (call sites still full-range)`

### Deliverable

`compute` scans the hourly peak over `[openHour,closeHour]` via the shared `clampLibraryHours` fallback; all call sites pass `0,23` so the app behaves exactly as before. New unit cases prove windowed-peak / out-of-window / inclusive-close / inverted-window / all-out-of-hours. Full suite green; both exes link.

---

## Task 2 — ViewModel: cache the window, turn it on for the screen bars + the export carrier, gate the hour caption

Caches `m_openHour`/`m_closeHour` from `headerInfo()` at analytics-arrival, passes them to `compute` and to a windowed `buildHourlyBars`, windows `buildTimeExport`, and suppresses the hour caption when the windowed peak is zero. After this task the screen and both export carriers are cropped to library hours.

### Files

- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h` — `buildHourlyBars` 3-arg; add `m_openHour`/`m_closeHour`.
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp` — cache window; window `buildHourlyBars` + `buildTimeExport`; gate hour captions.
- Modify: `qt-app/quick/tests/tst_reportingviewmodel.cpp` — windowed assertions + the `:848` slice rewrite; new windowed-peak / all-out-of-hours cases.

### Interfaces

**Produces:**
- `static QList<BarsModel::Bar> ReportingViewModel::buildHourlyBars(const QList<int> &hourly, int openHour, int closeHour);` — keeps the `hourly.size() != 24` early-return; applies `clampLibraryHours`; emits one bar per hour across `[lo,hi]` inclusive with **every** label present (no `h % 3` blanking).
- ViewModel members `int m_openHour = 7;` / `int m_closeHour = 21;` — cached from `headerInfo()` when analytics arrive; read by both the screen path and the const `buildTimeExport()`.

**Consumes:** `clampLibraryHours` + 4-arg `compute` (Task 1); existing `headerInfo()` (`AppSettings library/openHour` default 7, `library/closeHour` default 21), `hourTick`, `formatHourRange`, `weekdayName`, `weekdayShortName`.

### Steps

- [ ] **Step 1: Rewrite the affected tst_reportingviewmodel assertions to the windowed expectations, and add the two new cases (RED).**

The default test window is `[7,21]`, giving `21-7+1 = 15` hour entries, first label `"7A"` (hour 7), and hour 14 at index `14-7 = 7` (`"2P"`). **Why this is deterministic, not machine-dependent:** every `wits_add_qttest()` target links `testsupport/settingsisolation.cpp`, which calls `AppSettings::isolateForTesting()` before `main()` — so `headerInfo()` reads a process-private *empty* INI store and returns the code defaults `library/openHour`=7 / `library/closeHour`=21, never the developer's real registry. No test seeds `AppSettings`, and `ReportingViewModel` only ever *reads* library hours, so the isolated store stays empty for those keys and the window is `[7,21]` in every run. (If you ever need a different window in a test, seed it via `AppSettings` in that test's `init()` — do NOT assume the store is pre-populated.)

**(a)** In `timeModels_populatedWithLabelBlanking` (its name is now a misnomer — the blanking is gone, but leave the name to minimize churn), find:

```cpp
    QCOMPARE(vm.hourlyBars()->rowCount(), 24);
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);

    // Interval x-labels: hours 0/3/6.. carry a label, off-interval hours are blank.
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(0, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("12A"));
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(3, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("3A"));
    QVERIFY(vm.hourlyBars()->data(vm.hourlyBars()->index(1, 0),
            BarsModel::LabelRole).toString().isEmpty());
```

Replace with (15 windowed bars, EVERY label non-blank; index 0 = hour 7, index 7 = hour 14):

```cpp
    QCOMPARE(vm.hourlyBars()->rowCount(), 15);   // [7,21] inclusive = 15 bars
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);

    // EVERY hour in the window is labeled now (the h%3 blanking is gone).
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(0, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("7A"));   // openHour
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(7, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("2P"));   // hour 14
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(14, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("9P"));   // closeHour 21
    QVERIFY(!vm.hourlyBars()->data(vm.hourlyBars()->index(1, 0),
             BarsModel::LabelRole).toString().isEmpty());               // no blanks
```

**(b)** In `hasTimeData_falseOnAllZeroShowsEmptyState`, find:

```cpp
    QCOMPARE(vm.hourlyBars()->rowCount(), 24);  // bars still dense (all zero)
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);
```

Replace with (windowed, all-zero bars):

```cpp
    QCOMPARE(vm.hourlyBars()->rowCount(), 15);  // windowed bars (all zero)
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);
```

**(c)** In `buildTimeExport_dataState_populatesLabelsCountsPeaks`, find:

```cpp
    // 24 hour labels, byte-identical to hourTick for known hours.
    QCOMPARE(te.hourLabels.size(), 24);
    QCOMPARE(te.hourLabels.at(0), QStringLiteral("12A"));
    QCOMPARE(te.hourLabels.at(3), QStringLiteral("3A"));
    QCOMPARE(te.hourLabels.at(14), QStringLiteral("2P"));

    // Counts copied straight from the cached analytics.
    const TimeAnalytics ta = TimeAnalytics::compute(denseHours(), denseWeek(), 0, 23);
    QCOMPARE(te.hourCounts, ta.hourly);
    QCOMPARE(te.hourCounts.at(14), 12);
    QCOMPARE(te.hourCounts.at(9), 3);
```

Replace with (15 windowed labels; the count assertion compares against the `[7,21]` SLICE of the raw 24-wide `hourly`, NOT the whole array — the forward-note fix):

```cpp
    // 15 windowed hour labels ([7,21]), byte-identical to hourTick, EVERY label set.
    QCOMPARE(te.hourLabels.size(), 15);
    QCOMPARE(te.hourLabels.at(0), QStringLiteral("7A"));    // openHour 7
    QCOMPARE(te.hourLabels.at(7), QStringLiteral("2P"));    // hour 14
    QCOMPARE(te.hourLabels.at(14), QStringLiteral("9P"));   // closeHour 21

    // Counts equal the [7,21] SLICE of the RAW 24-wide hourly (NOT the whole array).
    const TimeAnalytics ta = TimeAnalytics::compute(denseHours(), denseWeek(), 7, 21);
    QCOMPARE(te.hourCounts, ta.hourly.mid(7, 15));   // hours 7..21 inclusive
    QCOMPARE(te.hourCounts.at(7), 12);   // hour 14 -> window index 7
    QCOMPARE(te.hourCounts.at(2), 3);    // hour 9  -> window index 2
```

The remaining assertions in that test (weekday labels/counts, `busiestHourLabel == "2–3 PM"`, `busiestDayLabel == "Monday"`) are unchanged — the window `[7,21]` still contains the peak at hour 14, and the weekday path is untouched.

**(d)** Add two slot declarations to the `private slots:` block (after `buildTimeExport_defensiveWrongLength_degradesToEmpty();`):

```cpp
    void windowedCaption_followsInWindowPeakNotOverall();
    void allOutOfHours_hourCaptionEmptyWeekdayStillShown();
```

Add the two bodies just before `QTEST_MAIN(TestReportingViewModel)`:

```cpp
void TestReportingViewModel::windowedCaption_followsInWindowPeakNotOverall() {
    // Overall 24h peak is at hour 6 (out of [7,21]); the in-window peak is hour 10.
    // The caption must name the in-window peak, and screen must match export.
    QList<int> byHour = zeros(24);
    byHour[6] = 30;    // taller, pre-open -> ignored by the window
    byHour[10] = 9;    // in-window peak -> "10-11 AM"
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(byHour, denseWeek());
    QCOMPARE(vm.busiestHourLabel(), QStringLiteral("10–11 AM"));
    QCOMPARE(vm.buildTimeExport().busiestHourLabel, QStringLiteral("10–11 AM"));
    QVERIFY(vm.hasTimeData());
}

void TestReportingViewModel::allOutOfHours_hourCaptionEmptyWeekdayStillShown() {
    // Every hourly visit is outside [7,21]; the windowed hour peak is 0 so the hour
    // caption is suppressed, but hasData/weekday data remain (decision 5).
    QList<int> byHour = zeros(24);
    byHour[2] = 6;     // pre-open staff login, out of window
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(byHour, denseWeek());
    QVERIFY(vm.busiestHourLabel().isEmpty());                       // screen caption gone
    QVERIFY(vm.buildTimeExport().busiestHourLabel.isEmpty());       // export caption gone
    QVERIFY(vm.hasTimeData());                                      // overall data present
    QCOMPARE(vm.busiestDayLabel(), QStringLiteral("Monday"));       // weekday unaffected
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);
}
```

(`"10–11 AM"` uses U+2013 — copy it exactly.)

- [ ] **Step 2: Build the VM test target and watch it FAIL (RED).**

```powershell
cmake --build C:\b\loams-lh --target tst_reportingviewmodel
```

Expected failures: the two new `no matching function`/undeclared-slot errors, plus `QCOMPARE` mismatches on the edited assertions once the target links against the still-full-24 `buildHourlyBars`/`buildTimeExport` — e.g. `Actual (vm.hourlyBars()->rowCount()): 24  Expected: 15`. (The build compiles because the assertions are runtime `QCOMPARE`s; the two new tests reference no new API, so this target compiles and fails at RUNTIME on the windowing expectations.)

- [ ] **Step 3: Change the `buildHourlyBars` declaration + add the members in the header.**

In `qt-app/quick/viewmodels/ReportingViewModel.h`, find:

```cpp
    static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly);        // 24, label blanked off-3h
```

Replace with:

```cpp
    static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly,
                                                 int openHour, int closeHour);     // [open,close], every label
```

Then find the When-section members block:

```cpp
    BarsModel m_hourlyBars;
    BarsModel m_weekdayBars;
    TimeAnalytics m_timeAnalytics;
    QString m_busiestHourLabel;
    QString m_busiestDayLabel;
```

Insert the two cached-window members after `TimeAnalytics m_timeAnalytics;`:

```cpp
    BarsModel m_hourlyBars;
    BarsModel m_weekdayBars;
    TimeAnalytics m_timeAnalytics;
    // Library-hours window cached from headerInfo() when analytics arrive, so the
    // screen bars and the const buildTimeExport() crop to the SAME window as the
    // peak scan (arrival-time parity anchor, spec §5.2). Defaults match the
    // AppSettings library/openHour(7) / library/closeHour(21) defaults.
    int m_openHour = 7;
    int m_closeHour = 21;
    QString m_busiestHourLabel;
    QString m_busiestDayLabel;
```

- [ ] **Step 4: Window `buildHourlyBars` (drop the `h % 3` blanking).**

In `qt-app/quick/viewmodels/ReportingViewModel.cpp`, find:

```cpp
QList<BarsModel::Bar> ReportingViewModel::buildHourlyBars(const QList<int> &hourly)
{
    QList<BarsModel::Bar> bars;
    if (hourly.size() != 24)
        return bars;
    bars.reserve(24);
    for (int h = 0; h < 24; ++h) {
        // Interval x-labels are DATA-DRIVEN (spec §5.4): only hours 0/3/6.. carry a
        // label; the rest are blank so LBarChart (a Text under EVERY bar) shows
        // ~every-3h ticks. The chart has no thinning logic of its own.
        const QString label = (h % 3 == 0) ? hourTick(h) : QString();
        bars.append({ label, double(hourly.at(h)) });
    }
    return bars;
}
```

Replace with (window `[lo,hi]` via the shared clamp; one labeled bar per hour, decision 2/3):

```cpp
QList<BarsModel::Bar> ReportingViewModel::buildHourlyBars(const QList<int> &hourly,
                                                          int openHour, int closeHour)
{
    QList<BarsModel::Bar> bars;
    if (hourly.size() != 24)
        return bars;
    // Crop to the library-hours window [openHour,closeHour] inclusive (decision 3)
    // via the SAME clamp fallback as the core peak scan (decision 4). Every bar
    // carries its hour label now — the window is small (~8-15 bars) so the old
    // every-3h blanking is dropped and the open/close endpoints become meaningful,
    // labeled bars (decision 2).
    int lo = openHour, hi = closeHour;
    clampLibraryHours(lo, hi);
    bars.reserve(hi - lo + 1);
    for (int h = lo; h <= hi; ++h)
        bars.append({ hourTick(h), double(hourly.at(h)) });
    return bars;
}
```

- [ ] **Step 5: Cache the window + pass it to `compute` and `buildHourlyBars`; gate the hour caption.**

In `onTimeAnalyticsReady`, find:

```cpp
void ReportingViewModel::onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday)
{
    m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday, 0, 23);

    m_hourlyBars.setBars(buildHourlyBars(m_timeAnalytics.hourly));
    m_weekdayBars.setBars(buildWeekdayBars(m_timeAnalytics.weekdayMonFirst));

    m_hasTimeData = m_timeAnalytics.hasData;
    emit hasTimeDataChanged();

    // Captions ONLY when there is data — peak indices default to 0 ("12–1 AM" /
    // "Monday") on an all-zero range and would mislead (spec §6).
    m_busiestHourLabel = m_timeAnalytics.hasData ? formatHourRange(m_timeAnalytics.peakHour)
                                                 : QString();
    emit busiestHourLabelChanged();
    m_busiestDayLabel = m_timeAnalytics.hasData ? weekdayName(m_timeAnalytics.peakWeekdayMonFirst)
                                                : QString();
    emit busiestDayLabelChanged();
```

Replace that leading block (down to and including the `busiestDayLabelChanged()` emit) with (cache BEFORE compute; window both builders; gate the HOUR caption on `peakHourCount > 0`, decision 5; DAY caption stays on `hasData`):

```cpp
void ReportingViewModel::onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday)
{
    // Cache the library-hours window ONCE, at arrival, from the same AppSettings the
    // export path reads. This is the parity anchor (spec §5.2): compute scans the
    // peak with this window, and buildHourlyBars / buildTimeExport later emit bars
    // with the SAME cached window, so the reported peak always names a drawn bar --
    // even if the librarian changes the hours in Settings between Generate and Export.
    const ReportHeaderInfo info = headerInfo();
    m_openHour = info.openHour;
    m_closeHour = info.closeHour;

    m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday, m_openHour, m_closeHour);

    m_hourlyBars.setBars(buildHourlyBars(m_timeAnalytics.hourly, m_openHour, m_closeHour));
    m_weekdayBars.setBars(buildWeekdayBars(m_timeAnalytics.weekdayMonFirst));

    m_hasTimeData = m_timeAnalytics.hasData;
    emit hasTimeDataChanged();

    // HOUR caption is gated on the WINDOWED peak (decision 5): when every visit
    // falls outside library hours, peakHourCount == 0 and the caption is suppressed
    // rather than naming a bar that is not drawn. The DAY caption stays gated on
    // hasData (the weekday chart is window-independent). In the common case the two
    // gates agree; they diverge only in the all-out-of-hours case.
    m_busiestHourLabel = (m_timeAnalytics.peakHourCount > 0)
                             ? formatHourRange(m_timeAnalytics.peakHour) : QString();
    emit busiestHourLabelChanged();
    m_busiestDayLabel = m_timeAnalytics.hasData ? weekdayName(m_timeAnalytics.peakWeekdayMonFirst)
                                                : QString();
    emit busiestDayLabelChanged();
```

The rest of `onTimeAnalyticsReady` (the `m_timeError` clear, `setTimeLoading(false)`, `m_timeAnalyticsSettled = true`, `emit canGenerateChanged()`) is unchanged.

- [ ] **Step 6: Window `buildTimeExport`'s Data-state hour fill + gate its hour caption.**

In `buildTimeExport() const`, find the Data-state hour fill and the peak assignment:

```cpp
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
```

Replace with (window the hour fill via the cached members + the SAME clamp; gate the hour caption on `peakHourCount > 0`; weekday fill + day caption unchanged):

```cpp
    // Crop the hour arrays to the cached library-hours window [m_openHour,m_closeHour]
    // inclusive (decision 3), the SAME window compute scanned — so the exported bars
    // and the exported peak agree. Same clamp fallback as core (decision 4).
    int lo = m_openHour, hi = m_closeHour;
    clampLibraryHours(lo, hi);
    te.hourLabels.reserve(hi - lo + 1);
    te.hourCounts.reserve(hi - lo + 1);
    for (int h = lo; h <= hi; ++h) {
        te.hourLabels.append(hourTick(h));                 // reused 4b-iv-a helper
        te.hourCounts.append(m_timeAnalytics.hourly.at(h));
    }

    te.weekdayLabels.reserve(7);
    te.weekdayCounts.reserve(7);
    for (int d = 0; d < 7; ++d) {
        te.weekdayLabels.append(weekdayShortName(d));       // single-sourced helper
        te.weekdayCounts.append(m_timeAnalytics.weekdayMonFirst.at(d));
    }

    // Mirror the screen gate (decision 5): empty hour caption when the windowed peak
    // is zero, so the export never prints a "Peak Hour: …" for an unshown bar.
    te.busiestHourLabel = (m_timeAnalytics.peakHourCount > 0)
                              ? formatHourRange(m_timeAnalytics.peakHour) : QString();
    te.busiestDayLabel  = weekdayName(m_timeAnalytics.peakWeekdayMonFirst);
    return te;
```

The `Error` / `Empty` / defensive-length branches above this Data block are unchanged (they already emit empty lists).

- [ ] **Step 7: Build + run `tst_reportingviewmodel` to GREEN.**

```powershell
cmake --build C:\b\loams-lh --target tst_reportingviewmodel
ctest --test-dir C:\b\loams-lh -R tst_reportingviewmodel --output-on-failure
```

Expected: the edited windowed assertions pass, the two new cases pass, and every unrelated case stays green.

- [ ] **Step 8: Full build + full `ctest`, both exes link.**

```powershell
cmake --build C:\b\loams-lh
ctest --test-dir C:\b\loams-lh --output-on-failure
```

Expected: all green. `tst_reportrenderer` still green (it seeds its own carrier — the VM's now-windowed `buildTimeExport` doesn't reach it, and the renderer is unchanged). `WITS`/`WITSQuick` link.

- [ ] **Step 9: Commit** (scope `reporting`). Stage only `qt-app/quick/viewmodels/ReportingViewModel.h`, `qt-app/quick/viewmodels/ReportingViewModel.cpp`, `qt-app/quick/tests/tst_reportingviewmodel.cpp`. Subject e.g.:

  `feat(reporting): window the on-screen + export When? hourly analytics to library hours`

### Deliverable

The on-screen hourly chart and the export carrier both span `[openHour,closeHour]` with every hour labeled; the hour caption follows the in-window peak and is suppressed when no visit falls in library hours; the window is cached at arrival so screen ↔ export stay in parity. VM tests green; full suite green.

---

## Task 3 — Renderer: Excel cursor past the taller table + empty-caption fallback; windowed fixture

The renderer needs no hour math, but two narrow correctness fixes surface once the carrier can be shorter than 7 rows or carry an empty peak caption. `makeHourlyBarChartImage` needs NO change — confirm and state so.

### Files

- Modify: `qt-app/core/reportrenderer.cpp` — Excel cursor `qMax`; `peakHourCaption` empty-value fallback.
- Modify: `qt-app/tests/tst_reportrenderer.cpp` — windowed `sampleTimeExportData()`; narrow-window Excel cursor test.

### Interfaces

**Produces:**
- `QString peakHourCaption(const ReportTimeExport &t)` — returns `"Hourly Visits"` when `t.busiestHourLabel` is empty, else `"Peak Hour: %1"`.
- Excel Data-state cursor advance: `row = baseRow + 1 + qMax(timeExport.hourCounts.size(), timeExport.weekdayCounts.size());`.

**Consumes:** `ReportTimeExport` (windowed carrier from the VM at runtime; seeded directly in tests).

### Steps

- [ ] **Step 1: Retarget `sampleTimeExportData()` to a windowed carrier + add the narrow-window Excel cursor test (RED).**

In `qt-app/tests/tst_reportrenderer.cpp`, find the fixture:

```cpp
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

Replace with a windowed `[7,21]` carrier (15 hour entries, labels `"7A".."9P"`, peak `"2–3 PM"` at the in-window `2P` bar):

```cpp
    // A Data-state carrier windowed to library hours [7,21] (15 hour entries),
    // matching what the VM's windowed buildTimeExport now produces. Labels "7A".."9P";
    // in-window peak "2–3 PM" at the 2P bar (hour 14). Weekday table stays 7 rows.
    static ReportTimeExport sampleTimeExportData() {
        ReportTimeExport t;
        t.state = TimeAnalyticsExportState::Data;
        static const char *const hourTicks[15] = {
            "7A","8A","9A","10A","11A","12P","1P","2P","3P","4P","5P","6P","7P","8P","9P" };
        // Window index 7 == hour 14 (2 PM) peak; window index 2 == hour 9.
        for (int i = 0; i < 15; ++i) {
            t.hourLabels << QString::fromLatin1(hourTicks[i]);
            t.hourCounts << (i == 7 ? 12 : (i == 2 ? 3 : 0));
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

The existing `writeReportToXlsx_timeBlock_dataStatePresent` still passes with this fixture: it asserts `foundHourCell` on `"2P"` (still present, now at window index 7) and `foundPeakHour` on `"Peak Hour: 2–3 PM"` — both hold. The maker cases assert `imageHasNonWhitePixel` + `>500` bar-color pixels, which a 15-bar chart still satisfies. No change needed to those three tests, but re-run them to confirm.

Now add a **narrow-window cursor** test. Add the slot declaration after `writeReportToXlsx_timeBlock_dataStatePresent();`:

```cpp
    void writeReportToXlsx_timeBlock_narrowWindowFooterBelowWeekday();
```

Add the body before `QTEST_MAIN(TstReportRenderer)` (a `[11,12]` window: hourly table 2 rows, SHORTER than the 7-row weekday table — the footer/prepared-by line must land below the TALLER weekday table, not overprint its rows):

```cpp
void TstReportRenderer::writeReportToXlsx_timeBlock_narrowWindowFooterBelowWeekday() {
    ReportTimeExport t;
    t.state = TimeAnalyticsExportState::Data;
    t.hourLabels << QStringLiteral("11A") << QStringLiteral("12P");   // 2-hour window
    t.hourCounts << 3 << 5;
    static const char *const days[7] = { "Mon","Tue","Wed","Thu","Fri","Sat","Sun" };
    const int dayCounts[7] = { 40, 8, 30, 8, 5, 1, 2 };
    for (int d = 0; d < 7; ++d) {
        t.weekdayLabels << QString::fromLatin1(days[d]);
        t.weekdayCounts << dayCounts[d];
    }
    t.busiestHourLabel = QStringLiteral("12–1 PM");
    t.busiestDayLabel  = QStringLiteral("Monday");

    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(),
        sampleAnalytics(), false, t));
    QVERIFY(xlsx.selectSheet("Summary"));

    // Find the weekday header ("Day") row and the system-generated footer row.
    int dayHdrRow = -1, footerRow = -1;
    for (int r = 1; r <= 120; ++r) {
        for (int c = 1; c <= 8; ++c) {
            const QString cell = xlsx.read(r, c).toString();
            if (cell == "Day") dayHdrRow = r;
            if (cell.startsWith("This is a system-generated report")) footerRow = r;
        }
    }
    QVERIFY(dayHdrRow > 0);
    QVERIFY(footerRow > 0);
    // The weekday table occupies dayHdrRow+1 .. dayHdrRow+7. The footer must sit
    // strictly BELOW the taller (weekday) table, never overprinting its rows.
    QVERIFY2(footerRow > dayHdrRow + 7,
             qPrintable(QString("footer row %1 overprints weekday table (Day hdr %2)")
                            .arg(footerRow).arg(dayHdrRow)));
}
```

(`"12–1 PM"` uses U+2013 — copy exactly.)

- [ ] **Step 2: Build + run `tst_reportrenderer` and watch the narrow-window case FAIL (RED).**

```powershell
cmake --build C:\b\loams-lh --target tst_reportrenderer
ctest --test-dir C:\b\loams-lh -R tst_reportrenderer --output-on-failure
```

Expected: `writeReportToXlsx_timeBlock_narrowWindowFooterBelowWeekday` fails — with the current cursor `row = baseRow + 1 + timeExport.hourCounts.size();` the footer advances past only the 2-row hourly table, landing inside the 7-row weekday table (`footer row … overprints weekday table`). The windowed-fixture cases stay green.

- [ ] **Step 3: Advance the Excel cursor past the taller table.**

In `qt-app/core/reportrenderer.cpp`, in the Data branch of the "When?" Excel block, find:

```cpp
        // Advance the cursor past the TALLER (24-row hourly) table so the
        // system-generated footer that follows lands below the whole block.
        row = baseRow + 1 + timeExport.hourCounts.size();
        row += 1;
```

Replace with (advance past whichever table is taller — the hourly table can now be shorter than the 7-row weekday table for narrow windows):

```cpp
        // Advance the cursor past the TALLER of the two tables so the system-generated
        // footer lands below the whole block. Windowing makes the hourly table as
        // short as 2 rows (e.g. an [11,12] window), so it is no longer guaranteed
        // taller than the 7-row weekday table (§5.4).
        row = baseRow + 1 + qMax(timeExport.hourCounts.size(), timeExport.weekdayCounts.size());
        row += 1;
```

- [ ] **Step 4: Add the empty-caption fallback to `peakHourCaption`.**

In `qt-app/core/reportrenderer.cpp`, find the shared helper (in the anonymous namespace near the top):

```cpp
QString peakHourCaption(const ReportTimeExport &t) { return QStringLiteral("Peak Hour: %1").arg(t.busiestHourLabel); }
```

Replace with (drop the prefix entirely when the value is empty — the all-out-of-hours Data case, decision 5; still a pure string guard, zero hour math):

```cpp
QString peakHourCaption(const ReportTimeExport &t) {
    return t.busiestHourLabel.isEmpty()
               ? QStringLiteral("Hourly Visits")
               : QStringLiteral("Peak Hour: %1").arg(t.busiestHourLabel);
}
```

`peakDayCaption` is unchanged.

- [ ] **Step 5: Confirm `makeHourlyBarChartImage` needs NO change (state it in the commit/PR).**

Read `makeHourlyBarChartImage` in `qt-app/core/reportrenderer.cpp`. Confirm its category loop already emits one label per carrier entry with no `i % 3` thinning:

```cpp
    for (int i = 0; i < t.hourCounts.size(); ++i) {
        *set << t.hourCounts.at(i);
        categories << (i < t.hourLabels.size() ? t.hourLabels.at(i) : QString());
    }
```

A shorter (windowed) carrier yields a shorter, every-hour-labeled chart automatically. No edit. (The earlier every-3rd-label thinning was already removed in 4b-iv-b — the comment above the maker documents why.)

- [ ] **Step 6: Build + run `tst_reportrenderer` to GREEN, then full build + full `ctest`.**

```powershell
cmake --build C:\b\loams-lh --target tst_reportrenderer
ctest --test-dir C:\b\loams-lh -R tst_reportrenderer --output-on-failure
cmake --build C:\b\loams-lh
ctest --test-dir C:\b\loams-lh --output-on-failure
```

Expected: the narrow-window case passes; the windowed-fixture + maker + Empty/Error/Disabled cases stay green; full suite green; both exes link.

- [ ] **Step 7: Commit** (scope `reporting`). Stage only `qt-app/core/reportrenderer.cpp`, `qt-app/tests/tst_reportrenderer.cpp`. Subject e.g.:

  `fix(reporting): Excel cursor past the taller When? table + empty-peak caption fallback`

### Deliverable

The Excel "When?" footer never overprints the weekday table for narrow windows; the PDF/Excel peak caption reads `"Hourly Visits"` (not `"Peak Hour: "`) in the all-out-of-hours Data case; `makeHourlyBarChartImage` confirmed unchanged. Renderer tests green; full suite green.

---

## Task 4 — QML caption gate + mandatory manual smoke (release gate)

Gates the on-screen hour caption so a windowed all-zero peak never renders a bare `"Busiest: "`, then runs the human smoke the offscreen tests structurally cannot cover (the QChartView screen-clamp is invisible headless — project memory "QtChart export screen clamp").

### Files

- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml` — `busiestHourCaption` visibility.

### Interfaces

**Produces:** the `busiestHourCaption` `Text` becomes `visible: … busiestHourLabel.length > 0`.

**Consumes:** `vm.busiestHourLabel` (empty in the all-out-of-hours case after Task 2).

### Steps

- [ ] **Step 1: Gate the `busiestHourCaption` visibility.**

In `qt-app/quick/qml/admin/ReportingScreen.qml`, find the `busiestHourCaption` `Text` (inside the `whenData` subtree):

```qml
                        Text {
                            objectName: "busiestHourCaption"
                            text: qsTr("Busiest: %1").arg(screen.vm ? screen.vm.busiestHourLabel : "")
                            textFormat: Text.PlainText
                            color: Theme.text; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
```

Add a `visible:` line so the caption disappears when there is no windowed peak (decision 5). The enclosing `whenData` subtree still gates on `!timeLoading && timeError.length === 0 && hasTimeData`, so the hourly *chart* still draws its (all-zero, windowed) bars while only the caption is suppressed:

```qml
                        Text {
                            objectName: "busiestHourCaption"
                            text: qsTr("Busiest: %1").arg(screen.vm ? screen.vm.busiestHourLabel : "")
                            visible: screen.vm ? screen.vm.busiestHourLabel.length > 0 : false
                            textFormat: Text.PlainText
                            color: Theme.text; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
```

- [ ] **Step 2: Full build + full `ctest`, both exes link.**

```powershell
cmake --build C:\b\loams-lh
ctest --test-dir C:\b\loams-lh --output-on-failure
```

Expected: all green (this is a QML-visibility-only change; no C++ test asserts the caption's `visible`). Confirm `WITSQuick.exe` and `WITS.exe` both link.

- [ ] **Step 3: Commit** (scope `reporting`). Stage only `qt-app/quick/qml/admin/ReportingScreen.qml`. Subject e.g.:

  `feat(reporting): hide the on-screen Busiest-hour caption when no visit falls in library hours`

- [ ] **Step 4: Manual `WITSQuick.exe` smoke — MANDATORY RELEASE GATE (owner-run, no code).**

Close any running `WITSQuick.exe`, rebuild, then launch `C:\b\loams-lh\quick\WITSQuick.exe` → Reporting screen. Synthetic data only (no real student PII). Confirm each, per spec §8.4:

1. **On screen** — over a range with data: the "When?" hourly chart shows only `[open..close]` bars, every hour labeled including the open/close endpoints, and the "Busiest: …" caption names an in-window hour.
2. **PDF export** — the hourly bar chart is cropped to the window with readable, non-giant fonts and real bars (not blank), correct `Peak Hour:` title. (The screen-clamp failure mode is giant-font/blank — this is the whole reason the gate is manual.)
3. **Excel export** — the hourly table lists only the window hours, and the footer / "Prepared by" line sits below the whole block (no overprint), including for a deliberately narrow window.
4. **Window tracks the setting** — change the library hours in Settings and re-generate to confirm the window follows; then change hours **after** Generate but **before** Export and confirm screen and export do NOT desync (the cached window holds).
5. **All-out-of-hours** — if reproducible, confirm the hour caption disappears (screen) / reads "Hourly Visits" (PDF/Excel) while the weekday chart still shows data.
6. **Legacy parity** — run `WITS.exe`, export PDF + Excel, confirm there is still NO "When?" section anywhere (byte-for-byte legacy output).

On any failure, route through `superpowers:systematic-debugging` (reproduce with a failing test → isolate → fix under TDD) before re-exporting.

### Deliverable

The on-screen hour caption is gated to a real windowed peak; owner-confirmed smoke for on-screen + PDF + Excel over data / narrow-window / (if reproducible) all-out-of-hours ranges, plus the legacy-parity check. This closes the follow-up.

---

## Post-build gate

After Task 3 (all automated green) and before finishing the branch: run `/claude-review` on the slice per the project workflow (`.claude/rules/workflow.md` §3), fix Critical/Important findings, and re-submit until APPROVE or the 3-round cap. Task 4's manual smoke is the release gate the automated suite structurally cannot replace. Then `superpowers:finishing-a-development-branch` → the project-scoped `create-pr` (three-agent gate: `dry-checker`, `security-reviewer`, `general-code-reviewer`).
