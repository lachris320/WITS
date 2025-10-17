<?php
header('Content-Type: application/json');

// Enable error logging for debugging
error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);

$data = json_decode(file_get_contents('php://input'), true);

if (!isset($data['students']) || !is_array($data['students'])) {
    echo json_encode(array(
        'status' => 'error', 
        'message' => 'Invalid data format. Expected students array.'
    ));
    exit;
}

if (count($data['students']) === 0) {
    echo json_encode(array(
        'status' => 'error', 
        'message' => 'No students provided for update.'
    ));
    exit;
}

$conn = new mysqli("localhost", "root", "", "wits_app");

if ($conn->connect_error) {
    echo json_encode(array(
        'status' => 'error', 
        'message' => 'Database connection failed: ' . $conn->connect_error
    ));
    exit;
}

$updated = 0;
$failed = 0;
$errors = array();

$conn->autocommit(FALSE); // Start transaction (compatible with older MySQL)

$stmt = $conn->prepare("UPDATE students 
    SET name=?, course=?, year_level=?, department=?, gender=?, status=? 
    WHERE school_id=?");

if (!$stmt) {
    echo json_encode(array(
        'status' => 'error', 
        'message' => 'Prepare failed: ' . $conn->error
    ));
    $conn->close();
    exit;
}

foreach ($data['students'] as $index => $student) {
    // Validate required fields
    if (empty($student['school_id'])) {
        $errors[] = "Row $index: Missing school_id";
        $failed++;
        continue;
    }
    
    // Use isset() instead of ?? for PHP 5.x compatibility
    $name = isset($student['name']) ? $student['name'] : '';
    $course = isset($student['course']) ? $student['course'] : '';
    $year_level = isset($student['year_level']) ? $student['year_level'] : '';
    $department = isset($student['department']) ? $student['department'] : '';
    $gender = isset($student['gender']) ? $student['gender'] : '';
    $status = isset($student['status']) ? $student['status'] : '';
    $school_id = $student['school_id'];
    
    $stmt->bind_param("sssssss", 
        $name,
        $course,
        $year_level,
        $department,
        $gender,
        $status,
        $school_id
    );
    
    if ($stmt->execute()) {
        if ($stmt->affected_rows > 0) {
            $updated++;
        } else {
            $errors[] = "No changes for student: $school_id";
        }
    } else {
        $failed++;
        $errors[] = "Failed to update $school_id: " . $stmt->error;
    }
}

if ($failed > 0) {
    $conn->rollback();
    echo json_encode(array(
        'status' => 'error',
        'updated' => 0,
        'failed' => $failed,
        'message' => 'Some updates failed, all changes rolled back',
        'errors' => $errors
    ));
} else {
    $conn->commit();
    
    $message = "$updated student(s) updated successfully";
    
    echo json_encode(array(
        'status' => 'success',
        'updated' => $updated,
        'failed' => $failed,
        'message' => $message,
        'errors' => $errors
    ));
}

$stmt->close();
$conn->close();
?>