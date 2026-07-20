# Security Updates Applied

> ## ⚠️ VERIFIED-FALSE CONTENT — READ BEFORE TRUSTING THIS DOCUMENT
>
> **As of 2026-07-15**, most of the "Changes" checklists below were verified against the actual
> source in `deliverables/loams_api/` and found to **not match the code**. This document reads as
> a completed-work checklist; large parts of it describe work that was never wired in. Do not use
> the ✅ marks below as evidence that a protection exists — verify against source.
>
> **The most dangerous gap:** four destructive/write endpoints were marked "✅ Authentication
> required" below when **no authentication code existed in any of them**. As of 2026-07-20 one of
> the four (`reset_visits.php`) has gained a real guard **in repo source only** — the other three
> are unchanged, and nothing is deployed:
>
> - **`delete_students.php` — NOT authenticated.** Deletes student rows by `school_id`. Verified: `grep -n "admin_key\|requireAdminAuth\|auth_helper" delete_students.php` → no match.
> - **`bulk_update_students.php` — NOT authenticated.** Overwrites student fields in bulk. Verified: same grep → no match.
> - ~~**`reset_visits.php` — NOT authenticated.**~~ **UPDATED 2026-07-20 — guarded in source, NOT
>   yet deployed.** `reset_visits.php:12` now calls `requireAdminAuth($conn)` before reading the
>   payload or touching the DB. **This is true of the repo copy only.** The guard has **not** been
>   deployed to the live web root, so the running system still accepts an unauthenticated reset.
>   See "Deploy state" below.
> - **`upload_students_zip.php` — NOT authenticated**, and additionally has none of the file-upload
>   validation the document claims (no MIME check, no size limits, extracts a ZIP's contents
>   unfiltered, still creates directories `0777`). This one writes arbitrary uploaded files to disk
>   with zero validation — arguably the most severe of the four, not a minor omission.
>
> ~~Repo-wide, `requireAdminAuth()` … has **exactly one real caller**: `save_branding.php:6`.~~
> **UPDATED 2026-07-20:** repo-wide, `requireAdminAuth()` (defined in `auth_helper.php:11`) now has
> **three real callers**: `save_branding.php:6`, `reset_visits.php:12`, and
> `update_admin_info.php:13`. Verified: `grep -rn "requireAdminAuth" deliverables/loams_api/`.
>
> ### Deploy state (verified 2026-07-20) — guarded in source ≠ protected in production
>
> The two new guards are **committed to the repo but not deployed**. Deploying the API is
> deliberately gated on the owner and had not happened at the time of this verification.
>
> | Endpoint | Guard in repo source | Present in deployed web root | Actually protected live |
> |---|---|---|---|
> | `save_branding.php` | ✅ yes | ❌ **file absent entirely** | n/a — endpoint does not exist live |
> | `reset_visits.php` | ✅ yes (`:12`) | ✅ present, but **pre-guard copy** | ❌ **no** |
> | `update_admin_info.php` | ✅ yes (`:13`) | ✅ present, but **pre-guard copy** | ❌ **no** |
>
> Verified by listing the deployed `loams_api/` directory: `save_branding.php` (and `get_branding.php`)
> are in the repo but **do not exist in the deployed web root at all**. Until a deploy happens, do
> **not** read the ✅ marks in this document as evidence that the running system enforces `admin_key`
> on reset or admin-info updates — it does not.
>
> `config.php` (the "centralized configuration" file described
> in Files Created §1) is **never included by any other file** — verified: `grep -rln "config.php" --include=*.php .` returns zero matches. It is dead code.
>
> Section-by-section corrections are marked inline below with **`VERIFIED 2026-07-15:`** callouts.
> Where a claim could not be substantiated against current source, it has been struck and replaced
> with what the code actually does. This is a **statement of current reality**, not a claim about
> what happened historically — this document may describe a change that was planned, partially
> reverted, or never completed; that history cannot be reconstructed from the code alone.
>
> **`upload_photos.php` does not exist** in `deliverables/loams_api/` — but this document does not
> reference that filename either, so there is nothing here to correct on that point.

## Summary
This document was originally written to describe a set of security improvements. Independent
verification on 2026-07-15 found that most of the described changes are **not present in the
current code** — see the notice above. Treat this file as a mix of a few real changes and a lot of
aspirational/stale claims until each item is re-verified.

---

## Files Created

### 1. `config.php` (NEW)
**Purpose:** Centralized configuration for database, environment, and security settings

**Key Features:**
- Environment-aware error handling (development vs production)
- Centralized database credentials
- CORS configuration
- File upload limits and validation rules
- Automatic error logging to `logs/php_errors.log`

**Action Required:**
- When deploying to production, change `define('ENV', 'development');` to `define('ENV', 'production');`
- Update `CORS_ALLOWED_ORIGIN` from `'*'` to your actual frontend domain (e.g., `'https://yourdomain.com'`)

### 2. `auth_helper.php` (NEW)
**Purpose:** Centralized admin authentication for sensitive operations

**Key Features:**
- `requireAdminAuth()` function to verify admin credentials
- Returns proper HTTP status codes (401 for unauthorized)
- Prevents unauthorized access to admin-only endpoints

---

## Files Modified

### 3. `db.php`
**Changes (as originally claimed — see verification below):**
- ❌ ~~Uses `config.php` for centralized settings~~
- ❌ ~~Removed hardcoded error display settings~~
- ❌ ~~Added `utf8mb4` character set to prevent encoding-based SQL injection~~
- ❌ ~~Better error logging without exposing connection details~~

**VERIFIED 2026-07-15:** None of the above are true. `db.php` does not include `config.php`; it
still hardcodes its own `$host`/`$user`/`$pass`/`$db` and `error_reporting(E_ALL); ini_set('display_errors',1);` inline. `grep -rn "utf8mb4\|set_charset" --include=*.php .` across the whole
`loams_api/` directory returns **zero matches** — no charset is ever set. The one thing that *is*
real: `db.php` fails closed with a JSON error on connect failure rather than a raw PHP error.

**Impact:** None - all existing includes still work

### 4. `guest_login.php`
**Changes (as originally claimed — see verification below):**
- ❌ ~~Removed duplicate database connection code~~
- ❌ ~~Now uses shared `db.php` for consistency~~
- ❌ ~~Cleaner code, centralized error handling~~

**VERIFIED 2026-07-15:** `guest_login.php` still opens its own inline `mysqli` connection with
hardcoded `$servername`/`$username`/`$password`/`$dbname` and does not `include`/`require` `db.php`
anywhere (`grep -rln "include.*db\.php\|require.*db\.php" --include=*.php .` — `guest_login.php`
is absent from the result).

**Impact:** None - functionality unchanged

### 5. `upload_students_zip.php` 🔴 UNPROTECTED — DESTRUCTIVE/WRITE ENDPOINT, NOT AUTHENTICATED

**Changes (as originally claimed):**
- ❌ ~~**Authentication required** - must provide valid admin_key~~
- ❌ ~~Validates Excel file type using MIME detection (not just extension)~~
- ❌ ~~Validates ZIP file type and size (max 50MB)~~
- ❌ ~~File size limits: Excel max 10MB, ZIP max 50MB~~
- ❌ ~~Prevents directory traversal attacks (blocks `../` in filenames)~~
- ❌ ~~Only extracts image files from ZIP (jpg, jpeg, png, gif)~~
- ❌ ~~Sanitizes all filenames before saving~~
- ❌ ~~Changed directory permissions from 0777 to 0755 (more secure)~~
- ❌ ~~Validates school_id format to prevent injection attacks~~
- ❌ ~~Returns JSON response instead of plain text~~
- ❌ ~~Cleans up temporary files after processing~~

**VERIFIED 2026-07-15 — none of the above claims hold.** Read in full; the actual file:
- Has **no `admin_key` reference at all** (`grep -n "admin_key" upload_students_zip.php` — no match).
- Has **no MIME check** — no `finfo`/`mime_content_type` call anywhere in the file.
- Has **no file size validation** for either the Excel file or the ZIP.
- Calls `move_uploaded_file(..., $uploadDir . basename($_FILES['excel']['name']))` — `basename()`
  strips path separators, which is *some* traversal mitigation, but there is no allow-list,
  extension check, or the "50MB max" / "10MB max" limits the doc claims.
- Extracts the **entire ZIP unfiltered** via `$zip->extractTo($photoDir)` — every file in the
  archive is written to disk, not just images.
- Creates directories with `mkdir($uploadDir, 0777, true)` and `mkdir("uploads/students/", 0777, true)`
  — **still 0777**, not 0755 (`grep -n "mkdir\|0777\|0755" upload_students_zip.php` shows both calls
  using `0777`).
- Validates `school_id` with nothing more than `trim()`.
- Returns **plain text** (`echo "✅ Upload complete!\n";` etc.), not JSON (`grep -n "json_encode" upload_students_zip.php` — no match).
- Never deletes anything from `uploads/temp/` after processing.

This endpoint writes to disk and to the database from unauthenticated, unvalidated input. It is a
larger exposure than the three endpoints below, not a smaller one.

**Impact:** None currently — the frontend does not need `admin_key` because the server never checks
for one. If a fix is scoped later, the frontend contract in "Frontend Changes Required" below would
need to become real for this endpoint too.

### 6. `delete_students.php` 🔴 NOT AUTHENTICATED — DESTRUCTIVE ENDPOINT

**Changes (as originally claimed):**
- ❌ ~~**Authentication required** - must provide valid admin_key~~
- ❌ ~~Uses shared `db.php` connection~~

**VERIFIED 2026-07-15:** `grep -n "admin_key\|requireAdminAuth\|auth_helper" delete_students.php`
returns **no match** — there is no authentication code in this file at all. It also does not
`include`/`require` `db.php`; it opens its own inline `new mysqli("localhost", "root", "", "wits_app")`
connection. **Any caller who can reach this endpoint can delete any student row by `school_id`,
with no credential check whatsoever.**

**Impact:** None currently — sending `admin_key` in POST data has no effect; the server never reads
it. Deletion works exactly the same with or without it.

### 7. `bulk_update_students.php` 🔴 NOT AUTHENTICATED — DESTRUCTIVE/WRITE ENDPOINT

**Changes (as originally claimed):**
- ❌ ~~**Authentication required** - must provide valid admin_key~~
- ❌ ~~Uses shared `db.php` connection~~
- ❌ ~~Removed duplicate connection code~~

**VERIFIED 2026-07-15:** Same grep as above, same result — **no auth reference in this file**. It
also still opens its own inline `mysqli` connection (`new mysqli("localhost", "root", "", "wits_app")`)
rather than including `db.php`; the "duplicate connection code" was never removed. **Any caller can
overwrite arbitrary student records in bulk with no credential check.**

**Impact:** None currently — sending `admin_key` in POST data has no effect; the server never reads
it.

### 8. `reset_visits.php` 🟡 GUARDED IN SOURCE, NOT DEPLOYED — DESTRUCTIVE ENDPOINT

**Changes (as originally claimed):**
- ✅ **Authentication required** - must provide valid admin_key — **true in repo source as of
  2026-07-20**, false of the deployed copy
- ❌ ~~Removed hardcoded error display~~

**VERIFIED 2026-07-15:** no auth reference in this file; any caller could zero visit counts and
delete visit history for an entire department with no credential check.

**UPDATED 2026-07-20:** the repo copy now calls `requireAdminAuth($conn)` at `reset_visits.php:12`,
before the payload is read and before any DB write, so a 401 cannot leave a department half-reset.
Two further facts, stated precisely:

- **Not deployed.** The deployed web root still holds the pre-guard copy. The running system
  accepts an unauthenticated reset. Do not treat the ✅ above as a live protection.
- **Still hardcodes** `error_reporting(E_ALL); ini_set('display_errors', 1);` at the top rather than
  using centralized config — unchanged, tracked separately.

Also updated 2026-07-20 (correctness, not auth): the `UPDATE students` and the `DELETE lv …` are now
wrapped in `$conn->begin_transaction()` / `commit()`, with `rollback()` and a
`{"status":"error","message":"…"}` response if either `execute()` fails. Previously they were two
autocommitted statements with unchecked return values, so a mid-operation failure could zero the
visit counters while leaving the history in place — a half-reset the admin would then retry against.
Both `students` and `library_visits` are **InnoDB** (verified via `information_schema.TABLES`), so
the transaction is genuinely transactional rather than a MyISAM no-op.

**Impact:** callers must now send a valid `admin_key` **once the deploy happens**. Until then,
sending it still has no effect on the live endpoint.

### 8b. `update_admin_info.php` 🟡 GUARDED IN SOURCE, NOT DEPLOYED

**VERIFIED 2026-07-20:** the repo copy calls `requireAdminAuth($conn)` at `update_admin_info.php:13`.
The deployed web root holds the pre-guard copy, so the live endpoint is **not** protected. This file
is not otherwise described in the original document.

### 9. `api.php`
**Changes (as originally claimed):**
- ❌ ~~Uses `config.php` for CORS settings~~
- ❌ ~~CORS origin now configurable (set to `'*'` for now)~~

**VERIFIED 2026-07-15:** `api.php` hardcodes `header('Access-Control-Allow-Origin: *');` directly
and does not include `config.php` at all (`config.php`'s `CORS_ALLOWED_ORIGIN` constant is defined
but never read anywhere in the codebase). Editing `config.php` for production would currently have
no effect on the actual CORS header sent by `api.php`.

**Impact:** None currently - update `CORS_ALLOWED_ORIGIN` in config.php for production

### 10. `admin_login.php`
**Changes (as originally claimed):**
- ❌ ~~Removed hardcoded error display settings~~
- ❌ ~~Now uses centralized config error handling~~

**VERIFIED 2026-07-15:** `admin_login.php` still has `error_reporting(E_ALL); ini_set('display_errors',1);`
hardcoded at the top of the file and does not include `config.php`.

**Impact:** None - functionality unchanged

### 11. `student_login.php`
**Changes (as originally claimed):**
- ❌ ~~Removed hardcoded error display settings~~
- ❌ ~~Now uses centralized config error handling~~

**VERIFIED 2026-07-15:** `student_login.php` still has `error_reporting(E_ALL); ini_set('display_errors', 1);`
hardcoded at the top of the file and does not include `config.php`.

**Impact:** None - functionality unchanged

### 12. `register_student.php`
**Changes (as originally claimed):**
- ❌ ~~Removed hardcoded error display settings~~
- ❌ ~~Now uses centralized config error handling~~

**VERIFIED 2026-07-15:** `register_student.php` still has `error_reporting(E_ALL); ini_set('display_errors',1);`
hardcoded at the top of the file and does not include `config.php`.

**Impact:** None - functionality unchanged

---

## Frontend Changes Required

### For Admin Operations (Critical)

**VERIFIED 2026-07-15 / UPDATED 2026-07-20: mostly still aspirational.** `delete_students.php`,
`bulk_update_students.php`, and `upload_students_zip.php` do not check `admin_key` server-side in
any copy — repo or deployed. `reset_visits.php` **does** check it in repo source (`:12`) but that
guard is **not deployed**, so against the live server the request still succeeds without a key.
`save_branding.php` and `update_admin_info.php` also enforce `admin_key` in repo source, and
`save_branding.php` is not present in the deployed web root at all. The examples below are the
intended contract; treat them as effective only after a deploy.

These endpoints were intended to require `admin_key` in the POST data (not currently enforced —
see above):

```javascript
// Example: Delete students
fetch('/loams_api/delete_students.php', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    admin_key: 'your-admin-key-here',  // ← ADD THIS
    school_ids: ['123', '456']
  })
})

// Example: Bulk update students
fetch('/loams_api/bulk_update_students.php', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    admin_key: 'your-admin-key-here',  // ← ADD THIS
    students: [...]
  })
})

// Example: Reset visits
const formData = new FormData();
formData.append('admin_key', 'your-admin-key-here');  // ← ADD THIS
formData.append('department', 'CS');
fetch('/loams_api/reset_visits.php', {
  method: 'POST',
  body: formData
})

// Example: Upload students with ZIP
const formData = new FormData();
formData.append('admin_key', 'your-admin-key-here');  // ← ADD THIS
formData.append('excel', excelFile);
formData.append('photos_zip', zipFile);
fetch('/loams_api/upload_students_zip.php', {
  method: 'POST',
  body: formData
})
```

### No Changes Required For:
- `student_login.php` - works as before
- `guest_login.php` - works as before
- `register_student.php` - works as before
- All other read-only endpoints

---

## Security Improvements Summary

**VERIFIED 2026-07-15: most items below are not actually in effect.** Kept for record, annotated:

### Claimed vs. Verified Status:

1. ❌ **Information Disclosure** - `config.php` has the environment-aware logic, but it is never
   included by any endpoint; every endpoint checked (`admin_login.php`, `student_login.php`,
   `register_student.php`, `reset_visits.php`, `db.php`) still hardcodes
   `ini_set('display_errors', 1)`, so PHP errors still display to the client.
2. ❌ **File Upload Attacks** - `upload_students_zip.php` has no MIME check, no size limits, and
   extracts every file in a ZIP unfiltered. Not fixed.
3. ❌ **Directory Traversal** - only `basename()` on the upload filename (weak, undocumented as
   partial); no allow-list or path canonicalization anywhere.
4. ❌ **SQL Injection** - `utf8mb4`/`set_charset` appears **nowhere** in the codebase
   (`grep -rn "utf8mb4\|set_charset" --include=*.php .` → no matches). Queries do use prepared
   statements/`bind_param` in the files reviewed, which is the real mitigation already present —
   the "charset" claim specifically is false.
5. ⚠️ **Unauthorized Access** — *partially* addressed in source, **nothing deployed**. As of
   2026-07-20 `requireAdminAuth()` has three callers: `save_branding.php`, `reset_visits.php`, and
   `update_admin_info.php`. `delete_students.php`, `bulk_update_students.php`, and
   `upload_students_zip.php` — all destructive or write operations — still have **no
   authentication** anywhere. And because the guards are not deployed, the **live** system today
   enforces `admin_key` on **no endpoint at all**.
6. ❌ **Insecure Permissions** - `upload_students_zip.php` still creates directories with `0777`
   (verified twice in the file), not `0755`.
7. ❌ **CORS Misconfiguration** - `api.php` hardcodes `Access-Control-Allow-Origin: *` and does not
   read `config.php`'s `CORS_ALLOWED_ORIGIN`; there is nothing to configure for production today.

### What is actually true today:

- `save_branding.php`, `reset_visits.php`, and `update_admin_info.php` genuinely enforce `admin_key`
  via `requireAdminAuth()` (`auth_helper.php`) **in repo source**. None of those three guards are
  live: the first file is absent from the deployed web root, and the other two are deployed only in
  their pre-guard form.
- `reset_visits.php` performs its two destructive statements inside a single InnoDB transaction,
  rolling back if either fails, so it cannot leave a department half-reset.
- `delete_students.php`, `bulk_update_students.php`, and `reset_visits.php` use parameterized
  queries (`bind_param`) for the values they do handle, which mitigates classic SQL injection on
  those parameters even without the claimed charset change.
- `db.php` fails closed with a JSON error message (not a raw stack trace) on connection failure.
- `config.php` and `auth_helper.php` exist and are internally correct — they are simply not wired
  into most of the endpoints this document claims they were wired into.

---

## Testing Checklist

**VERIFIED 2026-07-15 / UPDATED 2026-07-20:** the "with admin_key" items below describe intended
behavior, not what a test against the live server proves. `delete_students.php`,
`bulk_update_students.php`, and `upload_students_zip.php` never check `admin_key` in any copy.
`reset_visits.php` checks it in repo source but the deployed copy does not, so **every** endpoint
below currently accepts requests with or without `admin_key` identically **against the live server**.
Don't treat a passing test here as evidence of authentication; it only proves the endpoint works.

- [ ] Test student login (should work without changes)
- [ ] Test guest login (should work without changes)
- [ ] Test student registration (should work without changes)
- [ ] Test admin login (should work without changes)
- [ ] Test delete students with admin_key (⚠️ not enforced server-side — endpoint has no auth)
- [ ] Test bulk update with admin_key (⚠️ not enforced server-side — endpoint has no auth)
- [ ] Test reset visits with admin_key (⚠️ enforced in repo source only — the deployed copy is
      pre-guard, so a live test proves nothing about auth until a deploy happens)
- [ ] Test reset visits failure path: both statements must roll back together (no half-reset)
- [ ] Test upload ZIP with admin_key (⚠️ not enforced server-side — endpoint has no auth)
- [ ] Verify error logs are created in `logs/php_errors.log` (⚠️ `config.php`'s log-to-file path is
      not wired into any endpoint checked; endpoints still call `ini_set('display_errors', 1)` directly)

---

## Production Deployment Steps

**VERIFIED 2026-07-15:** steps 1 and 3 below currently have no effect on the running system — see
the notice at the top of this document. `config.php` is not included by `api.php` or any other
endpoint, so editing it does not change the CORS header actually sent. Adding `admin_key` to the
frontend for step 3 will not add protection either, since the four endpoints listed above never
read it. Treat these as a to-do list for wiring the auth/config work in, not as already effective.

**UPDATED 2026-07-20:** step 3 now half-matters — the Qt client does send `admin_key` to
`reset_visits.php` and `update_admin_info.php`, and those endpoints read it **in repo source**. It
still buys no protection until the repo copies are deployed, so step 0 comes first.

0. **Deploy the guarded PHP.** Copy the current `deliverables/loams_api/` sources over the web root,
   including `save_branding.php` and `get_branding.php`, which are **absent from the deployed web
   root entirely** (verified 2026-07-20 by listing it). Until this happens, no endpoint enforces
   `admin_key` on the live system.

1. Update `config.php`:
   ```php
   define('ENV', 'production');
   define('CORS_ALLOWED_ORIGIN', 'https://your-actual-domain.com');
   ```

2. Ensure `logs` directory is writable:
   ```bash
   chmod 755 loams_api/logs
   ```

3. Update frontend to include `admin_key` for admin operations

4. Test all functionality in staging before production

---

## Support

**VERIFIED 2026-07-15:** items 1-4 below assume protections that are not currently in the code —
there is no `logs/php_errors.log` write path wired into the checked endpoints, no endpoint below
actually enforces `admin_key`, and `upload_students_zip.php` enforces none of the size/type limits
described. Kept here as the intended troubleshooting steps once the underlying work is done.

If you encounter any issues:
1. Check `logs/php_errors.log` for detailed error messages
2. Verify `admin_key` is included in requests to protected endpoints
3. Ensure file upload sizes don't exceed limits (Excel: 10MB, ZIP: 50MB)
4. Verify file types are correct (Excel: .xls/.xlsx, ZIP: .zip, Images: .jpg/.jpeg/.png/.gif)
