# LOAMS 2.0 — Library-Hours Windowing for the "When?" Hourly Analytics (Design Spec)

**Date:** 2026-08-31
**Status:** Design approved (owner). Ready for `superpowers:writing-plans`.
**Depends on:** 4b-iv-a (on-screen "When?" analytics, merged), 4b-iv-b (export "When?" analytics, merged PR #45). Backend: **no change** — `get_report_time_data.php` keeps returning a dense 24-hour array; this slice adds no endpoint and issues no request.

**One-line summary:** crop the "When?" hourly time-analytics — on screen *and* in the PDF/Excel exports — to the librarian-set library-hours window `[openHour, closeHour]` instead of a fixed 24 hours, so the hourly chart shows only the hours the library is actually open. **Closes open follow-up #3 from the 4b-iv-b smoke.**

---

## 1. Problem & goal

The 4b-iv-a/4b-iv-b "When do students visit?" analytics render the hourly distribution as a **fixed 24-bar** series (midnight→11 PM). For a library open 7 AM–9 PM, roughly ten of those bars are structurally always zero (the closed hours), the x-axis wastes half its width on dead buckets, and — because of every-3rd-hour label thinning on screen — the open/close boundaries are usually *unlabeled*. The 4b-iv-b smoke logged this as follow-up #3: the hourly chart should span the hours the library is open, not the clock.

Meanwhile the app **already** crops another hourly visualization to library hours: the legacy per-course line chart, `ReportRenderer::makeLineChartImage(..., int openHour, int closeHour)`, restricts its x-axis with `axisX->setRange(openHour, closeHour)` and iterates `for (int h = openHour; h <= closeHour; ++h)` (`reportrenderer.cpp:323`, `:335`). Library-hours filtering is likewise already a **core** concern: `ReportRenderer::aggregateVisitsByCourseHour(const QJsonArray &, int openHour, int closeHour)` drops any visit whose hour is `< openHour || > closeHour` (`reportrenderer.cpp:108`). The "When?" hourly path is the one hourly surface that still ignores the window.

**Goal:** make the "When?" hourly analytics span `[openHour, closeHour]` inclusive — the same window source and boundary convention the export line chart already uses — everywhere the hourly distribution is shown: the on-screen `ReportingScreen.qml` "When do students visit?" section, the PDF hourly bar chart, and the Excel hourly table.

**Non-goal / scope guard:** this is a **pure client-side crop**. No backend change, no new endpoint, no new source files, no new CMake targets. The weekday (Mon→Sun) chart is untouched — it is not hour-based. KPIs, rankings, roster, totals, and `hasData` semantics are untouched.

## 2. Context — how the 24-hour hourly path works today

Three surfaces render the hourly distribution; all three currently assume a dense 24-wide series.

### 2.1 Core — `TimeAnalytics::compute` (`qt-app/core/timeanalytics.{h,cpp}`)

Current signature:

```cpp
static TimeAnalytics compute(const QList<int> &byHour,
                             const QList<int> &byWeekdaySunFirst);
```

It defensively bails (`hasData=false`, no OOB) if `byHour.size() != 24 || byWeekdaySunFirst.size() != 7` (`timeanalytics.cpp:11`), copies `byHour` verbatim into `hourly`, reorders the weekday array Sun-first→Mon-first, and scans for `peakHour`/`peakHourCount` **over all 24 buckets** with the earliest-bucket tie-break `if (byHour.at(h) > a.peakHourCount)` (`timeanalytics.cpp:25-30`). `hasData` = "any input count > 0" across either array (`:41-44`). The struct carries `hourly[24]`, `weekdayMonFirst[7]`, the two peak index/count pairs, and `hasData` (`timeanalytics.h:10-18`).

### 2.2 ViewModel — `ReportingViewModel` (`qt-app/quick/viewmodels/ReportingViewModel.{h,cpp}`)

- `onTimeAnalyticsReady(byHour, byWeekday)` calls `TimeAnalytics::compute(byHour, byWeekday)`, then `buildHourlyBars(m_timeAnalytics.hourly)` / `buildWeekdayBars(...)`, and sets `m_busiestHourLabel`/`m_busiestDayLabel` **gated on `m_timeAnalytics.hasData`** (`ReportingViewModel.cpp:485-502`).
- `buildHourlyBars(const QList<int> &hourly)` early-returns on `hourly.size() != 24`, then emits **24** bars with the label **thinned to every 3rd hour**: `const QString label = (h % 3 == 0) ? hourTick(h) : QString();` (`ReportingViewModel.cpp:114-128`). The `LBarChart` renders a `Text` under every bar, so the blank labels *are* the thinning — the chart has no thinning logic of its own.
- `buildTimeExport() const` (the 4b-iv-b export carrier assembler) fills `hourLabels[0..23] = hourTick(h)` and `hourCounts[0..23] = m_timeAnalytics.hourly[h]` over the full 24, and sets `busiestHourLabel = formatHourRange(peakHour)` in the `Data` state (`ReportingViewModel.cpp:204-219`).
- The window source **already exists** in the VM: `headerInfo()` reads `AppSettings` keys `library/openHour` (default `7`) and `library/closeHour` (default `21`) into `ReportHeaderInfo::openHour/closeHour` (`ReportingViewModel.cpp:623-624`; struct at `reportdata.h:30-31`). This is the identical source the export line chart consumes via `paintReport(..., info.openHour, info.closeHour)` (`reportrenderer.cpp:710`, `:720`).
- `hourTick(int)` → `"12A".."11P"`; `formatHourRange(int)` → `"2–3 PM"` (en-dash U+2013, `ReportingViewModel.cpp:164`); `weekdayName`/`weekdayShortName` for the day labels — all reused unchanged.

### 2.3 Renderer — `ReportRenderer` (`qt-app/core/reportrenderer.{h,cpp}`)

- `makeHourlyBarChartImage(const ReportTimeExport &t, QSize, const ReportPalette &)` iterates `for (int i = 0; i < t.hourCounts.size(); ++i)`, emitting **one bar and one label per carrier entry** (`reportrenderer.cpp:385-388`). **It already shows every label** — the earlier every-3rd-label thinning was removed during 4b-iv-b because duplicate empty category strings collapse `QBarCategoryAxis`'s plot range and blank the bars (the fix and its rationale are documented at `reportrenderer.cpp:368-376`). The peak caption rides in the chart title via `peakHourCaption(t)` = `"Peak Hour: %1"` (`:70`, `:397`).
- The Excel "When?" block (`writeReportToXlsx`, `reportrenderer.cpp:1000-1029`) writes a `Data`-state hourly table by looping `qMin(t.hourLabels.size(), t.hourCounts.size())` rows into columns 1–2, the weekday table into columns 4–5, then advances the sheet cursor **past the hourly table only**: `row = baseRow + 1 + timeExport.hourCounts.size();` (`:1026`).
- The PDF `Data` path calls `makeHourlyBarChartImage(timeExport, whenSize, palette)` then `makeWeekdayBarChartImage(...)` via `drawFullscreenChart` (`reportrenderer.cpp:734-740`); `whenSize = chartImageSize(usableWidth, false)` — the screen-safe size the QChartView is rendered at and then upscaled (the screen-clamp safeguard, `:735`).

**Key consequence:** because both the renderer's hourly maker and the Excel hourly loop already iterate the **carrier's own size**, narrowing the carrier narrows both automatically — *the renderer needs almost no change* (see §5.4 for the one exception). The 24→window crop is driven from **core** (the peak scan) and the **ViewModel** (the presentation arrays), exactly the layering the rest of the "When?" arc uses.

## 3. Goal & non-goals

**In scope**
- `TimeAnalytics::compute` computes the hourly peak over `[openHour, closeHour]` only.
- The on-screen hourly bars and the export carrier's hour arrays span `[openHour, closeHour]` inclusive, with **every** hour labeled.
- The window is sourced from the same `AppSettings` `library/openHour`/`library/closeHour` the export path already reads, and cached so screen and export use an identical window.

**Non-goals**
- The **weekday** (Mon→Sun) chart — not hour-based; entirely untouched (`buildWeekdayBars`, `makeWeekdayBarChartImage`, `weekdayMonFirst`, `peakWeekdayMonFirst`).
- **KPIs, rankings, roster, totals** — untouched.
- **Backend / endpoint** — no change; the endpoint keeps returning a dense 24-hour array, and the client crops.
- **A new toggle/config for the window** — none; the window is the existing library-hours setting, reused. No checkbox, no new `Q_PROPERTY`.
- **Timezone handling** — none; `login_time` stays server-local, unchanged.
- **`hasData`** — stays "any input count > 0" over the full input, unchanged (see decision 5).

## 4. The five locked decisions

### Decision 1 — Peak-hour caption reflects the busiest hour *within* the window
The `"Busiest: …"` (screen) / `"Peak Hour: …"` (export) caption names the highest bar **inside `[openHour, closeHour]`**, not the overall 24-hour peak. The earliest-bucket tie-break is preserved.
**Rationale:** the caption must always name a bar the user can actually see. Reporting the overall 24h peak while drawing only the windowed bars could point at an hour no bar represents (e.g. a 6 AM spike from a pre-open staff login when the window starts at 7).

### Decision 2 — Every hour in the window is labeled
Drop the on-screen `h % 3 == 0` thinning in `buildHourlyBars`; label **every** bar across `[openHour, closeHour]`. The PDF hourly maker already labels every bar (§2.3), so it needs no change. The Excel hourly table already lists every hour.
**Rationale:** the window is small (~8–14 bars for realistic hours; 15 for the 7–21 default), so every-hour labels fit without crowding, and — unlike the old thinning — the open/close endpoints become meaningful, labeled bars.

### Decision 3 — Inclusive close hour
Mirror the existing line chart's `for (int h = openHour; h <= closeHour; ++h)` (`reportrenderer.cpp:323`). `[7,21]` renders **15** bars, including the 9 PM (`21`) bar.
**Rationale:** boundary parity with the one hourly visualization the app already crops; a librarian who sets "close 21" expects the 9 PM hour drawn.

### Decision 4 — Defensive fallback to the full `0..23` range
If the stored hours are nonsensical — `openHour < 0 || closeHour > 23 || openHour > closeHour` — fall back to the full `0..23` window so the chart never blanks from a hand-edited settings typo.
**Rationale:** insurance only. The admin Settings spinboxes structurally guarantee `openHour ∈ [0,11] < closeHour ∈ [12,23]`, so this state is unreachable through the UI — but a hand-edited `QSettings` store (or a future settings change) must never produce an empty chart. This is the same "second net" posture `compute` already takes on a wrong-length input (`timeanalytics.cpp:11`).

### Decision 5 — All-visits-out-of-hours edge case
When every visit falls outside `[openHour, closeHour]` (rare — a bad clock or an off-hours staff login), the **windowed** `peakHourCount` is `0`, so the **hour caption is suppressed** (empty label, no bogus "12–1 AM"/"7 AM" peak). The **weekday chart still shows the data**, because `hasData` stays an **overall** ("any input count > 0") measure, not a windowed one.
**Rationale:** never assert a peak hour that has no bar; but never hide genuine visit data from the weekday view just because it landed outside library hours. Decoupling "is there a windowed hourly peak?" (`peakHourCount > 0`) from "is there any visit data at all?" (`hasData`) keeps both truthful.

## 5. Detailed design

The layering rule is unchanged from 4b-iv-a/4b-iv-b: **core owns the windowed peak scan; the ViewModel owns hour-label formatting and the presentation arrays; the renderer does zero hour math.**

### 5.1 Core — `TimeAnalytics::compute` gains the window (`timeanalytics.{h,cpp}`)

**Signature (old → new):**

```cpp
// OLD
static TimeAnalytics compute(const QList<int> &byHour,
                             const QList<int> &byWeekdaySunFirst);
// NEW
static TimeAnalytics compute(const QList<int> &byHour,
                             const QList<int> &byWeekdaySunFirst,
                             int openHour, int closeHour);
```

- `hourly` **still returns the RAW 24-wide array** (`a.hourly = byHour;` unchanged). This preserves the index-is-the-hour invariant that every downstream `hourTick(h)`/`hourCounts[h]` relies on — the window is applied by the *consumers* when they iterate, not by mutating the array.
- **Defensive fallback (decision 4), applied before the peak scan:**
  ```cpp
  int lo = openHour, hi = closeHour;
  if (lo < 0 || hi > 23 || lo > hi) { lo = 0; hi = 23; }
  ```
- **Windowed peak scan (decision 1):** scan only `[lo, hi]` inclusive, earliest-tie preserved (strictly-greater keeps the first max):
  ```cpp
  for (int h = lo; h <= hi; ++h)
      if (byHour.at(h) > a.peakHourCount) { a.peakHourCount = byHour.at(h); a.peakHour = h; }
  ```
- **Unchanged:** the wrong-length gate (`:11`), the Sun→Mon weekday reorder, `peakWeekdayMonFirst`/`peakWeekdayCount`, and `hasData` (still "any input count > 0" over the full arrays). When the window contains no positive bucket, `peakHour` stays `0` and `peakHourCount` stays `0` — the sentinel decision 5 keys on.

This matches the in-core precedent that library-hours filtering already lives in core (`aggregateVisitsByCourseHour(data, openHour, closeHour)`).

**Single-source the decision-4 clamp.** The same `if (lo < 0 || hi > 23 || lo > hi) { lo = 0; hi = 23; }` clamp is needed in `compute`, `buildHourlyBars`, and `buildTimeExport`. Three hand-copied copies are a DRY liability — if one drifts, the peak and the bars desync. The plan should factor it into **one** shared helper both layers can call, e.g. a free function in `timeanalytics.h` (already included by the ViewModel) such as `void clampLibraryHours(int &lo, int &hi)` or `std::pair<int,int> clampedLibraryHours(int open, int close)`. This keeps the window arithmetic identical everywhere and pre-empts the dry-checker at PR time.

### 5.2 ViewModel — caching + windowed presentation (`ReportingViewModel.{h,cpp}`)

**Cache the window at analytics-arrival (parity anchor).** Add two members:

```cpp
int m_openHour  = 7;    // cached from headerInfo() when time analytics arrive
int m_closeHour = 21;
```

In `onTimeAnalyticsReady` (`ReportingViewModel.cpp:485`), **before** calling `compute`, read the window once and cache it:

```cpp
const ReportHeaderInfo info = headerInfo();   // AppSettings library/openHour, library/closeHour
m_openHour = info.openHour;
m_closeHour = info.closeHour;
m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday, m_openHour, m_closeHour);
m_hourlyBars.setBars(buildHourlyBars(m_timeAnalytics.hourly, m_openHour, m_closeHour));
```

Both the screen path (here) and the `const buildTimeExport()` (export) read `m_openHour`/`m_closeHour`, so an identical window is guaranteed across screen and export **even if the librarian changes the hours in Settings between Generate and Export** — the window is frozen at the moment the analytics landed, not re-read at export time.

**This caching is a correctness requirement, not merely a parity nicety.** `compute` scans the peak using the *arrival-time* window; the bars that `buildHourlyBars`/`buildTimeExport` emit later **must** use that same window, or the reported peak could name an hour with no drawn bar. The tempting "simplification" — re-reading `headerInfo()` fresh inside the const `buildTimeExport()` — would desync the peak from the bars whenever Settings changed mid-session. **The plan must preserve arrival-time caching; do not replace the cached members with a fresh `headerInfo()` read in the export path.**

**Caption gate (decision 5).** Change the caption gate at `ReportingViewModel.cpp:497-502` from `m_timeAnalytics.hasData` to `m_timeAnalytics.peakHourCount > 0` for the **hour** caption:

```cpp
m_busiestHourLabel = (m_timeAnalytics.peakHourCount > 0)
                       ? formatHourRange(m_timeAnalytics.peakHour) : QString();
```

The **day** caption stays gated on `hasData` (weekday is unaffected). (In the common case `hasData` and `peakHourCount > 0` agree; they diverge only in the all-out-of-hours case.)

**`buildHourlyBars` gains the window (signature old → new):**

```cpp
// OLD
static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly);
// NEW
static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly,
                                             int openHour, int closeHour);
```

Body: keep the `hourly.size() != 24` early-return; apply the **same** decision-4 clamp as core; iterate `[lo, hi]` inclusive, emitting a bar per hour with **every** label present (drop the `h % 3` blanking, decision 2):

```cpp
int lo = openHour, hi = closeHour;
if (lo < 0 || hi > 23 || lo > hi) { lo = 0; hi = 23; }
for (int h = lo; h <= hi; ++h)
    bars.append({ hourTick(h), double(hourly.at(h)) });   // label on EVERY bar
```

**`buildTimeExport()` windows the carrier (`ReportingViewModel.cpp:204-219`).** In the `Data` state, replace the `for (int h = 0; h < 24; ++h)` fill with a `[lo, hi]` fill (same clamp), pushing only in-window hours into `hourLabels`/`hourCounts` (every label present). Gate the export hour caption on the windowed peak, mirroring the screen:

```cpp
te.busiestHourLabel = (m_timeAnalytics.peakHourCount > 0)
                        ? formatHourRange(m_timeAnalytics.peakHour) : QString();
te.busiestDayLabel  = weekdayName(m_timeAnalytics.peakWeekdayMonFirst);   // unchanged
```

`weekdayLabels`/`weekdayCounts` and `busiestDayLabel` are unchanged. The `Error`/`Empty`/defensive-length branches are unchanged (they already emit empty lists). Because `buildTimeExport()` feeds **both** `makeHourlyBarChartImage` (PDF) and the Excel hourly table, narrowing the carrier here narrows both export representations with no renderer restructuring.

### 5.3 On-screen QML — suppress the empty hour caption (`ReportingScreen.qml`)

The `busiestHourCaption` `Text` (`ReportingScreen.qml:456-462`) binds `qsTr("Busiest: %1").arg(vm.busiestHourLabel)`. In the all-out-of-hours case `busiestHourLabel` is empty, which would render a bare `"Busiest: "`. Gate its visibility so the caption disappears when there is no windowed peak:

```qml
visible: screen.vm ? screen.vm.busiestHourLabel.length > 0 : false
```

The weekday caption and the two charts are unchanged; the enclosing data subtree still gates on `!timeLoading && timeError.length === 0 && hasTimeData` (`:439-441`), so the hourly *chart* still draws its (all-zero, windowed) bars while only its caption is suppressed.

### 5.4 Renderer — automatic, with one narrow-window correctness fix (`reportrenderer.cpp`)

- **`makeHourlyBarChartImage`: NO change.** It already draws one bar + one label per carrier entry (§2.3); a shorter carrier yields a shorter, every-hour-labeled chart. The prompt's premise that this maker still carries an `i % 3` thinning to drop is **stale** — that thinning was already removed in 4b-iv-b (`reportrenderer.cpp:368-376`).
- **`makeWeekdayBarChartImage`, `paintReport` Data path, `chartImageSize` safeguard: NO change.** The PDF `Data` path (`:734-740`) passes the windowed carrier straight through; the screen-safe-size→upscale safeguard is untouched.
- **Excel cursor advance: ONE required fix.** The hourly loop (`:1016-1019`) and weekday loop (`:1020-1023`) already iterate their carriers' sizes, so both tables shrink automatically. But the cursor advance assumes the hourly table is always the taller one: `row = baseRow + 1 + timeExport.hourCounts.size();` (`:1026`). With a full 24-hour carrier that was always true (24 > 7). **Windowing breaks that assumption at the low end:** the spinbox permits `[11,12]` (a 2-hour window), for which the hourly table (2 rows) is *shorter* than the weekday table (7 rows), and the system-generated footer written next would overprint the weekday rows. Advance past the **taller** of the two:
  ```cpp
  row = baseRow + 1 + qMax(timeExport.hourCounts.size(), timeExport.weekdayCounts.size());
  ```
  This is the minimal, correct generalization; for the default 7–21 window (15 hourly rows > 7 weekday rows) it is identical to today's behavior.
- **Empty hour caption in the PDF/Excel `Data` state (all-out-of-hours).** With `busiestHourLabel` empty, `peakHourCaption(t)` (`:70`) yields the chart title `"Peak Hour: "` and the Excel peak cell `"Peak Hour: "`. Guard the shared helper so a missing value drops the prefix entirely (a pure string guard — still zero hour math in the renderer):
  ```cpp
  QString peakHourCaption(const ReportTimeExport &t) {
      return t.busiestHourLabel.isEmpty()
                 ? QStringLiteral("Hourly Visits")
                 : QStringLiteral("Peak Hour: %1").arg(t.busiestHourLabel);
  }
  ```
  This keeps the caption/title honest in the rare all-out-of-hours `Data` case without the renderer deriving any hour string. `peakDayCaption` is unchanged.

### 5.5 Legacy WITS.exe — unaffected (argument)

`adminwindow.cpp` passes a default `ReportTimeExport{}` (state `Disabled`) at all three renderer call sites (`adminwindow.cpp:1714`, `:1753`, `:1774`), so `paintReport`/`writeReportToXlsx` omit the "When?" section entirely — no hourly bar chart, no Excel time block. **`TimeAnalytics::compute` is never called from `adminwindow.cpp`** — verified: its only callers are `ReportingViewModel.cpp:487`, `tst_timeanalytics.cpp`, and `tst_reportingviewmodel.cpp:848`. The legacy admin window's own hourly visualization is the per-course line chart via `makeLineChartImage`, which already crops to `info.openHour/closeHour` independently. So the `compute` signature change and the caption/cursor tweaks leave WITS.exe's output byte-for-byte unchanged. The only mechanical concern is that all three `compute` **call sites in the Quick VM + tests** must pass the new window arguments — `adminwindow.cpp` needs no edit.

## 6. Data-flow diagram

```
   AppSettings: library/openHour (7), library/closeHour (21)
        │  (read once, via headerInfo(), when analytics arrive)
        ▼
  ReportingViewModel::onTimeAnalyticsReady(byHour[24], byWeekday[7])
        │   caches m_openHour / m_closeHour  ── parity anchor ──┐
        ▼                                                        │
  TimeAnalytics::compute(byHour, byWeekday, open, close)         │
        │   • hourly[24]  = byHour  (RAW, uncropped)             │
        │   • peakHour / peakHourCount  scanned over [open..close] ONLY (decision 1)
        │   • defensive fallback open<0|close>23|open>close → 0..23 (decision 4)
        │   • weekdayMonFirst / peakWeekday  — UNCHANGED
        │   • hasData — UNCHANGED (overall "any count > 0")
        ▼                                                        │
   m_timeAnalytics : TimeAnalytics                               │
        ├───────────────► buildHourlyBars(hourly, open, close) ──┤  screen
        │                    → [open..close] bars, EVERY label (decision 2/3)
        │                 hour caption gated on peakHourCount > 0 (decision 5)
        │                    → ReportingScreen.qml "When do students visit?"
        │
        └───────────────► buildTimeExport() const  (reads cached m_openHour/close)
                             → ReportTimeExport.hourLabels/hourCounts = [open..close] only
                             → busiestHourLabel empty when peakHourCount == 0
                                  │
                                  ├──► makeHourlyBarChartImage  (PDF: fewer bars, all labels — NO renderer change)
                                  └──► Excel hourly table  (fewer rows; cursor advances past TALLER table — §5.4)

   adminwindow.cpp → ReportTimeExport{} (Disabled) → section omitted; compute never called → WITS.exe unchanged
```

## 7. Edge cases

| Case | Behavior |
|---|---|
| **Defensive fallback (decision 4)** — `openHour < 0 \|\| closeHour > 23 \|\| openHour > closeHour` | Both `compute` and `buildHourlyBars` clamp to `0..23`; the chart renders all 24 bars rather than blanking. Unreachable via the Settings spinboxes; insurance for a hand-edited store. |
| **All visits out of hours (decision 5)** | Windowed `peakHourCount == 0` → hour caption suppressed (screen `Text` hidden; export `busiestHourLabel` empty → title/cell reads "Hourly Visits"). Weekday chart still shows the data (`hasData` stays overall). Hourly bars draw as an all-zero windowed series. |
| **Inclusive close boundary (decision 3)** | `[7,21]` → 15 bars incl. the 9 PM bar; matches `makeLineChartImage`'s `h <= closeHour`. |
| **Window wider than the data** | Hours with no visits draw as zero-height bars inside the window; normal, no special handling. |
| **Narrow window (e.g. `[11,12]`, 2 hours)** | Hourly table (2 rows) shorter than the weekday table (7 rows); the Excel cursor advances past the taller table (§5.4) so the footer never overprints the weekday rows. On-screen and PDF handle it automatically (fewer bars). |
| **Empty range (`Empty` state)** | Unchanged from 4b-iv-b — the section shows the "No visit activity in this range" note, no charts/tables; windowing is irrelevant (no `Data` state reached). |
| **Time-fetch failure (`Error` state)** | Unchanged — "Visit-time data could not be loaded" note; windowing irrelevant. |
| **Librarian changes hours between Generate and Export** | Known, accepted asymmetry: the "When?" hourly chart uses the window **cached at Generate**, but `renderToDevice` reads `headerInfo()` fresh (`reportrenderer.cpp:632-633`), so within one PDF the per-course **line chart** (`makeLineChartImage`, `:710`/`:720`) could use the *live* hours while the When chart uses the cached hours — two charts with slightly different x-ranges. Rare, cosmetic, and the caching tradeoff is still correct (arrival-time caching is what keeps the windowed peak and its bars in agreement). In the common case (no mid-session change) they agree — a net improvement over today's fixed 24h. Documented so it is not a surprise smoke finding. |

## 8. Testing strategy

TDD per the project workflow (red → green → refactor, in subagents). The `compute` signature change is a compile break that forces every call site — production and test — to pass a window, which is the natural red step.

### 8.1 `tst_timeanalytics` (`qt-app/tests/tst_timeanalytics.cpp`)
- **Existing cases updated** to the new signature. Pass the full `0, 23` window to keep the current assertions equivalent (`hoursPeak14()` peak stays 14; the reorder/tie-break/hasData cases are window-agnostic when given `0,23`).
- **New — peak restricted to the window:** a `byHour` with a peak at hour 6 and a smaller in-window bump at hour 10, window `[7,21]` → `peakHour == 10`, `peakHourCount ==` the in-window value (the taller out-of-window bar at 6 is ignored).
- **New — peak ignores a taller out-of-window bar:** an out-of-window hour (e.g. 22) holds the global max; window `[7,21]` → the peak is the largest **in-window** hour, not 22.
- **New — inclusive close boundary counts:** the sole positive bucket sits at `closeHour` (e.g. hour 21 with window `[7,21]`) → `peakHour == 21` (the close hour is included).
- **New — defensive `openHour > closeHour` degrades to full range:** window `[21,7]` (inverted) → peak scanned over `0..23` (equivalent to the unwindowed result); `[-1,23]` and `[0,30]` likewise clamp.
- **New — all-out-of-hours → `peakHourCount == 0`:** every positive bucket is outside `[7,21]` → `peakHour == 0 && peakHourCount == 0`, while `hasData == true` (asserts decision 5's sentinel at the core level).

### 8.2 `tst_reportingviewmodel` (`qt-app/quick/tests/tst_reportingviewmodel.cpp`)
- **`buildTimeExport` windows the carrier:** with a seeded window (default 7/21 via `headerInfo`, or a test-controlled `AppSettings` seed), `te.hourLabels`/`te.hourCounts` contain **only** `[open..close]` entries (count `== close-open+1`), **every** label non-empty, and the counts equal `m_timeAnalytics.hourly[open..close]`.
- **On-screen hourly bars window + every-hour labels:** `hourlyBars` (via `onTimeAnalyticsReady` + `buildHourlyBars`) yields `close-open+1` bars, each with a non-blank `label` (the `h % 3` blanking is gone).
- **Windowed peak caption + parity:** `busiestHourLabel` equals `formatHourRange` of the **windowed** peak, and equals the carrier's `busiestHourLabel` (screen↔export parity). Include a case where the overall 24h peak is out-of-window to prove the caption follows the windowed peak.
- **All-out-of-hours:** seed visits only outside the window → `busiestHourLabel().isEmpty()` and `buildTimeExport().busiestHourLabel.isEmpty()`, while `hasTimeData()` stays true and `weekdayBars` is populated.
- **Existing `denseHours()`/`denseWeek()`/`zeros(n)` helpers** are reused; the `buildTimeExport_dataState_*` case (currently asserting 24 entries, `tst_reportingviewmodel.cpp:835-863`) is retargeted to the windowed count and the `TimeAnalytics::compute(...)` reference call at `:848` gains the window. **The `:848-849` assertion is not a mechanical signature bump:** `QCOMPARE(te.hourCounts, ta.hourly)` compares the *windowed* carrier (e.g. 15 entries) against the *raw 24-wide* `hourly`, so it must be rewritten to compare against the `[open..close]` **slice** of `ta.hourly` (`ta.hourly.mid(open, close - open + 1)`), not the whole array. Call the assertion rewrite out explicitly in the plan.

### 8.3 `tst_reportrenderer` (`qt-app/tests/tst_reportrenderer.cpp`)
- **`sampleTimeExportData()` reflects a windowed carrier:** the fixture (`:116-135`) currently seeds a full 24-entry carrier; retarget it to a windowed carrier (e.g. 15 entries for `[7,21]`, labels `"7A".."9P"`, peak `"2–3 PM"` at the in-window `2P` bar). The hourly/weekday image-maker "non-blank + >500 bar-color pixels" cases (`:523-550`) then exercise the windowed carrier; assert the maker draws `hourCounts.size()` bars.
- **Excel `Data` block reflects the window:** update `writeReportToXlsx_timeBlock_dataStatePresent` (`:453-484`) to expect the windowed hour rows and to assert the footer/`"Prepared by"` line lands **below** the taller of the two tables (guards the §5.4 cursor fix). Add a **narrow-window** carrier (e.g. `[11,12]`, hourly shorter than weekday) and assert the weekday rows are not overprinted by the footer.
- The `Disabled`/`Empty`/`Error` cases (`:486-521`) are unchanged.

### 8.4 Manual smoke — MANDATORY release gate
OFFSCREEN tests cannot see the `QChartView` screen-clamp (project memory "QtChart export screen clamp") — there is no physical screen to clamp against — so a human must confirm the cropped hourly chart renders with **real bars and correct labels** in the live app and a generated PDF. Before the slice is done, run `WITSQuick.exe` and, over a range with data:
1. **On screen:** the "When?" hourly chart shows only `[open..close]` bars, every hour labeled including the open/close endpoints, and the "Busiest: …" caption names an in-window hour.
2. **PDF export:** the hourly bar chart is cropped to the window with readable, non-giant fonts and real bars (not blank), correct peak title.
3. **Excel export:** the hourly table lists only the window hours, and the footer/prepared-by line sits below the whole block (no overprint).
4. Change the library hours in Settings and re-generate to confirm the window tracks the setting; confirm changing hours **after** Generate but **before** Export does not desync screen and export (the cached window holds).
5. Confirm the **legacy WITS.exe** export still shows no "When?" section (unchanged).

Synthetic data only — no real student PII, per the project security-hygiene rule.

## 9. Formatting / ownership boundary

Same rule as 4b-iv-b: **core owns the windowed peak scan; the ViewModel owns hour-label formatting and the presentation arrays; the renderer does zero hour math.**

- **Core** (`TimeAnalytics::compute`) receives `openHour/closeHour` as plain ints and decides *which hour is the peak* within the window — indices and counts only, no strings. It never turns `14` into `"2–3 PM"`.
- **ViewModel** owns every hour string (`hourTick`, `formatHourRange`) and decides *which hours to emit* into the on-screen bars and the export carrier, plus the caption-suppression rule. It is the single place that knows the window came from `AppSettings` and caches it.
- **Renderer** paints exactly the bars/labels/rows the carrier holds and wraps the VM's finished peak **value** in fixed prose (`"Peak Hour: %1"` / the "Hourly Visits" fallback). It derives no hour string and applies no window arithmetic — the carrier is already windowed when it arrives.

## 10. Files-touched map

| File | Change |
|---|---|
| `qt-app/core/timeanalytics.h` | `compute` signature gains `int openHour, int closeHour`; doc comment updated for the windowed peak + fallback. |
| `qt-app/core/timeanalytics.cpp` | Defensive `[lo,hi]` clamp; windowed peak-hour scan; `hourly`/weekday/`hasData` unchanged. |
| `qt-app/quick/viewmodels/ReportingViewModel.h` | `buildHourlyBars` signature gains the window; new `m_openHour`/`m_closeHour` members. |
| `qt-app/quick/viewmodels/ReportingViewModel.cpp` | Cache window in `onTimeAnalyticsReady`; pass it to `compute` + `buildHourlyBars`; window the `buildHourlyBars` loop (drop `h % 3`, every label); window `buildTimeExport`'s hour fill; gate both hour captions on `peakHourCount > 0`. |
| `qt-app/quick/qml/admin/ReportingScreen.qml` | `busiestHourCaption` visibility gated on `busiestHourLabel.length > 0` (decision 5). |
| `qt-app/core/reportrenderer.cpp` | Excel cursor advance past the **taller** of the hourly/weekday tables (`qMax`, §5.4); `peakHourCaption` fallback when `busiestHourLabel` is empty. **No change** to `makeHourlyBarChartImage` (already every-label). |
| `qt-app/tests/tst_timeanalytics.cpp` | Existing cases pass `0,23`; new windowed-peak / out-of-window / inclusive-close / inverted-window / all-out-of-hours cases. |
| `qt-app/quick/tests/tst_reportingviewmodel.cpp` | Windowed `buildTimeExport`/`buildHourlyBars` assertions, windowed-peak caption + parity, all-out-of-hours; update the `compute(...)` reference at `:848`. |
| `qt-app/tests/tst_reportrenderer.cpp` | `sampleTimeExportData()` → windowed carrier; Excel `Data`-block + narrow-window cursor assertions. |
| `qt-app/adminwindow.cpp` | **Unchanged** — still passes `ReportTimeExport{}`; never calls `TimeAnalytics::compute`. Listed only to record it was checked. |

No new source files, no new CMake targets — the change extends existing signatures and bodies only. This slice runs its own `superpowers:writing-plans` → subagent-driven TDD build → `/claude-review` → `create-pr` cycle, per the project workflow rule.
