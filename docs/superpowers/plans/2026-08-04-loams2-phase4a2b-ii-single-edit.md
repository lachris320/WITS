# LOAMS 2.0 Phase 4a.2b-ii — Single-Student Edit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add editing one student to the Database screen: a header **Edit** button (enabled only when exactly one row is selected) and **double-click a row** open a prefilled modal form that saves via the existing `bulkUpdateStudents` with a one-element list.

**Architecture:** Strict MVVM. `DatabaseViewModel` grows an edit surface (`edit*` state, `canEdit`, `beginEdit`/`beginEditSelected`/`setEditDepartment`/`saveEdit`, and `bulkUpdateFinished`/`bulkUpdateFailed` handlers) plus a **second** `StudentController` that loads the edit dialog's course list independently of the filter. A new `StudentEditDialog.qml` (an `LDialog` form) is the view. `LTable` gains a `rowActivated(schoolId)` signal on double-click. The shared controller's reply classifier is generalized (`deleteReplyIsServerAnswer` → `replyIsServerAnswer`) and `bulkUpdateStudents` is rewired through it so a stale-admin-key 401 surfaces as an auth failure, not a transport error.

**Tech Stack:** Qt 6 / C++17 / QML (Qt Quick), CMake + Ninja + MinGW. QtTest (C++) + Qt Quick Test under CTest via `wits_add_qttest()`.

## Global Constraints

- **Strict MVVM** — C++ ViewModels are the ONLY QML-facing layer. QML screens take `property var vm`; QML never calls a `witscore` controller directly. QuickTests inject a plain-QML stub VM.
- **Theming** — `Theme.qml` is the single source of visual tokens. ZERO raw hex outside `Theme.qml`; opacity via `Qt.alpha(Theme.<token>, a)`, never a literal color.
- **Server-supplied text is PlainText** — any `Text` that can show a server string (reached over cleartext HTTP) MUST pin `textFormat: Text.PlainText` (anti-injection).
- **One component per QML file.** QML types + C++ ViewModel/model classes are `PascalCase`; C++ members are `m_camelCase`.
- **Tests** — register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add `OFFSCREEN` for any GUI/Quick/painting/network test.
- **Admin key** — RAM-only via `AdminSession::instance().key()`; never logged, URL-encoded into a GET, or exposed to QML.
- **Editable fields are exactly six:** `name, course, year_level, department, gender, status` WHERE `school_id`. `school_id`/`code`/`visits`/`photo` are never modified by this slice.
- **Commit via the `commit` skill only** — never raw `git add`/`git commit`. Never `git add -A` (stage by name). Never `--no-verify`. Never commit `qt-app/build/`.
- **Build/test commands** (Qt 6.11.1 MinGW kit, tools NOT on PATH — use the memorized toolchain): configure `cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH=<Qt kit>`; build `cmake --build qt-app/build`; test `ctest --test-dir qt-app/build --output-on-failure`. Use a short build dir (e.g. `C:\b\loams-dbg`) if the default overflows MAX_PATH.

---

### Task 1: Generalize the reply classifier + rewire `bulkUpdateStudents` (C++ controller)

Pull the 4a.2b-i 401 classification into `bulkUpdateStudents` (decision A). First prove the behavior with a failing 401 test, make it pass by rewiring the reply handler, then rename `deleteReplyIsServerAnswer` → `replyIsServerAnswer` as a pure refactor.

**Files:**
- Modify: `qt-app/core/studentcontroller.cpp` (`bulkUpdateStudents` reply lambda, lines ~230-275; `deleteReplyIsServerAnswer` definition, lines ~180-188; call site in `deleteStudents`, line ~304)
- Modify: `qt-app/core/studentcontroller.h` (classifier declaration + comment, lines ~39-44)
- Test: `qt-app/tests/tst_studentcontroller.cpp`

**Interfaces:**
- Consumes: `parseBulkUpdateResponse(const QByteArray&) -> BulkUpdateResult` (exists); `CapturingNam(body, error, httpStatus)` (exists).
- Produces: `static bool StudentController::replyIsServerAnswer(bool replyHadError, int httpStatus, const QByteArray &body)` (renamed from `deleteReplyIsServerAnswer`, logic unchanged). `bulkUpdateStudents` now emits `bulkUpdateFinished(parseBulkUpdateResponse(body))` for any server answer (incl. 401-with-body) and `bulkUpdateFailed(errorString)` only for a no-HTTP-status transport failure.

- [ ] **Step 1: Write the failing 401 integration test**

Add the slot declaration to the `private slots:` block in `tst_studentcontroller.cpp` (next to `deleteStudents_guard401WithBody_emitsDeleteFinishedNotFailed`):

```cpp
    void bulkUpdate_guard401WithBody_emitsBulkUpdateFinishedNotFailed();
```

Add the test body (after `deleteStudents_guard401WithBody_emitsDeleteFinishedNotFailed`):

```cpp
void TestStudentController::bulkUpdate_guard401WithBody_emitsBulkUpdateFinishedNotFailed()
{
    // requireAdminAuth answers a stale/bad admin key with HTTP 401 + a JSON
    // error body. The OLD bulkUpdateStudents treated ANY reply->error() as a
    // transport failure and emitted bulkUpdateFailed, never reaching
    // parseBulkUpdateResponse — mis-surfacing a stale key as a network problem.
    // The 401 body must reach bulkUpdateFinished with ok==false and the
    // server's message; bulkUpdateFailed must NOT fire. Mirrors the delete test.
    const QByteArray body = R"({"status":"error","message":"Invalid admin key"})";
    CapturingNam nam(body, QNetworkReply::AuthenticationRequiredError, 401);
    StudentController ctrl(&nam);

    QSignalSpy finishedSpy(&ctrl, &StudentController::bulkUpdateFinished);
    QSignalSpy failedSpy(&ctrl, &StudentController::bulkUpdateFailed);

    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    ctrl.bulkUpdateStudents(QList<StudentRecord>() << r, "stale-key");

    QVERIFY(finishedSpy.wait(1000));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);

    const BulkUpdateResult res = qvariant_cast<BulkUpdateResult>(finishedSpy.takeFirst().at(0));
    QVERIFY(!res.ok);
    QCOMPARE(res.message, QStringLiteral("Invalid admin key"));
}
```

> **Metatype note (verify at the RED step).** `qvariant_cast<BulkUpdateResult>` from a `QSignalSpy` arg should work under Qt 6 without any `Q_DECLARE_METATYPE`: moc auto-registers a signal's parameter types, and `QMetaType::fromType<BulkUpdateResult>()` resolves for a complete type. Do NOT assume it is "already registered" — `studentdata.h` has no `Q_DECLARE_METATYPE(BulkUpdateResult)` and no existing test captures `bulkUpdateFinished` via a spy (the existing `bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey` only inspects the request body). If Step 2's compile/run shows the cast failing, add `Q_DECLARE_METATYPE(BulkUpdateResult)` to `qt-app/core/studentdata.h` and `qRegisterMetaType<BulkUpdateResult>()` at the top of the test body, then re-run. Confirm the cast at the red step rather than trusting this note.

- [ ] **Step 2: Run the test to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: FAIL — `failedSpy.count()` is 1 and `finishedSpy.count()` is 0 (old early-return path).

- [ ] **Step 3: Rewire `bulkUpdateStudents` reply handler**

In `qt-app/core/studentcontroller.cpp`, replace the reply lambda in `bulkUpdateStudents` (currently lines ~263-274) with the read-everything-then-classify shape (mirrors `deleteStudents`):

```cpp
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray resp = reply->readAll();
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QString errorString = reply->errorString();
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (deleteReplyIsServerAnswer(hadError, httpStatus, resp)) {
            emit bulkUpdateFinished(parseBulkUpdateResponse(resp));   // 401 body reaches here
        } else {
            emit bulkUpdateFailed(errorString);                      // genuine transport failure only
        }
    });
```

(Classifier still named `deleteReplyIsServerAnswer` at this point — renamed in Step 5.)

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: PASS (all `tst_studentcontroller` cases, including the new one).

- [ ] **Step 5: Rename `deleteReplyIsServerAnswer` → `replyIsServerAnswer` (pure refactor)**

In `qt-app/core/studentcontroller.h`, rename the declaration and update the comment (it now serves both callers):

```cpp
    // True when a reply is a decodable server answer (has an HTTP status +
    // body) rather than a transport failure. Mirrors HttpForm::isServerAnswer
    // so a guard 401 (bad/expired admin key) reaches parseDeleteResponse /
    // parseBulkUpdateResponse instead of being misreported as a network error.
    // Shared by deleteStudents and bulkUpdateStudents.
    static bool replyIsServerAnswer(bool replyHadError, int httpStatus,
                                    const QByteArray &body);
```

In `qt-app/core/studentcontroller.cpp`, rename the definition (`bool StudentController::deleteReplyIsServerAnswer(...)` → `bool StudentController::replyIsServerAnswer(...)`, body unchanged) and both call sites (`deleteStudents` line ~304 and the `bulkUpdateStudents` lambda from Step 3) from `deleteReplyIsServerAnswer(` to `replyIsServerAnswer(`.

In `qt-app/tests/tst_studentcontroller.cpp`, rename the five classifier unit-test slots and their bodies from `deleteReplyIsServerAnswer_*` to `replyIsServerAnswer_*`, and update each `StudentController::deleteReplyIsServerAnswer(` call to `StudentController::replyIsServerAnswer(`:
- `deleteReplyIsServerAnswer_transportNoStatus_isFalse` → `replyIsServerAnswer_transportNoStatus_isFalse`
- `deleteReplyIsServerAnswer_401WithBody_isTrue` → `replyIsServerAnswer_401WithBody_isTrue`
- `deleteReplyIsServerAnswer_200Error_isTrue` → `replyIsServerAnswer_200Error_isTrue`
- `deleteReplyIsServerAnswer_200Success_isTrue` → `replyIsServerAnswer_200Success_isTrue`
- `deleteReplyIsServerAnswer_statusButEmptyBody_isFalse` → `replyIsServerAnswer_statusButEmptyBody_isFalse`

(Rename BOTH the declarations in the `private slots:` block and the `void TestStudentController::…` definitions.)

- [ ] **Step 6: Rebuild and run to verify the rename kept everything green**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_studentcontroller --output-on-failure`
Expected: PASS — every `tst_studentcontroller` case green; no reference to `deleteReplyIsServerAnswer` remains (grep to confirm: `grep -rn deleteReplyIsServerAnswer qt-app` returns nothing).

- [ ] **Step 7: Commit**

Use the `commit` skill. Suggested message: `refactor(core): generalize reply classifier + route bulkUpdate 401 through it (4a.2b-ii)`.

---

### Task 2: `DatabaseViewModel` edit state — `beginEdit`/`beginEditSelected`/`setEditDepartment` + second controller

Add the edit-form state, `canEdit`, the record-locate/prefill entry points, dependent course loading via a **second** `StudentController` (decision B), and the field setters. (Save + finish handlers are Task 3.)

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h`
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp`
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Consumes: `StudentsTableModel::allRecords() -> QList<StudentRecord>`, `selectedIds() -> QStringList`, `selectedCount() -> int`, `selectionChanged()` signal (all exist); `StudentController::loadCourses(const QString&)` + `coursesLoaded(const QStringList&)` (exist).
- Produces (relied on by Tasks 3, 5, 6): `Q_PROPERTY bool canEdit`; `Q_PROPERTY QString editSchoolId/editName/editYearLevel/editGender/editStatus/editDepartment/editCourse`; `Q_PROPERTY QStringList editCourses`; `Q_INVOKABLE void beginEdit(const QString&)`, `void beginEditSelected()`, `void setEditDepartment(const QString&)`, setters `setEditName/setEditYearLevel/setEditGender/setEditStatus/setEditCourse`; public slot `onEditCoursesLoaded(const QStringList&)`; signal `editReady()`; private members `m_editCode`/`m_editVisits` carry the located record's untouched fields for Task 3's `saveEdit`.

- [ ] **Step 1: Write the failing VM tests**

Add these slot declarations to the `private slots:` block in `tst_databaseviewmodel.cpp`:

```cpp
    void canEditIsTrueOnlyWhenExactlyOneSelected();
    void beginEditPrefillsAllFieldsAndEmitsReady();
    void beginEditNoMatchIsNoOp();
    void beginEditSelectedUsesSingleSelectedId();
    void setEditDepartmentClearsCourseAndReloads();
    void onEditCoursesLoadedPopulatesEditCourses();
```

Add the test bodies:

```cpp
void TestDatabaseViewModel::canEditIsTrueOnlyWhenExactlyOneSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    QCOMPARE(vm.canEdit(), false);              // 0 selected
    vm.students()->toggle("A");
    QCOMPARE(vm.canEdit(), true);               // exactly 1
    vm.students()->toggle("B");
    QCOMPARE(vm.canEdit(), false);              // 2 selected
}

void TestDatabaseViewModel::beginEditPrefillsAllFieldsAndEmitsReady()
{
    DatabaseViewModel vm;
    StudentRecord r;
    r.schoolId = "2023-001"; r.code = "C1"; r.name = "Juan Cruz"; r.course = "BSIT";
    r.department = "CCS"; r.yearLevel = "2"; r.gender = "Male"; r.status = "Active";
    r.visits = 7;
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);

    QSignalSpy readySpy(&vm, &DatabaseViewModel::editReady);
    vm.beginEdit("2023-001");

    QCOMPARE(vm.editSchoolId(), QStringLiteral("2023-001"));
    QCOMPARE(vm.editName(), QStringLiteral("Juan Cruz"));
    QCOMPARE(vm.editYearLevel(), QStringLiteral("2"));
    QCOMPARE(vm.editGender(), QStringLiteral("Male"));
    QCOMPARE(vm.editStatus(), QStringLiteral("Active"));
    QCOMPARE(vm.editDepartment(), QStringLiteral("CCS"));
    QCOMPARE(vm.editCourse(), QStringLiteral("BSIT"));
    QCOMPARE(readySpy.count(), 1);
}

void TestDatabaseViewModel::beginEditNoMatchIsNoOp()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);

    QSignalSpy readySpy(&vm, &DatabaseViewModel::editReady);
    vm.beginEdit("does-not-exist");
    QCOMPARE(readySpy.count(), 0);                 // no signal — dialog stays closed
    QVERIFY(vm.editSchoolId().isEmpty());          // edit state untouched
}

void TestDatabaseViewModel::beginEditSelectedUsesSingleSelectedId()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("B");                    // exactly one selected

    QSignalSpy readySpy(&vm, &DatabaseViewModel::editReady);
    vm.beginEditSelected();
    QCOMPARE(vm.editSchoolId(), QStringLiteral("B"));
    QCOMPARE(vm.editName(), QStringLiteral("Ben"));
    QCOMPARE(readySpy.count(), 1);
}

void TestDatabaseViewModel::setEditDepartmentClearsCourseAndReloads()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "A"; r.name = "Ann"; r.department = "CCS"; r.course = "BSIT";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.beginEdit("A");
    QCOMPARE(vm.editCourse(), QStringLiteral("BSIT"));

    vm.setEditDepartment("CBA");
    QCOMPARE(vm.editDepartment(), QStringLiteral("CBA"));
    QCOMPARE(vm.editCourse(), QString());          // dependent-clear
}

void TestDatabaseViewModel::onEditCoursesLoadedPopulatesEditCourses()
{
    DatabaseViewModel vm;
    vm.onEditCoursesLoaded({"BSIT", "BSCS"});
    QCOMPARE(vm.editCourses(), (QStringList{"BSIT", "BSCS"}));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir qt-app/build -R tst_databaseviewmodel --output-on-failure`
Expected: FAIL to COMPILE — `canEdit`, `editSchoolId`, `beginEdit`, etc. do not exist yet.

- [ ] **Step 3: Add the edit surface to the header**

In `qt-app/quick/viewmodels/DatabaseViewModel.h`, add these `Q_PROPERTY` lines after the existing `authFailure` property (line ~30):

```cpp
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY canEditChanged)
    Q_PROPERTY(QString editSchoolId READ editSchoolId NOTIFY editSchoolIdChanged)
    Q_PROPERTY(QString editName READ editName WRITE setEditName NOTIFY editNameChanged)
    Q_PROPERTY(QString editYearLevel READ editYearLevel WRITE setEditYearLevel NOTIFY editYearLevelChanged)
    Q_PROPERTY(QString editGender READ editGender WRITE setEditGender NOTIFY editGenderChanged)
    Q_PROPERTY(QString editStatus READ editStatus WRITE setEditStatus NOTIFY editStatusChanged)
    Q_PROPERTY(QString editDepartment READ editDepartment NOTIFY editDepartmentChanged)
    Q_PROPERTY(QString editCourse READ editCourse WRITE setEditCourse NOTIFY editCourseChanged)
    Q_PROPERTY(QStringList editCourses READ editCourses NOTIFY editCoursesChanged)
```

Add the getters in the `public:` block (after `bool authFailure() const`, line ~44):

```cpp
    bool canEdit() const { return m_students.selectedCount() == 1; }
    QString editSchoolId() const { return m_editSchoolId; }
    QString editName() const { return m_editName; }
    QString editYearLevel() const { return m_editYearLevel; }
    QString editGender() const { return m_editGender; }
    QString editStatus() const { return m_editStatus; }
    QString editDepartment() const { return m_editDepartment; }
    QString editCourse() const { return m_editCourse; }
    QStringList editCourses() const { return m_editCourses; }
```

Add the invokables + setters after `exportCsv` (line ~55):

```cpp
    Q_INVOKABLE void beginEdit(const QString &schoolId);
    Q_INVOKABLE void beginEditSelected();
    Q_INVOKABLE void setEditDepartment(const QString &dept);
    Q_INVOKABLE void setEditName(const QString &v);
    Q_INVOKABLE void setEditYearLevel(const QString &v);
    Q_INVOKABLE void setEditGender(const QString &v);
    Q_INVOKABLE void setEditStatus(const QString &v);
    Q_INVOKABLE void setEditCourse(const QString &v);
```

Add the public slot after `onDeleteFailed` (line ~64):

```cpp
    void onEditCoursesLoaded(const QStringList &courses);
```

Add the signals after `authFailureChanged()` (line ~74):

```cpp
    void canEditChanged();
    void editSchoolIdChanged();
    void editNameChanged();
    void editYearLevelChanged();
    void editGenderChanged();
    void editStatusChanged();
    void editDepartmentChanged();
    void editCourseChanged();
    void editCoursesChanged();
    void editReady();
```

Add the private members after `m_authFailure` / `m_deleteInFlight` (line ~91):

```cpp
    QNetworkAccessManager *m_editNam = nullptr;
    StudentController *m_editController = nullptr;
    QString m_editSchoolId, m_editName, m_editYearLevel, m_editGender, m_editStatus;
    QString m_editDepartment, m_editCourse, m_editCode;
    int m_editVisits = 0;
    QStringList m_editCourses;
```

- [ ] **Step 4: Implement the edit surface in the .cpp**

In `qt-app/quick/viewmodels/DatabaseViewModel.cpp`, in the constructor body (after the existing `connect(...)` lines, before the closing brace, line ~21), add the second controller + wiring:

```cpp
    m_editNam = new QNetworkAccessManager(this);
    m_editController = new StudentController(m_editNam, this);
    connect(m_editController, &StudentController::coursesLoaded,
            this, &DatabaseViewModel::onEditCoursesLoaded);
    // canEdit tracks selection size — re-emit whenever the model's selection changes.
    connect(&m_students, &StudentsTableModel::selectionChanged,
            this, &DatabaseViewModel::canEditChanged);
```

Add the method implementations (anywhere after the delete handlers, e.g. before `exportCsv`):

```cpp
void DatabaseViewModel::beginEdit(const QString &schoolId)
{
    const QList<StudentRecord> recs = m_students.allRecords();
    int idx = -1;
    for (int i = 0; i < recs.size(); ++i) {
        if (recs.at(i).schoolId == schoolId) { idx = i; break; }
    }
    if (idx < 0)
        return;   // not found — no-op; do not touch edit state or open the dialog

    const StudentRecord &r = recs.at(idx);
    m_editSchoolId = r.schoolId;   emit editSchoolIdChanged();
    m_editCode     = r.code;       // carried unchanged into saveEdit (not editable)
    m_editVisits   = r.visits;     // carried unchanged into saveEdit (not editable)
    m_editName     = r.name;       emit editNameChanged();
    m_editYearLevel= r.yearLevel;  emit editYearLevelChanged();
    m_editGender   = r.gender;     emit editGenderChanged();
    m_editStatus   = r.status;     emit editStatusChanged();
    m_editDepartment = r.department; emit editDepartmentChanged();
    m_editCourse   = r.course;     emit editCourseChanged();

    m_editController->loadCourses(r.department);   // independent of the filter's course list
    emit editReady();
}

void DatabaseViewModel::beginEditSelected()
{
    if (m_students.selectedCount() != 1)
        return;                                    // header button only enabled at 1
    beginEdit(m_students.selectedIds().first());
}

void DatabaseViewModel::setEditDepartment(const QString &dept)
{
    if (m_editDepartment == dept)
        return;   // no actual change — do NOT clear the course or reload (mirrors
                  // the filter's setDepartment at DatabaseViewModel.cpp:38-51, and
                  // is the second line of defense behind the dialog's prefill guard:
                  // re-selecting the same department must never blank the course).
    m_editDepartment = dept; emit editDepartmentChanged();
    if (!m_editCourse.isEmpty()) { m_editCourse.clear(); emit editCourseChanged(); }
    m_editController->loadCourses(dept);           // re-scope the edit course list
}

void DatabaseViewModel::onEditCoursesLoaded(const QStringList &courses)
{
    m_editCourses = courses; emit editCoursesChanged();
}

void DatabaseViewModel::setEditName(const QString &v)
{ if (m_editName != v) { m_editName = v; emit editNameChanged(); } }

void DatabaseViewModel::setEditYearLevel(const QString &v)
{ if (m_editYearLevel != v) { m_editYearLevel = v; emit editYearLevelChanged(); } }

void DatabaseViewModel::setEditGender(const QString &v)
{ if (m_editGender != v) { m_editGender = v; emit editGenderChanged(); } }

void DatabaseViewModel::setEditStatus(const QString &v)
{ if (m_editStatus != v) { m_editStatus = v; emit editStatusChanged(); } }

void DatabaseViewModel::setEditCourse(const QString &v)
{ if (m_editCourse != v) { m_editCourse = v; emit editCourseChanged(); } }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_databaseviewmodel --output-on-failure`
Expected: PASS (all cases, including the six new ones).

- [ ] **Step 6: Commit**

Use the `commit` skill. Suggested message: `feat(quick): DatabaseViewModel edit state + dependent course loading (4a.2b-ii)`.

---

### Task 3: `DatabaseViewModel.saveEdit` + `bulkUpdateFinished`/`bulkUpdateFailed` handlers

Build the `StudentRecord` from edit state and post it via the **primary** controller; handle the three success/failure branches; emit `editFinished()` to let the view close the dialog.

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h`
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp`
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Consumes: `m_controller->bulkUpdateStudents(QList<StudentRecord>, QString)` + `bulkUpdateFinished(const BulkUpdateResult&)` / `bulkUpdateFailed(const QString&)` (Task 1); `AdminSession::instance().key()`; `SettingsViewModel::isAuthFailureMessage(const QString&)` (both exist, already used by delete); `m_editCode`/`m_editVisits` (Task 2).
- Produces (relied on by Task 6): `Q_INVOKABLE void saveEdit()`; public slots `onBulkUpdateFinished(const BulkUpdateResult&)`, `onBulkUpdateFailed(const QString&)`; signal `editFinished()` (emitted on updated≥1 and on no-op updated==0; NOT on error).

- [ ] **Step 1: Write the failing handler tests**

Add slot declarations to `tst_databaseviewmodel.cpp`:

```cpp
    void onBulkUpdateFinishedSuccessSetsStatusReloadsAndFinishes();
    void onBulkUpdateFinishedNoChangeSetsStatusAndFinishes();
    void onBulkUpdateFinishedAuthFailureSetsAuthStateKeepsOpen();
    void onBulkUpdateFinishedGenericFailureSetsStatusNoAuth();
    void onBulkUpdateFailedSetsTransientStatus();
    void saveEditEntersGuardedPath();
```

Add the test bodies (`#include "studentdata.h"` is already present for `BulkUpdateResult`):

```cpp
void TestDatabaseViewModel::onBulkUpdateFinishedSuccessSetsStatusReloadsAndFinishes()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "A"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);

    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = true; res.updatedCount = 1;
    vm.onBulkUpdateFinished(res);

    QCOMPARE(vm.statusMessage(), QStringLiteral("Student updated"));
    QVERIFY(!vm.authFailure());
    QVERIFY(vm.loading());                 // reloadTable() flipped loading on
    QCOMPARE(finishedSpy.count(), 1);
}

void TestDatabaseViewModel::onBulkUpdateFinishedNoChangeSetsStatusAndFinishes()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = true; res.updatedCount = 0;
    vm.onBulkUpdateFinished(res);

    QCOMPARE(vm.statusMessage(), QStringLiteral("No changes to save"));
    QVERIFY(!vm.authFailure());
    QCOMPARE(finishedSpy.count(), 1);      // dialog closes on a no-op too
}

void TestDatabaseViewModel::onBulkUpdateFinishedAuthFailureSetsAuthStateKeepsOpen()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = false; res.message = "Invalid admin key";
    vm.onBulkUpdateFinished(res);

    QVERIFY(vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(finishedSpy.count(), 0);      // dialog stays open on error
}

void TestDatabaseViewModel::onBulkUpdateFinishedGenericFailureSetsStatusNoAuth()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = false; res.message = "Some updates failed, rolled back";
    vm.onBulkUpdateFinished(res);

    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.statusMessage(), QStringLiteral("Some updates failed, rolled back"));
    QCOMPARE(finishedSpy.count(), 0);
}

void TestDatabaseViewModel::onBulkUpdateFailedSetsTransientStatus()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    vm.onBulkUpdateFailed(QStringLiteral("Connection refused"));
    QVERIFY(!vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(finishedSpy.count(), 0);
}

void TestDatabaseViewModel::saveEditEntersGuardedPath()
{
    // saveEdit reads the AdminSession key and posts via the VM's own NAM (no
    // live server in CI). The exact wire form (1-element students JSON array +
    // admin_key) is asserted at the controller level in
    // tst_studentcontroller::bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey.
    // Here we only prove saveEdit does not early-return: with a located record
    // it leaves edit state intact and fires without crashing.
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    StudentRecord r; r.schoolId = "A"; r.name = "Ann"; r.department = "CCS"; r.course = "BSIT";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.beginEdit("A");
    vm.setEditName("Ann Edited");
    vm.saveEdit();                         // posts; reply handled asynchronously
    QCOMPARE(vm.editName(), QStringLiteral("Ann Edited"));
    AdminSession::instance().clear();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `ctest --test-dir qt-app/build -R tst_databaseviewmodel --output-on-failure`
Expected: FAIL to COMPILE — `saveEdit`, `onBulkUpdateFinished`, `onBulkUpdateFailed`, `editFinished` do not exist.

- [ ] **Step 3: Declare `saveEdit`, the handlers, and `editFinished` in the header**

In `DatabaseViewModel.h`, add to the invokables block (after `setEditCourse`, Task 2):

```cpp
    Q_INVOKABLE void saveEdit();
```

Add public slots (after `onEditCoursesLoaded`, Task 2):

```cpp
    void onBulkUpdateFinished(const BulkUpdateResult &result);
    void onBulkUpdateFailed(const QString &errorString);
```

Add the signal (after `editReady()`, Task 2):

```cpp
    void editFinished();
```

- [ ] **Step 4: Implement in the .cpp**

In the constructor body (after the Task 2 edit wiring), connect the PRIMARY controller's bulk signals:

```cpp
    connect(m_controller, &StudentController::bulkUpdateFinished,
            this, &DatabaseViewModel::onBulkUpdateFinished);
    connect(m_controller, &StudentController::bulkUpdateFailed,
            this, &DatabaseViewModel::onBulkUpdateFailed);
```

Add the method bodies:

```cpp
void DatabaseViewModel::saveEdit()
{
    StudentRecord rec;
    rec.schoolId   = m_editSchoolId;   // immutable identity (WHERE key)
    rec.code       = m_editCode;       // carried unchanged
    rec.visits     = m_editVisits;     // carried unchanged
    rec.name       = m_editName;
    rec.department = m_editDepartment;
    rec.course     = m_editCourse;
    rec.yearLevel  = m_editYearLevel;
    rec.gender     = m_editGender;
    rec.status     = m_editStatus;
    // Primary controller (search/delete/bulkUpdate) — NOT the edit course loader.
    m_controller->bulkUpdateStudents({rec}, AdminSession::instance().key());
}

void DatabaseViewModel::onBulkUpdateFinished(const BulkUpdateResult &result)
{
    if (result.ok && result.updatedCount >= 1) {
        setAuthFailure(false);
        setStatusMessage(tr("Student updated"));
        reloadTable();                 // re-fetch the current dept/course filter
        emit editFinished();
        return;
    }
    if (result.ok) {                   // updatedCount == 0: no-op edit is a success
        setAuthFailure(false);
        setStatusMessage(tr("No changes to save"));
        emit editFinished();
        return;
    }
    // Server rejection. Distinguish a 401 held-key failure from a generic error
    // via the SAME predicate delete uses (§Error Taxonomy). Keep the dialog open.
    if (SettingsViewModel::isAuthFailureMessage(result.message)) {
        setAuthFailure(true);
        setStatusMessage(tr("Admin authentication failed — re-enter via admin login."));
    } else {
        setAuthFailure(false);
        setStatusMessage(result.message.isEmpty() ? tr("Update failed.") : result.message);
    }
}

void DatabaseViewModel::onBulkUpdateFailed(const QString & /*errorString*/)
{
    setAuthFailure(false);
    setStatusMessage(tr("Update failed — check your connection."));
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_databaseviewmodel --output-on-failure`
Expected: PASS (all cases).

- [ ] **Step 6: Commit**

Use the `commit` skill. Suggested message: `feat(quick): DatabaseViewModel saveEdit + bulkUpdate result handlers (4a.2b-ii)`.

---

### Task 4: `LTable.rowActivated(schoolId)` on double-click

Add a backward-compatible double-click signal that does not fight the per-row checkbox.

**Files:**
- Modify: `qt-app/quick/qml/components/LTable.qml`
- Test: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Produces (relied on by Task 6): `signal rowActivated(string schoolId)` on `LTable`, emitted on row-body double-click with `model.schoolId`.

- [ ] **Step 1: Write the failing QuickTest**

Add a self-contained fixture to `tst_qml_components.qml` (append inside the root item, alongside the other component fixtures — match the file's existing fixture style):

```qml
    // --- LTable.rowActivated double-click (4a.2b-ii) ---
    Item {
        id: rowActBand
        width: 400; height: 200
        LTable {
            id: rowActTable
            anchors.fill: parent
            selectable: true
            columns: [ { key: "name", title: "Name" } ]
            model: ListModel {
                ListElement { schoolId: "S1"; name: "Ann"; selected: false }
                ListElement { schoolId: "S2"; name: "Ben"; selected: false }
            }
        }
        SignalSpy {
            id: rowActSpy
            target: rowActTable
            signalName: "rowActivated"
        }
        TestCase {
            name: "LTableRowActivated"; when: windowShown
            function init() { rowActSpy.clear(); }
            function test_doubleClickEmitsRowActivatedWithSchoolId() {
                var row = findChild(rowActTable, "tableRow_0");
                verify(row !== null);
                // Double-click on the row BODY (far right), away from the
                // ~18x18 checkbox at the left edge.
                mouseDoubleClick(row, row.width - 12, row.height / 2);
                compare(rowActSpy.count, 1);
                compare(rowActSpy.signalArguments[0][0], "S1");
            }
        }
    }
```

> If `tst_qml_components.qml` does not already `import QtTest`, it does (every `tst_qml_*.qml` uses `TestCase`); `SignalSpy` ships in the same `QtTest` import — no extra import needed.

- [ ] **Step 2: Run the test to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: FAIL — `rowActivated` is not a signal on `LTable` (QML error / spy never fires).

- [ ] **Step 3: Add the signal and the double-click handler**

In `qt-app/quick/qml/components/LTable.qml`, add the signal to the root `Rectangle` properties (after `property bool animateRows: false`, line ~24):

```qml
    // Double-click a row body to activate/edit it (4a.2b-ii). Backward-compatible:
    // no existing consumer is required to connect it. Emitted only on double-click.
    signal rowActivated(string schoolId)
```

In the row delegate (`Rectangle { id: rowDelegate … }`), add a `TapHandler` as a child of `rowDelegate`, placed immediately AFTER the `HoverHandler { id: hover }` line (~202) and BEFORE the `RowLayout`. A `TapHandler` on the row coexists with the checkbox's `MouseArea` (the MouseArea takes an exclusive grab within its 18×18 bounds, so a double-click there never reaches the row handler):

```qml
                // Row-body double-click → rowActivated. The per-row checkbox
                // MouseArea (below, inside the RowLayout) exclusively grabs its
                // own ~18x18 area, so a double-click on the checkbox toggles
                // selection and never activates the row; a double-click anywhere
                // else on the row activates it.
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onDoubleTapped: table.rowActivated(
                        rowDelegate.model.schoolId !== undefined
                            ? rowDelegate.model.schoolId : "")
                }
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir qt-app/build -R tst_qml_components --output-on-failure`
Expected: PASS. Re-run the full quick component + admin suites later to confirm no regression in existing selection behavior.

- [ ] **Step 5: Commit**

Use the `commit` skill. Suggested message: `feat(quick): LTable rowActivated double-click signal (4a.2b-ii)`.

---

### Task 5: `StudentEditDialog.qml` — the modal edit form

Create the `LDialog`-based form, register it in the QML module, and test it against a stub VM.

**Files:**
- Create: `qt-app/quick/qml/admin/StudentEditDialog.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (add the QML file to `QML_FILES`)
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes (from the vm): `departments`, `editCourses`, `editSchoolId`, `editName`, `editYearLevel`, `editGender`, `editStatus`, `editDepartment`, `editCourse`; `setEditDepartment(value)`, `setEditName/Year/Gender/Status/Course(value)`, `saveEdit()`; signal `editCourseChanged()`. `LComboBox.selectValue(v)` + `signal selected(string)`; `LTextField.text` alias.
- Produces (relied on by Task 6): a `StudentEditDialog { property var vm }` type; objectNames `editSchoolIdText`, `editNameField`, `editDeptCombo`, `editCourseCombo`, `editYearField`, `editGenderCombo`, `editStatusCombo`, `editSaveButton`, `editCancelButton`. Driven by plain `visible`. Closing is `visible = false` (Cancel) or the parent screen reacting to `vm.editFinished()`.

- [ ] **Step 1: Write the failing QuickTest fixture**

Append a new band to `tst_qml_admin.qml` (inside the root item, after the `databaseBand` closes, ~line 1761 — before the final root `}`):

```qml
    // --- StudentEditDialog fixture (own band below Database, y 4500..5200) ---
    // Minimal edit-only stub so the dialog can be exercised without the screen.
    Item {
        id: editDialogBand
        y: 4500
        width: 900; height: 700

        QtObject {
            id: editStub
            property var departments: ["CCS", "CBA"]
            property var editCourses: ["BSIT", "BSCS"]
            property string editSchoolId: "2023-001"
            property string editName: "Juan Cruz"
            property string editYearLevel: "2"
            property string editGender: "Male"
            property string editStatus: "Active"
            property string editDepartment: "CCS"
            property string editCourse: "BSIT"
            property int setDeptCount: 0
            property string lastDept: ""
            property int saveCount: 0
            function setEditDepartment(d) { setDeptCount++; lastDept = d; editDepartment = d; editCourse = ""; }
            function setEditName(v) { editName = v; }
            function setEditYearLevel(v) { editYearLevel = v; }
            function setEditGender(v) { editGender = v; }
            function setEditStatus(v) { editStatus = v; }
            function setEditCourse(v) { editCourse = v; }
            function saveEdit() { saveCount++; }
        }

        StudentEditDialog { id: editDialog; anchors.fill: parent; vm: editStub }

        TestCase {
            name: "StudentEditDialog"; when: windowShown
            function init() {
                editStub.editName = "Juan Cruz";
                editStub.editCourse = "BSIT";
                editStub.editDepartment = "CCS";
                editStub.setDeptCount = 0;
                editStub.saveCount = 0;
                editDialog.visible = false;
            }
            function test_prefillsFromVmOnOpen() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                compare(findChild(editDialog, "editNameField").text, "Juan Cruz");
                compare(findChild(editDialog, "editDeptCombo").currentValue, "CCS");
                compare(findChild(editDialog, "editCourseCombo").currentValue, "BSIT");
                compare(findChild(editDialog, "editGenderCombo").currentValue, "Male");
                compare(findChild(editDialog, "editStatusCombo").currentValue, "Active");
                verify(findChild(editDialog, "editSchoolIdText").text.indexOf("2023-001") !== -1);
            }
            function test_saveDisabledWhenNameEmpty() {
                editStub.editName = "";
                editDialog.visible = true;
                waitForRendering(editDialog);
                compare(findChild(editDialog, "editSaveButton").enabled, false);
                editStub.editName = "Something";
                compare(findChild(editDialog, "editSaveButton").enabled, true);
            }
            function test_saveInvokesVmSaveEdit() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                mouseClick(findChild(editDialog, "editSaveButton"));
                compare(editStub.saveCount, 1);
            }
            function test_cancelClosesDialog() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                mouseClick(findChild(editDialog, "editCancelButton"));
                compare(editDialog.visible, false);
            }
            function test_departmentChangeCallsVmAndResyncsCourseCombo() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                compare(findChild(editDialog, "editCourseCombo").currentValue, "BSIT");
                // Simulate picking a new department via the combo's own path.
                findChild(editDialog, "editDeptCombo").selectValue("CBA");
                compare(editStub.setDeptCount, 1);
                compare(editStub.lastDept, "CBA");
                // The vm cleared editCourse (""); the Connections re-sync must
                // reset the Course combo's displayed value.
                compare(findChild(editDialog, "editCourseCombo").currentValue, "");
            }
        }
    }
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: FAIL — `StudentEditDialog` is not a registered type (QML module error).

- [ ] **Step 3: Create the dialog**

Create `qt-app/quick/qml/admin/StudentEditDialog.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Single-student edit form (Phase 4a.2b-ii). An LDialog-based modal driven by
// plain `visible`. Takes `property var vm` (a DatabaseViewModel, or a plain-QML
// stub in QuickTests). Edits exactly the six server-editable fields; School ID
// is shown read-only (it is the immutable identity / WHERE key).
LDialog {
    id: root
    property var vm

    // Guard against the programmatic prefill firing the vm setters.
    // LComboBox.selectValue(v) EMITS `selected(v)` (LComboBox.qml:30-35), so
    // pushing vm state into a combo on open would re-enter its onSelected and
    // call the vm setter. For Department that is destructive: setEditDepartment
    // CLEARS editCourse, which would blank the just-prefilled Course before
    // courseCombo.selectValue(editCourse) runs. While `prefilling` is true, the
    // onSelected/onTextChanged handlers skip the vm push — so the open-time
    // sync only sets each control's displayed value, never mutates the vm.
    property bool prefilling: false
    title: qsTr("Edit student")

    // LComboBox.onActivated imperatively assigns its own currentValue, which
    // severs any declarative `currentValue: vm.editX` binding after the first
    // pick. So push vm state INTO the combos via selectValue(...) on open, and
    // — critically — re-sync the Course combo whenever the vm clears/changes
    // editCourse (a real department change sets it to ""), or the dependent-clear
    // would not visibly reset the combo. This re-sync's own selectValue is
    // prefilling-safe: a genuine clear happens with prefilling=false, and its
    // re-entrant setEditCourse("") is an idempotent no-op in the vm.
    Connections {
        target: root.vm ? root.vm : null
        function onEditCourseChanged() { courseCombo.selectValue(root.vm.editCourse); }
    }
    onVisibleChanged: if (visible && root.vm) {
        root.prefilling = true;
        nameField.text = root.vm.editName;
        yearField.text = root.vm.editYearLevel;
        deptCombo.selectValue(root.vm.editDepartment);
        courseCombo.selectValue(root.vm.editCourse);
        genderCombo.selectValue(root.vm.editGender);
        statusCombo.selectValue(root.vm.editStatus);
        root.prefilling = false;
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        Text {
            objectName: "editSchoolIdText"
            text: qsTr("School ID: %1").arg(root.vm ? root.vm.editSchoolId : "")
            // Read-only identity; server-supplied — pin plain (anti-injection).
            textFormat: Text.PlainText
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        LTextField {
            id: nameField
            objectName: "editNameField"
            Layout.fillWidth: true
            label: qsTr("Name")
            // No `text:` binding — see the combo-severance note: bind-then-type
            // would sever it. Set imperatively on open (above, under prefilling),
            // push edits back to the vm here so Save-enable (vm.editName) stays
            // reactive. `!root.prefilling` skips the push during open-time sync.
            onTextChanged: if (!root.prefilling && root.vm) root.vm.setEditName(text)
        }

        LComboBox {
            id: deptCombo
            objectName: "editDeptCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.departments : []
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditDepartment(value); }
        }

        LComboBox {
            id: courseCombo
            objectName: "editCourseCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.editCourses : []
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditCourse(value); }
        }

        LTextField {
            id: yearField
            objectName: "editYearField"
            Layout.fillWidth: true
            label: qsTr("Year Level")
            onTextChanged: if (!root.prefilling && root.vm) root.vm.setEditYearLevel(text)
        }

        LComboBox {
            id: genderCombo
            objectName: "editGenderCombo"
            Layout.fillWidth: true
            model: ["Male", "Female"]
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditGender(value); }
        }

        LComboBox {
            id: statusCombo
            objectName: "editStatusCombo"
            Layout.fillWidth: true
            model: ["Active", "Inactive"]
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditStatus(value); }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "editCancelButton"
                variant: "Outline"
                text: qsTr("Cancel")
                onClicked: root.visible = false
            }
            LButton {
                objectName: "editSaveButton"
                text: qsTr("Save")
                // Required Name — disabled while blank (inline field precondition).
                enabled: root.vm && root.vm.editName.trim().length > 0
                onClicked: if (root.vm) root.vm.saveEdit()
            }
        }
    }
}
```

- [ ] **Step 4: Register the QML file**

In `qt-app/quick/CMakeLists.txt`, add to the `QML_FILES` list (after `qml/admin/DatabaseScreen.qml`, line ~48):

```cmake
        qml/admin/StudentEditDialog.qml
```

- [ ] **Step 5: Configure, build, and run to verify the tests pass**

Run: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS (all five `StudentEditDialog` cases).

- [ ] **Step 6: Commit**

Use the `commit` skill. Suggested message: `feat(quick): StudentEditDialog modal edit form (4a.2b-ii)`.

---

### Task 6: Wire the Edit button, double-click, and dialog into `DatabaseScreen`

Add the header **Edit** button (left of Delete), route `LTable.rowActivated` to `beginEdit`, instantiate the dialog, and open/close it from `editReady`/`editFinished`.

**Files:**
- Modify: `qt-app/quick/qml/admin/DatabaseScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (extend the `databaseBand` stub + `DatabaseScreen` TestCase)

**Interfaces:**
- Consumes: `vm.canEdit`, `vm.beginEditSelected()`, `vm.beginEdit(schoolId)`, `vm.editReady()`, `vm.editFinished()`, `StudentEditDialog` (Tasks 2/3/5); `LTable.rowActivated` (Task 4).
- Produces: objectNames `editButton`, `editDialog` on the screen.

- [ ] **Step 1: Extend the stub VM + write the failing screen tests**

In `tst_qml_admin.qml`, extend the `databaseBand` `stubVm` (the `QtObject { id: stubVm … }`, ~line 1647) with the edit surface — add these members alongside the existing ones:

```qml
            property bool canEdit: false
            property string editSchoolId: "2023-001"
            property string editName: "Ann"
            property string editYearLevel: "2"
            property string editGender: "Female"
            property string editStatus: "Active"
            property string editDepartment: "CCS"
            property string editCourse: "BSIT"
            property var editCourses: ["BSIT", "BSCS"]
            property int beginEditSelectedCount: 0
            property string lastBeginEditId: ""
            signal editReady()
            signal editFinished()
            function beginEditSelected() { beginEditSelectedCount++; editReady(); }
            function beginEdit(id) { lastBeginEditId = id; editReady(); }
            function setEditDepartment(d) { editDepartment = d; editCourse = ""; }
            function setEditName(v) { editName = v; }
            function setEditYearLevel(v) { editYearLevel = v; }
            function setEditGender(v) { editGender = v; }
            function setEditStatus(v) { editStatus = v; }
            function setEditCourse(v) { editCourse = v; }
            function saveEdit() {}
```

Add to the `DatabaseScreen` TestCase `init()` (~line 1675) a reset line:

```qml
                stubVm.canEdit = false;
                stubVm.beginEditSelectedCount = 0;
                stubVm.lastBeginEditId = "";
                // Per-test isolation hygiene for the edit state (the prefill
                // guard means open no longer mutates it, but reset anyway so a
                // future test that DOES drive a dept change can't leak into the
                // next).
                stubVm.editDepartment = "CCS";
                stubVm.editCourse = "BSIT";
                var ed = findChild(databaseScreen, "editDialog");
                if (ed) ed.visible = false;
```

Add the new test functions to the `DatabaseScreen` TestCase:

```qml
            function test_editButtonEnabledOnlyWhenCanEdit() {
                var btn = findChild(databaseScreen, "editButton");
                verify(btn !== null);
                compare(btn.enabled, false);          // canEdit false
                stubVm.canEdit = true;
                compare(btn.enabled, true);
            }
            function test_editButtonInvokesBeginEditSelected() {
                stubVm.canEdit = true;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "editButton"));
                compare(stubVm.beginEditSelectedCount, 1);
            }
            function test_rowActivatedInvokesBeginEdit() {
                // Drive the screen's LTable→vm wiring directly (the stub model is
                // not a real row model). Task 4 proves the double-click emits it.
                findChild(databaseScreen, "studentsTable").rowActivated("2023-XYZ");
                compare(stubVm.lastBeginEditId, "2023-XYZ");
            }
            function test_editReadyOpensDialogAndFinishedCloses() {
                var ed = findChild(databaseScreen, "editDialog");
                verify(ed !== null);
                compare(ed.visible, false);
                stubVm.editReady();
                compare(ed.visible, true);
                stubVm.editFinished();
                compare(ed.visible, false);
            }
```

- [ ] **Step 2: Run to verify the new tests fail**

Run: `ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: FAIL — `editButton` / `editDialog` not found; `rowActivated` not wired.

- [ ] **Step 3: Add the Edit button (left of Delete)**

In `qt-app/quick/qml/admin/DatabaseScreen.qml`, insert an Edit `LButton` in the count-header `RowLayout` immediately BEFORE the `deleteMetrics` `TextMetrics` block (i.e., after the Export `LButton` closes, ~line 99):

```qml
            LButton {
                objectName: "editButton"
                variant: "Primary"
                compact: true
                text: qsTr("Edit")
                enabled: screen.vm ? screen.vm.canEdit : false
                tooltipText: qsTr("Select exactly one student to edit")
                accessibleName: qsTr("Edit the selected student")
                onClicked: if (screen.vm) screen.vm.beginEditSelected()
            }
```

- [ ] **Step 4: Wire double-click on the table**

Add an `onRowActivated` handler to the existing `LTable { id: studentsTable … }` (after the `columns: [...]` block, before the LTable's closing brace, ~line 139):

```qml
            onRowActivated: function(schoolId) { if (screen.vm) screen.vm.beginEdit(schoolId); }
```

- [ ] **Step 5: Instantiate the dialog + open/close wiring**

Add, after the `LToast { … }` / status `Connections` block near the end of the screen (~line 186, still inside the root `Rectangle`):

```qml
    StudentEditDialog {
        id: editDialog
        objectName: "editDialog"
        vm: screen.vm
    }

    // beginEdit/beginEditSelected emit editReady only when a record was located;
    // editFinished fires on save success or a no-op. Drive the modal from both.
    Connections {
        target: screen.vm ? screen.vm : null
        function onEditReady() { editDialog.visible = true; }
        function onEditFinished() { editDialog.visible = false; }
    }
```

- [ ] **Step 6: Build and run to verify the tests pass**

Run: `cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_qml_admin --output-on-failure`
Expected: PASS (existing Database cases + the four new ones).

- [ ] **Step 7: Run the full suite**

Run: `ctest --test-dir qt-app/build --output-on-failure`
Expected: PASS — the whole CTest suite green (no regression in delete/export/search/other quick tests).

- [ ] **Step 8: Commit**

Use the `commit` skill. Suggested message: `feat(quick): wire Edit button + double-click + edit dialog into DatabaseScreen (4a.2b-ii)`.

---

## Self-Review

**1. Spec coverage** (each spec section → task):

- Decision (A) classifier rename + `bulkUpdate` 401 rewire → **Task 1**. ✅
- Component §1 (`studentcontroller`) → **Task 1**. ✅
- Component §2 VM: `canEdit`, `edit*` props, `beginEdit`/`beginEditSelected`/`setEditDepartment`/setters, second controller, `onEditCoursesLoaded` → **Task 2**; `saveEdit`, `onBulkUpdateFinished`/`onBulkUpdateFailed`, `editFinished` → **Task 3**. ✅
- Component §3 `StudentEditDialog.qml` incl. combo `selectValue` sync + `onEditCourseChanged` re-sync → **Task 5**. ✅
- Component §4 `DatabaseScreen` Edit button (left of Delete), `onRowActivated` wiring, dialog instance → **Task 6**. ✅
- Component §5 `LTable.rowActivated` double-click, non-conflicting with checkbox → **Task 4**. ✅
- Data Flow (edit) — trigger→prefill→course reload→save→result branches → Tasks 2/3/5/6. ✅
- Error Taxonomy — transport vs 401 vs generic vs empty-Name → Task 1 (classify), Task 3 (auth/generic branch + keep-open), Task 5 (Save disabled on empty Name). ✅
- Refresh Behavior — `reloadTable()` on updated≥1 → Task 3. ✅
- Testing Plan seams — VM (Tasks 2/3), StudentController 401 (Task 1), QuickTest button/double-click/prefill/dept-resync/save/cancel (Tasks 4/5/6). ✅
- Out of Scope — no pencil column, no keyboard row-activation, no bulk edit, no school_id/code/visits/photo edit, no new endpoint: none introduced. ✅

**2. Placeholder scan:** No "TBD/handle edge cases/similar to Task N" — every code + test step carries complete content. ✅

**3. Type consistency:**
- `replyIsServerAnswer(bool,int,QByteArray)` — declared Task 1, used by both delete + bulkUpdate call sites; all old-name references renamed. ✅
- `beginEdit`/`beginEditSelected`/`setEditDepartment`/`editReady` — produced Task 2, consumed Tasks 5/6; `saveEdit`/`editFinished`/`onBulkUpdateFinished(const BulkUpdateResult&)`/`onBulkUpdateFailed(const QString&)` — produced Task 3, consumed Task 6. ✅
- `editCourse`/`editCourses`/`editDepartment` names match across VM (2/3), dialog (5), and stub (5/6). ✅
- `LComboBox.selectValue(v)` + `signal selected(string)` — matches the real component (read in prep). ✅
- `rowActivated(string schoolId)` — Task 4 signal name matches Task 6 `onRowActivated`. ✅
- Status strings `"Student updated"` / `"No changes to save"` — identical between Task 3 impl and its tests. ✅

Consistent. Plan ready for execution.
