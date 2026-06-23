<?php
/**
 * Authentication Helper
 * Provides functions to verify admin authentication
 */

/**
 * Verify admin key from request
 * Returns true if valid, sends error response and exits if invalid
 */
function requireAdminAuth($conn) {
    // Check if admin_key is provided
    $admin_key = isset($_POST['admin_key']) ? $_POST['admin_key'] : '';

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
