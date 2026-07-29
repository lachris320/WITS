<?php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401s on missing/invalid admin_key before any mutation

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $department = isset($_POST['department']) ? trim($_POST['department']) : '';

    if (empty($department)) {
        echo json_encode(["status" => "error", "message" => "No department provided."]);
        exit;
    }

    // 🔻 Deactivate all students in the given department
    $stmt = $conn->prepare("UPDATE students SET status = 'Inactive' WHERE department = ?");
    $stmt->bind_param("s", $department);

    if ($stmt->execute()) {
        echo json_encode([
            "status" => "success",
            "message" => "All students in department '$department' have been deactivated."
        ]);
    } else {
        echo json_encode([
            "status" => "error",
            "message" => "Failed to deactivate students."
        ]);
    }

    $stmt->close();
    $conn->close();
} else {
    echo json_encode(["status" => "error", "message" => "Invalid request method."]);
}
