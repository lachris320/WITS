# Access Control Backend Endpoints — Design (Sub-plan 2)

**Date:** 2026-09-28
**Status:** In codex-review (design-spec gate) → then `writing-plans`
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

## Producer invariant (what actually writes `turnstile_events`)

`turnstile.php` is the *only* writer of `turnstile_events`, and it publishes a row **only** when
all of the following hold (verified against `turnstile.php:302-449`):

1. The reader is the **ENTRY** reader (reader index 0). A non-entry reader (exit / stray index)
   is allowed through but **no** row is written (`turnstile.php:344-347`). So every published row
   is `reader = 0`.
2. The card **resolved to a student at gate time** (`code` then `school_id` fallback). An
   unresolved card is denied and the request exits *before* any row is written
   (`turnstile.php:335-338`).
3. The insert happens inside the same transaction as the attendance write + retransmit-dedup
   guard, and becomes visible only on `COMMIT` (`turnstile.php:354-446`).

These two read endpoints depend on that invariant, so it is stated here and asserted in tests. If
a future producer starts publishing EXIT rows or rejected swipes, this spec's "every row is an
observed entry" assumption must be revisited (the endpoints already carry `reader` in the payload
so a consumer can distinguish, but v1 does not filter on it because only reader-0 rows exist).

## Non-goals / scope

- **No schema change.** Both endpoints read the existing `turnstile_events` and `students`
  tables. No migration ships with this slice.
- **No attendance mutation.** Neither endpoint writes `library_visits`, consumes a
  `turnstile_events` token, or increments `students.visits`. `turnstile.php` remains the sole
  authoritative attendance writer (no double-count risk). A missed or re-shown *display* has no
  effect on attendance.
- **Legacy paths untouched.** `rfid_login.php`, `turnstile_pull.php`, and the PowerShell bridge
  are legacy-only and are not modified. The card→student resolver is *extracted into a new shared
  include* (below) rather than refactored out of `rfid_login.php`, so the deployed legacy login
  path carries zero regression risk from this slice.
- **Client-side cursor persistence, baseline-on-start, kiosk wiring, and admin UI are out of
  scope** — they are Sub-plans 3 and 4. This slice is the stateless backend contract only; the
  client-side responsibilities it *requires* are listed under *Cursor semantics* so Sub-plan 3
  implements them.

## Global constraints (verbatim from platform design + project rules)

- **Security-hygiene:** no admin keys, backend DB credentials, or real student PII (names, school
  IDs, photos, visit logs) in source, tests, fixtures, or commits. Tests use synthetic data only,
  seeded into a uniquely-named throwaway schema, with a runtime-generated ephemeral admin secret.
- **`admin_key` must never travel via GET** — it would leak into access logs. `access_recent.php`
  is POST-only (405 otherwise, enforced *before* auth) and reads the key through
  `auth_helper.php::extractAdminKey()` ($_POST / JSON body only).
- **No PII in logs.** Neither endpoint may `error_log()` a card value or a resolved student
  payload. On failure they log a generic message only (matches `turnstile.php:455-459`).
- **Plaintext-HTTP security debt is documented, not fixed here.** Both endpoints expose entry PII
  over plaintext HTTP in the current deployment; `turnstile_display.php` rests on the loopback
  trust boundary and `access_recent.php` on `admin_key`. TLS + per-user identity/RBAC + device
  tokens are the parallel security track that is a HARD GATE before any real go-live. Each
  endpoint carries a header comment stating this debt; the PR body repeats it.

## Shared DB connection contract (both endpoints)

Do **not** naively `include 'db.php'`. `db.php` sets `display_errors = 1` and `die()`s with an
HTTP **200** on a connection failure (`db.php:2-15`), which both leaks internals and breaks the
JSON error contract before any endpoint handler can run; it also never sets a charset, so student
names would round-trip through the connection's default charset and can corrupt the JSON.

Instead mirror `turnstile.php:303-312`:

```php
require_once __DIR__ . '/config.php';   // DB_HOST/DB_USER/DB_PASS/DB_NAME
ini_set('display_errors', '0');         // never leak warnings into JSON
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$conn = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
$conn->set_charset('utf8mb4');          // student names are unicode
```

Wrap the body in `try { … } catch (Throwable $e) { … }`. On failure, `error_log()` a
**sanitized** line — the exception **class and code only** (e.g. `get_class($e) . ' code ' .
$e->getCode()`), **never** `$e->getMessage()`, which can carry SQL fragments or connection detail
and would violate the no-PII / no-internals logging rule. A single generic JSON error writer then
emits `{"status":"error","message":"Internal server error"}` with HTTP 500 — never SQL,
connection, or stack detail to the client. Both endpoints also send `Cache-Control: no-store`
(responses carry PII) and check the return of `json_encode()` before echoing (emit the generic 500
if encoding fails).

---

## Endpoint 1 — `turnstile_display.php`

**Method:** `GET` only. Any other method → `http_response_code(405)` +
`{"status":"error","message":"Method not allowed"}` + exit, before any other work.

**Guard:** loopback-only. `$remoteIp = $_SERVER['REMOTE_ADDR'] ?? '';` then
`if (!in_array($remoteIp, ['127.0.0.1', '::1'], true)) { http_response_code(403); … exit; }` —
fail-closed, before any DB work, mirroring `turnstile_pull.php:40-47` (which likewise rejects the
IPv4-mapped form `::ffff:127.0.0.1`; kept consistent and fail-closed here). No `admin_key`:
loopback is the trust boundary, and a secret in a GET query string would leak to logs.

**Deployment invariant (must hold for the loopback guard to mean anything):** the endpoint must
be served directly by the local PHP/Apache instance, never behind a reverse proxy that forwards
remote traffic to it — otherwise PHP sees the proxy as `127.0.0.1` and the boundary is defeated.
The guard consults **only** `REMOTE_ADDR`; it must never consult `X-Forwarded-For`, `Forwarded`,
or any client-supplied header. This invariant is stated in the file header comment and in the PR.

**Query params:** `?since=<id>` — the client's last **processed** event id. Accepted only as a
scalar, ASCII-decimal string (validate with `is_string($raw) && ctype_digit($raw)`); **any**
other form — absent, an array (`since[]=…`), a negative sign, or a partly-numeric value like
`"12abc"` — is treated as `0`, not coerced with `(int)` (which would silently turn `"12abc"` into
`12` and warn on an array). After validation, cast the digit string to int for binding.

### Cursor semantics (load-bearing — corrected in review)

The endpoint returns the **oldest next** event with `id > since`, one at a time, so the client can
walk the backlog in id order:

- Query: `SELECT id, card, reader, created_at FROM turnstile_events WHERE id > ? ORDER BY id ASC LIMIT 1`
  (bind `since` as an int).
- `latest_id` = `SELECT MAX(id) FROM turnstile_events` (or `0` when empty / `NULL`). It is
  **informational backlog metadata only** — the client advances its processed cursor to the
  **returned entry's `id`**, never to `latest_id`.

**Delivery guarantee (narrowed after review):** *provided rows become visible in id order*, the
endpoint delivers every **committed** row in strictly increasing id order to a client that
advances its cursor only by returned-entry id. That proviso is not free: `turnstile_events` ids
are assigned at INSERT but rows appear at COMMIT (`turnstile.php:443-446`), so if two entries
commit out of id order (id 102 visible and consumed before id 101 commits) the client *would* skip
the lower id. Under the deployment's **producer invariant** — a single turnstile lane whose
controller issues one request per swipe and waits for the gate response — entries are effectively
serialized and this reordering does not occur in practice. The residual risk is bounded and **display-only**: at worst one welcome screen is not
shown. Attendance is unaffected (`turnstile.php` is authoritative), so this is an accepted
trade-off for a display channel, exactly like the legacy bridge it replaces. The spec deliberately
does **not** add a display-acknowledgement column or serialization coordination for v1; if a
multi-lane deployment ever makes concurrent out-of-order commits real, revisit with a
commit-ordered marker (e.g. an `observed_seq` assigned at commit) — noted as a forward item.

**Client responsibilities (Sub-plan 3, required by this contract):**
- **Persist the cursor** across restarts and **dedupe by id** (idempotent apply), so a crash
  between display and cursor-save re-shows at most one entry and never double-counts anything
  (there is no attendance write to double).
- **Baseline on a *fresh* start only** (no persisted cursor): issue one call, read `latest_id`,
  set the cursor there, and begin consuming from the next event — so pre-existing historical rows
  are not replayed onto the kiosk. A restart *with* a persisted cursor **resumes from it and
  consumes the downtime backlog** (every row that arrived while the app was down is displayed in
  order). Only the first-ever start, with no cursor to resume, skips the pre-existing history.

**Steady state:** `since=100` with committed rows 101, 102, 103 yields 101, then 102, then 103,
then `entry: null` — every committed event consumed in order.

### Card → student resolution

Resolve the row's `card` via the shared helper (see *Shared code*): `students.code` first, then
`students.school_id` fallback — the same resolution `turnstile.php` and `rfid_login.php` perform.
**Read-only** — no attendance insert, no token consume, no `visits` increment.

**Student projection (enumerated, not a raw row).** The endpoint returns a fixed, data-minimized
set of student fields — not `SELECT *` — so the provider contract is stable against `students`
schema changes and no unneeded PII is exposed. Display projection (what `KioskViewModel`
consumes): `name, school_id, course, department, year_level, gender, status, photo_path`.
`photo_path` is a **normalized relative path** (see helper), not an absolute URL.

### Response shapes

```jsonc
// A next entry exists (resolved student):
{ "status": "success", "latest_id": 103,
  "entry": { "id": 101, "card": "…", "created_at": "2026-09-28 09:14:02", "reader": 0,
             "student": { "name": "…", "school_id": "…", "course": "…", "department": "…",
                          "year_level": "…", "gender": "…", "status": "…",
                          "photo_path": "uploads/…" } } }

// A next entry exists but the card no longer resolves to a student (see below):
{ "status": "success", "latest_id": 103,
  "entry": { "id": 101, "card": "…", "created_at": "…", "reader": 0, "student": null } }

// Nothing newer than `since`:
{ "status": "success", "latest_id": 103, "entry": null }

// Empty table:
{ "status": "success", "latest_id": 0, "entry": null }

// Non-GET / non-loopback caller:
// HTTP 405 { "status": "error", "message": "Method not allowed" }
// HTTP 403 { "status": "error", "message": "Forbidden" }
```

**`student: null` is a defensive data-consistency fallback, not the "unregistered card" case.**
Per the producer invariant, the gate never publishes a row for a card that failed to resolve — so
`student: null` at read time means the card resolved when the gate admitted it but the student row
was **deleted or its `code`/`school_id` edited afterward**. The endpoint surfaces the entry with
`student: null` (rather than dropping it) so the cursor still advances and the kiosk can show a
neutral "entry recorded" state instead of stalling on a now-orphaned row.

---

## Endpoint 2 — `access_recent.php`

**Method:** `POST` only. Enforce `$_SERVER['REQUEST_METHOD'] === 'POST'` → else 405 + exit,
**before** `requireAdminAuth()`, so a non-POST request carrying a JSON body can never authenticate
(`extractAdminKey()` reads the body regardless of method).

**Guard:** after the method check — the shared connection contract above, then
`require_once 'auth_helper.php'; requireAdminAuth($conn);` (the `dashboard_summary.php:11-12`
pattern). The admin key arrives via `extractAdminKey()` ($_POST / JSON body, never GET).

**Request body:** none required in v1 (fixed newest-50 window). Any body beyond the `admin_key`
the guard reads is ignored.

### Response

```jsonc
{ "status": "success",
  "entries": [   // newest first, up to 50
    { "id": 103, "card": "…", "created_at": "2026-09-28 09:14:02", "reader": 0,
      "student": { "name": "…", "school_id": "…", "course": "…", "department": "…",
                   "photo_path": "uploads/…" } },   // or "student": null (see Endpoint 1)
    …
  ],
  "entries_today": 42,               // COUNT over today's range
  "last_entry_at": "2026-09-28 09:14:02" }  // or null when the table is empty
```

- `entries`: `SELECT id, card, reader, created_at FROM turnstile_events ORDER BY id DESC LIMIT 50`,
  each row resolved to a **slim** student projection (`name, school_id, course, department,
  photo_path`) — the admin table needs less than the display endpoint. `student: null` for an
  orphaned card, entry still included (same rationale as Endpoint 1).
- `entries_today`:
  `SELECT COUNT(*) FROM turnstile_events WHERE created_at >= CURDATE() AND created_at < CURDATE() + INTERVAL 1 DAY`
  — a half-open range, **not** `DATE(created_at) = CURDATE()`, so it stays sargable against a
  future index on `created_at`. **Timezone invariant:** neither endpoint (nor `turnstile.php`)
  overrides the connection's session timezone, so the writer's `NOW()` and this reader's
  `CURDATE()` both resolve against the **MySQL server's default timezone**. "Today" is that server
  day. This must hold even though the writer and reader run in separate PHP requests / mysqli
  connections — it does, because both inherit the same server default and neither issues
  `SET time_zone`. The endpoints therefore need no per-connection tz setup; the correctness
  dependency is simply "don't set a session tz." (Tests assert the boundary using the DB's own
  clock — see Testing — rather than a PHP wall-clock, so no cross-connection tz assumption is
  needed.)
- `last_entry_at`: `SELECT MAX(created_at) FROM turnstile_events` (`null` when empty).
- **Gate-specific by design:** counts `turnstile_events`, never `library_visits`, so it does not
  duplicate `dashboard_summary.php` / `get_library_visits.php`, which count general attendance.

---

## Shared code — `access_helpers.php` (NEW)

Both endpoints resolve a card to a student the same way, so that logic lives once in a new
`deliverables/loams_api/access_helpers.php`:

- `resolveStudentByCard(mysqli $conn, string $card): ?array` — `SELECT * FROM students WHERE code
  = ? LIMIT 1`, then fallback `WHERE school_id = ? LIMIT 1`; returns the associative row or `null`.
  Prepared statements only. Callers pick their enumerated projection from the returned row (the
  helper does the lookup; it does not decide the projection).
- `normalizeStudentPhotoPath(?array $student): string` — returns a **contained relative** path
  under `uploads/` (e.g. `uploads/abc.jpg`), or `uploads/default.jpg`. Containment is enforced, not
  assumed: the stored `photo` value is **rejected** (→ `uploads/default.jpg`) if it is empty, is an
  absolute path, contains `..`, contains a backslash or any alternate separator, or does not
  resolve inside the canonical uploads directory. Concretely: reject on those lexical checks, then
  confirm `realpath(__DIR__ . '/' . $photo)` exists **and** is a prefix-match under
  `realpath(__DIR__ . '/uploads')`; only then return the relative path, else the default. A naive
  `file_exists(__DIR__ . '/' . $photo)` alone is insufficient — `uploads/../config.php` would pass
  it. It returns a **relative path, not an absolute URL** — deliberately unlike `rfid_login.php`,
  which builds an absolute URL from the raw `HTTP_HOST` (an attacker-controllable header → poisoned
  URL). The desktop / admin client already knows its base URL (`ApiConfig`) and composes the full
  URL itself, so the backend never trusts `HTTP_HOST`.

`rfid_login.php` is **not** modified in this slice — the helper is a fresh extraction, so the
legacy login path is unaffected. (A later, separate cleanup could retrofit `rfid_login.php` onto
the helper; explicitly out of scope here.)

## Error handling

- Prepared statements throughout. On any DB/prepare/`json_encode` failure, `error_log()` only the
  exception **class and code** (no message, no card, no student payload, no SQL) and return
  `{"status":"error","message":"Internal server error"}` with HTTP 500 via the single shared error
  writer — never leak SQL, connection, or `HTTP_HOST` detail to the client or the log.
- `turnstile_display.php`: non-GET → 405; non-loopback → 403 — both before any DB access. A `since`
  that is not a scalar ASCII-decimal string (array, signed, partly-numeric, absent) → treated as
  `0`, not an error.
- `access_recent.php`: non-POST → 405 before auth; `requireAdminAuth()` owns 401 on missing/invalid
  key and exits.

## Testing (TDD, synthetic data only)

A throwaway-DB PHP harness at `deliverables/loams_api/tests/access_display_test.php` that:
- creates a **uniquely-named** schema (e.g. suffixed with a random token) and binds `php -S` to an
  **ephemeral port**, so concurrent test runs never collide (the legacy
  `turnstile_integration_test.php` uses a fixed DB + port — this harness must not);
- seeds **synthetic** `students` + `turnstile_events` rows (no real PII);
- generates an **ephemeral admin secret** at runtime and installs its hash, so no admin key is
  ever committed;
- seeds and asserts date boundaries using the **database's own clock** (`NOW()`, `NOW() - INTERVAL
  1 DAY`, `CURDATE()`) rather than a PHP wall-clock, so the test needs no assumption about how the
  endpoint's separate connection is timezoned — it exercises the exact same server-tz `CURDATE()`
  the endpoint uses;
- tears the schema down on exit.

Assertions:

**`turnstile_display.php`**
1. Non-GET method → 405; non-loopback `REMOTE_ADDR` → 403, no data leaked. (Loopback path is
   exercised via the local `php -S`, whose `REMOTE_ADDR` is the real Windows/Apache loopback form.)
2. `since` filtering returns the **oldest** `id > since`, not the newest (seed 3 rows, walk
   101→102→103→null); `latest_id` stays at MAX across all calls.
3. Empty table → `latest_id: 0`, `entry: null`.
4. Card resolves by `code`; card resolves by `school_id` fallback; **orphaned card** (a
   `turnstile_events` row whose matching student was deleted after insert) → entry present with
   `student: null` and the cursor still advances.
5. `photo_path` is relative and falls back to `uploads/default.jpg` when the file is absent; no
   absolute URL and no `HTTP_HOST` in the payload.
6. No row is mutated (assert `turnstile_events` / `library_visits` / `students.visits` unchanged
   after calls).
7. **Ordinary next-poll catch-up:** a row inserted *after* a poll returns is delivered by the
   following poll (walk to `entry: null`, insert a new row, assert the next call returns it and the
   cursor advances). This is plain sequential behavior, **not** a between-queries race repro. The
   concurrent-out-of-order-commit gap (a lower id committing after a higher id was already consumed)
   is a documented limitation, **not** asserted here: it cannot occur under the single-lane producer
   invariant and would require two deliberately interleaved transactions to force, which this
   single-threaded harness cannot express.

**`access_recent.php`**
8. Non-POST method → 405 **before** auth (a GET carrying a valid `admin_key` JSON body is still
   rejected 405, proving method-before-auth ordering).
9. Missing / wrong `admin_key` on POST → 401 (via `requireAdminAuth`).
10. Valid key → newest ≤50 entries, id DESC; slim student projection; orphaned card → `student:
    null`.
11. `entries_today` counts only today's rows via the range predicate, correct across the midnight
    boundary — seed boundary rows with the DB's own clock (`created_at = CURDATE() - INTERVAL 1
    SECOND` for "yesterday 23:59:59" and `created_at = CURDATE()` for "today 00:00:00") so the
    assertion uses the same server-tz day the endpoint's `CURDATE()` sees; `last_entry_at` = MAX,
    null when empty.

**DB-failure shape:** a forced connection/query failure returns the generic
`{"status":"error"}` JSON with HTTP 500 and leaks no SQL/connection detail (verify the endpoints
do not inherit `db.php`'s displayed-error / HTTP-200-die behavior).

**Regression:** legacy `deliverables/loams_api/tests/turnstile_integration_test.php` stays green
(these endpoints do not touch the tables it exercises for writes).

## Verification

- `php -l` clean on all three new files.
- The new `access_display_test.php` harness passes.
- Legacy `turnstile_integration_test.php` passes unchanged.

## Security debt (explicit, carried to the PR)

Both endpoints expose resolved student PII (name, school id, photo) over **plaintext HTTP** in the
current deployment. `turnstile_display.php` is defended by the loopback boundary
(127.0.0.1 / ::1 only, no proxy, `REMOTE_ADDR`-only); `access_recent.php` by `admin_key` over POST.
Both send `Cache-Control: no-store` and never log PII. This is acceptable *only* for the
pre-go-live framework build. TLS + per-user identity/RBAC + device tokens is the mandatory parallel
security track and a hard gate before this platform drives a real gate or ships to a customer.
Header comments in both files and the PR body state this.

## Forward notes (deferred, recorded so they are not lost)

- **Commit-ordered delivery marker** (e.g. `observed_seq` assigned at commit, or a display-ack
  column) — only needed if a *multi-lane* deployment makes concurrent out-of-order commits real.
  Not built in v1; the id cursor is sufficient for the single-lane producer.
- **Retrofit `rfid_login.php` onto `access_helpers.php`** — a separate cleanup once these endpoints
  prove the extraction; keeps this slice's blast radius off the deployed legacy path.
- **Index on `created_at`** — the `entries_today` range predicate is already sargable for when such
  an index is added; not added here (no schema change in this slice).
