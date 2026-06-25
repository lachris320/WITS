<?php
header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    http_response_code(405);
    echo json_encode(["status" => "error", "message" => "Method not allowed"]);
    exit;
}

$rfid_id = isset($_POST['rfid_id']) ? trim($_POST['rfid_id']) : '';

if (empty($rfid_id)) {
    echo json_encode(["status" => "error", "message" => "RFID ID is required"]);
    exit;
}

// Start transaction for consistency
$conn->autocommit(FALSE);

try {
    // Find student by RFID code (stored in 'code' column)
    $stmt = $conn->prepare("SELECT * FROM students WHERE code = ? LIMIT 1");
    if (!$stmt) throw new Exception("Prepare failed: " . $conn->error);

    $stmt->bind_param("s", $rfid_id);
    $stmt->execute();
    $result = $stmt->get_result();

    if ($student = $result->fetch_assoc()) {
        // Insert login record into library_visits table
        $logStmt = $conn->prepare("
            INSERT INTO library_visits
            (student_id, course, login_time, yearlog, year_level, department, name, gender, status, photo)
            VALUES (?, ?, NOW(), YEAR(CURDATE()), ?, ?, ?, ?, ?, ?)
        ");
        if (!$logStmt) throw new Exception("Log prepare failed: " . $conn->error);

        $logStmt->bind_param(
            "ssssssss",
            $student['school_id'],
            $student['course'],
            $student['year_level'],
            $student['department'],
            $student['name'],
            $student['gender'],
            $student['status'],
            $student['photo']
        );

        if (!$logStmt->execute()) {
            throw new Exception("Log insert failed: " . $logStmt->error);
        }
        $logStmt->close();

        // Increment visits counter
        $updateStmt = $conn->prepare("UPDATE students SET visits = COALESCE(visits, 0) + 1 WHERE code = ?");
        if (!$updateStmt) throw new Exception("Update prepare failed: " . $conn->error);

        $updateStmt->bind_param("s", $rfid_id);
        if (!$updateStmt->execute()) {
            throw new Exception("Update failed: " . $updateStmt->error);
        }
        $updateStmt->close();

        // Build photo URL safely (same as student_login.php)
        $protocol = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') ? "https://" : "http://";
        $host = $_SERVER['HTTP_HOST'];
        $scriptDir = rtrim(dirname($_SERVER['SCRIPT_NAME']), '/\\');
        $baseURL = $protocol . $host . $scriptDir . '/';

        // Check if photo exists using absolute path
        $photoRelPath = $student['photo'] ?: '';
        $photoAbsPath = $_SERVER['DOCUMENT_ROOT'] . '/' . ltrim($photoRelPath, '/');

        if ($photoRelPath && file_exists($photoAbsPath)) {
            $photoURL = $baseURL . $photoRelPath;
        } else {
            $photoURL = $baseURL . 'loams_api.uploads/default.jpg';
        }

        $student['photo_url'] = $photoURL;

        $conn->commit();
        echo json_encode(["status" => "success", "student" => $student]);

    } else {
        $conn->rollback();
        echo json_encode(["status" => "error", "message" => "RFID not registered"]);
    }

    $stmt->close();

} catch (Exception $e) {
    $conn->rollback();
    error_log("RFID login error: " . $e->getMessage());
    echo json_encode(["status" => "error", "message" => "Internal server error"]);
}

$conn->close();
?>
