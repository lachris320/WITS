<?php
error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401 before any DB read, ZIP extract, or insert

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    echo json_encode(["status" => "error", "message" => "POST required"]);
    exit;
}

// === 1. Client-sent, header-mapped rows (JSON array of 7-key objects) ===
$rowsJson = isset($_POST['rows']) ? $_POST['rows'] : '';
$rows = json_decode($rowsJson, true);
if (!is_array($rows)) {
    echo json_encode(["status" => "error", "message" => "Invalid rows payload."]);
    exit;
}

// === 2. Skip-set from skip_ids (comma-joined) ===
$skipSet = [];
if (!empty($_POST['skip_ids'])) {
    foreach (explode(',', $_POST['skip_ids']) as $sid) {
        $sid = trim($sid);
        if ($sid !== '') $skipSet[$sid] = true;
    }
}

// === 3. Optional photos ZIP — extracted ONCE, before the row loop (core ext-zip) ===
$uploadDir = "uploads/temp/";
if (!is_dir($uploadDir)) mkdir($uploadDir, 0777, true);
$photoDir = $uploadDir . "photos/";
$zipExtracted = false;

if (!empty($_FILES['photos_zip']['tmp_name'])) {
    $zipPath = $uploadDir . basename($_FILES['photos_zip']['name']);
    move_uploaded_file($_FILES['photos_zip']['tmp_name'], $zipPath);
    $zip = new ZipArchive;
    if ($zip->open($zipPath) === TRUE) {
        $zip->extractTo($photoDir);
        $zip->close();
        $zipExtracted = true;
    }
    // A failed ZIP open is non-fatal: import proceeds with photo = NULL.
}

if (!is_dir("uploads/students/")) mkdir("uploads/students/", 0777, true);

// === 4. Per-row insert ===
$success_count = 0;
$skipped_count = 0;
$error_count   = 0;

$stmt = $conn->prepare("INSERT INTO students
    (school_id, name, course, department, year_level, gender, status, photo)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

if (!$stmt) {
    echo json_encode(["status" => "error", "message" => "Prepare failed: " . $conn->error]);
    exit;
}

foreach ($rows as $row) {
    $school_id  = isset($row['school_id'])  ? trim($row['school_id'])  : '';
    $name       = isset($row['name'])       ? trim($row['name'])       : '';
    $course     = isset($row['course'])     ? trim($row['course'])     : '';
    $department = isset($row['department'])  ? trim($row['department']) : '';
    $year_level = isset($row['year_level'])  ? trim($row['year_level']) : '';
    $gender     = isset($row['gender'])      ? trim($row['gender'])     : '';
    $status     = isset($row['status'])      ? trim($row['status'])     : '';

    // Server-side re-validate: school_id + name required (the data guarantee).
    if ($school_id === '' || $name === '') {
        $error_count++;
        continue;
    }
    // Client-resolved duplicates are skipped here.
    if (isset($skipSet[$school_id])) {
        $skipped_count++;
        continue;
    }

    // Photo comes ONLY from the ZIP match (never from a file column).
    $photoPath = null;
    if ($zipExtracted) {
        $candidates = glob($photoDir . "*" . $school_id . "*.*");
        if ($candidates && count($candidates) > 0) {
            $targetPhoto = "uploads/students/" . $school_id . ".jpg";
            if (copy($candidates[0], $targetPhoto)) {
                $photoPath = $targetPhoto;
            }
        }
    }

    $stmt->bind_param("ssssssss",
        $school_id, $name, $course, $department,
        $year_level, $gender, $status, $photoPath);

    if ($stmt->execute()) {
        $success_count++;
    } else {
        $error_count++;
    }
}

$stmt->close();

echo json_encode([
    "status"        => "success",
    "success_count" => $success_count,
    "skipped_count" => $skipped_count,
    "error_count"   => $error_count,
    "message"       => "Imported $success_count, skipped $skipped_count, failed $error_count."
]);
?>
