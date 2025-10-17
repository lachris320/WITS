<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);
header("Content-Type: application/json");
include "db.php";

$response = ["status" => "error", "message" => "Unknown error."];

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $old_key = isset($_POST['old_key']) ? $_POST['old_key'] : '';
    $new_key = isset($_POST['new_key']) ? $_POST['new_key'] : '';

    $stmt = $conn->prepare("SELECT admin_key_hash FROM admin WHERE id = 1 LIMIT 1");
    if ($stmt && $stmt->execute()) {
        $result = $stmt->get_result();
        if ($row = $result->fetch_assoc()) {
            if (password_verify($old_key, $row['admin_key_hash'])) {
                $new_hash = password_hash($new_key, PASSWORD_BCRYPT);
                $update = $conn->prepare("UPDATE admin SET admin_key_hash = ? WHERE id = 1");
                $update->bind_param("s", $new_hash);
                if ($update->execute()) {
                    $response = ["status" => "success", "message" => "Password updated successfully."];
                } else {
                    $response = ["status" => "error", "message" => "Failed to update password."];
                }
            } else {
                $response = ["status" => "error", "message" => "Old password is incorrect."];
            }
        } else {
            $response = ["status" => "error", "message" => "Admin not found."];
        }
    } else {
        $response = ["status" => "error", "message" => "Database query failed."];
    }
}

echo json_encode($response);
?>