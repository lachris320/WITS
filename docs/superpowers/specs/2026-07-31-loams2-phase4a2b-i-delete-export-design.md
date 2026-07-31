# Design Spec: LOAMS 2.0 Phase 4a.2b-i — Multi-Delete + CSV Export

**Date:** 2026-07-31
**Status:** Approved
**Scope:** First mutation slice of the Database screen (Phase 4a track, LOAMS 2.0)

---

## Context & Roadmap

This is the **first of four slices** that decompose Phase 4a.2b (Database mutations).
Phase 4a.2b was split because the full mutation scope was too large for one
increment. The decomposition:

- **4a.2b-i (THIS spec)** — multi-delete + CSV export of the selected / filtered
  student rows.
- **4a.2b-ii** — single-student edit (reuses `bulkUpdateStudents` with a
  one-element list).
- **4a.2b-iii** — bulk edit + duplicate preview (adds `checkDuplicates` to the
  controller).
- **4a.2b-iv** — register student (form + file-picker photo via
  `QHttpMultiPart` + duplicate pre-check + ADD an admin-key guard to the
  currently-unauthenticated `register_student.php`).
- **4a.2c (separate track)** — department deactivate-vs-delete (cut from 4a.2b;
  a different entity, screen, and UX).

**Predecessor.** Phase 4a.2a (merged, PR #31) delivered the **read-only**
Database screen:

- `StudentsTableModel` — a `QAbstractListModel` with refresh-surviving
  multi-select keyed by `schoolId`.
- `LCascadingSelect` — the Dept → Course filter.
- `DatabaseViewModel` — wraps the shared `StudentController`.

This slice adds the **first mutations** on top of that read-only foundation.

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
- **Backend:** PHP over cleartext HTTP. `delete_students.php` is **admin-key
  guarded** (bcrypt `admin_key` FORM field, transactional with a
  `library_visits` cascade) as of the 4a.1 spine. No new endpoint is introduced
  in this slice.

---

## Approach

**Pure statics do the file/format logic; the ViewModel orchestrates given a
path; QML owns the native dialogs.**

Rationale: this mirrors the existing `StudentController::parse*` static pattern
and is the most unit-testable arrangement — the CSV formatter can be exercised
with pure data and no widgets.

**Alternative considered and rejected:** putting the CSV logic inside the VM, or
in a separate `CsvExporter` class. It buys nothing here and adds a type without
adding a seam.

---

## Components & Changes

### 1. `qt-app/core/studentcontroller.{h,cpp}` (shared C++)

Add ONE pure static:

```cpp
static QByteArray toCsv(const QList<StudentRecord> &rows);
```

Behavior:

- Emits a **header row** then one row per record.
- Columns = ALL `StudentRecord` fields EXCEPT photo, in this order:
  `code`, school ID (`schoolId`), `name`, `course`, `department`,
  year level (`yearLevel`), `gender`, `status`, `visits`.
  (`StudentRecord` = `{code, schoolId, name, course, department, yearLevel,
  gender, status, visits}`; there is no photo field client-side.)
- **RFC-4180 quoting:** a field containing a comma, double-quote, CR, or LF is
  wrapped in double-quotes, and any internal double-quote is doubled (`"` →
  `""`).
- **Line terminator:** CRLF (`\r\n`).
- **BOM:** prepend a UTF-8 BOM (`EF BB BF`) so Excel opens the file with correct
  encoding.
- **Empty input** ⇒ header row only.

Delete already exists on the controller — **no new endpoint, no new method**:

```cpp
void deleteStudents(const QStringList &schoolIds, const QString &adminKey);
// signals:
//   deleteFinished(bool ok, int requestedCount, const QString &message)
//   deleteFailed(const QString &errorString)
```

### 2. `qt-app/quick/models/StudentsTableModel.{h,cpp}`

Add two accessors the VM needs for export (the model already exposes
`selectedIds()`, `selectedCount()`, and `clearSelection()`):

```cpp
QList<StudentRecord> selectedRecords() const; // records whose schoolId is in the selection set
QList<StudentRecord> allRecords() const;      // all currently-loaded/filtered records
```

`allRecords()` returns the model's existing `QList<StudentRecord> m_records`
(the currently loaded/filtered set).

### 3. `qt-app/quick/viewmodels/DatabaseViewModel.{h,cpp}`

New surface:

```cpp
static constexpr int kTypeToConfirmThreshold = 10;

Q_INVOKABLE bool requiresTypedConfirmation(int count) const {
    return count >= kTypeToConfirmThreshold;
}

Q_INVOKABLE void deleteSelected();
Q_INVOKABLE bool exportCsv(const QUrl &fileUrl);
```

- **`requiresTypedConfirmation(int count)`** — the VM owns the small-vs-large
  decision; QML consumes only this boolean and never re-implements the
  comparison.
- **`deleteSelected()`** — reads `m_students.selectedIds()` and
  `AdminSession::instance().key()`; calls
  `m_controller->deleteStudents(ids, key)`.
  - On `deleteFinished(ok, ...)`:
    - success ⇒ set a status message (`"Deleted N students"`),
      `reloadTable()`, `m_students.clearSelection()`.
    - failure with a server message / 401 ⇒ set an auth/error state
      (see [Error Taxonomy](#error-taxonomy)).
  - On `deleteFailed` (transport) ⇒ transient error status.
- **`exportCsv(const QUrl &fileUrl)`** — rows = `selectedRecords()` if any are
  selected, else `allRecords()`; bytes = `StudentController::toCsv(rows)`; write
  via **`QSaveFile`** (atomic) to `fileUrl.toLocalFile()`. On success set a
  status (`"Exported N rows to <basename>"`) and return `true`; on write
  failure set an error status and return `false`.

State:

- A delete-in-flight bool.
- `Q_PROPERTY(QString statusMessage ...)` (with `NOTIFY`) that drives the toast.
- A distinct error / auth-failure state property.

**Admin key source (approved decision).** Use the HELD
`AdminSession::instance().key()` — **NO key re-type.** The type-`DELETE` gate is
the friction. (Note: the 4c reset-visits flow re-types the key; delete
deliberately differs because the typed-`DELETE` gate already supplies friction,
and a per-row action shouldn't demand a full key re-entry each time.)

### 4. `qt-app/quick/qml/components/LButton.qml`

Bake in a themed tooltip ONCE:

- `property string tooltipText: ""` rendered via a `ToolTip` whose
  background/text draw from `Theme` tokens (the default QQC2 `ToolTip` ignores
  `Theme`), `ToolTip.delay ~500ms`, shown on hover only when `tooltipText` is
  non-empty.
- `property string accessibleName: ""` — when set, overrides `Accessible.name`
  (so a screen reader reads "Delete 3 selected rows", not "Delete ( 3 )").

Preserve existing behavior when both new props are empty.

**Accessibility note:** tooltip text is NOT exposed to assistive tech, so any
scope information conveyed by the tooltip must ALSO live in `accessibleName` /
`Accessible.description`.

### 5. `qt-app/quick/qml/components/LConfirmDialog.qml`

Extend with an OPTIONAL typed-confirmation gate:

- `property bool requireTypedConfirmation: false`
- `property string confirmationWord: "DELETE"`

When `requireTypedConfirmation` is `true`, show a text field and keep the
confirm button disabled until the field text **equals** `confirmationWord`
(exact match). When `false`, behavior is unchanged — backward-compatible for
existing call sites.

This is reusable and was chosen over a bespoke `DeleteStudentsDialog`.

### 6. `qt-app/quick/qml/admin/DatabaseScreen.qml`

Into the EXISTING count-header row (`"N results · M selected"`, right-aligned),
add two `LButton`s:

- **Export**
  - Label: `Export CSV (all N)` when nothing selected (N = filtered count) /
    `Export CSV (M)` when M selected.
  - ALWAYS enabled.
  - Supplemental `tooltipText: "Exports selected rows, or all filtered rows if
    none are selected."`
  - RESERVED width via a worst-case `TextMetrics` so the button edge never
    jitters as the count changes.
- **Delete**
  - `Danger` variant.
  - Label: `Delete` (disabled) when M=0 / `Delete (M)` when M>0.
  - Disabled unless M>0.
  - Explicit `accessibleName`.
  - Reserved width.

Add:

- A `FileDialog { fileMode: FileDialog.SaveFile; defaultSuffix: "csv";
  nameFilters: ["CSV (*.csv)"] }` (from `QtQuick.Dialogs`, native on Windows in
  Qt 6) whose `onAccepted` calls `vm.exportCsv(selectedFile)`.
- The two-tier delete `LConfirmDialog`: content itemizes the impact
  (`"• M student records"`, `"• all associated visit history"`, `"This cannot be
  undone."`); set `requireTypedConfirmation: vm.requiresTypedConfirmation(M)` so
  M ≥ 10 requires typing `DELETE`. On confirm ⇒ `vm.deleteSelected()`.
- An `LToast` (existing component) driven by `vm.statusMessage` for post-action
  feedback.

---

## Data Flow

### Delete

1. User clicks the **Delete (M)** button (enabled only when M > 0).
2. The `LConfirmDialog` opens, itemizing impact (M student records + all
   associated visit history, irreversible). Its
   `requireTypedConfirmation` is bound to `vm.requiresTypedConfirmation(M)`, so
   for M ≥ 10 the user must type `DELETE` before confirm enables.
3. On confirm ⇒ `vm.deleteSelected()`.
4. The VM reads `m_students.selectedIds()` + `AdminSession::instance().key()`
   and calls `m_controller->deleteStudents(ids, key)`.
5. The controller POSTs to the guarded `delete_students.php`.
6. On `deleteFinished(ok, requestedCount, message)`: success ⇒ status
   `"Deleted N students"`, `reloadTable()`, `clearSelection()`. Failure / 401 ⇒
   auth/error state. On `deleteFailed` (transport) ⇒ transient error status.
7. The `LToast` reflects `vm.statusMessage`.

**Selection cleanup.** The model's `setRecords` already intersects the retained
selection with the ids present in the new data, so deleted rows drop out of the
selection automatically on refresh. `clearSelection()` is also called for
cleanliness.

### Export

1. User clicks **Export** (always enabled). Label reflects whether the export
   will cover the M selected rows or all N filtered rows.
2. The `FileDialog` (SaveFile mode, `.csv` default suffix, CSV name filter)
   opens natively.
3. On accept ⇒ `vm.exportCsv(selectedFile)`.
4. The VM chooses rows = `selectedRecords()` if any are selected, else
   `allRecords()`; produces bytes via `StudentController::toCsv(rows)`; writes
   atomically with `QSaveFile` to `fileUrl.toLocalFile()`.
5. Success ⇒ status `"Exported N rows to <basename>"`, returns `true`. Write
   failure ⇒ error status, returns `false`.
6. The `LToast` reflects `vm.statusMessage`.

---

## Error Taxonomy

Defined here; reused by later slices.

- **Transport failure** (`deleteFailed`, file I/O error) → transient toast.
- **Server rejection / 401 bad-or-expired admin key** → a clear dialog/message:
  `"Admin authentication failed — re-enter via admin login"`.
- **Field / precondition** (the typed-confirm gate) → inline in the dialog.

---

## Testing Plan (seams)

### C++ unit (QtTest)

- `StudentController::toCsv` — quoting edge cases (name with comma, embedded
  double-quote, embedded newline), BOM present, header row correct, all fields
  except photo in order, empty list ⇒ header only, CRLF terminators.
- `DatabaseViewModel::requiresTypedConfirmation` boundary (9 → false,
  10 → true).
- `StudentsTableModel::selectedRecords` / `allRecords`.

### C++ VM (OFFSCREEN, using the existing `qt-app/testsupport/CapturingNam`)

- `deleteSelected` posts the correct `school_ids[]` + `admin_key` FORM fields
  (admin key sourced from `AdminSession`).
- `onDeleteFinished` triggers a reload + clears selection + sets the status.
- `exportCsv(QUrl::fromLocalFile(tempfile))` writes the expected bytes.
- The selected-vs-all-filtered branch picks the right rows.

### QuickTest (stub VM)

- The two buttons show the correct dynamic labels for 0 / some selected.
- Delete disabled at 0.
- The `LConfirmDialog` typed-confirm gate engages when the stub's
  `requiresTypedConfirmation` returns `true`, and disables confirm until
  `DELETE` is typed.
- The export `FileDialog`'s accept path invokes `vm.exportCsv` with the chosen
  URL.
- Stub VM exposes `statusMessage`, `requiresTypedConfirmation`,
  `deleteSelected`, `exportCsv`, and a `students` stub with `count` /
  `selectedCount`.

---

## Out of Scope

Stated explicitly:

- Single / bulk edit, register, photo upload, duplicate check, department ops.
- Undo.
- Export-format options.
- Per-row partial-failure UI.
- No new backend endpoint (the delete endpoint already exists and is guarded;
  CSV is client-side only).

---

## Open Questions

None. Both prior open decisions are resolved:

- **(A)** Admin key: use the held `AdminSession` key, no re-type.
- **(B)** Confirmation dialog: extend `LConfirmDialog` (reusable) rather than
  build a bespoke dialog.
