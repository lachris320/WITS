# Phase 4a endpoint contract (captured 2026-07-30, before hardening)

`admin_key` always a FORM field; guard = `requireAdminAuth($conn)` → 401 `{"status":"error","message":"Admin authentication required"}` (missing) / `{"status":"error","message":"Invalid admin key"}` (wrong).

## delete_students.php
BEFORE (request): `application/json` body `{"school_ids":["2023-001","2023-002"]}`; opens hardcoded `new mysqli("localhost","root","","wits_app")`; no visit cascade.
BEFORE (response): `{"status":"success","deleted":<n>}`.
AFTER (request): `application/x-www-form-urlencoded` body `school_ids%5B%5D=2023-001&school_ids%5B%5D=2023-002&admin_key=<key>`; `include db.php` + `auth_helper.php`; wrapped in a transaction that also deletes `library_visits WHERE student_id IN (...)`.
AFTER (response): unchanged `{"status":"success","deleted":<n>}`.
NEGATIVE: no `admin_key` → 401; DB untouched (guard rejects before any DELETE).

## bulk_update_students.php
BEFORE (request): `application/json` body `{"students":[{school_id,code,name,department,course,year_level,gender,status}, ...]}`.
BEFORE (response): `{"status":"success","updated":<n>,"errors":[...]}` or `{"status":"error","message":...}`.
AFTER (request): `application/x-www-form-urlencoded`; `admin_key=<key>` + `students=<JSON-string of the same array>` (single field; server `json_decode`s it).
AFTER (response): unchanged.
NEGATIVE: no `admin_key` → 401; no UPDATE runs.

## deactivate_department.php
BEFORE (request): FORM `department=<dept>`. Response `{"status":"success","message":...}`.
AFTER (request): FORM `department=<dept>&admin_key=<key>`. Response unchanged.
NEGATIVE: no `admin_key` → 401.

## delete_department.php
BEFORE (request): FORM `department=<dept>` (already cascades visits in a transaction). Response `{"status":"success","message":...}`.
AFTER (request): FORM `department=<dept>&admin_key=<key>`. Response unchanged.
NEGATIVE: no `admin_key` → 401.
