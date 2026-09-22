<?php
/**
 * LOAMS Turnstile HTTP Integration
 *
 * Intended for the access controller's HTTP POST / online verification mode.
 * Place this file in: C:\xampp\htdocs\loams_api\turnstile.php
 *
 * Controller flow:
 *   Wiegand reader -> controller -> HTTP POST -> this endpoint
 *   -> LOAMS student lookup / attendance -> allow or reject response
 *
 * Protocol notes from supplier documentation:
 *   - Heartbeat: method=GetStatus, server echoes Key.
 *   - Access request: method=SearchCardAcs, controller sends Card, Reader, Serial, Index.
 *   - AcsRes=1 allows passage; AcsRes=0 rejects.
 *   - Reader 0 = entry, Reader 1 = exit.
 *
 * This endpoint is the AUTHORITATIVE attendance writer. On a successful entry it
 * records the library visit AND publishes a turnstile_events row (same transaction),
 * which the local PowerShell bridge polls in order to display the student on the
 * deployed legacy WITS.exe. See deliverables/loams_api/sql/turnstile_events.sql.
 */

declare(strict_types=1);

// Never print PHP warnings/notices into the controller protocol response.
error_reporting(E_ALL);
ini_set('display_errors', '0');
ini_set('log_errors', '1');

header('Content-Type: application/json; charset=UTF-8');
header('Cache-Control: no-store');

// -----------------------------------------------------------------------------
// Deployment settings
// -----------------------------------------------------------------------------

// The supplier's quick-start HTTP examples use plain JSON responses.
// If packet-capture testing shows this specific firmware expects "DATA=" before
// the JSON response, change this to 'DATA='.
const CONTROLLER_RESPONSE_PREFIX = '';

// LAN restriction. This endpoint is an UNAUTHENTICATED authoritative attendance
// writer: with an empty allowlist, any host that can reach Apache can forge
// attendance for any known student. It may be left empty ONLY during bench
// commissioning. Before production you MUST pin the controller IP here AND bind
// Apache to the controller-facing interface. Example: '192.168.1.64'
const ALLOWED_CONTROLLER_IP = '';

// Supplier protocol: Reader 0 = IN, Reader 1 = OUT.
// LOAMS currently records a library visit on entry only.
const ENTRY_READER = 0;

// Seconds the turnstile relay stays active after an allowed request.
const OPEN_TIME_SECONDS = 1;

// Window within which a repeated (Serial, Index) is treated as a controller
// retransmit of the SAME swipe and must NOT record attendance again. Safe to
// keep generous because (Serial, Index) uniquely identify one physical swipe.
const RETRANSMIT_WINDOW_SECONDS = 30;

// Fallback window used ONLY when the firmware omits Serial/Index, keyed on
// (card, reader). Deliberately short: long enough to absorb an immediate
// controller resend, short enough that a genuine re-entry is not swallowed
// (a person cannot physically walk the gate twice within a few seconds).
const RETRANSMIT_FALLBACK_WINDOW_SECONDS = 3;

// Gate-side greeting (voice + on-screen name), per the vendor doc's "voice
// broadcasting and screen display" SearchCardAcs response. These fields are
// OPTIONAL extras the CONTROLLER renders on its own speaker/screen — separate
// from the LOAMS PC display the bridge drives. If packet capture shows this
// firmware ignores or dislikes the extra fields, set GREETING_ENABLED = false
// to fall back to the plain {ActIndex, AcsRes, Time} response.
const GREETING_ENABLED     = true;
const GREETING_ENTRY_VOICE = 'Welcome';
const GREETING_EXIT_VOICE  = 'Goodbye';

// The doc says the voice/display response is GB2312-encoded. Plain-Latin (ASCII)
// text is byte-identical in GB2312, so the '' (UTF-8) default is safe for most
// names. If the gate garbles an accented name (e.g. "Peña"), set this to
// 'GB2312' to transcode the whole response — confirm the expectation by capture.
const GREETING_RESPONSE_CHARSET = '';

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

function controllerResponse(array $payload, int $httpStatus = 200): never
{
    http_response_code($httpStatus);

    $json = json_encode(
        $payload,
        JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
    );

    if ($json === false) {
        $json = '{"AcsRes":"0","ActIndex":"0","Time":"1"}';
    }

    $out = CONTROLLER_RESPONSE_PREFIX . $json;

    // Optional whole-response transcode for firmware that expects the greeting
    // response in GB2312 (the doc's stated encoding). ASCII is unchanged, so
    // this is a no-op for plain-Latin names; //TRANSLIT//IGNORE keeps it from
    // failing on a character GB2312 can't represent.
    if (GREETING_RESPONSE_CHARSET !== '') {
        $conv = @iconv('UTF-8', GREETING_RESPONSE_CHARSET . '//TRANSLIT//IGNORE', $out);
        if ($conv !== false) {
            $out = $conv;
        }
    }

    echo $out;
    exit;
}

function readControllerRequest(): array
{
    $data = [];

    // Query-string fields (including ?method=...)
    foreach ($_GET as $key => $value) {
        if (is_scalar($value)) {
            $data[(string)$key] = (string)$value;
        }
    }

    // Standard form POST, if the controller happens to use it.
    foreach ($_POST as $key => $value) {
        if (is_scalar($value)) {
            $data[(string)$key] = (string)$value;
        }
    }

    // Supplier's POST access examples send JSON in the raw request body,
    // sometimes with application/octet-stream rather than application/json.
    $raw = trim((string)file_get_contents('php://input'));
    if ($raw !== '') {
        $decoded = json_decode($raw, true);
        if (is_array($decoded)) {
            foreach ($decoded as $key => $value) {
                if (is_scalar($value) || $value === null) {
                    $data[(string)$key] = $value === null ? '' : (string)$value;
                }
            }
        }
    }

    return $data;
}

function denyAccess(int $reader = 0): never
{
    controllerResponse([
        'ActIndex' => (string)($reader & 0x01),
        'AcsRes'   => '0',
        'Time'     => (string)OPEN_TIME_SECONDS,
    ]);
}

function allowAccess(int $reader = 0): never
{
    controllerResponse([
        'ActIndex' => (string)($reader & 0x01),
        'AcsRes'   => '1',
        'Time'     => (string)OPEN_TIME_SECONDS,
    ]);
}

// Allow passage AND greet on the gate's own screen/speaker. Falls back to the
// plain allow response when greetings are disabled. $card is echoed as-received
// (the doc's display example carries the card number); Name/Note drive the
// on-screen text, Voice the announcement.
function allowAccessWithGreeting(int $reader, array $student, string $card, string $voice): never
{
    if (!GREETING_ENABLED) {
        allowAccess($reader);
    }

    $name   = trim((string)($student['name'] ?? ''));
    $course = trim((string)($student['course'] ?? ''));
    $year   = trim((string)($student['year_level'] ?? ''));
    $note   = trim($course . ($year !== '' ? ' ' . $year : ''));

    controllerResponse([
        'Card'     => $card,
        'Systime'  => date('Y-m-d H:i:s'),
        'Voice'    => $voice,
        'Name'     => $name,
        'Note'     => $note,
        'ActIndex' => (string)($reader & 0x01),
        'AcsRes'   => '1',
        'Time'     => (string)OPEN_TIME_SECONDS,
    ]);
}

// -----------------------------------------------------------------------------
// Basic request validation / protocol routing
// -----------------------------------------------------------------------------

if (!in_array($_SERVER['REQUEST_METHOD'] ?? '', ['POST', 'GET'], true)) {
    controllerResponse(['error' => 'Method not allowed'], 405);
}

if (ALLOWED_CONTROLLER_IP !== '') {
    $remoteIp = $_SERVER['REMOTE_ADDR'] ?? '';
    if (!hash_equals(ALLOWED_CONTROLLER_IP, $remoteIp)) {
        controllerResponse(['error' => 'Forbidden'], 403);
    }
}

$request = readControllerRequest();
$method = (string)($request['method'] ?? '');
$type   = (string)($request['type'] ?? '');

// 1) Heartbeat: the controller expects the original Key value back.
if ($method === 'GetStatus') {
    controllerResponse([
        'Key' => (string)($request['Key'] ?? ''),
    ]);
}

// 2) Historical swipe record acknowledgement.
// The supplier protocol uses type=100 and requires IndexEvent echoed back.
if ($type === '100') {
    controllerResponse([
        'IndexEvent' => (string)($request['IndexEvent'] ?? ''),
    ]);
}

// 3) Historical alarm record acknowledgement.
// The supplier protocol uses type=101 and requires IndexAlarm echoed back.
if ($type === '101') {
    controllerResponse([
        'IndexAlarm' => (string)($request['IndexAlarm'] ?? ''),
    ]);
}

// Access-control requests normally use method=SearchCardAcs.
// During commissioning, also accept a request containing Card even if the
// controller firmware omits the method field from the configured URL.
//
// NOTE: the vendor quick-start shows Card base64-encoded
// (Card=ODIwNDM0MjE3NDQwMDIwNA== -> "8204342174400204"), but other firmware sends
// plain digits. We deliberately DO NOT auto-decode here. Packet-capture the real
// request first; only add a decode once the observed format proves it is encoded.
$card   = trim((string)($request['Card'] ?? ''));
$reader = ((int)($request['Reader'] ?? 0)) & 0x01;

// (Serial, Index) identify a single physical swipe and are used for retransmit
// de-duplication so a controller resend cannot record a second visit.
$serial = trim((string)($request['Serial'] ?? ''));
$cindex = trim((string)($request['Index'] ?? ''));

if ($method !== 'SearchCardAcs' && $card === '') {
    denyAccess($reader);
}

if ($card === '') {
    denyAccess($reader);
}

// -----------------------------------------------------------------------------
// LOAMS database lookup and attendance transaction
// -----------------------------------------------------------------------------

try {
    require_once __DIR__ . '/config.php';

    // config.php may enable display_errors in development; turn it back off so
    // warnings never corrupt the controller's JSON response.
    ini_set('display_errors', '0');

    mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

    $conn = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
    $conn->set_charset('utf8mb4');

    // Prefer the dedicated RFID code when it exists. If no student matches
    // students.code, fall back to students.school_id. Some deployments use the
    // school ID itself as the RFID credential and therefore leave code empty.
    // rfid_login.php performs the SAME fallback, so any card the gate authorizes
    // can also be resolved by the display path.
    $stmt = $conn->prepare('SELECT * FROM students WHERE code = ? LIMIT 1');
    $stmt->bind_param('s', $card);
    $stmt->execute();
    $result = $stmt->get_result();
    $student = $result->fetch_assoc();
    $stmt->close();

    if (!$student) {
        $stmt = $conn->prepare('SELECT * FROM students WHERE school_id = ? LIMIT 1');
        $stmt->bind_param('s', $card);
        $stmt->execute();
        $result = $stmt->get_result();
        $student = $result->fetch_assoc();
        $stmt->close();
    }

    if (!$student) {
        $conn->close();
        denyAccess($reader);
    }

    // For EXIT (Reader 1), verify the credential and allow passage, but do not
    // create another library visit because the current LOAMS schema treats a
    // visit as an entry/login event.
    if ($reader !== ENTRY_READER) {
        $conn->close();
        allowAccessWithGreeting($reader, $student, $card, GREETING_EXIT_VOICE);
    }

    // ENTRY (Reader 0): attendance write + retransmit guard + turnstile_events
    // publish all happen inside ONE transaction. Committing attendance first and
    // then inserting the event separately would allow: attendance committed ->
    // event insert fails -> controller retransmits -> no event to identify the
    // duplicate -> attendance counted twice. Keeping them atomic prevents that.
    $conn->begin_transaction();

    try {
        // Idempotency: has this exact swipe already been recorded recently?
        // FOR UPDATE serialises two near-simultaneous identical requests.
        // (Once packet capture confirms Index uniqueness, a UNIQUE KEY on
        // (controller_serial, controller_index) makes the Serial/Index path
        // airtight — see the SQL.) When the firmware omits Serial/Index we fall
        // back to a short (card, reader) window so a resend still can't double
        // count.
        if ($serial !== '' && $cindex !== '') {
            $dupWindow = RETRANSMIT_WINDOW_SECONDS;
            $dupStmt = $conn->prepare(
                'SELECT id FROM turnstile_events '
                . 'WHERE controller_serial = ? AND controller_index = ? '
                . 'AND created_at >= (NOW() - INTERVAL ? SECOND) '
                . 'ORDER BY id DESC LIMIT 1 FOR UPDATE'
            );
            $dupStmt->bind_param('ssi', $serial, $cindex, $dupWindow);
        } else {
            $dupWindow = RETRANSMIT_FALLBACK_WINDOW_SECONDS;
            $dupStmt = $conn->prepare(
                'SELECT id FROM turnstile_events '
                . 'WHERE card = ? AND reader = ? '
                . 'AND created_at >= (NOW() - INTERVAL ? SECOND) '
                . 'ORDER BY id DESC LIMIT 1 FOR UPDATE'
            );
            $dupStmt->bind_param('sii', $card, $reader, $dupWindow);
        }
        $dupStmt->execute();
        $dupExisting = $dupStmt->get_result()->fetch_assoc();
        $dupStmt->close();

        if ($dupExisting) {
            // Same physical swipe resent by the controller. Attendance was
            // already recorded on the first request; just re-open the gate.
            $conn->commit();
            $conn->close();
            allowAccessWithGreeting($reader, $student, $card, GREETING_ENTRY_VOICE);
        }

        $logStmt = $conn->prepare(
            'INSERT INTO library_visits '
            . '(student_id, course, login_time, yearlog, year_level, department, name, gender, status, photo) '
            . 'VALUES (?, ?, NOW(), YEAR(CURDATE()), ?, ?, ?, ?, ?, ?)'
        );

        $schoolId   = (string)($student['school_id'] ?? '');
        $course     = (string)($student['course'] ?? '');
        $yearLevel  = (string)($student['year_level'] ?? '');
        $department = (string)($student['department'] ?? '');
        $name       = (string)($student['name'] ?? '');
        $gender     = (string)($student['gender'] ?? '');
        $status     = (string)($student['status'] ?? '');
        $photo      = (string)($student['photo'] ?? '');

        $logStmt->bind_param(
            'ssssssss',
            $schoolId,
            $course,
            $yearLevel,
            $department,
            $name,
            $gender,
            $status,
            $photo
        );
        $logStmt->execute();
        $logStmt->close();

        // Update the matched student by school_id rather than by the incoming
        // credential. This works whether the credential matched code or school_id.
        $updateStmt = $conn->prepare(
            'UPDATE students SET visits = COALESCE(visits, 0) + 1 WHERE school_id = ?'
        );
        $updateStmt->bind_param('s', $schoolId);
        $updateStmt->execute();
        $updateStmt->close();

        // Publish the event for the local bridge. Store the card EXACTLY as it
        // will be injected into WITS (so rfid_login.php can match it) plus the
        // controller identifiers for retransmit de-duplication.
        $evtStmt = $conn->prepare(
            'INSERT INTO turnstile_events (card, controller_serial, controller_index, reader, created_at) '
            . 'VALUES (?, ?, ?, ?, NOW())'
        );
        $serialOrNull = $serial !== '' ? $serial : null;
        $indexOrNull  = $cindex !== '' ? $cindex : null;
        $evtStmt->bind_param('sssi', $card, $serialOrNull, $indexOrNull, $reader);
        $evtStmt->execute();
        $evtStmt->close();

        $conn->commit();
        $conn->close();

        allowAccessWithGreeting($reader, $student, $card, GREETING_ENTRY_VOICE);
    } catch (Throwable $e) {
        $conn->rollback();
        $conn->close();
        throw $e;
    }
} catch (Throwable $e) {
    error_log(
        'LOAMS turnstile integration error: '
        . $e->getMessage()
    );

    // Fail closed: a backend/database failure must not open the turnstile.
    denyAccess($reader);
}
