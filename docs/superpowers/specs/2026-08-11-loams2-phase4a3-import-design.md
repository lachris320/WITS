# LOAMS 2.0 — Phase 4a.3: Bulk Student Import (Design Spec)

> Date: 2026-08-11 · Status: **APPROVED (brainstorm)** · Track: 4a (Database + Import) — **final increment**
> Predecessor slice: 4a.2c (department deactivate + delete, PR #38, `b846823`).
> No secrets, credentials, or real student PII in this document (repo hygiene rule). All keys/URLs are placeholders.

---

## 1. Background

**WITS** ships two binaries from one repo: legacy **`WITS.exe`** (Qt Widgets) and **`WITSQuick.exe`**
(LOAMS 2.0 — Qt 6.11 / C++17 / QML / Qt Quick, MVVM). New UI work lives in the Quick app. The MVVM
contract is strict: **ViewModels in `qt-app/quick/viewmodels/` are the only QML-facing C++** — QML never
calls a `witscore` controller directly. **`Theme.qml`** is the single source of every visual token (ZERO
raw hex outside `Theme.qml`).

Phase 4a.3 is the **last increment of track 4a (Database + Import)**. The 4a.2 CRUD UI (student
delete/edit/bulk/register + department deactivate/delete) is fully complete and merged. This slice brings
**bulk student import** into the Quick admin app and reworks the import request/backend path.

**Auth-guard lineage this slice extends:**

- 4a.1 guarded four destructive endpoints with `requireAdminAuth($conn)` (bcrypt-verified
  `$_POST['admin_key']`) and established that guarded mutations send `admin_key` in the POST body.
- 4a.2b-iv guarded `register_student.php` the same way and established the pattern this slice reuses:
  **guarded backend + `admin_key` threading, deployed WITH the client (BREAKING)** — an old client without
  key threading will 401.

**Existing asset — `ImportController` already exists.** `qt-app/core/importcontroller.{h,cpp}` +
`qt-app/core/importdata.h`, fully unit-tested (`qt-app/tests/tst_importcontroller.cpp`) and wired into
legacy `adminwindow.cpp`. It exposes pure statics `normalizeHeader` / `mapHeaders` / `parseCsv` /
`parseExcel` / `parseDuplicateResponse` / `parseUploadResponse`, async `checkDuplicates()` /
`uploadStudents()`, and signals `duplicatesResolved` / `importError` / `uploadStarted` / `uploadProgress` /
`uploadFinished` / `uploadFailed`. 4a.3 **brings this shared controller into the Quick app** via a new
`ImportViewModel` + `ImportStudentsDialog`, **and reworks the request/backend** per §3.

**`students` table columns** (from `register_student.php`'s INSERT):
`(code, school_id, name, course, year_level, department, gender, status, visits, photo, time_date)`.

---

## 2. Problems in the current import path (the WHY)

Three concrete defects in today's import make the current path fragile. Each is fixed by the approach in §4.

**① Strict positional format (silent data failure).**
`deliverables/loams_api/upload_students_zip.php` **ignores the header row** and reads columns **by
position**: A=`school_id`, B=`name`, C=`course`, D=`department`, E=`year_level`, F=`gender`, G=`status`.
The client's fuzzy `mapHeaders()` only feeds the **duplicate-check** and the legacy **preview** — it has
**no effect on the insert**. So a valid-looking file inserts wrong-column data (or fails) whenever the
columns are not in exact A–G order.

**② `skip_ids` is ignored + fabricated counts + plain-text response.**
The endpoint never reads `skip_ids` (grep across the backend finds no reference). It inserts **every** row;
`$stmt->execute()` is **unchecked** and `$count++` is **unconditional**, so `success_count` /
`error_count` are fabricated. It returns **plain text** (`"✅ Upload complete!"`), never real JSON counts.
The client-side duplicate-skip UX is therefore a **no-op** against the current endpoint.

**③ Composer / PhpSpreadsheet deploy pain.**
The endpoint re-parses the Excel **server-side** via `PhpOffice\PhpSpreadsheet` (`require
'vendor/autoload.php'`), requiring a manual **Composer** install. `composer.json` is **not tracked** in the
repo (it exists only in the deployed XAMPP copy), so every fresh deploy rediscovers the pain. Note the
photos-ZIP side uses **`ZipArchive`**, which is **core PHP** (`ext-zip`), **not** Composer.

**Bonus (OUT OF SCOPE) — the "21-1-0001 read as admin key" bug is NOT an import bug.**
It lives in the **legacy kiosk login heuristic** `mainwindow.cpp:193-201`:
`input.toLongLong(&ok)` → numeric ⇒ treated as `school_id`, else ⇒ treated as `admin_key`. A hyphenated ID
fails the integer parse and is routed to `admin_login.php`. This is a **login** bug, entirely separate from
import. **Import is immune by construction:** it reads `school_id`s from an **explicit column** as opaque
`QString`s and never touches that numeric-vs-admin heuristic. This spec records that immunity as a
**documented guarantee** and lists the login fix (plus a check of whether the Quick `KioskViewModel` shares
the heuristic) as an explicit **out-of-scope follow-up** (§11).

---

## 3. Approach — "Client-sends-rows, simplify server" (owner-chosen)

The client **already** parses the file into structured rows for the duplicate check. It now sends those
**already-parsed, header-mapped rows** to the server (as a JSON string) instead of the raw Excel file.

This single change fixes all three findings at once:

- **Fixes ③** — the server no longer parses Excel, so **PhpSpreadsheet / Composer is removed** entirely
  (only core `ext-zip` remains, for the optional photos ZIP).
- **Fixes ②** — the server receives discrete rows, so it can **honor `skip_ids`** and return **real
  counts** from checked inserts.
- **Fixes ①** — the server inserts exactly the **header-mapped** data the client validated; column order and
  extra columns no longer matter.

Net result: a **simpler server**. The client already owns parsing, mapping, and validation; the server
becomes a guarded, row-oriented inserter that returns honest counts.

---

## 4. Components

### 4.1 New (Quick) — `ImportViewModel`  (`qt-app/quick/viewmodels/`)

The **only** QML-facing surface for import. Owns an `ImportController` **and its own
`QNetworkAccessManager`** — mirroring the register/edit dual-controller ownership pattern in
`DatabaseViewModel`. Exposes:

- import state as `Q_PROPERTY` (busy/phase flags, parsed-row counts, duplicate counts, last result),
- actions as `Q_INVOKABLE` (pick data file, pick photos ZIP, start import, continue-after-duplicates,
  cancel, download template) — **`cancel` is pre-upload / return-to-idle only** (dismiss at the
  file-select or duplicate-confirm stage). It does **not** abort an in-flight upload: `ImportController`
  exposes no `QNetworkReply::abort` path today (legacy's `cancelUpload` flag never aborted the reply
  either), and adding one is out of scope for this slice.
- results as signals.

Register it for QML (declarative registration, per the module conventions).

### 4.2 New (Quick) — `ImportStudentsDialog.qml`  (`qt-app/quick/qml/`)

Modal dialog, **parallel to `RegisterStudentDialog.qml`**. File pickers + inline states
(parsing → duplicates → uploading → result). Uses **`QtQuick.Dialogs` `FileDialog`** — already a dependency
since 4a.2b-i added `Qt6::QuickDialogs2`. **Every server-provided string is rendered
`textFormat: Text.PlainText`.** Use **register-scoped `objectName`s** that do **not** collide with the
cascade or register-dialog objectNames.

### 4.3 `DatabaseScreen.qml`

Add an **"Import"** toolbar button next to **＋Add Student**; wire it to the dialog + `ImportViewModel`.

### 4.4 Modified (shared `witscore` — affects BOTH Quick and legacy)

`ImportController` and `importdata.h` are shared. All changes below apply to both binaries.

- **`ImportController::uploadStudents` — send rows, not raw Excel.**
  Change the signature/impl to POST the parsed **rows** as a JSON-string form field named `rows`, plus
  `admin_key`, **instead of** the raw-Excel file part. Keep the **optional** `photos_zip` file part
  (`ZipArchive` server-side). Keep the `skip_ids` part. New signature accepts the parsed rows (e.g. a
  `ParsedTable`, or pre-serialized rows) + `zipPath` + `skipIds` + `const QString &adminKey`.
  Remove the old **fatal** "Cannot open Excel file" branch (the client already parsed it). Keep the
  optional **ZIP-open-failure non-fatal warning**. `uploadStarted` fires **immediately before** the POST.

- **`ImportController::checkDuplicates` — gains `const QString &adminKey`; move to form fields.**
  Because `requireAdminAuth` reads `$_POST['admin_key']`, move this request from a **JSON body** to
  **`x-www-form-urlencoded` form fields**: `school_ids` as a **JSON-array string** in one field, plus
  `admin_key`. (Mirrors how `bulk_update_students` sends `students` as a JSON-string field + `admin_key`,
  per 4a.1.)

- **NEW pure static `ImportController::serializeRows(const ParsedTable &table) → QByteArray`.**
  Produces a **JSON array of row objects** with keys `school_id`, `name`, `course`, `department`,
  `year_level`, `gender`, `status`, using `table.headerIndex` to map columns. **Ignore `code`, `visits`,
  and any extra/unrecognized columns.** **Trim** each value. This is the primary unit-test seam.

- **`parseUploadResponse` — read real JSON.**
  Now reads `status`, `message`, `success_count`, `skipped_count`, `error_count`. **Keep the existing
  plain-text fallback** for safety (older/partly-deployed endpoints).

- **`UploadResult` struct (`importdata.h`) — add `int skippedCount = 0;`.**

- **401 routing.**
  The reply handlers must distinguish a **401-with-body auth failure** from a generic network error — route
  auth failures to a clear **"admin authentication"** message, not a raw network error. **Reuse the shared
  reply classifier already used by `StudentController`** (`StudentController::replyIsServerAnswer`, added
  4a.2b-i / generalized 4a.2b-ii) if accessible; otherwise mirror it locally. **Keep it minimal — do not
  build new infra.**

### 4.5 Modified (legacy) — `adminwindow.cpp`

Update the **3 call sites** to pass the row data + `m_adminKey` (already retained via `setAdminKey`):

- `checkDuplicates` (~line 918),
- `uploadStudents` (~line 931 and ~line 981).

**Correction to a naive assumption — legacy does NOT hold a `ParsedTable`.** `adminwindow` retains only
`selectedExcelPath` (a `QString`), `selectedZipPath`, and `bulkHeaderIndex` (a `QMap`); the row data lives
in the **live, user-editable `ui->bulkTable`** (`QTableWidget`). The duplicate-check path deliberately
reads that live table via a `getCell` lambda **specifically to pick up hand-edits made after preview**
([adminwindow.cpp:898-916](qt-app/adminwindow.cpp) — "school IDs are NOT cached from the ParsedTable, only
`bulkHeaderIndex` is"), and the upload call sites currently pass `selectedExcelPath` (the raw file).

**Therefore the legacy implementer must BUILD a `ParsedTable` (or the serialized rows) from the live
`ui->bulkTable` + `bulkHeaderIndex`, NOT by re-parsing `selectedExcelPath`.** Re-parsing the file would
silently discard the user's hand-edits — a behavior regression. This reconstruction is required legacy
work (not a free hand-off) and must feed the same `serializeRows` seam the Quick path uses. `m_adminKey`
is confirmed retained. **Legacy import must keep working end-to-end, hand-edits included.**

### 4.6 Modified (backend, deployed WITH client — BREAKING)

**`upload_students_zip.php`:**

1. `requireAdminAuth($conn)` **FIRST** — before any DB read, ZIP extract, or insert.
2. Read rows from `$_POST['rows']` (`json_decode` to an array of 7-key row objects).
3. Build a skip-set from `$_POST['skip_ids']` (existing comma-joined format).
1a. If `photos_zip` provided, extract once via **core `ZipArchive`** to a temp dir (before the row loop).
4. For each row:
   - **Server-side re-validate** `school_id` & `name` non-empty (invalid → `error_count++`, skip).
   - If `school_id` in skip-set → `skipped_count++`.
   - Else: **if a ZIP was extracted**, glob-match `*school_id*` in the extracted photos and copy the match
     to the students photo dir → `photoPath` (else `photoPath = NULL`). Then **prepared INSERT** of the
     7 core columns **plus `photo`** (bound to `photoPath`), and **CHECK `execute()`** (success →
     `success_count++`, failure → `error_count++`).
5. (photo extraction moved to step 1a; per-row matching folded into step 4 — this preserves today's
   per-row `photo`-path persistence exactly.)
6. Return JSON `{status:"success", success_count, skipped_count, error_count, message}`.
7. **REMOVE** `use PhpOffice\PhpSpreadsheet\IOFactory;` and `require 'vendor/autoload.php'`.

INSERT writes the **7 core columns PLUS `photo`** (`photo` = the ZIP-matched path for that `school_id`,
else `NULL` — mirroring today's working behavior at `upload_students_zip.php:65-78`, so the retained
photos-ZIP feature keeps persisting the path, not just copying the file). **`code` and `visits` are
excluded** and left to their DB defaults. Per the tracked schema (`deliverables/sql/wits_app.sql:97,106,331`)
`code varchar(50) DEFAULT NULL` and `photo varchar(255) DEFAULT NULL`, with `UNIQUE` only on `school_id` —
so omitting `code` is safe because it is **nullable**, not because of any `UNIQUE` constraint (import has
no RFID-`code` source; that is the actual reason to exclude it).

**`check_duplicates.php`:**

1. `requireAdminAuth($conn)`.
2. Read `school_ids` from `$_POST['school_ids']` as a `json_decode`'d array (moved off the raw JSON body to
   match the form transport).
3. Response shape **unchanged**: `{status:"success", duplicates:[...]}`.

### 4.7 Repo hygiene — contract doc

Add a contract doc at **`docs/superpowers/contracts/2026-08-11-phase4a3-import-endpoints.md`** documenting
the new request/response shapes for **both** endpoints, and stating explicitly that **Composer /
PhpSpreadsheet is NO LONGER REQUIRED** for `upload_students_zip.php` (only core `ext-zip`, for the optional
photos ZIP). Note the guard is **BREAKING**: an old client without `admin_key` threading will 401.

---

## 5. Backend contract (before / after)

Placeholders only — no real keys. `<ADMIN_KEY>` denotes the RAM-only admin key.

### 5.1 `check_duplicates.php`

**Before** — JSON body, unguarded:

```
POST check_duplicates.php
Content-Type: application/json
{ "school_ids": ["21-1-0001", "21-1-0002"] }
→ 200 { "status":"success", "duplicates":["21-1-0001"] }
```

**After** — form fields, guarded:

```
POST check_duplicates.php
Content-Type: application/x-www-form-urlencoded
school_ids=["21-1-0001","21-1-0002"]   (JSON-array string)
admin_key=<ADMIN_KEY>
→ 200 { "status":"success", "duplicates":["21-1-0001"] }   (shape unchanged)
→ 401 { ...auth-failure body... }                          (missing/invalid key)
```

### 5.2 `upload_students_zip.php`

**Before** — raw Excel file part; server re-parses via PhpSpreadsheet; `skip_ids` ignored; plain-text
reply; fabricated counts; unguarded.

```
POST upload_students_zip.php  (multipart/form-data)
  excel=<students.xlsx>       ← form part named "excel" ($_FILES['excel']); server re-parses, positional A–G
  photos_zip=<photos.zip>     (optional)
  skip_ids=21-1-0001,...      ← IGNORED
→ 200 text/plain "✅ Upload complete!"   (no real counts)
```

**After** — client-sent rows; guarded; honors `skip_ids`; real JSON counts; no Composer.

```
POST upload_students_zip.php  (multipart/form-data)
  rows=[{"school_id":"21-1-0001","name":"...","course":"...","department":"...",
         "year_level":"...","gender":"...","status":"..."}, ...]   (JSON-array string field)
  admin_key=<ADMIN_KEY>
  skip_ids=21-1-0001,...      (comma-joined; honored)
  photos_zip=<photos.zip>     (optional; core ZipArchive, glob *school_id*)
→ 200 { "status":"success",
        "success_count": <int>, "skipped_count": <int>, "error_count": <int>,
        "message": "..." }
→ 401 { ...auth-failure body... }
```

INSERT writes: `school_id, name, course, department, year_level, gender, status, photo`
(`photo` = ZIP-matched path for the row's `school_id`, else `NULL`). `code`, `visits` → DB defaults.

---

## 6. Dialog flow (state machine)

`Import` is enabled only when a **data file** is chosen. Busy / re-entry guarded so `Import` cannot
double-fire.

1. **Idle** — user picks Excel/CSV (**REQUIRED**) + optional photos ZIP. `Import` button enabled only once a
   data file is chosen.
2. **Parse (client-side)** — `parseExcel` for `.xlsx`, `parseCsv` for `.csv`, chosen **by extension**.
3. **Client-side validate** (see §7).
   - **On validation failure** → show inline friendly error, **stay in Idle**.
4. **Check duplicates** — `checkDuplicates(schoolIds, adminKey)` (busy). Then on `duplicatesResolved`:
   - **(a) none** → proceed to upload.
   - **(b) some** → inline "**N of M** rows already exist and will be skipped" with **Continue / Cancel**.
   - **(c) ALL rows duplicates** → "**Nothing to import**" with **Close only**.
5. **Upload (on Continue)** — `uploadStudents(rows, zipPath, skipIds, adminKey)`.
   - `uploadStarted` → **byte-progress bar** (`uploadProgress` percent).
   - After bytes sent → **indeterminate "Processing…"** (no fake server-row progress).
6. **Result** — `uploadFinished(result)` → toast/summary showing **REAL counts**
   "**X imported · Y skipped · Z failed**" + close dialog.
7. **Failure** — `uploadFailed` **OR** 401 → inline error; **dialog stays open**.

```
Idle ──pick data file(+opt ZIP)──▶ [Import enabled]
  └─▶ Parse ─▶ Validate ──fail──▶ (inline error, back to Idle)
                     │ok
                     ▼
              CheckDuplicates(busy)
                     │ duplicatesResolved
        ┌────────────┼─────────────────────────┐
     none          some (N/M)              all-dupes
        │      Continue │ Cancel→Idle       Close-only→(end)
        ▼              ▼
            Upload ─uploadStarted▶ byte-progress ─▶ "Processing…"(indeterminate)
                     │ uploadFinished(result)          │ uploadFailed / 401
                     ▼                                  ▼
        Result: "X imported · Y skipped · Z failed"   inline error (dialog stays open)
        + close dialog
```

---

## 7. Client-side validation

Client validation is the **fast, friendly UX layer** (lesson ①). **The server ALSO validates
independently** (§4.6) — client validation is for UX; **server validation protects the data**.

- **Required-column resolution.** The header map must resolve **both** `school_id` **and** `name`; else
  inline "**Missing required column: School ID**" / "**Name**", **naming which headers were found**.
- **Per-row `school_id`.** Each data row must have a non-empty `school_id`; else "**Row N has no School
  ID**" (report the first few offending rows).
- **Extra columns are IGNORED**, not an error — only the **required** columns must be present.
- **Column ORDER does not matter** (header-mapped, not positional).
- **Format hint.** The dialog shows an explicit hint **listing the recognized columns**.
- **Download Template button (approved, in scope).** An optional helper that writes a small sample
  CSV/XLSX showing the expected columns, so the admin can see the expected format.

---

## 8. Scope decisions

- **Formats:** `.xlsx` + `.csv` (both already parsed by the existing controller).
- **Fields written:** the **7 core** — `school_id, name, course, department, year_level, gender, status` —
  **plus `photo`** (written from the ZIP match for that `school_id`, else `NULL`; the photo value comes
  **only** from the ZIP match, never from a file column). `code` and `visits` are **EXCLUDED** even if
  present in the file — `code` because import has no RFID-code source (it is nullable in the tracked
  schema, so omitting it is safe), `visits` because import must not seed visit counts.
- **Extra columns ignored;** only required columns (`school_id`, `name`) must be present.
- **Progress:** upload bytes, then indeterminate "Processing…"; **no fabricated** server-side row progress.
  Note the byte bar is only meaningful when a **photos ZIP** is attached — the `rows` JSON field is tiny, so
  a rows-only import jumps to 100% instantly and the real wait is the indeterminate "Processing…" phase.
- **Duplicate resolution:** inline skip-confirm in the **same** dialog (Continue / Cancel; all-dupes →
  Close). The backend can only **SKIP** (no overwrite path) — **do not offer overwrite**.
- **Result:** real counts "X imported · Y skipped · Z failed", from the new JSON response.

---

## 9. Security

- **Guard both endpoints** with `requireAdminAuth` **before any read or mutation**.
- **`admin_key` is RAM-only** (held by `AdminSession`), sent in the **POST body only**, **never logged**,
  **never persisted**.
- **401-with-body** is routed to a clear **auth** message via the shared reply classifier (auth vs generic
  split), not a raw network error.
- **All server-provided strings render `Text.PlainText`** in QML.
- **ZIP photo filename glob (`*school_id*`)** is a **pre-existing path-shape concern**, now **admin-gated**.
  Full sanitize / content-sniff / store-outside-webroot is **deferred to Phase 6**. Note it; **do not fix
  here**.

---

## 10. Testing (TDD)

Register tests via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add **`OFFSCREEN`** for any
GUI/Quick/painting test. Full suite stays green (`ctest --test-dir <build> --output-on-failure`).

**Pure `serializeRows` unit tests** (the primary new seam):
- column mapping via `headerIndex`,
- `code` / `visits` **excluded**,
- extra/unrecognized columns **ignored**,
- value **trimming**,
- empty / ragged rows.

**Keep existing `parseCsv` / `parseExcel` tests.**

**`parseUploadResponse` tests** updated for real `success_count` / `skipped_count` / `error_count`, **and
keep the plain-text fallback test**.

**Request-assembly tests** (`qt-app/testsupport/capturingnam.*`):
- `uploadStudents` posts **multipart** with a `rows` **JSON field** + `admin_key` + `skip_ids`,
- `checkDuplicates` posts **form-encoded** `school_ids` **JSON** + `admin_key`.

**`ImportViewModel` state-machine tests** (network-free): idle → dup → upload → result; the **all-dupes**
path; the **401** path.

**`ImportStudentsDialog` QuickTest fixtures** (`OFFSCREEN`): the state transitions and the all-duplicates
"**Close only**" path. Follow the prior-phase QuickTest fixture conventions — **own y-band + raised root
height**; if any double-click is needed, use **`MouseArea.onDoubleClicked`, not `TapHandler`**
(QTBUG-102441).

**Backend — manual verification checklist** (deploy the guarded endpoints, then):
- negative-auth **401** with **no** key,
- positive-auth import **with** and **without** the photos ZIP,
- duplicate skip produces a **real `skipped_count`**,
- a row-level failure produces a **real `error_count`**,
- **legacy `WITS.exe` import still works** end-to-end.

---

## 11. Out of scope (explicit)

- **The legacy login numeric heuristic fix** (`mainwindow.cpp:193`) — a separate follow-up, plus a **check
  of whether the Quick `KioskViewModel` shares** the same numeric-vs-admin heuristic. (Import is immune —
  §2 bonus.)
- **HTTPS / transport hardening, `admin_key` `+`-urlencode corruption, session tokens** — Phase 6.
- **Overwrite-on-duplicate** — the endpoint has no such path.
- **Importing `code` / `visits` columns.**

---

## 12. Deployment note

The backend guard is **BREAKING**. Both `upload_students_zip.php` and `check_duplicates.php` must be
**deployed WITH this client** — an old client without `admin_key` threading will receive a **401** and
import will fail. Conversely, the new client must not be shipped against unguarded endpoints (its transport
changed to form fields carrying `admin_key`). Ship client + endpoints together, matching the 4a.2b-iv
guarded-deploy pattern.

**Composer is no longer needed.** `upload_students_zip.php` no longer requires
`PhpOffice\PhpSpreadsheet` or `vendor/autoload.php`; the only PHP extension it needs is **core `ext-zip`**,
and only for the **optional** photos ZIP.
