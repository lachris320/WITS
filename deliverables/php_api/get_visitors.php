<?php
header("Content-Type: application/json");
include "db.php";

// Decode JSON input safely
$input = json_decode(file_get_contents("php://input"), true);
if (!is_array($input)) $input = [];

// Extract filters
$search     = isset($input["search"]) ? trim($input["search"]) : "";
$dateType   = isset($input["date_type"]) ? strtolower(trim($input["date_type"])) : "all";
$startDate  = isset($input["start_date"]) ? $input["start_date"] : "";
$endDate    = isset($input["end_date"]) ? $input["end_date"] : "";

// Build query conditions
$where = "WHERE 1=1";

// --- Keyword filter ---
if ($search !== "") {
    $safe = $conn->real_escape_string($search);
    $where .= " AND (name LIKE '%$safe%' OR company LIKE '%$safe%' OR purpose LIKE '%$safe%')";
}

// --- Date filters ---
if ($dateType === "specific day" && $startDate !== "") {
    $where .= " AND DATE(time_in) = '$startDate'";
}
elseif ($dateType === "month" && $startDate !== "" && $endDate !== "") {
    // Month mode → full month range
    $where .= " AND DATE(time_in) BETWEEN '$startDate' AND '$endDate'";
}
elseif ($dateType === "date range" && $startDate !== "" && $endDate !== "") {
    $where .= " AND DATE(time_in) BETWEEN '$startDate' AND '$endDate'";
}

// --- Main query ---
$sql = "SELECT name, company, purpose, time_in 
        FROM visitors 
        $where 
        ORDER BY time_in DESC";

$result = $conn->query($sql);

if (!$result) {
    echo json_encode(["status" => "error", "message" => "Query failed: " . $conn->error]);
    exit;
}

// --- Collect rows ---
$logs = [];
while ($row = $result->fetch_assoc()) {
    $logs[] = $row;
}

// --- Output ---
echo json_encode([
    "status" => "success",
    "count" => count($logs),
    "visitors" => $logs
]);
?>
