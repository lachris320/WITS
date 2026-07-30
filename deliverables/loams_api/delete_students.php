<?php
header('Content-Type: application/json');
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401s before any read of the payload / any DELETE

// school_ids arrives as a repeated form field: school_ids[]=A&school_ids[]=B
$schoolIds = isset($_POST['school_ids']) ? $_POST['school_ids'] : null;
if (!is_array($schoolIds) || count($schoolIds) === 0) {
    echo json_encode(['status' => 'error', 'message' => 'Invalid data']);
    exit;
}

$conn->begin_transaction();
try {
    // 1. Cascade: delete the affected students' visit history first.
    // prepare() returns false on error when mysqli is NOT in exception mode
    // (older configs); throw so the catch below rolls back deterministically.
    $delVisits = $conn->prepare("DELETE FROM library_visits WHERE student_id = ?");
    if (!$delVisits) {
        throw new Exception($conn->error);
    }
    $delStudent = $conn->prepare("DELETE FROM students WHERE school_id = ?");
    if (!$delStudent) {
        throw new Exception($conn->error);
    }

    $deleted = 0;
    foreach ($schoolIds as $id) {
        $delVisits->bind_param("s", $id);
        $delVisits->execute();

        $delStudent->bind_param("s", $id);
        if ($delStudent->execute() && $delStudent->affected_rows > 0) {
            $deleted++;
        }
    }

    $conn->commit();
    echo json_encode(['status' => 'success', 'deleted' => $deleted]);

    $delVisits->close();
    $delStudent->close();
} catch (Exception $e) {
    $conn->rollback();
    echo json_encode(['status' => 'error', 'message' => 'Failed to delete students.']);
}

$conn->close();
?>
