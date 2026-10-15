<?php
/**
 * Student-registration path-traversal harness — unit + integration.
 *
 * Pure unit tests cover the shared school_id validator / upload-containment /
 * photo-filename / ZIP-photo-extraction helpers (student_id_helper.php) and the
 * presence of uploads/.htaccess. The HTTP block drives the REAL
 * register_student.php and upload_students_zip.php over `php -S` against a
 * THROWAWAY database (unique name) on an EPHEMERAL port with a runtime-generated
 * admin secret. Synthetic data only; never touches wits_app, and never writes
 * into the repo's uploads/ — the server's docroot is a temp copy, nested two
 * levels deep so a traversal escape still lands inside the temp tree we delete.
 * A second temp docroot whose db.php names a NONEXISTENT database checks that a
 * DB connect failure never leaks mysqli/exception text to the client.
 *
 * Run:  php deliverables/loams_api/tests/register_student_test.php
 * Needs: PHP CLI with mysqli + curl + zip, and a reachable MariaDB/MySQL as root.
 */
declare(strict_types=1);
error_reporting(E_ALL);
ini_set('display_errors', '1');

$apiDir = dirname(__DIR__);                         // deliverables/loams_api
// Functions-only helpers — safe to include.
foreach (['student_id_helper.php', 'json_error_guard.php'] as $h) {
    if (is_file($apiDir . '/' . $h)) { require_once $apiDir . '/' . $h; }
}

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

foreach (['2023-00123', '21-1-0001', 'TEST-0001', 'SID_A', 'S0001', '0', str_repeat('A', 50)] as $good) {
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
try {
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
    // Windows reserved device names: the stem (before the first '.', trailing
    // spaces/dots trimmed) must not be CON/PRN/AUX/NUL/COMn/LPTn/CONIN$/CONOUT$.
    $reserved = ['CON.jpg', 'nul.png', 'Com1.jpg', 'LPT9.gif', 'CON', 'aux.tar.gz', 'PRN.jpg',
                 'COM0.png', 'CONIN$.jpg', 'conout$.png', "COM\u{00B9}.jpg", "lpt\u{00B3}.png",
                 'CON .jpg', 'NUL..png'];
    foreach ($reserved as $rn) {
        ok("safeChildPath rejects reserved device name '$rn'", $c($unitDir . '/up/', $rn) === null);
    }
    foreach (['CONSOLE.jpg', 'COM10.jpg', 'NUL_1.png', 'S0001.jpg', 'AUXILIARY.png', 'xCON.jpg'] as $okName) {
        ok("safeChildPath still accepts '$okName'", $c($unitDir . '/up/', $okName) === $unitDir . '/up/' . $okName);
    }
} finally {
    @rmdir($unitDir . '/up'); @rmdir($unitDir);
}

// ---- Pure unit tests: ZIP photo extraction + per-request temp dir -----------
$png1x1 = base64_decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==');
$gif1x1 = base64_decode('R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7');
$hasExtract = function_exists('extractPhotoEntries');
ok('helper defines extractPhotoEntries()', $hasExtract);
$hasTmp = function_exists('createZipImportTempDir') && function_exists('removeDirTree');
ok('helper defines createZipImportTempDir() + removeDirTree()', $hasTmp);
$unitZipDir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_reg_zip_' . getmypid() . '_' . bin2hex(random_bytes(3));
try {
    @mkdir($unitZipDir . '/out', 0777, true);
    $uz = new ZipArchive();
    $uz->open($unitZipDir . '/u.zip', ZipArchive::CREATE | ZipArchive::OVERWRITE);
    foreach ([
        'S1.png'          => $png1x1,            // accepted
        'dir/S2.gif'      => $gif1x1,            // nested: accepted by basename
        'other/S1.png'    => $gif1x1,            // duplicate basename: first wins
        's1.PNG'          => $gif1x1,            // case-insensitive duplicate: skipped
        'shell.php'       => '<?php echo 1;',    // not allow-listed
        'S3.php.jpg'      => $png1x1,            // dot in stem: not allow-listed
        '.htaccess'       => 'x',
        'S4.jpg'          => 'not an image',     // allow-listed name, fails getimagesize
        'S5.png'          => $png1x1 . str_repeat("\0", 200),   // over the per-entry cap below
        'CON.jpg'         => $png1x1,            // reserved device name
        'emptydir/'       => '',
    ] as $n => $bytes) {
        if (substr($n, -1) === '/') { $uz->addEmptyDir(rtrim($n, '/')); } else { $uz->addFromString($n, $bytes); }
    }
    $uz->close();
    $uz = new ZipArchive();
    $uz->open($unitZipDir . '/u.zip');
    // Args 5-6 = $maxEntries, &$stats (passed positionally: an older helper
    // without them just ignores extra args, so the asserts fail instead of crashing).
    $st = null;
    $n = $hasExtract ? extractPhotoEntries($uz, $unitZipDir . '/out', 150, 1024 * 1024, 20000, $st) : -1;
    $uz->close();
    $outFiles = filesIn($unitZipDir . '/out');
    ok('extractPhotoEntries returns 2 (S1.png + S2.gif)', $n === 2);
    ok('extractPhotoEntries stats: written 2, 4 name-skips (shell.php, S3.php.jpg, .htaccess, CON.jpg), 4 other skips',
        is_array($st) && ($st['written'] ?? null) === 2 && ($st['skipped_name'] ?? null) === 4
        && ($st['skipped_other'] ?? null) === 4 && ($st['too_many_entries'] ?? null) === false);
    ok('extractPhotoEntries writes only allow-listed images', $outFiles === ['S1.png', 'S2.gif']);
    ok('extractPhotoEntries: first duplicate basename wins',
        is_file($unitZipDir . '/out/S1.png') && file_get_contents($unitZipDir . '/out/S1.png') === $png1x1);
    ok('extractPhotoEntries: no subdirectories created', glob($unitZipDir . '/out/*', GLOB_ONLYDIR) === []);

    // Total cap: budget for one 1x1 PNG only -> extraction stops after the first.
    @mkdir($unitZipDir . '/out2', 0777, true);
    $uz = new ZipArchive();
    $uz->open($unitZipDir . '/u.zip');
    $n2 = $hasExtract ? extractPhotoEntries($uz, $unitZipDir . '/out2', 150, strlen($png1x1) + 10) : -1;
    $uz->close();
    ok('extractPhotoEntries stops at the total-bytes cap', $n2 === 1 && filesIn($unitZipDir . '/out2') === ['S1.png']);
    ok('extractPhotoEntries: missing dest dir -> 0, nothing written',
        $hasExtract && (function () use ($unitZipDir) {
            $z = new ZipArchive(); $z->open($unitZipDir . '/u.zip');
            $r = extractPhotoEntries($z, $unitZipDir . '/nope'); $z->close();
            return $r === 0 && !file_exists($unitZipDir . '/nope');
        })());

    // Entry-count cap: more entries than $maxEntries -> extract nothing at all.
    @mkdir($unitZipDir . '/out3', 0777, true);
    $uz = new ZipArchive();
    $uz->open($unitZipDir . '/u.zip');
    $st3 = null;
    $n3 = $hasExtract ? extractPhotoEntries($uz, $unitZipDir . '/out3', 150, 1024 * 1024, 5, $st3) : -1;
    $uz->close();
    ok('extractPhotoEntries: too many entries -> 0, nothing written',
        $n3 === 0 && filesIn($unitZipDir . '/out3') === []);
    ok('extractPhotoEntries: too many entries flagged in stats', is_array($st3) && ($st3['too_many_entries'] ?? null) === true);

    // Photo listing (taken once per import) + stem-only whole-token matcher.
    @mkdir($unitZipDir . '/list/c.gif', 0777, true);         // a DIRECTORY with an image-like name
    file_put_contents($unitZipDir . '/list/b.png', 'x');
    file_put_contents($unitZipDir . '/list/a.jpg', 'x');
    $hasList = function_exists('listPhotoFiles');
    ok('helper defines listPhotoFiles()', $hasList);
    $sep = DIRECTORY_SEPARATOR;
    ok('listPhotoFiles: sorted regular files only, full paths',
        $hasList && listPhotoFiles($unitZipDir . '/list') === [$unitZipDir . "/list{$sep}a.jpg", $unitZipDir . "/list{$sep}b.png"]);
    ok('listPhotoFiles: missing dir -> []', $hasList && listPhotoFiles($unitZipDir . '/nope') === []);
    $hasMatch = function_exists('matchPhotoForId');
    ok('helper defines matchPhotoForId()', $hasMatch);
    $m = function (array $files, string $id) use ($hasMatch): ?string {
        return $hasMatch ? matchPhotoForId($files, $id) : 'x-missing';
    };
    ok('matchPhotoForId: id "png" does not match by extension', $m(['/d/ZOTHER-0001.png'], 'png') === null);
    ok('matchPhotoForId: id "jpg" does not match by extension', $m(['/d/A.jpg', '/d/B.jpeg'], 'jpg') === null);
    ok('matchPhotoForId: exact stem matches',                   $m(['/d/ZOTHER-0001.png'], 'ZOTHER-0001') === '/d/ZOTHER-0001.png');
    ok('matchPhotoForId: whole token inside the stem matches',  $m(['/d/x_2023-1.jpg'], '2023-1') === '/d/x_2023-1.jpg');
    ok('matchPhotoForId: prefix of a longer token does not',    $m(['/d/2023-12.jpg'], '2023-1') === null);
    ok('matchPhotoForId: first match in list order wins',       $m(['/d/A_S1.png', '/d/S1.png'], 'S1') === '/d/A_S1.png');
    ok('matchPhotoForId: empty list -> null',                   $m([], 'S1') === null);

    // Per-request temp dir: under sys_get_temp_dir(), recognisable prefix, removable.
    $td = $hasTmp ? createZipImportTempDir() : null;
    ok('createZipImportTempDir -> loams_zipimport_<hex> under the system temp dir',
        is_string($td) && is_dir($td) && realpath(dirname($td)) === realpath(sys_get_temp_dir())
        && preg_match('/^loams_zipimport_[0-9a-f]{16,}$/D', basename($td)) === 1);
    if (is_string($td) && is_dir($td)) {
        @mkdir($td . '/photos/deep', 0777, true);
        file_put_contents($td . '/upload.zip', 'x');
        file_put_contents($td . '/photos/deep/a.png', 'x');
        removeDirTree($td);
        ok('removeDirTree removes the temp dir recursively', !file_exists($td));
    } else {
        ok('removeDirTree removes the temp dir recursively', false);
    }
} finally {
    rrmdir($unitZipDir);
}

// ---- Pure unit tests: ZIP photo byte caps + copy budget ---------------------
// One extracted photo can whole-token-match many rows (e.g. "A_B_C.jpg" matches
// A, B, C), and each match is copied into uploads/students/ — so the bytes COPIED
// are capped by the same constant as the bytes extracted. A legitimate import
// (each photo used once) copies <= extracted and never hits it.
ok('helper defines ZIP_PHOTO_MAX_ENTRY_BYTES = 10 MiB',
    defined('ZIP_PHOTO_MAX_ENTRY_BYTES') && constant('ZIP_PHOTO_MAX_ENTRY_BYTES') === 10 * 1024 * 1024);
ok('helper defines ZIP_PHOTO_MAX_TOTAL_BYTES = 512 MiB',
    defined('ZIP_PHOTO_MAX_TOTAL_BYTES') && constant('ZIP_PHOTO_MAX_TOTAL_BYTES') === 512 * 1024 * 1024);
$defaultConsts = [];
if ($hasExtract) {
    foreach ((new ReflectionFunction('extractPhotoEntries'))->getParameters() as $prm) {
        if ($prm->isDefaultValueAvailable() && $prm->isDefaultValueConstant()) {
            $defaultConsts[$prm->getName()] = $prm->getDefaultValueConstantName();
        }
    }
}
ok('helper defines ZIP_PHOTO_MAX_ENTRIES = 20000',
    defined('ZIP_PHOTO_MAX_ENTRIES') && constant('ZIP_PHOTO_MAX_ENTRIES') === 20000);
ok('extractPhotoEntries caps default to the shared constants',
    array_values($defaultConsts) === ['ZIP_PHOTO_MAX_ENTRY_BYTES', 'ZIP_PHOTO_MAX_TOTAL_BYTES', 'ZIP_PHOTO_MAX_ENTRIES']);

// ---- Pure unit tests: isPathInside (temp dir must not be in the API tree) ---
$hasInside = function_exists('isPathInside');
ok('helper defines isPathInside()', $hasInside);
$in = function (string $p, string $r, ?bool $ci = null) use ($hasInside): ?bool {
    return $hasInside ? isPathInside($p, $r, $ci) : null;
};
ok('isPathInside: child is inside',                   $in('/a/api/tmp/x', '/a/api') === true);
ok('isPathInside: equal path is inside',              $in('/a/api', '/a/api') === true && $in('/a/api/', '/a/api') === true);
ok('isPathInside: sibling with shared prefix is not', $in('/a/api2/x', '/a/api') === false);
ok('isPathInside: unrelated path is not',             $in('/tmp/loams_zipimport_1', '/a/api') === false);
ok('isPathInside: parent is not inside its child',    $in('/a', '/a/api') === false);
ok('isPathInside: mixed separators normalised',       $in('C:\\x\\api\\t', 'C:/x/api') === true);
ok('isPathInside: case-insensitive when asked',       $in('C:/X/API/t', 'c:/x/api', true) === true);
ok('isPathInside: case-sensitive when asked',         $in('C:/X/API/t', 'c:/x/api', false) === false);
ok('isPathInside: empty root -> false',               $in('/a', '') === false);
$hasFits = function_exists('photoCopyFits');
ok('helper defines photoCopyFits()', $hasFits);
$fits = function (int $size, int $copied, int $budget) use ($hasFits): ?bool {
    return $hasFits ? photoCopyFits($size, $copied, $budget) : null;
};
ok('photoCopyFits: fits well under budget',           $fits(10, 0, 100) === true);
ok('photoCopyFits: exactly fills the budget',         $fits(40, 60, 100) === true);
ok('photoCopyFits: one byte over the budget',         $fits(41, 60, 100) === false);
ok('photoCopyFits: budget already exhausted',         $fits(1, 100, 100) === false);
ok('photoCopyFits: zero-byte file at the exact limit', $fits(0, 100, 100) === true);
ok('photoCopyFits: negative size (filesize failed) never fits', $fits(-1, 0, 100) === false);
ok('photoCopyFits: no int overflow near PHP_INT_MAX', $fits(PHP_INT_MAX, 10, 100) === false);
ok('photoCopyFits: the real 512 MiB budget',
    $hasFits && defined('ZIP_PHOTO_MAX_TOTAL_BYTES')
    && photoCopyFits(10 * 1024 * 1024, constant('ZIP_PHOTO_MAX_TOTAL_BYTES') - 10 * 1024 * 1024, constant('ZIP_PHOTO_MAX_TOTAL_BYTES'))
    && !photoCopyFits(10 * 1024 * 1024, constant('ZIP_PHOTO_MAX_TOTAL_BYTES') - 10 * 1024 * 1024 + 1, constant('ZIP_PHOTO_MAX_TOTAL_BYTES')));
// Wiring: the endpoint has no runtime override of the budget — it must use the
// shared constant through photoCopyFits(). STATIC check: the 512 MiB budget path
// is not driven end to end (it would need 512 MiB of copies).
$zipSrc = (string) @file_get_contents($apiDir . '/upload_students_zip.php');
ok('upload_students_zip.php budgets copies via photoCopyFits(..., ZIP_PHOTO_MAX_TOTAL_BYTES)',
    preg_match('/photoCopyFits\([^;]*ZIP_PHOTO_MAX_TOTAL_BYTES\s*\)/', $zipSrc) === 1);

// ---- Repo file: uploads/.htaccess (defence in depth) ------------------------
// STATIC content check only: php -S ignores .htaccess, so its effect is not
// exercised here (verify against a real Apache when it changes).
$htaccess = $apiDir . '/uploads/.htaccess';
$htText = is_file($htaccess) ? (string) file_get_contents($htaccess) : '';
ok('uploads/.htaccess exists in the repo',            $htText !== '');
ok('uploads/.htaccess denies script files',           strpos($htText, 'Require all denied') !== false);
ok('uploads/.htaccess turns the PHP engine off',      stripos($htText, 'engine off') !== false);
ok('uploads/.htaccess script deny is case-insensitive', strpos($htText, '<FilesMatch "(?i)') !== false);
ok('uploads/.htaccess disables Indexes, ExecCGI and Includes',
    preg_match('/^\s*Options\s+-Indexes\s+-ExecCGI\s+-Includes\s*$/m', $htText) === 1);

// ---- Pure unit tests: buildPhotoFilename ------------------------------------
$hasBuild = function_exists('buildPhotoFilename');
ok('helper defines buildPhotoFilename()', $hasBuild);
$b = function ($id, $ext) use ($hasBuild): ?string { return $hasBuild ? buildPhotoFilename($id, $ext) : 'x-missing'; };
$f1 = $b('S0001', 'png');
$f2 = $b('S0001', 'png');
ok('buildPhotoFilename -> <id>_<ts>_<16 hex>.<ext>',
    is_string($f1) && preg_match('/^S0001_\d+_[0-9a-f]{16}\.png$/D', $f1) === 1);
ok('buildPhotoFilename starts with "<school_id>_"', is_string($f1) && strpos($f1, 'S0001_') === 0);
ok('buildPhotoFilename: two calls in the same second differ', is_string($f1) && is_string($f2) && $f1 !== $f2);
ok('buildPhotoFilename accepts jpg / gif / id "0"',
    is_string($b('2023-00123', 'jpg')) && is_string($b('SID_A', 'gif')) && is_string($b('0', 'png')));
ok('buildPhotoFilename rejects invalid id (null)', $b('../../x', 'png') === null && $b('', 'png') === null);
ok('buildPhotoFilename rejects bad ext (null)',    $b('S0001', 'php') === null && $b('S0001', 'png/../x') === null);
ok('guard helper defines installJsonErrorGuard()', function_exists('installJsonErrorGuard'));

// ---- HTTP + DB harness ------------------------------------------------------
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$TEST_DB = 'wits_reg_it_' . getmypid() . '_' . bin2hex(random_bytes(3));
// A database name that is never created: the "DB down" docroot points at it.
$MISSING_DB = 'wits_reg_missing_' . getmypid() . '_' . bin2hex(random_bytes(3));

function freePort(): int {
    $s = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
    if ($s === false) { throw new RuntimeException('no free port'); }
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
// Every file under $dir, recursively, as '/'-separated paths relative to $dir.
function filesUnder(string $dir): array {
    if (!is_dir($dir)) { return []; }
    $out = [];
    $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS));
    foreach ($it as $f) {
        if ($f->isFile()) { $out[] = str_replace('\\', '/', substr($f->getPathname(), strlen($dir) + 1)); }
    }
    sort($out);
    return $out;
}
function httpGet(string $hostport, string $path): array {
    $ch = curl_init('http://' . $hostport . $path);
    curl_setopt_array($ch, [CURLOPT_RETURNTRANSFER => true, CURLOPT_TIMEOUT => 10]);
    $body = curl_exec($ch);
    $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    return ['code' => (int) $code, 'raw' => (string) $body];
}
// Start `php -S` on $hostport serving $docroot; returns the process resource.
// $iniArgs are extra CLI args placed before -S (e.g. ['-d', 'sys_temp_dir=...']).
function startServer(string $hostport, string $docroot, string $log, array $iniArgs = []) {
    $descr = [0 => ['pipe','r'], 1 => ['file', $log, 'a'], 2 => ['file', $log, 'a']];
    $proc = proc_open(array_merge([PHP_BINARY], $iniArgs, ['-S', $hostport, '-t', $docroot]), $descr, $pipes);
    if (!is_resource($proc)) { throw new RuntimeException('proc_open failed'); }
    for ($i = 0; $i < 50; $i++) {
        $cs = @stream_socket_client('tcp://' . $hostport, $en, $es, 0.1);
        if ($cs) { fclose($cs); return $proc; }
        usleep(100000);
    }
    proc_terminate($proc); proc_close($proc);
    throw new RuntimeException("php -S never came up on $hostport");
}

// Everything that creates a resource (DB, temp tree, server) happens INSIDE the
// try, and the finally drops/removes whatever was created — so a failure at any
// point (connect, CREATE, mkdir, server start, an assertion that throws) never
// leaks the throwaway DB or temp files.
$root      = null;
$dbCreated = false;
$procs     = [];
$base      = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_reg_' . getmypid() . '_' . bin2hex(random_bytes(3));
$fatal     = null;

try {
    $PORT = freePort();
    $HOSTPORT = '127.0.0.1:' . $PORT;

    // Throwaway schema mirroring the deployed students table shape (school_id
    // VARCHAR(50) UNIQUE, code UNIQUE) plus the time_date column the endpoint writes.
    $root = new mysqli('localhost', 'root', '');
    $dbCreated = true;   // set before CREATE: DROP IF EXISTS is harmless if it never landed
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
    $escape  = $base . '/a';                 // where a ../../ traversal would write
    $docroot = $escape . '/www';
    $uploads = $docroot . '/uploads';
    $downRoot = $base . '/down/www';         // docroot whose db.php names a missing DB
    @mkdir($uploads, 0777, true);
    @mkdir($downRoot . '/uploads', 0777, true);
    $endpointFiles = ['register_student.php', 'upload_students_zip.php', 'auth_helper.php',
                      'student_id_helper.php', 'json_error_guard.php'];
    foreach ([$docroot, $downRoot] as $dr) {
        foreach ($endpointFiles as $f) {
            if (is_file($apiDir . '/' . $f)) { copy($apiDir . '/' . $f, $dr . '/' . $f); }
        }
    }
    // Test db.php mimics the real one's display_errors=1 side effect (so the no-leak
    // assertions are faithful) but points at the throwaway DB. Real db.php untouched.
    $dbPhp = "<?php error_reporting(E_ALL); ini_set('display_errors',1); \$conn=new mysqli('localhost','root','','%s');\n";
    file_put_contents($docroot . '/db.php', sprintf($dbPhp, $TEST_DB));
    file_put_contents($downRoot . '/db.php', sprintf($dbPhp, $MISSING_DB));

    // Synthetic upload fixtures (outside the docroot). $png1x1 is defined above.
    $fixtures = $base . '/fixtures';
    @mkdir($fixtures, 0777, true);
    file_put_contents($fixtures . '/ok.png', $png1x1);
    file_put_contents($fixtures . '/fake.png', "this is not an image <?php echo 1; ?>\n");

    $procs[] = startServer($HOSTPORT, $docroot, $base . '/server.log');
    $DOWN_HOSTPORT = '127.0.0.1:' . freePort();
    $procs[] = startServer($DOWN_HOSTPORT, $downRoot, $base . '/server_down.log');

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
    $PHOTO_RE = '#^uploads/%s_\d+_[0-9a-f]{16}\.png$#D';
    $r = $register(['school_id' => '2023-00123', 'code' => 'CARD_OK'], $fixtures . '/ok.png');
    $pp = $r['json']['photo_path'] ?? '';
    ok('valid id + PNG -> success',                    ($r['json']['status'] ?? null) === 'success');
    ok('valid id + PNG -> photo_path uploads/<id>_<ts>_<rand>.png',
        is_string($pp) && preg_match(sprintf($PHOTO_RE, '2023-00123'), $pp) === 1);
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
        ($r['json']['status'] ?? null) === 'success' && is_string($pp) && preg_match(sprintf($PHOTO_RE, 'SID_A'), $pp) === 1);
    if (is_string($pp) && $pp !== '' && is_file($docroot . '/' . $pp)) { unlink($docroot . '/' . $pp); }

    // 7. DB error (unique `code` collision; the duplicate check is school_id-only)
    //    -> generic error, no mysqli text, no orphaned photo; an existing photo
    //    in uploads/ (another request's) is left alone.
    $bystander = $uploads . '/21-1-0001_bystander.png';
    file_put_contents($bystander, $png1x1);
    $before = filesIn($uploads);
    $r = $register(['school_id' => '21-1-0001', 'code' => 'CARD_DUP'], $fixtures . '/ok.png');
    ok('DB error -> generic "Failed to register student."',
        ($r['json']['status'] ?? null) === 'error' && ($r['json']['message'] ?? null) === 'Failed to register student.');
    ok('DB error -> no raw mysqli/SQL detail',     $noLeak($r['raw']));
    ok('DB error -> no row inserted',              !$rowExists('21-1-0001'));
    ok('DB error -> uploaded photo not orphaned',  filesIn($uploads) === $before);
    ok('DB error -> other photos in uploads/ untouched', is_file($bystander));
    @unlink($bystander);

    // 7a. Re-registering an existing school_id -> {"status":"duplicate"}.
    $r = $register(['school_id' => 'SEED-0001', 'code' => 'CARD_SEED2']);
    ok('existing school_id -> status duplicate', ($r['json']['status'] ?? null) === 'duplicate');
    ok('existing school_id -> seed row unchanged',
        $root->query("SELECT name FROM students WHERE school_id='SEED-0001'")->fetch_assoc()['name'] === 'Seed Test');

    // 7b. "0" is a valid school_id (the pattern allows it) and a valid name —
    //     empty() used to reject both as "required".
    $r = $register(['school_id' => '0', 'code' => 'CARD_ZERO']);
    ok('school_id "0" -> success',      ($r['json']['status'] ?? null) === 'success' && $rowExists('0'));
    $r = $register(['school_id' => 'NAME-ZERO', 'name' => '0', 'code' => 'CARD_NZ']);
    ok('name "0" -> success',           ($r['json']['status'] ?? null) === 'success' && $rowExists('NAME-ZERO'));
    $r = $register(['school_id' => '', 'code' => 'CARD_EMPTY']);
    ok('empty school_id -> "School ID and Name are required."',
        ($r['json']['message'] ?? null) === 'School ID and Name are required.');
    $r = $register(['school_id' => 'EMPTY-NAME', 'name' => '   ', 'code' => 'CARD_EN']);
    ok('blank name -> "School ID and Name are required."',
        ($r['json']['message'] ?? null) === 'School ID and Name are required.' && !$rowExists('EMPTY-NAME'));

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
    ok('zip: valid row photo column -> uploads/students/S0001.jpg',
        $root->query("SELECT photo FROM students WHERE school_id='S0001'")->fetch_assoc()['photo'] === 'uploads/students/S0001.jpg');

    // $clientName is the filename the client claims for the ZIP part (attacker-chosen).
    $uploadZip = function (array $rows, ?string $zip = null, string $clientName = 'photos.zip', string $skipIds = '')
            use ($HOSTPORT, $ADMIN_KEY): array {
        $fields = ['admin_key' => $ADMIN_KEY, 'rows' => json_encode($rows)];
        if ($zip !== null) { $fields['photos_zip'] = new CURLFile($zip, 'application/zip', $clientName); }
        if ($skipIds !== '') { $fields['skip_ids'] = $skipIds; }
        return httpPost($HOSTPORT, '/upload_students_zip.php', $fields);
    };
    $makeZip = function (string $path, array $entries): string {
        $z = new ZipArchive();
        $z->open($path, ZipArchive::CREATE | ZipArchive::OVERWRITE);
        foreach ($entries as $n => $bytes) { $z->addFromString($n, $bytes); }
        $z->close();
        return $path;
    };
    $nameOf = function (string $sid) use ($root): ?string {
        $s = $root->prepare('SELECT name FROM students WHERE school_id = ?');
        $s->bind_param('s', $sid); $s->execute();
        $row = $s->get_result()->fetch_assoc(); $s->close();
        return $row['name'] ?? null;
    };

    // 8a. Two rows sharing a NEW id in one upload: the second insert hits the
    //     UNIQUE key. mysqli throws on PHP 8.1+, which used to abort the loop
    //     (HTTP 500, later rows lost). Now: counted as an error, loop continues.
    $logBefore = strlen((string) @file_get_contents($base . '/server.log'));
    $r = $uploadZip([
        ['school_id' => 'ZDUP-EARLY', 'name' => 'Dup Early'],
        ['school_id' => 'ZDUP-0001',  'name' => 'Dup First'],
        ['school_id' => 'ZDUP-0001',  'name' => 'Dup Second'],
        ['school_id' => 'ZDUP-LATE',  'name' => 'Dup Late'],
    ]);
    ok('zip dup-in-upload -> HTTP 200',               $r['code'] === 200);
    ok('zip dup-in-upload -> status success',         ($r['json']['status'] ?? null) === 'success');
    ok('zip dup-in-upload -> success_count 3 (early + first dup + late)', ($r['json']['success_count'] ?? null) === 3);
    ok('zip dup-in-upload -> error_count 1 (second dup)', ($r['json']['error_count'] ?? null) === 1);
    ok('zip dup-in-upload -> earlier + later valid rows inserted', $rowExists('ZDUP-EARLY') && $rowExists('ZDUP-LATE'));
    ok('zip dup-in-upload -> first occurrence kept',  $nameOf('ZDUP-0001') === 'Dup First');
    ok('zip dup-in-upload -> response leaks nothing', $noLeak($r['raw']));
    $logNew = substr((string) @file_get_contents($base . '/server.log'), $logBefore);
    ok('zip dup-in-upload -> detail logged server-side', stripos($logNew, 'Duplicate entry') !== false);

    // 8b. A row for an id already in the DB (not in skip_ids) whose ZIP has a
    //     matching photo: the insert fails, so the EXISTING student's photo file
    //     must not be overwritten (the copy used to happen before the insert).
    $keepPhoto = $uploads . '/students/PHOTO-KEEP.jpg';
    $keepBytes = 'ORIGINAL-PHOTO-BYTES-' . bin2hex(random_bytes(4));
    file_put_contents($keepPhoto, $keepBytes);
    $root->query("INSERT INTO students (school_id, name, photo) VALUES ('PHOTO-KEEP','Keep Original','uploads/students/PHOTO-KEEP.jpg')");
    $zip2 = $makeZip($fixtures . '/photos2.zip', ['PHOTO-KEEP.jpg' => $png1x1, 'ZNEW-0001.jpg' => $png1x1]);
    $r = $uploadZip([
        ['school_id' => 'PHOTO-KEEP', 'name' => 'Imposter'],
        ['school_id' => 'ZNEW-0001',  'name' => 'New With Photo'],
    ], $zip2);
    ok('zip existing-id -> HTTP 200',                 $r['code'] === 200);
    ok('zip existing-id -> error_count 1',            ($r['json']['error_count'] ?? null) === 1);
    ok('zip existing-id -> success_count 1 (new row)', ($r['json']['success_count'] ?? null) === 1);
    ok('zip existing-id -> existing photo bytes unchanged',
        is_file($keepPhoto) && (string) file_get_contents($keepPhoto) === $keepBytes);
    ok('zip existing-id -> existing row unchanged',   $nameOf('PHOTO-KEEP') === 'Keep Original');
    ok('zip existing-id -> new row photo written + recorded',
        is_file($uploads . '/students/ZNEW-0001.jpg')
        && $root->query("SELECT photo FROM students WHERE school_id='ZNEW-0001'")->fetch_assoc()['photo'] === 'uploads/students/ZNEW-0001.jpg');
    ok('zip existing-id -> response leaks nothing',   $noLeak($r['raw']));

    // 8c. Photo copy fails after a successful insert (target name is occupied by
    //     a directory) -> row kept, but photo is NULL, never a path to a missing file.
    @mkdir($uploads . '/students/ZCOPY-FAIL.jpg', 0777, true);
    $zip3 = $makeZip($fixtures . '/photos3.zip', ['ZCOPY-FAIL.jpg' => $png1x1]);
    $r = $uploadZip([['school_id' => 'ZCOPY-FAIL', 'name' => 'Copy Fail']], $zip3);
    ok('zip copy-fail -> HTTP 200 + success_count 1',
        $r['code'] === 200 && ($r['json']['success_count'] ?? null) === 1);
    $cf = $rowExists('ZCOPY-FAIL')
        ? $root->query("SELECT photo FROM students WHERE school_id='ZCOPY-FAIL'")->fetch_assoc()['photo'] : 'missing-row';
    ok('zip copy-fail -> row photo is NULL', $cf === null);

    // 8d. Non-string row fields (JSON array/object) are read as '' instead of
    //     throwing TypeError from trim(): a name that becomes '' is an error, any
    //     other field just imports as ''. Scalars are cast to string.
    $r = $uploadZip([
        ['school_id' => 'ZTYPE-0001', 'name' => ['x']],
        ['school_id' => 'ZTYPE-0002', 'name' => 'Obj Course', 'course' => ['a' => 1],
         'department' => ['d'], 'gender' => ['g' => 'M'], 'status' => [1], 'year_level' => 2],
    ]);
    ok('zip non-string fields -> HTTP 200',            $r['code'] === 200);
    ok('zip non-string fields -> success 1, error 1',
        ($r['json']['success_count'] ?? null) === 1 && ($r['json']['error_count'] ?? null) === 1);
    ok('zip non-string fields -> array name row not inserted', !$rowExists('ZTYPE-0001'));
    $tr = $rowExists('ZTYPE-0002')
        ? $root->query("SELECT course, department, gender, status, year_level FROM students WHERE school_id='ZTYPE-0002'")->fetch_assoc()
        : null;
    ok('zip non-string fields -> non-scalars stored as \'\', int year_level as "2"',
        $tr === ['course' => '', 'department' => '', 'gender' => '', 'status' => '', 'year_level' => '2']);
    ok('zip non-string fields -> response leaks nothing', $noLeak($r['raw']));

    // 9. DB connect failure (db.php names a nonexistent DB; mysqli throws during
    //    the include while display_errors=1) -> HTTP 500 + generic JSON, no detail.
    $downLeak = function (string $raw) use ($MISSING_DB): array {
        $hits = [];
        foreach (['mysqli', 'Fatal', 'Stack trace', 'Uncaught', 'Warning', $MISSING_DB, '.php'] as $needle) {
            if (stripos($raw, $needle) !== false) { $hits[] = $needle; }
        }
        return $hits;
    };
    $downCases = [
        'register' => ['/register_student.php', ['admin_key' => $ADMIN_KEY, 'school_id' => 'S0002', 'name' => 'Down Test']],
        'zip'      => ['/upload_students_zip.php', ['admin_key' => $ADMIN_KEY, 'rows' => json_encode([])]],
    ];
    foreach ($downCases as $label => [$path, $fields]) {
        $r = httpPost($DOWN_HOSTPORT, $path, $fields);
        ok("DB down ($label) -> HTTP 500",                     $r['code'] === 500);
        ok("DB down ($label) -> body is JSON status error",    is_array($r['json']) && ($r['json']['status'] ?? null) === 'error');
        ok("DB down ($label) -> generic \"Server error.\"",    ($r['json']['message'] ?? null) === 'Server error.');
        $hits = $downLeak($r['raw']);
        ok("DB down ($label) -> leaks nothing" . ($hits ? ' (found: ' . implode(', ', $hits) . ')' : ''), $hits === []);
    }
    $downLog = (string) @file_get_contents($base . '/server_down.log');
    ok('DB down -> cause still logged server-side', stripos($downLog, 'mysqli_sql_exception') !== false);

    // 10. ZIP import must never put attacker-controlled files inside the docroot
    //     (admin-only RCE): the client filename is ignored, only allow-listed
    //     image entries are unpacked, into a per-request temp dir OUTSIDE the
    //     docroot that is removed when the request ends.
    $photoOf = function (string $sid) use ($root) {
        $s = $root->prepare('SELECT photo FROM students WHERE school_id = ?');
        $s->bind_param('s', $sid); $s->execute();
        $row = $s->get_result()->fetch_assoc(); $s->close();
        return $row === null ? 'missing-row' : $row['photo'];
    };
    $tmpImports = function (): array {
        $g = glob(sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'loams_zipimport_*') ?: [];
        sort($g);
        return $g;
    };
    $RCE_SRC    = "<?php echo 'RCE-'.(6*7); ?>";
    $RCE_MARKER = 'RCE-42';
    $treeBefore = filesUnder($escape);
    $tmpBefore  = $tmpImports();

    // 10a. "ZIP" that is really PHP source, uploaded as x.php (ZipArchive::open fails).
    file_put_contents($fixtures . '/rce_src.bin', $RCE_SRC);
    $r = $uploadZip([['school_id' => 'ZXPHP-0001', 'name' => 'Raw Php Upload']], $fixtures . '/rce_src.bin', 'x.php');
    ok('zip as x.php (raw PHP) -> status success',     ($r['json']['status'] ?? null) === 'success');
    ok('zip as x.php (raw PHP) -> row inserted, photo NULL', $photoOf('ZXPHP-0001') === null);
    ok('zip as x.php (raw PHP) -> no x.php anywhere under the server tree',
        array_filter(filesUnder($escape), fn($p) => strcasecmp(basename($p), 'x.php') === 0) === []);
    $g = httpGet($HOSTPORT, '/uploads/temp/x.php');
    ok('zip as x.php (raw PHP) -> GET /uploads/temp/x.php does not execute it', strpos($g['raw'], $RCE_MARKER) === false);

    // 10b. Valid ZIP (with a STORED, i.e. uncompressed, PHP payload entry so the
    //      archive bytes are also runnable PHP) uploaded as x.php -> processed as a
    //      ZIP, its photo is used, but nothing named x.php is ever written.
    $polyPath = $fixtures . '/poly.zip';
    $pz = new ZipArchive();
    $pz->open($polyPath, ZipArchive::CREATE | ZipArchive::OVERWRITE);
    $pz->addFromString('note.txt', $RCE_SRC);
    $pz->setCompressionName('note.txt', ZipArchive::CM_STORE);
    $pz->addFromString('ZXPHP-0002.jpg', $png1x1);
    $pz->close();
    $r = $uploadZip([['school_id' => 'ZXPHP-0002', 'name' => 'Polyglot Upload']], $polyPath, 'x.php');
    ok('zip as x.php (polyglot) -> status success',    ($r['json']['status'] ?? null) === 'success');
    ok('zip as x.php (polyglot) -> photo still extracted + recorded',
        $photoOf('ZXPHP-0002') === 'uploads/students/ZXPHP-0002.jpg' && is_file($uploads . '/students/ZXPHP-0002.jpg'));
    ok('zip as x.php (polyglot) -> no x.php anywhere under the server tree',
        array_filter(filesUnder($escape), fn($p) => strcasecmp(basename($p), 'x.php') === 0) === []);
    $g = httpGet($HOSTPORT, '/uploads/temp/x.php');
    ok('zip as x.php (polyglot) -> GET /uploads/temp/x.php does not execute it', strpos($g['raw'], $RCE_MARKER) === false);

    // 10c. Hostile entries: only allow-listed image basenames are unpacked.
    $hostile = $makeZip($fixtures . '/hostile.zip', [
        'ZRCE-0001.jpg'     => $png1x1,                                   // ok
        'shell.php'         => $RCE_SRC,                                  // script
        'ZRCE-0002.php.jpg' => $png1x1,                                   // double extension
        '.htaccess'         => "SetHandler application/x-httpd-php\n",    // config override
        'sub/ZRCE-0003.png' => $png1x1,                                   // nested: basename is safe
        '../ZRCE-0004.jpg'  => $png1x1,                                   // traversal: basename is safe
        'ZRCE-0005.jpg'     => "not an image <?php echo 1; ?>\n",         // fails getimagesize
    ]);
    $r = $uploadZip([
        ['school_id' => 'ZRCE-0001', 'name' => 'Rce One'],   ['school_id' => 'ZRCE-0002', 'name' => 'Rce Two'],
        ['school_id' => 'ZRCE-0003', 'name' => 'Rce Three'], ['school_id' => 'ZRCE-0004', 'name' => 'Rce Four'],
        ['school_id' => 'ZRCE-0005', 'name' => 'Rce Five'],
    ], $hostile);
    ok('zip hostile entries -> status success, 5 rows', ($r['json']['status'] ?? null) === 'success'
        && ($r['json']['success_count'] ?? null) === 5);
    ok('zip hostile entries -> plain image entry used',   $photoOf('ZRCE-0001') === 'uploads/students/ZRCE-0001.jpg'
        && is_file($uploads . '/students/ZRCE-0001.jpg'));
    ok('zip hostile entries -> double-extension entry ignored (photo NULL)', $photoOf('ZRCE-0002') === null);
    ok('zip hostile entries -> nested entry used by basename', $photoOf('ZRCE-0003') === 'uploads/students/ZRCE-0003.jpg'
        && is_file($uploads . '/students/ZRCE-0003.jpg'));
    ok('zip hostile entries -> "../" entry used by basename, nothing escaped',
        $photoOf('ZRCE-0004') === 'uploads/students/ZRCE-0004.jpg'
        && (glob($escape . '/ZRCE*') ?: []) === [] && (glob($base . '/ZRCE*') ?: []) === []);
    ok('zip hostile entries -> non-image .jpg ignored (photo NULL)', $photoOf('ZRCE-0005') === null
        && !is_file($uploads . '/students/ZRCE-0005.jpg'));
    $g = httpGet($HOSTPORT, '/uploads/temp/photos/shell.php');
    ok('zip hostile entries -> GET /uploads/temp/photos/shell.php does not execute it', strpos($g['raw'], $RCE_MARKER) === false);

    // 10d. Oversize entry: valid PNG header + > 10 MiB padding (compresses to a
    //      few KiB, so the upload itself is small) -> row imported, photo NULL.
    $big = $makeZip($fixtures . '/big.zip', ['ZBIG-0001.jpg' => $png1x1 . str_repeat("\0", 10 * 1024 * 1024 + 1)]);
    $r = $uploadZip([['school_id' => 'ZBIG-0001', 'name' => 'Big Photo']], $big);
    ok('zip oversize entry -> row imported',            ($r['json']['success_count'] ?? null) === 1);
    ok('zip oversize entry -> photo NULL, no file',     $photoOf('ZBIG-0001') === null
        && !is_file($uploads . '/students/ZBIG-0001.jpg'));

    // 10e. Stale photo: import #1's photo (row skipped) must not be picked up by
    //      import #2, whose own ZIP has no photo for that id.
    $st1 = $makeZip($fixtures . '/stale1.zip', ['ZSTALE-0001.jpg' => $png1x1]);
    $r = $uploadZip([['school_id' => 'ZSTALE-0001', 'name' => 'Stale Skipped']], $st1, 'photos.zip', 'ZSTALE-0001');
    ok('zip stale #1 -> row skipped', ($r['json']['skipped_count'] ?? null) === 1 && !$rowExists('ZSTALE-0001'));
    $st2 = $makeZip($fixtures . '/stale2.zip', ['ZSTALE-OTHER.jpg' => $png1x1]);
    $r = $uploadZip([['school_id' => 'ZSTALE-0001', 'name' => 'Stale Imported']], $st2);
    ok('zip stale #2 -> row inserted with photo NULL', ($r['json']['success_count'] ?? null) === 1
        && $photoOf('ZSTALE-0001') === null);
    ok('zip stale #2 -> no photo file from the earlier upload', !is_file($uploads . '/students/ZSTALE-0001.jpg'));

    // 10f. Windows reserved device name: "CON" is a valid school_id, but its photo
    //      must never be written as CON.jpg (a device, not a file, on Windows).
    $con = $makeZip($fixtures . '/con.zip', ['CON.jpg' => $png1x1]);
    $r = $uploadZip([['school_id' => 'CON', 'name' => 'Reserved Name']], $con);
    ok('zip reserved name -> row inserted',             ($r['json']['success_count'] ?? null) === 1 && $rowExists('CON'));
    ok('zip reserved name -> photo NULL',               $photoOf('CON') === null);
    ok('zip reserved name -> no CON* file in uploads/students',
        array_filter(scandir($uploads . '/students') ?: [], fn($f) => preg_match('/^con(\.|$)/i', $f) === 1) === []);

    // 10f2. One photo shared by several rows (whole-token match on each id):
    //       under the normal copy budget every row still gets its own copy.
    $amp = $makeZip($fixtures . '/amp.zip', ['ZAMP-A_ZAMP-B_ZAMP-C.jpg' => $png1x1]);
    $r = $uploadZip([
        ['school_id' => 'ZAMP-A', 'name' => 'Amp A'], ['school_id' => 'ZAMP-B', 'name' => 'Amp B'],
        ['school_id' => 'ZAMP-C', 'name' => 'Amp C'],
    ], $amp);
    ok('zip shared photo -> 3 rows imported', ($r['json']['success_count'] ?? null) === 3);
    $ampOk = true;
    foreach (['ZAMP-A', 'ZAMP-B', 'ZAMP-C'] as $sid) {
        $ampOk = $ampOk && $photoOf($sid) === "uploads/students/$sid.jpg"
            && is_file($uploads . "/students/$sid.jpg")
            && file_get_contents($uploads . "/students/$sid.jpg") === $png1x1;
    }
    ok('zip shared photo -> each matching row gets its own copy', $ampOk);

    // 10h. Matching is on the filename STEM: a valid school_id that equals an
    //      image extension ("png") must not match every "*.png" photo.
    $ext = $makeZip($fixtures . '/ext.zip', ['ZOTHER-0001.png' => $png1x1]);
    $r = $uploadZip([
        ['school_id' => 'png', 'name' => 'Ext Id'], ['school_id' => 'ZOTHER-0001', 'name' => 'Ext Other'],
    ], $ext);
    ok('zip id "png" -> both rows imported',         ($r['json']['success_count'] ?? null) === 2);
    ok('zip id "png" -> no photo (extension is not a token)', $photoOf('png') === null
        && !is_file($uploads . '/students/png.jpg'));
    ok('zip id "png" -> ZOTHER-0001 still gets its photo',
        $photoOf('ZOTHER-0001') === 'uploads/students/ZOTHER-0001.jpg' && is_file($uploads . '/students/ZOTHER-0001.jpg'));

    // 10i. Fail closed if the per-request temp dir would land inside the API tree:
    //      a second server on the same docroot whose sys_temp_dir is IN the docroot.
    $inTmp = $docroot . '/tmpin';
    @mkdir($inTmp, 0777, true);
    $IN_HOSTPORT = '127.0.0.1:' . freePort();
    $procs[] = startServer($IN_HOSTPORT, $docroot, $base . '/server_in.log', ['-d', 'sys_temp_dir=' . $inTmp]);
    $inZip = $makeZip($fixtures . '/in.zip', ['ZIN-0001.jpg' => $png1x1]);
    $r = httpPost($IN_HOSTPORT, '/upload_students_zip.php', [
        'admin_key'  => $ADMIN_KEY,
        'rows'       => json_encode([['school_id' => 'ZIN-0001', 'name' => 'Temp Inside']]),
        'photos_zip' => new CURLFile($inZip, 'application/zip', 'photos.zip'),
    ]);
    ok('zip temp-dir-in-API-tree -> row imported',   ($r['json']['success_count'] ?? null) === 1);
    ok('zip temp-dir-in-API-tree -> photo NULL (fail closed)', $photoOf('ZIN-0001') === null
        && !is_file($uploads . '/students/ZIN-0001.jpg'));
    ok('zip temp-dir-in-API-tree -> nothing left in the in-tree temp dir',
        (glob($inTmp . '/loams_zipimport_*') ?: []) === []);
    $inLog = (string) @file_get_contents($base . '/server_in.log');
    ok('zip temp-dir-in-API-tree -> logged server-side', stripos($inLog, 'inside the API tree') !== false);
    @rmdir($inTmp);

    // 10j. Diagnostics: a ZIP that fails to open and skipped entry names are logged
    //      (generic text + counts/codes, no client data).
    $logBefore = strlen((string) @file_get_contents($base . '/server.log'));
    $uploadZip([['school_id' => 'ZDIAG-0001', 'name' => 'Diag One']], $fixtures . '/rce_src.bin');
    $uploadZip([['school_id' => 'ZDIAG-0002', 'name' => 'Diag Two']],
        $makeZip($fixtures . '/diag.zip', ['my photo (1).jpg' => $png1x1, 'ZDIAG-0002.jpg' => $png1x1]));
    $logNew = substr((string) @file_get_contents($base . '/server.log'), $logBefore);
    ok('zip diagnostics -> ZIP open failure logged with its code', preg_match('/ZIP open failed \(code \d+\)/', $logNew) === 1);
    ok('zip diagnostics -> summary logs 1 written, 1 name-skip', preg_match('/1 written, 1 skipped \(name not allowed\)/', $logNew) === 1);
    ok('zip diagnostics -> no client filename in the log', strpos($logNew, 'my photo (1)') === false);

    // 10g. Global: no script/config file and nothing outside uploads/students/
    //      appeared anywhere in the server tree; no uploads/temp; no temp leftovers.
    $newFiles = array_values(array_diff(filesUnder($escape), $treeBefore));
    $badNew   = array_values(array_filter($newFiles, fn($p) => preg_match('#^www/uploads/students/[^/]+\.jpg$#D', $p) !== 1));
    ok('zip imports -> only uploads/students/*.jpg created' . ($badNew ? ' (found: ' . implode(', ', $badNew) . ')' : ''),
        $badNew === []);
    ok('zip imports -> no .php/.phtml/.htaccess created',
        array_filter($newFiles, fn($p) => preg_match('/(\.php|\.phtml|\.htaccess)$/i', $p) === 1) === []);
    ok('zip imports -> no uploads/temp directory under the docroot', !file_exists($uploads . '/temp'));
    // Diffing against the section-start snapshot ignores pre-existing dirs. A dir
    // created concurrently by ANOTHER process during section 10 would still be
    // counted (no PID or other owner marker to tell them apart).
    $leftover = array_values(array_diff($tmpImports(), $tmpBefore));
    ok('zip imports -> no per-request temp dir left behind' . ($leftover ? ' (' . count($leftover) . ' left)' : ''),
        $leftover === []);
} catch (Throwable $e) {
    $fatal = get_class($e) . ': ' . $e->getMessage();
} finally {
    foreach ($procs as $p) { if (is_resource($p)) { proc_terminate($p); proc_close($p); } }
    if ($root instanceof mysqli) {
        if ($dbCreated) { try { $root->query('DROP DATABASE IF EXISTS ' . $TEST_DB); } catch (Throwable $e) { /* best effort */ } }
        $root->close();
    }
    rrmdir($base);
}

if ($fatal !== null) {
    fwrite(STDERR, "FATAL: $fatal\n");
    echo "\n$pass passed, $fail failed (aborted)\n";
    exit(2);
}
echo "\n$pass passed, $fail failed\n";
exit($fail === 0 ? 0 : 1);
