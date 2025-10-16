<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);
header("Content-Type: application/json");
include "db.php";

$response = ["status" => "error", "message" => "Unknown error."];

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $name = isset($_POST['admin_name']) ? $_POST['admin_name'] : '';
    $position = isset($_POST['admin_position']) ? $_POST['admin_position'] : '';

    $stmt = $conn->prepare("UPDATE admin SET admin_name = ?, admin_position = ? WHERE id = 1");
    $stmt->bind_param("ss", $name, $position);

    if ($stmt->execute()) {
        $response = ["status" => "success", "message" => "Admin info updated successfully."];
    } else {
        $response = ["status" => "error", "message" => "Failed to update admin info."];
    }
}

echo json_encode($response);
?>