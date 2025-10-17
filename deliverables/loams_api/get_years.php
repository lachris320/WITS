<?php
header('Content-Type: application/json');
$conn = new mysqli("localhost", "root", "", "wits_app");

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => $conn->connect_error]);
    exit;
}

// Query distinct valid years from login_time
$sql = "SELECT DISTINCT YEAR(login_time) AS year 
        FROM library_visits 
        WHERE login_time IS NOT NULL 
        AND login_time != '0000-00-00 00:00:00'
        ORDER BY year DESC";

$result = $conn->query($sql);

$years = [];
while ($row = $result->fetch_assoc()) {
    if ((int)$row['year'] > 0) {   // ignore invalid/zero years
        $years[] = (int)$row['year'];
    }
}

// ✅ If no years found, fallback to current year
if (empty($years)) {
    $years[] = (int)date("Y");
}

echo json_encode(["status" => "success", "years" => $years]);
$conn->close();
