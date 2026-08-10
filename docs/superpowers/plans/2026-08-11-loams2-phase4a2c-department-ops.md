# LOAMS 2.0 — Phase 4a.2c: Department Deactivate & Delete Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the two department-scoped destructive operations — Deactivate department and Delete department — from the legacy Widgets app into the LOAMS 2.0 Qt Quick "Database" screen, as an inline button pair gated to the filtered department.

**Architecture:** One generic `StudentController::departmentOperation(DeptOp, dept, adminKey)` (single-sources the network plumbing, mirrors `deleteStudents`) → two `DatabaseViewModel` invokables + one finished-slot routing by op → two `LButton`s + two `LConfirmDialog`s on `DatabaseScreen.qml`. Pure client-side: both PHP endpoints already exist, are `requireAdminAuth`-guarded, and were deployed in 4a.1 — **no backend change, no deploy**.

**Tech Stack:** Qt 6.11.1 / C++17, QML (Qt Quick Controls 2), CMake + Ninja + MinGW. Qt Test + Qt Quick Test under CTest. MVVM (ViewModels are the only QML-facing C++). Theme.qml is the single source of visual tokens (zero raw hex).

**Spec:** `docs/superpowers/specs/2026-08-11-loams2-phase4a2c-department-ops-design.md` (claude-review APPROVED, 2 rounds).

## Global Constraints

- **No backend files touched; no deploy.** `deactivate_department.php` / `delete_department.php` are unchanged.
- **MVVM:** QML calls only the VM (`property var vm`); never a controller directly. QuickTests inject a plain-QML stub `vm`.
- **Zero raw hex** outside `Theme.qml`. Buttons use existing `LButton` variants only: `Deactivate` = `Outline`, `Delete` = `Danger`. There is **no** "Warning" variant.
- **Admin key** is RAM-only via `AdminSession::instance().key()`, sent in the POST body, never logged.
- **Server strings render PlainText** (`LConfirmDialog` / `LToast` already pin this).
- **DeptOp is a plain `enum class`** — no `Q_ENUM`, no `Q_DECLARE_METATYPE` (precedent: `SearchOutcome`/`RegisterOutcome`, carried in same-thread direct-connect signals). It is never exposed to QML.
- **Enable gate** (both dept buttons): `vm.department !== "" && vm.course === "" && !vm.deptOpBusy`. The `LCascadingSelect` "All" choice emits `""`.
- **No new files, no CMake changes.** Every edit lands in an existing source or test file.

### Build & test commands (Qt tools are NOT on PATH — prefix every shell)

```bash
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Qt/Tools/CMake_64/bin:$PATH"
# Configure once (no CMakeLists change in this plan, so usually only needed on first build):
cmake -S qt-app -B C:/b/l42c -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
# Build:
cmake --build C:/b/l42c
# Run one C++ test target:
ctest --test-dir C:/b/l42c -R tst_studentcontroller --output-on-failure
ctest --test-dir C:/b/l42c -R tst_databaseviewmodel --output-on-failure
# Run the QML admin suite:
ctest --test-dir C:/b/l42c -R tst_qml_admin --output-on-failure
# Full suite (must stay green before each commit):
ctest --test-dir C:/b/l42c --output-on-failure
```

Use a **short build dir** (`C:/b/l42c`) — the default in-tree dir overflows Windows MAX_PATH for the QML module. **Close any running `WITSQuick.exe`** before a rebuild or the final link fails (exe lock).

---

## File Structure

| File | Responsibility | Change |
|---|---|---|
| `qt-app/core/studentcontroller.h` | Controller API | Add `DeptOp` enum, `departmentOperation`, two signals |
| `qt-app/core/studentcontroller.cpp` | Controller impl | Add `departmentOperation` (mirrors `deleteStudents`, reuses `parseDeleteResponse` + `replyIsServerAnswer`) |
| `qt-app/quick/viewmodels/DatabaseViewModel.h` | DB screen VM API | Add `#include "studentcontroller.h"`, `deptOpBusy`, two invokables, two slots, signal, member |
| `qt-app/quick/viewmodels/DatabaseViewModel.cpp` | DB screen VM impl | Wire signals; two invokables; finished-slot (delete does `setDepartment("")`+`loadDepartments()`); failed-slot |
| `qt-app/quick/qml/admin/DatabaseScreen.qml` | DB screen UI | Two filter-card buttons + two confirm dialogs |
| `qt-app/tests/tst_studentcontroller.cpp` | Controller tests | Add request-assembly + routing cases |
| `qt-app/quick/tests/tst_databaseviewmodel.cpp` | VM tests | Add dept-op state-machine cases |
| `qt-app/quick/tests/tst_qml_admin.qml` | QML tests | Extend `stubVm`; add enable-gate + confirm cases |

Three tasks, one per deliverable layer. Each ends green + committed.

---

## Task 1: StudentController department-operation path

**Files:**
- Modify: `qt-app/core/studentcontroller.h`
- Modify: `qt-app/core/studentcontroller.cpp`
- Test: `qt-app/tests/tst_studentcontroller.cpp`

**Interfaces:**
- Consumes: existing `StudentController(QNetworkAccessManager*, QObject*)`, `ApiConfig::endpoint`, static `replyIsServerAnswer(bool,int,QByteArray)`, static `parseDeleteResponse(QByteArray, QString&)` (returns `bool` success + out-message; `{status,message}` shape identical to the dept endpoints).
- Produces (later tasks rely on these exact names/types):
  - `enum class StudentController::DeptOp { Deactivate, Delete };`
  - `void StudentController::departmentOperation(DeptOp op, const QString &department, const QString &adminKey);`
  - `signal void departmentOpFinished(DeptOp op, bool ok, const QString &message);`
  - `signal void departmentOpFailed(DeptOp op, const QString &errorString);`

- [ ] **Step 1: Write the failing tests**

Add these six declarations to the `private slots:` block in `qt-app/tests/tst_studentcontroller.cpp` (near the existing `deleteStudents_*` declarations, ~line 61-74):

```cpp
    void departmentOp_deactivate_buildsFormBodyToDeactivateEndpoint();
    void departmentOp_delete_targetsDeleteEndpoint();
    void departmentOp_success_emitsFinishedTrueWithOp();
    void departmentOp_serverError_emitsFinishedFalseWithMessage();
    void departmentOp_guard401WithBody_emitsFinishedNotFailed();
    void departmentOp_transportError_emitsFailedWithOp();
```

Add these six definitions at the end of the file (before the closing `QTEST_...`/`#include "tst_...moc"` line). They mirror the existing `deleteStudents_*` tests:

```cpp
void TestStudentController::departmentOp_deactivate_buildsFormBodyToDeactivateEndpoint()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    ctrl.departmentOperation(StudentController::DeptOp::Deactivate, "CE", "test-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    QVERIFY(nam.lastUrl.toString().endsWith(QStringLiteral("deactivate_department.php")));

    const QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QCOMPARE(q.queryItemValue("department"), QStringLiteral("CE"));
    QCOMPARE(q.queryItemValue("admin_key"), QStringLiteral("test-key"));
    QVERIFY(!nam.lastBody.contains("{"));   // not a JSON body
}

void TestStudentController::departmentOp_delete_targetsDeleteEndpoint()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    ctrl.departmentOperation(StudentController::DeptOp::Delete, "CE", "test-key");

    QVERIFY(nam.lastUrl.toString().endsWith(QStringLiteral("delete_department.php")));
    const QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QCOMPARE(q.queryItemValue("department"), QStringLiteral("CE"));
    QCOMPARE(q.queryItemValue("admin_key"), QStringLiteral("test-key"));
}

void TestStudentController::departmentOp_success_emitsFinishedTrueWithOp()
{
    CapturingNam nam;   // default canned body: {"status":"success"}, HTTP 200
    StudentController ctrl(&nam);

    QSignalSpy finishedSpy(&ctrl, &StudentController::departmentOpFinished);
    QSignalSpy failedSpy(&ctrl, &StudentController::departmentOpFailed);

    ctrl.departmentOperation(StudentController::DeptOp::Deactivate, "CE", "k");

    QVERIFY(finishedSpy.wait(1000));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    const QList<QVariant> args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).value<StudentController::DeptOp>(),
             StudentController::DeptOp::Deactivate);   // op echoed unchanged
    QCOMPARE(args.at(1).toBool(), true);
}

void TestStudentController::departmentOp_serverError_emitsFinishedFalseWithMessage()
{
    const QByteArray body = R"({"status":"error","message":"No department provided."})";
    CapturingNam nam(body);   // HTTP 200, error status in body
    StudentController ctrl(&nam);

    QSignalSpy finishedSpy(&ctrl, &StudentController::departmentOpFinished);
    QSignalSpy failedSpy(&ctrl, &StudentController::departmentOpFailed);

    ctrl.departmentOperation(StudentController::DeptOp::Delete, "CE", "k");

    QVERIFY(finishedSpy.wait(1000));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    const QList<QVariant> args = finishedSpy.takeFirst();
    QCOMPARE(args.at(1).toBool(), false);
    QCOMPARE(args.at(2).toString(), QStringLiteral("No department provided."));
}

void TestStudentController::departmentOp_guard401WithBody_emitsFinishedNotFailed()
{
    // A stale/bad admin key => HTTP 401 + JSON error body. replyIsServerAnswer must
    // route the body to departmentOpFinished(false, msg), NOT departmentOpFailed.
    const QByteArray body = R"({"status":"error","message":"Invalid admin key"})";
    CapturingNam nam(body, QNetworkReply::AuthenticationRequiredError, 401);
    StudentController ctrl(&nam);

    QSignalSpy finishedSpy(&ctrl, &StudentController::departmentOpFinished);
    QSignalSpy failedSpy(&ctrl, &StudentController::departmentOpFailed);

    ctrl.departmentOperation(StudentController::DeptOp::Delete, "CE", "stale-key");

    QVERIFY(finishedSpy.wait(1000));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    const QList<QVariant> args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).value<StudentController::DeptOp>(),
             StudentController::DeptOp::Delete);
    QCOMPARE(args.at(1).toBool(), false);
    QCOMPARE(args.at(2).toString(), QStringLiteral("Invalid admin key"));
}

void TestStudentController::departmentOp_transportError_emitsFailedWithOp()
{
    // Empty body + connection error + no HTTP status => genuine transport failure.
    CapturingNam nam(QByteArray(), QNetworkReply::ConnectionRefusedError, 0);
    StudentController ctrl(&nam);

    QSignalSpy finishedSpy(&ctrl, &StudentController::departmentOpFinished);
    QSignalSpy failedSpy(&ctrl, &StudentController::departmentOpFailed);

    ctrl.departmentOperation(StudentController::DeptOp::Deactivate, "CE", "k");

    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(failedSpy.takeFirst().at(0).value<StudentController::DeptOp>(),
             StudentController::DeptOp::Deactivate);
}
```

> Note: `QSignalSpy` stores the enum arg as a `QVariant`; `.value<StudentController::DeptOp>()` extracts it. This works for a plain `enum class` in a **direct** (same-thread) connection — no `Q_DECLARE_METATYPE` needed, exactly as `SearchOutcome` is used elsewhere. If a compile error about the metatype ever appears, the connection has been made queued by mistake — keep it direct.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/l42c -R tst_studentcontroller --output-on-failure`
Expected: **compile failure** — `departmentOperation` / `DeptOp` / `departmentOpFinished` are undefined.

- [ ] **Step 3: Add the API to the header**

In `qt-app/core/studentcontroller.h`, inside the class `public:` section (near the other mutation methods like `deleteStudents`), add the enum and method:

```cpp
    // Department-scoped destructive ops (4a.2c). One generic path single-sources
    // the near-identical network plumbing; the endpoint is chosen by `op`.
    // Plain enum class — NO registration (SearchOutcome/RegisterOutcome precedent);
    // carried in same-thread direct-connect signals, never exposed to QML.
    enum class DeptOp { Deactivate, Delete };

    // POSTs application/x-www-form-urlencoded: department + admin_key to the op's
    // endpoint. adminKey is held by AdminSession (RAM-only, body, never logged).
    // Result via departmentOpFinished / departmentOpFailed. A guard 401-with-body
    // routes to departmentOpFinished(op,false,message) via replyIsServerAnswer.
    void departmentOperation(DeptOp op, const QString &department, const QString &adminKey);
```

In the `signals:` section (near `deleteFinished`/`deleteFailed`), add:

```cpp
    // ok==true => server answered success; ok==false => server answered failure
    // (incl. a guard 401-with-body). op is echoed so one VM slot routes by op.
    void departmentOpFinished(DeptOp op, bool ok, const QString &message);
    // Fires only on a genuine transport error (no server answer).
    void departmentOpFailed(DeptOp op, const QString &errorString);
```

- [ ] **Step 4: Implement the method**

In `qt-app/core/studentcontroller.cpp`, add after `deleteStudents` (~line 337). It mirrors `deleteStudents` and reuses `parseDeleteResponse` (identical `{status,message}` body):

```cpp
void StudentController::departmentOperation(DeptOp op, const QString &department,
                                            const QString &adminKey)
{
    const QString endpoint = (op == DeptOp::Deactivate)
        ? QStringLiteral("deactivate_department.php")
        : QStringLiteral("delete_department.php");
    QNetworkRequest request(ApiConfig::endpoint(endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("department"), department);
    body.addQueryItem(QStringLiteral("admin_key"), adminKey);

    QNetworkReply *reply =
        m_nam->post(request, body.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply, op]() {
        const QByteArray resp = reply->readAll();
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QString errorString = reply->errorString();
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (replyIsServerAnswer(hadError, httpStatus, resp)) {
            QString message;
            const bool ok = parseDeleteResponse(resp, message);   // {status,message}
            emit departmentOpFinished(op, ok, message);           // 401 body reaches here
        } else {
            emit departmentOpFailed(op, errorString);             // transport failure only
        }
    });
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `ctest --test-dir C:/b/l42c -R tst_studentcontroller --output-on-failure`
Expected: **PASS** (all six new cases + the pre-existing ones).

- [ ] **Step 6: Run the full suite**

Run: `ctest --test-dir C:/b/l42c --output-on-failure`
Expected: all green (39 targets + the new functions inside `tst_studentcontroller`).

- [ ] **Step 7: Commit**

```bash
git add qt-app/core/studentcontroller.h qt-app/core/studentcontroller.cpp qt-app/tests/tst_studentcontroller.cpp
git commit -m "feat(core): StudentController department deactivate/delete path (4a.2c t1)"
```

---

## Task 2: DatabaseViewModel department-op state machine

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h`
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp`
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Consumes: Task 1's `StudentController::DeptOp`, `departmentOperation`, `departmentOpFinished`, `departmentOpFailed`; existing `m_controller`, `m_department`, `m_students` (`count()`), `AdminSession::instance().key()`, `refresh()`, `setDepartment(QString)` (resets selection + dependent-clears `m_course` + reloads), `applyServerRejection(msg, fallback)`, `setStatusMessage`, `setAuthFailure`, `m_controller->loadDepartments()`.
- Produces (Task 3 relies on these): `Q_PROPERTY bool deptOpBusy`, `Q_INVOKABLE void deactivateDepartment()`, `Q_INVOKABLE void deleteDepartment()`.

- [ ] **Step 1: Write the failing tests**

In `qt-app/quick/tests/tst_databaseviewmodel.cpp`, add to the `private slots:` block (near the `onDeleteFinished*` declarations):

```cpp
    void deactivateGuardsEmptyDepartmentAndReentry();
    void deleteGuardsEmptyDepartmentAndReentry();
    void deptOpBusyTogglesAroundDeactivate();
    void deactivateSuccessSetsStatusKeepsDepartment();
    void deleteSuccessResetsDepartmentFilterAndClearsCourse();
    void deptOpSuccessClearsAuthFailure();
    void deptOpAuthFailureSetsAuthState();
    void deptOpTransportFailureSetsStatusNoAuth();
```

Add these definitions (mirror the existing `onDeleteFinished*` tests; each constructs a `DatabaseViewModel vm`, drives slots directly, asserts observable state). `cleanup()` already clears the process-wide `AdminSession`:

```cpp
void TestDatabaseViewModel::deactivateGuardsEmptyDepartmentAndReentry()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");

    vm.deactivateDepartment();               // m_department is "" -> no-op
    QCOMPARE(vm.deptOpBusy(), false);

    vm.setDepartment("CE");
    vm.deactivateDepartment();               // now posts; busy latches
    QCOMPARE(vm.deptOpBusy(), true);
    vm.deactivateDepartment();               // re-entry guard: still one op
    QCOMPARE(vm.deptOpBusy(), true);
}

void TestDatabaseViewModel::deleteGuardsEmptyDepartmentAndReentry()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");

    vm.deleteDepartment();                    // "" -> no-op
    QCOMPARE(vm.deptOpBusy(), false);

    vm.setDepartment("CE");
    vm.deleteDepartment();
    QCOMPARE(vm.deptOpBusy(), true);
    vm.deleteDepartment();                    // re-entry guard
    QCOMPARE(vm.deptOpBusy(), true);
}

void TestDatabaseViewModel::deptOpBusyTogglesAroundDeactivate()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("k");
    vm.setDepartment("CE");
    QSignalSpy busySpy(&vm, &DatabaseViewModel::deptOpBusyChanged);

    vm.deactivateDepartment();
    QCOMPARE(vm.deptOpBusy(), true);
    vm.onDepartmentOpFinished(StudentController::DeptOp::Deactivate, true, QString());
    QCOMPARE(vm.deptOpBusy(), false);
    QVERIFY(busySpy.count() >= 2);            // true then false
}

void TestDatabaseViewModel::deactivateSuccessSetsStatusKeepsDepartment()
{
    DatabaseViewModel vm;
    vm.setDepartment("CE");
    vm.onDepartmentOpFinished(StudentController::DeptOp::Deactivate, true, QString());
    QVERIFY(vm.statusMessage().contains("CE"));
    QCOMPARE(vm.department(), QStringLiteral("CE"));   // dept persists; selection kept
}

void TestDatabaseViewModel::deleteSuccessResetsDepartmentFilterAndClearsCourse()
{
    DatabaseViewModel vm;
    vm.setDepartment("CE");
    vm.setCourse("BSCpE");
    vm.onDepartmentOpFinished(StudentController::DeptOp::Delete, true, QString());
    // Ghost-selection regression: a bare refresh() would leave department()=="CE"
    // with the destructive buttons still enabled. The reset drops it to "All".
    QCOMPARE(vm.department(), QString());
    QCOMPARE(vm.course(), QString());
    QVERIFY(vm.statusMessage().contains("CE"));
}

void TestDatabaseViewModel::deptOpSuccessClearsAuthFailure()
{
    DatabaseViewModel vm;
    vm.setDepartment("CE");
    // Simulate a prior auth failure, then a recovered op.
    vm.onDepartmentOpFinished(StudentController::DeptOp::Deactivate, false,
                              QStringLiteral("Admin authentication required"));
    QCOMPARE(vm.authFailure(), true);
    vm.onDepartmentOpFinished(StudentController::DeptOp::Deactivate, true, QString());
    QCOMPARE(vm.authFailure(), false);
}

void TestDatabaseViewModel::deptOpAuthFailureSetsAuthState()
{
    DatabaseViewModel vm;
    vm.setDepartment("CE");
    vm.onDepartmentOpFinished(StudentController::DeptOp::Delete, false,
                              QStringLiteral("Admin authentication required"));
    QCOMPARE(vm.authFailure(), true);
    QCOMPARE(vm.deptOpBusy(), false);
}

void TestDatabaseViewModel::deptOpTransportFailureSetsStatusNoAuth()
{
    DatabaseViewModel vm;
    vm.setDepartment("CE");
    vm.onDepartmentOpFailed(StudentController::DeptOp::Delete,
                            QStringLiteral("Connection refused"));
    QCOMPARE(vm.authFailure(), false);
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(vm.deptOpBusy(), false);
}
```

> `isAuthFailureMessage` matches the guard's "Admin authentication required" text (the shared `SettingsViewModel` predicate). If the exact string differs, use whatever `tst_databaseviewmodel.cpp`'s existing `onDeleteFinishedAuthFailureSetsAuthState` uses — copy that literal so both tests agree.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/l42c -R tst_databaseviewmodel --output-on-failure`
Expected: **compile failure** — `deptOpBusy` / `deactivateDepartment` / `onDepartmentOpFinished` undefined.

- [ ] **Step 3: Add the API to the header**

In `qt-app/quick/viewmodels/DatabaseViewModel.h`:

Add the include near the top (replace the forward decl usage — a nested enum in a slot signature can't be forward-declared):

```cpp
#include "studentcontroller.h"
```

(The line `class StudentController;` at ~line 12 may stay or be removed; the include is what matters.)

Add the property (near the other `Q_PROPERTY` lines):

```cpp
    Q_PROPERTY(bool deptOpBusy READ deptOpBusy NOTIFY deptOpBusyChanged)
```

Add the getter (near `bool bulkBusy()`):

```cpp
    bool deptOpBusy() const { return m_deptOpInFlight; }
```

Add the invokables (near `applyBulkEdit`):

```cpp
    Q_INVOKABLE void deactivateDepartment();
    Q_INVOKABLE void deleteDepartment();
```

Add the slots (in the public slots / test-seam block near `onDeleteFinished`):

```cpp
    void onDepartmentOpFinished(StudentController::DeptOp op, bool ok, const QString &message);
    void onDepartmentOpFailed(StudentController::DeptOp op, const QString &errorString);
```

Add the signal (near `bulkBusyChanged`):

```cpp
    void deptOpBusyChanged();
```

Add the member (near `bool m_bulkInFlight`):

```cpp
    bool m_deptOpInFlight = false;
```

- [ ] **Step 4: Wire signals + implement invokables and slots**

In `qt-app/quick/viewmodels/DatabaseViewModel.cpp` constructor (after the register wiring, ~line 42), add:

```cpp
    connect(m_controller, &StudentController::departmentOpFinished,
            this, &DatabaseViewModel::onDepartmentOpFinished);
    connect(m_controller, &StudentController::departmentOpFailed,
            this, &DatabaseViewModel::onDepartmentOpFailed);
```

Add the implementations (after `onDeleteFailed`, ~line 148):

```cpp
void DatabaseViewModel::deactivateDepartment()
{
    if (m_department.isEmpty() || m_deptOpInFlight) return;
    m_deptOpInFlight = true; emit deptOpBusyChanged();
    m_controller->departmentOperation(StudentController::DeptOp::Deactivate,
                                      m_department, AdminSession::instance().key());
}

void DatabaseViewModel::deleteDepartment()
{
    if (m_department.isEmpty() || m_deptOpInFlight) return;
    m_deptOpInFlight = true; emit deptOpBusyChanged();
    m_controller->departmentOperation(StudentController::DeptOp::Delete,
                                      m_department, AdminSession::instance().key());
}

void DatabaseViewModel::onDepartmentOpFinished(StudentController::DeptOp op, bool ok,
                                               const QString &message)
{
    m_deptOpInFlight = false; emit deptOpBusyChanged();
    if (ok) {
        setAuthFailure(false);
        if (op == StudentController::DeptOp::Deactivate) {
            setStatusMessage(tr("All students in '%1' deactivated.").arg(m_department));
            refresh();                       // dept persists; keep the selection
        } else {
            // Capture the count BEFORE the reset (setDepartment reloads the table
            // asynchronously; count() is truthful now under the Course=All gate).
            const int n = m_students.count();
            const QString dept = m_department;
            setStatusMessage(tr("Department '%1' deleted (%2 students).").arg(dept).arg(n));
            // NOT a bare refresh(): the dept no longer exists, so reset the filter
            // to "All" (clears selection + m_course + reloads all students), and
            // reload departments so the deleted dept drops out of the cascade.
            setDepartment(QString());
            m_controller->loadDepartments();
        }
        return;
    }
    applyServerRejection(message, op == StudentController::DeptOp::Deactivate
                                      ? tr("Deactivate failed.")
                                      : tr("Delete failed."));
}

void DatabaseViewModel::onDepartmentOpFailed(StudentController::DeptOp op,
                                             const QString & /*errorString*/)
{
    m_deptOpInFlight = false; emit deptOpBusyChanged();
    setAuthFailure(false);
    setStatusMessage(op == StudentController::DeptOp::Deactivate
                         ? tr("Deactivate failed — check your connection.")
                         : tr("Delete failed — check your connection."));
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `ctest --test-dir C:/b/l42c -R tst_databaseviewmodel --output-on-failure`
Expected: **PASS** (all eight new cases + pre-existing).

- [ ] **Step 6: Run the full suite**

Run: `ctest --test-dir C:/b/l42c --output-on-failure`
Expected: all green.

- [ ] **Step 7: Commit**

```bash
git add qt-app/quick/viewmodels/DatabaseViewModel.h qt-app/quick/viewmodels/DatabaseViewModel.cpp qt-app/quick/tests/tst_databaseviewmodel.cpp
git commit -m "feat(database): DatabaseViewModel department deactivate/delete ops (4a.2c t2)"
```

---

## Task 3: DatabaseScreen department-action buttons + confirms

**Files:**
- Modify: `qt-app/quick/qml/admin/DatabaseScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: Task 2's `vm.deptOpBusy`, `vm.deactivateDepartment()`, `vm.deleteDepartment()`, plus existing `vm.department`, `vm.course`, `screen.resultCount`; `LButton` (`Outline`/`Danger`), `LConfirmDialog` (`requireTypedConfirmation`/`confirmationWord`/`onConfirmed`, children `confirmButton`/`confirmTypedField`).
- Produces: `objectName`s `deptDeactivateButton`, `deptDeleteButton`, `deptDeactivateConfirm`, `deptDeleteConfirm` (the tests key off these).

- [ ] **Step 1: Extend the stub VM + write the failing QML tests**

In `qt-app/quick/tests/tst_qml_admin.qml`, add to the `stubVm` `QtObject` (after `registerStudent()`, ~line 1751):

```qml
            property bool deptOpBusy: false
            property int deactivateDepartmentCount: 0
            property int deleteDepartmentCount: 0
            function deactivateDepartment() { deactivateDepartmentCount++; }
            function deleteDepartment() { deleteDepartmentCount++; }
```

Add these test functions inside the `DatabaseScreen` `TestCase` (near `test_typedConfirmGateEngagesForLargeSelection`, ~line 1846). Reset `stubVm.department`/`course`/`deptOpBusy` at the end of each so cases don't leak state:

```qml
            function test_deptButtonsDisabledWhenNoDepartment() {
                stubVm.department = "";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, false);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, false);
            }
            function test_deptButtonsDisabledWhenCourseSelected() {
                stubVm.department = "CCS";
                stubVm.course = "BSIT";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, false);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, false);
                stubVm.department = ""; stubVm.course = "";
            }
            function test_deptButtonsEnabledWhenDeptAndCourseAll() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, true);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, true);
                stubVm.department = "";
            }
            function test_deptButtonsDisabledWhenBusy() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = true;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, false);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, false);
                stubVm.department = ""; stubVm.deptOpBusy = false;
            }
            function test_deactivateOpensPlainConfirmAndInvokesVm() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deptDeactivateButton"));
                var dlg = findChild(databaseScreen, "deptDeactivateConfirm");
                verify(dlg !== null);
                compare(dlg.visible, true);
                compare(dlg.requireTypedConfirmation, false);   // reversible, no typed gate
                mouseClick(findChild(dlg, "confirmButton"));
                compare(stubVm.deactivateDepartmentCount, 1);
                stubVm.department = "";
            }
            function test_deleteUsesTypedGateAndInvokesVm() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deptDeleteButton"));
                var dlg = findChild(databaseScreen, "deptDeleteConfirm");
                verify(dlg !== null);
                compare(dlg.visible, true);
                compare(dlg.requireTypedConfirmation, true);     // unconditional typed gate
                var btn = findChild(dlg, "confirmButton");
                compare(btn.enabled, false);                     // gated until DELETE typed
                findChild(dlg, "confirmTypedField").text = "DELETE";
                compare(btn.enabled, true);
                mouseClick(btn);
                compare(stubVm.deleteDepartmentCount, 1);
                stubVm.department = "";
            }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir C:/b/l42c -R tst_qml_admin --output-on-failure`
Expected: **FAIL** — `findChild(... "deptDeactivateButton")` returns null (buttons don't exist yet).

- [ ] **Step 3: Add the buttons to the filter card**

In `qt-app/quick/qml/admin/DatabaseScreen.qml`, in the filter card's `RowLayout { id: filterRow ... }` (after the `LCascadingSelect { id: cascade ... }` block, ~line 55), append the two buttons. The cascade keeps `Layout.fillWidth: true`, so it expands and pushes these implicit-width buttons to the row's right edge — **no extra spacer** (a second `fillWidth` would halve the cascade):

```qml
                LButton {
                    objectName: "deptDeactivateButton"
                    variant: "Outline"
                    compact: true
                    text: qsTr("Deactivate %1").arg(screen.vm ? screen.vm.department : "")
                    enabled: screen.vm
                             ? (screen.vm.department !== "" && screen.vm.course === ""
                                && !screen.vm.deptOpBusy)
                             : false
                    tooltipText: qsTr("Set all students in this department to Inactive. Clear the course filter to act on the whole department.")
                    onClicked: deptDeactivateConfirm.visible = true
                }
                LButton {
                    objectName: "deptDeleteButton"
                    variant: "Danger"
                    compact: true
                    text: qsTr("Delete %1").arg(screen.vm ? screen.vm.department : "")
                    enabled: screen.vm
                             ? (screen.vm.department !== "" && screen.vm.course === ""
                                && !screen.vm.deptOpBusy)
                             : false
                    tooltipText: qsTr("Permanently delete this entire department and all its visit history. Clear the course filter to act on the whole department.")
                    onClicked: deptDeleteConfirm.visible = true
                }
```

- [ ] **Step 4: Add the two confirm dialogs**

In `DatabaseScreen.qml`, after the existing `deleteConfirm` `LConfirmDialog` block (~line 194), add:

```qml
    LConfirmDialog {
        id: deptDeactivateConfirm
        objectName: "deptDeactivateConfirm"
        title: qsTr("Deactivate all students in \"%1\"?").arg(screen.vm ? screen.vm.department : "")
        message: qsTr("This sets all %1 students in department \"%2\" to Inactive — they will not be able to log in until reactivated.\n\nYou can undo this later: filter to %2 → select all → Edit → set Status → Active.")
                    .arg(screen.resultCount).arg(screen.vm ? screen.vm.department : "")
        confirmText: qsTr("Deactivate")
        // Reversible via bulk-edit → no typed gate.
        requireTypedConfirmation: false
        onConfirmed: { deptDeactivateConfirm.visible = false; if (screen.vm) screen.vm.deactivateDepartment(); }
    }

    LConfirmDialog {
        id: deptDeleteConfirm
        objectName: "deptDeleteConfirm"
        title: qsTr("Delete entire department \"%1\"?").arg(screen.vm ? screen.vm.department : "")
        message: qsTr("This will permanently delete:\n• %1 student records in \"%2\"\n• all associated visit history\n\nThis cannot be undone.")
                    .arg(screen.resultCount).arg(screen.vm ? screen.vm.department : "")
        confirmText: qsTr("Delete")
        // The app's most destructive op → unconditional typed gate.
        requireTypedConfirmation: true
        confirmationWord: "DELETE"
        onConfirmed: { deptDeleteConfirm.visible = false; if (screen.vm) screen.vm.deleteDepartment(); }
    }
```

- [ ] **Step 5: Rebuild and run the QML tests to verify they pass**

**Close any running `WITSQuick.exe` first** (exe lock). Then:

Run: `cmake --build C:/b/l42c && ctest --test-dir C:/b/l42c -R tst_qml_admin --output-on-failure`
Expected: **PASS** (six new cases + pre-existing). Remember: production `.qml` is compiled into the module — the rebuild is mandatory before the test reflects the edit.

- [ ] **Step 6: Run the full suite**

Run: `ctest --test-dir C:/b/l42c --output-on-failure`
Expected: all green.

- [ ] **Step 7: Commit**

```bash
git add qt-app/quick/qml/admin/DatabaseScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(database): department deactivate/delete buttons + confirms on Database screen (4a.2c t3)"
```

---

## Post-implementation (outside the per-task loop)

1. **Whole-branch build + full suite** green: `cmake --build C:/b/l42c && ctest --test-dir C:/b/l42c --output-on-failure`.
2. **GUI walkthrough** (this is a GUI app — clean build is necessary but not sufficient). Start XAMPP/Apache, launch `C:/b/l42c/quick/WITSQuick.exe`, admin-login, go to Database:
   - Buttons **disabled** at "All Departments"; **disabled** when a Course is also picked; **enabled** at a specific department + Course "All".
   - **Deactivate CE** → confirm names CE and count → students show Inactive; verify reactivation via Edit → Status → Active on the same set.
   - **Delete CE** → typed-"DELETE" gate → CE vanishes from the cascade, filter returns to All, buttons disable. Confirm the toast renders above the scrim (mirrors the edit/bulk/register z-order check).
   - Wrong/stale admin key → auth-failure toast, **no** mutation (guard 401).
3. `/claude-review` (PHASE mode, ~3 commits) → fix Critical/Important → APPROVE.
4. `create-pr` (project 3-agent gate: dry-checker, security-reviewer, general-code-reviewer).
5. Owner merges via `/merge-pr`. **Push `master` after any spec/plan doc commits** to avoid the recurring merge-divergence snag.

## Non-goals / follow-ups (not built here)

- One-click "Reactivate department" preset over bulk-update (recovery works today via bulk-edit Status→Active).
- Shared reply-decode preamble extraction across delete/bulk/register/dept-ops (standing rule-of-three follow-up).
- Phase 6: HTTPS transport, `+`-in-key encoding, session tokens.

---

## Self-Review

**Spec coverage:** Deactivate + Delete ops (T1 controller, T2 VM, T3 UI) ✅ · one generic `departmentOperation` + `DeptOp` ✅ · 401-routing via `replyIsServerAnswer` ✅ · `deptOpBusy` re-entry guard ✅ · enable gate `department!="" && course=="" && !busy` ✅ · Deactivate plain confirm / Delete unconditional typed gate ✅ · delete-success `setDepartment("")`+`loadDepartments()` reconciliation ✅ · `authFailure` cleared on success ✅ · header `#include "studentcontroller.h"` ✅ · no second `fillWidth` ✅ · Outline/Danger variants ✅ · no backend change ✅.

**Placeholder scan:** No TBD/TODO; every code step shows complete code and exact commands.

**Type consistency:** `DeptOp`, `departmentOperation`, `departmentOpFinished(op,ok,message)`, `departmentOpFailed(op,error)`, `deptOpBusy`, `deactivateDepartment`/`deleteDepartment`, `onDepartmentOpFinished`/`onDepartmentOpFailed`, `m_deptOpInFlight` — used identically across all three tasks and tests. `m_students.count()` matches the `StudentsTableModel::count()` accessor. QML objectNames (`deptDeactivateButton`/`deptDeleteButton`/`deptDeactivateConfirm`/`deptDeleteConfirm`) match between the screen and the tests.
