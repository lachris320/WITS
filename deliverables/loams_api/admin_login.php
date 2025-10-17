<?php
error_reporting(E_ALL);
ini_set('display_errors',1);

header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $admin_key = isset($_POST['admin_key']) ? $_POST['admin_key'] : '';


    $stmt = $conn->prepare("SELECT admin_key_hash FROM admin LIMIT 1");
    $stmt->execute();
    $result = $stmt->get_result();

    if ($row = $result->fetch_assoc()) {
        if (password_verify($admin_key, $row['admin_key_hash'])) {
            echo json_encode(["status" => "success", "message" => "Admin authenticated"]);
        } else {
            echo json_encode(["status" => "error", "message" => "Invalid admin key"]);
        }
    }
}
?>
