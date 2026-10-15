<?php
error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
require_once __DIR__ . "/student_id_helper.php";
// db.php re-enables display_errors; turn it back off so no warning/exception
// text (paths, SQL, DB names) can ever reach the JSON response.
ini_set('display_errors', 0);
requireAdminAuth($conn);   // 401 "Admin authentication required" before anything

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    // Non-string form values (e.g. name[]=x) read as '' instead of a TypeError.
    $postStr = function ($key) {
        return (isset($_POST[$key]) && is_string($_POST[$key])) ? trim($_POST[$key]) : '';
    };
    // school_id keeps a non-string as-is so isValidSchoolId() rejects it below.
    $school_id  = $_POST['school_id'] ?? '';
    if (is_string($school_id)) $school_id = trim($school_id);
    $name       = $postStr('name');
    $course     = $postStr('course');
    $year_level = $postStr('year_level');
    $department = $postStr('department');
    $code       = $postStr('code');
    $gender     = $postStr('gender');
    $status     = $postStr('status');
    $visits     = isset($_POST['visits']) ? intval($_POST['visits']) : 0;

    // ✅ Require school_id and name
    if (empty($school_id) || empty($name)) {
        echo json_encode(["status" => "error", "message" => "School ID and Name are required."]);
        exit;
    }

    // ✅ school_id is embedded in the photo filename — allow only [A-Za-z0-9_-]{1,50}
    if (!isValidSchoolId($school_id)) {
        echo json_encode(["status" => "error", "message" => "Invalid School ID format."]);
        exit;
    }

    // ✅ Check for duplicates
    $check = $conn->prepare("SELECT id FROM students WHERE school_id = ?");
    $check->bind_param("s", $school_id);
    $check->execute();
    $result = $check->get_result();
    if ($result->num_rows > 0) {
        echo json_encode(["status" => "duplicate", "message" => "Student already exists."]);
        $check->close();
        exit;
    }
    $check->close();

    // ✅ Handle photo upload with better validation
    $uploadDir = "uploads/";
    if (!is_dir($uploadDir)) {
        mkdir($uploadDir, 0755, true);
    }

    $photoPath = null;
    $photo = $_FILES['photo'] ?? null;
    if (is_array($photo) && !empty($photo['name'])) {
        // ✅ Get file extension (more reliable than MIME type)
        $fileName = is_string($photo['name']) ? $photo['name'] : '';
        $fileExtension = strtolower(pathinfo($fileName, PATHINFO_EXTENSION));

        // ✅ Check extension instead of MIME type
        $allowedExtensions = ['jpg', 'jpeg', 'png', 'gif'];

        if (!in_array($fileExtension, $allowedExtensions)) {
            echo json_encode([
                "status" => "error",
                "message" => "Invalid file type. Only JPG, PNG, and GIF allowed. (Got: .$fileExtension)"
            ]);
            exit;
        }

        // ✅ Optional: Check file size (max 5MB)
        $maxFileSize = 5 * 1024 * 1024; // 5MB
        if ($photo['size'] > $maxFileSize) {
            echo json_encode([
                "status" => "error",
                "message" => "File too large. Maximum size is 5MB."
            ]);
            exit;
        }

        $tmpName = is_string($photo['tmp_name'] ?? null) ? $photo['tmp_name'] : '';
        if (($photo['error'] ?? UPLOAD_ERR_NO_FILE) !== UPLOAD_ERR_OK || !is_uploaded_file($tmpName)) {
            echo json_encode(["status" => "error", "message" => "Failed to upload photo."]);
            exit;
        }

        // ✅ Content check: the bytes must really be a JPG/PNG/GIF, whatever the
        // name says. The stored extension follows the DETECTED type.
        $imageTypeExt = [IMAGETYPE_JPEG => 'jpg', IMAGETYPE_PNG => 'png', IMAGETYPE_GIF => 'gif'];
        $imageInfo = getimagesize($tmpName);
        $isImage = $imageInfo !== false && isset($imageTypeExt[$imageInfo[2]]);
        if ($isImage && class_exists('finfo')) {
            $mime = (new finfo(FILEINFO_MIME_TYPE))->file($tmpName);
            $isImage = in_array($mime, ['image/jpeg', 'image/png', 'image/gif'], true);
        }
        if (!$isImage) {
            echo json_encode([
                "status" => "error",
                "message" => "Invalid image file. Only JPG, PNG, and GIF allowed."
            ]);
            exit;
        }
        $fileExtension = $imageTypeExt[$imageInfo[2]];

        // ✅ Generate unique filename to prevent overwriting
        $uniqueFilename = $school_id . '_' . time() . '.' . $fileExtension;
        // ✅ Defence in depth: the target must be a direct child of uploads/
        $targetFile = safeChildPath($uploadDir, $uniqueFilename);
        if ($targetFile === null) {
            error_log("register_student: refused photo target outside uploads dir");
            echo json_encode(["status" => "error", "message" => "Failed to upload photo."]);
            exit;
        }

        // ✅ Move uploaded file
        if (move_uploaded_file($tmpName, $targetFile)) {
            $photoPath = $targetFile;
        } else {
            echo json_encode([
                "status" => "error",
                "message" => "Failed to upload photo. Check folder permissions."
            ]);
            exit;
        }
    }

    // ✅ Insert student with photo. mysqli may throw (PHP 8.1+ default report
    // mode) or return false — either way the detail goes to the log, never the client.
    $insertError = '';
    try {
        $stmt = $conn->prepare("
            INSERT INTO students
            (code, school_id, name, course, year_level, department, gender, status, visits, photo, time_date)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW())
        ");
        if ($stmt) {
            $stmt->bind_param("ssssssssis",
                $code, $school_id, $name, $course, $year_level, $department, $gender, $status, $visits, $photoPath
            );
            $inserted = $stmt->execute();
            if (!$inserted) $insertError = $stmt->error;
            $stmt->close();
        } else {
            $inserted = false;
            $insertError = $conn->error;
        }
    } catch (mysqli_sql_exception $e) {
        $inserted = false;
        $insertError = $e->getMessage();
    }

    if ($inserted) {
        echo json_encode([
            "status" => "success",
            "message" => "Student registered successfully",
            "photo_path" => $photoPath
        ]);
    } else {
        error_log("register_student: insert failed: " . $insertError);
        // Don't leave an orphaned photo behind for a row that was never written.
        if ($photoPath !== null && is_file($photoPath)) {
            unlink($photoPath);
        }
        echo json_encode([
            "status" => "error",
            "message" => "Failed to register student."
        ]);
    }
}
?>