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
