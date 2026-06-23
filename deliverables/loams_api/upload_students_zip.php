<?php
include 'db.php'; // adjust to your DB connection path

use PhpOffice\PhpSpreadsheet\IOFactory;
require 'vendor/autoload.php'; // PhpSpreadsheet autoload

$uploadDir = "uploads/temp/";
if (!is_dir($uploadDir)) mkdir($uploadDir, 0777, true);

// === 1. Save uploaded Excel file ===
if (empty($_FILES['excel']['tmp_name'])) {
    die("No Excel file uploaded.");
}

$excelPath = $uploadDir . basename($_FILES['excel']['name']);
move_uploaded_file($_FILES['excel']['tmp_name'], $excelPath);

// === 2. Extract ZIP if provided ===
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
    } else {
        echo "Warning: Failed to extract ZIP file.\n";
    }
}

// === 3. Parse Excel ===
$spreadsheet = IOFactory::load($excelPath);
$sheet = $spreadsheet->getActiveSheet();
$rows = $sheet->toArray(null, true, true, true);

// === 4. Loop through rows ===
$count = 0;
$photosMatched = 0;

if (!is_dir("uploads/students/")) mkdir("uploads/students/", 0777, true);

foreach ($rows as $i => $row) {
    if ($i == 1) continue; // Skip header row

    $school_id = trim($row['A']);
    $name = trim($row['B']);
    $course = trim($row['C']);
    $department = trim($row['D']);
    $year_level = trim($row['E']);
    $gender = trim($row['F']);
    $status = trim($row['G']);

    $photoPath = null;

    // Try to match photo if ZIP provided
    if ($zipExtracted && !empty($school_id)) {
        $candidates = glob($photoDir . "*$school_id*.*");
        if (count($candidates) > 0) {
            $photoSrc = $candidates[0];
            $targetPhoto = "uploads/students/" . $school_id . ".jpg";
            copy($photoSrc, $targetPhoto);
            $photoPath = $targetPhoto;
            $photosMatched++;
        }
    }

    // Insert into DB
    $stmt = $conn->prepare("INSERT INTO students
        (school_id, name, course, department, year_level, gender, status, photo)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    $stmt->bind_param("ssssssss",
        $school_id, $name, $course, $department,
        $year_level, $gender, $status, $photoPath);
    $stmt->execute();

    $count++;
}

echo "✅ Upload complete!\n";
echo "Total students inserted: $count\n";
if ($zipExtracted) echo "Photos matched and saved: $photosMatched\n";
else echo "No photo ZIP uploaded.\n";
?>
