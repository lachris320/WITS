<?php
header("Content-Type: application/json");
include "db.php";

// Check database connection
if ($conn->connect_error) {
    echo json_encode([
        "status" => "error",
        "message" => "Database connection failed: " . $conn->connect_error
    ]);
    exit;
}

$includeAll = isset($_GET['include_all']) && $_GET['include_all'] === 'true';
$sql = "SELECT DISTINCT department FROM students WHERE department <> '' ORDER BY department ASC";
$result = $conn->query($sql);

if (!$result) {
    echo json_encode([
        "status" => "error",
        "message" => "Query failed: " . $conn->error
    ]);
    exit;
}

$departments = [];
while ($row = $result->fetch_assoc()) {
    $departments[] = $row['department'];
}

if (empty($departments)) {
    echo json_encode([
        "status" => "error",
        "message" => "No departments found"
    ]);
    exit;
}

if ($includeAll) {
    array_unshift($departments, "All");
}

echo json_encode([
    "status" => "success",
    "departments" => $departments
]);
?>
