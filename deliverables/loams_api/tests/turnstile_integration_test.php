<?php
/**
 * Turnstile integration test — exactly-once attendance across the three tiers.
 *
 * Drives the REAL endpoint files (turnstile.php, turnstile_pull.php,
 * rfid_login.php) over HTTP against a THROWAWAY database, exercising the
 * claim -> inject -> consume state machine end to end and asserting that every
 * scenario records attendance exactly the intended number of times.
 *
 * It does NOT touch the real wits_app database and uses only synthetic students.
 *
 * Run:  php deliverables/loams_api/tests/turnstile_integration_test.php
 * Needs: PHP CLI with mysqli + curl, and a reachable MariaDB/MySQL as root.
 */

declare(strict_types=1);
error_reporting(E_ALL);
ini_set('display_errors', '1');

const TEST_DB   = 'wits_turnstile_it';
const HOST_PORT = '127.0.0.1:8099';
$apiDir  = dirname(__DIR__);                 // deliverables/loams_api
$docroot = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_it_' . getmypid();

$pass = 0; $fail = 0;
function ok(string $label, bool $cond): void {
    global $pass, $fail;
    if ($cond) { $pass++; echo "PASS: $label\n"; }
    else       { $fail++; echo "FAIL: $label\n"; }
}

// ---- HTTP helpers -----------------------------------------------------------
function httpGet(string $path): array {
    $ch = curl_init('http://' . HOST_PORT . $path);
    curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5]);
    $body = curl_exec($ch); curl_close($ch);
    return json_decode((string)$body, true) ?: [];
}
function httpPost(string $path, array $form): array {
    $ch = curl_init('http://' . HOST_PORT . $path);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5,
        CURLOPT_POST => true, CURLOPT_POSTFIELDS => http_build_query($form),
    ]);
    $body = curl_exec($ch); curl_close($ch);
    return json_decode((string)$body, true) ?: [];
}

// A gate entry request as the controller would send it.
function gateEntry(string $card, ?string $serial, ?string $index): array {
    $f = ['method' => 'SearchCardAcs', 'Card' => $card, 'Reader' => '0'];
    if ($serial !== null) { $f['Serial'] = $serial; }
    if ($index !== null)  { $f['Index']  = $index; }
    return httpPost('/turnstile.php', $f);
}
// The bridge's claim + WITS's resulting rfid_login scan.
function bridgeInjectAndScan(mysqli $db, string $card): void {
    $evt = httpGet('/turnstile_pull.php');
    if (!empty($evt['id']) && (int)$evt['id'] > 0) {
        httpPost('/turnstile_pull.php', ['action' => 'claim', 'id' => (int)$evt['id']]);
    }
    httpPost('/rfid_login.php', ['rfid_id' => $card]); // WITS echoes the injected card
}

// ---- DB helpers -------------------------------------------------------------
function visitCount(mysqli $db, string $schoolId): int {
    $s = $db->prepare('SELECT COUNT(*) c FROM library_visits WHERE student_id = ?');
    $s->bind_param('s', $schoolId); $s->execute();
    return (int)$s->get_result()->fetch_assoc()['c'];
}
function studentVisits(mysqli $db, string $schoolId): int {
    $s = $db->prepare('SELECT visits FROM students WHERE school_id = ?');
    $s->bind_param('s', $schoolId); $s->execute();
    return (int)$s->get_result()->fetch_assoc()['visits'];
}

// ---- Setup: throwaway DB, schema, synthetic students, temp docroot ----------
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$root = new mysqli('localhost', 'root', '');
$root->query('DROP DATABASE IF EXISTS ' . TEST_DB);
$root->query('CREATE DATABASE ' . TEST_DB);
$root->select_db(TEST_DB);

$root->query('CREATE TABLE students (
    school_id VARCHAR(64) PRIMARY KEY, code VARCHAR(64), course VARCHAR(64),
    year_level VARCHAR(32), department VARCHAR(64), name VARCHAR(128),
    gender VARCHAR(16), status VARCHAR(32), photo VARCHAR(255), visits INT DEFAULT 0)');
$root->query('CREATE TABLE library_visits (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, student_id VARCHAR(64), course VARCHAR(64),
    login_time DATETIME, yearlog INT, year_level VARCHAR(32), department VARCHAR(64),
    name VARCHAR(128), gender VARCHAR(16), status VARCHAR(32), photo VARCHAR(255))');
$root->query(file_get_contents($apiDir . '/sql/turnstile_events.sql'));

// Synthetic students — one per scenario so scenarios can't cross-interfere via
// the (card, reader) fallback dedup window.
foreach ([['SID_A','CARD_A'],['SID_B','CARD_B'],['SID_C','CARD_C'],['SID_D','CARD_D'],['SID_E','CARD_E']] as [$sid,$code]) {
    $st = $root->prepare('INSERT INTO students (school_id, code, name, visits) VALUES (?,?,?,0)');
    $nm = 'Test ' . $code; $st->bind_param('sss', $sid, $code, $nm); $st->execute(); $st->close();
}

// Temp docroot: the real endpoints + a test config/db pointing at the throwaway DB.
@mkdir($docroot, 0777, true);
foreach (['turnstile.php','turnstile_pull.php','rfid_login.php'] as $f) {
    copy($apiDir . '/' . $f, $docroot . '/' . $f);
}
$cfg = "<?php define('DB_HOST','localhost');define('DB_USER','root');define('DB_PASS','');define('DB_NAME','" . TEST_DB . "');\n";
file_put_contents($docroot . '/config.php', $cfg);
file_put_contents($docroot . '/db.php', "<?php \$conn=new mysqli('localhost','root','','" . TEST_DB . "');\n");

// Launch the built-in server on the temp docroot.
$php = PHP_BINARY;
$descr = [0 => ['pipe','r'], 1 => ['file', $docroot . '/server.log', 'a'], 2 => ['file', $docroot . '/server.log', 'a']];
$proc = proc_open('"' . $php . '" -S ' . HOST_PORT . ' -t "' . $docroot . '"', $descr, $pipes);
usleep(700000); // let it bind

try {
    // Scenario 1 — single swipe -> inject -> manual scan suppressed = 1 visit.
    gateEntry('CARD_A', 'S1', '1');
    bridgeInjectAndScan($root, 'CARD_A');
    ok('1: single entry + injected scan = exactly one visit', visitCount($root,'SID_A') === 1);
    ok('1: students.visits incremented once', studentVisits($root,'SID_A') === 1);

    // Scenario 2 — controller retransmit with same (Serial, Index) = 1 visit.
    gateEntry('CARD_B', 'S2', '7');
    gateEntry('CARD_B', 'S2', '7'); // exact resend
    ok('2: retransmit with same Serial/Index = one visit', visitCount($root,'SID_B') === 1);

    // Scenario 3 — retransmit with NO Serial/Index (fallback dedup) = 1 visit.
    gateEntry('CARD_C', null, null);
    gateEntry('CARD_C', null, null); // immediate resend, within fallback window
    ok('3: retransmit without Serial/Index = one visit (fallback)', visitCount($root,'SID_C') === 1);

    // Scenario 4 — two genuine distinct swipes (different Index) = 2 visits.
    gateEntry('CARD_D', 'S4', '10');
    gateEntry('CARD_D', 'S4', '11');
    ok('4: two distinct genuine entries = two visits', visitCount($root,'SID_D') === 2);

    // Scenario 5 — plain manual scan, no turnstile token, records normally.
    httpPost('/rfid_login.php', ['rfid_id' => 'CARD_E']);
    ok('5: manual scan with no token records one visit', visitCount($root,'SID_E') === 1);
    ok('5: no stray turnstile_events for a pure manual scan',
        (int)$root->query("SELECT COUNT(*) c FROM turnstile_events WHERE card='CARD_E'")->fetch_assoc()['c'] === 0);
} finally {
    if (is_resource($proc)) { proc_terminate($proc); proc_close($proc); }
    $root->query('DROP DATABASE ' . TEST_DB);
    $root->close();
    array_map('unlink', glob($docroot . '/*') ?: []);
    @rmdir($docroot);
}

echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
