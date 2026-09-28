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
ok('parseSince leading zeros "000..05" -> 5',
    parseSinceParam(str_repeat('0', 28) . '5') === 5);
ok('parseSince "0" -> 0',             parseSinceParam('0') === 0);

// Final tally + exit so THIS file is a real red/green gate on its own. The HTTP
// harness block (Step 5) is inserted immediately BEFORE these two lines.
echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
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
    // Strip leading zeroes BEFORE the length/value comparison, else a value like
    // "000...05" (30 chars) would be misjudged as > PHP_INT_MAX and clamped.
    $digits = ltrim($raw, '0');
    if ($digits === '') {
        return 0; // the string was all zeroes
    }
    $maxStr = (string) PHP_INT_MAX;
    if (strlen($digits) > strlen($maxStr)
        || (strlen($digits) === strlen($maxStr) && strcmp($digits, $maxStr) > 0)) {
        return PHP_INT_MAX;
    }
    return (int) $digits;
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
Expected: the 14 pure-helper assertions PASS (harness prints `14 passed, 0 failed`). The HTTP section is inserted in Step 5 *before* the tally added here, so this file is already a real pass/fail gate.

- [ ] **Step 5: Write the failing HTTP integration tests for `turnstile_display.php`**

Insert the throwaway-DB bootstrap, `php -S` launch, and display assertions into the same file **immediately before the final `echo`/`exit` tally added in Step 1** (so the file stays a single runnable gate). This mirrors `turnstile_integration_test.php` but with a unique DB name, an ephemeral port + readiness probe, and a synthetic `uploads/` (+ a sibling `uploads-evil/`) for the photo-containment cases.

> **Endpoint-level 403 note:** a non-loopback request cannot reach a loopback-bound `php -S` directly, so the 403 branch is exercised through a test-only `__force_remote.php` wrapper (written into the docroot below) that spoofs a non-loopback `REMOTE_ADDR` and then `require`s the real endpoint. The `isLoopback()` pure unit tests (`192.168.0.100`/`::ffff:127.0.0.1`/`''` → false) back it up.

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
// curl helpers: return decoded json AND the raw body (for no-leak checks). An
// optional method override + body drives the 405 and GET-with-body cases.
function httpReq(string $hostport, string $path, array $opt = []): array {
    $ch = curl_init('http://' . $hostport . $path);
    $base = [CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5];
    curl_setopt_array($ch, $base + $opt);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    return ['code' => (int) $code, 'raw' => (string) $body, 'json' => json_decode((string) $body, true)];
}
function httpGet(string $hp, string $p): array { return httpReq($hp, $p); }
function httpPostForm(string $hp, string $p, array $form): array {
    return httpReq($hp, $p, [CURLOPT_POST => true, CURLOPT_POSTFIELDS => http_build_query($form)]);
}
// Method override, optionally carrying a JSON body. JSON (not urlencoded) because
// extractAdminKey() reads $_POST or a JSON php://input body — PHP does not populate
// $_POST for a GET, so a urlencoded GET body would look like a MISSING key and the
// test would not actually carry a valid key. This proves method-before-auth.
function httpMethod(string $hp, string $method, string $p, array $jsonBody = []): array {
    $opt = [CURLOPT_CUSTOMREQUEST => $method];
    if ($jsonBody) {
        $opt[CURLOPT_POSTFIELDS] = json_encode($jsonBody);
        $opt[CURLOPT_HTTPHEADER] = ['Content-Type: application/json'];
    }
    return httpReq($hp, $p, $opt);
}
// A plain ($x['k'] ?? 'x') === null is ALWAYS false (?? treats null as absent), so
// null assertions must check key-existence + identity explicitly.
function isJsonNull($arr, string $key): bool {
    return is_array($arr) && array_key_exists($key, $arr) && $arr[$key] === null;
}

// Allocate the port FIRST — before anything that needs cleanup — so a freePort()
// failure exits with nothing to tear down. (A concurrent run could still lose the
// freed port to another process; the readiness probe + first-request assertion turn
// that into a visible test failure rather than a hang, which is acceptable here.)
$PORT = freePort();
$HOSTPORT = '127.0.0.1:' . $PORT;

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

// Students covering every resolution + photo case.
$root->query("INSERT INTO students (school_id, code, name, course, department, year_level, gender, status, photo, visits)
    VALUES ('SID_A','CARD_A','Alpha Test','BSCS','CIC','1','M','regular','uploads/a.jpg',0)"); // real photo, code hit
$root->query("INSERT INTO students (school_id, code, name, photo, visits)
    VALUES ('SID_B','', 'Beta Test', 'uploads/../config.php', 0)");   // hostile traversal; empty code -> school_id fallback
$root->query("INSERT INTO students (school_id, code, name, photo, visits)
    VALUES ('SID_C','CARD_C','Gamma Test', 'uploads/missing.jpg', 0)"); // absent file
$root->query("INSERT INTO students (school_id, code, name, photo, visits)
    VALUES ('SID_D','CARD_D','Delta Test', 'uploads-evil/x.jpg', 0)");  // sibling-prefix outside uploads/

// turnstile_events: explicit ids so cursor order is deterministic. reader=0.
// 103's card resolves to no student (orphaned-row case).
$root->query("INSERT INTO turnstile_events (id, card, reader, created_at) VALUES
    (101,'CARD_A',0,NOW()), (102,'SID_B',0,NOW()), (103,'CARD_GONE',0,NOW()),
    (105,'CARD_C',0,NOW()), (106,'CARD_D',0,NOW())");

// Temp docroot with the real endpoints + helper + a test config pointing at $TEST_DB.
$docroot = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_acc_' . getmypid() . '_' . $PORT;
@mkdir($docroot . '/uploads', 0777, true);
@mkdir($docroot . '/uploads-evil', 0777, true);
file_put_contents($docroot . '/uploads/a.jpg', 'JPEGDATA');        // real photo
file_put_contents($docroot . '/uploads/default.jpg', 'DEFAULT');   // fallback
file_put_contents($docroot . '/uploads-evil/x.jpg', 'EVIL');       // real file, but OUTSIDE uploads/
file_put_contents($docroot . '/config.php',
    "<?php define('DB_HOST','localhost');define('DB_USER','root');define('DB_PASS','');define('DB_NAME','" . $TEST_DB . "');\n");
// Test-only wrapper: spoof a non-loopback REMOTE_ADDR, then run the REAL endpoint,
// so its 403 branch is exercised end to end (a loopback-bound php -S cannot
// otherwise deliver a non-loopback request). Not shipped — lives only in the docroot.
file_put_contents($docroot . '/__force_remote.php',
    "<?php \$_SERVER['REMOTE_ADDR']='203.0.113.9'; require __DIR__ . '/turnstile_display.php';\n");
foreach (['access_helpers.php','turnstile_display.php'] as $f) {
    copy($apiDir . '/' . $f, $docroot . '/' . $f);
}

$proc = null;
$cleanup = function () use ($root, $TEST_DB, $docroot, &$proc) {
    if (is_resource($proc)) { proc_terminate($proc); proc_close($proc); }
    if ($root instanceof mysqli) { @$root->query('DROP DATABASE IF EXISTS ' . $TEST_DB); }
    foreach (['uploads','uploads-evil'] as $d) {
        array_map('unlink', glob($docroot . '/' . $d . '/*') ?: []);
        @rmdir($docroot . '/' . $d);
    }
    array_map('unlink', glob($docroot . '/*') ?: []);
    @rmdir($docroot);
};

$descr = [0 => ['pipe','r'], 1 => ['file', $docroot . '/server.log', 'a'], 2 => ['file', $docroot . '/server.log', 'a']];
$proc = proc_open([PHP_BINARY, '-S', $HOSTPORT, '-t', $docroot], $descr, $pipes);
if (!is_resource($proc)) { $cleanup(); fwrite(STDERR, "FATAL: proc_open failed\n"); exit(2); }
// Bounded readiness probe instead of a fixed sleep. (A freed-port bind race is
// still theoretically possible; the probe just confirms *something* is listening,
// and the first real request below returns our JSON, so a wrong process surfaces
// as a test failure rather than a hang.)
$ready = false;
for ($i = 0; $i < 50; $i++) { // up to ~5s
    $c = @stream_socket_client('tcp://' . $HOSTPORT, $en, $es, 0.1);
    if ($c) { fclose($c); $ready = true; break; }
    usleep(100000);
}
if (!$ready) { $cleanup(); fwrite(STDERR, "FATAL: php -S never came up on $HOSTPORT\n"); exit(2); }

try {
    // 0. Endpoint-level 403: the REAL endpoint via the wrapper that spoofs a
    // non-loopback REMOTE_ADDR. Proves the endpoint wires isLoopback() and 403s
    // (with generic JSON) before any DB access.
    $r = httpGet($HOSTPORT, '/__force_remote.php');
    ok('display: non-loopback REMOTE_ADDR -> 403', $r['code'] === 403);
    ok('display: 403 -> generic error json',       ($r['json']['status'] ?? null) === 'error');

    // 1. Method enforcement: non-GET -> 405.
    ok('display: non-GET -> 405', httpMethod($HOSTPORT, 'POST', '/turnstile_display.php')['code'] === 405);

    // 2. since walk returns OLDEST-next one at a time; latest_id stays at MAX.
    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=100');
    ok('display: since=100 -> entry 101 (oldest next)', ($r['json']['entry']['id'] ?? null) === 101);
    ok('display: latest_id = 106 (MAX)',                ($r['json']['latest_id'] ?? null) === 106);
    ok('display: 101 resolves by code',                 ($r['json']['entry']['student']['school_id'] ?? null) === 'SID_A');
    ok('display: 101 photo_path relative real file',    ($r['json']['entry']['student']['photo_path'] ?? null) === 'uploads/a.jpg');

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=101');
    ok('display: since=101 -> entry 102 (school_id fallback)', ($r['json']['entry']['student']['school_id'] ?? null) === 'SID_B');
    ok('display: hostile traversal photo -> default.jpg',      ($r['json']['entry']['student']['photo_path'] ?? null) === 'uploads/default.jpg');

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=102');
    ok('display: since=102 -> entry 103 present, student null (orphaned)',
        ($r['json']['entry']['id'] ?? null) === 103 && isJsonNull($r['json']['entry'] ?? null, 'student'));

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=103');
    ok('display: absent-file photo -> default.jpg', ($r['json']['entry']['student']['photo_path'] ?? null) === 'uploads/default.jpg');

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=105');
    ok('display: sibling uploads-evil/ photo -> default.jpg', ($r['json']['entry']['student']['photo_path'] ?? null) === 'uploads/default.jpg');

    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=106');
    ok('display: since=106 -> entry null (nothing newer)', isJsonNull($r['json'] ?? null, 'entry'));
    ok('display: latest_id still 106 when entry null',     ($r['json']['latest_id'] ?? null) === 106);

    // 3. No mutation: turnstile_events / library_visits / visits unchanged.
    $evtCount = (int) $root->query('SELECT COUNT(*) c FROM turnstile_events')->fetch_assoc()['c'];
    $visCount = (int) $root->query('SELECT COUNT(*) c FROM library_visits')->fetch_assoc()['c'];
    $visitsA  = (int) $root->query("SELECT visits FROM students WHERE school_id='SID_A'")->fetch_assoc()['visits'];
    ok('display: no rows mutated', $evtCount === 5 && $visCount === 0 && $visitsA === 0);

    // 4. Ordinary next-poll catch-up: a row inserted after draining is returned next.
    $root->query("INSERT INTO turnstile_events (id, card, reader, created_at) VALUES (107,'CARD_A',0,NOW())");
    ok('display: next-poll catch-up returns new 107',
        (httpGet($HOSTPORT, '/turnstile_display.php?since=106')['json']['entry']['id'] ?? null) === 107);

    // --- Task 2 (access_recent.php) assertions are inserted here in Task 2 ---

    // 5. Empty table -> latest_id 0, entry null. Run LAST of the read tests.
    $root->query('TRUNCATE TABLE turnstile_events');
    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=0');
    ok('display: empty table -> latest_id 0', ($r['json']['latest_id'] ?? 'x') === 0);
    ok('display: empty table -> entry null',  isJsonNull($r['json'] ?? null, 'entry'));

    // 6. Forced DB failure -> generic 500, no leaked SQL/DB detail. Point the
    // copied config at a nonexistent schema (php -S re-includes config per
    // request); this is destructive to the connection, so it runs last.
    file_put_contents($docroot . '/config.php',
        "<?php define('DB_HOST','localhost');define('DB_USER','root');define('DB_PASS','');define('DB_NAME','" . $TEST_DB . "_nope');\n");
    $r = httpGet($HOSTPORT, '/turnstile_display.php?since=0');
    ok('display: DB failure -> HTTP 500', $r['code'] === 500);
    ok('display: DB failure -> generic error json', ($r['json']['status'] ?? null) === 'error');
    ok('display: DB failure leaks no detail',
        stripos($r['raw'], 'mysqli') === false
        && stripos($r['raw'], 'Unknown database') === false
        && strpos($r['raw'], $TEST_DB) === false);
} finally {
    $cleanup();       // same teardown as the early-exit failure paths
    $root->close();
}
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
    $out = json_encode(['status' => 'error', 'message' => $msg]);
    echo $out !== false ? $out : '{"status":"error","message":"Internal server error"}';
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

    // JSON_THROW_ON_ERROR routes an encoding failure through the sanitized catch
    // below (logged as class+code, generic 500) instead of a silent false.
    echo json_encode(
        ['status' => 'success', 'latest_id' => $latestId, 'entry' => $entry],
        JSON_THROW_ON_ERROR
    );
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

In `tests/access_display_test.php`, add the `admin` table + a runtime-generated ephemeral secret **after the `turnstile_events` INSERTs and before the docroot/`freePort()`/`proc_open` section** (so the table exists before the server starts serving `requireAdminAuth` queries), and extend the docroot `foreach` copy list to include the new endpoint + `auth_helper.php`:

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

Replace the marker line `// --- Task 2 (access_recent.php) assertions are inserted here in Task 2 ---` (inside the `try { … }` block, after display test 4 and *before* display test 5's `TRUNCATE`) with these assertions. `$ADMIN_KEY` is in scope; at this point `turnstile_events` holds ids 101,102,103,105,106,107 (107 = CARD_A, newest). The recent-empty check does its own `TRUNCATE`, which display test 5 then repeats harmlessly.

```php
    // ---- access_recent.php ----
    // Method BEFORE auth: a GET carrying a VALID admin_key in a JSON body (which
    // extractAdminKey() WOULD accept) is still 405 — method is checked first.
    ok('recent: GET+valid-key JSON body -> 405 (method before auth)',
        httpMethod($HOSTPORT, 'GET', '/access_recent.php', ['admin_key' => $ADMIN_KEY])['code'] === 405);

    // Missing / wrong key on POST -> 401.
    ok('recent: missing admin_key -> 401', httpPostForm($HOSTPORT, '/access_recent.php', [])['code'] === 401);
    ok('recent: wrong admin_key -> 401',   httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => 'wrong'])['code'] === 401);

    // Valid key -> newest-first, resolved slim projection, orphaned null.
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => $ADMIN_KEY]);
    $entries = $r['json']['entries'] ?? [];
    ok('recent: valid key -> success',          ($r['json']['status'] ?? null) === 'success');
    ok('recent: newest first (id DESC) -> 107', ($entries[0]['id'] ?? null) === 107);
    // Slim projection asserted on a RESOLVED entry (107 = CARD_A): exact key set, no year_level.
    $st107 = $entries[0]['student'] ?? null;
    ok('recent: resolved slim projection keys',
        is_array($st107) && array_keys($st107) === ['name','school_id','course','department','photo_path']);
    // Orphaned row (103, CARD_GONE) present with student null.
    $has103Null = false;
    foreach ($entries as $e) { if (($e['id'] ?? null) === 103) { $has103Null = isJsonNull($e, 'student'); } }
    ok('recent: orphaned row present with student null', $has103Null);

    // 50-entry cap: bulk-insert past 50 (ids 300..359) and assert exactly 50, newest kept.
    for ($i = 300; $i < 360; $i++) {
        $root->query("INSERT INTO turnstile_events (id, card, reader, created_at) VALUES ($i,'CARD_A',0,NOW())");
    }
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => $ADMIN_KEY]);
    ok('recent: 50-entry cap',                 count($r['json']['entries'] ?? []) === 50);
    ok('recent: cap keeps newest (359 first)', ($r['json']['entries'][0]['id'] ?? null) === 359);

    // Counts: entries_today via the DB clock. Seed BOTH midnight boundaries with
    // the DB's own clock — one row at exactly CURDATE() (today 00:00:00, the
    // INCLUSIVE lower bound) and one at CURDATE() - 1s (yesterday 23:59:59, must be
    // excluded). last_entry_at = EXACT MAX(created_at).
    $root->query("INSERT INTO turnstile_events (id, card, reader, created_at)
                  VALUES (400,'CARD_A',0, CURDATE() - INTERVAL 1 SECOND)"); // yesterday 23:59:59 -> excluded
    $root->query("INSERT INTO turnstile_events (id, card, reader, created_at)
                  VALUES (401,'CARD_A',0, CURDATE())");                     // today 00:00:00 -> included
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => $ADMIN_KEY]);
    $todayCount = (int) $root->query("SELECT COUNT(*) c FROM turnstile_events
        WHERE created_at >= CURDATE() AND created_at < CURDATE() + INTERVAL 1 DAY")->fetch_assoc()['c'];
    $midnightIncluded = (int) $root->query("SELECT COUNT(*) c FROM turnstile_events
        WHERE id = 401 AND created_at >= CURDATE() AND created_at < CURDATE() + INTERVAL 1 DAY")->fetch_assoc()['c'];
    $maxAt = $root->query('SELECT MAX(created_at) m FROM turnstile_events')->fetch_assoc()['m'];
    ok('recent: entries_today matches DB range count',           ($r['json']['entries_today'] ?? null) === $todayCount);
    ok('recent: exact-midnight (CURDATE()) row is counted',      $midnightIncluded === 1);
    ok('recent: last_entry_at = exact MAX(created_at)',          ($r['json']['last_entry_at'] ?? null) === $maxAt);

    // Empty table -> entries [], entries_today 0, last_entry_at null. (Own TRUNCATE.)
    $root->query('TRUNCATE TABLE turnstile_events');
    $r = httpPostForm($HOSTPORT, '/access_recent.php', ['admin_key' => $ADMIN_KEY]);
    ok('recent: empty -> entries []',          ($r['json']['entries'] ?? 'x') === []);
    ok('recent: empty -> entries_today 0',     ($r['json']['entries_today'] ?? 'x') === 0);
    ok('recent: empty -> last_entry_at null',  isJsonNull($r['json'] ?? null, 'last_entry_at'));
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
    $out = json_encode(['status' => 'error', 'message' => $msg]);
    echo $out !== false ? $out : '{"status":"error","message":"Internal server error"}';
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

    // JSON_THROW_ON_ERROR routes an encoding failure through the sanitized catch.
    echo json_encode([
        'status'        => 'success',
        'entries'       => $entries,
        'entries_today' => $today,
        'last_entry_at' => $lastEntryAt,
    ], JSON_THROW_ON_ERROR);
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
- `turnstile_display.php` GET/loopback/405/403, oldest-next cursor, `latest_id`=MAX, enumerated display projection, `student:null` orphaned fallback, read-only → Task 1. Endpoint-level 403 is tested end-to-end via a test-only `__force_remote.php` wrapper that spoofs a non-loopback `REMOTE_ADDR` before `require`-ing the real endpoint (asserts 403 + generic JSON, no DB touched); the `isLoopback()` unit tests back it. ✅
- `access_recent.php` POST-only-before-auth (GET-with-valid-key-body → 405), admin_key, newest-50 id DESC + 50-cap, resolved slim projection, `entries_today` half-open range, exact `last_entry_at`, empty→[]/0/null → Task 2. ✅
- `access_helpers.php` with `resolveStudentByCard` + contained `normalizeStudentPhotoPath` (traversal/absent/sibling cases tested) + `isLoopback` + `parseSinceParam` (incl. leading-zeros + overflow) → Task 1 Step 3. ✅
- config.php connection (not db.php) + utf8mb4 + display_errors off + generic error writer + Cache-Control: no-store + class/code-only logging + `JSON_THROW_ON_ERROR` (encode failure → sanitized catch) → both endpoints. ✅
- Test harness: unique schema + ephemeral port + readiness probe + runtime ephemeral admin secret + DB-clock boundaries + synthetic data + teardown; method-405, **forced DB-failure** (config repointed at a nonexistent schema → asserts HTTP 500 + generic JSON + no leaked SQL/DB detail), empty-table, midnight boundary → Tasks 1–2. ✅
- `rfid_login.php`/`turnstile_pull.php`/`turnstile.php` untouched; legacy harness regression → Task 2 Step 6. ✅
- Security debt comments in both files + (carried to the PR) → file headers. ✅

**2. Placeholder scan:** every code step contains complete, runnable PHP; every run step names the exact command and expected output. No TBDs. ✅

**3. Type consistency:** helper signatures in the Task 1 Interfaces block match their definitions and both endpoints' call sites (`resolveStudentByCard(mysqli,string):?array`, `normalizeStudentPhotoPath(?array):string`, `isLoopback(?string):bool`, `parseSinceParam($raw):int`). Response keys (`status/latest_id/entry`, `status/entries/entries_today/last_entry_at`) match the spec and the test assertions. ✅

**Notes for the implementer:**
- The forced DB-failure test (display test 6) repoints the copied `config.php` at a nonexistent schema and runs **last**, because `php -S` re-includes `config.php` per request but the repoint breaks the connection for everything after it. Keep it as the final assertion before teardown.
- Task 2's assertions replace the `// --- Task 2 … ---` marker line (after display test 4, before display test 5's `TRUNCATE`) and do their own `TRUNCATE` for the recent-empty check; display test 5's `TRUNCATE` then repeats harmlessly.
- `php -S` isolates each request (includes/constants reset per request), so rewriting `config.php` between requests takes effect and prior-request state does not leak.
