<?php
header('Content-Type: application/json');
include 'db.php';
include 'date_window.php';

if ($conn->connect_error) {
    echo json_encode(["status" => "error", "message" => "Database connection failed"]);
    exit;
}

$range = isset($_GET['range']) ? strtolower(trim($_GET['range'])) : 'today';
$start = isset($_GET['start']) ? trim($_GET['start']) : '';
$end   = isset($_GET['end'])   ? trim($_GET['end'])   : '';

/**
 * Strictly parse a 'Y-m-d' date string.
 * createFromFormat() is lenient (e.g. it will roll "2026-13-45" into a
 * different valid date while only recording a warning, not a failure), so
 * this also checks getLastErrors() and round-trips the formatted result
 * back against the input. Returns a DateTime on success, null on any
 * malformed input — never throws.
 */
function parse_strict_date($value) {
    $dt = DateTime::createFromFormat('Y-m-d', $value);
    if ($dt === false) {
        return null;
    }
    $errors = DateTime::getLastErrors();
    if ($errors !== false && ($errors['warning_count'] > 0 || $errors['error_count'] > 0)) {
        return null;
    }
    if ($dt->format('Y-m-d') !== $value) {
        return null;
    }
    return $dt;
}

// Resolve a half-open [start, end) datetime window (spec §5 date semantics).
if ($range === 'week') {
    [$startDt, $endDt] = week_window();
} elseif ($start !== '' && $end !== '') {
    $startParsed = parse_strict_date($start);
    $endParsed   = parse_strict_date($end);
    if ($startParsed === null || $endParsed === null) {
        echo json_encode(["status" => "error", "message" => "Invalid date parameters"]);
        exit;
    }
    $startDt = $startParsed->format('Y-m-d 00:00:00');
    // Half-open [start, end+1day) so the entire $end day is included.
    $endDt   = $endParsed->modify('+1 day')->format('Y-m-d 00:00:00');
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
