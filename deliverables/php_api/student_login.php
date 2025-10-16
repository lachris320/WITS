<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);
header("Content-Type: application/json");

include "db.php";

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    http_response_code(405);
    echo json_encode(["status" => "error", "message" => "Method not allowed"]);
    exit;
}

$school_id = isset($_POST['school_id']) ? trim($_POST['school_id']) : '';

if (empty($school_id)) {
    echo json_encode(["status" => "error", "message" => "School ID is required"]);
    exit;
}

// Start transaction for consistency
$conn->autocommit(FALSE);

try {
    $stmt = $conn->prepare("SELECT * FROM students WHERE school_id = ? LIMIT 1");
    if (!$stmt) throw new Exception("Prepare failed: " . $conn->error);
    
    $stmt->bind_param("s", $school_id);
    $stmt->execute();
    $result = $stmt->get_result();

    if ($student = $result->fetch_assoc()) {
        // ✅ Insert login record
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

        // ✅ Increment visits
        $updateStmt = $conn->prepare("UPDATE students SET visits = COALESCE(visits, 0) + 1 WHERE school_id = ?");
        if (!$updateStmt) throw new Exception("Update prepare failed: " . $conn->error);
        
        $updateStmt->bind_param("s", $school_id);
        if (!$updateStmt->execute()) {
            throw new Exception("Update failed: " . $updateStmt->error);
        }
        $updateStmt->close();

        // ✅ Build photo URL safely
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
            $photoURL = $baseURL . 'uploads/default.jpg';
        }

        $student['photo_url'] = $photoURL;

        $conn->commit();
        echo json_encode(["status" => "success", "student" => $student]);

    } else {
        $conn->rollback();
        echo json_encode(["status" => "error", "message" => "Invalid School ID"]);
    }

    $stmt->close();

} catch (Exception $e) {
    $conn->rollback();
    error_log("Library login error: " . $e->getMessage());
    echo json_encode(["status" => "error", "message" => "Internal server error"]);
}

$conn->close();
?>