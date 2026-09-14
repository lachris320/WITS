<?php
/**
 * Authentication Helper
 * Provides functions to verify admin authentication
 */

/**
 * Extract the admin key from the request: $_POST first (urlencoded/multipart),
 * else the admin_key field of a JSON body. NEVER $_GET — a secret in the query
 * string would leak into access logs (security-hygiene rule). php://input is
 * re-readable for JSON bodies, so this does not disturb endpoints that decode
 * their own JSON payload.
 */
function extractAdminKey() {
    if (isset($_POST['admin_key']) && $_POST['admin_key'] !== '') {
        return (string) $_POST['admin_key'];
    }
    $raw = file_get_contents('php://input');
    if ($raw !== false && $raw !== '') {
        $body = json_decode($raw, true);
        if (is_array($body) && isset($body['admin_key'])) {
            return (string) $body['admin_key'];
        }
    }
    return '';
}

/**
 * Verify admin key from request
 * Returns true if valid, sends error response and exits if invalid
 */
function requireAdminAuth($conn) {
    $admin_key = extractAdminKey();

    if (empty($admin_key)) {
        http_response_code(401);
        echo json_encode(["status" => "error", "message" => "Admin authentication required"]);
        exit;
    }

    // Verify admin key against database
    $stmt = $conn->prepare("SELECT admin_key_hash FROM admin LIMIT 1");

    if (!$stmt) {
        http_response_code(500);
        echo json_encode(["status" => "error", "message" => "Authentication check failed"]);
        exit;
    }

    $stmt->execute();
    $result = $stmt->get_result();

    if ($row = $result->fetch_assoc()) {
        if (!password_verify($admin_key, $row['admin_key_hash'])) {
            http_response_code(401);
            echo json_encode(["status" => "error", "message" => "Invalid admin key"]);
            exit;
        }
    } else {
        http_response_code(500);
        echo json_encode(["status" => "error", "message" => "Admin configuration error"]);
        exit;
    }

    $stmt->close();
    // If we reach here, authentication succeeded
    return true;
}
?>
