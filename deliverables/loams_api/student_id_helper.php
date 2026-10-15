<?php
/**
 * Student-id / upload-path / ZIP-photo helpers. Functions only — no side effects on
 * include, so endpoints and tests can require_once it freely.
 */

/**
 * True iff $id is a well-formed school_id: 1-50 chars (the students.school_id
 * VARCHAR(50) width) of ASCII letters, digits, '_' or '-'. Covers every known
 * format (2023-00123, 21-1-0001, TEST-0001, SID_A, S0001) while excluding
 * path separators, dots, whitespace and NUL — so a valid id is always safe to
 * embed in a filename. Non-strings (e.g. an array from school_id[]=x) are
 * rejected. The D modifier stops '$' from matching before a trailing newline.
 */
function isValidSchoolId($id): bool
{
    return is_string($id) && preg_match('/^[A-Za-z0-9_-]{1,50}$/D', $id) === 1;
}

/**
 * True iff $filename would name a Windows reserved DOS device instead of a
 * file: its stem (the part before the first '.', trailing spaces/dots trimmed)
 * case-insensitively equals CON, PRN, AUX, NUL, COM0-9, LPT0-9 (also the
 * superscript COM¹²³ / LPT¹²³ forms) or CONIN$ / CONOUT$. So "CON.jpg",
 * "nul.png" and "aux.tar.gz" are reserved; "CONSOLE.jpg" and "COM10.jpg" are not.
 */
function isWindowsReservedName(string $filename): bool
{
    $dot  = strpos($filename, '.');
    $stem = rtrim($dot === false ? $filename : substr($filename, 0, $dot), ' .');
    // Byte-level match (no /u): the superscripts are the UTF-8 for U+00B9/B2/B3.
    return preg_match('/^(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|(COM|LPT)([0-9]|\xC2[\xB9\xB2\xB3]))$/iD', $stem) === 1;
}

/**
 * Join $dir and $filename, returning the path only if it names a DIRECT child
 * of an existing $dir; otherwise null (fail closed). $filename must be a bare
 * name — no '/', '\', ':' or NUL, not '.' / '..', and not a Windows reserved
 * device name (isWindowsReservedName(), e.g. "CON.jpg") — and the joined path's
 * parent must resolve (realpath) to the same directory as $dir. Defence in
 * depth behind isValidSchoolId() for any path built from client input.
 */
function safeChildPath($dir, $filename): ?string
{
    if (!is_string($dir) || !is_string($filename)) return null;
    if ($filename === '' || $filename === '.' || $filename === '..') return null;
    if (strpbrk($filename, "/\\:\0") !== false) return null;
    if (isWindowsReservedName($filename)) return null;

    $dirReal = realpath($dir);
    if ($dirReal === false || !is_dir($dirReal)) return null;

    $last = substr($dir, -1);
    $candidate = $dir . (($last === '/' || $last === '\\') ? '' : '/') . $filename;
    if (realpath(dirname($candidate)) !== $dirReal) return null;
    return $candidate;
}

/**
 * Collision-resistant stored-photo filename: "<school_id>_<unix ts>_<16 hex>.<ext>".
 * The school_id prefix is kept (humans and id-prefixed matching rely on it);
 * 64 random bits make two requests in the same second practically never pick
 * the same name, so one request's cleanup cannot delete another's photo.
 * Returns null (fail closed) for an invalid school_id or an extension other
 * than jpg / png / gif.
 */
function buildPhotoFilename($schoolId, $ext): ?string
{
    if (!isValidSchoolId($schoolId)) return null;
    if (!in_array($ext, ['jpg', 'png', 'gif'], true)) return null;
    return $schoolId . '_' . time() . '_' . bin2hex(random_bytes(8)) . '.' . $ext;
}

/**
 * Create a private (0700) per-request work dir for a ZIP import OUTSIDE the
 * web docroot: "<sys temp>/loams_zipimport_<32 hex>". Returns its path, or
 * null if it could not be created (the caller then imports without photos).
 */
function createZipImportTempDir(): ?string
{
    $base = rtrim(sys_get_temp_dir(), '/\\');
    for ($i = 0; $i < 3; $i++) {
        try {
            $dir = $base . DIRECTORY_SEPARATOR . 'loams_zipimport_' . bin2hex(random_bytes(16));
        } catch (Exception $e) {
            return null;
        }
        if (@mkdir($dir, 0700)) return $dir;
    }
    return null;
}

/**
 * Best-effort recursive delete of $dir (files first, then dirs). Symlinked
 * dirs are unlinked, never followed. Never throws and never prints, so it is
 * safe to run from a shutdown function.
 */
function removeDirTree(string $dir): void
{
    if (!is_dir($dir)) return;
    try {
        $it = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS),
            RecursiveIteratorIterator::CHILD_FIRST);
        foreach ($it as $f) {
            ($f->isDir() && !$f->isLink()) ? @rmdir($f->getPathname()) : @unlink($f->getPathname());
        }
    } catch (Throwable $e) {
        // fall through: still try to remove the (possibly non-empty) root
    }
    @rmdir($dir);
}

/**
 * Unpack the photo entries of an opened ZIP into $destDir (flat), WITHOUT
 * extractTo(). Only an entry whose basename (text after the last '/' or '\')
 * matches [A-Za-z0-9_-]{1,50}.(jpg|jpeg|png|gif) and is not a Windows reserved
 * device name is considered; directories, scripts, dotfiles and everything
 * else are skipped silently. Per entry at most $maxEntryBytes are read (the
 * header's declared size is not trusted; a larger entry is skipped), and the
 * bytes decompressed across all entries are capped at $maxTotalBytes
 * (extraction stops there). The content must be a real JPEG/PNG/GIF
 * (getimagesizefromstring) before it is written; a basename already written
 * in this call (case-insensitive) is skipped, so the first one in index order
 * wins. Files are created with 'x' (never overwrite). Returns the number of
 * photos written; 0 if $destDir is not an existing directory.
 */
function extractPhotoEntries(ZipArchive $zip, string $destDir,
                             int $maxEntryBytes = 10485760, int $maxTotalBytes = 536870912): int
{
    $dest = rtrim($destDir, '/\\');
    if ($dest === '' || !is_dir($dest)) return 0;

    $taken = [];   // lower-cased basenames written so far (NTFS is case-insensitive)
    $total = 0;    // bytes decompressed so far, accepted or not
    $count = 0;
    $entries = $zip->numFiles;
    for ($i = 0; $i < $entries; $i++) {
        $name = $zip->getNameIndex($i);
        if (!is_string($name)) continue;
        $base = preg_replace('#^.*[/\\\\]#s', '', $name);
        if (!is_string($base) || preg_match('/^[A-Za-z0-9_-]{1,50}\.(jpe?g|png|gif)$/iD', $base) !== 1) continue;
        if (isWindowsReservedName($base)) continue;
        $key = strtolower($base);
        if (isset($taken[$key])) continue;

        $budget = $maxTotalBytes - $total;
        if ($budget <= 0) break;
        // Read one byte past the applicable cap so an oversize entry is detected
        // from the bytes actually decompressed.
        $bytes = $zip->getFromIndex($i, min($maxEntryBytes, $budget) + 1);
        if (!is_string($bytes)) continue;            // corrupt / unreadable entry
        $len = strlen($bytes);
        $total += $len;
        if ($len > $budget) break;                   // total cap reached: stop
        if ($len > $maxEntryBytes || $len === 0) continue;   // oversize / empty: skip

        $info = @getimagesizefromstring($bytes);
        if (!is_array($info) || !in_array($info[2], [IMAGETYPE_JPEG, IMAGETYPE_PNG, IMAGETYPE_GIF], true)) continue;

        $path = $dest . DIRECTORY_SEPARATOR . $base;
        $fh = @fopen($path, 'xb');
        if ($fh === false) continue;
        $written = fwrite($fh, $bytes);
        fclose($fh);
        if ($written !== $len) {
            @unlink($path);
            continue;
        }
        $taken[$key] = true;
        $count++;
    }
    return $count;
}
