<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);

header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    // Guard BEFORE reading the payload or touching the DB: a 401 must never
    // leave a department half-reset (visits zeroed but history still present).
    requireAdminAuth($conn);

    $department = isset($_POST['department']) ? $_POST['department'] : '';

    if (empty($department)) {
        echo json_encode(["status" => "error", "message" => "Department is required"]);
        exit;
    }

    // Both statements must land together or not at all. Zeroing students.visits
    // without clearing library_visits leaves the department half-reset: the
    // counters read 0 while the history survives, and the admin — who just saw
    // a failure — retries against already-corrupted state. Both tables are
    // InnoDB (verified against information_schema), so the transaction is real.
    $failed = null;
    $conn->begin_transaction();

    try {
        // Reset visits column in students table
        $resetStmt = $conn->prepare("UPDATE students SET visits = 0 WHERE department = ?");
        if ($resetStmt === false) {
            throw new Exception("Could not prepare the visit-count reset.");
        }
        $resetStmt->bind_param("s", $department);
        if (!$resetStmt->execute()) {
            $resetStmt->close();
            throw new Exception("Could not reset the visit counts.");
        }
        $resetStmt->close();

        // Delete visit history from library_visits for all students in department
        $deleteStmt = $conn->prepare("
            DELETE lv
            FROM library_visits lv
            INNER JOIN students s ON lv.student_id = s.school_id
            WHERE s.department = ?
        ");
        if ($deleteStmt === false) {
            throw new Exception("Could not prepare the visit-history deletion.");
        }
        $deleteStmt->bind_param("s", $department);
        if (!$deleteStmt->execute()) {
            $deleteStmt->close();
            throw new Exception("Could not clear the visit history.");
        }
        $deleteStmt->close();

        $conn->commit();
    } catch (Exception $e) {
        // Roll back so the department is left exactly as it was found.
        $conn->rollback();
        $failed = $e->getMessage();
    }

    if ($failed !== null) {
        // Shape the client decodes: status/message, with a message that does not
        // collide with the client's auth-failure detection.
        echo json_encode([
            "status" => "error",
            "message" => $failed . " No changes were made."
        ]);
        $conn->close();
        exit;
    }

    echo json_encode([
        "status" => "success",
        "message" => "Visit counts reset and visit history cleared for department: $department"
    ]);

    $conn->close();
}
?>
