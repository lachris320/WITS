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
