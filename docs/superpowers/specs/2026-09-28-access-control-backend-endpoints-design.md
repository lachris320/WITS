# Access Control Backend Endpoints — Design (Sub-plan 2)

**Date:** 2026-09-28
**Status:** Approved (brainstorming), ready for `/codex-review` → `writing-plans`
**Part of:** LOAMS 2.0 Access Control platform (approved design 2026-09-25). See
`docs/superpowers/plans/2026-09-25-access-control-core-seam.md` (Sub-plan 1, MERGED to master,
PR #56 / squash `30f2cbe`) for the desktop core seam this backend feeds.

## Goal

Add the two PHP endpoints the LOAMS 2.0 Access Control platform needs, so that (Sub-plan 3) a
native `TurnstileProvider` can drive the kiosk display without the legacy PowerShell keyboard
bridge, and (Sub-plan 4) the admin Access Control page can show recent gate entries and health.

- **`turnstile_display.php`** — loopback-only, read-only GET the desktop `TurnstileProvider`
  polls for the next resolved gate entry (native display channel).
- **`access_recent.php`** — `admin_key`-guarded POST returning the newest gate entries plus
  gate-specific counts for the admin table + health tiles.

Both live in `deliverables/loams_api/`, read existing tables only, and mutate nothing.

## Non-goals / scope

- **No schema change.** Both endpoints read the existing `turnstile_events` and `students`
  tables. No migration ships with this slice.
- **No attendance mutation.** Neither endpoint writes `library_visits`, consumes a
  `turnstile_events` token, or increments `students.visits`. `turnstile.php` remains the sole
  authoritative attendance writer (no double-count risk).
- **Legacy paths untouched.** `rfid_login.php`, `turnstile_pull.php`, and the PowerShell bridge
  are legacy-only and are not modified. The card→student resolver is *extracted into a new shared
  include* (below) rather than refactored out of `rfid_login.php`, so the deployed legacy login
  path carries zero regression risk from this slice.
- **Desktop provider, kiosk wiring, and admin UI are out of scope** — they are Sub-plans 3 and 4.
  This slice is the backend contract only.

## Global constraints (verbatim from platform design + project rules)

- **Security-hygiene:** no admin keys, backend DB credentials, or real student PII (names, school
  IDs, photos, visit logs) in source, tests, fixtures, or commits. Tests use synthetic data only.
- **`admin_key` must never travel via GET** — it would leak into access logs. `access_recent.php`
  is POST and reads the key through `auth_helper.php::extractAdminKey()` ($_POST / JSON body only).
- **Plaintext-HTTP security debt is documented, not fixed here.** Both endpoints expose entry PII
  over plaintext HTTP in the current deployment; `turnstile_display.php` rests on the loopback
  trust boundary and `access_recent.php` on `admin_key`. TLS + per-user identity/RBAC + device
  tokens are the parallel security track that is a HARD GATE before any real go-live. Each
  endpoint carries a header comment stating this debt; the PR body repeats it.

---

## Endpoint 1 — `turnstile_display.php`

**Method:** `GET`
**Guard:** loopback-only. `$remoteIp = $_SERVER['REMOTE_ADDR'] ?? '';` then
`if (!in_array($remoteIp, ['127.0.0.1', '::1'], true)) { http_response_code(403); … exit; }` —
fail-closed, before any DB work, mirroring `turnstile_pull.php`. No `admin_key`: loopback is the
trust boundary, and a secret in a GET query string would leak to logs.

**Query params:** `?since=<id>` — the client's last **processed** event id. Optional; absent or
non-numeric is treated as `0`. Cast with `(int)` / validate as a non-negative integer.

### Cursor semantics (load-bearing — corrected in review)

The provider must observe **every** physical gate event in order; it must never skip rows that
arrive between polls. Therefore the endpoint returns the **oldest next** unprocessed event, one at
a time — it does **not** jump to the newest.

- Query: `SELECT … FROM turnstile_events WHERE id > ? ORDER BY id ASC LIMIT 1` (bind `since`).
- `latest_id` = `SELECT MAX(id) FROM turnstile_events` (or `0` when empty). It is **informational
  backlog metadata only** (lets the client gauge how far behind it is); the client advances its
  processed cursor to the **returned entry's `id`**, never to `latest_id`.
- **Startup baseline:** the *client* (`TurnstileProvider`, Sub-plan 3) baselines `since` to the
  current `latest_id` on start so history is not replayed. The endpoint itself is stateless and
  needs no baseline flag — a fresh client simply issues one call, reads `latest_id`, and sets its
  cursor there before it begins consuming. (Documented here so Sub-plan 3 implements it; the
  endpoint contract below is what makes it possible by always returning `latest_id`.)
- **Steady state:** the client polls repeatedly; each non-null entry advances `since` to that
  entry's `id`, and it keeps polling until `entry` is `null`. So `since=100` with rows 101, 102,
  103 present yields 101, then 102, then 103, then null — every event consumed in order.

### Card → student resolution

Mirror `rfid_login.php` exactly, via the shared helper (see *Shared code*): resolve the row's
`card` against `students.code` first, then fall back to `students.school_id`; attach a
`photo_url` built `__DIR__`-relative with an `uploads/default.jpg` fallback. **Read-only** — no
attendance insert, no token consume, no `visits` increment.

### Response shapes

```jsonc
// A next entry exists (resolved student):
{ "status": "success", "latest_id": 103,
  "entry": { "id": 101, "card": "…", "created_at": "2026-09-28 09:14:02", "reader": 0,
             "student": { /* full students row */ "photo_url": "http://…/uploads/…" } } }

// A next entry exists but the card is not registered:
{ "status": "success", "latest_id": 103,
  "entry": { "id": 101, "card": "…", "created_at": "…", "reader": 0, "student": null } }

// Nothing newer than `since`:
{ "status": "success", "latest_id": 103, "entry": null }

// Empty table:
{ "status": "success", "latest_id": 0, "entry": null }

// Non-loopback caller:
// HTTP 403  { "status": "error", "message": "Forbidden" }
```

**Unknown card → `student: null` with the entry still present** (approved). An unregistered
credential is a legitimate access-control event; surfacing it lets the kiosk show an "unregistered
card" state and keeps gate monitoring honest, instead of silently dropping a real event and
stalling the cursor.

---

## Endpoint 2 — `access_recent.php`

**Method:** `POST`
**Guard:** `header('Content-Type: application/json'); include 'db.php'; require_once
'auth_helper.php'; requireAdminAuth($conn);` — the exact `dashboard_summary.php` pattern. The
admin key arrives via `extractAdminKey()` ($_POST / JSON body, never GET).

**Request body:** none required in v1 (fixed newest-50 window). Any body is ignored beyond the
`admin_key` the guard reads.

### Response

```jsonc
{ "status": "success",
  "entries": [   // newest first, up to 50
    { "id": 103, "card": "…", "created_at": "2026-09-28 09:14:02", "reader": 0,
      "student": { "name": "…", "school_id": "…", "course": "…", "department": "…",
                   "photo_url": "http://…" } },   // or "student": null for an unknown card
    …
  ],
  "entries_today": 42,               // COUNT over today's range
  "last_entry_at": "2026-09-28 09:14:02" }  // or null when the table is empty
```

- `entries`: `SELECT … FROM turnstile_events ORDER BY id DESC LIMIT 50`, each row resolved to a
  **slim** student projection (`name, school_id, course, department, photo_url`) — the admin table
  does not need every column, unlike the display endpoint's full row. Unknown card → `student:
  null`, entry still included (same rationale as Endpoint 1).
- `entries_today`:
  `SELECT COUNT(*) FROM turnstile_events WHERE created_at >= CURDATE() AND created_at < CURDATE() + INTERVAL 1 DAY`
  — a half-open range, **not** `DATE(created_at) = CURDATE()`, so it stays sargable against an
  index on `created_at` if one is added later.
- `last_entry_at`: `SELECT MAX(created_at) FROM turnstile_events` (null when empty).
- **Gate-specific by design:** counts `turnstile_events`, never `library_visits`, so it does not
  duplicate `dashboard_summary.php` / `get_library_visits.php`, which count general attendance.

---

## Shared code — `access_helpers.php` (NEW)

Both endpoints resolve a card to a student the same way, so that logic lives once in a new
`deliverables/loams_api/access_helpers.php`:

- `resolveStudentByCard(mysqli $conn, string $card): ?array` — `SELECT * FROM students WHERE code
  = ? LIMIT 1`, then fallback `WHERE school_id = ? LIMIT 1`; returns the associative row or `null`.
  Prepared statements only.
- `buildStudentPhotoUrl(array $student): string` — the corrected `__DIR__`-relative resolution
  from `rfid_login.php` (host + script-dir base URL; existence check against
  `__DIR__ . '/' . photo`; `uploads/default.jpg` fallback).

`turnstile_display.php` returns the full resolved row with `photo_url` attached;
`access_recent.php` picks the slim projection from it. `rfid_login.php` is **not** modified in this
slice — the helper is a fresh extraction, so the legacy login path is unaffected. (A later,
separate cleanup could retrofit `rfid_login.php` onto the helper; explicitly out of scope here.)

## Error handling

- Prepared statements throughout; on a DB/prepare failure, `error_log()` the detail and return a
  generic `{"status":"error","message":"…"}` with an appropriate HTTP code — never leak SQL or
  connection detail to the client (matches `rfid_login.php` / `dashboard_summary.php`).
- `turnstile_display.php`: non-loopback → 403 before any DB access. Malformed `since` → treated as
  0, not an error.
- `access_recent.php`: `requireAdminAuth()` owns 401 on missing/invalid key and exits.

## Testing (TDD, synthetic data only)

A throwaway-DB PHP harness at `deliverables/loams_api/tests/access_display_test.php` (spins up a
temp schema + `php -S`, seeds **synthetic** `students` + `turnstile_events` rows — no real PII),
asserting:

**`turnstile_display.php`**
1. Non-loopback `REMOTE_ADDR` → 403, no data leaked.
2. `since` filtering returns the **oldest** `id > since`, not the newest (seed 3 rows, walk
   101→102→103→null); `latest_id` stays at MAX across all calls.
3. Empty table → `latest_id: 0`, `entry: null`.
4. Card resolves by `code`; card resolves by `school_id` fallback; unknown card → entry present
   with `student: null`.
5. `photo_url` present with the `uploads/default.jpg` fallback when the file is absent.
6. No row is mutated (assert `turnstile_events` / `library_visits` / `students.visits` unchanged
   after calls).

**`access_recent.php`**
7. Missing / wrong `admin_key` → 401 (via `requireAdminAuth`).
8. Valid key → newest ≤50 entries, id DESC; slim student projection; unknown card → `student:
   null`.
9. `entries_today` counts only today's rows via the range predicate; `last_entry_at` = MAX, null
   when empty.

**Regression:** legacy `deliverables/loams_api/tests/turnstile_integration_test.php` stays green
(these endpoints do not touch the tables it exercises for writes).

## Verification

- `php -l` clean on all three new files.
- The new `access_display_test.php` harness passes.
- Legacy `turnstile_integration_test.php` passes unchanged.

## Security debt (explicit, carried to the PR)

Both endpoints expose resolved student PII (name, school id, photo) over **plaintext HTTP** in the
current deployment. `turnstile_display.php` is defended by the loopback boundary
(127.0.0.1 / ::1 only); `access_recent.php` by `admin_key`. This is acceptable *only* for the
pre-go-live framework build. TLS + per-user identity/RBAC + device tokens is the mandatory
parallel security track and a hard gate before this platform drives a real gate or ships to a
customer. Header comments in both files and the PR body state this.
