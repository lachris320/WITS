# LOAMS 2.0 — Phase 4b-iii: Reporting Analytics (Design Spec)

**Date:** 2026-08-18
**Status:** Design approved (owner). PR #41 **MERGED** (`323f466`); implementing on `worktree-phase4b-iii-analytics` off `master`, which carries 4b-ii's export path + the overlap/chart-render fixes.
**Depends on:** 4b-i (Reporting preview, merged PR #40) and 4b-ii (export, merged PR #41). Backend: **no change** in this phase.

---

## 1. Problem & goal

The current Reporting output — on screen and in the PDF/Excel export — is a **raw per-student roster** plus one "visits by course" chart. It exposes data but does not *answer questions*. An administrator cannot glance at it and see how busy the library was, who the most active users are, or which courses/departments drive usage.

**Goal:** make Reporting *answer questions*, not merely surface more data — without turning it into a new dashboard system. This is an **extension of the existing 4b-i/4b-ii seams**, not a rewrite.

**Non-goal / explicit scope guard:** 4b-iii makes Reporting *smarter*, not *bigger*. No new dashboard framework, no new endpoints, no pagination, no time-series/heatmap views. Resist scope creep.

## 2. Organizing principle — the question ladder

The report walks the administrator down a fixed ladder:

> **How much? → Who? → Which? → Details**

| Rung | Answers | Component | Data backing |
|---|---|---|---|
| **How much?** | overall activity | KPI band (4 tiles) | ✅ fetched rows |
| **Who?** | most active users | Top 10 Students | ✅ fetched rows |
| **Which?** | driving courses/departments | Top 10 Courses, Top 10 Departments (+ the ranked bar) | ✅ fetched rows |
| **Details** | the itemized record | Full roster (demoted / opt-in) | ✅ fetched rows |
| ~~When?~~ | time-of-day / day-of-week | **DEFERRED** — see §12 | ⛔ backend change |

The **hierarchy is fixed**; exact grid geometry may be tuned in implementation.

## 3. Architecture — the load-bearing rule

**Analytics aggregators are pure and presentation-agnostic.** They take normalized report rows and return **normalized analytical value types** — never QML-specific structures, never PDF-specific formatting.

```
Raw report rows (QJsonArray, normalized)
        │
        ▼
Pure aggregators  ──►  Analytics value types  (plain C++ structs, witscore)
        │
   ┌────┴─────┐
   ▼          ▼
QML models   ReportRenderer      (both CONSUME the same value types)
(QAbstract*) (PDF / Excel)
```

- Aggregators live in **witscore** (new `qt-app/core/reportanalytics.{h,cpp}`), alongside the existing stateless `ReportController` / `ReportRenderer`. They are `static`, network-free, and unit-tested directly.
- The **ViewModel** (`ReportingViewModel`) calls the aggregator once per result and wraps the output into QML models; the **renderer** reads the same struct. Neither the QML models nor the renderer re-derive analytics.
- This is the single most important protection in the phase: it keeps computation testable in isolation and prevents screen/export divergence (both render from one computed truth).

### Value types (witscore, presentation-agnostic)

```cpp
struct ReportKpis {
    int     totalVisits      = 0;   // Σ visits
    int     uniqueVisitors   = 0;   // count of distinct school_id groups (robust to >1 row/student)
    double  avgVisitsPerVisitor = 0.0; // totalVisits / uniqueVisitors (0 when none)
    QString topDepartment;          // busiest department name ("" when none)
    int     topDepartmentVisits = 0;
    bool    hasData = false;        // false when zero rows -> surfaces render empty-states
};

struct RankingEntry {
    int     rank = 0;               // 1-based
    QString label;                  // student name / course / department
    QString sublabel;               // students: their course; courses/departments: "" (unused)
    int     visits = 0;
    double  percentOfTotal = 0.0;   // share of total visits (courses/departments; students optional)
};

struct ReportAnalytics {
    ReportKpis          kpis;
    QList<RankingEntry> topStudents;      // <= N
    QList<RankingEntry> topCourses;       // <= N
    QList<RankingEntry> topDepartments;   // <= N
};

// The one entry point. topN default 10.
static ReportAnalytics ReportAnalytics::compute(const QJsonArray &normalizedRows, int topN = 10);
```

**Input contract (loud).** `compute` requires **already-normalized** rows — numeric `visits`. 4b-ii's `normalizeExportRows` is the normalizer; `reportVisits()` lives in `quick/models/ReportRowsModel.h`, which **core cannot include**, so `compute` does NOT re-parse — passing a raw-string `visits` is a **caller error**. A unit test pins this contract (a raw-string-`visits` row is treated as 0 / ignored), and callers always pass `m_exportRows` (already normalized in `applyResult`).

**Aggregation is by key, not by row.** Students group on `school_id`, courses on `course`, departments on `department`, each **summing `visits`** across the group. So `uniqueVisitors` = number of distinct `school_id` groups, and a student appearing in more than one row is counted once with their visits summed. This makes every aggregate robust to row multiplicity rather than assuming exactly one row per student.

### Renderer consumes the struct — new export signatures (enforces "no re-derivation")

`paintReport` and `writeReportToXlsx` gain the computed struct + the roster flag as parameters, so the renderer never re-aggregates (§3):

- `paintReport(QPagedPaintDevice*, int resolution, const QJsonArray &rows, const QJsonObject &filters, const ReportPalette&, const ReportHeaderInfo&, const ReportAnalytics &analytics, bool includeRoster)`
- `writeReportToXlsx(QXlsx::Document&, const QJsonArray &rows, const QJsonObject &filters, const ReportHeaderInfo&, const ReportAnalytics &analytics, bool includeRoster)`

`rows` is retained ONLY for the optional detailed roster (drawn iff `includeRoster`); every KPI/ranking value comes from `analytics`. The ViewModel computes `ReportAnalytics` once from `m_exportRows` and hands the same struct to the QML models and to these two functions — the renderer performs zero aggregation. Existing callers (`ReportingViewModel::renderToDevice` and the export paths) are updated to the new arity.

**Slice boundary (see §13):** this renderer signature change and the passing-to-renderer half land in **4b-iii-b**. **4b-iii-a** only computes `ReportAnalytics` and wires the QML models — the renderer keeps its current 4b-ii signature until slice b. (Slice b also adds `reportanalytics.cpp/.h` to the `tst_reportrenderer` CMake `SOURCES`, since that target compiles `reportrenderer.cpp` directly without linking `witscore`.)

## 4. KPIs (the "How much?" band)

Four tiles, in order:

1. **Total Visits** — Σ visits.
2. **Unique Visitors** — distinct students (distinct `school_id` groups; see §3 aggregation-by-key).
3. **Avg. Visits / Visitor** — Total ÷ Unique, 1 decimal (e.g. `3.8`).
4. **Top Department** — the busiest department, rendered with the **department name as the primary value** and visits as supporting text:

   ```
   Top Department
   BSIT
   1,284 visits
   ```

*(The existing 4b-i `deriveTiles` — totalVisits / studentsShown / topCourse — is superseded by `ReportKpis`. `studentsShown` == `uniqueVisitors`.)*

## 5. Rankings (the "Who?" and "Which?" rungs)

Three rankings, each **top 10** by visits:

- **Top 10 Students** — rank, name, course (sublabel), visits.
- **Top 10 Courses** — rank, course, visits, **% of total**.
- **Top 10 Departments** — rank, department, visits, **% of total**.

Rules:
- **N = 10** default. If fewer than 10 entries exist, show exactly what exists (e.g. 6 rows).
- **UI header wording is "Top 10 Students / Courses / Departments"** (literal), even when fewer are listed. No pagination on screen — the full-roster toggle is the escape hatch for deeper inspection.
- **Tie-breaking:** equal visits → **alphabetical by label**, deterministic (stable across runs and across screen/export).
- **Percent** = entry.visits / totalVisits × 100 (guard totalVisits==0 → 0, but that path is an empty-state, see §6).

## 6. Empty / insufficient-data states (spec'd, NOT left to QML)

Every analytical component defines a meaningful state. **Never render empty tables, `0%` everywhere, or blank KPI tiles.**

| Condition | KPI band | A ranking table |
|---|---|---|
| **No report data** (0 rows, `hasData==false`) | Tiles show a single "No report data" state (not four "0"/"—" tiles) | "No data available." |
| **One student only** | Real numbers (Total=that student's visits, Unique=1, Avg=Total/1) | Lists the 1 entry under the "Top 10" header |
| **No department value** (blank `department` in a row) | Blank segment counted under a defined bucket **"(Unspecified)"** — never an empty label | same bucketing in course/dept rankings |
| **No course value** (blank `course`) | n/a | "(Unspecified)" bucket, same rule |
| **Ranking < N** | n/a | Show the entries that exist; header still "Top 10 …" |

- Blank/whitespace `course`/`department`/`name` values normalize to **"(Unspecified)"** inside `compute` so downstream never sees empty labels.
- The **Top Department** KPI may legitimately resolve to **"(Unspecified)"** when the busiest bucket is blank-department rows (real exports contain such rows) — that is an **acceptable display**, not an error state; the tile shows "(Unspecified)" + its visit count like any other.
- The whole analytics section only appears once a report is generated (`hasResult`); a generated-but-empty result is the "No report data" case above.

## 7. On-screen — analytics-first dashboard (layout B)

Extends the merged 4b-i preview in `ReportingScreen.qml`. Hierarchy (fixed; grid geometry tunable):

```
┌──────────────────────────────────────────────┐
│ FILTERS / REPORT CONTEXT                       │  (existing 4b-i)
├──────────────────────────────────────────────┤
│ TOTAL │ UNIQUE │ AVG │ TOP DEPARTMENT          │  How much?
├──────────────────────────────────────────────┤
│ TOP COURSES — visits-by-course chart           │  Which? (glanceable, high on screen)
├──────────────────────┬───────────────────────┤
│ TOP 10 STUDENTS      │ TOP 10 COURSES          │  Who? / Which?
├──────────────────────┼───────────────────────┤
│ TOP 10 DEPARTMENTS   │                         │  Which?
├──────────────────────┴───────────────────────┤
│ [ View full roster ]                           │  Details (collapsed)
└──────────────────────────────────────────────┘
```

- The **ranked bar chart is placed high on screen** (right after KPIs) — it is glanceable there. *(In the export it is demoted; see §8.)*
- **Full roster** is **collapsed/demoted** behind a **"View full roster"** toggle (the existing `ReportRowsModel` table). No pagination.
- A separate **"Include detailed roster in export"** checkbox lives with the export controls (see §9).
- QML consumes: KPIs via **scalar VM `Q_PROPERTY`s** (`totalVisits`/`uniqueVisitors`/`avgVisitsPerVisitor`/`topDepartment`/`topDepartmentVisits` — **NOT a `Q_GADGET`**, to keep the ViewModel the only QML-facing C++ per the MVVM rule), three ranking tables via a new `RankingModel : QAbstractListModel` (one instance per ranking, or a reusable type), and the existing `BarsModel`/`ReportRowsModel`. All fed from the one `ReportAnalytics` struct.
- **Theming:** all visual tokens via `Theme.qml`; zero raw hex; server/name-derived text — **including the new ranking-row delegates (student names, course/department labels)** — uses `Text.PlainText` (cleartext HTTP).

## 8. Export — PDF and Excel

Both surfaces render from the **same `ReportAnalytics`** struct via `ReportRenderer`. The DPI-scaling fix (65ed1ef) is already in the renderer.

### PDF hierarchy (`paintReport`)
```
Report context (header + filters)         (existing)
      ↓
KPI summary                               (new — the 4 KPIs)
      ↓
Rankings: Top 10 Students / Courses / Departments   (new)
      ↓
Visualization: the chart(s)               (existing bar/pie — DEMOTED to supporting, AFTER rankings)
      ↓
Detailed roster                           (existing table — ONLY when the export checkbox is on)
```
The chart is **supporting visualization, not an equal-priority section** — it moves *below* the rankings in the document (unlike the screen, where it sits high).

**This is a structural reorder of `paintReport`, not a light prepend.** Today the paint body runs header→filters→**roster**→charts→prepared-by; the new order moves the roster to the **end** (and makes it optional), inserts net-new KPI-band and three ranking `QPainter` layouts, and the DPI-scaled roster page-break logic (`if (y > usableHeight - vs(200))`, `reportrenderer.cpp`) must be **re-verified** against the new section order. The plan budgets for this as a rewrite of the paint body, not a small insertion.

### Excel (`writeReportToXlsx`) — multi-sheet
- **"Summary"** sheet — KPIs + the three rankings.
- **"Detailed Roster"** sheet — the full per-student list, **only when the export checkbox is on**.
- **"Charts"** sheet — *only if* the existing QXlsx export architecture supports embedding cleanly; otherwise **omit** (do not force chart embedding — that would be scope creep). The current `writeReportToXlsx` is data-only; treat the Charts sheet as optional/stretch, decided during implementation on feasibility, not a requirement.

Splitting Summary / Roster / Charts across worksheets is materially more useful than one dumped sheet.

## 9. Roster toggle distinction (deliberate, independent)

Two different actions that MUST NOT be coupled:

| Surface | Control | Governs |
|---|---|---|
| **Screen** | "View full roster" (expand/collapse) | on-screen disclosure only |
| **Export** | ☐ "Include detailed roster in export" (default **OFF**) | document content only |

The export checkbox **does not follow** the screen toggle. An administrator may inspect the roster on screen yet still export a clean, analytics-only PDF. Default export = analytics summary, **no roster dump**.

## 10. Testing

- **Qt Test (pure)** on `ReportAnalytics::compute`: KPI math (total/unique/avg/top-dept), each ranking (order, top-N cap, % of total), **alphabetical tie-break determinism**, and every **empty/insufficient-data** case in §6 (0 rows → hasData false; 1 student; blank course/dept → "(Unspecified)"; fewer than N).
- **OFFSCREEN export tests** on `paintReport` / `writeReportToXlsx`: the analytics summary renders; the roster is present iff the include-roster flag is set; Excel has the Summary sheet and the Roster sheet only when enabled.
- **QuickTests** on `ReportingScreen`: dashboard grid renders the KPI band + three rankings; "View full roster" toggles disclosure; the export checkbox is independent of the screen toggle; empty-state text shows for a zero-row result.
- Synthetic test data only — **no real student PII** in fixtures.
- **Release gate (manual):** the `QGuiApplication`→`QApplication` constraint and manual `WITSQuick.exe` export smoke from 4b-ii still apply; re-verify a real analytics PDF/Excel by eye.

## 11. Deferred / forward-notes

- **"When?" (time-of-day / day-of-week analytics)** is deferred. `get_report_data.php` GROUPs `login_time` away and returns per-student totals only — there is no time series to rank. Adding peak-hours/peak-day analytics requires a **backend change** (expose a login-time breakdown), which is the owner's manual step and a candidate **future slice (4b-iv)**. Called out explicitly so it is a known next step, not a silent gap.
- **Excel Charts sheet** — optional/stretch (see §8), only if QXlsx embedding is clean.

## 12. Decision freeze

| Area | Decision |
|---|---|
| Architecture | Extend existing 4b-i/ii seams; pure presentation-agnostic aggregators in witscore |
| Computation | Client-side over already-fetched rows; no backend |
| KPIs | 4 tiles: Total Visits, Unique Visitors, Avg. Visits/Visitor, Top Department |
| 4th KPI format | Department **name** primary, visits supporting |
| Rankings | Top 10 Students / Courses / Departments |
| Ranking depth | 10 (show fewer if fewer); no on-screen pagination |
| Tie-breaking | Alphabetical, deterministic |
| Screen | Analytics-first dashboard (layout B); chart high |
| Roster (screen) | Collapsed/demoted behind "View full roster" |
| Roster (export) | Optional checkbox, default **OFF**, independent of screen toggle |
| PDF order | Context → KPI summary → rankings → visualization → optional roster |
| Excel | Summary sheet + optional Detailed Roster sheet (+ optional Charts sheet) |
| Empty states | Explicitly designed (§6), not left to QML |
| Time analytics | Deferred (backend-dependent) |
| Backend | No change in 4b-iii |
| Implementation | Two slices: **4b-iii-a** (core aggregator + on-screen dashboard), **4b-iii-b** (export analytics). PR #41 already merged. |

## 13. Sequencing — two slices (mirrors 4b's i/ii)

This bundles a new core aggregator + tests, a `RankingModel`, VM wiring, a QML dashboard restructure + QuickTests, a PDF paint-body rewrite, and Excel multi-sheet — too much for one reviewable PR. The freeze table mandates the **design**, not one PR, so deliver in two slices:

- **4b-iii-a — Core analytics + on-screen dashboard:** `reportanalytics.{h,cpp}` (`compute` + value types) with unit tests; `RankingModel`; the ViewModel KPI `Q_PROPERTY`s + ranking models fed from `ReportAnalytics`; the `ReportingScreen.qml` dashboard grid (§7) + "View full roster" toggle + QuickTests. **No renderer changes** — ships a smarter screen on its own.
- **4b-iii-b — Export analytics:** the new `paintReport`/`writeReportToXlsx` signatures (§3), the PDF structural reorder (§8: KPI → rankings → demoted chart → optional roster, page-break re-verify), Excel multi-sheet (Summary + optional Roster, §8), the "Include detailed roster in export" checkbox (§9), OFFSCREEN export tests, and the **mandatory `WITSQuick.exe` export smoke** (§10).

Each slice runs its own `superpowers:writing-plans` → subagent-driven TDD build → `/claude-review` → `create-pr`. "When?"/time analytics stays a later slice (**4b-iv**, backend-dependent).
