# LOAMS 2.0 — Phase 4a.2c: Department Deactivate & Delete (Design Spec)

> Date: 2026-08-11 · Status: **APPROVED (brainstorm)** · Track: 4a (Database + Import)
> Predecessor slice: 4a.2b-iv (register student + photo, PR #37, `5d86f0e`).
> No secrets, credentials, or real student PII in this document (repo hygiene rule).

---

## 1. Summary

Bring the two **department-scoped** destructive operations — **Deactivate department**
and **Delete department** — into the LOAMS 2.0 Qt Quick admin **Database** screen
(`WITSQuick.exe`). Today these exist **only** in the legacy Qt Widgets app
(`adminwindow.cpp`); the Quick Database screen already does student-level
delete/edit/bulk/register but has **no department-level actions**.

This is a **pure client-side phase**. Both backend endpoints already exist, are
`requireAdminAuth`-guarded, and were deployed + negative-auth-verified in Phase 4a.1
(Task 10). There is **no backend change and no deploy** in this slice — a deliberate
contrast with 4a.2b-iv, which shipped a breaking backend guard.

---

## 2. Scope (owner-confirmed)

**In scope — two department operations:**

| Op | Endpoint (existing, guarded) | Backend effect |
|----|------------------------------|----------------|
| **Deactivate department** | `deactivate_department.php` | `UPDATE students SET status='Inactive' WHERE department=?` — students in the dept can no longer log in. |
| **Delete department** | `delete_department.php` | Transactional cascade: delete all `library_visits` for the dept's students, then `DELETE FROM students WHERE department=?`. Permanent; the app's single most destructive action. |

**Explicitly out of scope (documented, not built):**

- **Reset-visits per department.** Already shipped in the Quick **Settings** screen
  (`SettingsViewModel::resetVisits`, the tier-2 "Reset Visits" card with a Reset
  Manifest). Re-surfacing it here would create two homes for one destructive op.
- **A dedicated reactivate endpoint.** The backend `deactivate` is one-way, but
  **reactivation is already achievable with the shipped bulk-edit**: filter to the
  department → select all → **Edit** → toggle **Status → Active** → apply (`status`
  is a bulk-editable field). So a mis-click on Deactivate has a clean in-app recovery
  path today; no new endpoint is warranted. (A one-click "Reactivate department"
  preset over bulk-update is a possible future nicety — deferred.)
- **Any legacy `adminwindow.cpp` change.** The legacy dept-op buttons already POST
  `admin_key` and stay as-is.

---

## 3. UI design

### 3.1 Placement — inline pair in the filter card

Department ops act on the **filtered department**, not on the table's checked rows.
Their home is therefore the **filter card** (which holds the `LCascadingSelect`),
spatially and semantically apart from the row-action band (Add / Export / Edit /
Delete), which acts on table selection/results. This kills the "delete these selected
students" vs "delete this whole department" confusion.

```
┌─ Filter card ─────────────────────────────────────────────────────────┐
│  [ Department: CE ▾ ]  [ Course: All ▾ ]      [ Deactivate CE ] [ Delete CE ] │
└───────────────────────────────────────────────────────────────────────┘
   12 results · 3 selected              ＋Add   Export   Edit   Delete(3)
   ┌─────────────────────────── students table ───────────────────────────┐
```

- **`Deactivate <Dept>`** — `LButton variant: "Outline"` — neutral / lower-emphasis,
  visually distinct from the Danger Delete without inventing a new variant. (`LButton`
  supports `Primary | Accent | Outline | Danger | Ghost`; there is no "Warning"
  variant, and adding one is out of scope. All color from `Theme` tokens.)
- **`Delete <Dept>`** — `LButton variant: "Danger"` (`Theme.error`).
- Both buttons show the **live department name** in their label
  (`qsTr("Deactivate %1").arg(vm.department)`), so the target is never ambiguous.

### 3.2 Enable rules (scope-correctness gate)

Department ops hit the **entire department** on the backend (`WHERE department=?`),
regardless of any Course sub-filter. If a course were also picked, the on-screen row
count would be smaller than what actually gets affected — a dangerous confirm-dialog
mismatch. To keep every displayed count truthful **without an extra backend call**,
both buttons are:

```
enabled: vm.department !== ""      // a specific department is picked
     AND vm.course     === ""      // Course = "All" (whole department is on screen)
     AND !vm.deptOpBusy            // no op in flight
```

> Sentinel note: the `LCascadingSelect` "All" choice emits the **empty string** —
> `department`/`course` are `""` when unfiltered, non-empty for a specific value
> (verified in `LCascadingSelect.qml` and `DatabaseViewModel::setCourse`). The gate
> tests against `""`, not the literal `"All"`.

When a specific department **and** a course are selected, the buttons are **disabled**
with a tooltip: *"Clear the course filter to act on the whole department."* This makes
the on-screen `resultCount` exactly equal the number of students the op will affect.

### 3.3 Confirmation UX

Two `LConfirmDialog`s (reusing the existing primitive; `requireTypedConfirmation` +
`confirmationWord` + `onConfirmed`, `PlainText` message):

**Deactivate confirm** — reversible, so a strong **named** confirm, **no typed gate**:

> **Title:** Deactivate all students in "CE"?
> **Message:** This sets **all N students** in department "CE" to **Inactive** — they
> will not be able to log in until reactivated.
> You can undo this later: filter to CE → select all → **Edit** → set **Status →
> Active**.
> **Confirm button:** Deactivate

**Delete confirm** — maximally destructive → **typed gate, unconditional**:

- `requireTypedConfirmation: true` (always, not only at ≥10 — this deletes an entire
  department of students plus all their visit history).
- `confirmationWord: "DELETE"`.

> **Title:** Delete entire department "CE"?
> **Message:** This will **permanently delete**:
> • **N** student records in "CE"
> • all associated visit history
> This cannot be undone. Type **DELETE** to confirm.
> **Confirm button:** Delete

`N` is `screen.resultCount` — truthful because of the Course=All enable gate.

---

## 4. Architecture

### 4.1 `StudentController` — one generic department-op path

Mirrors the shipped `deleteStudents` mutation shape, but uses a **single generic
method + op enum** so the near-identical network plumbing (build form body, POST,
decode reply, 401-route) is written **once** (DRY; the two ops differ only by
endpoint):

```cpp
enum class DeptOp { Deactivate, Delete };   // Q_ENUM (SearchOutcome/RegisterOutcome precedent)

// POSTs application/x-www-form-urlencoded: department=<dept>&admin_key=<key>
// to the endpoint chosen by `op`. adminKey is held by AdminSession (RAM-only,
// POST-body, never logged). Result via departmentOpFinished / departmentOpFailed.
void departmentOperation(DeptOp op, const QString &department, const QString &adminKey);

signals:
  // ok==true  → server answered success; ok==false → server answered failure
  //             (incl. a guard 401-with-body, routed here via replyIsServerAnswer).
  void departmentOpFinished(DeptOp op, bool ok, const QString &message);
  // Fires only on a genuine transport error (no server answer).
  void departmentOpFailed(DeptOp op, const QString &errorString);
```

- Endpoint map: `DeptOp::Deactivate → "deactivate_department.php"`,
  `DeptOp::Delete → "delete_department.php"`.
- Reply handling reuses the shared `replyIsServerAnswer(replyHadError, httpStatus,
  body)` classifier: a 401-with-body (stale/invalid key) routes to
  `departmentOpFinished(op, false, message)` — **not** to `departmentOpFailed` — so a
  guard rejection reads as a server answer, consistent with delete/bulk/register.
- `op` is echoed back through both signals so a single VM slot can route by op.

### 4.2 `DatabaseViewModel` — two invokables, one finished-slot

```cpp
Q_PROPERTY(bool deptOpBusy READ deptOpBusy NOTIFY deptOpBusyChanged)

Q_INVOKABLE void deactivateDepartment();   // acts on m_department via m_controller
Q_INVOKABLE void deleteDepartment();       // acts on m_department via m_controller

// slots (test seam — driven network-free)
void onDepartmentOpFinished(StudentController::DeptOp op, bool ok, const QString &message);
void onDepartmentOpFailed(StudentController::DeptOp op, const QString &errorString);
```

- Both invokables use the **primary** `m_controller` (one-shot mutation, like
  `deleteStudents`) and the held `AdminSession::instance().key()`. Guard: no-op if
  `m_department` is empty or `m_deptOpBusy` is already true.
- `m_deptOpBusy` set **before** the controller call; cleared in both slots — the
  re-entry guard + button-disable source.
- **Success (`ok == true`):**
  - `Deactivate` → `setStatusMessage("All students in '<dept>' deactivated.")` then
    `refresh()` (reload departments + table; statuses now show Inactive; dept remains).
  - `Delete` → `setStatusMessage("Department '<dept>' deleted (<N> students).")` then
    `refresh()` (departments reload — the dept is gone — cascade resets to "All", table
    reloads).
- **Server-answered failure (`ok == false`):** route through the existing
  `applyServerRejection(message, genericFallback)` — sets `authFailure` on an auth
  message, otherwise a generic toast. (Same auth-vs-generic split as delete/bulk.)
- **Transport failure (`onDepartmentOpFailed`):** generic error toast; clear busy.
- Toasts surface via the existing `statusMessage` → `databaseToast` imperative
  `Connections` raise (never a destroyed `message:` binding — the documented LToast
  trap).

### 4.3 `DatabaseScreen.qml`

- Two `LButton`s appended to `filterRow` (right of the cascade, after a
  `Item { Layout.fillWidth: true }` spacer so they right-align).
- Two `LConfirmDialog`s (`deactivateDeptConfirm`, `deleteDeptConfirm`) with distinct
  `objectName`s for QuickTest.
- `onConfirmed` handlers hide the dialog then call the matching VM invokable.
- All colors from `Theme` tokens; no raw hex.

---

## 5. Testing (TDD)

**`StudentController` (request-assembly + decode, via `testsupport/capturingnam`):**
- `departmentOperation(Deactivate, ...)` POSTs to `deactivate_department.php` with
  form fields `department` + `admin_key`; `Delete` → `delete_department.php`.
- A success JSON body → `departmentOpFinished(op, true, message)`.
- A server error JSON body → `departmentOpFinished(op, false, message)` (0×
  `departmentOpFailed`).
- A **401-with-body** (guard rejection) → `departmentOpFinished(op, false, message)`,
  **not** `departmentOpFailed` (asserts `replyIsServerAnswer` routing).
- A transport error (no body) → `departmentOpFailed(op, errorString)`.
- `op` is echoed unchanged on both signals.

**`DatabaseViewModel` (state machine, driven network-free through the slots):**
- `deactivateDepartment()` / `deleteDepartment()` are no-ops when `m_department` is
  empty or `deptOpBusy` is already true (re-entry guard).
- Busy: `deptOpBusy` true after invoke, false after the finished/failed slot.
- Success routes: Deactivate → status message + `refresh()` invoked; Delete → status
  message (with count) + `refresh()` invoked.
- `ok == false` with an auth message → `authFailure` set (via `applyServerRejection`).
- `onDepartmentOpFailed` → generic error toast, busy cleared.

**QML (`tst_qml_admin.qml` fixture, OFFSCREEN):**
- Enable matrix: buttons **disabled** at Department=All; **disabled** at
  Department=CE + Course=CS; **enabled** at Department=CE + Course=All; **disabled**
  while `deptOpBusy`.
- Clicking `Deactivate CE` opens `deactivateDeptConfirm` (no typed field);
  `onConfirmed` calls `vm.deactivateDepartment()`.
- Clicking `Delete CE` opens `deleteDeptConfirm` with the typed gate; the confirm
  button is disabled until "DELETE" is typed; `onConfirmed` calls
  `vm.deleteDepartment()`.
- Uses a plain-QML stub `vm` (the `property var vm` injection pattern), not the C++ VM.

All tests register via `wits_add_qttest()` (`OFFSCREEN` for the Quick/GUI test).
Full suite must stay green (`ctest --test-dir <build> --output-on-failure`).

---

## 6. Error handling & edge cases

- **Stale/invalid admin key** → guard returns HTTP 401 with a JSON body →
  `departmentOpFinished(op, false, <auth message>)` → `applyServerRejection` sets
  `authFailure` + toast. No mutation occurred (guard rejects before any write).
- **Empty department name** → VM guards before calling the controller (button is
  already disabled at Department=All; the VM double-checks).
- **Delete of a department that no longer exists** (deleted in another window) → the
  endpoint's `DELETE ... WHERE department=?` affects 0 rows and still answers success;
  `refresh()` reconciles the cascade. Acceptable.
- **Re-entry** (double-click) → `m_deptOpBusy` short-circuits the second call and the
  buttons are disabled while busy.
- **`+` in the admin key** corrupts over `x-www-form-urlencoded` — a pre-existing,
  in-code-documented house behavior shared by all guarded endpoints; a Phase 6
  follow-up, not addressed here.
- **Cleartext HTTP transport** — pre-existing; Phase 6.

---

## 7. Security notes

- Admin key stays **RAM-only** (`AdminSession`), sent in the **POST body**, **never
  logged**, consistent with delete/bulk/register.
- Guard-before-mutation is already enforced server-side (4a.1) for both endpoints; the
  client cannot bypass it.
- All server-supplied strings render in `PlainText` (`LConfirmDialog` / `LToast`
  already pin this) — no `<img>`/markup injection over cleartext HTTP.

---

## 8. Deliverables

- `qt-app/core/studentcontroller.{h,cpp}` — `DeptOp` enum, `departmentOperation`,
  `departmentOpFinished` / `departmentOpFailed`.
- `qt-app/quick/viewmodels/DatabaseViewModel.{h,cpp}` — `deptOpBusy`,
  `deactivateDepartment` / `deleteDepartment`, the two slots, wiring.
- `qt-app/quick/qml/admin/DatabaseScreen.qml` — two filter-card buttons + two
  confirm dialogs.
- Tests: `StudentController` request-assembly cases, `DatabaseViewModel` state-machine
  cases, `tst_qml_admin.qml` fixture additions.
- **No backend files, no deploy.**

---

## 9. Non-goals / follow-ups (carried, not built here)

- One-click "Reactivate department" preset over bulk-update (recovery works today via
  bulk-edit Status→Active).
- Shared reply-decode preamble extraction across delete/bulk/register/**dept-ops**
  (rule-of-three+ duplication) — a standing follow-up noted since 4a.2b-iv; can be
  taken as its own refactor PR.
- Phase 6: HTTPS transport, `+`-in-key encoding, session tokens.
