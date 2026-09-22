<?php
/**
 * Turnstile POST-mode + gate-greeting test.
 *
 * The controller's POST mode (per the vendor HTTP-protocol quick-start) sends a
 * raw JSON body with Content-Type: application/json, with `method` either in the
 * query string or in the body. This drives the REAL turnstile.php over HTTP
 * against a throwaway DB and asserts:
 *   - GetStatus heartbeat echoes Key (JSON body)
 *   - SearchCardAcs authorizes, records one visit, publishes one event
 *   - the optional gate greeting (Name/Voice/Card/Systime) is returned on entry
 *   - an exit swipe greets with the exit voice and records no visit
 *   - method carried inside the JSON body works and stays idempotent
 *
 * Touches no real data; synthetic students only.
 *
 * Run:  php deliverables/loams_api/tests/turnstile_postmode_test.php
 * Needs: PHP CLI with mysqli + curl, and a reachable MariaDB/MySQL as root.
 */

declare(strict_types=1);
error_reporting(E_ALL);
ini_set('display_errors', '1');

const TEST_DB   = 'wits_turnstile_pm';
const HOST_PORT = '127.0.0.1:8096';
$apiDir  = dirname(__DIR__);
$docroot = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_pm_' . getmypid();

$pass = 0; $fail = 0;
function ok(string $label, bool $cond): void {
    global $pass, $fail;
    if ($cond) { $pass++; echo "PASS: $label\n"; }
    else       { $fail++; echo "FAIL: $label\n"; }
}

// POST a raw JSON body (application/json), like the controller's POST mode.
function postJson(string $path, array $body): array {
    $ch = curl_init('http://' . HOST_PORT . $path);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 5, CURLOPT_POST => true,
        CURLOPT_HTTPHEADER => ['Content-Type: application/json'],
        CURLOPT_POSTFIELDS => json_encode($body),
    ]);
    $resp = curl_exec($ch); curl_close($ch);
    return json_decode((string)$resp, true) ?: [];
}
function visitCount(mysqli $db, string $schoolId): int {
    $s = $db->prepare('SELECT COUNT(*) c FROM library_visits WHERE student_id = ?');
    $s->bind_param('s', $schoolId); $s->execute();
    return (int)$s->get_result()->fetch_assoc()['c'];
}

// ---- Setup ------------------------------------------------------------------
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$db = new mysqli('localhost', 'root', '');
$db->query('DROP DATABASE IF EXISTS ' . TEST_DB);
$db->query('CREATE DATABASE ' . TEST_DB);
$db->select_db(TEST_DB);
$db->query('CREATE TABLE students (
    school_id VARCHAR(64) PRIMARY KEY, code VARCHAR(64), course VARCHAR(64),
    year_level VARCHAR(32), department VARCHAR(64), name VARCHAR(128),
    gender VARCHAR(16), status VARCHAR(32), photo VARCHAR(255), visits INT DEFAULT 0)');
$db->query('CREATE TABLE library_visits (
    id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, student_id VARCHAR(64), course VARCHAR(64),
    login_time DATETIME, yearlog INT, year_level VARCHAR(32), department VARCHAR(64),
    name VARCHAR(128), gender VARCHAR(16), status VARCHAR(32), photo VARCHAR(255))');
$db->query(file_get_contents($apiDir . '/sql/turnstile_events.sql'));
$db->query("INSERT INTO students (school_id, code, course, year_level, name)
            VALUES ('SID_P','CARD_P','BSCS','3','Juan Dela Cruz')");

@mkdir($docroot, 0777, true);
foreach (['turnstile.php','turnstile_pull.php','rfid_login.php'] as $f) {
    copy($apiDir . '/' . $f, $docroot . '/' . $f);
}
file_put_contents($docroot . '/config.php',
    "<?php define('DB_HOST','localhost');define('DB_USER','root');define('DB_PASS','');define('DB_NAME','" . TEST_DB . "');\n");
file_put_contents($docroot . '/db.php',
    "<?php \$conn=new mysqli('localhost','root','','" . TEST_DB . "');\n");

// Array form so proc_terminate reliably kills php.exe on Windows.
$descr = [0 => ['pipe','r'], 1 => ['file', $docroot . '/server.log', 'a'], 2 => ['file', $docroot . '/server.log', 'a']];
$proc = proc_open([PHP_BINARY, '-S', HOST_PORT, '-t', $docroot], $descr, $pipes);
usleep(700000);

try {
    // Heartbeat, POST mode: method in query string, JSON body carries Key.
    $j = postJson('/turnstile.php?method=GetStatus', ['Serial' => 'R1', 'Key' => '26728']);
    ok('POST GetStatus (JSON body) echoes Key', ($j['Key'] ?? null) === '26728');

    // Entry, POST mode: method in query, Card/Reader/Serial/Index in JSON body.
    $j = postJson('/turnstile.php?method=SearchCardAcs',
        ['Card' => 'CARD_P', 'Reader' => '0', 'Serial' => 'R1', 'Index' => '42']);
    ok('POST SearchCardAcs (JSON body) authorizes (AcsRes=1)', ($j['AcsRes'] ?? null) === '1');
    ok('POST SearchCardAcs (JSON body) records one visit', visitCount($db, 'SID_P') === 1);
    ok('POST SearchCardAcs (JSON body) publishes one event',
        (int)$db->query("SELECT COUNT(*) c FROM turnstile_events WHERE card='CARD_P'")->fetch_assoc()['c'] === 1);

    // Gate greeting on entry: name/voice/card/systime present, course+year in Note.
    ok('greeting: entry Voice is Welcome',   ($j['Voice'] ?? null) === 'Welcome');
    ok('greeting: on-screen Name is student', ($j['Name'] ?? null) === 'Juan Dela Cruz');
    ok('greeting: Card echoed + Systime set', ($j['Card'] ?? null) === 'CARD_P' && !empty($j['Systime']));
    ok('greeting: Note carries course + year', ($j['Note'] ?? null) === 'BSCS 3');

    // method carried in the JSON body (no query string) — still works, idempotent.
    $j = postJson('/turnstile.php',
        ['method' => 'SearchCardAcs', 'Card' => 'CARD_P', 'Reader' => '0', 'Serial' => 'R1', 'Index' => '42']);
    ok('POST method-in-body retransmit is idempotent (one visit)', visitCount($db, 'SID_P') === 1);

    // Exit swipe (Reader 1): greets with the exit voice, records no visit.
    $j = postJson('/turnstile.php?method=SearchCardAcs',
        ['Card' => 'CARD_P', 'Reader' => '1', 'Serial' => 'R1', 'Index' => '43']);
    ok('exit swipe authorized (AcsRes=1)', ($j['AcsRes'] ?? null) === '1');
    ok('exit greeting Voice is Goodbye',  ($j['Voice'] ?? null) === 'Goodbye');
    ok('exit swipe records no visit',     visitCount($db, 'SID_P') === 1);
} finally {
    if (is_resource($proc)) { proc_terminate($proc); proc_close($proc); }
    $db->query('DROP DATABASE ' . TEST_DB);
    $db->close();
    array_map('unlink', glob($docroot . '/*') ?: []);
    @rmdir($docroot);
}

echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
