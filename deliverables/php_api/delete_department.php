<?php
error_reporting(E_ALL);
ini_set('display_errors',1);

header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $department = isset($_POST['department']) ? trim($_POST['department']) : '';

    if (empty($department)) {
        echo json_encode(["status" => "error", "message" => "Department is required."]);
        exit;
    }

    // ✅ Start transaction for safety
    $conn->begin_transaction();

    try {
        // 1. Delete visit history for students in this department
        $delVisits = $conn->prepare("
            DELETE lv FROM library_visits lv
            JOIN students s ON lv.student_id = s.school_id
            WHERE s.department = ?
        ");
        $delVisits->bind_param("s", $department);
        $delVisits->execute();

        // 2. Delete students in this department
        $delStudents = $conn->prepare("DELETE FROM students WHERE department = ?");
        $delStudents->bind_param("s", $department);
        $delStudents->execute();

        $conn->commit();
        echo json_encode([
            "status" => "success",
            "message" => "All records from department '$department' have been permanently deleted."
        ]);
    } catch (Exception $e) {
        $conn->rollback();
        echo json_encode(["status" => "error", "message" => "Failed to delete records: " . $e->getMessage()]);
    }
}
?>
