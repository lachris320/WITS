<?php
header("Content-Type: application/json");
include "db.php";

$result = $conn->query("SELECT theme_mode, palette_json, logo_hash, updated_at
                        FROM branding_config WHERE id = 1");
if (!$result) {
    echo json_encode(["status" => "error", "message" => "Query failed: " . $conn->error]);
    exit;
}

$row = $result->fetch_assoc();
if (!$row) {
    echo json_encode(["status" => "success", "branding" => null]);
    exit;
}

$palette = json_decode($row["palette_json"], true);

echo json_encode([
    "status" => "success",
    "branding" => [
        "theme_mode" => $row["theme_mode"],
        "palette"    => is_array($palette) ? $palette : null,
        "logo_hash"  => $row["logo_hash"],
        "updated_at" => $row["updated_at"],
    ],
]);
?>
