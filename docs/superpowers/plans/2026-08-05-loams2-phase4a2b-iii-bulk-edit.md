# LOAMS 2.0 Phase 4a.2b-iii — Bulk Edit + Change Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add multi-row bulk editing to the Database screen — apply shared field changes (department/course/year/gender/status) to N selected students at once, gated by a change-preview confirmation — reusing the existing `bulkUpdateStudents` array path.

**Architecture:** Extends the existing `DatabaseViewModel` (MVVM, the only QML-facing layer) with bulk-edit state, a pure `buildBulkUpdates` static, and a change-preview summary. A new `BulkEditDialog.qml` (tri-state fields, built on a new `LCheckbox.qml` primitive) composes the change; a second `LConfirmDialog` restates it. The adaptive **Edit** button opens single-edit at 1 selection and bulk-edit at ≥2. No backend change, no new endpoint, no new `QNetworkAccessManager`.

**Tech Stack:** Qt 6.11.1 / C++17 / QML (Qt Quick Controls 2), CMake + Ninja, QtTest + Qt Quick Test under CTest.

## Global Constraints

- **Strict MVVM:** C++ ViewModels are the ONLY QML-facing layer. QML screens take `property var vm` (a `DatabaseViewModel`, or a plain-QML stub in QuickTests). QML **never** calls a `witscore` controller directly.
- **Theming:** `Theme.qml` (pragma Singleton, `import LOAMS`) is the single source of every visual token. **ZERO raw hex** outside `Theme.qml`; opacity variants use `Qt.alpha(Theme.<token>, a)`, never a literal color.
- **Naming:** QML types + C++ ViewModel/model classes are `PascalCase`; C++ members are `m_camelCase`.
- **Backend contract (unchanged):** `bulk_update_students.php` is admin-key-guarded, transactional (all-or-nothing on real failures), and `SET`s all six columns (`name, course, year_level, department, gender, status`) unconditionally `WHERE school_id`. Unchanged fields MUST be resent with each student's existing value — this is why records are built from `selectedRecords()`. A no-op row (`affected_rows==0`) is a success, not a failure; `updated` counts only truly-changed rows (K ≤ N).
- **School ID / Name are NOT bulk-editable** (immutable identity / unique per student). Department ⇔ Course are **coupled** (move together — see Task 4).
- **Admin key:** RAM-only via `AdminSession::instance().key()`, sent in the POST body, never logged. Any server-supplied string rendered in QML MUST be `Text.PlainText` (cleartext-HTTP injection guard) — `LDialog`/`LToast`/`LComboBox` already pin plain.
- **Build dir:** a SHORT path — use **`C:/b/l42biii`** — to dodge the Windows MAX_PATH limit on the QML-module autogen dir. Configured once in the worktree step (see below).
- **Toolchain (tools are NOT on PATH):** every `cmake`/`ctest` command must be preceded, in the SAME shell invocation, by:
  ```powershell
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  ```
  One-time configure (in the worktree): `cmake -S qt-app -B C:/b/l42biii -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`.
- **Rebuild before ctest** every time (stale-binary trap): `cmake --build C:/b/l42biii` then `ctest`. Harmless warnings to ignore: `LF will be replaced by CRLF`, the vendored `QXlsx ... GuiPrivate` CMake warning.
- **QuickTest fixture rule:** any new `tst_*.qml` fixture gets its **own non-overlapping vertical y-band** and the root `height:` is raised to contain it — QTest mouse synthesis is window-local, so a fixture outside the window silently drops clicks.
- **Commit** after every green task via the `commit` skill (Conventional Commits; no Claude attribution trailer). Do not push per-task; the branch is pushed at PR time.

**Interfaces defined once, consumed across tasks** (exact signatures — an implementer sees only their own task):

```cpp
// DatabaseViewModel.h — declared above the class (Task 3):
struct BulkEditChanges {
    bool changeDepartment = false; QString department;
    bool changeCourse     = false; QString course;
    bool changeYearLevel  = false; QString yearLevel;
    bool changeGender     = false; QString gender;
    bool changeStatus     = false; QString status;
};

// DatabaseViewModel members (Tasks 3–6):
static QList<StudentRecord> buildBulkUpdates(const QList<StudentRecord> &selected,
                                             const BulkEditChanges &changes);   // Task 3
BulkEditChanges currentChanges() const;                                        // Task 4 (private)
// Q_PROPERTY toggles (bool, WRITE setChangeX, NOTIFY changeXChanged):
//   changeDepartment, changeCourse, changeYearLevel, changeGender, changeStatus   // Task 4
// Q_PROPERTY values  (QString, WRITE setBulkX, NOTIFY bulkXChanged):
//   bulkDepartment, bulkCourse, bulkYearLevel, bulkGender, bulkStatus             // Task 4
// Q_PROPERTY (READ, NOTIFY): bulkCourses, canApplyBulk, bulkChangeSummary, bulkBusy // Tasks 4/5/6
Q_INVOKABLE void beginBulkEditSelected();   // Task 5
Q_INVOKABLE void applyBulkEdit();           // Task 6
// signals: bulkEditReady(), bulkEditFinished()                                 // Tasks 5/6
enum class EditMode    { NoEdit, SingleEdit, BulkEdit };   // Task 5
enum class CourseTarget{ SingleEdit, BulkEdit };           // Task 5
void applyServerRejection(const QString &message, const QString &genericFallback); // Task 1 (private)
```

QML (Tasks 7–9): `LCheckbox { checked; label; enabled; signal toggled(bool) }`; `BulkEditDialog { property var vm; signal applyRequested() }`.

---

## Task 1: Fold-in — `applyServerRejection` dedup

Extract the duplicated 401-vs-generic server-rejection tail (`onDeleteFinished` and `onBulkUpdateFinished`) into one helper. Pure refactor: the existing suite is the regression net; one new test pins the shared behavior.

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (add private method decl)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp:120-146` (onDeleteFinished), `:228-252` (onBulkUpdateFinished)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Produces: `void applyServerRejection(const QString &message, const QString &genericFallback)` (private).

- [ ] **Step 1: Write the failing test**

Add the slot declaration to the `private slots:` block in `tst_databaseviewmodel.cpp` (after `onBulkUpdateFailedSetsTransientStatus();`):

```cpp
    void serverRejectionSharedByDeleteAndBulk();
```

Add the test function (before `QTEST_MAIN`):

```cpp
void TestDatabaseViewModel::serverRejectionSharedByDeleteAndBulk()
{
    // Same auth-failure message must produce the SAME auth state + toast on
    // both the delete and the bulk-update paths (proves the shared helper).
    DatabaseViewModel del;
    del.onDeleteFinished(false, 2, QStringLiteral("Invalid admin key"));
    DatabaseViewModel bulk;
    BulkUpdateResult res; res.ok = false; res.message = "Invalid admin key";
    bulk.onBulkUpdateFinished(res);
    QCOMPARE(del.authFailure(), true);
    QCOMPARE(bulk.authFailure(), true);
    QCOMPARE(del.statusMessage(), bulk.statusMessage());   // identical auth toast

    // And a generic (non-auth) message stays non-auth on both, using each
    // path's own fallback when empty.
    DatabaseViewModel del2;  del2.onDeleteFinished(false, 1, QString());
    DatabaseViewModel bulk2; BulkUpdateResult r2; r2.ok = false; bulk2.onBulkUpdateFinished(r2);
    QCOMPARE(del2.authFailure(), false);
    QCOMPARE(bulk2.authFailure(), false);
    QCOMPARE(del2.statusMessage(), QStringLiteral("Delete failed."));
    QCOMPARE(bulk2.statusMessage(), QStringLiteral("Update failed."));
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: FAIL — the empty-message delete branch currently yields `"Delete failed."` (passes) but this is the red harness for the refactor; if it already passes, proceed (this task is a green-preserving refactor and the test documents the shared contract).

- [ ] **Step 3: Add the helper and reroute both call sites**

In `DatabaseViewModel.h`, add to the `private:` section (near `setAuthFailure`):

```cpp
    void applyServerRejection(const QString &message, const QString &genericFallback);
```

In `DatabaseViewModel.cpp`, add the implementation (place after `setAuthFailure`'s definition area, e.g. below `onEditCoursesLoaded`):

```cpp
void DatabaseViewModel::applyServerRejection(const QString &message,
                                             const QString &genericFallback)
{
    // Tell a 401 held-key failure apart from a generic server error via the
    // SAME predicate SettingsViewModel uses (§Error Taxonomy).
    if (SettingsViewModel::isAuthFailureMessage(message)) {
        setAuthFailure(true);
        setStatusMessage(tr("Admin authentication failed — re-enter via admin login."));
    } else {
        setAuthFailure(false);
        setStatusMessage(message.isEmpty() ? genericFallback : message);
    }
}
```

Replace the tail of `onDeleteFinished` (the `if (SettingsViewModel::isAuthFailureMessage(message)) { ... } else { ... }` block) with:

```cpp
    applyServerRejection(message, tr("Delete failed."));
```

Replace the tail of `onBulkUpdateFinished` (its `if (SettingsViewModel::isAuthFailureMessage(result.message)) { ... } else { ... }` block) with:

```cpp
    applyServerRejection(result.message, tr("Update failed."));
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: PASS (all `tst_databaseviewmodel` cases, including the two existing auth/generic delete + bulk tests, still green).

- [ ] **Step 5: Commit** (via the `commit` skill) — `refactor(database): extract applyServerRejection for delete+bulk auth tail`.

---

## Task 2: Fold-in — adaptive `canEdit` enable logic + over-emit fix

Redefine `canEdit` to enable the Edit button at **≥1** selected (the adaptive button opens single- vs bulk-edit downstream), and emit `canEditChanged` only when the derived bool actually flips.

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h:54` (canEdit body) + add `m_lastCanEdit` member
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp:27-29` (the selectionChanged→canEditChanged connection)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp` (update one existing test + add one)

**Interfaces:**
- Produces: `canEdit()` == `selectedCount() >= 1`; `canEditChanged` fires only on boolean flips. `beginEditSelected()` still internally requires exactly 1 (unchanged).

- [ ] **Step 1: Update the existing test to the new semantics + add the over-emit test**

Replace the existing `canEditIsTrueOnlyWhenExactlyOneSelected()` body (lines ~215-226) and rename its declaration + definition to `canEditIsTrueWhenAtLeastOneSelected`. In `private slots:` change the line to:

```cpp
    void canEditIsTrueWhenAtLeastOneSelected();
    void canEditChangedEmitsOnlyOnBooleanFlip();
```

Definitions:

```cpp
void TestDatabaseViewModel::canEditIsTrueWhenAtLeastOneSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    QCOMPARE(vm.canEdit(), false);   // 0 selected
    vm.students()->toggle("A");
    QCOMPARE(vm.canEdit(), true);    // exactly 1 -> single-edit
    vm.students()->toggle("B");
    QCOMPARE(vm.canEdit(), true);    // 2 selected -> bulk-edit (button still enabled)
}

void TestDatabaseViewModel::canEditChangedEmitsOnlyOnBooleanFlip()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    QSignalSpy spy(&vm, &DatabaseViewModel::canEditChanged);
    vm.students()->toggle("A");   // false -> true  : emit
    vm.students()->toggle("B");   // true  -> true  : no emit
    vm.students()->toggle("B");   // true  -> true  : no emit
    vm.students()->toggle("A");   // true  -> false : emit
    QCOMPARE(spy.count(), 2);
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: FAIL — `canEdit()` currently returns `selectedCount()==1` (false at 2), and `canEditChanged` currently fires on every `selectionChanged` (spy count 4, not 2).

- [ ] **Step 3: Implement**

In `DatabaseViewModel.h`, change `canEdit()` (line ~54) to:

```cpp
    bool canEdit() const { return m_students.selectedCount() >= 1; }
```

Add a private member (near `m_deleteInFlight`):

```cpp
    bool m_lastCanEdit = false;
```

In `DatabaseViewModel.cpp`, replace the connection at lines ~27-29:

```cpp
    // canEdit is a derived bool over selection size; only re-emit when it
    // actually flips (was: fired on every selectionChanged -> over-emit).
    connect(&m_students, &StudentsTableModel::selectionChanged, this, [this] {
        const bool now = canEdit();
        if (now != m_lastCanEdit) { m_lastCanEdit = now; emit canEditChanged(); }
    });
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: PASS.

- [ ] **Step 5: Commit** — `refactor(database): enable Edit at >=1 selection; fix canEditChanged over-emit`.

---

## Task 3: `buildBulkUpdates` free static + `BulkEditChanges` POD

The pure record-builder — the testable heart of bulk edit. Free static (the VM has no NAM injection seam, so records posted through the internal controller are not observable).

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (add POD above class + static decl)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (static impl)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Produces: `struct BulkEditChanges { ... }`; `static QList<StudentRecord> buildBulkUpdates(const QList<StudentRecord>&, const BulkEditChanges&)`.

- [ ] **Step 1: Write the failing test**

Declaration in `private slots:`:

```cpp
    void buildBulkUpdatesOverridesOnlyToggledFieldsAndCarriesRest();
```

Definition:

```cpp
void TestDatabaseViewModel::buildBulkUpdatesOverridesOnlyToggledFieldsAndCarriesRest()
{
    StudentRecord a; a.schoolId="A"; a.code="C-A"; a.name="Ann"; a.course="BSIT";
    a.department="CCS"; a.yearLevel="2"; a.gender="Female"; a.status="Active"; a.visits=5;
    StudentRecord b; b.schoolId="B"; b.code="C-B"; b.name="Ben"; b.course="BSCS";
    b.department="CCS"; b.yearLevel="3"; b.gender="Male"; b.status="Active"; b.visits=9;

    BulkEditChanges ch;
    ch.changeDepartment = true; ch.department = "CBA";
    ch.changeCourse     = true; ch.course     = "BSBA";
    ch.changeStatus     = true; ch.status     = "Inactive";

    const QList<StudentRecord> out = DatabaseViewModel::buildBulkUpdates({a, b}, ch);
    QCOMPARE(out.size(), 2);
    // Overridden on every record.
    QCOMPARE(out[0].department, QStringLiteral("CBA"));
    QCOMPARE(out[0].course,     QStringLiteral("BSBA"));
    QCOMPARE(out[0].status,     QStringLiteral("Inactive"));
    QCOMPARE(out[1].department, QStringLiteral("CBA"));
    // Carried through per-record, untouched.
    QCOMPARE(out[0].schoolId,  QStringLiteral("A"));
    QCOMPARE(out[0].name,      QStringLiteral("Ann"));
    QCOMPARE(out[0].code,      QStringLiteral("C-A"));
    QCOMPARE(out[0].yearLevel, QStringLiteral("2"));    // not toggled
    QCOMPARE(out[0].gender,    QStringLiteral("Female"));// not toggled
    QCOMPARE(out[0].visits,    5);
    QCOMPARE(out[1].name,      QStringLiteral("Ben"));   // NOT wiped
    QCOMPARE(out[1].yearLevel, QStringLiteral("3"));
    QCOMPARE(out[1].visits,    9);
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: FAIL to compile — `BulkEditChanges` / `buildBulkUpdates` not declared.

- [ ] **Step 3: Implement**

In `DatabaseViewModel.h`, add ABOVE the `class DatabaseViewModel` declaration (after the includes):

```cpp
// The operator's bulk-edit choices: a toggle + value per editable field.
// Passed to DatabaseViewModel::buildBulkUpdates (pure, unit-tested).
struct BulkEditChanges {
    bool changeDepartment = false; QString department;
    bool changeCourse     = false; QString course;
    bool changeYearLevel  = false; QString yearLevel;
    bool changeGender     = false; QString gender;
    bool changeStatus     = false; QString status;
};
```

In the `public:` section (near the other statics like `toCsv`):

```cpp
    // Pure: copies each selected record, overriding ONLY the toggled fields;
    // name/schoolId/code/visits carry through untouched. A free static because
    // the VM news up its own NAM (no injection seam) — this is the only way to
    // unit-test the override/carry-through rules network-free.
    static QList<StudentRecord> buildBulkUpdates(const QList<StudentRecord> &selected,
                                                 const BulkEditChanges &changes);
```

In `DatabaseViewModel.cpp`:

```cpp
QList<StudentRecord> DatabaseViewModel::buildBulkUpdates(const QList<StudentRecord> &selected,
                                                         const BulkEditChanges &changes)
{
    QList<StudentRecord> out;
    out.reserve(selected.size());
    for (StudentRecord r : selected) {   // copy — carries name/schoolId/code/visits
        if (changes.changeDepartment) r.department = changes.department;
        if (changes.changeCourse)     r.course     = changes.course;
        if (changes.changeYearLevel)  r.yearLevel  = changes.yearLevel;
        if (changes.changeGender)     r.gender     = changes.gender;
        if (changes.changeStatus)     r.status     = changes.status;
        out.append(r);
    }
    return out;
}
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: PASS.

- [ ] **Step 5: Commit** — `feat(database): add pure buildBulkUpdates record builder`.

---

## Task 4: VM bulk state — toggles, Dept⇔Course coupling, `canApplyBulk`, `bulkChangeSummary`, `currentChanges`

All bulk-edit VM state and derived read-outs, network-free. (The dependent course *load* is wired in Task 5.)

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (properties, setters, members, `currentChanges`, `emitBulkDerivedChanged`)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (implementations)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Consumes: `BulkEditChanges` (Task 3).
- Produces: `changeDepartment/Course/YearLevel/Gender/Status` (bool props), `bulkDepartment/Course/YearLevel/Gender/Status` (QString props), `bulkCourses` (READ, populated in Task 5), `canApplyBulk` (READ), `bulkChangeSummary` (READ QStringList), `currentChanges()` (private).

- [ ] **Step 1: Write the failing tests**

Declarations in `private slots:`:

```cpp
    void couplingDepartmentDrivesCourseToggle();
    void setChangeCourseWithoutDepartmentIsNoOp();
    void setBulkDepartmentClearsCourseValue();
    void canApplyBulkGating();
    void bulkChangeSummaryListsOnlyToggledInOrder();
```

Definitions:

```cpp
void TestDatabaseViewModel::couplingDepartmentDrivesCourseToggle()
{
    DatabaseViewModel vm;
    vm.setChangeDepartment(true);
    QCOMPARE(vm.changeCourse(), true);           // dept ON forces course ON
    vm.setBulkCourse("BSBA");
    vm.setChangeDepartment(false);
    QCOMPARE(vm.changeCourse(), false);          // dept OFF forces course OFF
    QCOMPARE(vm.bulkCourse(), QString());         // and clears the course value
}

void TestDatabaseViewModel::setChangeCourseWithoutDepartmentIsNoOp()
{
    DatabaseViewModel vm;
    vm.setChangeCourse(true);                     // no department toggled
    QCOMPARE(vm.changeCourse(), false);           // guarded no-op
}

void TestDatabaseViewModel::setBulkDepartmentClearsCourseValue()
{
    DatabaseViewModel vm;
    vm.setChangeDepartment(true);
    vm.setBulkDepartment("CCS");
    vm.setBulkCourse("BSIT");
    vm.setBulkDepartment("CBA");                  // real dept change
    QCOMPARE(vm.bulkCourse(), QString());          // dependent-clear
}

void TestDatabaseViewModel::canApplyBulkGating()
{
    DatabaseViewModel vm;
    QCOMPARE(vm.canApplyBulk(), false);                    // nothing toggled
    vm.setChangeYearLevel(true);
    QCOMPARE(vm.canApplyBulk(), false);                    // toggled but empty value
    vm.setBulkYearLevel("3");
    QCOMPARE(vm.canApplyBulk(), true);                     // one valid change
    // Department requires a course too.
    DatabaseViewModel vm2;
    vm2.setChangeDepartment(true);
    vm2.setBulkDepartment("CBA");
    QCOMPARE(vm2.canApplyBulk(), false);                   // dept set, course still empty
    vm2.setBulkCourse("BSBA");
    QCOMPARE(vm2.canApplyBulk(), true);
}

void TestDatabaseViewModel::bulkChangeSummaryListsOnlyToggledInOrder()
{
    DatabaseViewModel vm;
    vm.setChangeDepartment(true);  vm.setBulkDepartment("CBA");  vm.setBulkCourse("BSBA");
    vm.setChangeStatus(true);      vm.setBulkStatus("Inactive");
    QCOMPARE(vm.bulkChangeSummary(),
             (QStringList{ QStringLiteral("Department → CBA"),
                           QStringLiteral("Course → BSBA"),
                           QStringLiteral("Status → Inactive") }));
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: FAIL to compile — the new properties/setters don't exist.

- [ ] **Step 3: Implement**

In `DatabaseViewModel.h`, add the Q_PROPERTY lines (after the existing `edit*` properties):

```cpp
    Q_PROPERTY(bool changeDepartment READ changeDepartment WRITE setChangeDepartment NOTIFY changeDepartmentChanged)
    Q_PROPERTY(bool changeCourse     READ changeCourse     WRITE setChangeCourse     NOTIFY changeCourseChanged)
    Q_PROPERTY(bool changeYearLevel  READ changeYearLevel  WRITE setChangeYearLevel  NOTIFY changeYearLevelChanged)
    Q_PROPERTY(bool changeGender     READ changeGender     WRITE setChangeGender     NOTIFY changeGenderChanged)
    Q_PROPERTY(bool changeStatus     READ changeStatus     WRITE setChangeStatus     NOTIFY changeStatusChanged)
    Q_PROPERTY(QString bulkDepartment READ bulkDepartment WRITE setBulkDepartment NOTIFY bulkDepartmentChanged)
    Q_PROPERTY(QString bulkCourse     READ bulkCourse     WRITE setBulkCourse     NOTIFY bulkCourseChanged)
    Q_PROPERTY(QString bulkYearLevel  READ bulkYearLevel  WRITE setBulkYearLevel  NOTIFY bulkYearLevelChanged)
    Q_PROPERTY(QString bulkGender     READ bulkGender     WRITE setBulkGender     NOTIFY bulkGenderChanged)
    Q_PROPERTY(QString bulkStatus     READ bulkStatus     WRITE setBulkStatus     NOTIFY bulkStatusChanged)
    Q_PROPERTY(QStringList bulkCourses READ bulkCourses NOTIFY bulkCoursesChanged)
    Q_PROPERTY(bool canApplyBulk READ canApplyBulk NOTIFY canApplyBulkChanged)
    Q_PROPERTY(QStringList bulkChangeSummary READ bulkChangeSummary NOTIFY bulkChangeSummaryChanged)
```

Getters (public, near the existing `edit*` getters):

```cpp
    bool changeDepartment() const { return m_changeDepartment; }
    bool changeCourse() const { return m_changeCourse; }
    bool changeYearLevel() const { return m_changeYearLevel; }
    bool changeGender() const { return m_changeGender; }
    bool changeStatus() const { return m_changeStatus; }
    QString bulkDepartment() const { return m_bulkDepartment; }
    QString bulkCourse() const { return m_bulkCourse; }
    QString bulkYearLevel() const { return m_bulkYearLevel; }
    QString bulkGender() const { return m_bulkGender; }
    QString bulkStatus() const { return m_bulkStatus; }
    QStringList bulkCourses() const { return m_bulkCourses; }
    bool canApplyBulk() const;
    QStringList bulkChangeSummary() const;
```

Setter declarations (Q_INVOKABLE, near the existing `setEdit*`):

```cpp
    Q_INVOKABLE void setChangeDepartment(bool v);
    Q_INVOKABLE void setChangeCourse(bool v);
    Q_INVOKABLE void setChangeYearLevel(bool v);
    Q_INVOKABLE void setChangeGender(bool v);
    Q_INVOKABLE void setChangeStatus(bool v);
    Q_INVOKABLE void setBulkDepartment(const QString &v);
    Q_INVOKABLE void setBulkCourse(const QString &v);
    Q_INVOKABLE void setBulkYearLevel(const QString &v);
    Q_INVOKABLE void setBulkGender(const QString &v);
    Q_INVOKABLE void setBulkStatus(const QString &v);
```

Signals (in the `signals:` block):

```cpp
    void changeDepartmentChanged();
    void changeCourseChanged();
    void changeYearLevelChanged();
    void changeGenderChanged();
    void changeStatusChanged();
    void bulkDepartmentChanged();
    void bulkCourseChanged();
    void bulkYearLevelChanged();
    void bulkGenderChanged();
    void bulkStatusChanged();
    void bulkCoursesChanged();
    void canApplyBulkChanged();
    void bulkChangeSummaryChanged();
```

Private helpers + members:

```cpp
    // Packs the live toggle/value state into the POD for buildBulkUpdates,
    // canApplyBulk and bulkChangeSummary.
    BulkEditChanges currentChanges() const;
    // Every setter re-emits the two derived read-outs.
    void emitBulkDerivedChanged();

    bool m_changeDepartment = false, m_changeCourse = false, m_changeYearLevel = false,
         m_changeGender = false, m_changeStatus = false;
    QString m_bulkDepartment, m_bulkCourse, m_bulkYearLevel, m_bulkGender, m_bulkStatus;
    QStringList m_bulkCourses;
```

In `DatabaseViewModel.cpp`:

```cpp
BulkEditChanges DatabaseViewModel::currentChanges() const
{
    BulkEditChanges c;
    c.changeDepartment = m_changeDepartment; c.department = m_bulkDepartment;
    c.changeCourse     = m_changeCourse;     c.course     = m_bulkCourse;
    c.changeYearLevel  = m_changeYearLevel;  c.yearLevel  = m_bulkYearLevel;
    c.changeGender     = m_changeGender;     c.gender     = m_bulkGender;
    c.changeStatus     = m_changeStatus;     c.status     = m_bulkStatus;
    return c;
}

void DatabaseViewModel::emitBulkDerivedChanged()
{
    emit canApplyBulkChanged();
    emit bulkChangeSummaryChanged();
}

bool DatabaseViewModel::canApplyBulk() const
{
    const BulkEditChanges c = currentChanges();
    bool any = false;
    if (c.changeDepartment) {                  // dept + course move together
        if (c.department.isEmpty() || c.course.isEmpty()) return false;
        any = true;
    } else if (c.changeCourse) {
        return false;                          // course without dept is invalid
    }
    if (c.changeYearLevel) { if (c.yearLevel.isEmpty()) return false; any = true; }
    if (c.changeGender)    { if (c.gender.isEmpty())    return false; any = true; }
    if (c.changeStatus)    { if (c.status.isEmpty())    return false; any = true; }
    return any;
}

QStringList DatabaseViewModel::bulkChangeSummary() const
{
    const BulkEditChanges c = currentChanges();
    QStringList lines;
    if (c.changeDepartment) lines << tr("%1 → %2").arg(tr("Department"), c.department);
    if (c.changeCourse)     lines << tr("%1 → %2").arg(tr("Course"),     c.course);
    if (c.changeYearLevel)  lines << tr("%1 → %2").arg(tr("Year Level"), c.yearLevel);
    if (c.changeGender)     lines << tr("%1 → %2").arg(tr("Gender"),     c.gender);
    if (c.changeStatus)     lines << tr("%1 → %2").arg(tr("Status"),     c.status);
    return lines;
}

void DatabaseViewModel::setChangeDepartment(bool v)
{
    if (m_changeDepartment == v) return;
    m_changeDepartment = v; emit changeDepartmentChanged();
    // Coupling: Department and Course toggle together.
    if (v) {
        if (!m_changeCourse) { m_changeCourse = true; emit changeCourseChanged(); }
    } else {
        if (m_changeCourse) { m_changeCourse = false; emit changeCourseChanged(); }
        if (!m_bulkCourse.isEmpty()) { m_bulkCourse.clear(); emit bulkCourseChanged(); }
    }
    emitBulkDerivedChanged();
}

void DatabaseViewModel::setChangeCourse(bool v)
{
    // Course tracks Department; independent enabling is a guarded no-op.
    const bool target = v && m_changeDepartment;
    if (m_changeCourse == target) return;
    m_changeCourse = target; emit changeCourseChanged(); emitBulkDerivedChanged();
}

void DatabaseViewModel::setChangeYearLevel(bool v)
{ if (m_changeYearLevel == v) return; m_changeYearLevel = v; emit changeYearLevelChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setChangeGender(bool v)
{ if (m_changeGender == v) return; m_changeGender = v; emit changeGenderChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setChangeStatus(bool v)
{ if (m_changeStatus == v) return; m_changeStatus = v; emit changeStatusChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkDepartment(const QString &v)
{
    if (m_bulkDepartment == v) return;
    m_bulkDepartment = v; emit bulkDepartmentChanged();
    if (!m_bulkCourse.isEmpty()) { m_bulkCourse.clear(); emit bulkCourseChanged(); }
    // NOTE: the dependent course-list LOAD is wired in Task 5.
    emitBulkDerivedChanged();
}

void DatabaseViewModel::setBulkCourse(const QString &v)
{ if (m_bulkCourse == v) return; m_bulkCourse = v; emit bulkCourseChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkYearLevel(const QString &v)
{ if (m_bulkYearLevel == v) return; m_bulkYearLevel = v; emit bulkYearLevelChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkGender(const QString &v)
{ if (m_bulkGender == v) return; m_bulkGender = v; emit bulkGenderChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkStatus(const QString &v)
{ if (m_bulkStatus == v) return; m_bulkStatus = v; emit bulkStatusChanged(); emitBulkDerivedChanged(); }
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: PASS.

- [ ] **Step 5: Commit** — `feat(database): bulk-edit VM state, Dept<->Course coupling, canApplyBulk + summary`.

---

## Task 5: Course-target routing + `beginBulkEditSelected` + edit-mode enums

Route `m_editController`'s `coursesLoaded` to the single vs bulk course list; wire `setBulkDepartment` to actually load courses; add `beginBulkEditSelected` (clean reset + emit ready).

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (enums, members, `beginBulkEditSelected`, signals)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (`onEditCoursesLoaded` dispatch, `beginEdit` mode/target, `setBulkDepartment` load, `beginBulkEditSelected`)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Consumes: bulk state (Task 4).
- Produces: `beginBulkEditSelected()`; `bulkEditReady()` signal; `enum class EditMode`, `enum class CourseTarget`; `onEditCoursesLoaded` routes by `m_courseTarget`.

- [ ] **Step 1: Write the failing tests**

Declarations in `private slots:`:

```cpp
    void courseTargetRoutesBulkVsSingle();
    void beginBulkEditSelectedGuardsBelowTwo();
    void beginBulkEditSelectedResetsStateAndEmitsReady();
```

Definitions:

```cpp
void TestDatabaseViewModel::courseTargetRoutesBulkVsSingle()
{
    DatabaseViewModel vm;
    vm.setBulkDepartment("CBA");                 // sets target = BulkEdit (+ fires a load)
    vm.onEditCoursesLoaded({"BSBA", "BSA"});
    QCOMPARE(vm.bulkCourses(), (QStringList{"BSBA", "BSA"}));
    QVERIFY(vm.editCourses().isEmpty());          // single list untouched

    StudentRecord r; r.schoolId="A"; r.name="Ann"; r.department="CCS"; r.course="BSIT";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.beginEdit("A");                            // sets target = SingleEdit
    vm.onEditCoursesLoaded({"BSIT", "BSCS"});
    QCOMPARE(vm.editCourses(), (QStringList{"BSIT", "BSCS"}));
    QCOMPARE(vm.bulkCourses(), (QStringList{"BSBA", "BSA"}));   // bulk list unchanged
}

void TestDatabaseViewModel::beginBulkEditSelectedGuardsBelowTwo()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId="A"; a.name="Ann";
    vm.onSearchFinished(SearchOutcome::Results, {a}, "", "", 1);
    vm.students()->toggle("A");                   // only 1 selected
    QSignalSpy readySpy(&vm, &DatabaseViewModel::bulkEditReady);
    vm.beginBulkEditSelected();
    QCOMPARE(readySpy.count(), 0);                 // guarded — needs >= 2
}

void TestDatabaseViewModel::beginBulkEditSelectedResetsStateAndEmitsReady()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId="A"; a.name="Ann";
    StudentRecord b; b.schoolId="B"; b.name="Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("A"); vm.students()->toggle("B");   // 2 selected
    // Dirty the bulk state from a prior open.
    vm.setChangeStatus(true); vm.setBulkStatus("Inactive");
    QSignalSpy readySpy(&vm, &DatabaseViewModel::bulkEditReady);
    vm.beginBulkEditSelected();
    QCOMPARE(vm.changeStatus(), false);            // reset
    QCOMPARE(vm.bulkStatus(), QString());
    QCOMPARE(vm.canApplyBulk(), false);
    QCOMPARE(readySpy.count(), 1);
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: FAIL to compile — `beginBulkEditSelected` / `bulkEditReady` / the enums don't exist.

- [ ] **Step 3: Implement**

In `DatabaseViewModel.h`, add the scoped enums (in `public:`, near the top of the class) and the invokable + signal:

```cpp
    enum class EditMode    { NoEdit, SingleEdit, BulkEdit };
    enum class CourseTarget { SingleEdit, BulkEdit };

    Q_INVOKABLE void beginBulkEditSelected();
```

Signals block: add `void bulkEditReady();` and `void bulkEditFinished();` (the latter is used in Task 6).

Private members:

```cpp
    EditMode     m_editMode     = EditMode::NoEdit;
    CourseTarget m_courseTarget = CourseTarget::SingleEdit;
```

In `DatabaseViewModel.cpp`, replace `onEditCoursesLoaded` with a routing version:

```cpp
void DatabaseViewModel::onEditCoursesLoaded(const QStringList &courses)
{
    if (m_courseTarget == CourseTarget::BulkEdit) {
        m_bulkCourses = courses; emit bulkCoursesChanged();
    } else {
        m_editCourses = courses; emit editCoursesChanged();
    }
}
```

In `beginEdit` (single), just before `m_editController->loadCourses(r.department);` add:

```cpp
    m_editMode = EditMode::SingleEdit;
    m_courseTarget = CourseTarget::SingleEdit;
```

In `setBulkDepartment`, replace the `// NOTE: the dependent course-list LOAD is wired in Task 5.` line with:

```cpp
    m_courseTarget = CourseTarget::BulkEdit;
    m_editController->loadCourses(v);   // dependent course list for the bulk dialog
```

Also, in `setEditDepartment` (single), just before its `m_editController->loadCourses(dept);` add `m_courseTarget = CourseTarget::SingleEdit;` so a mid-edit dept change routes to the single list.

Add `beginBulkEditSelected`:

```cpp
void DatabaseViewModel::beginBulkEditSelected()
{
    if (m_students.selectedCount() < 2) return;   // header button only branches here at >= 2
    // Clean reset so a reopened dialog never inherits a prior session's state.
    m_changeDepartment = m_changeCourse = m_changeYearLevel = false;
    m_changeGender = m_changeStatus = false;
    m_bulkDepartment.clear(); m_bulkCourse.clear(); m_bulkYearLevel.clear();
    m_bulkGender.clear(); m_bulkStatus.clear(); m_bulkCourses.clear();
    emit changeDepartmentChanged(); emit changeCourseChanged(); emit changeYearLevelChanged();
    emit changeGenderChanged(); emit changeStatusChanged();
    emit bulkDepartmentChanged(); emit bulkCourseChanged(); emit bulkYearLevelChanged();
    emit bulkGenderChanged(); emit bulkStatusChanged(); emit bulkCoursesChanged();
    emitBulkDerivedChanged();
    m_editMode = EditMode::BulkEdit;
    emit bulkEditReady();
}
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: PASS.

- [ ] **Step 5: Commit** — `feat(database): bulk course-target routing + beginBulkEditSelected`.

---

## Task 6: `applyBulkEdit` + `bulkBusy` guard + generalized result routing

Apply the batch, guard re-entry (observable via `bulkBusy`), and generalize `onBulkUpdateFinished` to a count-based message that routes the finished signal by edit mode.

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (`applyBulkEdit`, `bulkBusy` prop + signal, `m_bulkInFlight`)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (`applyBulkEdit`, `onBulkUpdateFinished`, `onBulkUpdateFailed`)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp` (update one existing test + add three)

**Interfaces:**
- Consumes: `buildBulkUpdates` (Task 3), `currentChanges`/`canApplyBulk` (Task 4), `EditMode`/`bulkEditReady` (Task 5), `applyServerRejection` (Task 1).
- Produces: `applyBulkEdit()`; `bulkBusy` (READ bool, NOTIFY); `bulkEditFinished()` emitted on bulk success/no-op.

- [ ] **Step 1: Update the existing message test + write the new tests**

Update the existing `onBulkUpdateFinishedSuccessSetsStatusReloadsAndFinishes` — change its message assertion (line ~307):

```cpp
    QCOMPARE(vm.statusMessage(), QStringLiteral("Updated 1 student"));
```

Declarations in `private slots:`:

```cpp
    void onBulkUpdateFinishedPluralCountMessage();
    void applyBulkEditRoutesBulkFinishedAndClearsBusy();
    void applyBulkEditGuardsEmptyInvalidAndReentry();
```

Definitions:

```cpp
void TestDatabaseViewModel::onBulkUpdateFinishedPluralCountMessage()
{
    DatabaseViewModel vm;
    BulkUpdateResult res; res.ok = true; res.updatedCount = 3;
    vm.onBulkUpdateFinished(res);
    QCOMPARE(vm.statusMessage(), QStringLiteral("Updated 3 students"));
}

void TestDatabaseViewModel::applyBulkEditRoutesBulkFinishedAndClearsBusy()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    StudentRecord a; a.schoolId="A"; a.name="Ann"; a.status="Active";
    StudentRecord b; b.schoolId="B"; b.name="Ben"; b.status="Active";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("A"); vm.students()->toggle("B");
    vm.beginBulkEditSelected();                         // mode = BulkEdit
    vm.setChangeStatus(true); vm.setBulkStatus("Inactive");

    QSignalSpy bulkDone(&vm, &DatabaseViewModel::bulkEditFinished);
    QSignalSpy singleDone(&vm, &DatabaseViewModel::editFinished);
    QCOMPARE(vm.bulkBusy(), false);
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), true);                       // in flight

    BulkUpdateResult res; res.ok = true; res.updatedCount = 2;
    vm.onBulkUpdateFinished(res);
    QCOMPARE(vm.statusMessage(), QStringLiteral("Updated 2 students"));
    QCOMPARE(vm.bulkBusy(), false);                      // guard released
    QCOMPARE(bulkDone.count(), 1);                       // routed to BULK finished
    QCOMPARE(singleDone.count(), 0);
    AdminSession::instance().clear();
}

void TestDatabaseViewModel::applyBulkEditGuardsEmptyInvalidAndReentry()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    // No selection -> no-op, never goes busy.
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), false);

    StudentRecord a; a.schoolId="A"; a.name="Ann"; a.status="Active";
    StudentRecord b; b.schoolId="B"; b.name="Ben"; b.status="Active";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("A"); vm.students()->toggle("B");
    vm.beginBulkEditSelected();
    // Selection present but no toggled change -> canApplyBulk false -> no-op.
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), false);

    // Valid change -> goes busy; a second call is a re-entry no-op.
    vm.setChangeStatus(true); vm.setBulkStatus("Inactive");
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), true);
    vm.applyBulkEdit();                                  // must not crash / double-post
    QCOMPARE(vm.bulkBusy(), true);
    AdminSession::instance().clear();
}
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: FAIL — `applyBulkEdit`/`bulkBusy` don't exist; the updated message assertion fails against the current "Student updated".

- [ ] **Step 3: Implement**

In `DatabaseViewModel.h`: add the property + getter + invokable + member:

```cpp
    Q_PROPERTY(bool bulkBusy READ bulkBusy NOTIFY bulkBusyChanged)
    // ...
    bool bulkBusy() const { return m_bulkInFlight; }
    Q_INVOKABLE void applyBulkEdit();
    // signals: void bulkBusyChanged();
    // members:
    bool m_bulkInFlight = false;
```

In `DatabaseViewModel.cpp`, add `applyBulkEdit`:

```cpp
void DatabaseViewModel::applyBulkEdit()
{
    if (m_bulkInFlight) return;                             // re-entry guard
    const QList<StudentRecord> sel = m_students.selectedRecords();
    if (sel.isEmpty() || !canApplyBulk()) return;          // nothing to do
    const QList<StudentRecord> updates = buildBulkUpdates(sel, currentChanges());
    m_bulkInFlight = true; emit bulkBusyChanged();
    m_controller->bulkUpdateStudents(updates, AdminSession::instance().key());
}
```

Rewrite `onBulkUpdateFinished` (count-based, mode-routed, guard-releasing):

```cpp
void DatabaseViewModel::onBulkUpdateFinished(const BulkUpdateResult &result)
{
    m_bulkInFlight = false; emit bulkBusyChanged();
    if (result.ok) {
        setAuthFailure(false);
        if (result.updatedCount >= 1) {
            setStatusMessage(result.updatedCount == 1
                ? tr("Updated 1 student")
                : tr("Updated %1 students").arg(result.updatedCount));
            reloadTable();                 // re-fetch the current dept/course filter
            m_students.clearSelection();
        } else {
            setStatusMessage(tr("No changes to save"));   // no-op is a success
        }
        // Close whichever dialog is open (single vs bulk); NoEdit -> single, so
        // the existing single-edit tests that never set BulkEdit still get editFinished.
        if (m_editMode == EditMode::BulkEdit) emit bulkEditFinished();
        else                                  emit editFinished();
        return;
    }
    applyServerRejection(result.message, tr("Update failed."));  // dialog stays open
}
```

In `onBulkUpdateFailed`, add the guard release as the first line:

```cpp
    m_bulkInFlight = false; emit bulkBusyChanged();
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_databaseviewmodel
```
Expected: PASS (all `tst_databaseviewmodel`, including the previously-updated single-edit message test).

- [ ] **Step 5: Commit** — `feat(database): applyBulkEdit + bulkBusy guard + count-based result routing`.

---

## Task 7: `LCheckbox` component

A minimal themed checkbox primitive for the tri-state field toggles (reusable by the 4a.2b-iv register form).

**Files:**
- Create: `qt-app/quick/qml/components/LCheckbox.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (add to `QML_FILES`)
- Test: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Produces: `LCheckbox { bool checked; string label; bool enabled; signal toggled(bool checked) }`.

- [ ] **Step 1: Write the failing test**

In `tst_qml_components.qml`, raise the root `height` from `3080` to `3160`, and add a fixture in its own band below all existing fixtures (place it clear of the other fixtures — the last bands are used up to ~3080):

```qml
    LCheckbox { id: chk; objectName: "chk"; y: 3090; label: "Change Status" }
```

Add a `TestCase` (alongside the other component TestCases in the file):

```qml
    TestCase {
        name: "LCheckbox"; when: windowShown
        function init() { chk.checked = false; }
        function test_clickTogglesCheckedAndEmits() {
            var spy = signalSpy.createObject(chk, { target: chk, signalName: "toggled" });
            compare(chk.checked, false);
            mouseClick(chk);
            compare(chk.checked, true);
            compare(spy.count, 1);
            mouseClick(chk);
            compare(chk.checked, false);
            compare(spy.count, 2);
            spy.destroy();
        }
        function test_disabledDoesNotToggle() {
            chk.enabled = false;
            mouseClick(chk);
            compare(chk.checked, false);
            chk.enabled = true;
        }
    }
    Component { id: signalSpy; SignalSpy {} }
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_qml_components
```
Expected: FAIL — `LCheckbox` is not a known LOAMS type (module doesn't list it).

- [ ] **Step 3: Implement the component and register it**

Create `qt-app/quick/qml/components/LCheckbox.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Binary checkbox form primitive (§11): a `checked` box + label, emits
// toggled(bool). Theme-token styling only. Used for BulkEditDialog's per-field
// "change this" toggles (Phase 4a.2b-iii); reusable by the register form.
Item {
    id: root
    property bool checked: false
    property string label: ""
    property bool enabled: true
    signal toggled(bool checked)

    implicitHeight: Math.max(box.implicitHeight, labelText.implicitHeight)
    implicitWidth: row.implicitWidth

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Theme.spacing.sm
        Rectangle {
            id: box
            implicitWidth: 20; implicitHeight: 20
            radius: Theme.radius.sm2
            color: root.checked ? Theme.brand.base : Theme.card
            border.width: 2
            border.color: root.checked ? Theme.brand.base : Theme.border
            opacity: root.enabled ? 1 : 0.5
            Text {
                anchors.centerIn: parent
                visible: root.checked
                text: "\u2713"                       // check mark
                color: Theme.brand.on
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }
        Text {
            id: labelText
            text: root.label
            textFormat: Text.PlainText
            color: Theme.text
            opacity: root.enabled ? 1 : 0.5
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
            Layout.fillWidth: true
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onClicked: { root.checked = !root.checked; root.toggled(root.checked); }
    }
    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    Accessible.checked: root.checked
}
```

In `qt-app/quick/CMakeLists.txt`, add to the `QML_FILES` list (with the other `qml/components/*`):

```cmake
        qml/components/LCheckbox.qml
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R "tst_qml_components|tst_notokenaliases"
```
Expected: PASS (`tst_notokenaliases` also stays green — no deprecated token names / raw hex introduced).

- [ ] **Step 5: Commit** — `feat(components): add LCheckbox primitive`.

---

## Task 8: `BulkEditDialog` component

The tri-state bulk-edit form: five `LCheckbox` toggles + value controls, Department⇔Course coupling, prefill/severance guards, and an `applyRequested()` signal.

**Files:**
- Create: `qt-app/quick/qml/admin/BulkEditDialog.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (add to `QML_FILES`)
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (new fixture band + TestCase; raise root height)

**Interfaces:**
- Consumes: `LCheckbox` (Task 7); the VM surface `changeX`/`bulkX`/`bulkCourses`/`canApplyBulk`/`setChangeX`/`setBulkX` (Tasks 4–5).
- Produces: `BulkEditDialog { property var vm; signal applyRequested() }` with objectNames used by Task 9's screen tests.

- [ ] **Step 1: Write the failing tests**

In `tst_qml_admin.qml`, extend the geometry ledger comment, raise the root `height` from `5200` to `5900`, and add a bulk fixture band below the editDialog band (which ends at 5200):

```qml
    // --- BulkEditDialog fixture (own band below editDialog, y 5200..5900) ---
    Item {
        id: bulkEditBand
        y: 5200
        width: 900; height: 700

        QtObject {
            id: bulkStub
            property var departments: ["CCS", "CBA"]
            property var bulkCourses: ["BSBA", "BSA"]
            property bool changeDepartment: false
            property bool changeCourse: false
            property bool changeYearLevel: false
            property bool changeGender: false
            property bool changeStatus: false
            property string bulkDepartment: ""
            property string bulkCourse: ""
            property string bulkYearLevel: ""
            property string bulkGender: ""
            property string bulkStatus: ""
            property bool canApplyBulk: false
            property bool bulkBusy: false
            // Mirror the real coupling so the dialog's driven bindings behave.
            function setChangeDepartment(v) {
                changeDepartment = v;
                changeCourse = v;                 // coupled
                if (!v) bulkCourse = "";
            }
            function setChangeCourse(v) { changeCourse = v && changeDepartment; }
            function setChangeYearLevel(v) { changeYearLevel = v; }
            function setChangeGender(v) { changeGender = v; }
            function setChangeStatus(v) { changeStatus = v; }
            function setBulkDepartment(v) { bulkDepartment = v; bulkCourse = ""; }
            function setBulkCourse(v) { bulkCourse = v; }
            function setBulkYearLevel(v) { bulkYearLevel = v; }
            function setBulkGender(v) { bulkGender = v; }
            function setBulkStatus(v) { bulkStatus = v; }
        }

        BulkEditDialog { id: bulkDialog; anchors.fill: parent; vm: bulkStub }

        TestCase {
            name: "BulkEditDialog"; when: windowShown
            function init() {
                bulkStub.changeDepartment = false; bulkStub.changeCourse = false;
                bulkStub.changeStatus = false; bulkStub.bulkStatus = "";
                bulkStub.bulkDepartment = ""; bulkStub.bulkCourse = "";
                bulkStub.canApplyBulk = false; bulkStub.bulkBusy = false;
                bulkDialog.visible = false;
            }
            function test_valueControlDisabledUntilToggleOn() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var statusCombo = findChild(bulkDialog, "bulkStatusCombo");
                verify(statusCombo !== null);
                compare(statusCombo.enabled, false);          // toggle off
                bulkStub.changeStatus = true;
                compare(statusCombo.enabled, true);
            }
            function test_courseToggleFollowsDepartment() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var courseCheck = findChild(bulkDialog, "bulkCourseCheck");
                var courseCombo = findChild(bulkDialog, "bulkCourseCombo");
                verify(courseCheck !== null);
                compare(courseCheck.checked, false);
                compare(courseCombo.enabled, false);           // needs dept + a chosen dept
                // Turn Department on via its checkbox -> Course check follows.
                bulkStub.setChangeDepartment(true);
                compare(courseCheck.checked, true);
                bulkStub.bulkDepartment = "CBA";
                compare(courseCombo.enabled, true);
            }
            function test_applyDisabledUntilCanApply_andEmitsApplyRequested() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var apply = findChild(bulkDialog, "bulkApplyButton");
                verify(apply !== null);
                compare(apply.enabled, false);                 // canApplyBulk false
                bulkStub.canApplyBulk = true;
                compare(apply.enabled, true);
                var spy = signalSpy.createObject(bulkDialog, { target: bulkDialog, signalName: "applyRequested" });
                mouseClick(apply);
                compare(spy.count, 1);
                spy.destroy();
            }
            function test_cancelClosesDialog() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                mouseClick(findChild(bulkDialog, "bulkCancelButton"));
                compare(bulkDialog.visible, false);
            }
        }
    }
    Component { id: signalSpy; SignalSpy {} }
```

(If a `signalSpy` Component already exists in this file from an earlier task, reuse it — do not declare a duplicate id.)

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_qml_admin
```
Expected: FAIL — `BulkEditDialog` is not a known LOAMS type.

- [ ] **Step 3: Implement the dialog and register it**

Create `qt-app/quick/qml/admin/BulkEditDialog.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Bulk-edit form (Phase 4a.2b-iii). An LDialog-based modal driven by plain
// `visible`. Takes `property var vm` (a DatabaseViewModel, or a plain-QML stub
// in QuickTests). Each field is a tri-state row: an LCheckbox "change this"
// toggle + a value control disabled until the toggle is on. Only toggled fields
// are applied. School ID / Name are never bulk-editable. Department and Course
// are COUPLED (move together). Emits applyRequested() — the screen opens the
// change-preview confirm; the dialog stays open until vm.bulkEditFinished.
LDialog {
    id: root
    property var vm
    signal applyRequested()
    title: qsTr("Bulk edit students")

    // Same combo severance/prefill trap as StudentEditDialog: LComboBox
    // .selectValue() emits selected() and onActivated severs bindings. Guard
    // the on-open reset so it only sets displayed values, never re-enters the
    // vm setters; and re-sync the Course combo when the vm clears bulkCourse.
    property bool prefilling: false
    Connections {
        target: root.vm ? root.vm : null
        function onBulkCourseChanged() { bulkCourseCombo.selectValue(root.vm.bulkCourse); }
    }
    onVisibleChanged: if (visible && root.vm) {
        root.prefilling = true;
        deptCombo.selectValue(root.vm.bulkDepartment);
        bulkCourseCombo.selectValue(root.vm.bulkCourse);
        yearField.text = root.vm.bulkYearLevel;
        genderCombo.selectValue(root.vm.bulkGender);
        statusCombo.selectValue(root.vm.bulkStatus);
        root.prefilling = false;
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        // Department (drives Course).
        LCheckbox {
            objectName: "bulkDeptCheck"
            label: qsTr("Change Department")
            checked: root.vm ? root.vm.changeDepartment : false
            onToggled: function(on) { if (root.vm) root.vm.setChangeDepartment(on); }
        }
        LComboBox {
            id: deptCombo
            objectName: "bulkDeptCombo"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeDepartment : false
            model: root.vm ? root.vm.departments : []
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkDepartment(v); }
        }

        // Course (coupled to Department).
        LCheckbox {
            objectName: "bulkCourseCheck"
            label: qsTr("Change Course")
            // Driven by Department — checked iff Department is on.
            checked: root.vm ? root.vm.changeDepartment : false
            enabled: false                       // not independently toggleable
        }
        LComboBox {
            id: bulkCourseCombo
            objectName: "bulkCourseCombo"
            Layout.fillWidth: true
            enabled: root.vm ? (root.vm.changeDepartment && root.vm.bulkDepartment.length > 0) : false
            model: root.vm ? root.vm.bulkCourses : []
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkCourse(v); }
        }

        // Year Level.
        LCheckbox {
            objectName: "bulkYearCheck"
            label: qsTr("Change Year Level")
            checked: root.vm ? root.vm.changeYearLevel : false
            onToggled: function(on) { if (root.vm) root.vm.setChangeYearLevel(on); }
        }
        LTextField {
            id: yearField
            objectName: "bulkYearField"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeYearLevel : false
            label: qsTr("Year Level")
            onTextChanged: if (!root.prefilling && root.vm) root.vm.setBulkYearLevel(text)
        }

        // Gender.
        LCheckbox {
            objectName: "bulkGenderCheck"
            label: qsTr("Change Gender")
            checked: root.vm ? root.vm.changeGender : false
            onToggled: function(on) { if (root.vm) root.vm.setChangeGender(on); }
        }
        LComboBox {
            id: genderCombo
            objectName: "bulkGenderCombo"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeGender : false
            model: ["Male", "Female"]
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkGender(v); }
        }

        // Status.
        LCheckbox {
            objectName: "bulkStatusCheck"
            label: qsTr("Change Status")
            checked: root.vm ? root.vm.changeStatus : false
            onToggled: function(on) { if (root.vm) root.vm.setChangeStatus(on); }
        }
        LComboBox {
            id: statusCombo
            objectName: "bulkStatusCombo"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeStatus : false
            model: ["Active", "Inactive"]
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkStatus(v); }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "bulkCancelButton"
                variant: "Outline"
                text: qsTr("Cancel")
                onClicked: root.visible = false
            }
            LButton {
                objectName: "bulkApplyButton"
                text: qsTr("Apply")
                // Disabled until at least one valid change AND not mid-flight.
                enabled: root.vm ? (root.vm.canApplyBulk && !root.vm.bulkBusy) : false
                onClicked: root.applyRequested()
            }
        }
    }
}
```

In `qt-app/quick/CMakeLists.txt`, add to `QML_FILES` (with the other `qml/admin/*`):

```cmake
        qml/admin/BulkEditDialog.qml
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R "tst_qml_admin|tst_notokenaliases"
```
Expected: PASS.

- [ ] **Step 5: Commit** — `feat(admin): add BulkEditDialog tri-state bulk-edit form`.

---

## Task 9: `DatabaseScreen` wiring — adaptive Edit button + change-preview confirm

Wire the adaptive Edit button, the `BulkEditDialog` instance, the `bulkEditConfirm` change-preview, and the ready/finished Connections.

**Files:**
- Modify: `qt-app/quick/qml/admin/DatabaseScreen.qml` (Edit button, dialog instance, confirm, Connections)
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (extend the Database fixture stub + TestCase)

**Interfaces:**
- Consumes: `BulkEditDialog` (Task 8); the VM surface `beginBulkEditSelected`/`beginEditSelected`/`bulkChangeSummary`/`requiresTypedConfirmation`/`applyBulkEdit`/`bulkEditReady`/`bulkEditFinished` (Tasks 4–6).

- [ ] **Step 1: Write the failing tests**

In `tst_qml_admin.qml`, extend the existing Database `stubVm` (around line 1648) with the bulk surface + spies. The bulk **field** props are required (not just the spies): the `BulkEditDialog` instance the screen hosts reads `vm.bulkDepartment`/`bulkCourse`/… in its `onVisibleChanged` prefill and in its enable bindings — without them the dialog logs undefined-read warnings on open, exactly as the `edit*` props already guard the single dialog.

```qml
            property int beginBulkEditSelectedCount: 0
            property int applyBulkEditCount: 0
            property var bulkChangeSummary: ["Status → Inactive"]
            property bool bulkBusy: false
            property bool canApplyBulk: false
            // Bulk field surface the hosted BulkEditDialog binds to.
            property bool changeDepartment: false
            property bool changeCourse: false
            property bool changeYearLevel: false
            property bool changeGender: false
            property bool changeStatus: false
            property string bulkDepartment: ""
            property string bulkCourse: ""
            property string bulkYearLevel: ""
            property string bulkGender: ""
            property string bulkStatus: ""
            property var bulkCourses: []
            signal bulkEditReady()
            signal bulkEditFinished()
            function beginBulkEditSelected() { beginBulkEditSelectedCount++; bulkEditReady(); }
            function applyBulkEdit() { applyBulkEditCount++; }
            // No-op setters so the dialog's onSelected/onToggled handlers resolve.
            function setChangeDepartment(v) { changeDepartment = v; changeCourse = v; if (!v) bulkCourse = ""; }
            function setChangeCourse(v) { changeCourse = v && changeDepartment; }
            function setChangeYearLevel(v) { changeYearLevel = v; }
            function setChangeGender(v) { changeGender = v; }
            function setChangeStatus(v) { changeStatus = v; }
            function setBulkDepartment(v) { bulkDepartment = v; bulkCourse = ""; }
            function setBulkCourse(v) { bulkCourse = v; }
            function setBulkYearLevel(v) { bulkYearLevel = v; }
            function setBulkGender(v) { bulkGender = v; }
            function setBulkStatus(v) { bulkStatus = v; }
```

Also change the stub's `canEdit` semantics note: keep `property bool canEdit: false` but the tests below drive `selectedCount`. Add a `selectedCount` mirror to `stubModel` usage — `stubModel.selectedCount` already exists.

Add tests to the `DatabaseScreen` TestCase (reset the new counters in `init()`):

```qml
            function test_editButtonEnabledWhenAnySelected() {
                var btn = findChild(databaseScreen, "editButton");
                stubModel.selectedCount = 0; stubVm.canEdit = false;
                compare(btn.enabled, false);
                stubModel.selectedCount = 1; stubVm.canEdit = true;
                compare(btn.enabled, true);
                stubModel.selectedCount = 3;   // canEdit stays true (>=1)
                compare(btn.enabled, true);
            }
            function test_editButtonOpensSingleEditAtOne() {
                stubModel.selectedCount = 1; stubVm.canEdit = true;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "editButton"));
                compare(stubVm.beginEditSelectedCount, 1);
                compare(stubVm.beginBulkEditSelectedCount, 0);
            }
            function test_editButtonOpensBulkEditAtTwoPlus() {
                stubModel.selectedCount = 2; stubVm.canEdit = true;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "editButton"));
                compare(stubVm.beginBulkEditSelectedCount, 1);
                compare(stubVm.beginEditSelectedCount, 0);
            }
            function test_bulkEditReadyOpensDialog_finishedCloses() {
                var d = findChild(databaseScreen, "bulkEditDialog");
                verify(d !== null);
                compare(d.visible, false);
                stubVm.bulkEditReady();
                compare(d.visible, true);
                stubVm.bulkEditFinished();
                compare(d.visible, false);
            }
            function test_applyRequestedOpensPreviewThenConfirmApplies() {
                stubModel.selectedCount = 2;
                stubVm.bulkEditReady();
                var d = findChild(databaseScreen, "bulkEditDialog");
                d.applyRequested();                       // dialog asks to apply
                var confirm = findChild(databaseScreen, "bulkEditConfirm");
                verify(confirm !== null);
                compare(confirm.visible, true);
                // Preview restates the change + count.
                verify(confirm.message.indexOf("Status → Inactive") !== -1);
                verify(confirm.message.indexOf("2") !== -1);
                compare(confirm.requireTypedConfirmation, false);   // 2 < 10
                mouseClick(findChild(confirm, "confirmButton"));
                compare(stubVm.applyBulkEditCount, 1);
            }
            function test_bulkConfirmTypedGateForLargeSelection() {
                stubModel.selectedCount = 12;
                stubVm.bulkEditReady();
                findChild(databaseScreen, "bulkEditDialog").applyRequested();
                var confirm = findChild(databaseScreen, "bulkEditConfirm");
                compare(confirm.requireTypedConfirmation, true);
                var btn = findChild(confirm, "confirmButton");
                compare(btn.enabled, false);
                findChild(confirm, "confirmTypedField").text = "UPDATE";
                compare(btn.enabled, true);
            }
```

Add the new counters to the Database TestCase `init()`:

```qml
                stubVm.beginBulkEditSelectedCount = 0;
                stubVm.applyBulkEditCount = 0;
                var bd = findChild(databaseScreen, "bulkEditDialog");
                if (bd) bd.visible = false;
                var bc = findChild(databaseScreen, "bulkEditConfirm");
                if (bc) { bc.visible = false; bc.clearKey(); }
```

- [ ] **Step 2: Run to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure -R tst_qml_admin
```
Expected: FAIL — no `bulkEditDialog`/`bulkEditConfirm` in the screen; the Edit button still branches only on `canEdit`.

- [ ] **Step 3: Implement the screen wiring**

In `qt-app/quick/qml/admin/DatabaseScreen.qml`, replace the Edit `LButton` (lines ~101-110) with the adaptive version:

```qml
            LButton {
                objectName: "editButton"
                variant: "Primary"
                compact: true
                text: qsTr("Edit")
                enabled: screen.vm ? screen.vm.canEdit : false     // >= 1 selected
                tooltipText: screen.selectedCount > 1
                             ? qsTr("Bulk-edit the %1 selected students").arg(screen.selectedCount)
                             : qsTr("Edit the selected student")
                accessibleName: screen.selectedCount > 1
                                ? qsTr("Bulk-edit the %1 selected students").arg(screen.selectedCount)
                                : qsTr("Edit the selected student")
                onClicked: {
                    if (!screen.vm) return;
                    if (screen.selectedCount === 1) screen.vm.beginEditSelected();
                    else                             screen.vm.beginBulkEditSelected();
                }
            }
```

After the existing `StudentEditDialog { id: editDialog ... }` block (near the end, ~line 200), add the bulk dialog, then — declared AFTER it so its scrim stacks on top — the confirm:

```qml
    BulkEditDialog {
        id: bulkEditDialog
        objectName: "bulkEditDialog"
        vm: screen.vm
        onApplyRequested: bulkEditConfirm.visible = true
    }

    // Change-preview gate. Declared AFTER bulkEditDialog so its scrim renders on
    // top of the still-open bulk dialog (LDialog stacks by declaration order).
    LConfirmDialog {
        id: bulkEditConfirm
        objectName: "bulkEditConfirm"
        title: qsTr("Apply bulk changes?")
        // PlainText (LDialog pins it): restate exactly what changes + the count.
        message: (screen.vm ? screen.vm.bulkChangeSummary.join("\n") : "")
                 + qsTr("\n\nApply to %1 students. Unlisted fields are left unchanged.")
                       .arg(screen.selectedCount)
        confirmText: qsTr("Apply")
        requireTypedConfirmation: screen.vm ? screen.vm.requiresTypedConfirmation(screen.selectedCount) : false
        confirmationWord: "UPDATE"
        onConfirmed: { bulkEditConfirm.visible = false; if (screen.vm) screen.vm.applyBulkEdit(); }
    }
```

Extend the edit-dialog `Connections` block (the one handling `onEditReady`/`onEditFinished`) to also drive the bulk dialog:

```qml
    Connections {
        target: screen.vm ? screen.vm : null
        function onEditReady() { editDialog.visible = true; }
        function onEditFinished() { editDialog.visible = false; }
        function onBulkEditReady() { bulkEditDialog.visible = true; }
        function onBulkEditFinished() { bulkEditDialog.visible = false; }
    }
```

- [ ] **Step 4: Run to verify green**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure
```
Expected: PASS — the FULL suite (all `tst_*`) green.

- [ ] **Step 5: Commit** — `feat(database): adaptive Edit button + bulk change-preview wiring`.

---

## Final verification (after Task 9)

- [ ] **Full clean build + suite:**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biii; ctest --test-dir C:/b/l42biii --output-on-failure
```
Expected: all tests PASS (no new warnings beyond the known CRLF / QXlsx ones).

- [ ] **GUI smoke-test** (desktop GUI app — clean build is necessary but not sufficient): run `C:/b/l42biii/qt-app/quick/WITSQuick.exe` against a running XAMPP. Log in as admin, open Database, select several students, click **Edit** → the bulk dialog opens. Toggle Status → Inactive and (optionally) Department (which forces Course) → click **Apply** → the preview lists exactly those changes and "Apply to N students" → confirm. Verify the table reflects the change and the toast reads "Updated K students". Kill the app before any `git worktree remove` (a running exe blocks removal on Windows).

- [ ] Then run `/claude-review` (fresh-context Opus subagent — `claude -p` is not logged in) on the branch, address Critical/Important, and hand to `create-pr`.

---

## Notes / gotchas carried into execution

- **Rebuild before every ctest** (stale-binary trap) — the QuickTest binaries embed the QML.
- **New QML files must be added to `QML_FILES`** in `qt-app/quick/CMakeLists.txt` or `import LOAMS; <Type>` fails to resolve (Tasks 7, 8).
- **QuickTest fixture bands** (Tasks 7, 8, 9): each new fixture gets its own y-band and the root `height:` is raised — synthetic mouse events are window-local ([[quicktest-taphandler-doubletap-unusable]]).
- **`TapHandler` double-tap is unusable under QuickTest** — not relevant here (no new double-click), but the row double-click reused in single-edit stays `MouseArea.onDoubleClicked`.
- **`m_editMode == BulkEdit` routes the finished signal**; the existing single-edit result tests never set `BulkEdit`, so `NoEdit → editFinished` keeps them green (Task 6).
- **Known narrow race** (acknowledged in the spec, not fixed): `m_courseTarget` is a single shared var and `loadCourses` replies carry no request-id; a late single-edit `coursesLoaded` could land in `bulkCourses` after a dialog switch. Harmless (re-scoped on the next dept pick).
