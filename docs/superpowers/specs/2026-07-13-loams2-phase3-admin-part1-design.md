# LOAMS 2.0 Phase 3 — Admin Part 1 Design Spec (Admin shell + Dashboard + Search + Visit Logs)

Date: 2026-07-13
Status: Approved by owner 2026-07-13; ready for implementation-plan (writing-plans skill next)
Summary: Replace the Phase-2 admin placeholder with a real admin surface — a maroon sidebar
shell, a page header with a live clock, and three functional screens (Dashboard, Search,
Visit Logs) — driven by per-screen ViewModels over `witscore`, and backed by two new
read-only PHP endpoints (plus a small extension to one existing endpoint) that the current
backend does not yet provide. This is Phase 3 of the roadmap in the parent spec
(`2026-07-07-loams2-qtquick-design.md` §9, row 3).

## 1. Goal

Turn the admin surface from a stub into the first working slice of the LOAMS 2.0 admin
experience. Today `qt-app/quick/qml/admin/AdminScreen.qml:9` is a single centered label
("Admin (coming in Phase 3)"); admin entry already works —
`AppShell.qml`'s `Loader` swaps Kiosk↔Admin on `Navigator.currentSurface`, and the kiosk's
admin-key login reaches it via `KioskScreen.qml:12` (`onAdminRequested: Navigator.showAdmin()`).
Phase 3 fills that surface with a shell and three screens, matching the reference design
`../Library System UI Modernization/Admin Dashboard.dc.html` (a sibling folder outside the
repo; normative for tokens/layout/motion, described here as "the reference").

This is a design spec — the what and why plus the interfaces. The step-by-step task list
(TDD cycles, commit points) is produced separately by the writing-plans skill.

## 2. Context — where Phase 3 sits

| Phase | State | Relevance to Phase 3 |
|---|---|---|
| 1 (witscore extraction, AppShell + Theme, L* skeletons) | MERGED to master | Supplies the shell host, the `Theme` singleton, and the L* component skeletons this phase makes functional. |
| 2 (Kiosk parity) | MERGED to master | Supplies `Navigator` (`qt-app/quick/viewmodels/Navigator.h`), `RecentLoginsModel` (`qt-app/quick/models/RecentLoginsModel.h`), `HttpForm` (`qt-app/quick/HttpForm.h`), the `wits_add_qttest()` helper, and the `quick/` conventions in CLAUDE.md. |
| **3 (this spec)** | **Approved; ready to plan** | Admin shell + Dashboard + Search + Visit Logs. |
| 4 | Deferred | Database + import, Reporting/exports, Settings (see §11). |

The parent spec's §7 Admin paragraph already names the target: "persistent left sidebar
(Dashboard, Database, Reporting, Search, Visit Logs, Settings), page header with clock;
dashboard card grid … tables in the design's row style; segmented controls." Phase 3
delivers Dashboard, Search, and Visit Logs from that list; Database, Reporting, and Settings
are shown as disabled "soon" sidebar items and built in Phase 4.

## 3. Reason for being — the reference design outruns the backend

**This is the spec's central finding and the reason it exists.** The reference Admin
Dashboard shows richer data than the PHP backend under `deliverables/loams_api/` can produce
today. The evidence:

| Reference expects | Backend reality today | Source |
|---|---|---|
| Dashboard: hourly histogram, peak hour, visitors-today / this-week / registered-students totals | `get_report_data.php` **requires a department** and returns only per-student aggregate visit counts (`COUNT(v.id) AS visits`) over a date range — no hourly buckets, no peak hour, no today/week totals. | `deliverables/loams_api/get_report_data.php:26-29` (department required), `:33-45` + `:85-88` (per-student aggregate only) |
| Visit Logs: student attendance rows | `get_visitors.php` reads the **GUEST** `visitors` table (`name`, `company`, `purpose`, `time_in`) — a different table from student attendance. | `deliverables/loams_api/get_visitors.php:37-40` |
| Student attendance with time-in and time-out | Student attendance lives in `library_visits`, written **login-only**: `student_login.php` and `rfid_login.php` `INSERT … login_time = NOW()` — no logout / time-out column is ever written. | `deliverables/loams_api/student_login.php:34-55`, `deliverables/loams_api/rfid_login.php:32-54` |
| Search: per-student visit total | `students` has a lifetime `visits` counter (incremented per login), but there is **no per-month count**, and `search_students.php` does not even return `visits`. | `student_login.php:59` / `rfid_login.php:57` (lifetime increment); `search_students.php:74-84` (returned columns omit `visits`) |

`library_visits` columns present (from the insert): `student_id`, `course`, `login_time`,
`yearlog`, `year_level`, `department`, `name`, `gender`, `status`, `photo`
(`student_login.php:34-37`). `api.php` is only a thin router, and it routes just a **subset**
of the backend — `get_report_data.php` (via `reports/data`) plus register / departments /
courses / years / admin (`deliverables/loams_api/api.php:31-119`); it does **not** route
`get_visitors.php`, `search_students.php`, `student_login.php`, or `rfid_login.php` (those are
called directly by filename), and it adds none of the missing aggregate queries. The two new
endpoints are likewise called by filename via `ApiConfig::endpoint(...)`, not added to the
router switch.

### Owner decision on the gap

**Option 1 — pull backend work forward, but only the read-only query endpoints strictly
required for the Phase 3 UI.** Backend hardening/auth stays in Phase 6 (parent spec §9). The
owner has explicitly approved deploying the two new read-only endpoints to the library server.
Everything added in §5 is additive and read-only; no existing endpoint's contract changes
except the additive `visits` field on `search_students.php` (whose client-side plumbing is
detailed in §5.3).

## 4. Scope — deliverables

Replace the `AdminScreen.qml` placeholder with a real admin surface:

- A maroon **sidebar shell** (`LSideNav`) + a **page header with a live clock** (`LPageHeader`).
- Three functional screens: **Dashboard**, **Search**, **Visit Logs**.
- The sidebar also lists **Database / Reporting / Settings** as **disabled "soon"** items
  (those screens are Phase 4).
- A **back-to-kiosk** affordance that calls `Navigator.showKiosk()`. No such affordance
  exists today — Phase 2 wired only the Kiosk→Admin direction (`KioskScreen.qml:12`);
  `Navigator::showKiosk()` already exists (`Navigator.h:24`) but is currently unreachable
  from the admin UI.

## 5. Backend — two new read-only endpoints + one existing-endpoint extension

Contract-fixtures-first (parent spec §8): each new endpoint gets a captured request/response
**contract fixture** that feeds a pure C++ parser unit test; **no live network in tests**
(house rule).

The JSON shapes below are the **proposed** contract; the exact field names are **frozen when
the fixtures are captured** during implementation. The two new endpoints are called directly by
filename via `ApiConfig::endpoint("…​.php")` (as all existing endpoints are) — they are **not**
added to `api.php`'s router switch.

**SQL-safety mandate (both new endpoints).** `get_report_data.php` is the model to follow:
build every query with **prepared statements + `bind_param`**, never string interpolation. This
is called out because the reused `get_visitors.php` interpolates its date parameters unescaped
(`get_visitors.php:26-33`; only `search` is escaped) — the new files must **not** copy that
pattern. See §5.4 for how the reused endpoint's pre-existing exposure is bounded in Phase 3.

**Date-window semantics (frozen with the fixtures).** To avoid baking an unintended window:
- **"today"** = the server-local calendar date of `login_time`, i.e. `DATE(login_time) = CURDATE()`.
  The deployment is a single on-prem library server, so its local time is authoritative (no
  client-side timezone reconciliation).
- **"week"** = the current **Monday–Sunday calendar week** — Monday 00:00 through Sunday 23:59
  of the week containing "today" (ISO week). Chosen over the reference's Mon–Fri card subtitle:
  a full calendar week is more universally correct, and — critically — it **keeps the endpoint
  contract stable** when the future admin-configurable operational-week setting (below) lands.
  Narrowing/widening operational days then becomes a *filter over a fixed weekly window* rather
  than a change to what "week" means at the endpoint, so no fixture re-capture is forced. The
  reference's "Mon – Fri" subtitle is **superseded** by this decision; the This-Week card and
  the Visit Logs range label should show the Mon–Sun range.

Both definitions apply identically to `dashboard_summary.php` (`today`/`week`) and to
`get_library_visits.php` (`range=today|week`).

**Deferred (post-Phase-3) — admin-configurable operational days.** Making the operational week
configurable by the admin (e.g. institutions with Saturday operations) is a planned future
enhancement, **not Phase 3**. Phase 3 ships the fixed Mon–Sun calendar week deliberately so the
setting can be added later without re-capturing fixtures or changing endpoint semantics — it
lands naturally alongside the Settings screen (Phase 4+). See §11.

### 5.1 `dashboard_summary.php` (NEW, read-only)

Feeds the Dashboard stat cards and charts. Sourced from `library_visits` (attendance) +
`students` (registered count). Response shape:

```json
{
  "status": "success",
  "today": 128,
  "week": 812,
  "students": 3450,
  "hourly": [ { "hour": 8, "count": 12 }, { "hour": 9, "count": 34 } ],
  "departments": [ { "name": "CE", "count": 210 }, { "name": "IT", "count": 180 } ]
}
```

Peak hour is **derived client-side** from `hourly` (the endpoint does not compute it), keeping
the endpoint a straight aggregation and the "which hour is the peak" presentation choice in
the ViewModel. `today` and `week` are counts of `library_visits` rows over the respective
date ranges (§5 date-window semantics); `students` is the row count of the `students` table.

**Cost & refresh.** The endpoint runs the today/week counts plus the hourly and department
`GROUP BY`s on `library_visits` per call — acceptable at single-library scale; an index on
`login_time` covers the date-bounded scans if needed. The Dashboard **fetches once on
navigation to it** (and offers a manual refresh affordance); there is no live polling in
Phase 3, so a dashboard viewed for a long time is a point-in-time snapshot by design.

### 5.2 `get_library_visits.php` (NEW, read-only)

Feeds the Visit Logs **student** view — student attendance rows from `library_visits`:

```json
{
  "status": "success",
  "count": 2,
  "visits": [
    { "date": "2026-07-13", "name": "Maria Santos", "course": "BSCE",
      "department": "CE", "time_in": "08:14" }
  ]
}
```

Parameter `range=today|week` (or explicit `start`/`end`). A time-out field is **omitted by
design** because the system is login-only — `library_visits` has no logout column
(`student_login.php:34-37`).

### 5.3 `search_students.php` — surface the lifetime `visits` count (a **multi-layer** change)

The Search "Total Visits: N" stat is **not** a one-line PHP edit — it threads through four
layers, and all four must change together or the number silently never appears:

1. **PHP** — the query already `SELECT *`s every column (`search_students.php:22`) but the
   response map omits `visits` (`search_students.php:74-84`). Add
   `"visits" => isset($row['visits']) ? intval($row['visits']) : 0` to that map.
2. **Struct** — `StudentRecord` (`qt-app/core/studentdata.h:11-21`) has **no `visits` field**;
   add one (e.g. `int visits = 0;`). This is the struct `parseSearchResponse` fills, so without
   it the PHP field is dropped on the floor.
3. **Parser** — extend `StudentController::parseSearchResponse`
   (`qt-app/core/studentcontroller.h:21-24`) to read `visits` into the new field; the captured
   fixture uses the extended shape and the parser test asserts the value round-trips.
4. **VM / screen** — `SearchViewModel` exposes it as `totalVisits`; the result card renders the
   explicit **"Total Visits: N"** label (§6.2).

Because `StudentRecord` is shared with the bulk-update path (`studentdata.h:8-10`), the new
field is read-only for Search and simply ignored on the serialize-back direction — no contract
change to bulk update.

### 5.4 `get_visitors.php` — reused unchanged (with a bounded, known exposure)

The existing GUEST-log endpoint (`get_visitors.php:37-40`) is reused verbatim as the
**secondary** GUEST toggle inside Visit Logs. It **interpolates its date parameters unescaped**
(`get_visitors.php:26-33`) — a pre-existing SQL-injection exposure this phase does not create.
Phase 3 bounds it by driving the guest toggle only with **app-generated ISO date strings** from
the Today/This-Week control (never free-text user input on the date path), and the `search`
field is left unused by the toggle. Full parameterization of this endpoint is deferred to the
Phase 6 backend hardening pass (parent spec §9), recorded here as a forward-note rather than a
silent reuse. The two **new** endpoints do not inherit this pattern (§5 SQL-safety mandate).

## 6. Screen behavior

### 6.1 Dashboard

Net-new (no legacy equivalent — see §12). Composition:

- **Stat cards** via `LStatTile` (reused as-is): visitors today, visitors this week,
  registered students, peak hour. `peakHourLabel` is derived in the ViewModel from the
  `hourly` array (§5.1).
- **Hourly bar chart** — **vertical** columns. `LBarChart` **already renders this shape**
  (bottom-aligned rects whose height ∝ value, `LBarChart.qml:17-34`); the hourly chart largely
  **reuses** it and only needs **per-bar hour labels** added. Custom QML, **not QtCharts**
  (parent spec Risk 2).
- **Department bars** — **horizontal** (a label + a width-proportional track). This is the
  variant `LBarChart` does **not** yet have (its `orientation` property is declared but unused,
  `LBarChart.qml:9`); building it is the real chart work here — see §8.
- Motion: `barGrow` / `pageIn` / `rowIn` via `Theme.motion.*` (parent spec §7).

### 6.2 Search

- Reuse `StudentController::searchStudents(search, department, course)`
  (`qt-app/core/studentcontroller.h:33-35`, async → `searchFinished` / `searchFailed`,
  `:50-54`). No new *endpoint* for the query itself, but the "Total Visits" stat requires the
  **four-layer `visits` plumbing** of §5.3 (PHP field → `StudentRecord.visits` →
  `parseSearchResponse` → `SearchViewModel.totalVisits`), not a bare PHP tweak.
- Show the lifetime visit count **relabeled explicitly as "Total Visits: N"** (owner
  decision) — never a bare number — to avoid this-month-vs-lifetime ambiguity, since the
  backend has no per-month count (§3).
- Search by name / ID / course; **course filter chips**.

### 6.3 Visit Logs

- **Student attendance is the PRIMARY / DEFAULT view** (owner decision), fed by
  `get_library_visits.php` (§5.2). The **GUEST** log (`get_visitors.php`) is a **SECONDARY
  toggle** — not an equally-prominent tab.
- **The two feeds have different columns**, so the `LTable` **column set switches on `mode`**
  (not just the row data):
  - **Student (default):** Date · Name · Course · Department · Time In · Time Out (always "—").
  - **Guest:** Name · Company · Purpose · Time In. (No course/department — the guest
    `visitors` table has none; `get_visitors.php` returns only `name`/`company`/`purpose`/
    `time_in`, so a date column, if shown, is derived from the `time_in` datetime.)
  `VisitLogsViewModel` exposes the active-mode rows through one `QAbstractListModel` whose roles
  cover the **union** of both column sets (unused roles are empty per mode); the
  `VisitLogsScreen` selects the column list from `mode`. See §7.3.
- **Today / This-Week** segmented control via `LSegmented` (reused as-is). This is the
  Today/This-Week visitor-log toggle the parent spec §7 says "lands natively here."
- The **time-out column shows "—" always** (login-only; §3, §5.2), and applies only to the
  student view (the guest column set has no Time Out).

## 7. Architecture — MVVM, extend `Navigator`

MVVM holds throughout: **ViewModels are the only QML-facing C++; QML never calls a `witscore`
controller directly** (CLAUDE.md → "qt-app/quick/ conventions"). Each screen receives its VM
as `property var vm` so QuickTests inject a plain-QML stub — the established convention
(CLAUDE.md; mirrors `KioskScreen.qml`'s `BrandPanel { vm: kioskVm }` at `KioskScreen.qml:39`).

### 7.1 Navigation — extend the existing `Navigator` singleton

Chosen approach: **extend `Navigator`** (`qt-app/quick/viewmodels/Navigator.h`) rather than
add a second navigation object. The parent spec (§4) wants navigation and keyboard shortcuts
routed through one testable place, and this mirrors the existing top-level Kiosk/Admin
pattern (`Navigator.h:16` `enum Surface { Kiosk, Admin }`).

Additions to `Navigator`:

| Member | Kind | Purpose |
|---|---|---|
| `enum AdminPage { Dashboard, Search, VisitLogs }` | `Q_ENUM` | Which admin sub-page is active. |
| `adminPage` | `Q_PROPERTY(AdminPage … NOTIFY adminPageChanged)` | Bound by the shell's `Loader`. Default `Dashboard`. |
| `showAdminPage(AdminPage)` | `Q_INVOKABLE` slot | Switch sub-page; idempotent (mirrors the existing `setSurface` guard, `Navigator.h:31`). |

`showKiosk()` / `showAdmin()` stay as-is (`Navigator.h:24-25`).

### 7.2 Shell and screens (QML)

- `admin/AdminScreen.qml` becomes a **shell**: `LSideNav` + `LPageHeader` + a `Loader` keyed
  on `Navigator.adminPage` (replacing the current placeholder label, `AdminScreen.qml:7-13`).
- New screens, each with `property var vm`:
  - `admin/DashboardScreen.qml`
  - `admin/SearchScreen.qml`
  - `admin/VisitLogsScreen.qml`

### 7.3 ViewModels (one per screen, `quick/viewmodels/`)

| ViewModel | Exposes | Row model(s) |
|---|---|---|
| `DashboardViewModel` | `statToday`, `statWeek`, `statStudents`, `peakHourLabel` + **state** | `hourlyModel`, `departmentModel` |
| `VisitLogsViewModel` | `mode` (Student\|Guest), `range` (Today\|Week), count / range label + **state** | row model |
| `SearchViewModel` | results incl. `totalVisits` + **state** | results model |

- **Fetch state is a first-class part of every data VM.** Each of the three fetches over the
  network, so each exposes a **status surface** — a `loading` bool and an `errorText` (or a
  single `status` enum: `Idle` / `Loading` / `Ready` / `Error`), following the Phase 2 kiosk
  pattern where `HttpForm::submit(...)` splits `onSuccess` / `onNetworkError`
  (`qt-app/quick/HttpForm.h`). The screens render accordingly:
  - **Loading** — a busy indicator (cards/table dimmed); no stale data flashed.
  - **Empty** — `LTable.emptyStateText` for the tables; a "no data yet" state for the Dashboard.
  - **Error** — an inline error message with a **retry** action; a failed fetch never silently
    renders as an empty result. Error strings must not leak backend internals (security-hygiene).
- **Pure JSON parsers** (one per endpoint) live in `core/` or as VM statics so they unit-test
  **without widgets and without network** — the same pattern Phase 2 used for `LoginParser`.
- **Row lists are `QAbstractListModel`s** following the `RecentLoginsModel` pattern
  (`qt-app/quick/models/RecentLoginsModel.h:10` — roles enum, `roleNames()`, a typed `Row`
  struct).
- Endpoints are built **only** via `ApiConfig::endpoint(...)` over the shipped base
  (`qt-app/core/apiconfig.h:27`; base `http://localhost/loams_api/` unchanged — revisiting it
  is Phase 6, parent spec §9). Where a POST is needed, reuse the shared `HttpForm` transport
  helper (`qt-app/quick/HttpForm.h:38` `submit(...)`).

## 8. Component build-outs (Phase 1 skeletons → functional)

These L* components shipped as Phase 1 skeletons; Phase 3 makes them functional. New tokens
the screens need may be added to `Theme.qml`, but **ZERO raw hex outside `Theme.qml`**
(CLAUDE.md).

| Component | Current state | Phase 3 build-out |
|---|---|---|
| `LSideNav.qml` | Text-only `Repeater`, no interaction (`qt-app/quick/qml/components/LSideNav.qml:21-30`) | Active state, gold dot, click handling, disabled "soon" items, footer slot. |
| `LTable.qml` | Header rect only; "full column/model wiring lands with Database/Visit Logs screens" (`qt-app/quick/qml/components/LTable.qml:5-6,21-27`) | Columns + model rows + hover + empty state. |
| `LBarChart.qml` | Already renders **vertical** columns (bottom-aligned rects, height ∝ value, `LBarChart.qml:17-34`); the `orientation` property is declared but **unused** (`:9`). | Add the **horizontal** variant (department bars: label + width-proportional track) and **per-bar labels** for the hourly (vertical) chart. The vertical shape is reused, not rebuilt. |
| `LPageHeader.qml` | Shows subtitle **OR** clock, not both (`qt-app/quick/qml/components/LPageHeader.qml:29-35`) | Extend to show subtitle under the title **and** a right-aligned clock together. |
| `LSegmented`, `LStatTile`, `LCard` | — | Reused as-is. |

## 9. Accessibility & keyboard (Phase 3 slice)

Phase 3 adds **sidebar arrow-cycling**, **table focus**, and **`Accessible.*` roles** on the
new and extended components (the skeletons already carry base roles, e.g.
`LSideNav.qml:33` `Accessible.role: Accessible.List`, `LTable.qml:29` `Accessible.Table`).
The global **Ctrl+K quick-search palette** and the full `Accessible.*` sweep are Phase 5
(parent spec §7 keyboard/a11y, §9 row 5).

## 10. Design system compliance

- Every visual token comes from `Theme.qml`; a few dashboard/table tokens may be added there.
  No literal `#RRGGBB` in a screen; opacity variants use `Qt.alpha(Theme.<token>, a)`
  (CLAUDE.md).
- Screens compose the L* components and never restyle primitives locally (parent spec §5).

## 11. Deferred / explicit non-goals

| Deferred item | Goes to |
|---|---|
| Database CRUD + import, Reporting/exports, Settings | Phase 4 (parent spec §9 row 4) |
| Admin-configurable operational days / week window (Phase 3 hardcodes Mon–Sun; §5) | Phase 4+ (with Settings) |
| Global Ctrl+K quick-search palette; full `Accessible.*` sweep | Phase 5 |
| Backend auth/hardening; the hardcoded `http://localhost` base (`apiconfig.h:18-21`) | Phase 6 |
| Time-out / logout tracking | Out of scope — not in the schema (`library_visits` is login-only, §3) |

## 12. Verification model & gates

- **Keep the existing ctest suite green** — currently **23 targets** (registered across
  `qt-app/tests/CMakeLists.txt` + `qt-app/quick/CMakeLists.txt`). Every task keeps it green;
  each new test raises the count.
- **New test targets:** `tst_dashboardviewmodel`, `tst_visitlogsviewmodel`,
  `tst_searchviewmodel`, one parser test per new endpoint (fed captured contract fixtures,
  no live network), and OFFSCREEN QuickTests for the three screens plus the built-out
  components. Register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`), adding
  `OFFSCREEN` for any GUI/Quick/painting test.
- **Per-phase gates** (parent spec §8): Debug + Release build clean, full ctest,
  `/claude-review`, the create-pr review trio (dry-checker / security-reviewer /
  general-code-reviewer, diff-scoped), and a **human visual walkthrough** of the admin
  surface against the reference `Admin Dashboard.dc.html`.

### Parity note (net-new, not a legacy port)

The legacy Widgets admin (`qt-app/adminwindow.cpp`) has **no dashboard page** — its
`visitorPage`/`visitorTable` is fed by `VisitorController::visitorsLoaded`
(`adminwindow.cpp:149`, `:309`, `:2540`), i.e. the **guest** table only. Therefore:

- The **Dashboard** is net-new; its "parity checklist" is a **design-fidelity check against
  the reference**, not a legacy-behavior port.
- The Visit Logs **student** view is likewise net-new (legacy `visitorPage` showed only the
  guest table). The **guest** toggle is the one part with a legacy analog.
