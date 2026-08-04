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
- `tooltipText` / `accessibleName` become **bindings on `selectedCount`** (the
  current button hard-codes "Select exactly one student to edit" / "Edit the
  selected student" at `DatabaseScreen.qml:107–108`): "Edit the selected student"
  at 1 vs "Bulk-edit the %1 selected students" at ≥2.

The two dialogs are **mutually exclusive** — never open at once. This is what
lets bulk edit safely **reuse `m_editController`** for its dependent course
list (no third `QNetworkAccessManager`).

### BulkEditDialog — tri-state fields

A new `qt-app/quick/qml/admin/BulkEditDialog.qml`, an `LDialog`-based modal
taking `property var vm`. Each editable field is a **tri-state row**: a
"Change this" toggle plus a value control that is **disabled until the toggle
is on**. Only toggled fields are applied.

**New primitive — `LCheckbox`.** `qt-app/quick/qml/components/` has **no**
switch/checkbox primitive today (only `LSegmented`, `LComboBox`, `LTextField`,
…). This slice adds a minimal **`LCheckbox.qml`** (a checked/label control,
Theme-token-only, `checked` property + `toggled(bool)` signal, accessible name)
used for the five field toggles. It is a named build task (not "compose if
none exists"), gets its own `tst_qml_components` case, and is reusable by the
4a.2b-iv register form.

| Field | Toggle | Value control | Notes |
|-------|--------|---------------|-------|
| Department | `changeDepartment` | `LComboBox(vm.departments)` | picking a dept re-scopes the course list; **toggling this on forces Course on** |
| Course | `changeCourse` | `LComboBox(vm.bulkCourses)` | **coupled to Department — on iff Department is on** (see below) |
| Year Level | `changeYearLevel` | `LTextField` | free text (mixed section/number semantics, per existing filter behavior) |
| Gender | `changeGender` | `LComboBox(["Male","Female"])` | |
| Status | `changeStatus` | `LComboBox(["Active","Inactive"])` | |

**Never** Name (unique per student) or School ID (immutable) — neither appears.

**Department ↔ Course are coupled — they move together.** This is the fix for a
data-integrity hole: because `bulk_update_students.php` SETs **all six columns
unconditionally** (`bulk_update_students.php:38–40`), changing *only* Department
would resend each student's **old** course (which belonged to the old
department), leaving N students in a new dept carrying a stale course. Single-edit
already prevents this by clearing the course on any real dept change
(`DatabaseViewModel.cpp:187–189`); bulk edit matches that invariant by coupling
the two:

- **Course cannot change without Department** (the operator can't pick a course
  in isolation across a possibly mixed-department selection), **and**
- **Department cannot change without also setting a Course** (no silent
  mismatch). Both are part of the same change or neither is.

Enforced in **two layers** (belt-and-suspenders, mirroring the single-edit
prefill guard):

- **QML:** the Course toggle is **driven by**, not independent of, Department —
  it is `checked: vm.changeDepartment` and disabled from independent toggling;
  the Course *value combo* is `enabled: vm.changeDepartment && vm.bulkDepartment.length > 0`.
- **VM:** `setChangeDepartment(true)` also sets `changeCourse = true`;
  `setChangeDepartment(false)` forces `changeCourse = false` and clears
  `bulkCourse`. `setChangeCourse(...)` independent of Department is a guarded
  no-op. Setting `bulkDepartment` clears `bulkCourse` and reloads the course
  list (dependent-clear, exactly as single-edit `setEditDepartment` does,
  `DatabaseViewModel.cpp:180–190`).
- **`canApplyBulk`** therefore requires: when Department is toggled, a non-empty
  `bulkDepartment` **and** a non-empty `bulkCourse` (from the new dept's list).

> **Owner sign-off point:** this makes "move a cohort to a new department" always
> require also choosing their course in that department — you cannot bulk-change
> department alone. That is the intended integrity behavior (the owner chose the
> mismatch-preventing option during brainstorming); flagged here for visibility.

**Combo prefill / severance guards (same traps as single-edit).**
`LComboBox.selectValue()` sets `currentValue` **and emits `selected()`**
(`LComboBox.qml:30–35`), and `onActivated` imperatively severs any declarative
`currentValue:` binding (`LComboBox.qml:45–48`). `BulkEditDialog` therefore
replicates `StudentEditDialog`'s machinery (`StudentEditDialog.qml:21–45`):

- a **`prefilling`** flag so the on-open reset's `selectValue()` calls don't
  re-enter the `setBulk*`/`setChange*` setters;
- an **`onVisibleChanged`** handler that, on open, pushes the VM's freshly-reset
  state into every combo (`selectValue("")` / default) so a reopened dialog never
  shows stale prior selections;
- a **`Connections { onBulkCourseChanged: courseCombo.selectValue(vm.bulkCourse) }`**
  re-sync so the VM's dependent-clear of `bulkCourse` (on a dept change) is
  visibly reflected in the course combo — otherwise the clear is invisible.

**Apply** is disabled until `vm.canApplyBulk` — at least one field toggled **and**
every toggled field has a valid (non-empty) value (with the Department⇒Course
rule above).

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
- **Stacking:** both `BulkEditDialog` and `LConfirmDialog` are full-screen
  `LDialog` scrims that stack by declaration order — declare `bulkEditConfirm`
  **after** `BulkEditDialog` in `DatabaseScreen.qml` so the confirm renders on
  top of the still-open bulk dialog.

### Apply mechanics

Record-building is a **pure function** of (selected records × toggle state ×
values) and MUST be a **free `static`** (or public) — **not a private member**.
`DatabaseViewModel` news up its own `m_nam`/`m_controller` in the ctor
(`DatabaseViewModel.cpp:13–14, 23–24`) with **no injection seam**, and existing
unit tests drive the VM purely through its public `onXxx` slots
(`tst_databaseviewmodel.cpp:44–48`); a test cannot observe records posted through
the internal NAM. A directly-callable static is the only way to unit-test the
override/carry-through rules.

```cpp
// A small POD of the operator's choices (toggle + value per field).
struct BulkEditChanges {
    bool changeDepartment=false; QString department;
    bool changeCourse=false;     QString course;
    bool changeYearLevel=false;  QString yearLevel;
    bool changeGender=false;     QString gender;
    bool changeStatus=false;     QString status;
};
// Pure: copies each record, overrides ONLY toggled fields; name/schoolId/code/
// visits carried through untouched.
static QList<StudentRecord> buildBulkUpdates(const QList<StudentRecord> &selected,
                                             const BulkEditChanges &changes);
```

`DatabaseViewModel::applyBulkEdit()` then:

1. Guard: no-op if `m_bulkInFlight`, or `selectedRecords()` empty, or
   `!canApplyBulk()`.
2. `const auto updates = buildBulkUpdates(m_students.selectedRecords(), currentChanges());`
3. `m_bulkInFlight = true;` (re-entry guard — the dialog stays open behind the
   confirm, so a second Apply click could otherwise fire a duplicate batch;
   cleared in the bulk-update result handlers). Apply is also disabled in QML
   while a batch is in flight.
4. `m_controller->bulkUpdateStudents(updates, AdminSession::instance().key());`
   (the **primary** controller — same path single-edit uses.)

### Result handling — generalized for single + bulk

`onBulkUpdateFinished` serves both paths. Its message becomes **count-based**:

Every branch first **clears `m_bulkInFlight = false`** (re-entry guard release):

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

**Known narrow race (acknowledged, not fixed here).** `m_courseTarget` is a
single shared var and `m_editController->loadCourses` replies carry no request-id
(unlike `searchStudents`). If a single-edit dialog is closed and a bulk dialog
opened before a late `coursesLoaded` from the prior session arrives, that reply
could land in `bulkCourses` under the flipped target. This is pre-existing in
spirit (single-edit already races on rapid dept changes), extremely unlikely
under the manual open-dialog → change-dept cadence, and harmless (a wrong course
list is visibly re-scoped on the next dept pick). Left as-is; a request-id guard
on `loadCourses` is a future hardening, not this slice.

---

## VM Surface Changes (`DatabaseViewModel`)

**New Q_PROPERTYs** (WRITE where the dialog mutates, all with NOTIFY):

- `bool changeDepartment` / `changeCourse` / `changeYearLevel` / `changeGender`
  / `changeStatus` — the five toggles.
- `QString bulkDepartment` / `bulkCourse` / `bulkYearLevel` / `bulkGender`
  / `bulkStatus` — the five values.
- `QStringList bulkCourses` (READ) — dependent course list for the bulk dialog.
- `bool canApplyBulk` (READ) — Apply-enable predicate.
- `QStringList bulkChangeSummary` (READ) — preview lines (`canApplyBulk` also
  encodes the Department⇒Course rule: if `changeDepartment`, both
  `bulkDepartment` and `bulkCourse` must be non-empty).

**New Q_INVOKABLEs:**

- `beginBulkEditSelected()` — guard `selectedCount >= 2`; reset all toggles/values
  to a clean state; `m_editMode = BulkEdit`; emit `bulkEditReady()`.
- setters: `setChangeDepartment/Course/YearLevel/Gender/Status(bool)`,
  `setBulkDepartment/Course/YearLevel/Gender/Status(QString)` (with the
  Department⇔Course coupling above).
- `applyBulkEdit()`.
- Reuse existing `requiresTypedConfirmation(int)`.

**New free static** (in `DatabaseViewModel`, testable without a VM instance):
`buildBulkUpdates(const QList<StudentRecord>&, const BulkEditChanges&)`.

**New private members:** `BulkEditChanges`-worth of toggle/value state;
`bool m_bulkInFlight` (re-entry guard); `enum { NoEdit, SingleEdit, BulkEdit }
m_editMode`; `enum { SingleEdit, BulkEdit } m_courseTarget`; the last-emitted
`canEdit` bool (over-emit fix).

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

- **Record building (the static):** call `buildBulkUpdates(selection, changes)`
  **directly** — it produces records that override **only** toggled fields and
  **carry through** each student's own name / schoolId / code / visits. (Must be
  a free static; the VM has no NAM injection seam, so records posted through the
  internal controller are not observable — see Apply mechanics.)
- **`canApplyBulk`:** false with no toggle; false when a toggled field is empty;
  false when `changeDepartment` is on but `bulkCourse` is empty (the coupling);
  true when ≥1 toggle has a valid value.
- **`bulkChangeSummary`:** correct lines, field order, only-toggled, for a
  representative toggle set.
- **Department⇔Course coupling:** `setChangeDepartment(true)` sets `changeCourse`
  true; `setChangeDepartment(false)` forces Course off + clears `bulkCourse`;
  `setChangeCourse(true)` without a department is a no-op; setting
  `bulkDepartment` clears `bulkCourse`. Assert the course-reload landed via the
  `bulkCourses`/course-target seam below (**no** capturing NAM — the VM's NAM is
  not injectable).
- **`m_bulkInFlight` re-entry guard:** a second `applyBulkEdit()` before a result
  arrives is a no-op; the guard clears on the next result.
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

- `LCheckbox`: a `tst_qml_components` case — `checked` reflects state, clicking
  emits `toggled(bool)`, Theme-token styling.
- BulkEditDialog: a field's value control is disabled until its toggle is on;
  the Course toggle follows Department (checking Department checks Course; the
  Course value combo is enabled only with a department chosen; unchecking
  Department unchecks Course).
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

**Design-spec review round 1 (fresh Opus subagent, 2026-08-05) — resolved:**

- **Critical:** Department-without-Course would persist a stale dept/course pair
  (backend SETs all columns) → fixed by **coupling Department⇔Course** (they
  toggle together; `canApplyBulk` requires a course when dept changes). Owner
  sign-off point flagged in-line.
- **Important:** builder must be a **free static** (VM has no NAM injection seam)
  → spec now mandates `buildBulkUpdates(...)` static + direct unit test.
- **Important:** no toggle primitive exists → spec now names an **`LCheckbox`**
  build task with its own test.
- **Important:** BulkEditDialog needs the same **prefill/severance guards** as
  single-edit → spec now specifies the `prefilling` flag, `onVisibleChanged`
  reset, and `onBulkCourseChanged` re-sync.
- **Minor (folded in):** `m_bulkInFlight` re-entry guard; confirm dialog declared
  after the bulk dialog (stacking); acknowledged the narrow `coursesLoaded`
  cross-dialog race; adaptive-button tooltip/accessibleName become bindings.
