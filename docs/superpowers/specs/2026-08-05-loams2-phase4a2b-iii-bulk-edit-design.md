# Design Spec: LOAMS 2.0 Phase 4a.2b-iii — Bulk Edit + Change Preview

**Date:** 2026-08-05
**Status:** Approved
**Scope:** Third mutation slice of the Database screen (Phase 4a track, LOAMS 2.0)

---

## Context & Roadmap

This is the **third of four slices** that decompose Phase 4a.2b (Database
mutations). The decomposition:

- **4a.2b-i (MERGED, PR #32)** — multi-delete + CSV export. Established the
  header-button + `LConfirmDialog` + `LToast` + `reloadTable()` + error-taxonomy
  patterns this slice builds on.
- **4a.2b-ii (MERGED, PR #34)** — single-student edit. Added
  `StudentEditDialog`, the VM edit state, the second (`m_editController`)
  network path for the dialog's dependent course list, and routed
  `saveEdit` through `bulkUpdateStudents` with a **one-element** list.
- **4a.2b-iii (THIS spec)** — bulk edit + change preview. Reuses the same
  `bulkUpdateStudents` path with an **N-element** list.
- **4a.2b-iv** — register student (form + file-picker photo via
  `QHttpMultiPart`; **`checkDuplicates` / duplicate pre-check lands here**,
  where a *new* `school_id` genuinely needs an existence check; ADD an admin-key
  guard to the currently-unauthenticated `register_student.php`).
- **4a.2c (separate track)** — department deactivate-vs-delete.

**Scope decision (owner, 2026-08-05).** The roadmap phrase "duplicate preview /
adds `checkDuplicates`" is **explicitly deferred to 4a.2b-iv**. A bulk edit
updates existing students by `school_id` (the immutable WHERE key) and cannot
create new IDs, so `check_duplicates.php` (which detects *already-existing* IDs)
has no role here. What a bulk edit actually wants is a **change preview** — a
confirmation restating exactly which fields change across N students before
committing. That is this slice's second deliverable.

**Predecessors on master.**

- **4a.2a** — the read-only Database screen: `StudentsTableModel` (a
  `QAbstractListModel` with refresh-surviving multi-select keyed by `schoolId`,
  exposing `selectedRecords()` — full records for the selected rows), the
  Dept → Course filter, and `DatabaseViewModel` wrapping the shared
  `StudentController`.
- **4a.2b-i** — delete + CSV export; the header-button, `LConfirmDialog`
  (`requireTypedConfirmation` / `confirmationWord`), `LToast`, `reloadTable()`,
  and error-taxonomy patterns.
- **4a.2b-ii** — single-student edit; `StudentEditDialog`, VM edit state, and
  the `bulkUpdateStudents({rec}, key)` → `onBulkUpdateFinished` result path this
  slice generalizes.

This slice adds **editing many students at once** on top of the existing
multi-select table, and a **change preview** gate for it.

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
  **admin-key guarded**, **transactional (all-or-nothing on real failures)**,
  and already accepts an **array** of student objects. **No new endpoint, no
  new controller method, no backend change** is introduced in this slice.

---

## The Backend Contract (unchanged)

`bulk_update_students.php` (guarded, transactional) runs, per student object:

```sql
UPDATE students SET name=?, course=?, year_level=?, department=?, gender=?, status=?
WHERE school_id=?
```

Precise consequences for bulk edit:

- The **editable columns are exactly those six.** `code`, `visits`, `photo`,
  and `school_id` are never modified by this endpoint (`school_id` is the WHERE
  key).
- The endpoint **sets all six columns unconditionally** for every object.
  Therefore, for any field the operator leaves **unchanged**, the client must
  resend that student's **existing** value — most importantly each student's own
  `name`, which is *not* a bulk-editable field and would be wiped if sent blank.
  This is why the VM builds each update from `selectedRecords()` (the full
  existing record), overriding only the toggled fields.
- **Atomicity:** the endpoint wraps the batch in a transaction. A row that
  `execute()`-fails (`$failed > 0`) rolls back the **entire** batch and returns
  `status:error`, `message:"Some updates failed, all changes rolled back"`. A
  **no-op** row (`affected_rows == 0`, e.g. the value already matched) is *not* a
  failure — it is appended to a benign `errors[]` note and the commit proceeds.
- `updated` counts only rows whose `affected_rows > 0`. So a partial-no-op batch
  returns `status:success`, `updated:K` where `K ≤ N`. The result message must
  reflect **K** (what actually changed), which may be fewer than the N shown in
  the preview — this is honest and intended.

The decoded response is `BulkUpdateResult { ok, updatedCount, errors, message }`
(existing `studentdata.h` type). The existing `StudentController::replyIsServerAnswer`
already routes a guard **401-with-body** to `parseBulkUpdateResponse` so a stale
admin key surfaces as an auth message, not a network error (shared with delete).

---

## Architecture

### Entry point — adaptive Edit button

The existing header **Edit** button (`DatabaseScreen.qml`) becomes adaptive:

- `enabled` when **≥1** row is selected (was: exactly 1).
- `onClicked`: if `selectedCount === 1` → `vm.beginEditSelected()` (existing
  single-edit path, `StudentEditDialog`, name editable); else →
  `vm.beginBulkEditSelected()` (new bulk path, `BulkEditDialog`).
- **Double-click a row** → `vm.beginEdit(schoolId)` (single-edit) — **unchanged.**
- Tooltip/accessible name reflect the two modes ("Edit the selected student" vs
  "Bulk-edit the N selected students").

The two dialogs are **mutually exclusive** — never open at once. This is what
lets bulk edit safely **reuse `m_editController`** for its dependent course
list (no third `QNetworkAccessManager`).

### BulkEditDialog — tri-state fields

A new `qt-app/quick/qml/admin/BulkEditDialog.qml`, an `LDialog`-based modal
taking `property var vm`. Each editable field is a **tri-state row**: a
"Change this" toggle (an `LSwitch`/checkbox-style control; if none exists,
a checkbox composed from existing primitives — no raw hex, Theme tokens only)
plus a value control that is **disabled until the toggle is on**. Only toggled
fields are applied.

| Field | Toggle | Value control | Notes |
|-------|--------|---------------|-------|
| Department | `changeDepartment` | `LComboBox(vm.departments)` | picking a dept re-scopes the course list |
| Course | `changeCourse` | `LComboBox(vm.bulkCourses)` | **enabled only while `changeDepartment` is on and a dept is picked** |
| Year Level | `changeYearLevel` | `LTextField` | free text (mixed section/number semantics, per existing filter behavior) |
| Gender | `changeGender` | `LComboBox(["Male","Female"])` | |
| Status | `changeStatus` | `LComboBox(["Active","Inactive"])` | |

**Never** Name (unique per student) or School ID (immutable) — neither appears.

**Course-requires-Department** is enforced in **two layers** (mirroring the
single-edit prefill guard's belt-and-suspenders):

- **QML:** the Course toggle is `enabled: vm.changeDepartment && vm.bulkDepartment.length > 0`.
- **VM:** `setChangeDepartment(false)` also forces `changeCourse` off and clears
  `bulkCourse`; `setChangeCourse(true)` is a guarded no-op unless
  `changeDepartment && !bulkDepartment.isEmpty()`. Setting `bulkDepartment`
  clears `bulkCourse` and reloads the course list (dependent-clear, exactly as
  the single-edit `setEditDepartment` does).

**Apply** is disabled until `vm.canApplyBulk` — at least one field toggled **and**
every toggled field has a valid (non-empty) value.

The dialog has **Cancel** (closes) and **Apply** (opens the change preview —
below). The dialog **stays open** through the preview and the network round-trip;
it closes only on `bulkEditFinished` (success/no-op). On a server error the
dialog remains open with the error surfaced via the existing `LToast` — matching
single-edit semantics.

### Change preview — the confirmation gate

`DatabaseScreen.qml` gains a second `LConfirmDialog` (`bulkEditConfirm`,
alongside the existing `deleteConfirm`). BulkEditDialog's **Apply** button emits
`applyRequested()`; the screen opens `bulkEditConfirm`, whose PlainText message
is composed from the VM:

```
Apply these changes to N students:
• Department → CIC
• Course → BSCS
• Status → Inactive

Unlisted fields are left unchanged.
```

- `N` = `vm.students.selectedCount` (live; the modal prevents selection change).
- The bullet lines come from **`vm.bulkChangeSummary`** — a **VM-computed
  `QStringList`** (each line `tr("%1 → %2").arg(fieldLabel).arg(value)`, only for
  toggled fields, in a fixed field order). Computing the summary in C++ keeps the
  change logic **unit-testable** and out of QML.
- **Typed confirmation reused:** `requireTypedConfirmation:
  vm.requiresTypedConfirmation(selectedCount)` (the existing ≥10 threshold),
  `confirmationWord: "UPDATE"` (distinct from delete's `"DELETE"`).
- `onConfirmed` → `bulkEditConfirm.visible = false; vm.applyBulkEdit();`
  (BulkEditDialog stays open behind it until `bulkEditFinished`).

### Apply mechanics

`DatabaseViewModel::applyBulkEdit()`:

1. `const QList<StudentRecord> sel = m_students.selectedRecords();` — guard empty.
2. For each record, **copy it**, then override **only toggled fields**:
   `changeDepartment → rec.department`; `changeCourse → rec.course`;
   `changeYearLevel → rec.yearLevel`; `changeGender → rec.gender`;
   `changeStatus → rec.status`. `name`, `schoolId`, `code`, `visits` are carried
   through untouched.
3. `m_controller->bulkUpdateStudents(updates, AdminSession::instance().key());`
   (the **primary** controller — same path single-edit uses.)

Because record-building is a pure function of (selected records × toggle state ×
values), it is extracted to a **testable helper** — e.g. a private
`QList<StudentRecord> buildBulkUpdates() const` (or a free static taking the
records + a small "changes" struct) so a unit test can assert the override /
carry-through rules without the network.

### Result handling — generalized for single + bulk

`onBulkUpdateFinished` serves both paths. Its message becomes **count-based**:

- `ok && updatedCount >= 1` → `tr("Updated %n student(s)", "", updatedCount)`
  (or explicit `updatedCount == 1 ? tr("Updated 1 student") : tr("Updated %1 students")`),
  then `reloadTable()`, `m_students.clearSelection()`, and emit the appropriate
  **finished** signal (see below).
- `ok && updatedCount == 0` → `tr("No changes to save")` + finished.
- `!ok` → `applyServerRejection(result.message, tr("Update failed."))` (the
  folded-in helper), dialog stays open.

This **changes the single-edit success toast** from "Student updated" to
"Updated 1 student"; the corresponding assertion in `tst_databaseviewmodel`
updates in this slice. `onBulkUpdateFailed` → `applyServerRejection`-style
network fallback (`tr("Update failed — check your connection.")`).

**Finished signals.** The single and bulk dialogs get **separate** ready/finished
pairs so each closes on its own event and there is no cross-dialog coupling:

- single: existing `editReady()` / `editFinished()`.
- bulk: new `bulkEditReady()` / `bulkEditFinished()`.

`onBulkUpdateFinished` must emit the right one. The VM tracks which mode is
in-flight with a small `enum { NoEdit, SingleEdit, BulkEdit } m_editMode` set at
`beginEdit`/`beginBulkEditSelected` and consulted (then reset) when the result
arrives. (Alternative considered: reuse `editFinished` for both — rejected to
avoid the single dialog reacting to a bulk result.)

### Course-list routing (reusing `m_editController`)

Both dialogs load "courses for department D" via `m_editController`, whose
`coursesLoaded` currently lands in `onEditCoursesLoaded` → `m_editCourses`.
Because the dialogs are mutually exclusive, we route by target: a private
`enum { SingleEdit, BulkEdit } m_courseTarget` is set immediately before each
`m_editController->loadCourses(...)` call, and `onEditCoursesLoaded` dispatches
the result to `m_editCourses` (single) or `m_bulkCourses` (bulk) accordingly.
No third `QNetworkAccessManager`.

---

## VM Surface Changes (`DatabaseViewModel`)

**New Q_PROPERTYs** (WRITE where the dialog mutates, all with NOTIFY):

- `bool changeDepartment` / `changeCourse` / `changeYearLevel` / `changeGender`
  / `changeStatus` — the five toggles.
- `QString bulkDepartment` / `bulkCourse` / `bulkYearLevel` / `bulkGender`
  / `bulkStatus` — the five values.
- `QStringList bulkCourses` (READ) — dependent course list for the bulk dialog.
- `bool canApplyBulk` (READ) — Apply-enable predicate.
- `QStringList bulkChangeSummary` (READ) — preview lines.

**New Q_INVOKABLEs:**

- `beginBulkEditSelected()` — guard `selectedCount >= 2`; reset all toggles/values
  to a clean state; `m_editMode = BulkEdit`; emit `bulkEditReady()`.
- setters: `setChangeDepartment/Course/YearLevel/Gender/Status(bool)`,
  `setBulkDepartment/Course/YearLevel/Gender/Status(QString)` (with the
  dept→course dependency logic above).
- `applyBulkEdit()`.
- Reuse existing `requiresTypedConfirmation(int)`.

**New signals:** `bulkEditReady()`, `bulkEditFinished()`, plus `*Changed()` for
each new property.

**Folded-in follow-up 1 — `applyServerRejection` dedup.** Extract the duplicated
401-vs-generic tail (currently `DatabaseViewModel.cpp:132–138` in
`onDeleteFinished` and `:245–251` in `onBulkUpdateFinished`) into a private
`void applyServerRejection(const QString &message, const QString &genericFallback)`
that does the `SettingsViewModel::isAuthFailureMessage` split + sets
`authFailure` + status message. Reroute both call sites through it.

**Folded-in follow-up 2 — `canEdit` enable logic.** The Edit-button rule moves to
`selectedCount >= 1`. `canEdit` is redefined as `selectedCount >= 1` (button
enable), and the double-click / single-`beginEditSelected` guards continue to
require exactly 1 internally. Fix `canEditChanged` to emit **only when the
derived boolean actually flips** (track the last emitted value) instead of on
every `selectionChanged` — killing the over-emit.

**Deferred (not in this slice):** `beginEdit` O(n) scan → index; single-dialog
`Save.enabled` undefined guard.

---

## Data Flow (bulk edit, happy path)

```
[≥2 selected] → Edit button (enabled, selectedCount>=1)
      → onClicked branch: selectedCount>1 → vm.beginBulkEditSelected()
      → VM: reset toggles, m_editMode=BulkEdit, emit bulkEditReady()
      → BulkEditDialog opens
   operator toggles fields + picks values (setChange*/setBulk* → canApplyBulk, bulkChangeSummary update)
      → Apply (enabled by canApplyBulk) → dialog emits applyRequested()
      → screen opens bulkEditConfirm (message from vm.bulkChangeSummary + count; typed "UPDATE" if ≥10)
      → onConfirmed → vm.applyBulkEdit()
      → VM builds N records (selectedRecords override toggled) → controller.bulkUpdateStudents(list, key)
      → POST bulk_update_students.php (guarded, transactional)
      → onBulkUpdateFinished(result)
          ok & updated≥1 → toast "Updated K students" → reloadTable() → clearSelection() → emit bulkEditFinished()
          ok & updated=0 → toast "No changes to save" → emit bulkEditFinished()
          !ok           → applyServerRejection(...) → toast → dialog STAYS open
      → BulkEditDialog closes on bulkEditFinished
```

---

## Error Taxonomy (unchanged, now shared)

| Condition | Signal path | authFailure | Toast |
|-----------|-------------|-------------|-------|
| Success (K≥1) | `bulkUpdateFinished`, ok | false | "Updated K students" |
| No-op (K=0) | `bulkUpdateFinished`, ok | false | "No changes to save" |
| Stale admin key (401-with-body) | `bulkUpdateFinished`, !ok, `isAuthFailureMessage` | **true** | "Admin authentication failed — re-enter via admin login." |
| Server rejection (batch rolled back) | `bulkUpdateFinished`, !ok | false | server `message` (or "Update failed.") |
| Transport failure | `bulkUpdateFailed` | false | "Update failed — check your connection." |

The 401-with-body vs transport-failure split is already handled by
`StudentController::replyIsServerAnswer`; the auth-vs-generic split within a
server rejection is `applyServerRejection` (folded-in helper).

---

## Testing Strategy

**Unit — `tst_databaseviewmodel` (network-free, driven via the public slots):**

- **Record building:** given a selection and a toggle/value set, `applyBulkEdit`
  produces records that override **only** toggled fields and **carry through**
  each student's own name / schoolId / code / visits.
- **`canApplyBulk`:** false with no toggle; false when a toggled field is empty;
  true when ≥1 toggle has a valid value.
- **`bulkChangeSummary`:** correct lines, field order, only-toggled, for a
  representative toggle set.
- **Course-requires-Department:** toggling Department off forces Course off +
  clears `bulkCourse`; `setChangeCourse(true)` is a no-op without a department;
  setting `bulkDepartment` clears `bulkCourse` and requests the course reload
  (assert via a capturing NAM or the `bulkCourses`/course-target routing).
- **Course-target routing:** an `onEditCoursesLoaded` while `m_courseTarget ==
  BulkEdit` populates `bulkCourses`, not `editCourses`, and vice-versa.
- **Count-based result messages:** `onBulkUpdateFinished` with `updatedCount`
  1 / N / 0 → the three messages; `!ok` auth vs generic → `applyServerRejection`.
- **`applyServerRejection` shared behavior:** an auth-failure message via the
  *delete* path and via the *bulk* path both set `authFailure` + the auth toast
  (proves the dedup preserved both call sites).
- **`canEditChanged` over-emit fix:** stepping selection 0→1→2→1 emits
  `canEditChanged` only when the derived bool flips.

**QuickTest — `tst_qml_admin` (OFFSCREEN, plain-QML stub VM):**

- BulkEditDialog: a field's value control is disabled until its toggle is on;
  the Course toggle is disabled until Department's toggle is on with a dept
  chosen.
- Apply is disabled until `canApplyBulk`; Apply emits `applyRequested`.
- The change-preview `LConfirmDialog` shows the summary lines + count; typed
  confirmation appears for a stubbed `requiresTypedConfirmation === true`.
- Adaptive Edit button: with the stub reporting `selectedCount === 1` the click
  calls `beginEditSelected`; with `selectedCount === 2` it calls
  `beginBulkEditSelected` (assert via stub spy flags).
- **Fixture rule:** any new `tst_qml_*` fixture gets its **own non-overlapping
  y-band** and the root `height:` is raised to contain it, or synthetic mouse
  events land outside the window and are silently dropped
  ([[quicktest-taphandler-doubletap-unusable]]).

**Build/verify:** rebuild before ctest (stale-binary trap); `ctest
--test-dir qt-app/build --output-on-failure` all green; then **GUI-verify**
against `WITSQuick.exe` + XAMPP — select several students, bulk-change a field,
confirm the preview lists exactly those changes, apply, and confirm the table
reflects the change and the toast reports the right count.

---

## Out of Scope

- `checkDuplicates` / duplicate pre-check and guarding `check_duplicates.php`
  → **4a.2b-iv** (register), where a new `school_id` needs an existence check.
- Register student, photo upload (`QHttpMultiPart`) → 4a.2b-iv.
- Department deactivate-vs-delete → 4a.2c.
- Excel/ZIP import → 4a.3.
- The deferred follow-ups (`beginEdit` O(n) scan; single-dialog Save-enable
  guard).
- Bulk-editing Name or School ID (not server-editable / immutable identity).
- Cleartext-HTTP transport and `+`-in-admin-key encoding (Phase 6, house-wide).

---

## Open Questions

None. All forks resolved during brainstorming (2026-08-05): change-preview over
duplicate-preview; adaptive single Edit button; Course-requires-Department;
summary + typed-confirm for ≥10; fold in `applyServerRejection` + `canEdit`
logic, defer the other two follow-ups.
