<?php
header("Content-Type: application/json");
include "db.php";

$data = json_decode(file_get_contents("php://input"), true);
$department = $data["department"] ?? "";

if ($department === "" || $department === "All") {
    // Return all courses
    $result = $conn->query("SELECT DISTINCT course FROM students WHERE course <> '' ORDER BY course ASC");
} else {
    // Return only courses for that department
    $stmt = $conn->prepare("SELECT DISTINCT course FROM students WHERE course <> '' AND department = ? ORDER BY course ASC");
    $stmt->bind_param("s", $department);
    $stmt->execute();
    $result = $stmt->get_result();
}

$courses = [];
while ($row = $result->fetch_assoc()) {
    $courses[] = $row['course'];
}

echo json_encode([
    "status" => "success",
    "courses" => $courses
]);
?>
