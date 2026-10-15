<?php
error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);
// Installed BEFORE db.php: a DB connect failure throws during the include (with
// display_errors back on) — log it, send HTTP 500 + generic JSON, leak nothing.
require_once __DIR__ . "/json_error_guard.php";
installJsonErrorGuard("upload_students_zip");
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
require_once __DIR__ . "/student_id_helper.php";
// db.php re-enables display_errors; turn it back off so no warning/exception
// text can reach the JSON response.
ini_set('display_errors', 0);
requireAdminAuth($conn);   // 401 before any DB read, ZIP extract, or insert

// Read a row field as a trimmed string. JSON arrays/objects (non-scalars) read
// as '' instead of making trim() throw TypeError mid-loop; scalars are cast.
function rowString($row, $key) {
    return (isset($row[$key]) && is_scalar($row[$key])) ? trim((string) $row[$key]) : '';
}

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    echo json_encode(["status" => "error", "message" => "POST required"]);
    exit;
}

// === 1. Client-sent, header-mapped rows (JSON array of 7-key objects) ===
$rowsJson = isset($_POST['rows']) ? $_POST['rows'] : '';
$rows = json_decode($rowsJson, true);
if (!is_array($rows)) {
    echo json_encode(["status" => "error", "message" => "Invalid rows payload."]);
    exit;
}

// === 2. Skip-set from skip_ids (comma-joined) ===
$skipSet = [];
if (!empty($_POST['skip_ids'])) {
    foreach (explode(',', $_POST['skip_ids']) as $sid) {
        $sid = trim($sid);
        if ($sid !== '') $skipSet[$sid] = true;
    }
}

// === 3. Optional photos ZIP — unpacked ONCE, before the row loop (core ext-zip) ===
// Nothing from the ZIP ever lands in the web docroot: the upload is moved into
// a private per-request dir under the system temp dir, under a SERVER-chosen
// name (the client filename is never used on disk), and only allow-listed
// image entries are unpacked from it (extractPhotoEntries — no extractTo()).
// The whole dir is removed at shutdown, so early exits and fatals clean up
// too, and a later import can never match a photo left over from this one.
// Every failure here is non-fatal: the import proceeds with photo = NULL, and
// is logged (generic text, codes and counts only — never client data) so an
// admin asking "why no photos?" can find the reason.
$photoFiles = [];      // unpacked photos, listed ONCE (sorted) for matchPhotoForId()
$zipExtracted = false;

$zipUpload = isset($_FILES['photos_zip']) && is_array($_FILES['photos_zip']) ? $_FILES['photos_zip'] : null;
$zipErr = $zipUpload !== null ? ($zipUpload['error'] ?? UPLOAD_ERR_NO_FILE) : UPLOAD_ERR_NO_FILE;
if ($zipErr !== UPLOAD_ERR_OK && $zipErr !== UPLOAD_ERR_NO_FILE) {
    error_log("upload_students_zip: photos ZIP upload error code " . (is_int($zipErr) ? $zipErr : -1)
        . "; importing without photos");
}
if ($zipErr === UPLOAD_ERR_OK
        && is_string($zipUpload['tmp_name'] ?? null)
        && is_uploaded_file($zipUpload['tmp_name'])) {
    $workDir = createZipImportTempDir();
    if ($workDir === null) {
        error_log("upload_students_zip: could not create temp dir; importing without photos");
    } else {
        // Fail closed if the system temp dir resolves into the API tree (it is
        // served by the web server): nothing from the ZIP may land there.
        $workReal = realpath($workDir);
        $apiReal  = realpath(__DIR__);
        if ($workReal === false || $apiReal === false || isPathInside($workReal, $apiReal)) {
            error_log("upload_students_zip: temp dir is inside the API tree (or unresolvable); importing without photos");
            removeDirTree($workDir);
            $workDir = null;
        }
    }
    if ($workDir !== null) {
        $zip = new ZipArchive;
        $zipOpen = false;
        // Registered after installJsonErrorGuard()'s handler, so on a fatal the
        // JSON error is sent first, then this runs. A fatal mid-extraction skips
        // the finally below, and Windows cannot delete upload.zip while the
        // archive is still open — so close it here first if needed.
        register_shutdown_function(function () use (&$zip, &$zipOpen, $workDir) {
            if ($zipOpen) {
                $zipOpen = false;
                try { @$zip->close(); } catch (Throwable $e) { /* already closed / invalid */ }
            }
            removeDirTree($workDir);
        });
        $zipPath = $workDir . DIRECTORY_SEPARATOR . "upload.zip";
        if (!move_uploaded_file($zipUpload['tmp_name'], $zipPath)) {
            error_log("upload_students_zip: move_uploaded_file failed; importing without photos");
        } else {
            $openRc = $zip->open($zipPath);
            if ($openRc !== TRUE) {
                error_log("upload_students_zip: ZIP open failed (code " . (int) $openRc . "); importing without photos");
            } else {
                $zipOpen = true;
                $extractDir = $workDir . DIRECTORY_SEPARATOR . "photos";
                try {
                    if (@mkdir($extractDir, 0700)) {
                        $zipStats = null;
                        // Positional, not a named arg: the API must still parse on PHP 7.4.
                        extractPhotoEntries($zip, $extractDir, ZIP_PHOTO_MAX_ENTRY_BYTES,
                            ZIP_PHOTO_MAX_TOTAL_BYTES, ZIP_PHOTO_MAX_ENTRIES, $zipStats);
                        if ($zipStats['too_many_entries']) {
                            error_log("upload_students_zip: ZIP has more than " . ZIP_PHOTO_MAX_ENTRIES
                                . " entries; no photos unpacked");
                        }
                        error_log("upload_students_zip: ZIP photos: {$zipStats['written']} written, "
                            . "{$zipStats['skipped_name']} skipped (name not allowed), "
                            . "{$zipStats['skipped_other']} skipped (duplicate/oversize/not an image)"
                            . ($zipStats['total_cap_hit'] ? ", total size cap reached" : ""));
                        $photoFiles = listPhotoFiles($extractDir);
                        $zipExtracted = true;
                    } else {
                        error_log("upload_students_zip: could not create photo dir; importing without photos");
                    }
                } finally {
                    $zipOpen = false;
                    $zip->close();   // release the handle so the shutdown cleanup can delete it
                }
            }
        }
    }
}

if (!is_dir("uploads/students/")) mkdir("uploads/students/", 0755, true);

// === 4. Per-row insert ===
$success_count = 0;
$skipped_count = 0;
$error_count   = 0;
$copiedBytes      = 0;       // photo bytes copied into uploads/students/ so far
$copyBudgetLogged = false;   // log the exhausted budget once, not per row

$stmt = $conn->prepare("INSERT INTO students
    (school_id, name, course, department, year_level, gender, status, photo)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

// Used only when a row inserted fine but its photo copy then failed; keyed by
// the new row's primary key so it can only ever touch the row just inserted.
$clearPhotoStmt = $conn->prepare("UPDATE students SET photo = NULL WHERE id = ?");

if (!$stmt || !$clearPhotoStmt) {
    echo json_encode(["status" => "error", "message" => "Import failed to initialize."]);
    exit;
}

foreach ($rows as $row) {
    // Every field is read as a string: a non-scalar (JSON array/object) reads
    // as '' — so a non-scalar school_id or name is counted as an error below.
    $school_id  = rowString($row, 'school_id');
    $name       = rowString($row, 'name');
    $course     = rowString($row, 'course');
    $department = rowString($row, 'department');
    $year_level = rowString($row, 'year_level');
    $gender     = rowString($row, 'gender');
    $status     = rowString($row, 'status');

    // Server-side re-validate: school_id + name required (the data guarantee).
    if ($school_id === '' || $name === '') {
        $error_count++;
        continue;
    }
    // school_id is embedded in a photo path — allow only [A-Za-z0-9_-]{1,50}.
    if (!isValidSchoolId($school_id)) {
        $error_count++;
        continue;
    }
    // Client-resolved duplicates are skipped here.
    if (isset($skipSet[$school_id])) {
        $skipped_count++;
        continue;
    }

    // Photo comes ONLY from a whole-token ZIP match (never from a file column).
    // Only the target path is resolved here; the file is written AFTER the
    // insert succeeds, so a failed row (e.g. a duplicate school_id) can never
    // overwrite an existing student's photo.
    $match = null;
    $matchSize = 0;
    $photoPath = null;
    if ($zipExtracted) {
        $match = matchPhotoForId($photoFiles, $school_id);
        if ($match !== null) {
            // One photo can match several rows and is copied once per row, so
            // the bytes copied into the docroot share the extraction cap. Over
            // budget: the row is still imported, just with photo = NULL.
            $size = @filesize($match);
            $matchSize = ($size === false) ? -1 : $size;
            if (!photoCopyFits($matchSize, $copiedBytes, ZIP_PHOTO_MAX_TOTAL_BYTES)) {
                if (!$copyBudgetLogged) {
                    error_log("upload_students_zip: photo copy budget exhausted; remaining matched rows get no photo");
                    $copyBudgetLogged = true;
                }
                $match = null;
            } else {
                // Defence in depth: must be a direct child of uploads/students/.
                $photoPath = safeChildPath("uploads/students/", $school_id . ".jpg");
            }
        }
    }

    $stmt->bind_param("ssssssss",
        $school_id, $name, $course, $department,
        $year_level, $gender, $status, $photoPath);

    // PHP 8.1+ mysqli throws on a failed execute (e.g. a duplicate school_id
    // within this upload, or one already in the DB but not in skip_ids). Count
    // the row as an error and keep going instead of aborting the whole import.
    try {
        $inserted = $stmt->execute();
    } catch (mysqli_sql_exception $e) {
        error_log("upload_students_zip: row insert failed: " . $e->getMessage());
        $inserted = false;
    }
    if (!$inserted) {
        $error_count++;
        continue;
    }
    $success_count++;
    $newId = (int) $stmt->insert_id;

    // Row is in: now write its photo. If the copy fails, clear the photo column
    // so the row never points at a missing file.
    if ($photoPath !== null) {
        if (copy($match, $photoPath)) {
            $copiedBytes += $matchSize;
        } else {
            error_log("upload_students_zip: photo copy failed; clearing photo for inserted row");
            try {
                $clearPhotoStmt->bind_param("i", $newId);
                $clearPhotoStmt->execute();
            } catch (mysqli_sql_exception $e) {
                error_log("upload_students_zip: clearing photo failed: " . $e->getMessage());
            }
        }
    }
}

$stmt->close();
$clearPhotoStmt->close();

echo json_encode([
    "status"        => "success",
    "success_count" => $success_count,
    "skipped_count" => $skipped_count,
    "error_count"   => $error_count,
    "message"       => "Imported $success_count, skipped $skipped_count, failed $error_count."
]);
?>
