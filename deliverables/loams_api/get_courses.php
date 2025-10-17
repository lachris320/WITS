<?php
header("Content-Type: application/json");
include "db.php";

if ($conn->connect_error) {
    echo json_encode([
        "status" => "error",
        "message" => "Database connection failed: " . $conn->connect_error
    ]);
    exit;
}

$department = isset($_GET['department']) ? trim($_GET['department']) : "";
if ($department === "") {
    echo json_encode([
        "status" => "error",
        "message" => "Department not provided"
    ]);
    exit;
}

// ✅ Check if "All" option should be included
$includeAll = isset($_GET['include_all']) && $_GET['include_all'] === 'true';

$stmt = $conn->prepare("SELECT DISTINCT course FROM students WHERE department = ? AND course <> '' ORDER BY course ASC");
if (!$stmt) {
    echo json_encode([
        "status" => "error",
        "message" => "Prepare failed: " . $conn->error
    ]);
    exit;
}

$stmt->bind_param("s", $department);
$stmt->execute();
$result = $stmt->get_result();

$courses = [];
while ($row = $result->fetch_assoc()) {
    $courses[] = $row['course'];
}

if (empty($courses)) {
    echo json_encode([
        "status" => "error",
        "message" => "No courses found for this department"
    ]);
    exit;
}

// ✅ Only add "All" if requested
if ($includeAll) {
    array_unshift($courses, "All");
}

echo json_encode([
    "status" => "success",
    "courses" => $courses
]);

$stmt->close();
$conn->close();
?>