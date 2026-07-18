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
**Reporting**, and **Settings** (`AdminScreen.qml:80-82`, currently `enabled: false`; `LSideNav`
renders the "soon" badge for any disabled item, so flipping `enabled: true` both activates the
item and drops its badge). It is a **presentation-layer rebuild over existing, already-unit-tested C++
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

**Deploy topology (corrected — the tracks are NOT uniformly deploy-free).** Because Phase 4
adopts the `requireAdminAuth` guard (§5), the tracks differ in backend coupling, and the spec is
explicit about it:

| Track | Backend deploy | Shipped-screen edits |
|---|---|---|
| **4b Reporting** | **None** — purely client-side (read-only endpoints unchanged). | No. |
| **4c Settings** | **Yes** — guards `reset_visits` + `update_admin_info`; lockstep with the client + the *legacy* `reset_visits` caller (§5.5). | No. |
| **4a Database** | **Yes** — guards `delete_students`/`bulk_update` (harmonized) + the dept ops; lockstep with the client + the legacy callers: the direct dept-op calls **and** the shared-`StudentController` delete/bulk path (§5.5). | No. |
| **4d Photo** | **Yes** — additive `photo` on `search_students`. | **Yes** — edits shipped Kiosk (Phase 2) + Search (Phase 3). |

So each PR is still **independently reviewable and mergeable**, but "independently *deployable* with no backend step" is true only for **4b**. 4a/4c each carry their own breaking endpoint deploy (§5.4); 4d is unique in *also* editing already-shipped screens, which is why it is isolated (a regression there is a small revertable PR).

## 3. Architecture & the shared contract (lands FIRST, in the contract-spec PR)

### 3.1 Shell/nav scaffolding pre-landed in the contract-spec PR

To remove the top merge-collision risk (three PRs otherwise all editing nav registration), the
contract-spec PR pre-lands the shell wiring:

- Flip `database` / `reporting` / `settings` to `enabled: true` in
  `qt-app/quick/qml/admin/AdminScreen.qml`.
- **Extend the `Navigator::AdminPage` enum** — it is currently only
  `{ Dashboard, Search, VisitLogs }` (`Navigator.h:19`); add `Database`, `Reporting`, `Settings`
  and the matching cases to the `pageLoader` switch (`AdminScreen.qml:135-139`). Trivially
  additive but a required C++ change, not just a QML flag flip.
- Register each in the `Navigator.adminPage` `Loader`.
- Ship **placeholder screens** so navigation works before the tracks fill them in.

### 3.2 MVVM unchanged from Phases 2–3

One ViewModel per screen (`DatabaseViewModel`, `ReportingViewModel`, `SettingsViewModel`), each
injected into its screen as `property var vm` so QuickTests can inject a plain-QML stub.
ViewModels are the **ONLY** QML-facing C++; QML never calls a `witscore` controller directly.
VMs own the existing controllers. Naming: PascalCase QML types + C++ VM classes; `m_camelCase`
members; the QML module target is `witsquickmodule`.

**Caveat — this is "mostly presentation," NOT "zero controller changes."** One shared controller
*does* change: adopting `requireAdminAuth` on `delete_students.php` / `bulk_update_students.php`
(§5) forces a **request-format change** to `StudentController::deleteStudents` (`studentcontroller.cpp:216`)
and `bulkUpdateStudents` (`:177`) — today they POST an `application/json` body; they must move to a
FORM post carrying `admin_key`. `StudentController` is **shared** (`DatabaseViewModel` will own it
*and* the legacy `adminwindow.cpp` already uses it — see §5.5), so this one edit touches both
surfaces at once. It also ships **uncovered** unless the 4a plan adds request-side tests:
`tst_studentcontroller.cpp` only exercises *response* parsers (`parseSearchResponse`,
`parseBulkUpdateResponse`, `parseDeleteResponse`), never request building. Everything else in
Phase 4 genuinely is presentation over unchanged controllers; delete/bulk is the one exception,
and the 4a plan must treat it as a controller change with its own tests.

### 3.3 In-memory admin session

**The client does NOT retain the admin key today — this is capture to be BUILT, not wiring to an
existing value.** `KioskViewModel::submitLogin` (`qt-app/quick/viewmodels/KioskViewModel.cpp:107`)
posts `admin_key` to `admin_login.php` and then emits `adminRequested()` with the key dropped;
`BrandingController.h:26` documents "the client does not retain it" (which is why `saveBranding`
re-takes the key as a parameter). So this section adds a new **`AdminSession`** holder:

- **Capture seam:** at the admin-entry gate. Note (implementation forward-note, don't read the
  phrasing too literally): `submitLogin` (`KioskViewModel.cpp:107`) holds the key as its local
  `trimmed`, but the `r.ok && r.isAdmin` decision resolves later in the shared `postForm` reply
  lambda (`:98`), which captures only `this` and no longer has the key in scope. So the plan must
  either stash the key in a member at `submitLogin` time and consume it when `isAdmin` resolves,
  or thread it through `postForm` into the reply handler — hand it to `AdminSession` *before*
  `emit adminRequested()`.
- Held in **RAM only** — **NEVER written to QSettings** (no persisted client copy) — and attached
  to destructive POSTs as the `admin_key` field.
- **Refreshed** when the key is changed in Settings (§4.1) during the same session, so a
  same-session key change doesn't leave a stale held key that 401s subsequent destructive ops.

This is what makes the new endpoint-level auth (§5) work without any token/session concept (tokens
stay a Phase 6 concern). Reset-visits (§4.1) is the one op that does **not** use the held key: its
tier-2 dialog sends a freshly re-typed key — coherent because `requireAdminAuth` only checks
`$_POST['admin_key']`, so a fresh field and the held field are interchangeable server-side.

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

- **School name/address/logo** — logo import triggers live re-theme by reusing the existing
  Quick-wired path `ThemeViewModel::regenerateFromImportedLogo(path)`
  (`qt-app/quick/viewmodels/ThemeViewModel.h:65`), which calls
  `BrandTheme::regenerateFromLogo(...)` (`qt-app/core/brandtheme.h:62`) — NOT a
  `BrandingController` method (no such method exists). No restart; persistence on the
  `branding_config` backend + QSettings cache, exactly as shipped.
- **Admin name/position** → `update_admin_info.php`.
- **Admin-key change** → `update_admin_key.php` (requires the OLD key; bcrypt-verified
  server-side).
- **Library open/close hours** (24h, stored in QSettings via `SettingsController`).
- **Guest-login toggle** (existing feature flag).
- **Reset-visits per department** → `reset_visits.php`. This permanently **DELETES** the
  department's `library_visits` history (not just zeroes the count), so it is a **tier-2**
  confirm: names the department + the history-deletion, requires a re-typed admin key, plus an
  **export-before-destroy CSV**. Note the tier distinction is a **client-side UX gate**, not a
  stronger server check — with the guard applied (§5), *every* destructive op is equally
  `requireAdminAuth`-verified server-side; tier-2 just adds a deliberate re-type step in front of
  the most irreversible one.

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

- **`LAvatar`** on the Kiosk login result and on the Search results, with an **initials
  fallback** for the ~171 existing students whose `photo` column is NULL.
- **The two screens deliver photos in DIFFERENT shapes — `LAvatar` normalizes both:**
  - Kiosk: `student_login.php` already returns an **absolute** `photo_url`
    (`$protocol.$_SERVER['HTTP_HOST'].…`), and on a NULL/missing photo it substitutes the
    **`uploads/default.jpg` sentinel** rather than an empty value — so the initials fallback would
    *never* fire on the kiosk unless `LAvatar` treats that sentinel as "no photo."
  - Search: `search_students.php` gains an additive **relative** `photo` field (empty on NULL);
    the client prefixes the `ApiConfig` base (§5).
  - **Normalization rule (client-side, in `LAvatar`):** show initials when the source is empty,
    absent, OR ends with `default.jpg`; otherwise load the URL, prefixing the `ApiConfig` base if
    it is relative. Additionally, **wire the failed-*load* branch** (`Image.status === Image.Error`
    → initials): unlike Kiosk, `search_students.php` does **no** `file_exists()` check
    (`student_login.php:78` does), so a student whose `photo` column is set but whose file is
    missing emits a live-but-broken URL that only the load-failure fallback catches. This is the
    one component that makes the two divergent server shapes look uniform; it must be unit-tested
    against *both* shapes AND the broken-URL case (§6.2). No change to `student_login.php` is
    required (keeps back-compat).
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

Each guarded endpoint needs `include "auth_helper.php";` (in addition to its existing `db.php`)
plus the `requireAdminAuth($conn);` call.

| Endpoint | Change |
|---|---|
| `reset_visits.php`, `deactivate_department.php`, `delete_department.php`, `update_admin_info.php` (guard only) | **Add the `requireAdminAuth` guard.** These already read `$_POST`, so the client just adds an `admin_key` form field. (`reset_visits.php` currently includes only `db.php` — add the `auth_helper.php` include too.) |
| `delete_students.php` **and** `bulk_update_students.php` (harmonization) | **Both** currently read a **JSON** request body (`json_decode(file_get_contents('php://input'))`) and open a hardcoded `new mysqli("localhost","root","",...)` connection — so a naive `requireAdminAuth($conn)` would 401 every call (the key isn't in `$_POST` and there's no shared `$conn`). Rewrite both to the house style — `include "db.php"` + `include "auth_helper.php"`, read `admin_key` + their payload from a **FORM** post — so `requireAdminAuth($conn)` applies uniformly. **Payload encoding (pin before the plan):** `delete_students` takes a flat `school_ids[]`; `bulk_update` carries an array of 7-field student objects (`bulk_update_students.php:56-81`), which does **not** urlencode cleanly — send it as a **JSON string in a single `students` form field** (`admin_key` stays a sibling form field so `requireAdminAuth` still reads `$_POST['admin_key']`; PHP `json_decode`s the `students` field server-side). The mirrored client change is in the shared `StudentController` (§3.2); the captured contract fixture (§6.3) and the request-assembly tests (§6.1) both encode this exact shape. |
| `delete_students.php` (cascade fix) | Wrap in a **transaction** and also delete the affected students' `library_visits` rows (by `student_id IN (...)`), mirroring what `delete_department.php` already does (which correctly cascades). Today `delete_students.php` orphans visit rows; this fixes it. (`reset_visits.php` uses an INNER JOIN so it is unaffected.) |
| `search_students.php` (additive `photo`, 4d) | Emit `"photo" => $row['photo']` as a **RELATIVE** path (empty on NULL); the client prefixes the `ApiConfig` base (so it survives the Phase 6 base-URL change). Purely additive — existing callers ignore unknown fields; the client's `StudentRecord` + `StudentController::parseSearchResponse` gain a `photo` field. (Kiosk uses the separate absolute `photo_url` from `student_login.php` — see §4.4 for how `LAvatar` unifies the two shapes.) |

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

The legacy Qt Widgets admin window (`qt-app/adminwindow.cpp`) is the designated **rollback** until
Phase 6 deletes it. **Owner decision: keep the rollback whole** — every guarded op the legacy
window can reach must keep working after the deploy. It reaches the guarded endpoints two ways,
and BOTH must be covered:

- **Direct URL calls:** `deactivate_department`, `reset_visits`, `delete_department` (plus the
  non-destructive `update_admin_key`, which already needs the old key and gets no guard).
- **Indirect, via the SHARED `StudentController`:** `delete_students` and `bulk_update_students`
  — the legacy window calls `m_studentController->bulkUpdateStudents(...)` (`adminwindow.cpp:2137`)
  and `deleteStudents(...)` (`:2230`), on the `StudentController` instantiated at `:166`. These were missed by
  an earlier URL-string grep because the endpoint URLs live in `StudentController`, not
  `adminwindow.cpp`. When §3.2's request-format change lands on that shared controller, the legacy
  window's delete/bulk paths change with it and would 401 without a key.

Consequences the per-track plans must honor:

- **The legacy key work fragments across tracks, matching the endpoint each track guards:**
  `reset_visits` → **4c**; `deactivate_department` + `delete_department` **and** the shared-
  controller `delete_students` + `bulk_update_students` → **4a**. Each track's breaking deploy must
  be **lockstep with its own matching legacy edit** — there is no single monolithic legacy touch.
- **Legacy retains no key either** (same finding as §3.3 — its destructive POSTs currently send
  only `department`, and it has no key member). So each legacy edit is "**capture + retain the key
  at the legacy admin-login gate, then thread it in**" — for the direct calls, into the POST; for
  delete/bulk, as the new `admin_key` argument on the shared `StudentController` methods (the same
  argument `DatabaseViewModel` passes). Contained, but real work — an app Phase 6 removes anyway;
  without it, deploying a guard breaks that rollback path.

## 6. Testing & gates

Follows the project TDD rule: QtTest + QuickTest under ctest, registered via `wits_add_qttest()`
(`qt-app/cmake/WitsTest.cmake`), with `OFFSCREEN` for any GUI/Quick/painting test. Phase 3's
**32 ctest targets / 128 QuickTest cases** are the regression floor — they must stay green.

### 6.1 Pure C++ unit tests (synthetic payloads, NO live network — house rule)

- New VM logic: `DatabaseViewModel` selection state / bulk-edit-preview assembly / delete-request
  assembly including `admin_key`; `ReportingViewModel` filter→request mapping; `SettingsViewModel`
  dirty-tracking + key-change flow + reset-visits gating; `AdminSession` holds key + refreshes on
  change.
- **New `StudentController` request-side coverage (the currently-untested edit, §3.2):** the
  harmonized `deleteStudents` / `bulkUpdateStudents` build a FORM body with `admin_key` +
  the pinned `students`-JSON-field encoding (§5.2) — assert the request shape, since
  `tst_studentcontroller.cpp` today covers only response parsers. This is the shared code path
  the legacy window also drives (§5.5), so the test guards both surfaces.
- The **401/auth-failure decode path** on every destructive call.
- The **export-before-destroy CSV serializer**.
- The **additive `photo` parse**.
- Existing controller tests (e.g. `computeDateRange`, upload-result parsing) stay green untouched.

### 6.2 QuickTest (offscreen, plain-QML VM stubs)

- `LConfirmDialog` (tier-1 vs tier-2, confirm disabled in-flight, key field required for tier-2).
- The cascading Dept→Course→Year selector (dependent-clear, "All" semantics, loading states).
- `LTable` multi-select (select-all, N-selected header, selection survives refresh).
- `LAvatar` — tested against **both** server shapes (§4.4): an absolute `photo_url` loads → image;
  the `default.jpg` sentinel → initials; a relative `photo` → prefixed + loaded; empty/failed →
  initials fallback.
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
