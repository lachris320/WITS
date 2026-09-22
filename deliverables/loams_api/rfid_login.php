<?php
header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    http_response_code(405);
    echo json_encode(["status" => "error", "message" => "Method not allowed"]);
    exit;
}

$rfid_id = isset($_POST['rfid_id']) ? trim($_POST['rfid_id']) : '';

if (empty($rfid_id)) {
    echo json_encode(["status" => "error", "message" => "RFID ID is required"]);
    exit;
}

// Seconds a turnstile-injected token stays acceptable for de-duplication. This is
// deliberately LONGER than the bridge's inject window (turnstile_pull.php ~5 s):
// the asymmetric margin means a token injected near the bridge's cutoff is still
// recognised here, so it can never slip past and cause a second attendance write.
const TURNSTILE_TOKEN_ACCEPT_SECONDS = 15;

// Start transaction for consistency
$conn->autocommit(FALSE);

try {
    // Resolve the student by RFID code first, then fall back to school_id. The
    // turnstile (turnstile.php) authorizes on the SAME code->school_id fallback,
    // so any card the gate admits can also be resolved here for display.
    $matchColumn = 'code';
    $stmt = $conn->prepare("SELECT * FROM students WHERE code = ? LIMIT 1");
    if (!$stmt) throw new Exception("Prepare failed: " . $conn->error);
    $stmt->bind_param("s", $rfid_id);
    $stmt->execute();
    $result = $stmt->get_result();
    $student = $result->fetch_assoc();
    $stmt->close();

    if (!$student) {
        $matchColumn = 'school_id';
        $stmt = $conn->prepare("SELECT * FROM students WHERE school_id = ? LIMIT 1");
        if (!$stmt) throw new Exception("Prepare failed: " . $conn->error);
        $stmt->bind_param("s", $rfid_id);
        $stmt->execute();
        $result = $stmt->get_result();
        $student = $result->fetch_assoc();
        $stmt->close();
    }

    if ($student) {
        // Turnstile de-duplication: if this scan corresponds to a gate entry the
        // turnstile already recorded (an injected, not-yet-consumed token for this
        // card within the window), consume the token and SKIP the attendance write.
        // Oldest-first + FOR UPDATE handles two rapid swipes of the same card in
        // order. Attendance is left to turnstile.php, the authoritative writer.
        $acceptWindow = TURNSTILE_TOKEN_ACCEPT_SECONDS;
        $tokenStmt = $conn->prepare(
            "SELECT id FROM turnstile_events
             WHERE card = ?
               AND injected_at IS NOT NULL
               AND consumed_at IS NULL
               AND created_at >= (NOW() - INTERVAL ? SECOND)
             ORDER BY id ASC LIMIT 1 FOR UPDATE"
        );
        if (!$tokenStmt) throw new Exception("Token prepare failed: " . $conn->error);
        $tokenStmt->bind_param("si", $rfid_id, $acceptWindow);
        $tokenStmt->execute();
        $token = $tokenStmt->get_result()->fetch_assoc();
        $tokenStmt->close();

        $suppressAttendance = false;
        if ($token) {
            $consumeStmt = $conn->prepare(
                "UPDATE turnstile_events SET consumed_at = NOW() WHERE id = ?"
            );
            if (!$consumeStmt) throw new Exception("Consume prepare failed: " . $conn->error);
            $consumeStmt->bind_param("i", $token['id']);
            if (!$consumeStmt->execute()) {
                throw new Exception("Consume failed: " . $consumeStmt->error);
            }
            $consumeStmt->close();
            $suppressAttendance = true;
        }

        if (!$suppressAttendance) {
            // Insert login record into library_visits table
            $logStmt = $conn->prepare("
                INSERT INTO library_visits
                (student_id, course, login_time, yearlog, year_level, department, name, gender, status, photo)
                VALUES (?, ?, NOW(), YEAR(CURDATE()), ?, ?, ?, ?, ?, ?)
            ");
            if (!$logStmt) throw new Exception("Log prepare failed: " . $conn->error);

            $logStmt->bind_param(
                "ssssssss",
                $student['school_id'],
                $student['course'],
                $student['year_level'],
                $student['department'],
                $student['name'],
                $student['gender'],
                $student['status'],
                $student['photo']
            );

            if (!$logStmt->execute()) {
                throw new Exception("Log insert failed: " . $logStmt->error);
            }
            $logStmt->close();

            // Increment visits counter on whichever column matched the student.
            $updateSql = $matchColumn === 'school_id'
                ? "UPDATE students SET visits = COALESCE(visits, 0) + 1 WHERE school_id = ?"
                : "UPDATE students SET visits = COALESCE(visits, 0) + 1 WHERE code = ?";
            $updateStmt = $conn->prepare($updateSql);
            if (!$updateStmt) throw new Exception("Update prepare failed: " . $conn->error);

            $updateStmt->bind_param("s", $rfid_id);
            if (!$updateStmt->execute()) {
                throw new Exception("Update failed: " . $updateStmt->error);
            }
            $updateStmt->close();
        }

        // Build photo URL safely (same corrected resolution as student_login.php).
        // Photos are stored relative to THIS script's directory (loams_api/uploads/…);
        // the existence check must resolve against __DIR__, not DOCUMENT_ROOT (which
        // pointed one level too high and made every student fall back to default.jpg).
        $protocol = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off') ? "https://" : "http://";
        $host = $_SERVER['HTTP_HOST'];
        $scriptDir = rtrim(dirname($_SERVER['SCRIPT_NAME']), '/\\');
        $baseURL = $protocol . $host . $scriptDir . '/';

        $photoRelPath = $student['photo'] ?: '';
        $photoAbsPath = __DIR__ . '/' . ltrim($photoRelPath, '/');

        if ($photoRelPath && file_exists($photoAbsPath)) {
            $photoURL = $baseURL . $photoRelPath;
        } else {
            $photoURL = $baseURL . 'uploads/default.jpg';
        }

        $student['photo_url'] = $photoURL;

        $conn->commit();
        echo json_encode(["status" => "success", "student" => $student]);

    } else {
        $conn->rollback();
        echo json_encode(["status" => "error", "message" => "RFID not registered"]);
    }

} catch (Exception $e) {
    $conn->rollback();
    error_log("RFID login error: " . $e->getMessage());
    echo json_encode(["status" => "error", "message" => "Internal server error"]);
}

$conn->close();
?>
