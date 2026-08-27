# LOAMS 2.0 — Phase 4b-iv-b: Export the "When?" Time Analytics (Design Spec)

**Date:** 2026-08-26
**Status:** Design approved (owner). Ready for `superpowers:writing-plans`.
**Depends on:** 4b-iii-b (export analytics — KPIs + rankings in PDF/Excel, merged PR #43), 4b-iv-a (on-screen "When?" time analytics, merged). Backend: **no change** — this slice adds no endpoint and issues no request.

---

## 1. Problem & goal

4b-iv-a added the fifth rung of the reporting ladder — **When?** — to the on-screen dashboard: a 24-bar peak-hours chart and a 7-bar busiest-days chart, plus "Busiest: …" captions, all computed once by `TimeAnalytics::compute` and cached on the ViewModel as `m_timeAnalytics`. That slice was deliberately **on-screen only**; it left a visible gap. A librarian who generates a report, sees "Busiest hour: 2–3 PM / Busiest day: Wednesday" on screen, then exports to PDF or Excel, gets a document that silently drops the entire "When?" section. The exported artifact no longer matches what they were just looking at.

**Goal:** render the SAME time analytics already shown on screen into the PDF and Excel exports, so an exported report carries the full question ladder — **How much? → Who? → Which? → When? → Details** — and screen↔export parity holds automatically.

**Load-bearing invariant — one computed truth:** this slice **consumes the ViewModel's already-cached `m_timeAnalytics`** (the `TimeAnalytics` from the single `fetchTimeAnalytics` of the current Generate). There is **NO second backend fetch and NO duplicate aggregation** in the export path. Screen and export read from the same struct, so they cannot diverge — exactly the discipline 4b-iii-b used for `ReportAnalytics` (screen and export share one `compute` result). The export path adds only *formatting and layout*, never recomputation.

## 2. Scope & slicing

| Slice | Contents | This spec |
|---|---|---|
| **4b-iv-a** | On-screen "When?" dashboard section + new backend endpoint | ✅ MERGED (prior slice) |
| **4b-iv-b** | The same time analytics rendered into the PDF and Excel exports | ✅ THIS slice |

| Explicitly out of scope (this slice, and the whole 4b-iv arc) |
|---|
| Hour × day-of-week heatmap |
| Per-course / per-department time breakdowns |
| A new export toggle / checkbox / config for time analytics |
| Timezone handling (`login_time` stays server-local) |
| Any change to the on-screen 4b-iv-a surface |
| Any second backend fetch or re-aggregation in the export path |
| Any change to legacy WITS.exe (`adminwindow.cpp`) behavior |

## 3. Representation & inclusion (the two locked framing decisions)

Two decisions govern everything below and match the 4b-iii-b precedent exactly:

1. **Medium split — PDF is visual, Excel is data.** In the **PDF**, the "When?" analytics render as **two bar-CHART images** (a 24-bar hourly chart with 3-hour-interval x-labels, and a 7-bar weekday chart in Monday→Sunday order). In **Excel**, they render as a **TABLE of cells**. This is the same visual-PDF / tabular-Excel division 4b-iii-b established for rankings.
2. **Always included — no new toggle.** The "When?" block is included in **every** export whenever time data exists; there is **no** new checkbox, filter, or config to enable it. Inclusion is driven purely by the carrier's `state` (below), which the ViewModel computes from data it already holds. (Contrast the pre-existing `includeRoster` bool, which stays exactly as-is and is untouched by this slice.) Consequently the renderer signatures gain **one** `const ReportTimeExport &` parameter and **no new bool**.

## 4. Architecture & data flow

The layering rule is the same one 4b-iii/4b-iv-a enforce: **core is presentation-agnostic; the ViewModel owns all data-derived presentation; the renderer owns fixed export-document prose and layout.** 4b-iv-b threads a presentation-ready *carrier* from the VM to the stateless renderer.

```
                 (already computed in 4b-iv-a — NOT recomputed here)
  m_timeAnalytics : TimeAnalytics     m_timeError : QString
        │  peakHour, peakWeekdayMonFirst,       │  (non-empty iff the time
        │  hourly[24], weekdayMonFirst[7],      │   fetch failed this Generate)
        │  hasData                              │
        └───────────────┬───────────────────────┘
                        ▼
     ReportingViewModel::buildTimeExport()   ← VM owns ALL formatting:
        │   • labels via hourTick / weekdayName (reused from 4b-iv-a)
        │   • peak VALUE strings via formatHourRange / weekdayName
        │   • state = Error | Empty | Data       (Disabled never produced here)
        ▼
     ReportTimeExport  (core carrier struct, reportdata.h)   ← plain data, no logic
        │
        ├──────────────► ReportRenderer::paintReport(...)        → PDF: 2 chart images + captions
        └──────────────► ReportRenderer::writeReportToXlsx(...)  → Excel: side-by-side tables
                              ▲
   adminwindow.cpp (legacy WITS.exe) passes a default ReportTimeExport{} (state = Disabled)
                              → renderer omits the section; WITS.exe behaviorally unchanged
```

- The renderer stays **stateless and pure** (no QSettings, no `ui->`, no member state) — it is a pure function of its arguments, exactly as 4b-iii-b left it.
- The carrier is **plain data with no methods** — it lives in `witscore` (`reportdata.h`) alongside `ReportHeaderInfo`/`ReportPalette`, and carries only VM-formatted strings and counts.
- **No re-aggregation and no re-formatting inside core/renderer:** the renderer never turns `14` into `"2–3 PM"` or `2` into `"Wednesday"`, and never re-reads `login_time`. It receives finished strings and paints/writes them.

## 5. The carrier + state enum (core — `qt-app/core/reportdata.h`)

Both types are added to `reportdata.h` (the same header that already holds `ReportHeaderInfo` and `ReportPalette`), so they compile into `witscore` and are visible to both the ViewModel and the renderer. `reportdata.h` today includes only `<QColor>/<QString>/<QVector>`, so add `<QStringList>` and `<QList>` for the new fields. It includes **no project headers**, so it stays a dependency-free leaf — no include cycle from the carrier living here.

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

**Why 24 full `hourLabels` (not pre-thinned):** the carrier holds a label for **every** hour so the **Excel** "Hour" column can label all 24 rows. The **PDF** hourly chart's "3-hour interval" x-labels are produced by the chart maker showing an axis label only on every 3rd category (a fixed axis-tick-density concern the renderer owns by *position*) — it never re-derives an hour string, it only chooses which of the VM's finished labels to display. This keeps all hour/weekday *string* formatting in one place (the VM) while letting the renderer decide tick spacing, and it is why the carrier is not pre-blanked the way the on-screen `buildHourlyBars` blanks off-interval labels.

**Why short weekday labels + a separate full `busiestDayLabel`:** the 7-bar chart axis and the Excel "Day" column read best with short names ("Mon".."Sun"), matching the on-screen weekday chart; the peak *caption* wants the full name ("Wednesday"). Both are VM-formatted (`weekdayName` / the existing short-name array), so the split is presentation data, not renderer logic.

## 6. Responsibility split — VM data vs. renderer prose

The single most important boundary in this slice. Getting it right is what keeps export wording centralized (future i18n) and formatting un-duplicated (DRY).

| Concern | Owner | Detail |
|---|---|---|
| Hour labels (24), weekday labels (7) | **ViewModel** | `hourTick(h)` for hours (a shipped static). Weekday short names ("Mon".."Sun") are currently a **function-local `static kShort[7]` inside `buildWeekdayBars`** (`ReportingViewModel.cpp:~135`), not a shipped helper — extract it into a small shared VM helper so `buildTimeExport` and `buildWeekdayBars` single-source it (rather than duplicating the array, which would violate this slice's own DRY intent). |
| Hourly/weekday counts | **ViewModel** | Copied straight from `m_timeAnalytics.hourly` / `.weekdayMonFirst`. |
| Peak VALUE strings | **ViewModel** | `busiestHourLabel = formatHourRange(peakHour)`; `busiestDayLabel = weekdayName(peakWeekdayMonFirst)` — reused from 4b-iv-a. |
| `state` (Data / Empty / Error) | **ViewModel** | Computed from `m_timeError` + `m_timeAnalytics.hasData` (§7). |
| Section title "When do students visit?" | **Renderer** | Fixed export-document prose. |
| Column headers "Hour" / "Count" / "Day" | **Renderer** | Fixed table headers. |
| Peak caption templates `"Peak Hour: %1"` / `"Busiest Day: %1"` | **Renderer** | Renderer wraps the VM's peak VALUES with the label prose. |
| State messages | **Renderer** | Error → `"Visit-time data could not be loaded"`; Empty → `"No visit activity in this range"`. |
| Which states render what | **Renderer** | The `switch` in §9. |

**Rule:** the VM never embeds PDF/Excel wording (no "Peak Hour:" prefix, no section title, no state sentences) — it hands over bare VALUES ("2–3 PM", "Wednesday") and a `state`. The renderer never embeds hour/weekday math — it wraps VALUES in its own fixed templates. This centralizes every English string that is *about the export document* in the renderer, ready for a single future i18n pass, while every string that is *derived from the data* stays in the VM's presentation layer.

## 7. ViewModel changes — `ReportingViewModel`

A single new private helper assembles the carrier from state the VM already holds; the two export call sites pass its result to the renderer. No new fetch, no new signal, no new property.

### 7.1 `buildTimeExport()` — the assembler

```cpp
// Assembles the presentation-ready carrier from cached time state. Pure w.r.t.
// member state; performs no fetch. Reuses the 4b-iv-a formatting helpers so the
// exported labels are byte-identical to the on-screen ones (parity).
ReportTimeExport ReportingViewModel::buildTimeExport() const;
```

- **State computation (locked order):**
  1. `m_timeError` non-empty → `state = Error`. (Failure wins — see §9's Error≠Empty rule.)
  2. else `!m_timeAnalytics.hasData` → `state = Empty`.
  3. else → `state = Data`.
  - The VM never produces `Disabled`; that state exists only for the default-constructed carrier the legacy caller passes (§10).
- **In the `Data` state**, populate all list/label fields:
  - `hourLabels[h] = hourTick(h)` for `h` in 0..23; `hourCounts = m_timeAnalytics.hourly`.
  - `weekdayLabels[d]` = the short name for `d` (Mon..Sun); `weekdayCounts = m_timeAnalytics.weekdayMonFirst`.
  - `busiestHourLabel = formatHourRange(m_timeAnalytics.peakHour)`; `busiestDayLabel = weekdayName(m_timeAnalytics.peakWeekdayMonFirst)`.
- **In the `Error` / `Empty` states**, leave the lists and peak labels empty — the renderer draws only a note, so no chart/table data is needed (and empty peak labels prevent a stale "12–1 AM" default from leaking, mirroring the on-screen caution).
- **Defensive length guard:** if `m_timeAnalytics.hourly.size() != 24` or `weekdayMonFirst.size() != 7`, degrade to `Empty` rather than emitting a short carrier — a second net beneath 4b-iv-a's parse-boundary validation, consistent with `buildHourlyBars`/`buildWeekdayBars` which already early-return on wrong length.

### 7.2 Export call sites

Both existing export seams gain one trailing argument — the carrier — and nothing else:

- `renderToDevice(...)` (used by `exportPdf` and `printReport`) — the `ReportRenderer::paintReport(...)` call (current `ReportingViewModel.cpp:580-581`) appends `, buildTimeExport()`.
- `exportExcel(...)` — the `ReportRenderer::writeReportToXlsx(...)` call (current `ReportingViewModel.cpp:658-661`) appends `, buildTimeExport()`.

The carrier is built at export time from the current cached state, so it reflects exactly what is on screen. `printReport` (PDF to a printer) shares `renderToDevice`, so print output gets the section for free.

### 7.3 What does NOT change in the VM

- `canExport` is **still gated on the primary report rows only** (unchanged since 4b-iii). A time-fetch **Error** must never block export — the exported PDF/Excel simply shows the localized "could not be loaded" note. A time-fetch success is not a precondition for export either.
- No new `Q_PROPERTY`, no new signal, no new fetch. The parallel-fetch / three-flag loading model from 4b-iv-a is untouched.

## 8. Renderer — signature changes & the two chart makers

### 8.1 Signatures (add one carrier parameter each, no new bool)

```cpp
static bool paintReport(QPagedPaintDevice *device, int resolution,
                        const QJsonArray &data, const QJsonObject &filters,
                        const ReportPalette &palette,
                        const ReportHeaderInfo &info,
                        const ReportAnalytics &analytics, bool includeRoster,
                        const ReportTimeExport &timeExport);   // NEW, trailing

static bool writeReportToXlsx(QXlsx::Document &xlsx,
                              const QJsonArray &rows,
                              const QJsonObject &filters,
                              const ReportHeaderInfo &info,
                              const ReportAnalytics &analytics, bool includeRoster,
                              const ReportTimeExport &timeExport);  // NEW, trailing
```

### 8.2 Two new chart-image makers (PDF only)

```cpp
static QImage makeHourlyBarChartImage(const ReportTimeExport &t, QSize size,
                                      const ReportPalette &palette);   // 24 bars, 3-hour x-labels
static QImage makeWeekdayBarChartImage(const ReportTimeExport &t, QSize size,
                                       const ReportPalette &palette);  // 7 bars, Mon→Sun
```

- Both are **structurally identical to `makeBarChartImage`**: a single `QBarSet` holding every bucket's count, one `QBarCategoryAxis` category per bucket, fonts sized to `size.height()` (so labels stay legible after upscaling), legend hidden, zero margins, then `return renderChartToImage(chart, size);`.
- **Hourly maker:** 24 categories from `t.hourCounts` / `t.hourLabels`; the x-axis shows a label only on every 3rd category (index `% 3 == 0`) by blanking the others in the category list it hands the axis — this is the "3-hour interval" spacing, done by **position**, consuming (never re-deriving) the VM's labels. Chart title is the peak caption: `QChart::setTitle(QStringLiteral("Peak Hour: %1").arg(t.busiestHourLabel))` — the SAME `"Peak Hour: %1"` template named in §6/§8.4/§14; the fixed prose lives here in the renderer, only the VALUE comes from the VM. The chart's hour x-axis identifies it as the hourly distribution.
- **Weekday maker:** 7 categories from `t.weekdayCounts` / `t.weekdayLabels`, already Mon→Sun. Chart title is the peak caption: `setTitle(QStringLiteral("Busiest Day: %1").arg(t.busiestDayLabel))` (same `"Busiest Day: %1"` template). All 7 labels shown; the weekday x-axis identifies it.
- **Why the caption rides in the title:** `drawFullscreenChart` draws only the image and exposes no seam to place a caption below it (§8.4), so folding the caption into the `QChart` title is the clean, low-risk way to keep the peak caption on the chart's own page without touching the shared helper. Optionally brush the peak bar for a free visual cue — not required for correctness.

### 8.3 Screen-safe-size → upscale safeguard (BINDING — do not deviate)

Both new makers **MUST** obtain their raster size from `chartImageSize(usableWidth, /*square=*/false)` and let `paintReport`'s `drawFullscreenChart` upscale the returned image to fill the page — **exactly** the path `makeBarChartImage`/`makeLineChartImage` already use. They **MUST NOT** render at an arbitrary or print-resolution size.

**Rationale (cite it in code, it is a release-gate hazard):** a `QChartView` is a `QWidget` the window system **clamps to the physical screen**. Rendering it directly at print resolution (~9000 px) lays the chart out at ~screen size in a corner with **giant fonts and a blank remainder** — a corrupt export. `chartImageSize` deliberately returns a modest, screen-fitting size, and the page-fill scaling happens afterward in `drawFullscreenChart` (`img.size().scaled(targetArea, Qt::KeepAspectRatio)`). This clamp is invisible to headless/offscreen tests (there is no real screen to clamp against), which is precisely why the manual export smoke (§11) is a mandatory release gate. This is the same bug recorded in project memory ("QtChart export screen clamp").

### 8.4 PDF placement & pagination

The "When?" analytics form **one logical section titled "When do students visit?"**, placed **after the existing course chart(s) and before the optional Detailed Roster**. The full document order becomes:

> Header → Filters → **KPI Summary** → **Rankings** (Students/Courses/Departments) → **course chart(s)** (existing bar/pie/line per `chartType`) → **When do students visit?** (hourly chart → weekday chart, with peak captions) → *optional* Detailed Roster → Prepared By / footer.

This preserves the reporting hierarchy — **what happened → what ranked → what courses → when it happened → detailed records.**

- **Insertion point:** insert the When? section **into** the chart flow, immediately **before** the existing terminal `drawFooter(currentPage)` (current `reportrenderer.cpp:590`), so that terminal footer foots the last When? page. **Not** *after* l.590 — that would leave the last When? page unfooted, because `drawFullscreenChart` foots the **prior** page at entry (l.536, before `newPage`), never its own page at exit.
- **`drawFullscreenChart` reality (mechanism, corrected):** the helper (`reportrenderer.cpp:530`) foots the **prior** page at entry, opens a new page, redraws the header, and draws **only** the upscaled image — it declares a **local `y` (l.552) that shadows the outer `y`**, so it neither advances the outer `y` nor leaves a seam to draw text below the chart. The two charts are one *section* conceptually but the renderer **must not** force both onto one physical page; each lands on its own page via `drawFullscreenChart`, so **natural page flow decides layout** — identical to how the existing three charts paginate. Consequences per state:
  - **`Data`:** call `drawFullscreenChart` twice — hourly, then weekday. The terminal `drawFooter` (l.590) foots the last one. The peak caption is **not** a separate text draw (there is no y-seam and the outer `y` is unusable) — it is composed by the renderer into the **chart title** inside each maker (§8.2), so it rides in the image. (Alternatively the plan may extend `drawFullscreenChart` to take an optional caption; baking it into the title is the lower-risk default that leaves the existing three charts untouched.)
  - **`Empty` / `Error`:** the section is a single note, which **cannot** be drawn on the last course-chart page (that page holds a chart image and the outer `y` sits near the header). **First foot the current page** — call `drawFooter(currentPage)` — **then** open one new page (`device->newPage()` + `drawHeader(y)`) and draw the section title + the state note in the normal `y`-flow; the terminal `drawFooter` (l.590) foots the note page. The explicit prior-page foot is required because the note path — unlike the `Data` path (whose first `drawFullscreenChart` entry-foots the prior page) and unlike the roster (which sits *after* l.590 and is footed by it) — sits *before* l.590 and has no entry-foot of its own.
  - **`Disabled`:** draw nothing, advance no page.
- **Captions (Data state):** the renderer composes the carrier's peak VALUES into each chart's title — `"Peak Hour: %1"` / `"Busiest Day: %1"` wrapping `t.busiestHourLabel` / `t.busiestDayLabel` (§8.2). The `"%1"` prose stays in the renderer.
- **Roster page-break invariant preserved:** the terminal `drawFooter(currentPage)` still foots the last page (now the last When? page), and the `if (includeRoster)` block still opens its own page unconditionally — unchanged. The roster still starts on a fresh page.

## 9. Error / empty / disabled semantics — the renderer `switch`

Both `paintReport` and `writeReportToXlsx` branch on `timeExport.state` at the When-section insertion point:

| `state` | PDF | Excel |
|---|---|---|
| **Disabled** | Omit the section **entirely** — draw nothing, advance no pages. | Write **no** time block — Summary sheet ends after rankings exactly as today. |
| **Error** | Draw the section title + the note `"Visit-time data could not be loaded"`; no charts. | Write the title + the same note; no tables. |
| **Empty** | Draw the section title + the note `"No visit activity in this range"`; no charts. | Write the title + the same note; no tables. |
| **Data** | Draw the two chart images + peak captions (§8.4). | Write the peak-label row + side-by-side tables (§10). |

**Error ≠ Empty is load-bearing.** A failed time fetch (**Error**) and a genuinely empty range (**Empty**) render **distinct** messages. An exported report must **never** imply "no visits occurred" when the fetch actually failed — that would be a data-integrity lie in a document a librarian may file or forward. The VM's state order in §7.1 (error checked before hasData) guarantees the distinction survives into the export.

**Disabled = legacy parity.** The `Disabled` branch produces byte-for-byte the same document the renderer produced before this slice — no section, no page, no cells. This is what makes WITS.exe unchanged (§10).

## 10. Excel layout — side-by-side tables on the Summary sheet

The time block is written on the **existing "Summary" sheet**, **below the ranking tables** (immediately after the third `writeRanking(...)` call, current `reportrenderer.cpp:806`, and before the system-generated footer line at 808). **"Detailed Roster" remains the ONLY secondary worksheet** — no new sheet is added.

Layout for the `Data` state (compact, comparison-friendly):

1. **Section title row:** `"When do students visit?"` in `sectionFmt` (bold, 12 pt), spanning the block.
2. **Peak-label row:** two cells — `"Peak Hour: <busiestHourLabel>"` and, a couple of columns over, `"Busiest Day: <busiestDayLabel>"`.
3. **Two tables SIDE-BY-SIDE (not stacked):**
   - **"Hourly Visits"** — header cells `"Hour"` | `"Count"` (in `hdrFmt`), then 24 data rows (`hourLabels[h]`, `hourCounts[h]`) in one column group (e.g. columns 1–2).
   - **"Visits by Day"** — header cells `"Day"` | `"Count"` (in `hdrFmt`), then 7 data rows (`weekdayLabels[d]`, `weekdayCounts[d]`) in an adjacent column group (e.g. columns 4–5), starting on the same header row as the hourly table so the two read across.

   The weekday table (7 rows) is shorter than the hourly table (24 rows); it simply ends higher — the hourly table's row cursor governs where the block ends.

   **Cursor mechanics:** the two side-by-side tables cannot both go through the existing single-`row`-incrementing `writeRanking` lambda. Capture a `baseRow` for the shared header line, then write both column groups directly via QXlsx's arbitrary `write(baseRow + offset, col)` addressing (used throughout the writer) — hourly in columns ~1–2, weekday in columns ~4–5 — and finally advance the sheet's `row` cursor past the **taller** (24-row hourly) table so the footer that follows lands below the whole block.

- **Formula-injection guard:** every app-generated **label/caption cell** (the section title, the two peak-label strings, `hourLabels`, `weekdayLabels`, and the table headers) is written through the existing `sanitizeXlsxText(...)` helper — even though these are app-generated and low-risk, running them through the same guard keeps a single, uniform escaping path for all string cells and avoids a reviewer flagging an inconsistency. Count cells are integers and need no sanitizing.
- **Empty / Error states:** write only the section title row + the single note cell (also sanitized); no tables.
- **Disabled state:** write nothing (legacy parity).

## 11. Legacy WITS.exe — `adminwindow.cpp` (unchanged behavior)

WITS.exe (legacy Qt Widgets admin window) calls the shared renderer at **three** sites, updated in 4b-iii-b to pass `ReportAnalytics::compute(data), true`:

- `adminwindow.cpp:1713-1714` — `paintReport(&pdf, 150, data, filters, palette, info, ReportAnalytics::compute(data), true)`
- `adminwindow.cpp:1752-1753` — `paintReport(&printer, …, ReportAnalytics::compute(data), true)`
- `adminwindow.cpp:1773-1774` — `writeReportToXlsx(xlsx, rows, filters, info, ReportAnalytics::compute(rows), true)`

Each of the three gains a **trailing `ReportTimeExport{}`** argument (default-constructed → `state == Disabled`, empty lists). By §9's Disabled branch, the renderer omits the time section entirely, so **WITS.exe's exported PDF/Excel are byte-for-byte unchanged** by this slice. WITS.exe computes no time analytics and gains no "When?" section — that is intentional; the legacy app is frozen and only the LOAMS 2.0 `WITSQuick.exe` grows the feature.

> These three edits are the ONLY change to `adminwindow.cpp` in this slice, and they are mechanical (append one default argument). They exist solely to keep the shared-renderer call sites compiling against the new signature.

## 12. Testing

TDD per the project workflow: red → green → refactor, in subagents. New assertions target the VM assembler, the renderer image-makers, and the Excel writer; the manual export smoke is the release gate the automated tests structurally cannot replace.

### 12.1 ViewModel unit — `tst_reportingviewmodel`

A helper drives `buildTimeExport()` from seeded `m_timeAnalytics` + `m_timeError` and asserts:

- **Data state:** `state == Data`; `hourLabels` has 24 entries and matches `hourTick` for known hours; `hourCounts == m_timeAnalytics.hourly`; `weekdayLabels` is Mon→Sun (7); `weekdayCounts == weekdayMonFirst`; `busiestHourLabel`/`busiestDayLabel` equal the `formatHourRange`/`weekdayName` of the seeded peaks (parity with the on-screen captions).
- **Empty state:** all-zero analytics, no error → `state == Empty`; lists and peak labels empty.
- **Error state:** `m_timeError` non-empty (even if `hasData` were true) → `state == Error`; lists and peak labels empty — **error wins over empty**.
- **Defensive:** a wrong-length `hourly`/`weekdayMonFirst` → degrades to `Empty`, no OOB.
- (The `Disabled` state is not produced by the VM — it is asserted at the renderer level via a default carrier, §12.2.)

### 12.2 Renderer unit — `tst_reportrenderer`

- **Image makers:** `makeHourlyBarChartImage` and `makeWeekdayBarChartImage`, fed a `Data` carrier, return **non-null, non-blank** images at the `chartImageSize(usableWidth, false)` size (assert size and that the image is not uniformly white — same "not blank" check style used for the existing bar/line maker coverage). OFFSCREEN test.
- **Excel — Data state:** after `writeReportToXlsx(..., dataCarrier)`, scan the Summary sheet and assert the presence of the section title, both peak-label strings, the "Hour"/"Count"/"Day" headers, and representative hourly + weekday cells at their expected side-by-side positions.
- **Excel — Disabled state (legacy parity):** with a default `ReportTimeExport{}`, assert the Summary sheet contains **no** time block — the cells after the rankings are exactly the pre-slice content. This is the automated guard that WITS.exe's output is unchanged.
- **Excel — Empty / Error states:** the note cell is present and the two distinct messages differ; no table headers written.

### 12.3 Manual export smoke — MANDATORY release gate

OFFSCREEN tests cannot validate real chart appearance (no physical screen → the clamp bug of §8.3 is invisible). Before the slice is considered done, run `WITSQuick.exe` and, for **three** ranges, export **both** formats and eyeball them:

1. **A range with data** — PDF: the hourly and weekday charts render correctly (readable fonts, real bars, **not** giant-font/blank), with correct peak captions; Excel: the side-by-side block is present and the tables read across.
2. **An empty range** — both exports show the "No visit activity in this range" note, no charts/tables, and the rest of the report is intact.
3. **A simulated time-fetch failure** (e.g. point the time endpoint at a bad URL / stop the backend for the time call only) — both exports show the "Visit-time data could not be loaded" note, distinct from the empty-range note, while the primary report (KPIs, rankings, course chart, roster) still renders fully.

Confirm in the same pass that the **legacy WITS.exe** export is visually unchanged (no "When?" section).

### 12.4 Fixtures

Synthetic data only — **no real student PII**, per the project security-hygiene rule.

## 13. Build infrastructure

- **No new source files.** The carrier + enum go into the existing `qt-app/core/reportdata.h`; the two chart makers and the `switch` logic go into the existing `reportrenderer.{h,cpp}`; `buildTimeExport()` goes into the existing `ReportingViewModel.{h,cpp}`. No CMake target additions for production code.
- **Test wiring:** the new assertions extend the **existing** `tst_reportingviewmodel` and `tst_reportrenderer` targets — no new test target, just new cases. Both already link the sources they exercise; confirm `reportdata.h`'s new types are visible (they are header-only additions to a header both targets already include).
- **Undefined-reference caution (carried from 4b-iii-b):** `tst_reportrenderer` compiles renderer sources directly rather than linking `witscore`; the new chart makers live in the already-compiled `reportrenderer.cpp`, so no new source needs adding to that target — but verify, don't assume, before green.

## 14. Decision freeze

| Area | Decision |
|---|---|
| Data source | Consumes the VM's cached `m_timeAnalytics` (+ `m_timeError`); **no second fetch, no re-aggregation** |
| Screen↔export parity | Automatic — same struct, same 4b-iv-a formatting helpers |
| Carrier | `ReportTimeExport` (plain data) in `core/reportdata.h`; default = `state Disabled`, empty lists |
| State enum | `TimeAnalyticsExportState` — exactly **four** states: Disabled, Data, Empty, Error |
| VM owns | Labels (24 hour, 7 weekday), counts, peak VALUE strings, and `state` |
| Renderer owns | Section title, column headers, `"Peak Hour: %1"`/`"Busiest Day: %1"` templates, the two state messages, the state `switch` |
| Formatting boundary | Renderer does **zero** hour/weekday math; VM does **zero** export-document wording |
| Inclusion | Always included when data exists; **no** new toggle/checkbox/config |
| Renderer signature | One trailing `const ReportTimeExport &` on `paintReport` and `writeReportToXlsx`; **no new bool** |
| PDF representation | Two bar-chart images (hourly 24-bar w/ 3-hour x-labels; weekday 7-bar Mon→Sun) |
| PDF placement | One logical "When do students visit?" section after the course chart(s), before the optional roster |
| PDF pagination | Conceptual grouping only; each chart via `drawFullscreenChart`; natural page flow, **no forced same-page**. Insert **before** the terminal `drawFooter` (l.590) so it foots the last When? page; peak captions ride in each **chart title** (the helper exposes no caption seam); Empty/Error note gets its own page |
| PDF safeguard | New makers use `chartImageSize` screen-safe size → `drawFullscreenChart` upscale — **never** arbitrary/print size (screen-clamp bug) |
| Excel representation | Table of cells |
| Excel placement | Existing "Summary" sheet, below rankings; **side-by-side** Hourly (Hour/Count, 24) + Visits-by-Day (Day/Count, 7) |
| Excel sheets | "Detailed Roster" stays the ONLY secondary worksheet |
| Excel safety | Label/caption/header cells run through existing `sanitizeXlsxText` |
| Error vs Empty | Distinct messages; failed fetch (Error) never rendered as "no visits" (Empty); error checked first |
| Legacy WITS.exe | 3 renderer call sites pass default `ReportTimeExport{}` (Disabled) → output byte-for-byte unchanged |
| Export gating | `canExport` still rows-only; time-fetch Error never blocks export |
| Testing | VM assembler cases (Data/Empty/Error/defensive) + renderer image-maker + Excel Data/Disabled scan |
| Release gate | Manual `WITSQuick.exe` PDF+Excel smoke over data/empty/fetch-failure ranges — headless can't validate chart appearance |
| Out of scope | Heatmap; per-course/per-dept time; new toggle; timezone; on-screen changes; second fetch; WITS.exe changes |

## 15. Non-goals / forward notes

- **Hour × day-of-week heatmap** — not planned; would need a third 24×7 aggregation and a new visualization component.
- **Per-course / per-department time breakdowns** — not planned; would need the time aggregation re-run per breakdown dimension.
- **New export toggle / config** — explicitly rejected; the block is always included when data exists, gated only by `state`.
- **Timezone handling** — none; `login_time` stays server-local everywhere, unchanged from the app.
- **On-screen surface** — 4b-iv-a is frozen; this slice touches only the export path.
- **Second backend fetch / re-aggregation** — never; the export consumes the one cached `TimeAnalytics`.
- **Legacy WITS.exe** — receives only the three mechanical default-argument edits; gains no "When?" section.

This slice closes the **4b-iv** arc (When? on screen *and* in exports) and, with it, the full Phase 4b Reporting question ladder end-to-end. It runs its own `superpowers:writing-plans` → subagent-driven TDD build → `/claude-review` → `create-pr` cycle, per the project workflow rule.
