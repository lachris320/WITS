<?php
/**
 * LOAMS Turnstile bridge poll endpoint (LOCALHOST ONLY)
 *
 * Consumed exclusively by loams-turnstile-bridge.ps1 running on the same PC as
 * WITS.exe. It hands out the oldest pending turnstile event so the bridge can
 * inject the card into the deployed app, and lets the bridge claim/release that
 * event atomically.
 *
 *   GET   (no action)          -> oldest pending event { id, card, created_at } | {}
 *                                  (pending = injected_at IS NULL AND fresh)
 *   POST  action=claim&id=N    -> set injected_at=NOW() if still unclaimed
 *                                  { claimed: true|false }
 *   POST  action=release&id=N  -> clear injected_at (pre-SendInput abort)
 *                                  { released: true|false }
 *
 * GET never mutates; state changes are POST only.
 */

declare(strict_types=1);

error_reporting(E_ALL);
ini_set('display_errors', '0');
ini_set('log_errors', '1');

header('Content-Type: application/json; charset=UTF-8');
header('Cache-Control: no-store');

// Only events younger than this are offered to the bridge for injection. Must
// stay well under the acceptance window in rfid_login.php (asymmetric margin).
const PULL_FRESHNESS_SECONDS = 5;

function jsonOut(array $payload, int $status = 200): never
{
    http_response_code($status);
    echo json_encode($payload, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    exit;
}

// -----------------------------------------------------------------------------
// Loopback-only guard. The bridge calls http://127.0.0.1/... so REMOTE_ADDR is
// the IPv4 loopback; ::1 is accepted too in case the stack resolves that way.
// -----------------------------------------------------------------------------
$remoteIp = $_SERVER['REMOTE_ADDR'] ?? '';
if (!in_array($remoteIp, ['127.0.0.1', '::1'], true)) {
    jsonOut(['error' => 'Forbidden'], 403);
}

$httpMethod = $_SERVER['REQUEST_METHOD'] ?? '';
$action     = isset($_REQUEST['action']) ? (string)$_REQUEST['action'] : '';

try {
    require_once __DIR__ . '/config.php';
    ini_set('display_errors', '0');
    mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

    $conn = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
    $conn->set_charset('utf8mb4');

    // --- Read: oldest pending event -----------------------------------------
    if ($httpMethod === 'GET' && $action === '') {
        $freshness = PULL_FRESHNESS_SECONDS;
        $stmt = $conn->prepare(
            'SELECT id, card, created_at FROM turnstile_events '
            . 'WHERE injected_at IS NULL AND created_at >= (NOW() - INTERVAL ? SECOND) '
            . 'ORDER BY id ASC LIMIT 1'
        );
        $stmt->bind_param('i', $freshness);
        $stmt->execute();
        $row = $stmt->get_result()->fetch_assoc();
        $stmt->close();
        $conn->close();

        if (!$row) {
            jsonOut(['id' => 0]); // nothing pending; bridge treats id<=0 as empty
        }
        jsonOut([
            'id'         => (int)$row['id'],
            'card'       => (string)$row['card'],
            'created_at' => (string)$row['created_at'],
        ]);
    }

    // --- Mutations: POST only ------------------------------------------------
    if ($httpMethod !== 'POST') {
        $conn->close();
        jsonOut(['error' => 'Method not allowed'], 405);
    }

    $id = isset($_POST['id']) ? (int)$_POST['id'] : 0;
    if ($id <= 0) {
        $conn->close();
        jsonOut(['error' => 'id required'], 400);
    }

    if ($action === 'claim') {
        // Win the claim only if still unclaimed. affected_rows tells us who won.
        $stmt = $conn->prepare(
            'UPDATE turnstile_events SET injected_at = NOW() '
            . 'WHERE id = ? AND injected_at IS NULL'
        );
        $stmt->bind_param('i', $id);
        $stmt->execute();
        $claimed = $stmt->affected_rows === 1;
        $stmt->close();
        $conn->close();
        jsonOut(['claimed' => $claimed]);
    }

    if ($action === 'release') {
        // Undo a claim ONLY while it has not been consumed. The bridge calls this
        // when it aborts before SendInput (foreground lost / event aged out).
        $stmt = $conn->prepare(
            'UPDATE turnstile_events SET injected_at = NULL '
            . 'WHERE id = ? AND consumed_at IS NULL'
        );
        $stmt->bind_param('i', $id);
        $stmt->execute();
        $released = $stmt->affected_rows === 1;
        $stmt->close();
        $conn->close();
        jsonOut(['released' => $released]);
    }

    $conn->close();
    jsonOut(['error' => 'Unknown action'], 400);
} catch (Throwable $e) {
    error_log('LOAMS turnstile_pull error: ' . $e->getMessage());
    jsonOut(['error' => 'Internal server error'], 500);
}
