<?php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401 before any read

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    echo json_encode(["status" => "error", "message" => "POST required"]);
    exit;
}

// school_ids arrives as a JSON-array string in a form field (moved off the raw
// JSON body so requireAdminAuth can read admin_key from $_POST).
$idsJson = isset($_POST['school_ids']) ? $_POST['school_ids'] : '';
$school_ids = json_decode($idsJson, true);

if (!is_array($school_ids)) {
    echo json_encode(["status" => "error", "message" => "Invalid input"]);
    exit;
}

$duplicates = [];
$stmt = $conn->prepare("SELECT id FROM students WHERE school_id = ?");
foreach ($school_ids as $school_id) {
    $stmt->bind_param("s", $school_id);
    $stmt->execute();
    $result = $stmt->get_result();
    if ($result->num_rows > 0) {
        $duplicates[] = $school_id;
    }
}
$stmt->close();

echo json_encode([
    "status" => "success",
    "duplicates" => $duplicates
]);
?>
