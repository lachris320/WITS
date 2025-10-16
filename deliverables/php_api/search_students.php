<?php
header('Content-Type: application/json');
include 'db.php';

// --- Check connection ---
if ($conn->connect_error) {
    echo json_encode([
        "status" => "error",
        "message" => "Database connection failed: " . $conn->connect_error
    ]);
    exit;
}

// --- Read incoming JSON ---
$input = json_decode(file_get_contents('php://input'), true);

$search = isset($input['search']) ? trim($input['search']) : '';
$department = isset($input['department']) ? trim($input['department']) : '';
$course = isset($input['course']) ? trim($input['course']) : '';

// --- Base query ---
$query = "SELECT * FROM students WHERE 1=1";
$params = array();
$types = "";

// --- Filters ---
if ($search !== "") {
    $query .= " AND (name LIKE ? OR school_id LIKE ?)";
    $types .= "ss";
    $like = "%$search%";
    $params[] = $like;
    $params[] = $like;
}

if ($department !== "" && $department !== "Select Department") {
    $query .= " AND department = ?";
    $types .= "s";
    $params[] = $department;
}

if ($course !== "" && $course !== "Select Course" && $course !== "All") {
    $query .= " AND course = ?";
    $types .= "s";
    $params[] = $course;
}

// --- Prepare statement ---
$stmt = $conn->prepare($query);
if (!$stmt) {
    echo json_encode([
        "status" => "error",
        "message" => "SQL Prepare failed: " . $conn->error
    ]);
    exit;
}

// --- Bind parameters if needed ---
if (!empty($params)) {
    $stmt->bind_param($types, ...$params);
}

// --- Execute query ---
if (!$stmt->execute()) {
    echo json_encode([
        "status" => "error",
        "message" => "SQL Execute failed: " . $stmt->error
    ]);
    exit;
}

$result = $stmt->get_result();
$students = [];

while ($row = $result->fetch_assoc()) {
    $students[] = [
        "code" => isset($row['code']) ? $row['code'] : "",
        "school_id" => $row['school_id'],
        "name" => $row['name'],
        "department" => $row['department'],
        "course" => $row['course'],
        "year_level" => $row['year_level'],
        "status" => $row['status'],
        "gender" => $row['gender']
    ];
}

// --- Response ---
if (empty($students)) {
    echo json_encode([
        "status" => "success",
        "students" => [],
        "searchTerm" => $search,
        "message" => "No students found for given filters."
    ]);
} else {
    echo json_encode([
        "status" => "success",
        "students" => $students,
        "searchTerm" => $search
    ]);
}

$stmt->close();
$conn->close();
?>
