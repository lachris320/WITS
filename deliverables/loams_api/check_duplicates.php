<?php
header("Content-Type: application/json");
include "db.php";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $data = json_decode(file_get_contents("php://input"), true);

    // ✅ Expect {"school_ids": ["...","..."]}
    if (!isset($data["school_ids"]) || !is_array($data["school_ids"])) {
        echo json_encode(["status" => "error", "message" => "Invalid input"]);
        exit;
    }

    $duplicates = [];
    foreach ($data["school_ids"] as $school_id) {
        $stmt = $conn->prepare("SELECT id FROM students WHERE school_id = ?");
        $stmt->bind_param("s", $school_id);
        $stmt->execute();
        $result = $stmt->get_result();
        if ($result->num_rows > 0) {
            $duplicates[] = $school_id;
        }
        $stmt->close();
    }

    echo json_encode([
        "status" => "success",
        "duplicates" => $duplicates
    ]);
}
?>
