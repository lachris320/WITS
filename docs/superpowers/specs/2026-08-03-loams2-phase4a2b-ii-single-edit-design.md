# Design Spec: LOAMS 2.0 Phase 4a.2b-ii — Single-Student Edit

**Date:** 2026-08-03
**Status:** Approved
**Scope:** Second mutation slice of the Database screen (Phase 4a track, LOAMS 2.0)

---

## Context & Roadmap

This is the **second of four slices** that decompose Phase 4a.2b (Database
mutations). The decomposition:

- **4a.2b-i (MERGED, PR #32)** — multi-delete + CSV export. Established the
  header-button + `LConfirmDialog` + `LToast` + `reloadTable()` + error-taxonomy
  patterns this slice builds on.
- **4a.2b-ii (THIS spec)** — single-student edit. Reuses
  `bulkUpdateStudents` with a one-element list.
- **4a.2b-iii** — bulk edit + duplicate preview (adds `checkDuplicates` to the
  controller).
- **4a.2b-iv** — register student (form + file-picker photo via
  `QHttpMultiPart` + ADD an admin-key guard to the currently-unauthenticated
  `register_student.php`).
- **4a.2c (separate track)** — department deactivate-vs-delete.

**Predecessors on master.**

- **4a.2a** — the read-only Database screen: `StudentsTableModel` (a
  `QAbstractListModel` with refresh-surviving multi-select keyed by `schoolId`),
  the Dept → Course filter, and `DatabaseViewModel` wrapping the shared
  `StudentController`.
- **4a.2b-i** — delete + CSV export; established the header-button,
  `LConfirmDialog`, `LToast`, `reloadTable()`, and error-taxonomy patterns.
- The merged **csvutil dedup** (PR #33).

This slice adds **editing one student** on top of the existing multi-select
table.

---

## Project Constraints

- **Platform:** Qt 6 / C++17 / QML, built with CMake + Ninja.
- **Strict MVVM:** C++ ViewModels are the ONLY QML-facing layer. QML screens
  take `property var vm` so QuickTests can inject a plain-QML stub VM. QML
  **never** calls a `witscore` controller directly.
- **Theming:** `Theme.qml` is the single source of visual tokens. Zero raw hex
  outside `Theme.qml`; opacity variants use `Qt.alpha(Theme.<token>, a)`, never
  a literal color.
- **Tests:** QtTest (C++) + Qt Quick Test under CTest, registered via
  `wits_add_qttest()`. Add `OFFSCREEN` for any GUI / Quick / painting / network
  test.
- **Backend:** PHP over cleartext HTTP. `bulk_update_students.php` is
  **admin-key guarded** and transactional. No new endpoint is introduced in this
  slice.

---

## The Backend Contract

`bulk_update_students.php` (guarded, transactional) UPDATEs the columns
`name, course, year_level, department, gender, status` WHERE `school_id`. Precise
consequences for this slice:

- The **editable fields are exactly those six.**
- `school_id` is the **immutable identity** — it is the WHERE key, never
  modified.
- `code`, `visits`, and `photo` are **NOT touched** by this endpoint.
- A **no-op edit** (no field actually changed) returns `status:success` with
  `updated:0`. `affected_rows == 0` is **not** a failure.
- Any **real failure rolls the whole transaction back.**

The shared controller method already exists:

```cpp
void StudentController::bulkUpdateStudents(const QList<StudentRecord> &students,
                                           const QString &adminKey);
// signals:
//   bulkUpdateFinished(const BulkUpdateResult &result);
//   bulkUpdateFailed(const QString &errorString);
//
// struct BulkUpdateResult { bool ok; int updatedCount; QStringList errors; QString message; };
```

It posts the students as a **JSON array in the `students` FORM field** plus an
`admin_key` FORM field.

`StudentRecord` (`qt-app/core/studentdata.h`):

```cpp
struct StudentRecord {
    QString code;
    QString schoolId;
    QString name;
    QString course;
    QString department;
    QString yearLevel;
    QString gender;
    QString status;
    int     visits;
};
```

---

## Approach

**Reuse the 4a.2b-i pattern:** a modal `LDialog` form; the ViewModel orchestrates
the save via `bulkUpdateStudents([oneRecord], key)`; QML owns the dialog.

Everything is keyed by `school_id` — stable across the Dept → Course filter.
**Never a row index**, which the filter reorders.

---

## Locked Decisions

Recorded as settled — do NOT relitigate.

### Edit trigger

- A header **Edit** `LButton` in the count-header row, positioned **LEFT of
  Delete**, enabled ONLY when exactly one row is selected
  (`canEdit == (selectedCount == 1)`), with tooltip *"Select exactly one student
  to edit"*.
- PLUS **double-click a row / Enter on the focused row** opens that student's
  edit dialog.
- **No per-row pencil column** this slice (YAGNI — deferred).

### Editable fields / inputs

- **name** — `LTextField`, required.
- **school_id** — read-only display (identity).
- **year_level** — free-text `LTextField` (matches legacy; mixed semantics).
- **gender** — `LComboBox` `{Male, Female}`.
- **status** — `LComboBox` `{Active, Inactive}`.
- **department + course** — **dependent dropdowns, NO "All" option.** Department
  `LComboBox` (real values); Course `LComboBox` re-scoped to the chosen
  department. Changing the department **clears** the current course and
  **reloads** the course list.

Legacy vocabulary confirmed from `adminwindow.ui`: gender Male/Female, status
Active/Inactive; year_level was a free-text line edit.

### (A) Pull the bulkUpdate 401-classification into THIS slice

`bulkUpdateStudents` currently has the OLD error-first reply handling — it bails
to `bulkUpdateFailed` on ANY `reply->error()` **without reading the body** (the
same pre-fix shape `deleteStudents` had before 4a.2b-i). This slice is the FIRST
Quick-UI consumer of `bulkUpdate` with the **held admin key**, so a stale-key 401
must surface as an **auth failure**, not a generic transport error.

Reuse the existing pure classifier introduced in 4a.2b-i, but **generalize its
name** from the delete-specific form to a generic one shared by both callers:

```cpp
// True when the reply is a decodable server answer (has an HTTP status +
// body) rather than a transport failure. Mirrors HttpForm::isServerAnswer.
static bool replyIsServerAnswer(bool replyHadError, int httpStatus,
                                const QByteArray &body);
```

(This is the former `StudentController::deleteReplyIsServerAnswer`, renamed — the
logic is unchanged.) Use it in **BOTH** `deleteStudents` and the rewired
`bulkUpdateStudents` reply handler.

Rewire `bulkUpdateStudents` exactly like `deleteStudents`: read body / error /
status **before a single `deleteLater`**, then classify:

- A **server answer** (including a 401-with-body) ⇒
  `bulkUpdateFinished(parseBulkUpdateResponse(body))`.
- A **no-HTTP-status transport failure** ⇒ `bulkUpdateFailed(errorString)`.

Update the existing delete tests / call sites for the rename. This retires the
4a.2b-iii "reuse the classifier for bulkUpdate" follow-up early.

### (B) Edit course-loading via a SECOND `StudentController`

The VM already owns one `StudentController` (filter / search, whose
`coursesLoaded` drives the filter's course chips). Add a **dedicated second
`StudentController`** (its own `QNetworkAccessManager`) for the edit dialog so
the dialog's `editCourses` loads **independently** of the filter's `courses` — no
coupling, no shared-signature change, and no need to disambiguate a shared
`coursesLoaded` stream.

---

## Components & Changes

### 1. `qt-app/core/studentcontroller.{h,cpp}` (shared C++)

- **Rename** `deleteReplyIsServerAnswer` → `replyIsServerAnswer` (generic — see
  decision A).
- **Rewire** `bulkUpdateStudents` to use `replyIsServerAnswer`: read
  body/error/status before one `deleteLater`; a server answer (incl. 401-with-
  body) ⇒ `bulkUpdateFinished(parseBulkUpdateResponse(body))`; a no-status
  transport failure ⇒ `bulkUpdateFailed`.
- No new endpoint.

### 2. `qt-app/quick/viewmodels/DatabaseViewModel.{h,cpp}`

Add:

```cpp
Q_PROPERTY(bool canEdit READ canEdit NOTIFY canEditChanged) // selectedCount == 1

// Edit-form state (each with a NOTIFY signal):
Q_PROPERTY(QString     editSchoolId   READ editSchoolId   NOTIFY editSchoolIdChanged)   // read-only
Q_PROPERTY(QString     editName       READ editName       WRITE setEditName       NOTIFY editNameChanged)
Q_PROPERTY(QString     editYearLevel  READ editYearLevel  WRITE setEditYearLevel  NOTIFY editYearLevelChanged)
Q_PROPERTY(QString     editGender     READ editGender     WRITE setEditGender     NOTIFY editGenderChanged)
Q_PROPERTY(QString     editStatus     READ editStatus     WRITE setEditStatus     NOTIFY editStatusChanged)
Q_PROPERTY(QString     editDepartment READ editDepartment                         NOTIFY editDepartmentChanged)
Q_PROPERTY(QString     editCourse     READ editCourse     WRITE setEditCourse     NOTIFY editCourseChanged)
Q_PROPERTY(QStringList editCourses    READ editCourses    NOTIFY editCoursesChanged) // model for the course combo

Q_INVOKABLE void beginEdit(const QString &schoolId);
Q_INVOKABLE void setEditDepartment(const QString &dept);
Q_INVOKABLE void saveEdit();
```

- **`canEdit`** = `selectedCount == 1`.
- **`beginEdit(schoolId)`** — locate the record in the model, prefill **all**
  `edit*` state from it, and load the courses for its department into
  `editCourses` (via the second controller — decision B).
- **`setEditDepartment(dept)`** — set `editDepartment`, **clear** `editCourse`,
  and **reload** `editCourses` for `dept` (second controller).
- **setters** for `editName` / `editYearLevel` / `editGender` / `editStatus` /
  `editCourse` (Q_INVOKABLE or WRITE).
- **`saveEdit()`** — build a `StudentRecord` from the `edit*` state (`schoolId`
  unchanged; `code` and `visits` carried from the **original** located record
  unchanged), read `AdminSession::instance().key()`, then call
  `bulkUpdateStudents([rec], key)` on the **primary controller** (the existing
  one used for search / delete — NOT the second edit controller). Its
  `bulkUpdateFinished` / `bulkUpdateFailed` drive the handlers below.

Handlers (wire both controller signals in the ctor with function-pointer
`connect`):

- **`onBulkUpdateFinished(const BulkUpdateResult &result)`**
  - `ok && updatedCount >= 1` ⇒ status `"Student updated"` + `reloadTable()` +
    set an `editDone` / close signal.
  - `ok && updatedCount == 0` ⇒ status `"No changes to save"` + close.
  - `!ok` ⇒ if `SettingsViewModel::isAuthFailureMessage(result.message)` set the
    auth-failure state; else set `statusMessage = result.message`. **Keep the
    dialog open on error.**
- **`onBulkUpdateFailed(const QString &errorString)`** ⇒ transient error status.

Second controller:

- Add `StudentController *m_editController` (+ its own `QNetworkAccessManager`),
  **parented**. It is used **only** to load the edit dialog's course list —
  search / delete / bulkUpdate all stay on the primary controller.
- Connect its `coursesLoaded` → an `onEditCoursesLoaded` slot that sets
  `editCourses`. (The primary controller's `coursesLoaded` continues to drive the
  filter's `courses` — the two streams never mix.)

Reuse `statusMessage` / `authFailure` from 4a.2b-i for the toast / auth surface.

### 3. `qt-app/quick/qml/admin/StudentEditDialog.qml` (NEW)

An `LDialog`-based modal form. `property var vm`. A titled form containing:

- **School ID** — read-only `Text` (`textFormat: Text.PlainText`).
- **Name** — `LTextField` bound to `vm.editName`.
- **Department** — `LComboBox` (`model: vm.departments`,
  `currentValue: vm.editDepartment`, `onSelected: vm.setEditDepartment(...)`).
- **Course** — `LComboBox` (`model: vm.editCourses`,
  `currentValue: vm.editCourse`, `onSelected: vm.setEditCourse(...)`).
- **Year Level** — `LTextField` bound to `vm.editYearLevel`.
- **Gender** — `LComboBox` (`model: ["Male", "Female"]`).
- **Status** — `LComboBox` (`model: ["Active", "Inactive"]`).
- **Save** `LButton` — enabled iff `vm.editName` trimmed is non-empty →
  `vm.saveEdit()`.
- **Cancel** `LButton` → `close()`.

Zero raw hex; `Theme` tokens only. One component per file.

### 4. `qt-app/quick/qml/admin/DatabaseScreen.qml`

- Add the **Edit** `LButton` to the count-header `RowLayout`, **LEFT of Delete**
  (`enabled: vm.canEdit`, with `tooltipText` and `accessibleName`). `onClicked`
  calls `vm.beginEdit(<the single selected schoolId>)` then opens the dialog.
- Wire `LTable.onRowActivated(schoolId)` → `vm.beginEdit(schoolId)` + open the
  dialog.
- Instantiate `StudentEditDialog { vm: screen.vm }`, driven visible. The existing
  `LToast` covers the status message.

### 5. `qt-app/quick/qml/components/LTable.qml`

- Add `signal rowActivated(string schoolId)`, emitted on **row double-click** and
  on **Enter on the focused row**.
- It MUST NOT fight the per-row checkbox: the checkbox cell swallows its own
  clicks, so the double-click's first click must not toggle selection.
- **Backward-compatible** — no existing consumer is required to connect it.

---

## Data Flow (Edit)

1. User **selects one** row and clicks **Edit**, OR **double-clicks** a row (or
   presses **Enter** on the focused row).
2. `vm.beginEdit(schoolId)` locates the record, **prefills** all `edit*` state,
   and **loads** `editCourses` for the record's department (second controller).
3. The dialog opens **prefilled**.
4. The user edits fields. Changing the **Department** combo →
   `vm.setEditDepartment(dept)` **reloads** the course list and **clears** the
   selected course.
5. On **Save** ⇒ `vm.saveEdit()` builds the `StudentRecord` (`schoolId`/`code`/
   `visits` unchanged) and reads the `AdminSession` key.
6. `bulkUpdateStudents([rec], key)` ⇒ guarded POST.
7. On `bulkUpdateFinished`:
   - `updated >= 1` ⇒ toast `"Student updated"` + `reloadTable()` + **close**.
   - `updated == 0` ⇒ toast `"No changes to save"` + **close**.
   - `!ok` ⇒ **auth dialog** (401) or the server message; the dialog **stays
     open** so the user can retry / cancel.
8. On `bulkUpdateFailed` ⇒ transient toast; the dialog stays open.

---

## Error Taxonomy

Reuses the 4a.2b-i taxonomy. Depends on the reply classification in decision (A)
— without it a 401 is indistinguishable from a transport failure.

- **Transport failure** — `bulkUpdateFailed` / a reply with **no** HTTP status ⇒
  transient toast.
- **Server rejection** via `bulkUpdateFinished(!ok, message)`:
  - **401 bad/expired admin key** → auth dialog *"Admin authentication failed —
    re-enter via admin login."* Detect via
    `SettingsViewModel::isAuthFailureMessage(message)`.
  - **other** (e.g. a transactional *"some updates failed, rolled back"*) →
    toast with the server message; the dialog **stays open** so the user can
    retry / cancel.
- **Field / precondition** — empty Name ⇒ Save disabled (inline).

---

## Refresh Behavior

After a successful save, `reloadTable()` re-fetches the current Dept → Course
filter (consistent with delete). Because `school_id` is unchanged:

- The edited student **stays present + selected** if it still matches the current
  filter.
- It **drops** if the edit moved it out of the current filter — correct, because
  it no longer matches.

---

## Testing Plan (seams)

### C++ VM (OFFSCREEN, `CapturingNam`)

- `canEdit` is `true` **iff** `selectedCount == 1`.
- `beginEdit` prefills **every** `edit*` field from the located record and
  populates `editCourses`.
- `setEditDepartment` updates the department, **clears** `editCourse`, and
  **reloads** `editCourses`.
- `saveEdit` posts a **1-element `students` JSON array** carrying the EDITED
  field values plus `admin_key` (assert the wire form).
- `onBulkUpdateFinished(ok, updated >= 1)` ⇒ status `"Student updated"` +
  triggers reload + emits the close / `editDone` signal.
- `updated == 0` ⇒ status `"No changes to save"`.
- `!ok` with a 401 message ⇒ auth state (invoke the handler **directly**, like
  the delete tests).

### C++ StudentController

- The `replyIsServerAnswer` rename keeps the delete tests green.
- ADD a bulkUpdate 401 integration test mirroring
  `deleteStudents_guard401WithBody_emitsDeleteFinishedNotFailed`: a `CapturingNam`
  returning 401 + `{"status":"error","message":"Invalid admin key"}` ⇒
  `bulkUpdateFinished(ok=false, message="Invalid admin key")` fires and
  `bulkUpdateFailed` fires **0 times**.

### QuickTest (stub VM)

- Edit button enabled **only** at `M == 1` (disabled at 0 and ≥ 2).
- Double-clicking a table row emits `rowActivated` → the screen calls
  `vm.beginEdit(schoolId)`.
- The dialog renders **prefilled** from the stub's `edit*` state.
- Changing the Department combo calls `vm.setEditDepartment`.
- **Save** invokes `vm.saveEdit` and is **disabled** when `editName` is empty.
- **Cancel** closes the dialog.
- The stub VM exposes: `canEdit`, the `edit*` state, `editCourses`, `beginEdit`,
  `setEditDepartment`, the `setEdit*` setters, `saveEdit`, and `statusMessage`.

---

## Out of Scope

Stated explicitly:

- Per-row pencil column.
- Bulk / multi-row edit (that is 4a.2b-iii).
- Editing `school_id` / `code` / `visits` / `photo` (photo is 4a.2b-iv).
- Duplicate checking; register; department ops.
- No new backend endpoint — `bulk_update_students.php` already exists and is
  guarded.

---

## Open Questions

None. Both decisions are resolved:

- **(A)** Generalize the classifier (`replyIsServerAnswer`) and classify
  `bulkUpdate` in this slice.
- **(B)** Use a second `StudentController` for edit course-loading.
