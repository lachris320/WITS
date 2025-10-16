<?php
header('Content-Type: application/json');

$data = json_decode(file_get_contents('php://input'), true);

if (!isset($data['school_ids']) || !is_array($data['school_ids'])) {
    echo json_encode(['status' => 'error', 'message' => 'Invalid data']);
    exit;
}

$conn = new mysqli("localhost", "root", "", "wits_app");

if ($conn->connect_error) {
    echo json_encode(['status' => 'error', 'message' => 'Connection failed']);
    exit;
}

$deleted = 0;
$stmt = $conn->prepare("DELETE FROM students WHERE school_id = ?");

foreach ($data['school_ids'] as $id) {
    $stmt->bind_param("s", $id);
    if ($stmt->execute() && $stmt->affected_rows > 0) {
        $deleted++;
    }
}

echo json_encode(['status' => 'success', 'deleted' => $deleted]);

$stmt->close();
$conn->close();
?>