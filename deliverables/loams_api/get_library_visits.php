<?php
header('Content-Type: application/json');
include 'db.php';

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "Database connection failed"]);
    exit;
}

$range = isset($_GET['range']) ? strtolower(trim($_GET['range'])) : 'today';
$start = isset($_GET['start']) ? trim($_GET['start']) : '';
$end   = isset($_GET['end'])   ? trim($_GET['end'])   : '';

// Resolve a half-open [start, end) datetime window (spec §5 date semantics).
if ($range === 'week') {
    // Current Mon–Sun calendar week as a half-open [start, end) datetime range
    // (spec §5). date('N') = 1(Mon)..7(Sun); server-local time is authoritative.
    $dow = (int)date('N');
    $monday = new DateTime('today');
    $monday->modify('-' . ($dow - 1) . ' days');
    $startDt = $monday->format('Y-m-d 00:00:00');
    $endDt   = (clone $monday)->modify('+7 days')->format('Y-m-d 00:00:00');
} elseif ($start !== '' && $end !== '') {
    $startDt = $start . ' 00:00:00';
    // Half-open [start, end+1day) so the entire $end day is included.
    $endDt   = (new DateTime($end))->modify('+1 day')->format('Y-m-d 00:00:00');
} else { // today (default)
    $startDt = (new DateTime('today'))->format('Y-m-d 00:00:00');
    $endDt   = (new DateTime('tomorrow'))->format('Y-m-d 00:00:00');
}

$stmt = $conn->prepare(
    "SELECT DATE(login_time) AS date, name, course, department,
            TIME_FORMAT(login_time, '%H:%i') AS time_in
     FROM library_visits
     WHERE login_time >= ? AND login_time < ?
     ORDER BY login_time DESC");
$stmt->bind_param("ss", $startDt, $endDt);
$stmt->execute();
$res = $stmt->get_result();

$visits = [];
while ($row = $res->fetch_assoc()) {
    $visits[] = $row;
}
$stmt->close();

echo json_encode(["status" => "success", "count" => count($visits), "visits" => $visits]);
$conn->close();
?>
