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
| Key source | Payload-aware: `$_POST['admin_key']` OR the JSON body — **never the query string** | The reads split three ways (JSON body, `$_GET` filters, no input); the key rides in the POST body or JSON so it's covered uniformly, and a secret must never go in a URL (server/proxy logs) per the security-hygiene rule. |
| Client key origin | `AdminSession` (RAM-only, never logged) | Same source the guarded write paths already use; these reads only fire from admin surfaces, so the key is always present. |
| Testing | Client-side ctest TDD + manual server curl checklist | Matches how the write guards + the Phase 4a auth spine were verified; avoids a disproportionate new PHP toolchain for one-line guard calls. |
| Deploy | Repo source + client ship together; owner deploys the PHP | Deploy is owner-gated (per `SECURITY_UPDATES.md`); ordering caveat in §6. |

## 3. Architecture

### 3.1 `auth_helper.php` — make `requireAdminAuth()` payload-aware

Today `requireAdminAuth($conn)` reads only `$_POST['admin_key']`. The four JSON-body reads (`search_students`, `get_visitors`, `get_report_data`, `get_report_time_data`) send `admin_key` inside a JSON object read via `php://input`, so `$_POST` is empty for them.

Change: extract the key from a unified source, in order:
1. `$_POST['admin_key']` (form-urlencoded / multipart writes, and the two `$_GET` reads once their client requests are switched to POST — see §3.3).
2. Otherwise, the `admin_key` field of the decoded JSON body (`json_decode(file_get_contents('php://input'))`).

**The helper never reads `$_GET`** — a secret in the query string would leak into server access logs and proxies (security-hygiene rule). The `password_verify` check against `admin_key_hash` and the 401/500 responses are unchanged. Because every guarded **write** already calls `requireAdminAuth($conn)` and sends the key in `$_POST`, branch 1 still matches them — backward-compatible.

Note on `php://input`: no caching gymnastics are needed. `php://input` is re-readable for `application/json` (and any non-`multipart` body), so a JSON endpoint that already does its own `file_get_contents('php://input')` is unaffected by the helper also reading it; and for `multipart`/urlencoded writes `php://input` is empty but branch 1 (`$_POST`) resolves the key first. The helper simply tries `$_POST`, then a JSON decode of the body.

### 3.2 The six guarded reads

Add a single `requireAdminAuth($conn)` call near the top of each file, **before** any query is prepared or the payload is trusted, so a 401 exits before any DB work:

| Endpoint | Leaks | Payload style |
|---|---|---|
| `search_students.php` | full roster incl. **photo paths (facial PII)** | JSON body |
| `get_library_visits.php` | visit logs (names / school IDs) | **`$_GET` filters** (`range`/`start`/`end`) |
| `get_visitors.php` | guest visitor logs (names) | JSON body |
| `get_report_data.php` | report rows (names / IDs) | JSON body |
| `get_report_time_data.php` | hourly aggregates (low PII) | JSON body |
| `dashboard_summary.php` | counts (low PII) | **no input** |

No other change to these files' bodies in this slice (error-echoing / `display_errors` are the info-disclosure slice). Two consequences of the payload styles above:

- **The two non-JSON reads need a client method switch (§3.3), not a server input change.** `get_library_visits.php` reads its filters from `$_GET`; `dashboard_summary.php` reads nothing. Neither has a `$_POST` body today. Rather than change how they read filters, the client switches these two requests from GET to POST and sends only `admin_key` in the urlencoded body — **keeping `range`/`start`/`end` in the URL query string** so the server's existing `$_GET` reads still resolve. `$_GET` is populated from the query string regardless of method, so this is a client-only change; the endpoint body is untouched apart from the guard call.
- **`api.php` routes `reports/data` → `require_once 'get_report_data.php'` (`api.php:99`).** Guarding `get_report_data.php` therefore also guards the `api.php` router path — one guard covers both entry points. (This is what breaks the legacy app's report preview — see §6.)

### 3.3 Client — thread `admin_key` into the admin read paths

Mirror the established write pattern exactly: the **view-model** calls the process-wide singleton `AdminSession::instance().key()` and passes the key **as a method parameter** to the controller (as `DatabaseViewModel`/`ImportViewModel` already do for the writes) — controllers are not made `AdminSession`-aware. Guard fields are never logged. Per endpoint:

- `StudentController::searchStudents` (`SearchViewModel`) → `search_students.php`: add `admin_key` **into the JSON body**.
- `ReportController::fetchReportRows` / `fetchTimeAnalytics` (`ReportingViewModel`) → `get_report_data.php` / `get_report_time_data.php`: `admin_key` **into the JSON body**.
- `VisitLogsViewModel::refresh()` **guest branch** (the inline `get_visitors.php` POST at `VisitLogsViewModel.cpp:123`, which builds its own JSON payload — **not** `VisitorController::fetchVisitors`, which is legacy-only, called only from `adminwindow.cpp` and left to break per §6) → `admin_key` **into the JSON body**.
- `VisitLogsViewModel::refresh()` **student branch** → `get_library_visits.php`: **switch the request GET→POST**, keep `range`/`start`/`end` in the URL query string, send `admin_key` in the urlencoded body (§3.2).
- `DashboardViewModel` → `dashboard_summary.php`: **switch the request GET→POST**, send `admin_key` in the urlencoded body (no filters to preserve).

`AdminSession.h` is already included by the write paths but **not** by `DashboardViewModel`, `VisitLogsViewModel`, or the reporting path today — that include is new wiring in this slice. These reads only fire from admin surfaces (post-login), so `AdminSession::instance().hasKey()` is always true in practice; the VM still guards defensively (§5) rather than firing an unauthenticated request.

### 3.4 Client — handle a 401

Mostly already handled — a 401 sets `reply->error() != NoError`, and the Quick read paths already route that to their existing failure signals (`searchFailed`, `reportError` / `timeAnalyticsError`, and the Dashboard/VisitLogs inline `netErr → "Network error. Please try again."`, e.g. `VisitLogsViewModel.cpp:130`). So a 401 does **not** fall through to success-with-empty-rows on the LOAMS 2.0 client today. The work here is therefore only: (a) a per-path test asserting a 401 lands in the error state (§5), and (b) optionally distinguishing an auth failure from a generic network error in the message. The one genuinely-silent path (`ReportController::fetchPreviewData`, `qDebug`-only) belongs to the **legacy** Widgets app and is addressed under §6, not fixed here.

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
- Each affected controller/VM **attaches `admin_key`** (from `AdminSession::instance().key()`, passed as a controller param) to its request, in the correct place: a JSON body field for the four JSON reads, or the urlencoded body for the two GET→POST reads — with `range`/`start`/`end` still asserted present in the query string for `get_library_visits`. Assert on the captured request (the existing `tst_studentcontroller` / `tst_importcontroller` tests already assert `admin_key` presence for writes — mirror that).
- A **401** response drives the VM into an error state, not a success-with-empty-rows.
- No key available → the read reports an error rather than firing an unauthenticated request (defensive).

**Server (manual, documented checklist — no PHP harness):**
- For each of the six endpoints: request **with** a valid key → `200` + data; **without** a key (and with a wrong key) → `401` + `{"status":"error"}`, no rows in the body.
- Confirm a guarded **write** still works (regression on the payload-aware helper's form branch).

All client tests via `wits_add_qttest()` (+ `OFFSCREEN` where a test needs it).

## 6. Risks & deploy

- **Deploy ordering (LOAMS 2.0 client).** A *gated server + old Quick client* would `401` the admin reads (client sends no key). So the updated `WITSQuick` build and the PHP deploy land **together** — the owner controls both. The reverse (new client → still-ungated server) is harmless: the extra `admin_key` field is ignored by an old endpoint.
- **Legacy `WITS.exe` (Widgets rollback) admin reads WILL break — accepted.** The legacy Widgets app sends no `admin_key` on any read, so once the guarded PHP deploys, every legacy admin read returns `401`: Dashboard, Visit Logs, Search, and Reports (including the `api.php` `reports/data` route, whose `fetchPreviewData` fails **silently** — an empty preview, `qDebug` only). **Decision for 6a: accept this.** `WITSQuick` (LOAMS 2.0) is the production admin client; the legacy app is a rollback slated for retirement (§8.5), and threading keys through the code being retired isn't worth it. The rollback's **kiosk still works** — student/guest/RFID login and reference endpoints are unguarded (§3.5) — so the public wall-display fallback is unaffected; only the legacy *admin* screens lose reads. **Owner-approved (2026-09-14):** the narrowed rollback is accepted — 6a threads keys into the LOAMS 2.0 (`WITSQuick`) client only; the legacy Widgets admin reads are left to break, to be resolved by legacy retirement (§8.5).
- **`php://input` is not a problem.** It is re-readable for JSON bodies, so the helper reading it does not break the JSON endpoints' own `file_get_contents('php://input')`; no caching is required (§3.1). The manual checklist's JSON-endpoint cases confirm each still parses its body.
- **No kiosk impact.** No kiosk/reference endpoint is touched (§3.5); the public wall display is unaffected on either client.
- **`SECURITY_UPDATES.md` is stale** on the write endpoints (it predates their guarding). This slice does not attempt to correct that document; the info-disclosure slice can revisit it.

## 7. Deliverables

- `deliverables/loams_api/auth_helper.php`: payload-aware key extraction.
- `deliverables/loams_api/{search_students,get_library_visits,get_visitors,get_report_data,get_report_time_data,dashboard_summary}.php`: `requireAdminAuth()` guard.
- Client: `StudentController`/`SearchViewModel` (JSON body), `ReportController` (JSON body ×2), `VisitLogsViewModel::refresh()` guest branch (inline `get_visitors.php` JSON POST, `cpp:123`) and student branch (`get_library_visits.php` **GET→POST**, key in urlencoded body, filters kept in query string), `DashboardViewModel` (`dashboard_summary.php` **GET→POST**) — thread `admin_key` from `AdminSession::instance().key()`; new `AdminSession.h` includes in the read VMs; per-path 401-in-error-state test. (`VisitorController::fetchVisitors` is legacy-only and intentionally not threaded — §6.)
- Client tests: key-attached + 401-handling cases for each path.
- A manual server-verification checklist (curl with/without key → 200/401) in the PR body or a docs note.

## 8. Phase 6 slice map (for context)

1. **6a — Admin read-endpoint auth** (this spec).
2. **6b — Upload hardening** (`upload_students_zip.php`).
3. **6c — Info-disclosure & config hygiene** (`display_errors`, raw error echo, CORS, dead `config.php`).
4. **6d — Transport & sessions** (HTTPS; replace shared `admin_key` with sessions/tokens) — likely its own multi-slice sub-project.
5. Retire the legacy Widgets rollback app once 2.0 is trusted.
