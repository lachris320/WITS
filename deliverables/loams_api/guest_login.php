<?php
error_reporting(E_ALL);
ini_set('display_errors', 1);
header("Content-Type: application/json");

// Connect to DB
$servername = "localhost";
$username   = "root";      // adjust if needed
$password   = "";          // adjust if needed
$dbname     = "wits_app"; // adjust to your database name

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "DB connection failed"]);
    exit;
}

// Collect POST data safely
$name    = isset($_POST['name']) ? trim($_POST['name']) : "";
$company = isset($_POST['company']) ? trim($_POST['company']) : "";
$contact = isset($_POST['contact']) ? trim($_POST['contact']) : "";
$purpose = isset($_POST['purpose']) ? trim($_POST['purpose']) : "";

// ✅ Require name, company, and purpose
if (empty($name) || empty($company) || empty($purpose)) {
    echo json_encode([
        "status" => "error",
        "message" => "Name, Company, and Purpose are required."
    ]);
    exit;
}

// Insert into DB
$stmt = $conn->prepare("INSERT INTO visitors (name, company, contact, purpose, time_in) VALUES (?, ?, ?, ?, NOW())");

if (!$stmt) {
    echo json_encode([
        "status" => "error",
        "message" => "Prepare failed: " . $conn->error
    ]);
    exit;
}

// ✅ Bind the parameters
$stmt->bind_param("ssss", $name, $company, $contact, $purpose);

// ✅ Execute the query
if ($stmt->execute()) {
    echo json_encode(["status" => "success", "message" => "Guest logged successfully."]);
} else {
    echo json_encode([
        "status" => "error",
        "message" => "Insert failed: " . $stmt->error
    ]);
}

$stmt->close();
$conn->close();
?>
