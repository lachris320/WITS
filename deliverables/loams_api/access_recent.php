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
