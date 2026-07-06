<?php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";

requireAdminAuth($conn);

$theme_mode = isset($_POST['theme_mode']) ? strtolower(trim($_POST['theme_mode'])) : '';
$palette    = isset($_POST['palette']) ? $_POST['palette'] : '';
$logo_hash  = isset($_POST['logo_hash']) ? trim($_POST['logo_hash']) : '';

if (!in_array($theme_mode, ['auto', 'manual'], true)) {
    http_response_code(400);
    echo json_encode(["status" => "error", "message" => "theme_mode must be 'auto' or 'manual'"]);
    exit;
}

$decoded = json_decode($palette, true);
if (!is_array($decoded)) {
    http_response_code(400);
    echo json_encode(["status" => "error", "message" => "palette must be a JSON object"]);
    exit;
}

if ($logo_hash !== '' && !preg_match('/^[0-9a-f]{64}$/', $logo_hash)) {
    http_response_code(400);
    echo json_encode(["status" => "error", "message" => "logo_hash must be a 64-character SHA-256 hex string"]);
    exit;
}

$stmt = $conn->prepare("INSERT INTO branding_config (id, theme_mode, palette_json, logo_hash)
                        VALUES (1, ?, ?, ?)
                        ON DUPLICATE KEY UPDATE
                            theme_mode   = VALUES(theme_mode),
                            palette_json = VALUES(palette_json),
                            logo_hash    = VALUES(logo_hash)");
if (!$stmt) {
    http_response_code(500);
    echo json_encode(["status" => "error", "message" => "Prepare failed: " . $conn->error]);
    exit;
}

$stmt->bind_param("sss", $theme_mode, $palette, $logo_hash);
if (!$stmt->execute()) {
    http_response_code(500);
    echo json_encode(["status" => "error", "message" => "Save failed: " . $stmt->error]);
    exit;
}
$stmt->close();

echo json_encode(["status" => "success"]);
?>
