<?php
header('Content-Type: application/json');
include 'db.php';
include 'date_window.php';

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "Database connection failed"]);
    exit;
}

// Current Mon–Sun calendar week as a half-open [weekStart, weekEnd) datetime range
// (spec §5). Shared with get_library_visits.php via date_window.php so the two
// "this week" definitions cannot silently drift apart.
[$weekStart, $weekEnd] = week_window();

$response = ["status" => "success"];

// --- Visitors today ---
$stmt = $conn->prepare("SELECT COUNT(*) FROM library_visits WHERE DATE(login_time) = CURDATE()");
$stmt->execute();
$stmt->bind_result($today);
$stmt->fetch();
$stmt->close();
$response["today"] = intval($today);

// --- Visitors this week [weekStart, weekEnd) ---
$stmt = $conn->prepare("SELECT COUNT(*) FROM library_visits WHERE login_time >= ? AND login_time < ?");
$stmt->bind_param("ss", $weekStart, $weekEnd);
$stmt->execute();
$stmt->bind_result($week);
$stmt->fetch();
$stmt->close();
$response["week"] = intval($week);

// --- Registered students ---
$stmt = $conn->prepare("SELECT COUNT(*) FROM students");
$stmt->execute();
$stmt->bind_result($students);
$stmt->fetch();
$stmt->close();
$response["students"] = intval($students);

// --- Hourly histogram for today ---
$hourly = [];
$stmt = $conn->prepare(
    "SELECT HOUR(login_time) AS h, COUNT(*) AS c
     FROM library_visits
     WHERE DATE(login_time) = CURDATE()
     GROUP BY HOUR(login_time)
     ORDER BY h");
$stmt->execute();
$res = $stmt->get_result();
while ($row = $res->fetch_assoc()) {
    $hourly[] = ["hour" => intval($row["h"]), "count" => intval($row["c"])];
}
$stmt->close();
$response["hourly"] = $hourly;

// --- Department breakdown for the current week ---
$departments = [];
$stmt = $conn->prepare(
    "SELECT department AS name, COUNT(*) AS c
     FROM library_visits
     WHERE login_time >= ? AND login_time < ?
     GROUP BY department
     ORDER BY c DESC");
$stmt->bind_param("ss", $weekStart, $weekEnd);
$stmt->execute();
$res = $stmt->get_result();
while ($row = $res->fetch_assoc()) {
    $departments[] = ["name" => $row["name"], "count" => intval($row["c"])];
}
$stmt->close();
$response["departments"] = $departments;

echo json_encode($response);
$conn->close();
?>
