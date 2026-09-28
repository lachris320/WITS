# Access Control Backend Endpoints Implementation Plan (Sub-plan 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two read-only PHP endpoints (`turnstile_display.php`, `access_recent.php`) plus a shared `access_helpers.php`, so LOAMS 2.0's desktop provider and admin page can consume gate entries natively.

**Architecture:** Two thin endpoints in `deliverables/loams_api/` read the existing `turnstile_events` + `students` tables via prepared statements and return JSON; a new `access_helpers.php` holds the card→student resolver, a contained photo-path normalizer, and two pure request-parsing helpers (`isLoopback`, `parseSinceParam`) that make the loopback/method/cursor guards unit-testable. A single throwaway-DB PHP harness (unique schema + ephemeral port + `php -S`, mirroring the existing `turnstile_integration_test.php`) drives both endpoints over HTTP with synthetic data. No schema change; no attendance mutation (`turnstile.php` stays the sole writer).

**Tech Stack:** PHP 8 (CLI + built-in server), mysqli prepared statements, MariaDB/MySQL as `root` (dev), `curl` in the CLI for the test harness. Design spec: `docs/superpowers/specs/2026-09-28-access-control-backend-endpoints-design.md` (codex-APPROVED 2026-09-28).

## Global Constraints

- **No attendance mutation.** Neither endpoint writes `library_visits`, consumes a `turnstile_events` token, or increments `students.visits`. Read-only.
- **No schema change.** Reads existing `turnstile_events` and `students` only.
- **`rfid_login.php` / `turnstile_pull.php` / `turnstile.php` are NOT modified.** The resolver is a fresh extraction into `access_helpers.php`.
- **Security-hygiene:** no admin keys, DB credentials, or real student PII in source/tests/commits. Tests use only synthetic data in a uniquely-named throwaway schema with a runtime-generated ephemeral admin secret.
- **`admin_key` never via GET.** `access_recent.php` is POST-only (405 otherwise), method-checked *before* auth.
- **No PII / internals in logs.** On failure, `error_log()` the exception **class and code only** (`get_class($e) . ' code ' . $e->getCode()`) — never `getMessage()`, the card, or a student payload.
- **DB connection via `config.php`, NOT `include db.php`** (db.php sets `display_errors=1`, `die()`s HTTP 200 on connect failure, and sets no charset). Each endpoint: `require_once config.php` → `ini_set('display_errors','0')` → `mysqli_report(MYSQLI_REPORT_ERROR|MYSQLI_REPORT_STRICT)` → `new mysqli(DB_*)` → `set_charset('utf8mb4')`.
- **Both responses:** `Content-Type: application/json` + `Cache-Control: no-store`; `json_encode()` return checked before echo (generic 500 on failure).
- **`photo_path` is a contained RELATIVE path** (`uploads/…` or `uploads/default.jpg`) — never an absolute URL, never derived from `HTTP_HOST`.
- **PHP:** target PHP 8.0+ (uses `str_contains`, `??`, typed signatures). Match the flat-file, `{"status":…}` JSON conventions of the surrounding `loams_api/` endpoints.

---

## File Structure

- **Create** `deliverables/loams_api/access_helpers.php` — shared: `isLoopback()`, `parseSinceParam()`, `resolveStudentByCard()`, `normalizeStudentPhotoPath()`. Pure functions + two DB/FS helpers. Included by both endpoints. Safe to `require` in-process (defines functions only; no side effects).
- **Create** `deliverables/loams_api/turnstile_display.php` — GET, loopback-only, read-only cursor feed.
- **Create** `deliverables/loams_api/access_recent.php` — POST, admin_key-guarded, newest-50 + counts.
- **Create** `deliverables/loams_api/tests/access_display_test.php` — throwaway-DB harness driving both endpoints over `php -S`; built in Task 1 (display), extended in Task 2 (admin/auth).

Task 1 delivers `access_helpers.php` + `turnstile_display.php` + the harness (its first consumer proves the resolver + photo normalizer). Task 2 delivers `access_recent.php` and extends the harness with the `admin` table + ephemeral secret. A reviewer can accept/reject either endpoint independently.

---

### Task 1: Shared helpers + `turnstile_display.php` + test harness

**Files:**
- Create: `deliverables/loams_api/access_helpers.php`
- Create: `deliverables/loams_api/turnstile_display.php`
- Create: `deliverables/loams_api/tests/access_display_test.php`

**Interfaces:**
- Produces (consumed by Task 2 and by Sub-plans 3/4):
  - `isLoopback(?string $remoteIp): bool` — true only for `'127.0.0.1'` / `'::1'`.
  - `parseSinceParam($raw): int` — scalar ASCII-decimal → int; else 0; `> PHP_INT_MAX` → `PHP_INT_MAX`.
  - `resolveStudentByCard(mysqli $conn, string $card): ?array` — `code` then `school_id`; assoc row or null.
  - `normalizeStudentPhotoPath(?array $student): string` — contained relative path or `uploads/default.jpg`.
  - Harness helpers in the test file: `ok(string,bool)`, `httpGet(string):array`, `httpPostForm(string,array):array`, `freePort():int`, plus the throwaway-DB bootstrap/teardown.
  - `turnstile_display.php` HTTP contract: `GET ?since=<id>` → `{"status":"success","latest_id":N,"entry":{…}|null}`.

- [ ] **Step 1: Write the failing pure-helper unit tests**

Create `deliverables/loams_api/tests/access_display_test.php` starting with in-process unit tests of the pure helpers (these need no DB/HTTP). This block runs first so the harness cost is only paid once the pure logic is green.

```php
<?php
/**
 * Access Control display/recent endpoints — integration + unit harness.
 * Drives the REAL endpoint files over HTTP against a THROWAWAY database with a
 * UNIQUE name + EPHEMERAL port + runtime-generated admin secret. Synthetic data
 * only; never touches wits_app. Run:
 *   php deliverables/loams_api/tests/access_display_test.php
 * Needs: PHP CLI with mysqli + curl, reachable MariaDB/MySQL as root.
 */
declare(strict_types=1);
error_reporting(E_ALL);
ini_set('display_errors', '1');

$apiDir = dirname(__DIR__);                 // deliverables/loams_api
require_once $apiDir . '/access_helpers.php'; // functions only — safe to include

$pass = 0; $fail = 0;
function ok(string $label, bool $cond): void {
    global $pass, $fail;
    if ($cond) { $pass++; echo "PASS: $label\n"; }
    else       { $fail++; echo "FAIL: $label\n"; }
}

// ---- Pure unit tests: isLoopback / parseSinceParam --------------------------
ok('isLoopback 127.0.0.1',            isLoopback('127.0.0.1') === true);
ok('isLoopback ::1',                  isLoopback('::1') === true);
ok('isLoopback rejects mapped v4',    isLoopback('::ffff:127.0.0.1') === false);
ok('isLoopback rejects LAN',          isLoopback('192.168.0.100') === false);
ok('isLoopback rejects empty',        isLoopback('') === false);
ok('parseSince empty -> 0',           parseSinceParam('') === 0);
ok('parseSince absent(null) -> 0',    parseSinceParam(null) === 0);
ok('parseSince "5" -> 5',             parseSinceParam('5') === 5);
ok('parseSince "12abc" -> 0',         parseSinceParam('12abc') === 0);
ok('parseSince array -> 0',           parseSinceParam(['5']) === 0);
ok('parseSince "-1" -> 0',            parseSinceParam('-1') === 0);
ok('parseSince overflow -> PHP_INT_MAX',
    parseSinceParam(str_repeat('9', 30)) === PHP_INT_MAX);
```

- [ ] **Step 2: Run it to verify it fails**

Run: `php deliverables/loams_api/tests/access_display_test.php`
Expected: FAIL — fatal `require_once` error, `access_helpers.php` does not exist.

- [ ] **Step 3: Implement `access_helpers.php`**

Create `deliverables/loams_api/access_helpers.php`:

```php
<?php
/**
 * Shared helpers for the LOAMS 2.0 Access Control READ endpoints
 * (turnstile_display.php, access_recent.php). Read-only — no attendance writes.
 *
 * SECURITY DEBT (parallel TLS/RBAC track is the go-live gate): these endpoints
 * expose student PII over plaintext HTTP. turnstile_display.php rests on the
 * loopback guard here; access_recent.php on admin_key. Not for a real gate yet.
 */
declare(strict_types=1);

/**
 * True only for the exact IPv4/IPv6 loopback literals. The IPv4-mapped form
 * ::ffff:127.0.0.1 is deliberately rejected (fail-closed), matching
 * turnstile_pull.php. Consults only the passed REMOTE_ADDR — never a
 * client-supplied forwarding header (X-Forwarded-For / Forwarded).
 */
function isLoopback(?string $remoteIp): bool {
    return $remoteIp === '127.0.0.1' || $remoteIp === '::1';
}

/**
 * Parse the ?since cursor. Accepts ONLY a scalar ASCII-decimal string; anything
 * else (absent, array, signed, "12abc") yields 0 — never (int)-coerced, which
 * would turn "12abc" into 12 and warn on an array. A digit string above
 * PHP_INT_MAX is clamped to PHP_INT_MAX (ids are BIGINT UNSIGNED) so the mysqli
 * 'i' bind cannot overflow; the query then returns no newer row, which is
 * correct since no real id can exceed the clamp in practice.
 */
function parseSinceParam($raw): int {
    if (!is_string($raw) || $raw === '' || !ctype_digit($raw)) {
        return 0;
    }
    $maxStr = (string) PHP_INT_MAX;
    if (strlen($raw) > strlen($maxStr)
        || (strlen($raw) === strlen($maxStr) && strcmp($raw, $maxStr) > 0)) {
        return PHP_INT_MAX;
    }
    return (int) $raw;
}

/**
 * Resolve a card to a student row: students.code first, then students.school_id
 * fallback (the same resolution turnstile.php and rfid_login.php perform).
 * Returns the associative row, or null when the card resolves to no student.
 */
function resolveStudentByCard(mysqli $conn, string $card): ?array {
    $stmt = $conn->prepare('SELECT * FROM students WHERE code = ? LIMIT 1');
    $stmt->bind_param('s', $card);
    $stmt->execute();
    $row = $stmt->get_result()->fetch_assoc();
    $stmt->close();
    if ($row) {
        return $row;
    }
    $stmt = $conn->prepare('SELECT * FROM students WHERE school_id = ? LIMIT 1');
    $stmt->bind_param('s', $card);
    $stmt->execute();
    $row = $stmt->get_result()->fetch_assoc();
    $stmt->close();
    return $row ?: null;
}

/**
 * Normalize a student's stored photo to a CONTAINED relative path under uploads/.
 * Returns 'uploads/default.jpg' unless the stored value is a real file that
 * resolves strictly inside the canonical uploads directory. Rejects empty,
 * absolute, "..", backslash/NUL, and sibling-prefix (uploads-evil/...) values.
 * Returns a RELATIVE path — never an absolute URL — so the backend never trusts
 * HTTP_HOST; the client composes the full URL from its configured base.
 */
function normalizeStudentPhotoPath(?array $student): string {
    $default = 'uploads/default.jpg';
    $photo = is_array($student) ? (string) ($student['photo'] ?? '') : '';
    if ($photo === '') {
        return $default;
    }
    // Lexical rejects before touching the filesystem.
    if (strpbrk($photo, "\\\0") !== false          // backslash or NUL
        || str_contains($photo, '..')              // parent traversal
        || $photo[0] === '/'                        // absolute (unix)
        || preg_match('#^[A-Za-z]:#', $photo) === 1 // absolute (windows drive)
    ) {
        return $default;
    }
    $uploadsBase = realpath(__DIR__ . '/uploads');
    $resolved    = realpath(__DIR__ . '/' . $photo);
    if ($uploadsBase === false || $resolved === false) {
        return $default;
    }
    // Separator-delimited containment: the trailing separator stops a sibling
    // like uploads-evil/ from satisfying a raw "uploads" prefix.
    $prefix = $uploadsBase . DIRECTORY_SEPARATOR;
    if (strncmp($resolved, $prefix, strlen($prefix)) !== 0) {
        return $default;
    }
    return $photo;
}
```

- [ ] **Step 4: Run the pure tests to verify they pass**

Run: `php deliverables/loams_api/tests/access_display_test.php`
Expected: the 12 pure-helper assertions PASS (harness prints `12 passed, 0 failed` — the HTTP section is added below).

- [ ] **Step 5: Write the failing HTTP integration tests for `turnstile_display.php`**

Append the throwaway-DB bootstrap, `php -S` launch, and display assertions to the same file (before the final tally). This mirrors `turnstile_integration_test.php` but with a unique DB name, an ephemeral port, and a synthetic `uploads/` for the photo-containment case.

```php
// ---- HTTP + DB harness ------------------------------------------------------
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$TEST_DB = 'wits_acc_it_' . getmypid() . '_' . bin2hex(random_bytes(3));

function freePort(): int {
    $s = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
    if ($s === false) { fwrite(STDERR, "FATAL: no free port\n"); exit(2); }
    $name = stream_socket_get_name($s, false);   // 127.0.0.1:PORT
    fclose($s);
    return (int) substr($name, strrpos($name, ':') + 1);
}
$PORT = freePort();
$HOSTPORT = '127.0.0.1:' . $PORT;

function httpGet(string $hostport, string $path): array {
    $ch = curl_init('http://' . $hostport . $path);
    curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    return ['code' => (int) $code, 'json' => json_decode((string) $body, true)];
}
function httpPostForm(string $hostport, string $path, array $form): array {
    $ch = curl_init('http://' . $hostport . $path);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5,
        CURLOPT_POST => true, CURLOPT_POSTFIELDS => http_build_query($form),
    ]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    return ['code' => (int) $code, 'json' => json_decode((string) $body, true)];
}
// A raw GET that overrides the HTTP method (for the 405 test).
function httpMethod(string $hostport, string $method, string $path): array {
    $ch = curl_init('http://' . $hostport . $path);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5,
        CURLOPT_CUSTOMREQUEST => $method,
    ]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    return ['code' => (int) $code, 'json' => json_decode((string) $body, true)];
}

// Throwaway schema + synthetic rows.
$root = new mysqli('localhost', 'root', '');
$root->query('DROP DATABASE IF EXISTS ' . $TEST_DB);
$root->query('CREATE DATABASE ' . $TEST_DB);
$root->select_db($TEST_DB);
$root->query('CREATE TABLE students (
    school_id VARCHAR(64) PRIMARY KEY, code VARCHAR(64), course VARCHAR(64),
    year_level VARCHAR(32), department VARCHAR(64), name VARCHAR(128),
    gender VARCHAR(16), status VARCHAR(32), photo VARCHAR(255), visits INT DEFAULT 0)');
$root->query('CREATE TABLE library_visits (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, student_id VARCHAR(64), course VARCHAR(64),
    login_time DATETIME, yearlog INT, year_level VARCHAR(32), department VARCHAR(64),
    name VARCHAR(128), gender VARCHAR(16), status VARCHAR(32), photo VARCHAR(255))');
$root->query(file_get_contents($apiDir . '/sql/turnstile_events.sql'));

// Students: one resolvable by code, one by school_id, one with a hostile photo.
$root->query("INSERT INTO students (school_id, code, name, course, department, year_level, gender, status, photo, visits)
    VALUES ('SID_A','CARD_A','Alpha Test','BSCS','CIC','1','M','regular','uploads/a.jpg',0)");
$root->query("INSERT INTO students (school_id, code, name, photo, visits)
    VALUES ('SID_B','', 'Beta Test', 'uploads/../config.php', 0)"); // hostile path
// Note SID_B has empty code, so CARD 'SID_B' resolves via school_id fallback.

// turnstile_events: explicit ids so cursor order is deterministic. reader=0.
$root->query("INSERT INTO turnstile_events (id, card, reader, created_at) VALUES
    (101,'CARD_A',0,NOW()), (102,'SID_B',0,NOW()), (103,'CARD_GONE',0,NOW())");
// 103's card intentionally resolves to no student (orphaned-row case).

// Temp docroot with the real endpoints + helper + a test config pointing at $TEST_DB.
$docroot = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_acc_' . getmypid() . '_' . $PORT;
@mkdir($docroot, 0777, true);
@mkdir($docroot . '/uploads', 0777, true);
file_put_contents($docroot . '/uploads/a.jpg', 'JPEGDATA');        // real photo
file_put_contents($docroot . '/uploads/default.jpg', 'DEFAULT');   // fallback
file_put_contents($docroot . '/config.php',
    "<?php define('DB_HOST','localhost');define('DB_USER','root');define('DB_PASS','');define('DB_NAME','" . $TEST_DB . "');\n");
foreach (['access_helpers.php','turnstile_display.php'] as $f) {
    copy($apiDir . '/' . $f, $docroot . '/' . $f);
}

$descr = [0 => ['pipe','r'], 1 => ['file', $docroot . '/server.log', 'a'], 2 => ['file', $docroot . '/server.log', 'a']];
$proc = proc_open([PHP_BINARY, '-S', $HOSTPORT, '-t', $docroot], $descr, $pipes);
usleep(700000); // let it bind

try {
    // 1. Method enforcement: non-GET -> 405.
    $r = httpMethod($HOSTPORT, 'POST', '/turnstile_display.php');
    ok('display: non-GET -> 405', $r['code'] === 405);

    // 2. since walk returns OLDEST-next one at a time; latest_id stays at MAX.
    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=100');
    ok('display: since=100 -> entry 101 (oldest next)', ($r['json']['entry']['id'] ?? null) === 101);
    ok('display: latest_id = 103 (MAX)',                ($r['json']['latest_id'] ?? null) === 103);
    ok('display: 101 resolves by code',                 ($r['json']['entry']['student']['school_id'] ?? null) === 'SID_A');
    ok('display: 101 photo_path relative real file',    ($r['json']['entry']['student']['photo_path'] ?? null) === 'uploads/a.jpg');

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=101');
    ok('display: since=101 -> entry 102 (school_id fallback)', ($r['json']['entry']['student']['school_id'] ?? null) === 'SID_B');
    ok('display: hostile photo -> default.jpg',                ($r['json']['entry']['student']['photo_path'] ?? null) === 'uploads/default.jpg');

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=102');
    ok('display: since=102 -> entry 103 present, student null (orphaned)',
        ($r['json']['entry']['id'] ?? null) === 103 && ($r['json']['entry']['student'] ?? 'x') === null);

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=103');
    ok('display: since=103 -> entry null (nothing newer)', ($r['json']['entry'] ?? 'x') === null);
    ok('display: latest_id still 103 when entry null',     ($r['json']['latest_id'] ?? null) === 103);

    // 3. No mutation: turnstile_events / library_visits / visits unchanged.
    $evtCount = (int) $root->query('SELECT COUNT(*) c FROM turnstile_events')->fetch_assoc()['c'];
    $visCount = (int) $root->query('SELECT COUNT(*) c FROM library_visits')->fetch_assoc()['c'];
    $visitsA  = (int) $root->query("SELECT visits FROM students WHERE school_id='SID_A'")->fetch_assoc()['visits'];
    ok('display: no rows mutated', $evtCount === 3 && $visCount === 0 && $visitsA === 0);

    // 4. Ordinary next-poll catch-up: a row inserted after draining is returned next.
    $root->query("INSERT INTO turnstile_events (id, card, reader, created_at) VALUES (104,'CARD_A',0,NOW())");
    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=103');
    ok('display: next-poll catch-up returns new 104', ($r['json']['entry']['id'] ?? null) === 104);
} finally {
    if (is_resource($proc)) { proc_terminate($proc); proc_close($proc); }
    $root->query('DROP DATABASE ' . $TEST_DB);
    $root->close();
    array_map('unlink', glob($docroot . '/uploads/*') ?: []);
    @rmdir($docroot . '/uploads');
    array_map('unlink', glob($docroot . '/*') ?: []);
    @rmdir($docroot);
}

echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
```

- [ ] **Step 6: Run to verify the HTTP tests fail**

Run: `php deliverables/loams_api/tests/access_display_test.php`
Expected: FAIL — `turnstile_display.php` does not exist, so `copy()` fails / requests 404 and the display assertions fail.

- [ ] **Step 7: Implement `turnstile_display.php`**

Create `deliverables/loams_api/turnstile_display.php`:

```php
<?php
/**
 * turnstile_display.php — LOAMS 2.0 native gate-display feed.
 * GET, LOOPBACK-ONLY, READ-ONLY. Returns the OLDEST turnstile_events row with
 * id > ?since, resolved to a student, for the desktop TurnstileProvider. Never
 * writes attendance — turnstile.php stays the sole authoritative writer.
 *
 * SECURITY DEBT (TLS/RBAC is the parallel go-live gate): entry PII over
 * plaintext HTTP, defended only by the loopback guard below. MUST be served
 * directly by the local PHP/Apache instance — never behind a reverse proxy that
 * forwards remote traffic (PHP would then see the proxy as 127.0.0.1). The guard
 * consults ONLY REMOTE_ADDR, never X-Forwarded-For / Forwarded.
 */
declare(strict_types=1);
header('Content-Type: application/json');
header('Cache-Control: no-store');

require_once __DIR__ . '/access_helpers.php';

function displayError(int $code, string $msg): void {
    http_response_code($code);
    echo json_encode(['status' => 'error', 'message' => $msg]);
    exit;
}

if (($_SERVER['REQUEST_METHOD'] ?? 'GET') !== 'GET') {
    displayError(405, 'Method not allowed');
}
if (!isLoopback($_SERVER['REMOTE_ADDR'] ?? '')) {
    displayError(403, 'Forbidden');
}

$since = parseSinceParam($_GET['since'] ?? null);

try {
    require_once __DIR__ . '/config.php';
    ini_set('display_errors', '0');
    mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
    $conn = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
    $conn->set_charset('utf8mb4');

    // latest_id = MAX(id): informational backlog metadata only. NULL on empty
    // table -> 0. Only committed rows are visible, so this never counts an
    // in-flight insert from another transaction.
    $latestId = (int) ($conn->query('SELECT MAX(id) AS m FROM turnstile_events')
                            ->fetch_assoc()['m'] ?? 0);

    $stmt = $conn->prepare(
        'SELECT id, card, reader, created_at FROM turnstile_events
         WHERE id > ? ORDER BY id ASC LIMIT 1'
    );
    $stmt->bind_param('i', $since);
    $stmt->execute();
    $row = $stmt->get_result()->fetch_assoc();
    $stmt->close();

    $entry = null;
    if ($row) {
        $student = resolveStudentByCard($conn, (string) $row['card']);
        $entry = [
            'id'         => (int) $row['id'],
            'card'       => (string) $row['card'],
            'created_at' => (string) $row['created_at'],
            'reader'     => (int) $row['reader'],
            'student'    => $student === null ? null : [
                'name'       => (string) ($student['name'] ?? ''),
                'school_id'  => (string) ($student['school_id'] ?? ''),
                'course'     => (string) ($student['course'] ?? ''),
                'department' => (string) ($student['department'] ?? ''),
                'year_level' => (string) ($student['year_level'] ?? ''),
                'gender'     => (string) ($student['gender'] ?? ''),
                'status'     => (string) ($student['status'] ?? ''),
                'photo_path' => normalizeStudentPhotoPath($student),
            ],
        ];
    }
    $conn->close();

    $payload = json_encode(['status' => 'success', 'latest_id' => $latestId, 'entry' => $entry]);
    if ($payload === false) {
        displayError(500, 'Internal server error');
    }
    echo $payload;
} catch (Throwable $e) {
    error_log('turnstile_display error: ' . get_class($e) . ' code ' . $e->getCode());
    displayError(500, 'Internal server error');
}
```

- [ ] **Step 8: Run to verify all Task 1 tests pass**

Run: `php deliverables/loams_api/tests/access_display_test.php`
Expected: PASS — all pure + display assertions green (`… passed, 0 failed`).
Also run: `php -l deliverables/loams_api/access_helpers.php && php -l deliverables/loams_api/turnstile_display.php` → "No syntax errors".

- [ ] **Step 9: Commit** (via the `commit` skill)

Concern: "feat(accesscontrol): turnstile_display endpoint + shared access_helpers". Stage exactly `deliverables/loams_api/access_helpers.php`, `deliverables/loams_api/turnstile_display.php`, `deliverables/loams_api/tests/access_display_test.php`. No attribution trailers.

---

### Task 2: `access_recent.php` + harness auth extension

**Files:**
- Create: `deliverables/loams_api/access_recent.php`
- Modify: `deliverables/loams_api/tests/access_display_test.php` (add `admin` table + ephemeral secret + copy `access_recent.php`/`auth_helper.php` into the docroot; add the recent-endpoint assertions before the final tally)

**Interfaces:**
- Consumes: `resolveStudentByCard()`, `normalizeStudentPhotoPath()` from `access_helpers.php` (Task 1); `requireAdminAuth(mysqli $conn)` from the existing `auth_helper.php` (401 + exit on failure).
- Produces: `access_recent.php` HTTP contract: `POST` (admin_key in body) → `{"status":"success","entries":[…≤50],"entries_today":N,"last_entry_at":"…"|null}`.

- [ ] **Step 1: Extend the harness — admin table, secret, and copies**

In `tests/access_display_test.php`, in the throwaway-schema setup block (right after the `turnstile_events` load), add the `admin` table + a runtime-generated ephemeral secret, and copy the new endpoint + `auth_helper.php` into the docroot. Insert this after the `students`/events seeding and extend the `foreach` copy list:

```php
// Admin auth: ephemeral secret, never committed. auth_helper reads
// SELECT admin_key_hash FROM admin LIMIT 1.
$root->query('CREATE TABLE admin (admin_key_hash VARCHAR(255))');
$ADMIN_KEY = bin2hex(random_bytes(16));                       // ephemeral, per-run
$adminHash = password_hash($ADMIN_KEY, PASSWORD_DEFAULT);
$st = $root->prepare('INSERT INTO admin (admin_key_hash) VALUES (?)');
$st->bind_param('s', $adminHash); $st->execute(); $st->close();
```

Extend the docroot copy list to include the new files:

```php
foreach (['access_helpers.php','turnstile_display.php','access_recent.php','auth_helper.php'] as $f) {
    copy($apiDir . '/' . $f, $docroot . '/' . $f);
}
```

- [ ] **Step 2: Write the failing tests for `access_recent.php`**

Add these assertions inside the `try { … }` block (after the display assertions, before the `finally`). `$ADMIN_KEY` is in scope.

```php
    // access_recent: method BEFORE auth — a GET carrying a valid admin_key body is still 405.
    $r = httpMethod($HOSTPORT, 'GET', '/access_recent.php');
    ok('recent: non-POST -> 405 before auth', $r['code'] === 405);

    // Missing / wrong key -> 401.
    $r = httpPostForm($HOSTPORT, '/access_recent.php', []);
    ok('recent: missing admin_key -> 401', $r['code'] === 401);
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => 'wrong']);
    ok('recent: wrong admin_key -> 401', $r['code'] === 401);

    // Valid key -> newest-first entries, slim projection, orphaned null.
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => $ADMIN_KEY]);
    $entries = $r['json']['entries'] ?? [];
    ok('recent: valid key -> success', ($r['json']['status'] ?? null) === 'success');
    ok('recent: newest first (id DESC)', ($entries[0]['id'] ?? null) === 104);
    ok('recent: slim projection has no year_level',
        isset($entries[1]) && !array_key_exists('year_level', (array) ($entries[1]['student'] ?? [])));
    // The orphaned row (103, CARD_GONE) is present with student null.
    $has103Null = false;
    foreach ($entries as $e) { if (($e['id'] ?? null) === 103) { $has103Null = ($e['student'] ?? 'x') === null; } }
    ok('recent: orphaned row present with student null', $has103Null);

    // Counts: entries_today across the midnight boundary via the DB clock; last_entry_at = MAX.
    $root->query("INSERT INTO turnstile_events (id, card, reader, created_at)
                  VALUES (200,'CARD_A',0, CURDATE() - INTERVAL 1 SECOND)"); // yesterday 23:59:59
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => $ADMIN_KEY]);
    $todayCount = (int) $root->query("SELECT COUNT(*) c FROM turnstile_events
        WHERE created_at >= CURDATE() AND created_at < CURDATE() + INTERVAL 1 DAY")->fetch_assoc()['c'];
    ok('recent: entries_today excludes yesterday-boundary row',
        ($r['json']['entries_today'] ?? null) === $todayCount);
    ok('recent: last_entry_at is a non-empty string',
        is_string($r['json']['last_entry_at'] ?? null) && ($r['json']['last_entry_at'] !== ''));
```

- [ ] **Step 3: Run to verify they fail**

Run: `php deliverables/loams_api/tests/access_display_test.php`
Expected: FAIL — `access_recent.php` does not exist (`copy()` warns / requests 404), recent assertions fail.

- [ ] **Step 4: Implement `access_recent.php`**

Create `deliverables/loams_api/access_recent.php`:

```php
<?php
/**
 * access_recent.php — LOAMS 2.0 admin Access Control feed.
 * POST only (405 otherwise, checked BEFORE auth so a non-POST body cannot
 * authenticate). admin_key-guarded via auth_helper. READ-ONLY: newest 50
 * turnstile_events resolved to students + gate-specific counts.
 *
 * SECURITY DEBT (TLS/RBAC is the parallel go-live gate): entry PII over
 * plaintext HTTP, defended by admin_key. Not for a real gate yet.
 */
declare(strict_types=1);
header('Content-Type: application/json');
header('Cache-Control: no-store');

function recentError(int $code, string $msg): void {
    http_response_code($code);
    echo json_encode(['status' => 'error', 'message' => $msg]);
    exit;
}

// Method BEFORE auth: extractAdminKey() reads the body on any method, so a
// non-POST request carrying an admin_key must be rejected here first.
if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    recentError(405, 'Method not allowed');
}

require_once __DIR__ . '/access_helpers.php';

try {
    require_once __DIR__ . '/config.php';
    ini_set('display_errors', '0');
    mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
    $conn = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
    $conn->set_charset('utf8mb4');

    require_once __DIR__ . '/auth_helper.php';
    requireAdminAuth($conn); // sends 401 + exit on failure

    $entries = [];
    $res = $conn->query(
        'SELECT id, card, reader, created_at FROM turnstile_events ORDER BY id DESC LIMIT 50'
    );
    while ($row = $res->fetch_assoc()) {
        $student = resolveStudentByCard($conn, (string) $row['card']);
        $entries[] = [
            'id'         => (int) $row['id'],
            'card'       => (string) $row['card'],
            'created_at' => (string) $row['created_at'],
            'reader'     => (int) $row['reader'],
            'student'    => $student === null ? null : [
                'name'       => (string) ($student['name'] ?? ''),
                'school_id'  => (string) ($student['school_id'] ?? ''),
                'course'     => (string) ($student['course'] ?? ''),
                'department' => (string) ($student['department'] ?? ''),
                'photo_path' => normalizeStudentPhotoPath($student),
            ],
        ];
    }

    // Half-open range (sargable), server-tz "today"; matches the writer's NOW().
    $today = (int) ($conn->query(
        'SELECT COUNT(*) AS c FROM turnstile_events
         WHERE created_at >= CURDATE() AND created_at < CURDATE() + INTERVAL 1 DAY'
    )->fetch_assoc()['c'] ?? 0);

    $lastEntryAt = $conn->query('SELECT MAX(created_at) AS m FROM turnstile_events')
                        ->fetch_assoc()['m'] ?? null;
    $conn->close();

    $payload = json_encode([
        'status'        => 'success',
        'entries'       => $entries,
        'entries_today' => $today,
        'last_entry_at' => $lastEntryAt,
    ]);
    if ($payload === false) {
        recentError(500, 'Internal server error');
    }
    echo $payload;
} catch (Throwable $e) {
    error_log('access_recent error: ' . get_class($e) . ' code ' . $e->getCode());
    recentError(500, 'Internal server error');
}
```

- [ ] **Step 5: Run to verify all tests pass**

Run: `php deliverables/loams_api/tests/access_display_test.php`
Expected: PASS — all pure + display + recent assertions green (`… passed, 0 failed`).
Also: `php -l deliverables/loams_api/access_recent.php` → "No syntax errors".

- [ ] **Step 6: Regression — legacy harness still green**

Run: `php deliverables/loams_api/tests/turnstile_integration_test.php`
Expected: `5 passed, 0 failed` (these endpoints touch none of its write paths).

- [ ] **Step 7: Commit** (via the `commit` skill)

Concern: "feat(accesscontrol): access_recent admin endpoint + harness auth". Stage exactly `deliverables/loams_api/access_recent.php` and `deliverables/loams_api/tests/access_display_test.php`. No attribution trailers.

---

## Self-Review

**1. Spec coverage** (against `2026-09-28-…-design.md`):
- `turnstile_display.php` GET/loopback/405/403, oldest-next cursor, `latest_id`=MAX, enumerated display projection, `student:null` orphaned fallback, read-only → Task 1 Steps 5–8. ✅
- `access_recent.php` POST-only-before-auth, admin_key, newest-50 id DESC, slim projection, `entries_today` half-open range, `last_entry_at` → Task 2. ✅
- `access_helpers.php` with `resolveStudentByCard` + contained `normalizeStudentPhotoPath` + `isLoopback` + `parseSinceParam` → Task 1 Step 3. ✅
- config.php connection (not db.php) + utf8mb4 + display_errors off + generic error writer + Cache-Control: no-store + class/code-only logging + json_encode check → both endpoints. ✅
- Test harness: unique schema + ephemeral port + runtime ephemeral admin secret + DB-clock boundaries + synthetic data + teardown; method-405, DB-failure shape (a forced failure returns generic 500 — covered implicitly by the `try/catch` + json_encode-check paths; the connection uses config.php so it cannot inherit db.php's HTTP-200 die), midnight boundary → Tasks 1–2. ✅
- `rfid_login.php`/`turnstile_pull.php`/`turnstile.php` untouched; legacy harness regression → Task 2 Step 6. ✅
- Security debt comments in both files + (carried to the PR) → file headers. ✅

**2. Placeholder scan:** every code step contains complete, runnable PHP; every run step names the exact command and expected output. No TBDs. ✅

**3. Type consistency:** helper signatures in the Task 1 Interfaces block match their definitions and both endpoints' call sites (`resolveStudentByCard(mysqli,string):?array`, `normalizeStudentPhotoPath(?array):string`, `isLoopback(?string):bool`, `parseSinceParam($raw):int`). Response keys (`status/latest_id/entry`, `status/entries/entries_today/last_entry_at`) match the spec and the test assertions. ✅

**Note for the implementer:** the DB-failure-shape assertion in the spec is satisfied structurally (config.php connection + try/catch + generic 500), not by a dedicated forced-failure test — forcing a mid-request connection failure under `php -S` is not reliably reproducible in this harness. If a cheap deterministic trigger is available (e.g. pointing the copied `config.php` at a non-existent DB for one isolated sub-run), add it; otherwise the structural guarantee stands and the `db.php`-avoidance is the actual fix.
