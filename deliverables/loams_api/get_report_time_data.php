<?php
header('Content-Type: application/json');
include 'db.php'; // must set $conn = new mysqli(...);

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    echo json_encode(['status' => 'error', 'message' => 'Invalid request method']);
    exit;
}

$raw = file_get_contents('php://input');
$data = json_decode($raw, true);
if (!is_array($data)) {
    echo json_encode(['status' => 'error', 'message' => 'Invalid input']);
    exit;
}

// Sanitize incoming filters — identical shape to get_report_data.php.
$department   = isset($data['department'])   ? trim($data['department'])   : '';
$course       = isset($data['course'])       ? trim($data['course'])       : '';
$durationType = isset($data['durationType']) ? trim($data['durationType']) : '';
$start        = isset($data['start'])        ? trim($data['start'])        : '';
$end          = isset($data['end'])          ? trim($data['end'])          : '';
$year         = isset($data['year'])         ? intval($data['year'])       : 0;
$semester     = isset($data['semester'])     ? trim($data['semester'])     : '';

// Build the shared WHERE clause + bound params ONCE, then reuse for both
// aggregations. Filter/WHERE logic is REUSED VERBATIM from get_report_data.php,
// INCLUDING the DATE()-vs-raw asymmetry (day/custom/month use DATE(v.login_time);
// semester compares the raw datetime against date-only bounds). Do NOT "fix" it —
// matching the roster endpoint verbatim is what makes byHour totals reconcile
// with the roster totals for the same filters (spec §3).
$where  = " WHERE 1=1";
$params = [];
$types  = "";

// Department filter (optional — empty/"All" = all departments)
if ($department !== '' && !in_array(strtolower($department), ['all', 'all departments'])) {
    $where .= " AND s.department = ?";
    $params[] = $department;
    $types   .= "s";
}

// Course filter
if ($course !== '' && !in_array(strtolower($course), ['all', 'all courses'])) {
    $where .= " AND s.course = ?";
    $params[] = $course;
    $types   .= "s";
}

// Duration filters (verbatim from get_report_data.php)
if (($durationType === 'day' || $durationType === 'custom' || $durationType === 'month')
    && $start !== '' && $end !== '') {
    $where .= " AND DATE(v.login_time) BETWEEN ? AND ?";
    $params[] = $start;
    $params[] = $end;
    $types  .= "ss";
}
elseif ($durationType === 'semester' && !empty($semester) && $year > 0) {
    $sem = strtolower($semester);
    if (strpos($sem, '1') !== false || stripos($sem, 'first') !== false) {
        $where .= " AND v.login_time BETWEEN ? AND ?";
        $params[] = "$year-06-01";
        $params[] = "$year-10-31";
        $types  .= "ss";
    } elseif (strpos($sem, '2') !== false || stripos($sem, 'second') !== false) {
        $where .= " AND v.login_time BETWEEN ? AND ?";
        $params[] = "$year-11-01";
        $params[] = ($year + 1) . "-03-31";
        $types  .= "ss";
    } elseif (stripos($sem, 'summer') !== false) {
        $where .= " AND v.login_time BETWEEN ? AND ?";
        $params[] = "$year-04-01";
        $params[] = "$year-05-31";
        $types  .= "ss";
    }
}

// Run a grouped COUNT aggregation with the shared params; returns
// [true, [bucket => count]] or [false, errorMessage]. Uses the same
// get_result/bind_result fallback as get_report_data.php.
function runAggregation($conn, $sql, $types, $params) {
    $stmt = $conn->prepare($sql);
    if ($stmt === false) {
        return [false, 'SQL prepare error: ' . $conn->error];
    }
    if (count($params) > 0) {
        $bindParams = [];
        $bindParams[] = &$types;
        for ($i = 0; $i < count($params); $i++) {
            $bindParams[] = &$params[$i];
        }
        call_user_func_array([$stmt, 'bind_param'], $bindParams);
    }
    if (!$stmt->execute()) {
        $err = $stmt->error;
        $stmt->close();
        return [false, 'Execute failed: ' . $err];
    }
    $out = [];
    if (method_exists($stmt, 'get_result')) {
        $result = $stmt->get_result();
        while ($r = $result->fetch_assoc()) {
            $out[(int)$r['bucket']] = (int)$r['cnt'];
        }
    } else {
        $bucket = null; $cnt = null;
        $stmt->bind_result($bucket, $cnt);
        while ($stmt->fetch()) {
            $out[(int)$bucket] = (int)$cnt;
        }
    }
    $stmt->close();
    return [true, $out];
}

// INNER JOIN so student-table dept/course filters apply and visits from
// since-deleted / unmatched students never appear (spec §3).
$base = " FROM library_visits v INNER JOIN students s ON s.school_id = v.student_id";

// byHour: GROUP BY HOUR(v.login_time)
$sqlHour = "SELECT HOUR(v.login_time) AS bucket, COUNT(*) AS cnt" . $base . $where
         . " GROUP BY HOUR(v.login_time)";
list($okH, $resH) = runAggregation($conn, $sqlHour, $types, $params);
if (!$okH) {
    echo json_encode(['status' => 'error', 'message' => $resH]);
    exit;
}

// byWeekday: GROUP BY DAYOFWEEK(v.login_time)
$sqlDow = "SELECT DAYOFWEEK(v.login_time) AS bucket, COUNT(*) AS cnt" . $base . $where
        . " GROUP BY DAYOFWEEK(v.login_time)";
list($okD, $resD) = runAggregation($conn, $sqlDow, $types, $params);
if (!$okD) {
    echo json_encode(['status' => 'error', 'message' => $resD]);
    exit;
}

// Densify to fixed-length arrays; any missing bucket = 0.
$byHour = array_fill(0, 24, 0);           // index 0..23 = hour of day
foreach ($resH as $h => $c) {
    if ($h >= 0 && $h <= 23) $byHour[$h] = $c;
}

// DAYOFWEEK 1=Sunday … 7=Saturday -> 0-based index i = dow - 1, so [0]=Sunday.
$byWeekday = array_fill(0, 7, 0);
foreach ($resD as $dow => $c) {
    $i = $dow - 1;
    if ($i >= 0 && $i <= 6) $byWeekday[$i] = $c;
}

echo json_encode([
    'status'    => 'success',
    'byHour'    => array_values($byHour),
    'byWeekday' => array_values($byWeekday),
]);
