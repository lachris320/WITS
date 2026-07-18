# LOAMS 2.0 Phase 4 — Admin Part 2 Design Spec (Database + Import, Reporting, Settings; contract-first + 4 tracks)

Date: 2026-07-19
Status: Approved by owner 2026-07-19; ready for implementation-plan (writing-plans skill next)
Supersedes/Extends: parent roadmap 2026-07-07-loams2-qtquick-design.md §9 row 4; predecessor 2026-07-13-loams2-phase3-admin-part1-design.md
Summary: Fill the three disabled admin sidebar items — Database (+ student import), Reporting,
and Settings — as a presentation-layer rebuild over existing, already-unit-tested C++
controllers, not new business logic. This spec is the single architectural contract; after
approval, implementation splits into four independent, separately-reviewable/mergeable tracks
(4a Database+Import, 4b Reporting, 4c Settings, 4d Photo display). Backend edits are small,
targeted, and contract-tested — chiefly applying the already-shipped `requireAdminAuth` helper
to the destructive endpoints and cascading `delete_students.php`.

## 1. Goal & framing

Phase 4 fills the three currently-disabled admin sidebar items — **Database (+ student import)**,
**Reporting**, and **Settings** (`AdminScreen.qml:80-82`, currently `enabled: false` with a
"soon" badge). It is a **presentation-layer rebuild over existing, already-unit-tested C++
controllers** (`StudentController`, `ImportController`, `ReportController` + stateless
`ReportRenderer`, `SettingsController`, `BrandingController`) — **NOT new business logic**. The
backend is essentially already complete: nearly every feature maps to an existing, deployed PHP
endpoint. Phase 4 makes only small, targeted, contract-tested backend edits (detailed in §5).

This is a design spec — the what and why plus the interfaces. The step-by-step task list (TDD
cycles, commit points) is produced separately by the writing-plans skill, **per track**.

## 2. Structure — ONE contract spec, then FOUR independent execution tracks

This spec is the **architectural contract** for "Admin Part 2." After it is approved,
implementation splits into independent tracks, each with its own implementation plan, build
cycle, tests, review gate, and PR — each independently reviewable and mergeable without waiting
for the others:

| Track | Content |
|---|---|
| **4a — Database + Import** | Student table (multi-select), single register (file-picker photo), edit + bulk-update (preview + duplicate check), multi-delete, department deactivate vs delete, Excel/ZIP import. |
| **4b — Reporting** | Cascading filters + duration selector, palette/chart choice, on-screen native-QML preview, PDF/Excel/print export via `ReportRenderer` (off-thread). |
| **4c — Settings** | School name/address/logo (live re-theme), admin name/position, admin-key change, hours, guest-login toggle, reset-visits per department (tier-2 destructive dialog debut). |
| **4d — Photo display** | `LAvatar` on the Kiosk login result and Search results; additive `photo` field on `search_students.php`; coordinated deploy. |

**Sequencing:** **4c → 4a**, with **4b parallel** to either, then **4d last** (depends on 4a's
photo capture and edits shipped screens). Rationale: 4c Settings is the smallest surface and is
where the tiered destructive-op dialog debuts (reset-visits lives there) — prove that pattern on
the cheap track before 4a's multi-delete.

**Why 4d is its own track.** Photo *display* reaches into the already-shipped Kiosk (Phase 2)
and Search (Phase 3) screens and needs a coordinated backend deploy; keeping it isolated keeps
4a/4b/4c deployment-free and independently revertable.

## 3. Architecture & the shared contract (lands FIRST, in the contract-spec PR)

### 3.1 Shell/nav scaffolding pre-landed in the contract-spec PR

To remove the top merge-collision risk (three PRs otherwise all editing nav registration), the
contract-spec PR pre-lands the shell wiring:

- Flip `database` / `reporting` / `settings` to `enabled: true` in
  `qt-app/quick/qml/admin/AdminScreen.qml`.
- Register each in the `Navigator.adminPage` `Loader`.
- Ship **placeholder screens** so navigation works before the tracks fill them in.

### 3.2 MVVM unchanged from Phases 2–3

One ViewModel per screen (`DatabaseViewModel`, `ReportingViewModel`, `SettingsViewModel`), each
injected into its screen as `property var vm` so QuickTests can inject a plain-QML stub.
ViewModels are the **ONLY** QML-facing C++; QML never calls a `witscore` controller directly.
VMs own the existing controllers — **no controller rewrites**. Naming: PascalCase QML types +
C++ VM classes; `m_camelCase` members; the QML module target is `witsquickmodule`.

### 3.3 In-memory admin session

An admin-key holder (on `Navigator` or a small new `AdminSession` type) captured at admin entry,
held in **RAM only** — **NEVER written to QSettings** — attached to destructive POSTs as the
`admin_key` field, and refreshed if the key is changed in Settings during the same session. This
is what makes the new endpoint-level auth (§5) work without any token/session concept (tokens
stay a Phase 6 concern). There must be **NO persisted client copy of the key**.

### 3.4 Shared components — defined HERE, before the tracks diverge

This is the single biggest anti-duplication lever. All of the following are specified centrally
in the contract PR so no two tracks re-invent them:

- **Cascading Department→Course→Year selector** — used by Database filters, Reporting filters,
  AND reset-visits scoping. Define its loading states, "All" semantics, and dependent-clear
  behavior centrally. (Highest duplication risk in the whole phase.)
- **Form primitives** — `LTextField`, `LComboBox`, a date-range picker (Reporting needs it), and
  a shared validation-error presentation.
- **Tiered `LConfirmDialog`** — tier 1 = states exact impact + affected count; tier 2 = impact +
  count + an admin-key entry field. Confirm button disabled while the request is in flight.
- **`LTable` multi-select** — checkbox column, select-all, "N selected" header, selection that
  survives a data refresh. (Only 4a needs it, but it's an `LTable` change, so it's specified
  centrally.)
- **Busy/progress pattern** for long operations (zip import, report render/Excel export).
- **`LAvatar`** (photo-or-initials, wrapping the existing `Initials` implementation shared with
  the kiosk feed) — specified here, consumed in 4d.

### 3.5 Theming

Every new visual token via `Theme.qml` (pragma Singleton); **ZERO raw hex outside `Theme.qml`**;
opacity variants via `Qt.alpha(Theme.<token>, a)`. Same rule enforced since Phase 1.

## 4. The four tracks in detail

### 4.1 Track 4c — Settings (smallest; debuts the destructive-op dialog)

- **School name/address/logo** — logo import triggers live re-theme via
  `BrandingController.regenerateFromLogo` (no restart); persistence on the `branding_config`
  backend + QSettings cache, exactly as shipped.
- **Admin name/position** → `update_admin_info.php`.
- **Admin-key change** → `update_admin_key.php` (requires the OLD key; bcrypt-verified
  server-side).
- **Library open/close hours** (24h, stored in QSettings via `SettingsController`).
- **Guest-login toggle** (existing feature flag).
- **Reset-visits per department** → `reset_visits.php`. This permanently **DELETES** the
  department's `library_visits` history (not just zeroes the count), so it is a **tier-2**
  confirm: names the department + the history-deletion, requires a re-typed admin key that is
  verified **server-side** (see §5), plus an **export-before-destroy CSV**.

**CUT from legacy Settings:**

| Cut item | Reason |
|---|---|
| Font-family/size picker | Superseded by the fixed `Theme` typography — Source Serif 4 + Public Sans. |
| Poster upload | The 2.0 kiosk has no poster display. |
| "Book Drop System" checkbox (dead) | Borrow/Return is out of 2.0 scope. |
| Standalone "clear attendance" checkbox | Folded into reset-visits. |

### 4.2 Track 4a — Database + Import

- **Student table** rendered with the multi-select `LTable`.
- **Register a single student** → `register_student.php`, including a **file-picker photo**
  (browse to JPG/PNG/GIF; matches the endpoint's existing multipart `photo` field). **NO
  live-camera capture** — that would pull in Qt Multimedia and is explicitly out of scope.
- **Edit a student; bulk-update many** → `bulk_update_students.php`, but **ONLY after** a preview
  step ("N students, field X: old→new") and a `check_duplicates.php` pass before commit. Bulk
  edit is the sneakiest destroyer — it looks non-destructive.
- **Multi-delete students** → `delete_students.php`. **Tier-1** confirm naming the count AND
  their visit-record count ("Delete 14 students and their 212 visit records"), preceded by an
  **export-before-destroy CSV** of the affected rows.
- **Department deactivate vs delete** — deactivate (`deactivate_department.php`, sets all its
  students Inactive) versus delete (`delete_department.php`, permanently deletes departments +
  students + their visit history). The UI distinction must be **brutal**: different button
  styling and different dialog tiers; the dialog states whether deactivate is reversible from
  the UI.
- **Excel/ZIP import** → `upload_students_zip.php` + `check_duplicates.php`, via
  `ImportController`. Report partial failures honestly ("42 imported, 3 skipped
  (duplicates): …"); use the busy/progress pattern; disable the commit button while in flight.

### 4.3 Track 4b — Reporting

- **Filters** — the cascading Dept→Course→Year selector + a duration selector
  (day / month / semester / custom) driving `ReportController::computeDateRange(...)`.
- **Palette choice + chart-type choice.**
- **On-screen preview** via `api.php/reports/data` (`ReportController::fetchPreviewData`), drawn
  as **native QML** (not QtCharts).
- **PDF / Excel / print export** via the stateless `ReportRenderer`
  (`get_report_data.php` → `ReportController::fetchReportRows`). NOTE: `get_report_data.php` and
  `api.php/reports/data` are **NOT** duplication — they serve different jobs (full report data
  vs. on-screen preview); both stay. The renderer uses QtCharts (offscreen `QImage`) + QXlsx —
  this is the known, accepted `Qt::Widgets` link on the reports/export path (`QChart` derives
  from `QGraphicsWidget` in Qt 6; parent-spec Risk #2). Long renders/exports **MUST** run off
  the GUI thread (worker + queued signal) so the UI never freezes.

### 4.4 Track 4d — Photo display

- **`LAvatar`** on the Kiosk login result (backend `student_login.php` **ALREADY** returns a
  photo URL) and on the Search results (needs the additive `photo` field on
  `search_students.php` — see §5). **Initials fallback** for the ~171 existing students whose
  `photo` column is NULL.
- Edits the shipped Phase 2 Kiosk and Phase 3 Search screens, hence its own isolated, revertable
  PR + coordinated deploy.

## 5. Backend changes (small, targeted, contract-tests-first, deploy-gated)

### 5.1 Verified facts

- `deliverables/loams_api/auth_helper.php` defines `requireAdminAuth($conn)` — it reads
  `$_POST['admin_key']`, bcrypt-verifies it against `admin.admin_key_hash`, and 401s on
  missing/invalid. It is **REAL**, deployed to xampp, and **ALREADY** used by exactly one
  endpoint, `save_branding.php` (`include "auth_helper.php"; requireAdminAuth($conn);`).
  Applying it to the destructive endpoints is therefore a **2-line change reusing a shipped,
  in-production helper** — distinct from the Phase 6 hardening effort (sessions/tokens/HTTPS/the
  hardcoded `http://localhost` base URL all stay Phase 6).
- Credential endpoints are already sound: `admin_login.php` and `update_admin_key.php`
  bcrypt-verify; key change requires the old key.

### 5.2 Changes to make

| Endpoint | Change |
|---|---|
| `reset_visits.php`, `deactivate_department.php`, `delete_department.php`, `update_admin_info.php`, `bulk_update_students.php` | **Add the `requireAdminAuth` guard.** These already read `$_POST`, so the client just adds an `admin_key` form field. |
| `delete_students.php` (harmonization) | It currently reads a **JSON** request body and opens a hardcoded `new mysqli("localhost","root","",...)` connection. Rewrite it to the house style — `include "db.php"`, read `admin_key` + `school_ids` from a **FORM** post — so `requireAdminAuth($conn)` applies uniformly. |
| `delete_students.php` (cascade fix) | Wrap in a **transaction** and also delete the affected students' `library_visits` rows (by `student_id IN (...)`), mirroring what `delete_department.php` already does (which correctly cascades). Today `delete_students.php` orphans visit rows; this fixes it. (`reset_visits.php` uses an INNER JOIN so it is unaffected.) |
| `search_students.php` (additive `photo`, 4d) | Emit `"photo" => $row['photo']` as a **RELATIVE** path; the client prefixes the `ApiConfig` base (so it survives the Phase 6 base-URL change). Purely additive — existing callers ignore unknown fields; the client's `StudentRecord` + `StudentController::parseSearchResponse` gain a `photo` field. |

### 5.3 Contract convention

`admin_key` **always** travels as a form field. The auth guard is a **BREAKING** contract change
(missing/invalid key now → 401), so each endpoint's captured fixture gains the key and a
negative-auth (401) case. Per parent-spec §8, capture the current request/response fixture
**BEFORE** editing, then harden against it.

### 5.4 Deployment discipline (the Phase 3 lesson)

Every edited endpoint must be copied to `C:/xampp/htdocs/loams_api/` and **byte-verified** —
Phase 3's "Network Error" bug was undeployed endpoints, not code. Because the auth guard is
breaking, deployment must be **LOCKSTEP** with the client change AND the legacy-Widgets touch
below, or the moment an endpoint deploys, any caller not sending `admin_key` 401s.

### 5.5 Legacy rollback stays whole

The legacy Qt Widgets admin window (`qt-app/adminwindow.cpp`) still calls these same destructive
endpoints and is the designated **rollback** until Phase 6 deletes it. So its destructive calls
**also** get `admin_key` attached (captured at its existing admin-login gate). Contained edit to
an app Phase 6 removes anyway; without it, deploying the guard breaks the rollback's
delete/reset/deactivate.

## 6. Testing & gates

Follows the project TDD rule: QtTest + QuickTest under ctest, registered via `wits_add_qttest()`
(`qt-app/cmake/WitsTest.cmake`), with `OFFSCREEN` for any GUI/Quick/painting test. Phase 3's
**32 ctest targets / 128 QuickTest cases** are the regression floor — they must stay green.

### 6.1 Pure C++ unit tests (synthetic payloads, NO live network — house rule)

- New VM logic: `DatabaseViewModel` selection state / bulk-edit-preview assembly / delete-request
  assembly including `admin_key`; `ReportingViewModel` filter→request mapping; `SettingsViewModel`
  dirty-tracking + key-change flow + reset-visits gating; `AdminSession` holds key + refreshes on
  change.
- The **401/auth-failure decode path** on every destructive call.
- The **export-before-destroy CSV serializer**.
- The **additive `photo` parse**.
- Existing controller tests (e.g. `computeDateRange`, upload-result parsing) stay green untouched.

### 6.2 QuickTest (offscreen, plain-QML VM stubs)

- `LConfirmDialog` (tier-1 vs tier-2, confirm disabled in-flight, key field required for tier-2).
- The cascading Dept→Course→Year selector (dependent-clear, "All" semantics, loading states).
- `LTable` multi-select (select-all, N-selected header, selection survives refresh).
- `LAvatar` (URL loads → image; empty/failed → initials fallback).
- Each screen's flows: Settings save + reset-visits re-key dialog; Database
  register-validation / delete-confirm / import-progress; Reporting filter-enable / preview /
  export-trigger; 4d Kiosk + Search photo display.

### 6.3 Backend contract tests

Per-endpoint before/after fixtures carrying the `admin_key` requirement + a negative-auth (401)
case; deployed copies byte-match master.

### 6.4 Long-op safety

Report render / Excel export run off the GUI thread (worker + queued signal), verified with
`QSignalSpy`.

### 6.5 Per-track gates (each PR independently)

- Debug + Release build clean with **no new warnings**.
- Full ctest green.
- `/claude-review` (NOTE: the Codex CLI is not installed on this machine, so the design/code
  review gate uses `/claude-review`, not `/codex-review`).
- A per-track **parity checklist vs the legacy screen**.
- A **human GUI walkthrough** running the actual `WITS` executable (a clean build is necessary
  but not sufficient), including exercising the real destructive paths against a **SYNTHETIC test
  database** (no real student PII, per the project security-hygiene rule): delete a fake student
  and confirm the visit-cascade, reset a department, deactivate a department.

## 7. Out of scope / deferred

- **Live camera photo capture** (Qt Multimedia) — file picker only in Phase 4.
- **Poster upload, font-family/size picker, Book Drop System** — cut.
- **The broader Phase 6 backend hardening:** sessions/tokens, HTTPS, the hardcoded
  `http://localhost` base URL in `apiconfig.h`, and parameterizing/removing debug endpoints — all
  remain Phase 6. Phase 4 only applies the already-shipped `requireAdminAuth` helper to the
  destructive endpoints.
- **Inventory / Borrow-Return / AI Assistant** — out of all of 2.0.

## 8. Risks

| Risk | Mitigation |
|---|---|
| Breaking auth change must deploy lockstep with client + legacy touch. | The deployment-discipline + legacy sections above (§5.4, §5.5). |
| The cascading Dept→Course→Year selector is the top duplication risk if not locked in the contract PR first. | Specified centrally in §3.4 and landed in the contract PR before the tracks diverge. |
| 4d edits two shipped, reviewed screens — regressions there. | Isolated as its own track so a regression is a small revertable PR. |
| Report export on the GUI thread would freeze the UI. | Mandated off-thread (worker + queued signal); verified with `QSignalSpy` (§6.4). |
