<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);

header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $department = isset($_POST['department']) ? $_POST['department'] : '';

    if (empty($department)) {
        echo json_encode(["status" => "error", "message" => "Department is required"]);
        exit;
    }

    // ✅ Reset visits column in students table
    $resetStmt = $conn->prepare("UPDATE students SET visits = 0 WHERE department = ?");
    $resetStmt->bind_param("s", $department);
    $resetStmt->execute();
    $resetStmt->close();

    // ✅ Delete visit history from library_visits for all students in department
    $deleteStmt = $conn->prepare("
        DELETE lv 
        FROM library_visits lv
        INNER JOIN students s ON lv.student_id = s.school_id
        WHERE s.department = ?
    ");
    $deleteStmt->bind_param("s", $department);
    $deleteStmt->execute();
    $deleteStmt->close();

    echo json_encode([
        "status" => "success",
        "message" => "Visit counts reset and visit history cleared for department: $department"
    ]);

    $conn->close();
}
?>
