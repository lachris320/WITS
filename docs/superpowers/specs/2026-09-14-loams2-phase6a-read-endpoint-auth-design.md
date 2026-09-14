# LOAMS 2.0 — Phase 6a: Admin Read-Endpoint Authentication (Design Spec)

**Date:** 2026-09-14
**Status:** Design — approved for planning
**Roadmap:** Phase 6 (Backend Hardening). This is the **first slice** of Phase 6; the phase is decomposed into independent, separately-deployable slices (see §8).

## 1. Goal

Require a valid `admin_key` on every **admin-only read endpoint** in the PHP backend, closing the unauthenticated student-PII enumeration hole (`search_students.php` returns the full roster including photo paths) and its five sibling admin reads. The public kiosk and reference endpoints stay open.

### In scope
- Server-side auth guard on the six admin read endpoints (§3.2).
- A **payload-aware** `requireAdminAuth()` that reads the key from a form field *or* a JSON body (§3.1).
- Client-side threading of `admin_key` into the four admin read paths that don't send it yet (§3.3).
- Client handling of a `401` on those reads (§3.4).

### Out of scope (explicitly — later Phase 6 slices)
- **Info-disclosure / config hygiene:** `display_errors`, raw `$conn->error`/`$stmt->error` echoed to clients, CORS `*`, the dead `config.php`. The guarded files keep their current error behavior in this slice.
- **Upload hardening** (`upload_students_zip.php` MIME/size/ZIP-filter/permissions).
- **Transport & session model** (HTTPS, replacing the single shared `admin_key` with sessions/tokens).
- **Write endpoints** — already guarded in repo source (`requireAdminAuth`) and the client already sends the key; nothing to do here.
- Guarding public/reference endpoints — see §3.5 (must NOT be touched).

## 2. Key decisions (locked)

| Decision | Choice | Why |
|---|---|---|
| Read scope | **All six** admin reads | No unauthenticated admin read left behind — one coherent "the admin API requires the key" surface; marginal per-endpoint cost is small once the helper + client pattern exist. |
| Auth mechanism | Reuse the existing `admin_key` + `requireAdminAuth()` (`password_verify` vs `admin_key_hash`) | Same shared-secret model the writes already use; replacing it with sessions/tokens is a separate Phase 6 slice. |
| Key source | Payload-aware: `$_POST['admin_key']` OR the JSON body | Four of the six reads send a JSON body via `php://input`; the helper must handle both without breaking endpoints that re-read the body. |
| Client key origin | `AdminSession` (RAM-only, never logged) | Same source the guarded write paths already use; these reads only fire from admin surfaces, so the key is always present. |
| Testing | Client-side ctest TDD + manual server curl checklist | Matches how the write guards + the Phase 4a auth spine were verified; avoids a disproportionate new PHP toolchain for one-line guard calls. |
| Deploy | Repo source + client ship together; owner deploys the PHP | Deploy is owner-gated (per `SECURITY_UPDATES.md`); ordering caveat in §6. |

## 3. Architecture

### 3.1 `auth_helper.php` — make `requireAdminAuth()` payload-aware

Today `requireAdminAuth($conn)` reads only `$_POST['admin_key']`. The four JSON-body reads (`search_students`, `get_visitors`, `get_report_data`, `get_report_time_data`) send `admin_key` inside a JSON object read via `php://input`, so `$_POST` is empty for them.

Change: extract the key from a unified source, in order:
1. `$_POST['admin_key']` (form-urlencoded / multipart writes and the two form reads).
2. Otherwise, the `admin_key` field of the decoded JSON body.

The raw `php://input` is read **once** and cached (module-level static) so an endpoint that also calls `file_get_contents('php://input')` to parse its own JSON still sees the body. The `password_verify` check against `admin_key_hash` and the 401/500 responses are unchanged. Because every guarded **write** already calls `requireAdminAuth($conn)` and sends the key in `$_POST`, this change is backward-compatible for them (branch 1 still matches).

### 3.2 The six guarded reads

Add a single `requireAdminAuth($conn)` call near the top of each file, **before** any query is prepared or the payload is trusted, so a 401 exits before any DB work:

| Endpoint | Leaks | Payload style |
|---|---|---|
| `search_students.php` | full roster incl. **photo paths (facial PII)** | JSON |
| `get_library_visits.php` | visit logs (names / school IDs) | form |
| `get_visitors.php` | guest visitor logs (names) | JSON |
| `get_report_data.php` | report rows (names / IDs) | JSON |
| `get_report_time_data.php` | hourly aggregates (low PII) | JSON |
| `dashboard_summary.php` | counts (low PII) | form |

No other change to these files in this slice (error-echoing / `display_errors` are the info-disclosure slice).

### 3.3 Client — thread `admin_key` into the four admin read paths

Mirror the established write pattern (`AdminSession` holds the key, RAM-only; guard fields are never logged). For JSON-body endpoints the key goes **into the JSON object**; for form endpoints it's a `$_POST` field.

- `StudentController::searchStudents` (`SearchViewModel`) → `search_students.php` (JSON).
- `VisitLogsViewModel` / `VisitorController` → `get_library_visits.php` (form) + `get_visitors.php` (JSON).
- `DashboardViewModel` → `dashboard_summary.php` (form).
- `ReportController` → `get_report_data.php` + `get_report_time_data.php` (JSON).

Each path needs the admin key available; inject `AdminSession` (or accept the key as a parameter) the same way the write controllers already do. These reads only fire from admin surfaces (post-login), so the key is always populated.

### 3.4 Client — handle a 401

A guarded read that returns HTTP 401 (or a `{"status":"error"}` auth body) must surface an **error state**, not parse the body as an empty/garbage result set (which would render a misleading empty list). Each affected view-model exposes the failure the same way it exposes other request errors.

### 3.5 Endpoints that MUST stay public (do NOT guard)

Login and reference-data endpoints — guarding these would break the kiosk and admin login:
- `student_login.php`, `guest_login.php`, `rfid_login.php`, `admin_login.php`
- `get_departments.php`, `get_courses.php`, `get_courses_by_department.php`, `get_years.php` (reference data; used by kiosk + admin filters; no PII)
- `get_branding.php` (public branding)

## 4. Data flow

1. Admin logs in → `AdminSession` holds the `admin_key` (RAM-only).
2. An admin screen (Search / Visit Logs / Dashboard / Reporting) fires a read → the VM/controller attaches `admin_key` (JSON field or form field).
3. Server: `requireAdminAuth($conn)` extracts the key (form → else JSON), `password_verify` against `admin_key_hash`. Invalid/absent → `401` and exit before any query. Valid → the read proceeds unchanged.
4. Client: 200 → parse rows as today; 401 → error state.
5. Kiosk / reference reads are unguarded and behave exactly as before.

## 5. Testing

**Client (Qt Test / ctest — TDD, red→green):**
- Each affected controller/VM **attaches `admin_key`** (from the injected session/param) to its request, in the correct place (JSON field vs form field). Assert on the captured request body (the existing `tst_studentcontroller` / `tst_importcontroller` tests already assert `admin_key` presence for writes — mirror that).
- A **401** response drives the VM into an error state, not a success-with-empty-rows.
- No key available → the read reports an error rather than firing an unauthenticated request (defensive).

**Server (manual, documented checklist — no PHP harness):**
- For each of the six endpoints: request **with** a valid key → `200` + data; **without** a key (and with a wrong key) → `401` + `{"status":"error"}`, no rows in the body.
- Confirm a guarded **write** still works (regression on the payload-aware helper's form branch).

All client tests via `wits_add_qttest()` (+ `OFFSCREEN` where a test needs it).

## 6. Risks & deploy

- **Deploy ordering.** A *gated server + old client* would `401` the admin reads (client sends no key). So the client build and the PHP deploy land **together** — the owner controls both. The reverse (new client → still-ungated server) is harmless: the extra `admin_key` field is ignored by an old endpoint.
- **`php://input` double-read.** Mitigated by caching the raw body in the helper (§3.1); the manual checklist's JSON-endpoint cases prove the endpoints still parse their own body.
- **No kiosk impact.** No kiosk/reference endpoint is touched (§3.5); the public wall display is unaffected.
- **`SECURITY_UPDATES.md` is stale** on the write endpoints (it predates their guarding). This slice does not attempt to correct that document; the info-disclosure slice can revisit it.

## 7. Deliverables

- `deliverables/loams_api/auth_helper.php`: payload-aware key extraction.
- `deliverables/loams_api/{search_students,get_library_visits,get_visitors,get_report_data,get_report_time_data,dashboard_summary}.php`: `requireAdminAuth()` guard.
- Client: `StudentController`/`SearchViewModel`, `VisitLogsViewModel`/`VisitorController`, `DashboardViewModel`, `ReportController` — thread `admin_key` + 401 handling.
- Client tests: key-attached + 401-handling cases for each path.
- A manual server-verification checklist (curl with/without key → 200/401) in the PR body or a docs note.

## 8. Phase 6 slice map (for context)

1. **6a — Admin read-endpoint auth** (this spec).
2. **6b — Upload hardening** (`upload_students_zip.php`).
3. **6c — Info-disclosure & config hygiene** (`display_errors`, raw error echo, CORS, dead `config.php`).
4. **6d — Transport & sessions** (HTTPS; replace shared `admin_key` with sessions/tokens) — likely its own multi-slice sub-project.
5. Retire the legacy Widgets rollback app once 2.0 is trusted.
