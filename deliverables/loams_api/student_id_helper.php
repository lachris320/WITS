<?php
/**
 * Student-id / upload-path helpers. Functions only — no side effects on
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
 * Join $dir and $filename, returning the path only if it names a DIRECT child
 * of an existing $dir; otherwise null (fail closed). $filename must be a bare
 * name — no '/', '\', ':' or NUL, and not '.' / '..' — and the joined path's
 * parent must resolve (realpath) to the same directory as $dir. Defence in
 * depth behind isValidSchoolId() for any path built from client input.
 */
function safeChildPath($dir, $filename): ?string
{
    if (!is_string($dir) || !is_string($filename)) return null;
    if ($filename === '' || $filename === '.' || $filename === '..') return null;
    if (strpbrk($filename, "/\\:\0") !== false) return null;

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
