<?php
error_reporting(E_ALL);
ini_set('display_errors',1);
header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $school_id  = isset($_POST['school_id']) ? trim($_POST['school_id']) : '';
    $name       = isset($_POST['name']) ? trim($_POST['name']) : '';
    $course     = isset($_POST['course']) ? trim($_POST['course']) : '';
    $year_level = isset($_POST['year_level']) ? trim($_POST['year_level']) : '';
    $department = isset($_POST['department']) ? trim($_POST['department']) : '';
    $code       = isset($_POST['code']) ? trim($_POST['code']) : '';
    $gender     = isset($_POST['gender']) ? trim($_POST['gender']) : '';
    $status     = isset($_POST['status']) ? trim($_POST['status']) : '';
    $visits     = isset($_POST['visits']) ? intval($_POST['visits']) : 0;
    
    // ✅ Require school_id and name
    if (empty($school_id) || empty($name)) {
        echo json_encode(["status" => "error", "message" => "School ID and Name are required."]);
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
        mkdir($uploadDir, 0777, true);
    }
    
    $photoPath = null;
    if (!empty($_FILES['photo']['name'])) {
        // ✅ Get file extension (more reliable than MIME type)
        $fileName = $_FILES['photo']['name'];
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
        if ($_FILES['photo']['size'] > $maxFileSize) {
            echo json_encode([
                "status" => "error", 
                "message" => "File too large. Maximum size is 5MB."
            ]);
            exit;
        }
        
        // ✅ Generate unique filename to prevent overwriting
        $uniqueFilename = $school_id . '_' . time() . '.' . $fileExtension;
        $targetFile = $uploadDir . $uniqueFilename;
        
        // ✅ Move uploaded file
        if (move_uploaded_file($_FILES['photo']['tmp_name'], $targetFile)) {
            $photoPath = $targetFile;
        } else {
            echo json_encode([
                "status" => "error", 
                "message" => "Failed to upload photo. Check folder permissions."
            ]);
            exit;
        }
    }
    
    // ✅ Insert student with photo
    $stmt = $conn->prepare("
        INSERT INTO students 
        (code, school_id, name, course, year_level, department, gender, status, visits, photo, time_date)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW())
    ");
    $stmt->bind_param("sssssssiss", 
        $code, $school_id, $name, $course, $year_level, $department, $gender, $status, $visits, $photoPath
    );
    
    if ($stmt->execute()) {
        echo json_encode([
            "status" => "success", 
            "message" => "Student registered successfully",
            "photo_path" => $photoPath
        ]);
    } else {
        echo json_encode([
            "status" => "error", 
            "message" => "Failed to register student: " . $stmt->error
        ]);
    }
    
    $stmt->close();
}
?>