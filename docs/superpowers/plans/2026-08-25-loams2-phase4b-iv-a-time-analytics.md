# Phase 4b-iv-a — Reporting "When?" Time Analytics — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. **Line numbers in this plan are current-ish snapshots and WILL drift** — locate every edit by the surrounding code/context quoted here, not by the line number alone.

**Goal:** Surface WHEN students visit — a 24-bucket peak-hours breakdown and a Monday–Sunday busiest-days breakdown — on the Reporting dashboard, honoring the exact same filters (department, course, date range, semester) as the rest of the report. On-screen only; no export of the time analytics (that is the deferred 4b-iv-b slice).

**Architecture:** A new backend endpoint `get_report_time_data.php` (sibling of `get_report_data.php`, which is untouched) returns two dense arrays — `byHour[24]` and `byWeekday[7]` (Sunday-first). The client layers strictly:

```
get_report_time_data.php  (dense byHour[24], byWeekday[7] Sunday-first)
        │
        ▼
ReportController::fetchTimeAnalytics → parseTimeAnalytics  (parse + CONTRACT VALIDATION: exact length + numeric)
        │  timeAnalyticsReady(QList<int>,QList<int>)  |  timeAnalyticsError(QString)
        ▼
TimeAnalytics::compute  (core, pure: Sun→Mon reorder, peak detection, hasData; presentation-agnostic)
        │
        ▼
ReportingViewModel  (presentation state: two BarsModel, caption strings, hasTimeData/timeError/timeLoading)
        │
        ▼
ReportingScreen.qml  "When do students visit?" LCard
```

The pure aggregator lives in **witscore** alongside `reportanalytics.{h,cpp}`, is `static`, network-free, and is never called from QML (only the ViewModel touches it). The backend owns densification; core consumes already-dense arrays; the ViewModel owns all human-readable formatting (12-hour range, weekday name); QML owns presentation only.

**Tech Stack:** Qt 6.11 / C++17, QtTest (offscreen where GUI) under ctest, QML (URI `LOAMS`) + Qt Quick Test, CMake + Ninja + MinGW. Backend PHP/MySQL on XAMPP.

## Global Constraints

- **Spec:** `docs/superpowers/specs/2026-08-25-loams2-phase4b-iv-a-time-analytics-design.md` (owner-approved, claude-review APPROVED). Every §-decision in it is **binding**; the Decision Freeze table (§11) is the tie-breaker.
- **This is 4b-iv-a ONLY** — the new endpoint + the on-screen section. **No** PDF/Excel rendering of time analytics, **no** hour×day heatmap, **no** per-course/per-dept time breakdown, **no** timezone handling. `login_time` stays server-local.
- **`get_report_data.php` is NOT modified.** The new endpoint is a sibling file.
- **Backend filter/WHERE logic is REUSED VERBATIM** from `get_report_data.php` — dept/course optional (incl. `"all"`/`"all departments"`/`"all courses"`, case-insensitive), `day`/`custom`/`month` use `DATE(v.login_time) BETWEEN ? AND ?`, `semester` uses raw `v.login_time BETWEEN ? AND ?` with the Philippine windows (first Jun 1–Oct 31 / second Nov 1–(year+1) Mar 31 / summer Apr 1–May 31). **Copy the `DATE()`-vs-raw asymmetry exactly — do NOT "fix" it**, or the time totals stop reconciling with the roster totals (spec §3). All filters bound with `bind_param`.
- **Disjoint error paths (spec §4.1/§5.2, load-bearing):** the time path gets a **dedicated** `timeAnalyticsError` signal + a dedicated VM slot that sets **only** `m_timeError`, **never** `m_errorText`. Reusing `reportError`/`setError` would blank the whole preview (`showPreview = hasResult && !isError`) and block export (`canExport` requires `m_errorText.isEmpty()`). A time failure is **localized** to the "When?" section; a rows failure is **fatal** to the whole operation. This asymmetry must never be inverted.
- **Three decoupled "busy" notions (spec §5.1):** operation-in-flight (gates `canGenerate`; clears only when BOTH children settle), rows `loading` (drives ONLY the main preview dim; clears at rows-settle), `timeLoading` (drives ONLY the "When?" spinner; clears at time-settle).
- **Contract validation happens at the `ReportController` parse boundary** (exact length 24/7, numeric) BEFORE anything reaches the pure aggregator; the aggregator's own length checks are a second, independent net.
- **Formatting boundary:** core deals only in indices + counts. Turning `14` into `"2–3 PM"` or `2` into `"Wednesday"` is the ViewModel's job, never core's.
- **Theme:** every visual token in the new QML via `Theme.qml`; ZERO raw hex outside `Theme.qml`; all labels/captions `qsTr` + `Text.PlainText` (cleartext-HTTP-derived-data rule).
- **MVVM:** `ReportingViewModel` is the ONLY QML-facing C++ for this screen; QML never calls a `witscore` controller directly.
- **No real student PII** in any test/fixture — synthetic data only.
- **Release gate (spec §8/§9):** OFFSCREEN QuickTests run under `QApplication` and cannot fully verify visual layout or the real network path. A manual `WITSQuick.exe` on-screen smoke is a mandatory release gate — Task 7. The backend has no in-repo PHP harness — it is verified by the manual `curl` + deploy procedure in Task 6.
- **Build (PowerShell; Qt tools NOT on PATH; external short build dir avoids the Windows MAX_PATH overflow on the QML module):**
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B C:/b/loams-4biva -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build C:/b/loams-4biva
  ctest --test-dir C:/b/loams-4biva --output-on-failure
  ```
  **Baseline:** run the full suite once at branch start and record the green count — **do not hard-code it**. Task 1 adds exactly one new target (`tst_timeanalytics`). Note the known `tst_settingsviewmodel` flake under full-suite parallel load (passes in isolation) — re-run it alone before treating a failure as real. Any CMakeLists change requires a **reconfigure** (`cmake -S … -B …`) before `--build`. **Close any running `WITSQuick.exe` before rebuilding** — it locks the exe against relink. Ignore the "LF will be replaced by CRLF" and pre-existing QXlsx "GuiPrivate target" warnings.

## File Structure

**Created:**
- `qt-app/core/timeanalytics.h` — `struct TimeAnalytics` + `static compute(...)` (Task 1).
- `qt-app/core/timeanalytics.cpp` — the pure aggregator (Task 1).
- `qt-app/tests/tst_timeanalytics.cpp` — pure-aggregator unit tests (Task 1).
- `deliverables/loams_api/get_report_time_data.php` — the new endpoint (Task 6); ALSO deployed to `C:/xampp/htdocs/loams_api/` (not in repo).

**Modified:**
- `qt-app/core/CMakeLists.txt` — add `timeanalytics.{h,cpp}` to the `witscore` source list (Task 1).
- `qt-app/tests/CMakeLists.txt` — add the `tst_timeanalytics` target with `timeanalytics.cpp/.h` in its SOURCES (Task 1).
- `qt-app/core/reportcontroller.h` / `.cpp` — `parseTimeAnalytics` static, `fetchTimeAnalytics`, `timeAnalyticsReady`/`timeAnalyticsError` signals (Task 2).
- `qt-app/tests/tst_reportcontroller.cpp` — parse-boundary tests (Task 2).
- `qt-app/quick/viewmodels/ReportingViewModel.h` / `.cpp` — parallel-fetch state machine (Task 3) + presentation population (Task 4).
- `qt-app/quick/tests/tst_reportingviewmodel.cpp` — state-machine tests (Task 3) + presentation tests (Task 4); a few existing tests updated for the both-settle model (Task 3).
- `qt-app/quick/qml/admin/ReportingScreen.qml` — the "When?" `LCard` section (Task 5).
- `qt-app/quick/tests/tst_qml_admin.qml` — reporting fixture stub + QuickTests + geometry bump (Task 5).

**Not touched:** `get_report_data.php`; legacy `qt-app/adminwindow.cpp` (WITS.exe is unaffected — it never calls `ReportController`).

**Metatype note (Task 2/3):** the `timeAnalyticsReady(QList<int>, QList<int>)` signal connects to a VM slot in the **same thread** (a direct connection), and every test drives the VM slots **directly** (the network-free seam already used for `onReportDataReady`). No `qRegisterMetaType<QList<int>>()` and no `QSignalSpy` on that signal are required.

---

### Task 1: `core/timeanalytics.{h,cpp}` — pure aggregator + `tst_timeanalytics` + CMake

**Files:**
- Create: `qt-app/core/timeanalytics.h`
- Create: `qt-app/core/timeanalytics.cpp`
- Create: `qt-app/tests/tst_timeanalytics.cpp`
- Modify: `qt-app/core/CMakeLists.txt`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces:**
- Produces: `struct TimeAnalytics { QList<int> hourly; QList<int> weekdayMonFirst; int peakHour; int peakHourCount; int peakWeekdayMonFirst; int peakWeekdayCount; bool hasData; };`
- Produces: `static TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour, const QList<int> &byWeekdaySunFirst);` — Sun→Mon reorder (`monFirst[i] = sunFirst[(i+1)%7]`), peak hour + peak weekday with **earliest-bucket-wins** tie-break, `hasData` = any count > 0, defensive on wrong-length input (returns `hasData=false`, no OOB).
- Consumed by: `ReportingViewModel` (Task 4) only. Never by QML.

- [ ] **Step 1: Write the failing test**

Create `qt-app/tests/tst_timeanalytics.cpp` (mirror `tst_reportanalytics.cpp`'s style: `QTEST_APPLESS_MAIN`, synthetic data only):

```cpp
#include <QtTest>
#include <QList>
#include "timeanalytics.h"

class TstTimeAnalytics : public QObject
{
    Q_OBJECT
private slots:
    void reorder_sundayFirstToMondayFirst();
    void peakHour_picksHighestIndexAndCount();
    void peakWeekday_picksHighestMonFirstIndexAndCount();
    void tieBreak_earliestBucketWins();
    void allZero_hasDataFalse();
    void hasData_trueWhenAnyCountPositive();
    void badLength_hasDataFalseNoCrash();

private:
    static QList<int> zeros(int n) {
        QList<int> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.append(0);
        return v;
    }
    // A valid dense 24-hour array with a single peak at 14 (2 PM).
    static QList<int> hoursPeak14() {
        QList<int> v = zeros(24);
        v[8] = 5; v[14] = 12; v[19] = 9;
        return v;
    }
    // A valid dense 7-weekday array, Sunday-first, no ties.
    static QList<int> weekSunFirst() { return QList<int>{2, 40, 8, 30, 8, 5, 1}; }
};

void TstTimeAnalytics::reorder_sundayFirstToMondayFirst() {
    QList<int> byHour = zeros(24); byHour[9] = 3;          // some data -> hasData true
    QList<int> byWeekday{10, 20, 30, 40, 50, 60, 70};      // Sun..Sat
    const TimeAnalytics a = TimeAnalytics::compute(byHour, byWeekday);
    QCOMPARE(a.weekdayMonFirst, (QList<int>{20, 30, 40, 50, 60, 70, 10})); // Mon..Sun
    QCOMPARE(a.hourly, byHour);                            // passed through unchanged
    QVERIFY(a.hasData);
}

void TstTimeAnalytics::peakHour_picksHighestIndexAndCount() {
    const TimeAnalytics a = TimeAnalytics::compute(hoursPeak14(), weekSunFirst());
    QCOMPARE(a.peakHour, 14);
    QCOMPARE(a.peakHourCount, 12);
}

void TstTimeAnalytics::peakWeekday_picksHighestMonFirstIndexAndCount() {
    // Sun-first {2,40,8,30,8,5,1}; Mon-first = [40,8,30,8,5,1,2]; peak idx0 = Monday.
    const TimeAnalytics a = TimeAnalytics::compute(hoursPeak14(), weekSunFirst());
    QCOMPARE(a.peakWeekdayMonFirst, 0);   // Monday
    QCOMPARE(a.peakWeekdayCount, 40);
}

void TstTimeAnalytics::tieBreak_earliestBucketWins() {
    QList<int> byHour = zeros(24); byHour[6] = 10; byHour[18] = 10;   // tie -> earliest (6)
    QList<int> byWeekday{0, 7, 0, 0, 0, 7, 0};   // Sun-first: Mon & Fri tie at 7
    const TimeAnalytics a = TimeAnalytics::compute(byHour, byWeekday);
    QCOMPARE(a.peakHour, 6);
    QCOMPARE(a.peakHourCount, 10);
    // Mon-first = [7,0,0,0,7,0,0]; earliest max is index 0 (Monday).
    QCOMPARE(a.peakWeekdayMonFirst, 0);
    QCOMPARE(a.peakWeekdayCount, 7);
}

void TstTimeAnalytics::allZero_hasDataFalse() {
    const TimeAnalytics a = TimeAnalytics::compute(zeros(24), zeros(7));
    QVERIFY(!a.hasData);
    QCOMPARE(a.peakHour, 0);
    QCOMPARE(a.peakHourCount, 0);
    QCOMPARE(a.peakWeekdayMonFirst, 0);
    QCOMPARE(a.peakWeekdayCount, 0);
    QCOMPARE(a.weekdayMonFirst.size(), 7);   // still dense/reordered
    QCOMPARE(a.hourly.size(), 24);
}

void TstTimeAnalytics::hasData_trueWhenAnyCountPositive() {
    QList<int> byHour = zeros(24); byHour[0] = 1;
    const TimeAnalytics a = TimeAnalytics::compute(byHour, zeros(7));
    QVERIFY(a.hasData);
}

void TstTimeAnalytics::badLength_hasDataFalseNoCrash() {
    const TimeAnalytics shortHour = TimeAnalytics::compute(zeros(23), zeros(7));
    QVERIFY(!shortHour.hasData);
    QVERIFY(shortHour.hourly.isEmpty());        // never populated on bad input
    const TimeAnalytics shortWeek = TimeAnalytics::compute(zeros(24), zeros(6));
    QVERIFY(!shortWeek.hasData);
    const TimeAnalytics emptyBoth = TimeAnalytics::compute(QList<int>{}, QList<int>{});
    QVERIFY(!emptyBoth.hasData);
}

QTEST_APPLESS_MAIN(TstTimeAnalytics)
#include "tst_timeanalytics.moc"
```

- [ ] **Step 2: Register the test target (and confirm it fails to build)**

In `qt-app/tests/CMakeLists.txt`, append at the end (after the `tst_reportanalytics` block, ~line 263) — this MUST list `timeanalytics.cpp/.h` in `SOURCES` because these `wits_add_qttest` targets compile the class-under-test directly and do NOT link `witscore` (the undefined-reference trap from the 4b-iii-b `reportanalytics` lesson):

```cmake

# --- "When?" time-of-visit analytics (pure core, no offscreen) ---
wits_add_qttest(tst_timeanalytics
    SOURCES
        tst_timeanalytics.cpp
        ${CMAKE_SOURCE_DIR}/core/timeanalytics.cpp
        ${CMAKE_SOURCE_DIR}/core/timeanalytics.h
    INCLUDES ${CMAKE_SOURCE_DIR}/core)
```

Reconfigure + build and confirm the RED (the test cannot compile/link — `timeanalytics.h` does not exist yet):
```
cmake -S qt-app -B C:/b/loams-4biva -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-4biva --target tst_timeanalytics
```
Expected: build FAILS — `timeanalytics.h: No such file or directory` (or undefined `TimeAnalytics::compute`).

- [ ] **Step 3: Create the header**

Create `qt-app/core/timeanalytics.h`:

```cpp
#ifndef TIMEANALYTICS_H
#define TIMEANALYTICS_H

#include <QList>

// "When?" time-of-visit analytics (spec 4b-iv-a §4.2). PURE and
// presentation-agnostic: deals only in indices + counts. All human-readable
// formatting (12-hour range, weekday name) is the ViewModel's job, never here —
// same rule as ReportAnalytics::compute in 4b-iii.
struct TimeAnalytics {
    QList<int> hourly;                    // 24 entries, index = hour 0..23 (as received)
    QList<int> weekdayMonFirst;          // 7 entries, index 0=Mon .. 6=Sun (reordered)
    int        peakHour            = 0;  // 0..23, hour bucket with the highest count
    int        peakHourCount       = 0;
    int        peakWeekdayMonFirst = 0;  // 0=Mon .. 6=Sun
    int        peakWeekdayCount    = 0;
    bool       hasData             = false;  // true iff any input count > 0

    // byHour: dense 24-element array, index = hour 0..23.
    // byWeekdaySunFirst: dense 7-element array, [0]=Sunday .. [6]=Saturday (the wire
    // format). Tie-break for both peaks: earliest/first bucket wins. Defensive: if
    // either array is not the expected length, returns hasData=false with no OOB access
    // (a second net beneath ReportController's parse-boundary validation, spec §4.2).
    static TimeAnalytics compute(const QList<int> &byHour,
                                 const QList<int> &byWeekdaySunFirst);
};

#endif // TIMEANALYTICS_H
```

- [ ] **Step 4: Create the implementation**

Create `qt-app/core/timeanalytics.cpp`:

```cpp
#include "timeanalytics.h"

TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour,
                                     const QList<int> &byWeekdaySunFirst)
{
    TimeAnalytics a;

    // Defensive length gate (spec §4.2). A malformed array bails with hasData=false
    // rather than indexing out of bounds — a second net under ReportController's
    // parse-boundary validation.
    if (byHour.size() != 24 || byWeekdaySunFirst.size() != 7)
        return a;

    a.hourly = byHour;

    // Reorder Sunday-first (wire) -> Monday-first (presentation):
    //   monFirst[i] = sunFirst[(i + 1) % 7]
    //   i=0 -> sun[1]=Mon ... i=5 -> sun[6]=Sat, i=6 -> sun[0]=Sun.
    a.weekdayMonFirst.reserve(7);
    for (int i = 0; i < 7; ++i)
        a.weekdayMonFirst.append(byWeekdaySunFirst.at((i + 1) % 7));

    // Peak hour — highest count; earliest bucket wins ties (strictly-greater keeps
    // the first max seen).
    for (int h = 0; h < 24; ++h) {
        if (byHour.at(h) > a.peakHourCount) {
            a.peakHourCount = byHour.at(h);
            a.peakHour = h;
        }
    }

    // Peak weekday — over the Monday-first array; earliest bucket wins ties.
    for (int d = 0; d < 7; ++d) {
        if (a.weekdayMonFirst.at(d) > a.peakWeekdayCount) {
            a.peakWeekdayCount = a.weekdayMonFirst.at(d);
            a.peakWeekdayMonFirst = d;
        }
    }

    // hasData = any count across either array > 0 (both agree in practice).
    for (int h = 0; h < 24 && !a.hasData; ++h)
        if (byHour.at(h) > 0) a.hasData = true;
    for (int d = 0; d < 7 && !a.hasData; ++d)
        if (byWeekdaySunFirst.at(d) > 0) a.hasData = true;

    return a;
}
```

- [ ] **Step 5: Add the source to `witscore`**

In `qt-app/core/CMakeLists.txt`, in the `add_library(witscore STATIC ...)` list, add the two files immediately after the `reportanalytics.h reportanalytics.cpp` line (~line 37):

```cmake
    reportanalytics.h reportanalytics.cpp
    timeanalytics.h timeanalytics.cpp
```

- [ ] **Step 6: Reconfigure, build, and run — verify GREEN**

```
cmake -S qt-app -B C:/b/loams-4biva -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-4biva
ctest --test-dir C:/b/loams-4biva -R tst_timeanalytics --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1` — all 7 `tst_timeanalytics` cases green.

- [ ] **Step 7: Run the full suite (no regressions)**

```
ctest --test-dir C:/b/loams-4biva --output-on-failure
```
Expected: all green (baseline + 1). If `tst_settingsviewmodel` fails, re-run it alone (`ctest --test-dir C:/b/loams-4biva -R tst_settingsviewmodel`) to confirm the known parallel-load flake.

- [ ] **Step 8: Commit**

```bash
git add qt-app/core/timeanalytics.h qt-app/core/timeanalytics.cpp qt-app/tests/tst_timeanalytics.cpp qt-app/core/CMakeLists.txt qt-app/tests/CMakeLists.txt
git commit -m "feat(reporting): TimeAnalytics core aggregator (Sun→Mon reorder, peak detection) for 4b-iv-a When? analytics"
```

---

### Task 2: `ReportController` — pure `parseTimeAnalytics` + `fetchTimeAnalytics` + dedicated signals

**Files:**
- Modify: `qt-app/core/reportcontroller.h`
- Modify: `qt-app/core/reportcontroller.cpp`
- Test: `qt-app/tests/tst_reportcontroller.cpp`

**Interfaces:**
- Produces: `static bool parseTimeAnalytics(const QByteArray &raw, QList<int> &outByHour, QList<int> &outByWeekday, QString &outError);` — valid ONLY if `status=="success"`, `byHour` has **exactly** 24 entries, `byWeekday` has **exactly** 7, and every entry parses as a non-negative int (JSON number OR numeric string). Any violation → returns `false`, arrays cleared, reason in `outError`.
- Produces: `void fetchTimeAnalytics(const QJsonObject &filters);` — POSTs `get_report_time_data.php` mirroring `fetchReportRows`; on finish routes through `parseTimeAnalytics`.
- Produces signals: `void timeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday);` (validated success only) and `void timeAnalyticsError(const QString &message);` (network failure, non-success status, OR contract-validation failure).
- Consumed by: `ReportingViewModel` (Task 3).
- **No CMake change:** `tst_reportcontroller` already compiles `reportcontroller.cpp` directly (`qt-app/tests/CMakeLists.txt` ~line 136–141); the new static/method are picked up automatically. `parseTimeAnalytics` does NOT use `TimeAnalytics`, so this target does not need `timeanalytics.cpp`.

- [ ] **Step 1: Write the failing tests**

In `qt-app/tests/tst_reportcontroller.cpp`, add `#include <QList>` near the top includes. Add these slots to the `private slots:` block (beside the other parsers):

```cpp
    // ---- parseTimeAnalytics (get_report_time_data.php) ----
    void parseTimeAnalytics_valid_fillsArrays();
    void parseTimeAnalytics_wrongLengthHour_fails();
    void parseTimeAnalytics_wrongLengthWeekday_fails();
    void parseTimeAnalytics_missingField_fails();
    void parseTimeAnalytics_nonNumeric_fails();
    void parseTimeAnalytics_stringEncodedCounts_ok();
    void parseTimeAnalytics_statusError_failsWithMessage();
```

Add two helpers to the `private:` section (beside `obj(...)`):

```cpp
    static QJsonArray numArray(int n, int fill = 0) {
        QJsonArray a; for (int i = 0; i < n; ++i) a.append(fill); return a;
    }
    static QByteArray timeObj(const QJsonArray &byHour, const QJsonArray &byWeekday,
                              const QString &status = QStringLiteral("success")) {
        QJsonObject o{{"status", status}, {"byHour", byHour}, {"byWeekday", byWeekday}};
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }
```

Add the bodies (place after `parsePreviewData_error_returnsEmpty`):

```cpp
void TstReportController::parseTimeAnalytics_valid_fillsArrays() {
    QJsonArray hours = numArray(24); hours[9] = 5; hours[14] = 12;
    QJsonArray week  = numArray(7);  week[1] = 40;      // Monday (Sun-first idx 1)
    QList<int> outH, outW; QString err;
    QVERIFY(ReportController::parseTimeAnalytics(timeObj(hours, week), outH, outW, err));
    QCOMPARE(outH.size(), 24);
    QCOMPARE(outW.size(), 7);
    QCOMPARE(outH.at(14), 12);
    QCOMPARE(outW.at(1), 40);
    QVERIFY(err.isEmpty());
}

void TstReportController::parseTimeAnalytics_wrongLengthHour_fails() {
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        timeObj(numArray(23), numArray(7)), outH, outW, err));
    QVERIFY(!err.isEmpty());
    QVERIFY(outH.isEmpty());     // nothing malformed handed downstream
    QVERIFY(outW.isEmpty());
}

void TstReportController::parseTimeAnalytics_wrongLengthWeekday_fails() {
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        timeObj(numArray(24), numArray(8)), outH, outW, err));
    QVERIFY(!err.isEmpty());
}

void TstReportController::parseTimeAnalytics_missingField_fails() {
    QJsonObject o{{"status", "success"}, {"byHour", numArray(24)}};   // no byWeekday
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        QJsonDocument(o).toJson(QJsonDocument::Compact), outH, outW, err));
}

void TstReportController::parseTimeAnalytics_nonNumeric_fails() {
    QJsonArray hours = numArray(24); hours[3] = "abc";               // non-numeric string
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        timeObj(hours, numArray(7)), outH, outW, err));
}

void TstReportController::parseTimeAnalytics_stringEncodedCounts_ok() {
    QJsonArray hours = numArray(24); hours[5] = "8";                 // numeric STRING
    QJsonArray week  = numArray(7);  week[2] = "15";
    QList<int> outH, outW; QString err;
    QVERIFY(ReportController::parseTimeAnalytics(timeObj(hours, week), outH, outW, err));
    QCOMPARE(outH.at(5), 8);
    QCOMPARE(outW.at(2), 15);
}

void TstReportController::parseTimeAnalytics_statusError_failsWithMessage() {
    QJsonObject o{{"status", "error"}, {"message", "boom"}};
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        QJsonDocument(o).toJson(QJsonDocument::Compact), outH, outW, err));
    QCOMPARE(err, QStringLiteral("boom"));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-4biva --target tst_reportcontroller
ctest --test-dir C:/b/loams-4biva -R tst_reportcontroller --output-on-failure
```
Expected: compile error — `parseTimeAnalytics` is not a member of `ReportController`.

- [ ] **Step 3: Declare the parser, method, and signals in the header**

In `qt-app/core/reportcontroller.h`:

Add `#include <QList>` beside the other Qt includes (after `#include <QByteArray>`, line 4).

Add the static parser beside `parsePreviewData` (in the "Pure, unit-testable statics" block, ~line 30):
```cpp
    // get_report_time_data.php: validates status + EXACT lengths (24/7) + numeric
    // entries (number OR numeric string). false on any violation, arrays cleared.
    static bool parseTimeAnalytics(const QByteArray &raw,
                                   QList<int> &outByHour,
                                   QList<int> &outByWeekday,
                                   QString &outError);
```

Add the async method beside `fetchReportRows` (~line 45):
```cpp
    void fetchTimeAnalytics(const QJsonObject &filters); // POST get_report_time_data.php
```

Add the signals beside `reportError` (~line 53):
```cpp
    void timeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday);
    void timeAnalyticsError(const QString &message);
```

- [ ] **Step 4: Implement the parser + the fetch method**

In `qt-app/core/reportcontroller.cpp`, add the parser after `parsePreviewData` (~line 135):

```cpp
// Pure, unit-testable parse boundary for get_report_time_data.php (spec §4.1).
// Valid ONLY if status=="success", byHour has EXACTLY 24 entries, byWeekday
// EXACTLY 7, and every entry parses as a non-negative integer (JSON number OR
// numeric string). Any violation -> false, arrays cleared, reason in outError.
bool ReportController::parseTimeAnalytics(const QByteArray &raw,
                                          QList<int> &outByHour,
                                          QList<int> &outByWeekday,
                                          QString &outError) {
    outByHour.clear();
    outByWeekday.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        outError = QStringLiteral("Invalid response from server.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value("status").toString() != QStringLiteral("success")) {
        const QString msg = obj.value("message").toString();
        outError = msg.isEmpty() ? QStringLiteral("Couldn't load visit times.") : msg;
        return false;
    }
    if (!obj.value("byHour").isArray() || !obj.value("byWeekday").isArray()) {
        outError = QStringLiteral("Malformed time analytics response.");
        return false;
    }
    const QJsonArray hourArr = obj.value("byHour").toArray();
    const QJsonArray weekArr = obj.value("byWeekday").toArray();
    if (hourArr.size() != 24 || weekArr.size() != 7) {
        outError = QStringLiteral("Malformed time analytics response.");
        return false;
    }

    // Robust non-negative-int coercion: JSON number OR numeric string.
    auto asCount = [](const QJsonValue &v, int &out) -> bool {
        if (v.isDouble()) {
            const int i = int(v.toDouble());
            if (i < 0) return false;
            out = i;
            return true;
        }
        if (v.isString()) {
            bool ok = false;
            const int i = v.toString().trimmed().toInt(&ok);
            if (!ok || i < 0) return false;
            out = i;
            return true;
        }
        return false;
    };

    QList<int> hours; hours.reserve(24);
    for (const QJsonValue &v : hourArr) {
        int c = 0;
        if (!asCount(v, c)) {
            outError = QStringLiteral("Malformed time analytics response.");
            return false;
        }
        hours.append(c);
    }
    QList<int> week; week.reserve(7);
    for (const QJsonValue &v : weekArr) {
        int c = 0;
        if (!asCount(v, c)) {
            outError = QStringLiteral("Malformed time analytics response.");
            return false;
        }
        week.append(c);
    }

    outByHour = hours;
    outByWeekday = week;
    outError.clear();
    return true;
}
```

Add the fetch method after `fetchReportRows` (~line 311, before `fetchPreviewData`):

```cpp
// Sibling of fetchReportRows for get_report_time_data.php (spec §4.1). Same
// request plumbing (POST, application/json, JSON body from filters). On finish it
// routes through the pure parseTimeAnalytics: success -> timeAnalyticsReady; any
// failure -> the DEDICATED timeAnalyticsError. This path is deliberately DISJOINT
// from reportError, which the VM routes to m_errorText (blanking the whole preview
// + blocking export) — a time hiccup must never do that (spec §4.1/§5.2).
void ReportController::fetchTimeAnalytics(const QJsonObject &filters) {
    QUrl url = ApiConfig::endpoint("get_report_time_data.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit timeAnalyticsError(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        QList<int> byHour, byWeekday;
        QString error;
        if (parseTimeAnalytics(resp, byHour, byWeekday, error))
            emit timeAnalyticsReady(byHour, byWeekday);
        else
            emit timeAnalyticsError(error);
    });
}
```

- [ ] **Step 5: Build and run — verify GREEN**

```
cmake --build C:/b/loams-4biva --target tst_reportcontroller
ctest --test-dir C:/b/loams-4biva -R tst_reportcontroller --output-on-failure
```
Expected: PASS — all `parseTimeAnalytics_*` cases green plus the pre-existing `tst_reportcontroller` cases.

- [ ] **Step 6: Commit**

```bash
git add qt-app/core/reportcontroller.h qt-app/core/reportcontroller.cpp qt-app/tests/tst_reportcontroller.cpp
git commit -m "feat(reporting): ReportController fetchTimeAnalytics + parseTimeAnalytics (dedicated time-error path) for 4b-iv-a"
```

---

### Task 3: `ReportingViewModel` — parallel-fetch state machine (rows + time as one operation)

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `ReportController::timeAnalyticsReady/timeAnalyticsError` (Task 2).
- Produces properties (all with NOTIFY): `bool hasTimeData`, `QString timeError`, `bool timeLoading`, `QString busiestHourLabel`, `QString busiestDayLabel`; and `BarsModel *hourlyBars` / `BarsModel *weekdayBars` (CONSTANT). In THIS task the two `BarsModel` and the two caption strings stay empty (`onTimeAnalyticsReady` only clears `timeError` and settles); full population lands in Task 4. Each commit still builds and its tests pass.
- Produces slots: `void onTimeAnalyticsReady(const QList<int>&, const QList<int>&)`, `void onTimeAnalyticsError(const QString&)`.
- **Reworks `canGenerate`** to gate on operation-in-flight (both children settled) instead of `m_loading`.
- **No CMake change:** `tst_reportingviewmodel` links `witsquickmodule` → `witscore` (PUBLIC), so `TimeAnalytics::compute` (needed in Task 4) is already reachable; Task 3 does not use it yet.

- [ ] **Step 1: Write the failing tests**

In `qt-app/quick/tests/tst_reportingviewmodel.cpp`, add these slot declarations to the `private slots:` block:

```cpp
    void timeSection_propertyDefaults();
    void generate_operationFinalizesOnlyWhenBothSettle();
    void generate_operationFinalizesRegardlessOfSettleOrder();
    void generate_rowsLoadingClearsAtRowsSettleIndependently();
    void outcome_rowsSuccessTimeError_reportRendersTimeErrorLocalized();
    void outcome_rowsErrorTimeSuccess_primaryErrorFires();
    void canExport_unaffectedByTimeOutcome();
    void resetAtGenerate_clearsStaleTimeState();
```

Add two dense-array helpers to the `private:` section (there is none yet — add a `private:` block after the slots, or reuse an existing one):

```cpp
private:
    static QList<int> denseHours() {          // valid 24-array, peak at 14 (2 PM)
        QList<int> v; v.reserve(24);
        for (int i = 0; i < 24; ++i) v.append(0);
        v[9] = 3; v[14] = 12;
        return v;
    }
    static QList<int> denseWeek() {            // Sun-first; Monday busiest (idx1=40)
        return QList<int>{2, 40, 8, 30, 8, 5, 1};
    }
    static QList<int> zeros(int n) {
        QList<int> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.append(0);
        return v;
    }
```

Add the bodies (place after `setIncludeRosterInExport_togglesAndSignals`):

```cpp
void TestReportingViewModel::timeSection_propertyDefaults() {
    ReportingViewModel vm;
    QVERIFY(vm.timeError().isEmpty());
    QVERIFY(!vm.timeLoading());
    QVERIFY(!vm.hasTimeData());
    QVERIFY(vm.busiestHourLabel().isEmpty());
    QVERIFY(vm.busiestDayLabel().isEmpty());
    QVERIFY(vm.hourlyBars() != nullptr);
    QVERIFY(vm.weekdayBars() != nullptr);
    // Before any Generate, canGenerate depends on filters only (no operation pending).
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::generate_operationFinalizesOnlyWhenBothSettle() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.generateReport();
    QVERIFY(!vm.canGenerate());                       // operation in flight
    vm.onReportDataReady(QJsonArray());               // only rows settle
    QVERIFY(!vm.canGenerate());                       // time still pending -> not finalized
    vm.onTimeAnalyticsReady(denseHours(), denseWeek()); // time settles
    QVERIFY(vm.canGenerate());                        // both settled -> finalized
}

void TestReportingViewModel::generate_operationFinalizesRegardlessOfSettleOrder() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onTimeAnalyticsError("x");                     // time settles FIRST
    QVERIFY(!vm.canGenerate());                       // rows still pending
    vm.onReportDataReady(QJsonArray());               // rows settle
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::generate_rowsLoadingClearsAtRowsSettleIndependently() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    QVERIFY(vm.loading());        // rows loading -> preview dim
    QVERIFY(vm.timeLoading());    // section spinner
    vm.onReportDataReady(QJsonArray());
    QVERIFY(!vm.loading());       // preview un-dims at rows settle...
    QVERIFY(vm.timeLoading());    // ...even though time is still running
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());
    QVERIFY(!vm.timeLoading());
}

void TestReportingViewModel::outcome_rowsSuccessTimeError_reportRendersTimeErrorLocalized() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","course":"BSIT","department":"CCS","visits":5}])").array());
    vm.onTimeAnalyticsError("network down");
    QVERIFY(vm.hasResult());                 // primary report still rendered
    QVERIFY(vm.errorText().isEmpty());       // NOT the fatal rows-error path
    QVERIFY(vm.canExport());                 // export unaffected by the time failure
    QCOMPARE(vm.timeError(), QStringLiteral("network down"));  // localized
    QVERIFY(!vm.hasTimeData());
}

void TestReportingViewModel::outcome_rowsErrorTimeSuccess_primaryErrorFires() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportError("Server 500", false);              // primary fatal path
    vm.onTimeAnalyticsReady(denseHours(), denseWeek()); // time succeeds anyway
    QCOMPARE(vm.errorText(), QStringLiteral("Server 500"));  // primary error fires
    QVERIFY(!vm.canExport());                           // rows error blocks export
    QVERIFY(vm.timeError().isEmpty());                  // time path had no error
    // (The "When?" section is gated on hasResult in QML — Task 5 — so it stays hidden here.)
}

void TestReportingViewModel::canExport_unaffectedByTimeOutcome() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","course":"BSIT","department":"CCS","visits":5}])").array());
    QVERIFY(vm.canExport());
    vm.onTimeAnalyticsError("boom");         // time error must not change canExport
    QVERIFY(vm.canExport());
}

void TestReportingViewModel::resetAtGenerate_clearsStaleTimeState() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsError("stale error");
    QCOMPARE(vm.timeError(), QStringLiteral("stale error"));
    // A second Generate must clear ALL When-section state before the new fetch fires.
    vm.setDay("2026-08-15");
    vm.generateReport();
    QVERIFY(vm.timeError().isEmpty());   // cleared at Generate start (staleness guard)
    QVERIFY(!vm.hasTimeData());
    QVERIFY(vm.timeLoading());           // section spinning again
}
```

**Update the two existing tests that assumed rows-settle finalizes the operation** (the both-settle model changes them):

Replace `generateWhileLoadingIsNoop()` body with:
```cpp
void TestReportingViewModel::generateWhileLoadingIsNoop()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.generateReport();                 // fires; operation in flight
    QVERIFY(vm.loading());
    QVERIFY(!vm.canGenerate());          // gated while the operation is in flight
    vm.generateReport();                 // no-op while in flight
    QVERIFY(vm.loading());
    // BOTH children must settle before the operation finalizes.
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(QList<int>{}, QList<int>{});
    QVERIFY(!vm.loading());
    QVERIFY(vm.canGenerate());
}
```
(`failedRefetchDisablesExportAndClearsRows` still passes unchanged — it asserts only `canExport`/`loading`, which are unaffected by the time path; the time child simply never settles in that test, which is harmless.)

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-4biva --target tst_reportingviewmodel
ctest --test-dir C:/b/loams-4biva -R tst_reportingviewmodel --output-on-failure
```
Expected: compile error — `timeError`, `timeLoading`, `hasTimeData`, `busiestHourLabel`, `busiestDayLabel`, `hourlyBars`, `weekdayBars`, `onTimeAnalyticsReady`, `onTimeAnalyticsError` are undeclared.

- [ ] **Step 3: Declare the new state in the header**

In `qt-app/quick/viewmodels/ReportingViewModel.h`:

Add the include beside the other core includes (after `#include "reportanalytics.h"`, line 15):
```cpp
#include "timeanalytics.h"        // TimeAnalytics — computed in applyResult's sibling path
```

Add the Q_PROPERTYs beside `includeRosterInExport` (~line 67):
```cpp
    Q_PROPERTY(bool hasTimeData READ hasTimeData NOTIFY hasTimeDataChanged)
    Q_PROPERTY(QString timeError READ timeError NOTIFY timeErrorChanged)
    Q_PROPERTY(bool timeLoading READ timeLoading NOTIFY timeLoadingChanged)
    Q_PROPERTY(QString busiestHourLabel READ busiestHourLabel NOTIFY busiestHourLabelChanged)
    Q_PROPERTY(QString busiestDayLabel READ busiestDayLabel NOTIFY busiestDayLabelChanged)
    Q_PROPERTY(BarsModel *hourlyBars READ hourlyBars CONSTANT)
    Q_PROPERTY(BarsModel *weekdayBars READ weekdayBars CONSTANT)
```

Add the getters beside `includeRosterInExport()` (~line 133):
```cpp
    bool hasTimeData() const { return m_hasTimeData; }
    QString timeError() const { return m_timeError; }
    bool timeLoading() const { return m_timeLoading; }
    QString busiestHourLabel() const { return m_busiestHourLabel; }
    QString busiestDayLabel() const { return m_busiestDayLabel; }
    BarsModel *hourlyBars() { return &m_hourlyBars; }
    BarsModel *weekdayBars() { return &m_weekdayBars; }
```

Add the two slots beside `onReportError` (in the "Public slots" block, ~line 161):
```cpp
    void onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday);
    void onTimeAnalyticsError(const QString &message);
```

Add the signals beside `includeRosterInExportChanged()` (~line 188):
```cpp
    void hasTimeDataChanged();
    void timeErrorChanged();
    void timeLoadingChanged();
    void busiestHourLabelChanged();
    void busiestDayLabelChanged();
```

Add the private helpers beside `setError` (~line 192):
```cpp
    void setTimeLoading(bool v);
    void resetTimeSection();          // clears ALL When-section state at Generate start
    bool operationInFlight() const;   // true until BOTH children settle
```

Add the members beside `m_includeRosterInExport` (~line 238):
```cpp
    BarsModel m_hourlyBars;
    BarsModel m_weekdayBars;
    TimeAnalytics m_timeAnalytics;
    QString m_busiestHourLabel;
    QString m_busiestDayLabel;
    bool m_hasTimeData = false;
    QString m_timeError;
    bool m_timeLoading = false;
    // Settle flags start TRUE = "no operation pending", so canGenerate works
    // before the first Generate. generateReport() sets both false; each child
    // flips its own true on settle (success OR failure).
    bool m_reportRowsSettled = true;
    bool m_timeAnalyticsSettled = true;
```

- [ ] **Step 4: Wire the controller signals + implement the state machine**

In `qt-app/quick/viewmodels/ReportingViewModel.cpp`:

**(a)** In the constructor, after the `connect(... reportError ...)` block (~line 33), add:
```cpp
    connect(m_controller, &ReportController::timeAnalyticsReady,
            this, &ReportingViewModel::onTimeAnalyticsReady);
    connect(m_controller, &ReportController::timeAnalyticsError,
            this, &ReportingViewModel::onTimeAnalyticsError);
```

**(b)** Replace `canGenerate()` (~line 214) so it gates on operation-in-flight, and add `operationInFlight()`:
```cpp
bool ReportingViewModel::operationInFlight() const
{
    return !(m_reportRowsSettled && m_timeAnalyticsSettled);
}

bool ReportingViewModel::canGenerate() const
{
    return filtersComplete() && !operationInFlight();
}
```

**(c)** Replace `generateReport()` (~line 285) so it resets the When-section, marks both children in flight, and fires BOTH fetches:
```cpp
void ReportingViewModel::generateReport()
{
    if (operationInFlight())          // single-in-flight = ONE operation
        return;
    if (!filtersComplete()) {
        setError(QStringLiteral("Complete the selected duration before generating a report."));
        m_validationError = true;
        return;
    }
    m_validationError = false;
    setError(QString());

    // Reset ALL When-section state BEFORE the fetches (staleness guard, spec §5.1):
    // without this, a re-run would show the previous run's captions/bars in flight.
    resetTimeSection();

    // Two child requests, ONE logical Generate operation.
    m_reportRowsSettled = false;
    m_timeAnalyticsSettled = false;
    setLoading(true);                 // rows loading -> main preview dim
    setTimeLoading(true);             // section spinner
    emit canGenerateChanged();        // operation now in flight

    const QJsonObject filters = buildFilters(
        m_department, m_course, m_durationType,
        parseDate(m_day), m_month, m_monthYear,
        m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd));
    m_controller->fetchReportRows(filters);
    m_controller->fetchTimeAnalytics(filters);   // parallel, same filters
}
```

**(d)** Update `onReportDataReady` (~line 335) and `onReportError` (~line 341) to flip the rows settle flag BEFORE `setLoading(false)` (so the `canGenerateChanged` it emits reflects the settled state):
```cpp
void ReportingViewModel::onReportDataReady(const QJsonArray &data)
{
    m_reportRowsSettled = true;
    setLoading(false);
    applyResult(data);
}

void ReportingViewModel::onReportError(const QString &message, bool /*critical*/)
{
    m_reportRowsSettled = true;
    setLoading(false);
    setError(message.isEmpty() ? QStringLiteral("Report failed. Please try again.") : message);
    m_validationError = false;   // a real fetch error, not a validation prompt
}
```

**(e)** Add the time slots + helpers. Place them after `onReportError` (or beside `setError`):
```cpp
void ReportingViewModel::onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday)
{
    // Task 3: settle the flag + clear timeError so the operation can finalize.
    // Full model/caption/hasTimeData population lands in Task 4.
    Q_UNUSED(byHour);
    Q_UNUSED(byWeekday);
    if (!m_timeError.isEmpty()) { m_timeError.clear(); emit timeErrorChanged(); }
    setTimeLoading(false);
    m_timeAnalyticsSettled = true;
    emit canGenerateChanged();
}

void ReportingViewModel::onTimeAnalyticsError(const QString &message)
{
    // Localized failure (spec §5.2): set ONLY m_timeError; NEVER m_errorText,
    // which would blank the whole preview + block export.
    m_timeError = message.isEmpty() ? QStringLiteral("Couldn't load visit times.") : message;
    emit timeErrorChanged();
    if (m_hasTimeData) { m_hasTimeData = false; emit hasTimeDataChanged(); }
    setTimeLoading(false);
    m_timeAnalyticsSettled = true;
    emit canGenerateChanged();
}

void ReportingViewModel::setTimeLoading(bool v)
{
    if (m_timeLoading == v) return;
    m_timeLoading = v;
    emit timeLoadingChanged();
}

void ReportingViewModel::resetTimeSection()
{
    m_timeAnalytics = TimeAnalytics();
    m_hourlyBars.setBars({});
    m_weekdayBars.setBars({});
    if (m_hasTimeData)              { m_hasTimeData = false;      emit hasTimeDataChanged(); }
    if (!m_busiestHourLabel.isEmpty()) { m_busiestHourLabel.clear(); emit busiestHourLabelChanged(); }
    if (!m_busiestDayLabel.isEmpty())  { m_busiestDayLabel.clear();  emit busiestDayLabelChanged(); }
    if (!m_timeError.isEmpty())    { m_timeError.clear();        emit timeErrorChanged(); }
}
```

- [ ] **Step 5: Build and run — verify GREEN**

```
cmake --build C:/b/loams-4biva --target tst_reportingviewmodel
ctest --test-dir C:/b/loams-4biva -R tst_reportingviewmodel --output-on-failure
```
Expected: PASS — the eight new state-machine cases green, the updated `generateWhileLoadingIsNoop` green, and every pre-existing case still green.

- [ ] **Step 6: Run the full suite (no regressions)**

```
ctest --test-dir C:/b/loams-4biva --output-on-failure
```
Expected: all green (re-run `tst_settingsviewmodel` alone if it flakes).

- [ ] **Step 7: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): VM parallel rows+time fetch as one Generate operation (three-flag loading, localized time-error)"
```

---

### Task 4: `ReportingViewModel` — presentation population (bar models + captions + hasTimeData)

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `TimeAnalytics::compute` (Task 1).
- Produces: `onTimeAnalyticsReady` now runs `compute`, populates `m_hourlyBars` (24 bars, interval-label blanking every 3h) and `m_weekdayBars` (7 bars Mon-first), sets `m_busiestHourLabel` (12-hour "2–3 PM" from `peakHour`), `m_busiestDayLabel` (full weekday name from `peakWeekdayMonFirst`), and `m_hasTimeData` (= `TimeAnalytics::hasData`). Captions are empty when `hasTimeData` is false.
- Produces private static helpers: `buildHourlyBars`, `buildWeekdayBars`, `hourTick`, `formatHourRange`, `weekdayName`.

- [ ] **Step 1: Write the failing tests**

In `qt-app/quick/tests/tst_reportingviewmodel.cpp`, add slot declarations:
```cpp
    void timeModels_populatedWithLabelBlanking();
    void captions_formattedForKnownPeaks();
    void hasTimeData_falseOnAllZeroShowsEmptyState();
```

Add the bodies (after `resetAtGenerate_clearsStaleTimeState`):
```cpp
void TestReportingViewModel::timeModels_populatedWithLabelBlanking() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());

    QCOMPARE(vm.hourlyBars()->rowCount(), 24);
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);

    // Interval x-labels: hours 0/3/6.. carry a label, off-interval hours are blank.
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(0, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("12A"));
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(3, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("3A"));
    QVERIFY(vm.hourlyBars()->data(vm.hourlyBars()->index(1, 0),
            BarsModel::LabelRole).toString().isEmpty());

    // Weekday bars are Monday-first; value at Mon = denseWeek() Sun-first idx1 = 40.
    QCOMPARE(vm.weekdayBars()->data(vm.weekdayBars()->index(0, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("Mon"));
    QCOMPARE(vm.weekdayBars()->data(vm.weekdayBars()->index(6, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("Sun"));
    QCOMPARE(vm.weekdayBars()->data(vm.weekdayBars()->index(0, 0),
             BarsModel::ValueRole).toInt(), 40);
}

void TestReportingViewModel::captions_formattedForKnownPeaks() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());   // peak hour 14, peak day Monday
    QVERIFY(vm.hasTimeData());
    QCOMPARE(vm.busiestHourLabel(), QStringLiteral("2\u20133 PM"));
    QCOMPARE(vm.busiestDayLabel(), QStringLiteral("Monday"));
}

void TestReportingViewModel::hasTimeData_falseOnAllZeroShowsEmptyState() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(zeros(24), zeros(7));
    QVERIFY(!vm.hasTimeData());
    QVERIFY(vm.busiestHourLabel().isEmpty());   // captions unused in the empty state
    QVERIFY(vm.busiestDayLabel().isEmpty());
    QCOMPARE(vm.hourlyBars()->rowCount(), 24);  // bars still dense (all zero)
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-4biva --target tst_reportingviewmodel
ctest --test-dir C:/b/loams-4biva -R tst_reportingviewmodel --output-on-failure
```
Expected: FAIL — `hourlyBars()` is empty (rowCount 0), captions empty (Task 3 stub does not populate them).

- [ ] **Step 3: Declare the presentation helpers in the header**

In `qt-app/quick/viewmodels/ReportingViewModel.h`, add beside the other private helpers (near `resetTimeSection`):
```cpp
    // Presentation shaping for the "When?" section (formatting lives HERE, not in
    // core — spec §5.4). Static + pure so they are directly unit-testable.
    static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly);        // 24, label blanked off-3h
    static QList<BarsModel::Bar> buildWeekdayBars(const QList<int> &weekdayMonFirst); // 7, Mon-first
    static QString hourTick(int hour);          // 0..23 -> "12A","3A",...,"9P"
    static QString formatHourRange(int hour);   // 14 -> "2–3 PM"
    static QString weekdayName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Monday".."Sunday"
```

- [ ] **Step 4: Rewrite `onTimeAnalyticsReady` + implement the helpers**

In `qt-app/quick/viewmodels/ReportingViewModel.cpp`, replace the Task-3 stub `onTimeAnalyticsReady` with the full version:
```cpp
void ReportingViewModel::onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday)
{
    m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday);

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

    if (!m_timeError.isEmpty()) { m_timeError.clear(); emit timeErrorChanged(); }

    setTimeLoading(false);
    m_timeAnalyticsSettled = true;
    emit canGenerateChanged();
}
```

Add the helper definitions (place near the other pure statics, e.g. after `aggregateVisitsByCourse`):
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

QString ReportingViewModel::hourTick(int hour)
{
    const int h12 = (hour % 12 == 0) ? 12 : (hour % 12);
    const QChar suffix = (hour < 12) ? QLatin1Char('A') : QLatin1Char('P');
    return QStringLiteral("%1%2").arg(h12).arg(suffix);
}

QString ReportingViewModel::formatHourRange(int hour)
{
    const int startH = hour;
    const int endH = (hour + 1) % 24;
    const auto to12 = [](int h) { return (h % 12 == 0) ? 12 : (h % 12); };
    const auto ampm = [](int h) { return h < 12 ? QStringLiteral("AM") : QStringLiteral("PM"); };
    // Same meridiem -> one suffix ("2–3 PM"); otherwise annotate both ("11 PM–12 AM").
    if (ampm(startH) == ampm(endH))
        return QStringLiteral("%1\u2013%2 %3").arg(to12(startH)).arg(to12(endH)).arg(ampm(startH));
    return QStringLiteral("%1 %2\u2013%3 %4")
            .arg(to12(startH)).arg(ampm(startH)).arg(to12(endH)).arg(ampm(endH));
}

QString ReportingViewModel::weekdayName(int monFirstIndex)
{
    static const char *const kNames[7] = { "Monday", "Tuesday", "Wednesday",
                                           "Thursday", "Friday", "Saturday", "Sunday" };
    if (monFirstIndex < 0 || monFirstIndex >= 7)
        return QString();
    return QString::fromLatin1(kNames[monFirstIndex]);
}
```

- [ ] **Step 5: Build and run — verify GREEN**

```
cmake --build C:/b/loams-4biva --target tst_reportingviewmodel
ctest --test-dir C:/b/loams-4biva -R tst_reportingviewmodel --output-on-failure
```
Expected: PASS — the three new presentation cases green plus all earlier `tst_reportingviewmodel` cases (including the Task-3 state-machine cases, which still hold: `onTimeAnalyticsReady` still settles the flag and clears `timeError`).

- [ ] **Step 6: Run the full suite (no regressions)**

```
ctest --test-dir C:/b/loams-4biva --output-on-failure
```
Expected: all green.

- [ ] **Step 7: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): VM populates hourly/weekday bar models + 12-hour and weekday captions for When? section"
```

---

### Task 5: `ReportingScreen.qml` — "When do students visit?" section + QuickTests

**Files:**
- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: `vm.hourlyBars`, `vm.weekdayBars` (BarsModel), `vm.busiestHourLabel`, `vm.busiestDayLabel`, `vm.hasTimeData`, `vm.timeError`, `vm.timeLoading` (Tasks 3–4).
- Produces: an `objectName: "whenSection"` `LCard` inside the preview column, with `whenLoading` / `whenError` / `whenEmpty` / `whenData` state subtrees, two `LBarChart`s (`hourlyChart`, `weekdayChart`) and two captions (`busiestHourCaption`, `busiestDayCaption`).
- **Placement + gating rationale:** the section is added **inside** the existing preview `ColumnLayout` (the one at `ReportingScreen.qml:198`, `visible: screen.showPreview`, `opacity: screen.isLoading ? 0.4 : 1.0`), immediately after the `rankingsRow` (`objectName: "rankingsRow"`, closes ~line 362) and before the `viewRosterToggle`. That puts it **below** the KPI band / rankings / course chart (spec §6), gates it on `showPreview = hasResult && !isError` (so it is hidden on a rows-error, matching spec §5.2 "When not shown"), and lets the preview un-dim to full opacity when **rows** settle while the section shows its OWN spinner bound to `timeLoading`. The card's `visible` is bound to `screen.hasRows` so it collapses on a zero-row result exactly like the chart/rankings do (keeping `test_dashboard_kpiBandCollapsesOnEmptyResult`'s "one empty state" invariant); `hasRows` implies `hasResult`, so this is consistent with the spec's "gated on hasResult".

- [ ] **Step 1: Write the failing QuickTests**

In `qt-app/quick/tests/tst_qml_admin.qml`:

**(a) Add two stub bar models** near `reportBarsStub` (~line 2513), for the LBarCharts to bind:
```qml
    ListModel { id: whenHourlyStub
        property real maxValue: 12
        ListElement { label: "12A"; value: 0 }
        ListElement { label: ""; value: 3 }
        ListElement { label: "3A"; value: 12 }
    }
    ListModel { id: whenWeekdayStub
        property real maxValue: 40
        ListElement { label: "Mon"; value: 40 }
        ListElement { label: "Tue"; value: 8 }
        ListElement { label: "Sun"; value: 2 }
    }
```

**(b) Extend `reportingStub`** (after `chartType`, ~line 2562) with the time-section props:
```qml
        property bool timeLoading: false
        property string timeError: ""
        property bool hasTimeData: true
        property string busiestHourLabel: "2\u20133 PM"
        property string busiestDayLabel: "Monday"
        property var hourlyBars: whenHourlyStub
        property var weekdayBars: whenWeekdayStub
```

**(c) Grow the reporting fixture band** so the taller screen (now with the "When?" card) keeps its clickable controls inside the rendered Flickable band — the reporting body is a clipping `Flickable` (`reportScroll`), and `mouseClick`/`findChild` hit-testing on the export bar controls (used by the existing `test_generateInvokesVm`, `test_exportButtonsFireVmMethods`, `test_includeRosterCheckboxDefaultsUncheckedAndWritesVm`, `test_paletteComboHasAccessibleNameAndWrites`, roster toggle) fails on a control scrolled out of the band. Update:
- The `reporting` instance (`ReportingScreen { id: reporting; ... height: 1120 ... }`, ~line 2592): `height: 1120` → `height: 1500`.
- The host root (`width: 1100; height: 8420`, ~line 18): `height: 8420` → `height: 8800`.
- The geometry-ledger comment (~line 15): `reporting 7300..8420` → `reporting 7300..8800`.
(`vmlessReporting` at `x: 2000` is a separate column and needs no change — `7300..8300` still fits within `8800`.)

**(d) Reset the new stub props in the reporting `TestCase` `init()`** (~line 2599, beside the existing resets) for order-independence:
```qml
            reportingStub.timeLoading = false;
            reportingStub.timeError = "";
            reportingStub.hasTimeData = true;
```

**(e) Add the tests** to the `ReportingScreen` `TestCase` (after `test_exportErrorPersistsAsFeedback`, ~line 2712):
```qml
        function test_whenSection_rendersChartsWithData() {
            reportingStub.hasResult = true;
            reportingStub.timeLoading = false;
            reportingStub.timeError = "";
            reportingStub.hasTimeData = true;
            var section = findChild(reporting, "whenSection");
            verify(section, "when-section card exists");
            verify(section.visible, "section visible with a non-empty result");
            var data = findChild(reporting, "whenData");
            verify(data.visible, "data subtree visible in success+data state");
            verify(findChild(reporting, "hourlyChart"), "hourly chart exists");
            verify(findChild(reporting, "weekdayChart"), "weekday chart exists");
            var hourCap = findChild(reporting, "busiestHourCaption");
            verify(hourCap.text.indexOf("2\u20133 PM") >= 0, "hour caption reflects busiestHourLabel");
            var dayCap = findChild(reporting, "busiestDayCaption");
            verify(dayCap.text.indexOf("Monday") >= 0, "day caption reflects busiestDayLabel");
        }

        function test_whenSection_emptyState() {
            reportingStub.timeLoading = false;
            reportingStub.timeError = "";
            reportingStub.hasTimeData = false;
            var empty = findChild(reporting, "whenEmpty");
            verify(empty, "empty-state element exists");
            verify(empty.visible, "empty state shows on success + all-zero");
            verify(!findChild(reporting, "whenData").visible, "data subtree hidden in empty state");
            reportingStub.hasTimeData = true;   // restore
        }

        function test_whenSection_inlineError() {
            reportingStub.timeLoading = false;
            reportingStub.timeError = "network down";
            var err = findChild(reporting, "whenError");
            verify(err, "inline error element exists");
            verify(err.visible, "inline error shows when timeError set");
            // Localized: the rest of the report stays visible above the section.
            verify(findChild(reporting, "kpiBand").visible, "KPI band still visible during a time-only error");
            reportingStub.timeError = "";   // restore
        }

        function test_whenSection_gatedOffOnEmptyResult() {
            reportingStub.hasResult = true;
            reportRowsStub.clear();          // 0 rows -> analytics scaffolding collapses
            var section = findChild(reporting, "whenSection");
            verify(section, "section element exists");
            compare(section.visible, false, "when-section hidden on 0 rows (collapses with chart/rankings)");
            // reportRowsStub is restored by cleanup()
        }
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-4biva --target tst_qml_admin
ctest --test-dir C:/b/loams-4biva -R tst_qml_admin --output-on-failure
```
Expected: FAIL — `findChild(reporting, "whenSection")` returns null.

- [ ] **Step 3: Add the "When?" section to `ReportingScreen.qml`**

In `qt-app/quick/qml/admin/ReportingScreen.qml`, inside the preview `ColumnLayout` (the one at line 198), immediately AFTER the `rankingsRow` `RowLayout` closes (`}` at ~line 362) and BEFORE the `LButton { objectName: "viewRosterToggle" ... }`, insert:

```qml
            // --- "When do students visit?" (spec 4b-iv-a §6) — below KPIs/
            // rankings/course chart, gated with the rest of the analytics
            // scaffolding on hasRows. Its OWN state (spinner / data / empty /
            // inline error) reflects only the time request's outcome. ---
            LCard {
                objectName: "whenSection"
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                visible: screen.hasRows

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("When do students visit?")
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                        font.bold: true
                    }

                    // Loading: the section's OWN spinner, bound to timeLoading —
                    // NOT the operation flag and NOT the rows loading/opacity, so
                    // the main report renders at full opacity as soon as rows
                    // settle while only this section keeps spinning (spec §6).
                    Item {
                        objectName: "whenLoading"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: screen.vm ? screen.vm.timeLoading : false
                        BusyIndicator { anchors.centerIn: parent; running: parent.visible }
                    }

                    // Inline time-error (localized — the rest of the report above
                    // still renders, spec §5.2).
                    Text {
                        objectName: "whenError"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: screen.vm ? (!screen.vm.timeLoading && screen.vm.timeError.length > 0) : false
                        text: qsTr("Couldn't load visit times")
                        textFormat: Text.PlainText
                        color: Theme.error
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        verticalAlignment: Text.AlignVCenter
                    }

                    // Success + all-zero: empty state.
                    Text {
                        objectName: "whenEmpty"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: screen.vm ? (!screen.vm.timeLoading
                                              && screen.vm.timeError.length === 0
                                              && !screen.vm.hasTimeData) : false
                        text: qsTr("No visit activity in this range")
                        textFormat: Text.PlainText
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        verticalAlignment: Text.AlignVCenter
                    }

                    // Success + data: both charts + captions. Captions live INSIDE
                    // this subtree (not merely hidden by opacity), so the "Busiest:"
                    // strings never render in the loading/empty/error states where
                    // the peak indices are meaningless (spec §6).
                    ColumnLayout {
                        objectName: "whenData"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.spacing.sm
                        visible: screen.vm ? (!screen.vm.timeLoading
                                              && screen.vm.timeError.length === 0
                                              && screen.vm.hasTimeData) : false

                        Text {
                            text: qsTr("Peak hours"); textFormat: Text.PlainText
                            color: Theme.mutedTextCaption; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.eyebrow
                        }
                        LBarChart {
                            objectName: "hourlyChart"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90
                            orientation: "Vertical"
                            model: screen.vm ? screen.vm.hourlyBars : null
                            maxValue: (screen.vm && screen.vm.hourlyBars) ? screen.vm.hourlyBars.maxValue : 100
                        }
                        Text {
                            objectName: "busiestHourCaption"
                            text: qsTr("Busiest: %1").arg(screen.vm ? screen.vm.busiestHourLabel : "")
                            textFormat: Text.PlainText
                            color: Theme.text; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }

                        Text {
                            text: qsTr("Busiest days"); textFormat: Text.PlainText
                            color: Theme.mutedTextCaption; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.eyebrow
                        }
                        LBarChart {
                            objectName: "weekdayChart"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90
                            orientation: "Vertical"
                            model: screen.vm ? screen.vm.weekdayBars : null
                            maxValue: (screen.vm && screen.vm.weekdayBars) ? screen.vm.weekdayBars.maxValue : 100
                        }
                        Text {
                            objectName: "busiestDayCaption"
                            text: qsTr("Busiest: %1").arg(screen.vm ? screen.vm.busiestDayLabel : "")
                            textFormat: Text.PlainText
                            color: Theme.text; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
                    }
                }
            }
```

- [ ] **Step 4: Build and run — verify GREEN**

```
cmake --build C:/b/loams-4biva --target tst_qml_admin
ctest --test-dir C:/b/loams-4biva -R tst_qml_admin --output-on-failure
```
Expected: PASS — the four new `test_whenSection_*` cases green AND the pre-existing reporting QuickTests still green (the geometry bump keeps their `mouseClick` targets inside the rendered band).

- [ ] **Step 5: Run the full suite (no regressions)**

```
ctest --test-dir C:/b/loams-4biva --output-on-failure
```
Expected: all green.

- [ ] **Step 6: Commit**

```bash
git add qt-app/quick/qml/admin/ReportingScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(reporting): When? section on ReportingScreen (peak-hours + busiest-days charts, own loading/empty/error states)"
```

---

### Task 6: Backend `get_report_time_data.php` — create, verify, deploy (manual/owner-runnable)

**This is a manual task — there is no in-repo PHP test harness (spec §8).** The steps are: write the file, `php -l`, back up the XAMPP target, `curl`-verify, then deploy. Route any failure through `superpowers:systematic-debugging`.

**Files:**
- Create: `deliverables/loams_api/get_report_time_data.php`
- Deploy (not in repo): `C:/xampp/htdocs/loams_api/get_report_time_data.php`

**Interfaces:**
- Consumes: POST JSON body `{department, course, durationType, start, end, year, semester}` — the SAME shape `fetchTimeAnalytics` sends (identical to `get_report_data.php`).
- Produces: `{"status":"success","byHour":[24 ints],"byWeekday":[7 ints]}` (byWeekday `[0]`=Sunday), or `{"status":"error","message":"..."}`.

- [ ] **Step 1: Create the endpoint file**

Create `deliverables/loams_api/get_report_time_data.php` — filter/WHERE logic copied VERBATIM from `get_report_data.php` (incl. the `DATE()`-vs-raw asymmetry), `library_visits v INNER JOIN students s`, two grouped aggregations, dense output, and the same `get_result`/`bind_result` fallback + `bind_param` pattern:

```php
<?php
header('Content-Type: application/json');
include 'db.php'; // must set $conn = new mysqli(...);

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    echo json_encode(['status' => 'error', 'message' => 'Invalid request method']);
    exit;
}

$raw = file_get_contents('php://input');
$data = json_decode($raw, true);
if (!is_array($data)) {
    echo json_encode(['status' => 'error', 'message' => 'Invalid input']);
    exit;
}

// Sanitize incoming filters — identical shape to get_report_data.php.
$department   = isset($data['department'])   ? trim($data['department'])   : '';
$course       = isset($data['course'])       ? trim($data['course'])       : '';
$durationType = isset($data['durationType']) ? trim($data['durationType']) : '';
$start        = isset($data['start'])        ? trim($data['start'])        : '';
$end          = isset($data['end'])          ? trim($data['end'])          : '';
$year         = isset($data['year'])         ? intval($data['year'])       : 0;
$semester     = isset($data['semester'])     ? trim($data['semester'])     : '';

// Build the shared WHERE clause + bound params ONCE, then reuse for both
// aggregations. Filter/WHERE logic is REUSED VERBATIM from get_report_data.php,
// INCLUDING the DATE()-vs-raw asymmetry (day/custom/month use DATE(v.login_time);
// semester compares the raw datetime against date-only bounds). Do NOT "fix" it —
// matching the roster endpoint verbatim is what makes byHour totals reconcile
// with the roster totals for the same filters (spec §3).
$where  = " WHERE 1=1";
$params = [];
$types  = "";

// Department filter (optional — empty/"All" = all departments)
if ($department !== '' && !in_array(strtolower($department), ['all', 'all departments'])) {
    $where .= " AND s.department = ?";
    $params[] = $department;
    $types   .= "s";
}

// Course filter
if ($course !== '' && !in_array(strtolower($course), ['all', 'all courses'])) {
    $where .= " AND s.course = ?";
    $params[] = $course;
    $types   .= "s";
}

// Duration filters (verbatim from get_report_data.php)
if (($durationType === 'day' || $durationType === 'custom' || $durationType === 'month')
    && $start !== '' && $end !== '') {
    $where .= " AND DATE(v.login_time) BETWEEN ? AND ?";
    $params[] = $start;
    $params[] = $end;
    $types  .= "ss";
}
elseif ($durationType === 'semester' && !empty($semester) && $year > 0) {
    $sem = strtolower($semester);
    if (strpos($sem, '1') !== false || stripos($sem, 'first') !== false) {
        $where .= " AND v.login_time BETWEEN ? AND ?";
        $params[] = "$year-06-01";
        $params[] = "$year-10-31";
        $types  .= "ss";
    } elseif (strpos($sem, '2') !== false || stripos($sem, 'second') !== false) {
        $where .= " AND v.login_time BETWEEN ? AND ?";
        $params[] = "$year-11-01";
        $params[] = ($year + 1) . "-03-31";
        $types  .= "ss";
    } elseif (stripos($sem, 'summer') !== false) {
        $where .= " AND v.login_time BETWEEN ? AND ?";
        $params[] = "$year-04-01";
        $params[] = "$year-05-31";
        $types  .= "ss";
    }
}

// Run a grouped COUNT aggregation with the shared params; returns
// [true, [bucket => count]] or [false, errorMessage]. Uses the same
// get_result/bind_result fallback as get_report_data.php.
function runAggregation($conn, $sql, $types, $params) {
    $stmt = $conn->prepare($sql);
    if ($stmt === false) {
        return [false, 'SQL prepare error: ' . $conn->error];
    }
    if (count($params) > 0) {
        $bindParams = [];
        $bindParams[] = &$types;
        for ($i = 0; $i < count($params); $i++) {
            $bindParams[] = &$params[$i];
        }
        call_user_func_array([$stmt, 'bind_param'], $bindParams);
    }
    if (!$stmt->execute()) {
        $err = $stmt->error;
        $stmt->close();
        return [false, 'Execute failed: ' . $err];
    }
    $out = [];
    if (method_exists($stmt, 'get_result')) {
        $result = $stmt->get_result();
        while ($r = $result->fetch_assoc()) {
            $out[(int)$r['bucket']] = (int)$r['cnt'];
        }
    } else {
        $bucket = null; $cnt = null;
        $stmt->bind_result($bucket, $cnt);
        while ($stmt->fetch()) {
            $out[(int)$bucket] = (int)$cnt;
        }
    }
    $stmt->close();
    return [true, $out];
}

// INNER JOIN so student-table dept/course filters apply and visits from
// since-deleted / unmatched students never appear (spec §3).
$base = " FROM library_visits v INNER JOIN students s ON s.school_id = v.student_id";

// byHour: GROUP BY HOUR(v.login_time)
$sqlHour = "SELECT HOUR(v.login_time) AS bucket, COUNT(*) AS cnt" . $base . $where
         . " GROUP BY HOUR(v.login_time)";
list($okH, $resH) = runAggregation($conn, $sqlHour, $types, $params);
if (!$okH) {
    echo json_encode(['status' => 'error', 'message' => $resH]);
    exit;
}

// byWeekday: GROUP BY DAYOFWEEK(v.login_time)
$sqlDow = "SELECT DAYOFWEEK(v.login_time) AS bucket, COUNT(*) AS cnt" . $base . $where
        . " GROUP BY DAYOFWEEK(v.login_time)";
list($okD, $resD) = runAggregation($conn, $sqlDow, $types, $params);
if (!$okD) {
    echo json_encode(['status' => 'error', 'message' => $resD]);
    exit;
}

// Densify to fixed-length arrays; any missing bucket = 0.
$byHour = array_fill(0, 24, 0);           // index 0..23 = hour of day
foreach ($resH as $h => $c) {
    if ($h >= 0 && $h <= 23) $byHour[$h] = $c;
}

// DAYOFWEEK 1=Sunday … 7=Saturday -> 0-based index i = dow - 1, so [0]=Sunday.
$byWeekday = array_fill(0, 7, 0);
foreach ($resD as $dow => $c) {
    $i = $dow - 1;
    if ($i >= 0 && $i <= 6) $byWeekday[$i] = $c;
}

echo json_encode([
    'status'    => 'success',
    'byHour'    => array_values($byHour),
    'byWeekday' => array_values($byWeekday),
]);
```

- [ ] **Step 2: Syntax-check**

```
& "C:/xampp/php/php.exe" -l "deliverables/loams_api/get_report_time_data.php"
```
Expected: `No syntax errors detected`.

- [ ] **Step 3: Back up the XAMPP target directory**

```
Copy-Item -Recurse -Force "C:/xampp/htdocs/loams_api" "C:/xampp/htdocs/loams_api.bak-4biva"
```
(A full-directory backup so the endpoint can be rolled back if a `curl` check fails.)

- [ ] **Step 4: Deploy the file to XAMPP, then `curl`-verify (with XAMPP/MySQL running)**

Copy the file in:
```
Copy-Item -Force "deliverables/loams_api/get_report_time_data.php" "C:/xampp/htdocs/loams_api/get_report_time_data.php"
```

Run the checks (use `curl.exe` explicitly — the PowerShell `curl` alias is `Invoke-WebRequest`):

1. **All departments, wide range** — expect 24 + 7 dense integer arrays:
   ```
   curl.exe -s -X POST http://localhost/loams_api/get_report_time_data.php -H "Content-Type: application/json" -d "{\"department\":\"\",\"durationType\":\"custom\",\"start\":\"2020-01-01\",\"end\":\"2035-12-31\"}"
   ```
2. **A specific department** — same call with `\"department\":\"<a real dept>\"`; confirm the counts narrow relative to (1).
3. **A range known to contain visits** — confirm non-zero buckets land where a manual look at `library_visits` says they should.
4. **A range known to contain NO visits** (e.g. a far-future window) — confirm `status:success` with all-zero 24/7 arrays, **not** an error and **not** a short array:
   ```
   curl.exe -s -X POST http://localhost/loams_api/get_report_time_data.php -H "Content-Type: application/json" -d "{\"department\":\"\",\"durationType\":\"custom\",\"start\":\"2099-01-01\",\"end\":\"2099-12-31\"}"
   ```

For each response confirm `byHour` has exactly 24 entries and `byWeekday` exactly 7, all integers. If any check fails, fix the PHP, re-run `php -l`, re-copy, and re-verify before proceeding — do not leave a broken endpoint deployed.

- [ ] **Step 5: Commit the endpoint (source only — the XAMPP copy is a deploy artifact)**

```bash
git add deliverables/loams_api/get_report_time_data.php
git commit -m "feat(loams_api): get_report_time_data.php — byHour[24]/byWeekday[7] aggregation for When? analytics"
```
Record the `curl` results (pass/fail + a sample response) for the PR body.

---

### Task 7: Manual `WITSQuick.exe` on-screen smoke — RELEASE GATE (no code)

**Mandatory owner-run release gate (spec §8/§9).** OFFSCREEN QuickTests cannot verify visual layout or the real network path. Do not claim the slice complete until this passes. Requires Task 6 deployed and XAMPP/MySQL running.

- [ ] **Step 1: Build and launch the real app**

```
cmake --build C:/b/loams-4biva
```
Run `C:/b/loams-4biva/quick/WITSQuick.exe` (close any other running `WITSQuick.exe` first — it locks the exe). Sign in to Admin → Reporting.

- [ ] **Step 2: Generate a report with data + verify the "When?" section**

Pick a department (or All Departments) + a duration with known data, Generate. Confirm the main dashboard renders (KPI band, rankings, course chart), and below them the **"When do students visit?"** section shows BOTH charts (24 hourly bars with ~every-3h x-labels; 7 Monday-first day bars) with the **"Busiest: …"** captions populated.

- [ ] **Step 3: Verify the empty state**

Generate for a range with visits present in the roster but no visit activity, OR a far range — confirm the section shows **"No visit activity in this range"** instead of charts, while the rest of the report renders normally.

- [ ] **Step 4: Verify graceful degradation (localized time error)**

Simulate/observe a time-fetch failure (e.g. temporarily rename the deployed `get_report_time_data.php`, Generate, then restore it). Confirm: the main report (KPIs, rankings, course chart, roster) renders normally; the "When?" section shows the inline **"Couldn't load visit times"**; and **export is NOT blocked** (PDF/Excel/Print still enabled). Confirm the reverse too: a rows failure surfaces the primary error and the "When?" section is not shown.

- [ ] **Step 5: Verify the decoupled loading**

On Generate, confirm the main report is **not** dimmed while only the "When?" section spins — i.e. the report un-dims as soon as rows arrive even if the time fetch is still running.

- [ ] **Step 6: Record the result**

Note pass/fail (and any layout issues) for the PR body. On failure, route through `superpowers:systematic-debugging` (reproduce with a failing test where possible) before fixing.

---

## Post-implementation (outside the task loop)

1. **`/claude-review`** (branch mode) — fix Critical/Important, resubmit until APPROVE or 3 rounds. (Codex CLI is not installed on this machine; use `/claude-review`.)
2. **`create-pr`** — the project **3-agent** gate (`dry-checker`, `security-reviewer`, `general-code-reviewer`; **no `api-checker`** — if a loaded `create-pr` names a fourth agent, re-read `.claude/skills/create-pr/SKILL.md` and follow the project copy). PRs target **master**.
3. Owner merges via `/merge-pr` (PR-open ≠ merge approval — ask separately).

**Deferred / forward (not this slice):** **4b-iv-b** — the same time analytics rendered into the PDF/Excel exports (extend `ReportRenderer::paintReport` / `writeReportToXlsx` from the computed `TimeAnalytics`, preserving screen/export parity); hour×day heatmap; per-course / per-department time breakdowns; timezone handling.
