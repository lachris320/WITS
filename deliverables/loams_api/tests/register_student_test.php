<?php
/**
 * Student-registration path-traversal harness — unit + integration.
 *
 * Pure unit tests cover the shared school_id validator / upload-containment
 * helper (student_id_helper.php). The HTTP block drives the REAL
 * register_student.php and upload_students_zip.php over `php -S` against a
 * THROWAWAY database (unique name) on an EPHEMERAL port with a runtime-generated
 * admin secret. Synthetic data only; never touches wits_app, and never writes
 * into the repo's uploads/ — the server's docroot is a temp copy, nested two
 * levels deep so a traversal escape still lands inside the temp tree we delete.
 *
 * Run:  php deliverables/loams_api/tests/register_student_test.php
 * Needs: PHP CLI with mysqli + curl + zip, and a reachable MariaDB/MySQL as root.
 */
declare(strict_types=1);
error_reporting(E_ALL);
ini_set('display_errors', '1');

$apiDir = dirname(__DIR__);                         // deliverables/loams_api
$helper = $apiDir . '/student_id_helper.php';        // functions only — safe to include
if (is_file($helper)) { require_once $helper; }

$pass = 0; $fail = 0;
function ok(string $label, bool $cond): void {
    global $pass, $fail;
    if ($cond) { $pass++; echo "PASS: $label\n"; }
    else       { $fail++; echo "FAIL: $label\n"; }
}

// ---- Pure unit tests: isValidSchoolId ---------------------------------------
$hasValidator = function_exists('isValidSchoolId');
ok('helper defines isValidSchoolId()', $hasValidator);
$v = function ($id) use ($hasValidator): ?bool { return $hasValidator ? isValidSchoolId($id) : null; };

foreach (['2023-00123', '21-1-0001', 'TEST-0001', 'SID_A', 'S0001', str_repeat('A', 50)] as $good) {
    ok('isValidSchoolId accepts ' . (strlen($good) > 20 ? '50-char id' : "'$good'"), $v($good) === true);
}
$bad = [
    '../../x' => '../../x', 'a/b' => 'a/b', 'a\\b' => 'a\\b', '..' => '..', '.' => '.',
    'a.b' => 'a.b', 'a b (space)' => 'a b', 'empty' => '', '51 chars' => str_repeat('A', 51),
    'trailing newline' => "abc\n", 'NUL byte' => "abc\0def", 'leading space' => ' abc',
];
foreach ($bad as $label => $id) {
    ok("isValidSchoolId rejects $label", $v($id) === false);
}
ok('isValidSchoolId rejects array',   $v(['2023-00123']) === false);
ok('isValidSchoolId rejects int',     $v(123) === false);
ok('isValidSchoolId rejects null',    $v(null) === false);

// ---- Pure unit tests: safeChildPath -----------------------------------------
$hasChild = function_exists('safeChildPath');
ok('helper defines safeChildPath()', $hasChild);
$unitDir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_reg_unit_' . getmypid() . '_' . bin2hex(random_bytes(3));
@mkdir($unitDir . '/up', 0777, true);
$c = function (string $dir, string $name) use ($hasChild): ?string {
    return $hasChild ? safeChildPath($dir, $name) : 'x-missing';
};
ok('safeChildPath plain name -> dir . name',     $c($unitDir . '/up/', 'S0001_1.png') === $unitDir . '/up/S0001_1.png');
ok('safeChildPath rejects ../ escape',           $c($unitDir . '/up/', '../evil.png') === null);
ok('safeChildPath rejects backslash escape',     $c($unitDir . '/up/', '..\\evil.png') === null);
ok('safeChildPath rejects subdir',               $c($unitDir . '/up/', 'a/b.png') === null);
ok('safeChildPath rejects "."',                  $c($unitDir . '/up/', '.') === null);
ok('safeChildPath rejects ".."',                 $c($unitDir . '/up/', '..') === null);
ok('safeChildPath rejects empty',                $c($unitDir . '/up/', '') === null);
ok('safeChildPath rejects missing dir',          $c($unitDir . '/nope/', 'a.png') === null);
@rmdir($unitDir . '/up'); @rmdir($unitDir);

// ---- HTTP + DB harness ------------------------------------------------------
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$TEST_DB = 'wits_reg_it_' . getmypid() . '_' . bin2hex(random_bytes(3));

function freePort(): int {
    $s = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
    if ($s === false) { fwrite(STDERR, "FATAL: no free port\n"); exit(2); }
    $name = stream_socket_get_name($s, false);
    fclose($s);
    return (int) substr($name, strrpos($name, ':') + 1);
}
// $fields is either an array (multipart; may hold CURLFile) or a pre-encoded
// urlencoded string (needed to send school_id[]=x).
function httpPost(string $hostport, string $path, $fields): array {
    $ch = curl_init('http://' . $hostport . $path);
    curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 10,
                            CURLOPT_POST => true, CURLOPT_POSTFIELDS => $fields]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    return ['code' => (int) $code, 'raw' => (string) $body, 'json' => json_decode((string) $body, true)];
}
function rrmdir(string $dir): void {
    if (!is_dir($dir)) { return; }
    $it = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS),
        RecursiveIteratorIterator::CHILD_FIRST);
    foreach ($it as $f) { $f->isDir() ? @rmdir($f->getPathname()) : @unlink($f->getPathname()); }
    @rmdir($dir);
}
function filesIn(string $dir): array {
    $out = [];
    foreach (glob($dir . '/*') ?: [] as $f) { if (is_file($f)) { $out[] = basename($f); } }
    sort($out);
    return $out;
}

$PORT = freePort();
$HOSTPORT = '127.0.0.1:' . $PORT;

// Throwaway schema mirroring the deployed students table shape (school_id
// VARCHAR(50) UNIQUE, code UNIQUE) plus the time_date column the endpoint writes.
$root = new mysqli('localhost', 'root', '');
$root->query('DROP DATABASE IF EXISTS ' . $TEST_DB);
$root->query('CREATE DATABASE ' . $TEST_DB);
$root->select_db($TEST_DB);
$root->query('CREATE TABLE students (
    id INT AUTO_INCREMENT PRIMARY KEY, school_id VARCHAR(50) UNIQUE NOT NULL,
    code VARCHAR(50) UNIQUE, name VARCHAR(100) NOT NULL, course VARCHAR(100),
    department VARCHAR(100), year_level VARCHAR(20), gender VARCHAR(16),
    status VARCHAR(32), photo VARCHAR(255), visits INT DEFAULT 0, time_date DATETIME)');
$root->query("INSERT INTO students (school_id, code, name) VALUES ('SEED-0001','CARD_DUP','Seed Test')");
$root->query('CREATE TABLE admin (admin_key_hash VARCHAR(255))');
$ADMIN_KEY = bin2hex(random_bytes(16));                       // ephemeral, per-run
$adminHash = password_hash($ADMIN_KEY, PASSWORD_DEFAULT);
$st = $root->prepare('INSERT INTO admin (admin_key_hash) VALUES (?)');
$st->bind_param('s', $adminHash); $st->execute(); $st->close();

// Temp tree: $base/a/www is the docroot, so "uploads/../../x" lands in $base/a.
$base    = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_reg_' . getmypid() . '_' . $PORT;
$escape  = $base . '/a';                 // where a ../../ traversal would write
$docroot = $escape . '/www';
$uploads = $docroot . '/uploads';
@mkdir($uploads, 0777, true);
foreach (['register_student.php', 'upload_students_zip.php', 'auth_helper.php', 'student_id_helper.php'] as $f) {
    if (is_file($apiDir . '/' . $f)) { copy($apiDir . '/' . $f, $docroot . '/' . $f); }
}
// Test db.php mimics the real one's display_errors=1 side effect (so the no-leak
// assertions are faithful) but points at the throwaway DB. Real db.php untouched.
file_put_contents($docroot . '/db.php',
    "<?php error_reporting(E_ALL); ini_set('display_errors',1); \$conn=new mysqli('localhost','root','','" . $TEST_DB . "');\n");

// Synthetic upload fixtures (outside the docroot).
$png1x1 = base64_decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==');
$fixtures = $base . '/fixtures';
@mkdir($fixtures, 0777, true);
file_put_contents($fixtures . '/ok.png', $png1x1);
file_put_contents($fixtures . '/fake.png', "this is not an image <?php echo 1; ?>\n");

$proc = null;
$cleanup = function () use ($root, $TEST_DB, $base, &$proc) {
    if (is_resource($proc)) { proc_terminate($proc); proc_close($proc); }
    if ($root instanceof mysqli) { @$root->query('DROP DATABASE IF EXISTS ' . $TEST_DB); }
    rrmdir($base);
};

$descr = [0 => ['pipe','r'], 1 => ['file', $base . '/server.log', 'a'], 2 => ['file', $base . '/server.log', 'a']];
$proc = proc_open([PHP_BINARY, '-S', $HOSTPORT, '-t', $docroot], $descr, $pipes);
if (!is_resource($proc)) { $cleanup(); fwrite(STDERR, "FATAL: proc_open failed\n"); exit(2); }
$ready = false;
for ($i = 0; $i < 50; $i++) {
    $cs = @stream_socket_client('tcp://' . $HOSTPORT, $en, $es, 0.1);
    if ($cs) { fclose($cs); $ready = true; break; }
    usleep(100000);
}
if (!$ready) { $cleanup(); fwrite(STDERR, "FATAL: php -S never came up on $HOSTPORT\n"); exit(2); }

$rowExists = function (string $sid) use ($root): bool {
    $s = $root->prepare('SELECT COUNT(*) c FROM students WHERE school_id = ?');
    $s->bind_param('s', $sid); $s->execute();
    $n = (int) $s->get_result()->fetch_assoc()['c']; $s->close();
    return $n > 0;
};
$register = function (array $fields, ?string $photo = null, string $photoName = 'pic.png')
        use ($HOSTPORT, $ADMIN_KEY): array {
    $fields += ['name' => 'Synthetic Student', 'course' => 'BSCS', 'year_level' => '1',
                'department' => 'CIC', 'gender' => 'M', 'status' => 'regular',
                'admin_key' => $ADMIN_KEY];
    if ($photo !== null) { $fields['photo'] = new CURLFile($photo, 'image/png', $photoName); }
    return httpPost($HOSTPORT, '/register_student.php', $fields);
};
$noLeak = function (string $raw) use ($TEST_DB): bool {
    foreach (['Fatal', 'Uncaught', 'mysqli', 'Duplicate entry', 'Warning', 'TypeError', 'Stack trace', $TEST_DB] as $needle) {
        if (stripos($raw, $needle) !== false) { return false; }
    }
    return true;
};

try {
    // 0. Auth still first.
    ok('register: missing admin_key -> 401',
        httpPost($HOSTPORT, '/register_student.php', ['school_id' => 'S0001', 'name' => 'X'])['code'] === 401);

    // 1. Traversal id + valid PNG -> rejected, no row, no escaped file.
    $before = filesIn($uploads);
    $r = $register(['school_id' => '../../evil_x', 'code' => 'CARD_T1'], $fixtures . '/ok.png');
    ok('traversal id -> status error',               ($r['json']['status'] ?? null) === 'error');
    ok('traversal id -> "Invalid School ID format."', ($r['json']['message'] ?? null) === 'Invalid School ID format.');
    ok('traversal id -> no row inserted',             !$rowExists('../../evil_x'));
    ok('traversal id -> no file escaped uploads/',    (glob($escape . '/evil_x*') ?: []) === [] && (glob($base . '/evil_x*') ?: []) === []);
    ok('traversal id -> nothing written into uploads/', filesIn($uploads) === $before);

    // 2. Separator-free but still invalid id ("a.b") -> rejected the same way.
    $r = $register(['school_id' => 'a.b', 'code' => 'CARD_T2']);
    ok('dotted id -> Invalid School ID format.', ($r['json']['message'] ?? null) === 'Invalid School ID format.' && !$rowExists('a.b'));

    // 3. Array school_id (school_id[]=x) -> clean JSON error, no fatal leak.
    $r = httpPost($HOSTPORT, '/register_student.php',
        http_build_query(['school_id' => ['S0009'], 'name' => 'Arr Test', 'admin_key' => $ADMIN_KEY]));
    ok('array school_id -> Invalid School ID format.', ($r['json']['message'] ?? null) === 'Invalid School ID format.');
    ok('array school_id -> no fatal/warning leak',     $noLeak($r['raw']));

    // 4. Valid id + valid PNG -> success, file inside uploads/, row inserted.
    $r = $register(['school_id' => '2023-00123', 'code' => 'CARD_OK'], $fixtures . '/ok.png');
    $pp = $r['json']['photo_path'] ?? '';
    ok('valid id + PNG -> success',                    ($r['json']['status'] ?? null) === 'success');
    ok('valid id + PNG -> photo_path uploads/<id>_<ts>.png',
        is_string($pp) && preg_match('#^uploads/2023-00123_\d+\.png$#D', $pp) === 1);
    ok('valid id + PNG -> file exists inside uploads/', is_string($pp) && $pp !== '' && is_file($docroot . '/' . $pp)
        && realpath(dirname($docroot . '/' . $pp)) === realpath($uploads));
    ok('valid id + PNG -> row inserted with that photo', $rowExists('2023-00123')
        && $root->query("SELECT photo FROM students WHERE school_id='2023-00123'")->fetch_assoc()['photo'] === $pp);
    ok('valid id -> status/visits bound correctly (PR #63 regression)',
        $root->query("SELECT status, visits FROM students WHERE school_id='2023-00123'")->fetch_assoc()
            === ['status' => 'regular', 'visits' => '0']);
    if (is_string($pp) && $pp !== '' && is_file($docroot . '/' . $pp)) { unlink($docroot . '/' . $pp); }

    // 5. Valid id + ".png" whose bytes are not an image -> rejected, no row, no file.
    $before = filesIn($uploads);
    $r = $register(['school_id' => 'TEST-0001', 'code' => 'CARD_FAKE'], $fixtures . '/fake.png', 'fake.png');
    ok('non-image .png -> status error',          ($r['json']['status'] ?? null) === 'error');
    ok('non-image .png -> no row inserted',       !$rowExists('TEST-0001'));
    ok('non-image .png -> nothing written',       filesIn($uploads) === $before);

    // 6. Real PNG content named .jpg -> accepted, stored under its TRUE type.
    $r = $register(['school_id' => 'SID_A', 'code' => 'CARD_MIS'], $fixtures . '/ok.png', 'photo.jpg');
    $pp = $r['json']['photo_path'] ?? '';
    ok('PNG named .jpg -> success, saved as .png',
        ($r['json']['status'] ?? null) === 'success' && is_string($pp) && preg_match('#^uploads/SID_A_\d+\.png$#D', $pp) === 1);
    if (is_string($pp) && $pp !== '' && is_file($docroot . '/' . $pp)) { unlink($docroot . '/' . $pp); }

    // 7. DB error (unique `code` collision; the duplicate check is school_id-only)
    //    -> generic error, no mysqli text, no orphaned photo.
    $before = filesIn($uploads);
    $r = $register(['school_id' => '21-1-0001', 'code' => 'CARD_DUP'], $fixtures . '/ok.png');
    ok('DB error -> generic "Failed to register student."',
        ($r['json']['status'] ?? null) === 'error' && ($r['json']['message'] ?? null) === 'Failed to register student.');
    ok('DB error -> no raw mysqli/SQL detail',     $noLeak($r['raw']));
    ok('DB error -> no row inserted',              !$rowExists('21-1-0001'));
    ok('DB error -> uploaded photo not orphaned',  filesIn($uploads) === $before);

    // 8. upload_students_zip.php: invalid-id rows counted as errors and skipped.
    $zipPath = $fixtures . '/photos.zip';
    $zip = new ZipArchive();
    $zip->open($zipPath, ZipArchive::CREATE | ZipArchive::OVERWRITE);
    $zip->addFromString('S0001.jpg', $png1x1);
    $zip->addFromString('evil_z.jpg', $png1x1);
    $zip->close();
    $rows = [
        ['school_id' => 'S0001',          'name' => 'Zip Good'],
        ['school_id' => '../../evil_z',   'name' => 'Zip Traversal'],
        ['school_id' => 'a b',            'name' => 'Zip Space'],
        ['school_id' => str_repeat('Z', 51), 'name' => 'Zip TooLong'],
    ];
    $r = httpPost($HOSTPORT, '/upload_students_zip.php', [
        'admin_key'  => $ADMIN_KEY,
        'rows'       => json_encode($rows),
        'photos_zip' => new CURLFile($zipPath, 'application/zip', 'photos.zip'),
    ]);
    ok('zip: response status success',            ($r['json']['status'] ?? null) === 'success');
    ok('zip: success_count 1 (valid row only)',   ($r['json']['success_count'] ?? null) === 1);
    ok('zip: error_count 3 (invalid ids)',        ($r['json']['error_count'] ?? null) === 3);
    ok('zip: invalid ids never inserted',         !$rowExists('../../evil_z') && !$rowExists('a b'));
    ok('zip: valid row photo inside uploads/students/', is_file($uploads . '/students/S0001.jpg'));
    ok('zip: nothing escaped the docroot',        (glob($escape . '/evil_z*') ?: []) === [] && (glob($base . '/evil_z*') ?: []) === []);
    ok('zip: response leaks nothing',             $noLeak($r['raw']));
} finally {
    $cleanup();
    $root->close();
}

echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
