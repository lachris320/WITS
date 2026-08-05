# Design Spec: LOAMS 2.0 Phase 4a.2b-iv — Register Student + Photo

**Date:** 2026-08-05
**Status:** Approved
**Scope:** Fourth (final) mutation slice of the Database screen (Phase 4a track, LOAMS 2.0)

---

## Context & Roadmap

The **last of four slices** decomposing Phase 4a.2b (Database mutations):

- **4a.2b-i (MERGED, PR #32)** — multi-delete + CSV export.
- **4a.2b-ii (MERGED, PR #34)** — single-student edit (`StudentEditDialog`).
- **4a.2b-iii (MERGED, PR #36)** — bulk edit + change preview (`BulkEditDialog`,
  `LCheckbox`, the `m_editController`/`CourseTarget` course-routing this slice
  extends).
- **4a.2b-iv (THIS spec)** — register a new student, with an optional photo
  upload, plus the backend security fix that finally guards the
  currently-unauthenticated `register_student.php`.

This is the **riskiest slice** (deliberately isolated last): it introduces a new
`QHttpMultiPart` file-upload path, a file-picker, and a **breaking backend
change** (guarding an endpoint) that must be deployed together with a legacy
client fix.

**Scope decisions (owner, 2026-08-05):**

- **No `checkDuplicates`.** `register_student.php` already performs its own
  duplicate check (`SELECT id FROM students WHERE school_id`) **before** it
  handles the photo, returning `status:"duplicate"`. So a single-student
  register needs no separate pre-check; the response drives an inline error.
  `check_duplicates.php` stays untouched (that pre-check belongs to the bulk
  import, 4a.3, if ever).
- **Backend: guard only.** Add `requireAdminAuth` to `register_student.php` and
  fix the legacy call-site to send the key. The endpoint already force-renames
  every upload to `<school_id>_<timestamp>.<validated-ext>` (ext ∈ jpg/png/gif),
  so the classic upload-a-`.php` RCE vector is already neutralized; content
  sniffing / store-outside-webroot is a **Phase 6** follow-up, not this slice.
- **Register lives on the Database screen** as a header button + a new
  `RegisterStudentDialog` (mirroring the edit dialogs), not a separate screen.
- **Register state lives in `DatabaseViewModel`** (uniform with single/bulk
  edit), reusing `m_editController` with a 3-valued `CourseTarget`.
- **Photo is upload-only.** Rendering student photos (`LAvatar` on kiosk/search)
  is a separate roadmap track; this slice shows only the *selected local
  filename*, no image preview.

---

## Project Constraints

- **Platform:** Qt 6 / C++17 / QML (Qt Quick Controls 2), CMake + Ninja.
- **Strict MVVM:** C++ ViewModels are the ONLY QML-facing layer; QML screens
  take `property var vm` (a `DatabaseViewModel`, or a plain-QML stub in
  QuickTests). QML **never** calls a `witscore` controller directly.
- **Theming:** `Theme.qml` is the single source of every visual token. ZERO raw
  hex outside `Theme.qml`.
- **Tests:** QtTest (C++) + Qt Quick Test under CTest, registered via
  `wits_add_qttest()` (OFFSCREEN for GUI/Quick/network tests).
- **Security:** admin key is RAM-only via `AdminSession::instance().key()`, sent
  in the POST body, never logged, never rendered. Any server-supplied string
  rendered in QML MUST be `Text.PlainText` (cleartext-HTTP injection guard).
- **No new shared component** is required by this slice (Fable UI review
  confirmed: use plain error `Text`, a button label-swap for busy, and the
  existing `LDialog`/`LTextField`/`LComboBox`/`LCascadingSelect`/`LButton`).

---

## The Backend Contract

### `register_student.php` (today — unauthenticated)

`POST` (multipart form-data) with fields `code, name, school_id, year_level,
course, department, gender, status` (+ optional `visits`, defaulted 0) and an
optional `photo` file. Order of operations:

1. Requires `school_id` and `name` (else `status:"error"`).
2. **Duplicate check** on `school_id` → `status:"duplicate"` (returns *before*
   any photo handling — a dup never orphans an upload).
3. Photo (if present): validates extension ∈ {jpg,jpeg,png,gif} and size ≤ 5 MB,
   saves to `uploads/<school_id>_<time>.<ext>` (attacker controls neither the
   stored name nor extension).
4. `INSERT` the student (+ `photo` path, `time_date NOW()`).
5. Response `status`: `success` | `duplicate` | `error`, with a `message`.

### The change: add the admin-key guard

Insert immediately after `include "db.php";` (so it 401s before *any* DB read,
dup check, or file write):

```php
include "auth_helper.php";
requireAdminAuth($conn);   // 401 "Admin authentication required" before anything
```

Multipart form-data populates `$_POST`, so `requireAdminAuth` reads
`$_POST['admin_key']` (bcrypt) exactly as the other four guarded endpoints do.
**This is breaking**: any client that doesn't send `admin_key` now 401s — hence
the legacy fix below. Deploy `register_student.php` to XAMPP together with the
client (like 4a.1 Task 10), and negative-auth verify (401 without key) +
positive verify (success with key).

### Legacy call-site fix (required)

`adminwindow.cpp:696` builds its own inline multipart register POST and sends
**no** `admin_key`. After the guard deploys it would 401. Add one line to its
`addPart(...)` block:

```cpp
addPart("admin_key", m_adminKey);   // m_adminKey retained via setAdminKey (added in 4a.1)
```

(Confirm `m_adminKey` is in scope at the register lambda; it is the same member
threaded through the shared-controller call-sites in 4a.1. This is the exact
pattern 4a.1 used when it guarded the dept-ops endpoints.)

---

## Architecture

### Entry point

A new header button on `DatabaseScreen`, **`＋ Add Student`** (Outline variant,
placed at the left of the action group so the *create* action reads distinctly
from the selection-dependent Edit/Delete). It is enabled independent of
selection (you can register any time; disabled only while `vm.loading`).
`onClicked → vm.beginRegister()` → the VM resets a clean form and emits
`registerReady` → `RegisterStudentDialog` opens.

### `RegisterStudentDialog` (new `qt-app/quick/qml/admin/RegisterStudentDialog.qml`)

An `LDialog`-based modal, `property var vm`. Title **"Register student"**; submit
button **"Register"**. It is a *fresh* form (no prefill from existing data), so
it needs **none** of the combo-severance/`prefilling` guard machinery the edit
dialogs carry — `onVisibleChanged` just resets controls to their placeholder
state and focuses School ID.

**Field order & grouping** (Fable UX review — identity → academic → photo, height
budgeted for `LDialog`'s no-scroll 460px card):

1. **School ID \*** (`LTextField`) — required, editable. Directly under it: an
   inline error `Text` (`visible: vm.regDuplicate`, `Text.PlainText`, error
   token) reading "This School ID already exists."
2. **Name \*** (`LTextField`) — required.
3. **Code** (`LTextField`) — optional.
4. **Department → Course** (`LCascadingSelect`) — dependent cascade; course model
   is `vm.regCourses`.
5. **Year Level** (`LTextField`) — optional free text; `placeholderText`
   "e.g. 1, 2, 3, 4".
6. **Gender** + **Status** paired on one `RowLayout` (two `LComboBox` ~50/50) —
   saves a row of height; Gender ∈ [Male, Female], Status ∈ [Active, Inactive].
7. **Photo (optional)** row (see below).
8. A muted "\* required" caption; footer `RowLayout` right-aligned
   **[Outline Cancel | Primary Register]**.

**Required-field affordance:** only School ID and Name get the `" *"` label
suffix (mark the minority). The Register button is
`enabled: vm.canRegister && !vm.regBusy` (where `canRegister` = trimmed School ID
**and** Name non-empty).

**Photo picker (no preview):** a labeled "Photo (optional)" row:
- **Empty state** — an Outline `LButton` "Choose photo…" + muted `Text`
  "No photo selected — JPG, PNG, or GIF, up to 5MB" (stating the constraints
  up front is the operator's only confidence signal with no preview).
- **Chosen state** — the button flips to "Change photo…"; `vm.regPhotoName` shown
  at `Theme.text`, `Text.ElideMiddle` (keeps the extension visible); + a muted
  (NOT Danger-red) "Remove" control → `vm.clearRegPhoto()` returns to empty.
- A `FileDialog` (`QtQuick.Dialogs`, already a dependency since 4a.2b-i) with
  `nameFilters: ["Images (*.jpg *.jpeg *.png *.gif)"]`; `onAccepted →
  vm.setRegPhoto(selectedFile)`.

**Busy / submit feedback:**
- `regBusy` in flight → the Register button label swaps to **"Registering…"** and
  disables; Cancel also disables (state can't be torn down mid-request). No
  spinner component.
- **Enter-to-submit:** Return in any text field triggers `vm.registerStudent()`
  **only when `canRegister && !regBusy`** (same guard, one path) — safe for
  RFID/barcode wedges that append Enter after the School ID (a scan into an
  otherwise-empty form leaves Name blank → button disabled → no half-submit).
- **Esc-to-cancel** closes (blocked while `regBusy`); the `LDialog` scrim is
  non-dismissing (an accidental click must not discard a filled form).

**Focus management (Fable UX):**
- On open → focus School ID (also the field a wedge scanner types into).
- On a **duplicate** result → return focus to School ID and select-all its text
  (the fix is one keystroke away); the inline error clears on the next School ID
  keystroke (the error describes a specific rejected value — once the value
  changes, the claim is stale).
- On **success** → dialog closes and focus returns to the students table
  (`studentsTable.forceActiveFocus()`), so the operator can keep working the list.
- Every control carries an `objectName` (QuickTest seam).

### Duplicate & error handling (shared `databaseToast`, consistent with edit/bulk)

Owner decision: register **failures use the shared `databaseToast`** (uniform
with single/bulk edit), and only the **duplicate** case is a field-specific
inline error.

- **`success`** → toast "Registered %1" (the entered name; falls back to
  "Student registered" if name is somehow empty — it can't be, given
  `canRegister`) + `reloadTable()` + close + restore table focus.
- **`duplicate`** → `vm.regDuplicate = true` → inline error under School ID; **no
  toast, no reload**; dialog stays open, data preserved, focus returns to School
  ID.
- **`error` (generic)** → shared `databaseToast` with the server message; dialog
  stays open, data preserved.
- **401 held-key** → routed via the same `SettingsViewModel::isAuthFailureMessage`
  split (`applyServerRejection`) as delete/bulk → auth toast + `authFailure`;
  dialog stays open.
- **network failure** → `registerFailed` → toast "Registration failed — check
  your connection."; dialog stays open.

### Shared `StudentController` (witscore)

**New async method** (mirrors `ImportController::uploadStudents`'s multipart
pattern exactly):

```cpp
// Builds a QHttpMultiPart: text parts for code/name/school_id/year_level/
// course/department/gender/status + admin_key; an optional `photo` QHttpPart
// (QFile body device, file->setParent(multiPart)) when photoFilePath is
// non-empty. POSTs to register_student.php; multiPart->setParent(reply).
// On finished: registerFailed on a genuine transport error (replyIsServerAnswer
// false), else registerFinished(parseRegisterResponse(body)).
void registerStudent(const StudentRecord &rec, const QString &photoFilePath,
                     const QString &adminKey);
```

- If the photo file fails to open → emit `registerFailed("Could not open the
  photo file.")`, free the multipart, and return (as legacy does) — never send a
  half-built request.
- Does **not** send `visits` (endpoint defaults 0; a new student has none).

**New pure parser** (the testable core):

```cpp
// status "success" -> Success; "duplicate" -> Duplicate; else/invalid -> Error.
// outMessage = server "message" (or a default). Reuses replyIsServerAnswer so a
// guard 401-with-body reaches this parser instead of being a network error.
static RegisterOutcome parseRegisterResponse(const QByteArray &raw, QString &outMessage);
```

`enum class RegisterOutcome { Success, Duplicate, Error };` in `studentdata.h`.

**New signals:** `registerFinished(RegisterOutcome outcome, const QString &message)`,
`registerFailed(const QString &errorString)`.

> **Testability note:** `QHttpMultiPart` request *assembly* is not readily
> introspectable through the `CapturingNam` harness (the body is a multipart
> device). So the unit-testable core is **`parseRegisterResponse`** (pure) plus
> the VM's `onRegisterFinished` mapping and gating; the multipart body itself is
> GUI-verified against XAMPP. A light "`registerStudent` enters the guarded path
> (sets `regBusy`, doesn't early-return)" VM test mirrors `saveEditEntersGuardedPath`.

### `DatabaseViewModel` additions

**New Q_PROPERTYs** (register form state):

- `QString regSchoolId / regName / regCode / regYearLevel / regGender / regStatus`
  (WRITE `setRegX`, NOTIFY).
- `QString regDepartment` (NOTIFY; set via `setRegDepartment`), `QString regCourse`
  (WRITE, NOTIFY).
- `QStringList regCourses` (READ, NOTIFY) — dependent course list.
- `QString regPhotoName` (READ, NOTIFY) — display filename of the picked photo
  ("" when none).
- `bool canRegister` (READ, NOTIFY) — `regSchoolId.trimmed()` **and**
  `regName.trimmed()` non-empty.
- `bool regBusy` (READ, NOTIFY) — in-flight guard.
- `bool regDuplicate` (READ, NOTIFY) — drives the inline School-ID error.

**New Q_INVOKABLEs:**

- `beginRegister()` — reset all `reg*` fields to empty, `regDuplicate=false`,
  `m_regPhotoPath` clear; emit `registerReady()`.
- `setRegSchoolId(v)` — on change, also clear `regDuplicate` (stale claim).
- `setRegName/Code/YearLevel/Gender/Status(v)`, `setRegCourse(v)`.
- `setRegDepartment(dept)` — set + clear `regCourse` + `m_courseTarget =
  CourseTarget::Register` + `m_editController->loadCourses(dept)`.
- `setRegPhoto(const QUrl &fileUrl)` — store `fileUrl.toLocalFile()` in
  `m_regPhotoPath`, derive `regPhotoName` = `QFileInfo(...).fileName()`.
- `clearRegPhoto()`.
- `registerStudent()` — guard `regBusy || !canRegister` → return; build a
  `StudentRecord` from the `reg*` fields (visits 0); `regBusy=true`;
  `m_controller->registerStudent(rec, m_regPhotoPath, AdminSession::instance().key())`.

**New public slots (test seam), wired to the primary `m_controller` in the ctor:**

- `onRegisterFinished(RegisterOutcome outcome, const QString &message)`:
  - `regBusy=false`.
  - `Success` → `setAuthFailure(false)`; `setStatusMessage(tr("Registered %1").arg(m_regName))`;
    `reloadTable()`; emit `registerFinished()`.
  - `Duplicate` → `regDuplicate=true` (no toast, no reload; dialog stays open).
  - `Error` → `applyServerRejection(message, tr("Registration failed."))` (auth
    vs generic, both via the shared toast; dialog stays open).
- `onRegisterFailed(const QString &errorString)` → `regBusy=false`;
  `setStatusMessage(tr("Registration failed — check your connection."))`.

**New signals:** `registerReady()`, `registerFinished()`, plus `*Changed()` for
each new property.

**`CourseTarget` gains `Register`:** `enum class CourseTarget { SingleEdit,
BulkEdit, Register }`. `onEditCoursesLoaded` routes: `Register → m_regCourses`
(+`regCoursesChanged`), `BulkEdit → m_bulkCourses`, else `m_editCourses`.
(Register needs no `EditMode` — it has its own `registerFinished` signal, so no
mode-routed finish like bulk/single share.)

**Private members:** the `reg*` state above + `QString m_regPhotoPath`.

### `DatabaseScreen` wiring

- The `＋ Add Student` header button → `vm.beginRegister()`.
- A `RegisterStudentDialog { vm: screen.vm }` instance; a `bulkEditConfirm`-style
  declaration order is not relevant here (no stacked confirm).
- `Connections` on the vm: `onRegisterReady → registerDialog.visible = true`;
  `onRegisterFinished → { registerDialog.visible = false; studentsTable.forceActiveFocus(); }`.
- Failures already flow through the existing `databaseToast`
  (`onStatusMessageChanged`) — no new toast wiring.

### Shared `StudentDialog` structure (considered — deferred)

`RegisterStudentDialog`, `StudentEditDialog`, and (loosely) `BulkEditDialog`
share an `LDialog` shell + a column of student-field rows (Name, Dept→Course,
Year, Gender, Status) + a right-aligned Cancel/primary footer. A shared
`StudentDialog` base is worth **considering** — but **not extracting in this
slice**, because:

- It would touch the **shipped** `StudentEditDialog` in the riskiest slice
  (new backend guard + file upload) — needless blast radius.
- The three dialogs differ materially (School ID read-only vs editable vs absent;
  photo/Code only in register; tri-state in bulk; prefill vs fresh), so the right
  seam isn't yet obvious — a premature base risks conditional-soup.

**What this slice does instead:** build `RegisterStudentDialog` with its field
rows, labels, and footer **deliberately parallel** to `StudentEditDialog` (same
control order for the shared fields, same footer shape), so a later extraction is
mechanical. The extraction is recorded as a **tracked follow-up** (rule of three:
after this lands there are two close cousins; a shared base is justified once a
third form — or a concrete reuse need — appears).

---

## Data Flow (register, happy path)

```
[＋ Add Student] → vm.beginRegister() (reset reg*, regDuplicate=false) → registerReady
   → RegisterStudentDialog opens, focus School ID
operator fills School ID + Name (+ optional Code/Year/Dept→Course/Gender/Status/photo)
   setRegDepartment → m_courseTarget=Register → loadCourses → onEditCoursesLoaded → regCourses
   setRegPhoto(url) → regPhotoName; canRegister flips true when School ID + Name non-empty
   → Register (enabled) → vm.registerStudent()
   → guard(!regBusy && canRegister) → build StudentRecord → regBusy=true (button "Registering…")
   → controller.registerStudent(rec, photoPath, AdminSession key) → multipart POST register_student.php
   → onRegisterFinished(outcome, message)
        Success   → toast "Registered <name>" → reloadTable() → registerFinished → close + focus table
        Duplicate → regDuplicate=true (inline under School ID) → dialog stays open, focus School ID
        Error     → applyServerRejection(...) → shared toast → dialog stays open
   → (network) onRegisterFailed → toast → dialog stays open
```

---

## Error Taxonomy

| Condition | Signal path | Surface | Dialog |
|-----------|-------------|---------|--------|
| Success | `registerFinished`, Success | toast "Registered <name>" + reloadTable | closes, focus → table |
| Duplicate ID | `registerFinished`, Duplicate | inline error under School ID | stays open, focus → School ID |
| Generic server error | `registerFinished`, Error (not auth) | shared `databaseToast`, server message | stays open |
| Stale admin key (401-with-body) | `registerFinished`, Error + `isAuthFailureMessage` | shared toast, `authFailure` | stays open |
| Transport failure | `registerFailed` | shared toast | stays open |

The 401-with-body vs transport split is handled by `replyIsServerAnswer`; the
auth-vs-generic split within a rejection is the existing `applyServerRejection`.

---

## Testing Strategy

**Unit — `tst_studentcontroller` (pure):**
- `parseRegisterResponse`: `success`→Success (+message), `duplicate`→Duplicate,
  `error`→Error (+message), invalid JSON→Error ("Invalid server response.").

**Unit — `tst_databaseviewmodel` (network-free, via public slots):**
- `canRegister` gating: false until both School ID and Name (trimmed) non-empty;
  whitespace-only doesn't satisfy it.
- `beginRegister` resets all `reg*` + clears `regDuplicate` + emits `registerReady`.
- `setRegSchoolId` clears `regDuplicate`.
- `setRegDepartment` routes the course load to `regCourses` (`CourseTarget::Register`)
  and clears `regCourse` — asserted via the `regCourses`/course-target seam
  (no capturing NAM — the VM's NAM isn't injectable).
- `setRegPhoto`/`clearRegPhoto` set/clear `regPhotoName`.
- `onRegisterFinished` mapping: Success → status "Registered <name>" + reload +
  `registerFinished`; Duplicate → `regDuplicate` true, no `registerFinished`;
  Error(auth) → `authFailure`; Error(generic) → server message; all clear `regBusy`.
- `regBusy` re-entry guard: a second `registerStudent()` before a result is a
  no-op; clears on the next result.

**QuickTest — `tst_qml_admin` (OFFSCREEN, plain-QML stub vm):**
- `＋ Add Student` button → `beginRegister`; `registerReady` opens the dialog,
  `registerFinished` closes it.
- Register button disabled until School ID + Name present; label swaps to
  "Registering…" when the stub reports `regBusy`.
- Duplicate → inline error visible under School ID; clears on School ID edit.
- Dept→Course cascade drives `setRegDepartment`; photo-pick shows the filename,
  Remove clears it.
- New fixture gets its own non-overlapping y-band + raised root height
  ([[quicktest-taphandler-doubletap-unusable]] fixture rule).

**Backend / GUI:**
- Deploy `register_student.php` (guarded) to XAMPP; negative-auth (401 without
  key) + positive (success with key) verify, like 4a.1 Task 10.
- GUI smoke (WITSQuick + XAMPP): register a new student with and without a photo;
  a duplicate ID shows the inline error; the table refreshes and the toast names
  the student. Also build the **legacy** WITS.exe and confirm its register still
  works (now sends `admin_key`).

---

## Out of Scope

- Photo **display** (`LAvatar` on kiosk/search) — separate roadmap track.
- `checkDuplicates` / guarding `check_duplicates.php` — not used here.
- File-**content** hardening (MIME sniff, store outside webroot) — Phase 6.
- Extracting a shared `StudentDialog` base — tracked follow-up (see above).
- Bulk Excel/ZIP import — 4a.3.
- Editing an existing student's photo — this slice registers new students only.
- Cleartext-HTTP transport and `+`-in-admin-key encoding — Phase 6, house-wide.

---

## Open Questions

None. All forks resolved during brainstorming (2026-08-05): rely on the
endpoint's built-in duplicate check (no `checkDuplicates`); Register button +
dialog on the Database screen; backend guard-only (+ legacy call-site fix);
register state in `DatabaseViewModel`; failures via the shared `databaseToast`
(duplicate inline). Fable 5 UI/UX review folded in: `＋ Add Student` label,
identity→academic→photo order with Gender+Status paired, required-field markers,
"Choose/Change/Remove" photo states with stated constraints, "Registering…" busy
label, autofocus + refocus-on-duplicate + restore-table-focus-on-success,
name-in-success-toast, and a deliberately-parallel structure to
`StudentEditDialog` for a future shared-base extraction.
