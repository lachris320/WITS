# Phase 4a.3 — Import Endpoints Contract

> Date: 2026-08-11 · Track 4a (Database + Import) · **BREAKING** — deploy WITH the client.
> `<ADMIN_KEY>` denotes the RAM-only admin key. No real keys or student PII here.

Both endpoints are now guarded by `requireAdminAuth($conn)` (bcrypt-verified
`$_POST['admin_key']`) and reject a missing/invalid key with HTTP **401**. An old
client that does not thread `admin_key` will 401 — ship client + endpoints together.

## check_duplicates.php

**Before** — JSON body, unguarded:

    POST check_duplicates.php
    Content-Type: application/json
    { "school_ids": ["21-1-0001", "21-1-0002"] }
    -> 200 { "status":"success", "duplicates":["21-1-0001"] }

**After** — form fields, guarded (response shape unchanged):

    POST check_duplicates.php
    Content-Type: application/x-www-form-urlencoded
    school_ids=["21-1-0001","21-1-0002"]   (JSON-array string)
    admin_key=<ADMIN_KEY>
    -> 200 { "status":"success", "duplicates":["21-1-0001"] }
    -> 401 { "status":"error", "message":"..." }

## upload_students_zip.php

**Before** — raw Excel file part; server re-parsed via PhpSpreadsheet (Composer);
`skip_ids` ignored; plain-text reply; fabricated counts; unguarded.

    POST upload_students_zip.php  (multipart/form-data)
      excel=<students.xlsx>       (server re-parsed positional A-G)
      photos_zip=<photos.zip>     (optional)
      skip_ids=21-1-0001,...      (IGNORED)
    -> 200 text/plain "OK Upload complete!"   (no real counts)

**After** — client-sent rows; guarded; honors skip_ids; real JSON counts; no Composer.

    POST upload_students_zip.php  (multipart/form-data)
      rows=[{"school_id":"21-1-0001","name":"Juan Dela Cruz","course":"...","department":"...",
             "year_level":"...","gender":"...","status":"..."}, ...]   (JSON-array string field)
      admin_key=<ADMIN_KEY>
      skip_ids=21-1-0001,...      (comma-joined; honored)
      photos_zip=<photos.zip>     (optional; core ZipArchive, glob *school_id*)
    -> 200 { "status":"success",
             "success_count":<int>, "skipped_count":<int>, "error_count":<int>,
             "message":"..." }
    -> 401 { "status":"error", "message":"..." }

INSERT writes: `school_id, name, course, department, year_level, gender, status, photo`
(`photo` = the ZIP-matched path for the row's `school_id`, else NULL). `code` and
`visits` are excluded and left to their DB defaults. (Contrast: the fuller
`register_student.php` INSERT — `deliverables/loams_api/register_student.php` —
also writes `code` and `visits`; the bulk-import path deliberately does not.)

## Composer no longer required

`upload_students_zip.php` no longer requires `PhpOffice\PhpSpreadsheet` or
`vendor/autoload.php`. The only PHP extension it needs is core **ext-zip**, and
only for the optional photos ZIP.
