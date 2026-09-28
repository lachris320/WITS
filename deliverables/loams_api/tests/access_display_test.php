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

// Admin auth: ephemeral secret, never committed. auth_helper reads
// SELECT admin_key_hash FROM admin LIMIT 1.
$root->query('CREATE TABLE admin (admin_key_hash VARCHAR(255))');
$ADMIN_KEY = bin2hex(random_bytes(16));                       // ephemeral, per-run
$adminHash = password_hash($ADMIN_KEY, PASSWORD_DEFAULT);
$st = $root->prepare('INSERT INTO admin (admin_key_hash) VALUES (?)');
$st->bind_param('s', $adminHash); $st->execute(); $st->close();

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
foreach (['access_helpers.php','turnstile_display.php','access_recent.php','auth_helper.php'] as $f) {
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

// Final tally + exit so THIS file is a real red/green gate on its own. The HTTP
// harness block (Step 5) is inserted immediately BEFORE these two lines.
echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
