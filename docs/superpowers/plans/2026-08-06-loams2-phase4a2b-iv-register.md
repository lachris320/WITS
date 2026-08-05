# LOAMS 2.0 Phase 4a.2b-iv — Register Student + Photo — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "＋ Add Student" registration flow (new-student form + optional photo upload) to the LOAMS 2.0 Database screen, and guard the currently-unauthenticated `register_student.php` backend endpoint.

**Architecture:** Strict MVVM. A new pure parser + multipart method on the shared `StudentController` (witscore); new register form-state + slots on `DatabaseViewModel`; a new `RegisterStudentDialog.qml` mirroring `StudentEditDialog`; and a coordinated backend security fix (add `requireAdminAuth` to `register_student.php` + one-line legacy `adminwindow.cpp` call-site fix so the widgets client keeps working). The register course cascade reuses the existing `m_editController`/`CourseTarget` course-routing, extended with a third `Register` target.

**Tech Stack:** Qt 6.11.1 / C++17 / QML (Qt Quick Controls 2), CMake + Ninja, MinGW kit. QtTest (C++) + Qt Quick Test under CTest. PHP/MySQL backend (XAMPP).

## Global Constraints

- **Strict MVVM:** C++ ViewModels are the ONLY QML-facing layer. QML screens take `property var vm`; QML never calls a `witscore` controller directly.
- **Theming:** `Theme.qml` is the single source of every visual token. ZERO raw hex outside `Theme.qml`; opacity variants use `Qt.alpha(Theme.<token>, a)`.
- **Security — admin key:** RAM-only via `AdminSession::instance().key()`, sent in the POST body, NEVER logged, NEVER rendered in QML.
- **Security — server strings:** any server-supplied string rendered in QML MUST be `Text.PlainText` (cleartext-HTTP injection guard).
- **No real student PII** in fixtures, sample data, or commit messages — synthetic data only.
- **File naming:** QML types + C++ ViewModel/model classes are `PascalCase`; C++ member variables are `m_camelCase`.
- **Tests:** register via `wits_add_qttest()`; add `OFFSCREEN` for any GUI/Quick/network test.
- **Build dir:** use a SHORT build directory (e.g. `C:/b/l42biv`) to dodge the Windows MAX_PATH limit on the QML module autogen dir.
- **Tool paths:** Qt tools are NOT on PATH. Prepend in every PowerShell call:
  `$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH`
- PowerShell `;` does NOT short-circuit — verify the build succeeded before trusting ctest (`if ($?) { ctest ... }`).

## Build & test commands

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S "C:\Users\USER\OneDrive - usep.edu.ph\Documents\WITS\WITS-main\qt-app" -B C:/b/l42biv -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/l42biv
if ($?) { ctest --test-dir C:/b/l42biv --output-on-failure }
```

Run a single test target during a task, e.g.:
```powershell
ctest --test-dir C:/b/l42biv -R tst_studentcontroller --output-on-failure
```

## File Structure

- `qt-app/core/studentdata.h` — **Modify:** add `enum class RegisterOutcome`.
- `qt-app/core/studentcontroller.h` / `.cpp` — **Modify:** add `parseRegisterResponse` (pure), `registerStudent` (multipart POST), and `registerFinished`/`registerFailed` signals.
- `qt-app/tests/tst_studentcontroller.cpp` — **Modify:** add `parseRegisterResponse` unit tests (target already compiles `studentcontroller.cpp`; no CMake change).
- `qt-app/quick/viewmodels/DatabaseViewModel.h` / `.cpp` — **Modify:** add register form-state properties, invokables, slots, `CourseTarget::Register`.
- `qt-app/quick/tests/tst_databaseviewmodel.cpp` — **Modify:** add register-path VM unit tests.
- `qt-app/quick/qml/components/LTextField.qml` — **Modify:** add additive `accepted()` signal + `forceFieldFocus()`/`selectAllText()` passthroughs (the focusable element is the inner `TextInput`).
- `qt-app/quick/qml/admin/RegisterStudentDialog.qml` — **Create:** the registration dialog.
- `qt-app/quick/qml/admin/DatabaseScreen.qml` — **Modify:** add the "＋ Add Student" button, the `RegisterStudentDialog` instance, and the `registerReady`/`registerFinished` Connections.
- `qt-app/quick/CMakeLists.txt` — **Modify:** add `RegisterStudentDialog.qml` to `qt_add_qml_module(... QML_FILES ...)`.
- `qt-app/quick/tests/tst_qml_admin.qml` — **Modify:** extend the Database stub vm with the register surface + add a `RegisterStudentDialog` fixture band and tests; raise the host height.
- `deliverables/loams_api/register_student.php` — **Modify:** insert the `requireAdminAuth` guard.
- `qt-app/adminwindow.cpp` — **Modify:** one-line `addPart("admin_key", m_adminKey)` in the legacy register lambda.

---

### Task 1: StudentController — RegisterOutcome, parseRegisterResponse, registerStudent

**Files:**
- Modify: `qt-app/core/studentdata.h`
- Modify: `qt-app/core/studentcontroller.h`
- Modify: `qt-app/core/studentcontroller.cpp`
- Test: `qt-app/tests/tst_studentcontroller.cpp`

**Interfaces:**
- Produces: `enum class RegisterOutcome { Success, Duplicate, Error };` (in `studentdata.h`).
- Produces: `static RegisterOutcome StudentController::parseRegisterResponse(const QByteArray &raw, QString &outMessage);`
- Produces: `void StudentController::registerStudent(const StudentRecord &rec, const QString &photoFilePath, const QString &adminKey);`
- Produces signals: `void registerFinished(RegisterOutcome outcome, const QString &message);` and `void registerFailed(const QString &errorString);`
- Consumes: existing `StudentController::replyIsServerAnswer` and `ApiConfig::endpoint`.

- [ ] **Step 1: Add the RegisterOutcome enum**

In `qt-app/core/studentdata.h`, after the `SearchOutcome` enum (before `#endif`), add:

```cpp
// Decoded register_student.php outcome. "success" -> Success (row inserted);
// "duplicate" -> Duplicate (school_id already exists — surfaced as an inline
// field error, not a toast); anything else / invalid JSON -> Error (+ message).
enum class RegisterOutcome
{
    Success,
    Duplicate,
    Error
};
```

- [ ] **Step 2: Write the failing parser tests**

In `qt-app/tests/tst_studentcontroller.cpp`, add four slot declarations to the `private slots:` block (near the `parseDeleteResponse` group):

```cpp
    // parseRegisterResponse
    void parseRegisterResponse_success_returnsSuccessEmptyMessage();
    void parseRegisterResponse_duplicate_returnsDuplicate();
    void parseRegisterResponse_error_returnsErrorWithMessage();
    void parseRegisterResponse_invalidJson_returnsErrorWithDefault();
```

And add the four test bodies (near the other `parseDeleteResponse_*` bodies):

```cpp
void TestStudentController::parseRegisterResponse_success_returnsSuccessEmptyMessage()
{
    QString msg = "sentinel";
    const RegisterOutcome outcome = StudentController::parseRegisterResponse(
        R"({"status":"success","message":"Student registered successfully"})", msg);
    QCOMPARE(outcome, RegisterOutcome::Success);
    QVERIFY(msg.isEmpty());   // success carries no surfaced message
}

void TestStudentController::parseRegisterResponse_duplicate_returnsDuplicate()
{
    QString msg;
    const RegisterOutcome outcome = StudentController::parseRegisterResponse(
        R"({"status":"duplicate","message":"Student already exists."})", msg);
    QCOMPARE(outcome, RegisterOutcome::Duplicate);
}

void TestStudentController::parseRegisterResponse_error_returnsErrorWithMessage()
{
    QString msg;
    const RegisterOutcome outcome = StudentController::parseRegisterResponse(
        R"({"status":"error","message":"School ID and Name are required."})", msg);
    QCOMPARE(outcome, RegisterOutcome::Error);
    QCOMPARE(msg, QStringLiteral("School ID and Name are required."));
}

void TestStudentController::parseRegisterResponse_invalidJson_returnsErrorWithDefault()
{
    QString msg;
    const RegisterOutcome outcome = StudentController::parseRegisterResponse("not json", msg);
    QCOMPARE(outcome, RegisterOutcome::Error);
    QCOMPARE(msg, QStringLiteral("Invalid server response."));
}
```

- [ ] **Step 3: Run the tests to verify they fail**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biv --target tst_studentcontroller
```
Expected: **compile FAILS** — `parseRegisterResponse` is not declared, `RegisterOutcome` unknown in this TU. (This is the red state.)

- [ ] **Step 4: Declare the new members on StudentController**

In `qt-app/core/studentcontroller.h`, in the `// Pure, unit-testable statics` block (after `parseDeleteResponse`), add:

```cpp
    // status "success" -> Success; "duplicate" -> Duplicate; else / invalid JSON
    // -> Error with outMessage set (server "message", or "Invalid server
    // response." when the body is not a JSON object). Reuses replyIsServerAnswer
    // at the call site so a guard 401-with-body reaches this parser.
    static RegisterOutcome parseRegisterResponse(const QByteArray &raw, QString &outMessage);
```

After `deleteStudents(...)` (in the async-methods block) add:

```cpp
    // Async — result arrives via registerFinished / registerFailed. Builds a
    // multipart/form-data POST to register_student.php: text parts for
    // code/name/school_id/year_level/course/department/gender/status +
    // admin_key, and an optional `photo` file part when photoFilePath is
    // non-empty. Mirrors ImportController::uploadStudents' ownership pattern.
    // Does NOT send `visits` (endpoint defaults 0 for a new student).
    void registerStudent(const StudentRecord &rec, const QString &photoFilePath,
                         const QString &adminKey);
```

In the `signals:` block (after `deleteFailed`) add:

```cpp
    void registerFinished(RegisterOutcome outcome, const QString &message);
    void registerFailed(const QString &errorString);
```

- [ ] **Step 5: Implement parseRegisterResponse + registerStudent**

In `qt-app/core/studentcontroller.cpp`, add these includes to the existing include block:

```cpp
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
```

After `parseDeleteResponse` (before `toCsv`), add the pure parser:

```cpp
RegisterOutcome StudentController::parseRegisterResponse(const QByteArray &raw, QString &outMessage)
{
    outMessage.clear();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        outMessage = QStringLiteral("Invalid server response.");
        return RegisterOutcome::Error;
    }
    const QJsonObject obj = doc.object();
    const QString status = obj["status"].toString();
    if (status == QLatin1String("success"))
        return RegisterOutcome::Success;
    if (status == QLatin1String("duplicate"))
        return RegisterOutcome::Duplicate;   // inline field error is client-side text
    outMessage = obj["message"].toString();
    return RegisterOutcome::Error;
}
```

At the end of the `// --- Network methods ---` section (after `deleteStudents`), add:

```cpp
void StudentController::registerStudent(const StudentRecord &rec,
                                        const QString &photoFilePath,
                                        const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("register_student.php")));
    // Do NOT set ContentTypeHeader — QHttpMultiPart sets the multipart boundary
    // Content-Type itself (matches the legacy call-site at adminwindow.cpp:742).
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto addText = [multiPart](const QString &name, const QString &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"%1\"").arg(name)));
        part.setBody(value.toUtf8());
        multiPart->append(part);
    };

    addText(QStringLiteral("code"),       rec.code);
    addText(QStringLiteral("name"),       rec.name);
    addText(QStringLiteral("school_id"),  rec.schoolId);
    addText(QStringLiteral("year_level"), rec.yearLevel);
    addText(QStringLiteral("course"),     rec.course);
    addText(QStringLiteral("department"), rec.department);
    addText(QStringLiteral("gender"),     rec.gender);
    addText(QStringLiteral("status"),     rec.status);
    addText(QStringLiteral("admin_key"),  adminKey);   // guard field — never logged

    if (!photoFilePath.isEmpty()) {
        QHttpPart filePart;
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant(QStringLiteral("form-data; name=\"photo\"; filename=\"%1\"")
                                        .arg(QFileInfo(photoFilePath).fileName())));
        QFile *file = new QFile(photoFilePath);
        if (!file->open(QIODevice::ReadOnly)) {
            delete file;
            delete multiPart;                       // never send a half-built request
            emit registerFailed(QStringLiteral("Could not open the photo file."));
            return;
        }
        filePart.setBodyDevice(file);
        file->setParent(multiPart);                 // auto-delete with the multipart
        multiPart->append(filePart);
    }

    QNetworkReply *reply = m_nam->post(request, multiPart);
    multiPart->setParent(reply);                    // auto-delete with the reply

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray resp = reply->readAll();
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QString errorString = reply->errorString();
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (replyIsServerAnswer(hadError, httpStatus, resp)) {
            QString message;
            const RegisterOutcome outcome = parseRegisterResponse(resp, message);
            emit registerFinished(outcome, message);   // 401 body reaches here
        } else {
            emit registerFailed(errorString);          // genuine transport failure only
        }
    });
}
```

- [ ] **Step 6: Run the tests to verify they pass**

```powershell
cmake --build C:/b/l42biv --target tst_studentcontroller
if ($?) { ctest --test-dir C:/b/l42biv -R tst_studentcontroller --output-on-failure }
```
Expected: **PASS** — all four `parseRegisterResponse_*` tests green, existing tests still green.

- [ ] **Step 7: Commit**

Use the `commit` skill. Expected grouping: one commit, e.g. `feat(core): add register_student parser + multipart method to StudentController`.

---

### Task 2: DatabaseViewModel — register form-state, slots, CourseTarget::Register

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h`
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp`
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp`

**Interfaces:**
- Consumes: Task 1's `RegisterOutcome`, `StudentController::registerStudent`, `registerFinished`, `registerFailed`.
- Produces (QML-facing): properties `regSchoolId, regName, regCode, regYearLevel, regGender, regStatus, regDepartment, regCourse, regCourses, regPhotoName, canRegister, regBusy, regDuplicate`; invokables `beginRegister(), setRegSchoolId/Name/Code/YearLevel/Gender/Status/Course(v), setRegDepartment(dept), setRegPhoto(QUrl), clearRegPhoto(), registerStudent()`; signals `registerReady()`, `registerFinished()`.
- Produces (test seam): public slots `onRegisterFinished(RegisterOutcome, QString)`, `onRegisterFailed(QString)`.

- [ ] **Step 1: Write the failing VM tests**

In `qt-app/quick/tests/tst_databaseviewmodel.cpp`, add these slot declarations to the `private slots:` block (after `applyBulkEditGuardsEmptyInvalidAndReentry`):

```cpp
    void canRegisterGatingRequiresSchoolIdAndName();
    void beginRegisterResetsFieldsClearsDuplicateAndEmitsReady();
    void setRegSchoolIdClearsDuplicate();
    void setRegDepartmentRoutesCoursesToRegisterTarget();
    void setRegPhotoAndClearSetsAndClearsPhotoName();
    void onRegisterFinishedSuccessSetsStatusReloadsAndFinishes();
    void onRegisterFinishedDuplicateSetsFlagNoFinishNoReload();
    void onRegisterFinishedAuthFailureSetsAuthStateKeepsOpen();
    void onRegisterFinishedGenericErrorSetsStatusNoAuth();
    void onRegisterFailedSetsTransientStatusClearsBusy();
    void registerStudentReentryGuard();
```

Add the test bodies (after `applyBulkEditGuardsEmptyInvalidAndReentry`, before `QTEST_MAIN`):

```cpp
void TestDatabaseViewModel::canRegisterGatingRequiresSchoolIdAndName()
{
    DatabaseViewModel vm;
    QCOMPARE(vm.canRegister(), false);
    vm.setRegSchoolId("2023-050");
    QCOMPARE(vm.canRegister(), false);                 // name still empty
    vm.setRegName("   ");                              // whitespace-only doesn't count
    QCOMPARE(vm.canRegister(), false);
    vm.setRegName("Ana Reyes");
    QCOMPARE(vm.canRegister(), true);
}

void TestDatabaseViewModel::beginRegisterResetsFieldsClearsDuplicateAndEmitsReady()
{
    DatabaseViewModel vm;
    vm.setRegSchoolId("2023-050"); vm.setRegName("Ana");
    vm.onRegisterFinished(RegisterOutcome::Duplicate, QString());   // set regDuplicate
    QVERIFY(vm.regDuplicate());

    QSignalSpy readySpy(&vm, &DatabaseViewModel::registerReady);
    vm.beginRegister();
    QVERIFY(vm.regSchoolId().isEmpty());
    QVERIFY(vm.regName().isEmpty());
    QVERIFY(!vm.regDuplicate());
    QCOMPARE(vm.canRegister(), false);
    QCOMPARE(readySpy.count(), 1);
}

void TestDatabaseViewModel::setRegSchoolIdClearsDuplicate()
{
    DatabaseViewModel vm;
    vm.onRegisterFinished(RegisterOutcome::Duplicate, QString());
    QVERIFY(vm.regDuplicate());
    vm.setRegSchoolId("2023-051");                     // editing the rejected value clears it
    QVERIFY(!vm.regDuplicate());
}

void TestDatabaseViewModel::setRegDepartmentRoutesCoursesToRegisterTarget()
{
    DatabaseViewModel vm;
    vm.setRegDepartment("CCS");                        // sets target = Register (+ fires a load)
    vm.onEditCoursesLoaded({"BSIT", "BSCS"});
    QCOMPARE(vm.regCourses(), (QStringList{"BSIT", "BSCS"}));
    QVERIFY(vm.editCourses().isEmpty());                // single-edit list untouched
    QVERIFY(vm.bulkCourses().isEmpty());                // bulk list untouched
}

void TestDatabaseViewModel::setRegPhotoAndClearSetsAndClearsPhotoName()
{
    DatabaseViewModel vm;
    vm.setRegPhoto(QUrl::fromLocalFile("C:/tmp/ana_photo.jpg"));
    QCOMPARE(vm.regPhotoName(), QStringLiteral("ana_photo.jpg"));
    vm.clearRegPhoto();
    QVERIFY(vm.regPhotoName().isEmpty());
}

void TestDatabaseViewModel::onRegisterFinishedSuccessSetsStatusReloadsAndFinishes()
{
    DatabaseViewModel vm;
    vm.setRegSchoolId("2023-050"); vm.setRegName("Ana Reyes");
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::registerFinished);
    vm.onRegisterFinished(RegisterOutcome::Success, QStringLiteral("Student registered successfully"));
    QCOMPARE(vm.statusMessage(), QStringLiteral("Registered Ana Reyes"));
    QVERIFY(!vm.authFailure());
    QVERIFY(vm.loading());                              // reloadTable() flipped loading on
    QCOMPARE(finishedSpy.count(), 1);
}

void TestDatabaseViewModel::onRegisterFinishedDuplicateSetsFlagNoFinishNoReload()
{
    DatabaseViewModel vm;
    vm.setRegSchoolId("2023-050"); vm.setRegName("Ana");
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::registerFinished);
    vm.onRegisterFinished(RegisterOutcome::Duplicate, QString());
    QVERIFY(vm.regDuplicate());
    QCOMPARE(finishedSpy.count(), 0);                   // dialog stays open
    QVERIFY(!vm.loading());                             // no reload on a duplicate
}

void TestDatabaseViewModel::onRegisterFinishedAuthFailureSetsAuthStateKeepsOpen()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::registerFinished);
    vm.onRegisterFinished(RegisterOutcome::Error, QStringLiteral("Invalid admin key"));
    QVERIFY(vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(finishedSpy.count(), 0);
}

void TestDatabaseViewModel::onRegisterFinishedGenericErrorSetsStatusNoAuth()
{
    DatabaseViewModel vm;
    vm.onRegisterFinished(RegisterOutcome::Error, QStringLiteral("School ID and Name are required."));
    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.statusMessage(), QStringLiteral("School ID and Name are required."));
}

void TestDatabaseViewModel::onRegisterFailedSetsTransientStatusClearsBusy()
{
    DatabaseViewModel vm;
    vm.onRegisterFailed(QStringLiteral("Connection refused"));
    QVERIFY(!vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(vm.regBusy(), false);
}

void TestDatabaseViewModel::registerStudentReentryGuard()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    // No School ID/Name -> canRegister false -> no-op, never goes busy.
    vm.registerStudent();
    QCOMPARE(vm.regBusy(), false);

    vm.setRegSchoolId("2023-050"); vm.setRegName("Ana");
    vm.registerStudent();                               // posts via the VM's own NAM
    QCOMPARE(vm.regBusy(), true);
    vm.registerStudent();                               // second call is a no-op
    QCOMPARE(vm.regBusy(), true);
    AdminSession::instance().clear();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```powershell
cmake --build C:/b/l42biv --target tst_databaseviewmodel
```
Expected: **compile FAILS** — the `reg*` methods/slots don't exist yet.

- [ ] **Step 3: Declare the register surface on DatabaseViewModel**

In `qt-app/quick/viewmodels/DatabaseViewModel.h`, add these `Q_PROPERTY` lines inside the class (after the `bulkBusy` property, before `public:`):

```cpp
    Q_PROPERTY(QString regSchoolId READ regSchoolId WRITE setRegSchoolId NOTIFY regSchoolIdChanged)
    Q_PROPERTY(QString regName READ regName WRITE setRegName NOTIFY regNameChanged)
    Q_PROPERTY(QString regCode READ regCode WRITE setRegCode NOTIFY regCodeChanged)
    Q_PROPERTY(QString regYearLevel READ regYearLevel WRITE setRegYearLevel NOTIFY regYearLevelChanged)
    Q_PROPERTY(QString regGender READ regGender WRITE setRegGender NOTIFY regGenderChanged)
    Q_PROPERTY(QString regStatus READ regStatus WRITE setRegStatus NOTIFY regStatusChanged)
    Q_PROPERTY(QString regDepartment READ regDepartment NOTIFY regDepartmentChanged)
    Q_PROPERTY(QString regCourse READ regCourse WRITE setRegCourse NOTIFY regCourseChanged)
    Q_PROPERTY(QStringList regCourses READ regCourses NOTIFY regCoursesChanged)
    Q_PROPERTY(QString regPhotoName READ regPhotoName NOTIFY regPhotoNameChanged)
    Q_PROPERTY(bool canRegister READ canRegister NOTIFY canRegisterChanged)
    Q_PROPERTY(bool regBusy READ regBusy NOTIFY regBusyChanged)
    Q_PROPERTY(bool regDuplicate READ regDuplicate NOTIFY regDuplicateChanged)
```

Change the `CourseTarget` enum to add `Register`:

```cpp
    enum class CourseTarget { SingleEdit, BulkEdit, Register };
```

Add the getters (after `bool bulkBusy() const { return m_bulkInFlight; }`):

```cpp
    QString regSchoolId() const { return m_regSchoolId; }
    QString regName() const { return m_regName; }
    QString regCode() const { return m_regCode; }
    QString regYearLevel() const { return m_regYearLevel; }
    QString regGender() const { return m_regGender; }
    QString regStatus() const { return m_regStatus; }
    QString regDepartment() const { return m_regDepartment; }
    QString regCourse() const { return m_regCourse; }
    QStringList regCourses() const { return m_regCourses; }
    QString regPhotoName() const { return m_regPhotoName; }
    bool canRegister() const
    { return !m_regSchoolId.trimmed().isEmpty() && !m_regName.trimmed().isEmpty(); }
    bool regBusy() const { return m_regInFlight; }
    bool regDuplicate() const { return m_regDuplicate; }
```

Add the invokables (after `Q_INVOKABLE void applyBulkEdit();`):

```cpp
    Q_INVOKABLE void beginRegister();
    Q_INVOKABLE void setRegSchoolId(const QString &v);
    Q_INVOKABLE void setRegName(const QString &v);
    Q_INVOKABLE void setRegCode(const QString &v);
    Q_INVOKABLE void setRegYearLevel(const QString &v);
    Q_INVOKABLE void setRegGender(const QString &v);
    Q_INVOKABLE void setRegStatus(const QString &v);
    Q_INVOKABLE void setRegCourse(const QString &v);
    Q_INVOKABLE void setRegDepartment(const QString &dept);
    Q_INVOKABLE void setRegPhoto(const QUrl &fileUrl);
    Q_INVOKABLE void clearRegPhoto();
    Q_INVOKABLE void registerStudent();
```

Add the public slots (after `void onBulkUpdateFailed(const QString &errorString);`):

```cpp
    void onRegisterFinished(RegisterOutcome outcome, const QString &message);
    void onRegisterFailed(const QString &errorString);
```

Add the signals (in `signals:`, after `bulkBusyChanged();`):

```cpp
    void regSchoolIdChanged();
    void regNameChanged();
    void regCodeChanged();
    void regYearLevelChanged();
    void regGenderChanged();
    void regStatusChanged();
    void regDepartmentChanged();
    void regCourseChanged();
    void regCoursesChanged();
    void regPhotoNameChanged();
    void canRegisterChanged();
    void regBusyChanged();
    void regDuplicateChanged();
    void registerReady();
    void registerFinished();
```

Add the private members (after the `bool m_bulkInFlight = false;` line):

```cpp
    QString m_regSchoolId, m_regName, m_regCode, m_regYearLevel, m_regGender, m_regStatus;
    QString m_regDepartment, m_regCourse, m_regPhotoName, m_regPhotoPath;
    QStringList m_regCourses;
    bool m_regInFlight = false;
    bool m_regDuplicate = false;
```

- [ ] **Step 4: Wire the controller signals + implement the register methods**

In `qt-app/quick/viewmodels/DatabaseViewModel.cpp`, in the constructor (after the `bulkUpdateFailed` connect at the end of the ctor body), add:

```cpp
    connect(m_controller, &StudentController::registerFinished,
            this, &DatabaseViewModel::onRegisterFinished);
    connect(m_controller, &StudentController::registerFailed,
            this, &DatabaseViewModel::onRegisterFailed);
```

Replace `onEditCoursesLoaded` with the 3-way routed version:

```cpp
void DatabaseViewModel::onEditCoursesLoaded(const QStringList &courses)
{
    if (m_courseTarget == CourseTarget::Register) {
        m_regCourses = courses; emit regCoursesChanged();
    } else if (m_courseTarget == CourseTarget::BulkEdit) {
        m_bulkCourses = courses; emit bulkCoursesChanged();
    } else {
        m_editCourses = courses; emit editCoursesChanged();
    }
}
```

At the end of the file (after `setAuthFailure`), add the register implementation:

```cpp
void DatabaseViewModel::beginRegister()
{
    m_regSchoolId.clear();  emit regSchoolIdChanged();
    m_regName.clear();      emit regNameChanged();
    m_regCode.clear();      emit regCodeChanged();
    m_regYearLevel.clear(); emit regYearLevelChanged();
    m_regGender.clear();    emit regGenderChanged();
    m_regStatus.clear();    emit regStatusChanged();
    m_regDepartment.clear();emit regDepartmentChanged();
    m_regCourse.clear();    emit regCourseChanged();
    m_regCourses.clear();   emit regCoursesChanged();
    m_regPhotoPath.clear();
    m_regPhotoName.clear(); emit regPhotoNameChanged();
    if (m_regDuplicate) { m_regDuplicate = false; emit regDuplicateChanged(); }
    emit canRegisterChanged();
    emit registerReady();
}

void DatabaseViewModel::setRegSchoolId(const QString &v)
{
    if (m_regSchoolId == v) return;
    m_regSchoolId = v; emit regSchoolIdChanged();
    // Editing the rejected value makes the "already exists" claim stale.
    if (m_regDuplicate) { m_regDuplicate = false; emit regDuplicateChanged(); }
    emit canRegisterChanged();
}

void DatabaseViewModel::setRegName(const QString &v)
{
    if (m_regName == v) return;
    m_regName = v; emit regNameChanged();
    emit canRegisterChanged();
}

void DatabaseViewModel::setRegCode(const QString &v)
{ if (m_regCode != v) { m_regCode = v; emit regCodeChanged(); } }

void DatabaseViewModel::setRegYearLevel(const QString &v)
{ if (m_regYearLevel != v) { m_regYearLevel = v; emit regYearLevelChanged(); } }

void DatabaseViewModel::setRegGender(const QString &v)
{ if (m_regGender != v) { m_regGender = v; emit regGenderChanged(); } }

void DatabaseViewModel::setRegStatus(const QString &v)
{ if (m_regStatus != v) { m_regStatus = v; emit regStatusChanged(); } }

void DatabaseViewModel::setRegCourse(const QString &v)
{ if (m_regCourse != v) { m_regCourse = v; emit regCourseChanged(); } }

void DatabaseViewModel::setRegDepartment(const QString &dept)
{
    // Guard makes a re-entrant setRegDepartment("") from the dialog's on-open
    // combo reset a hard no-op (m_regDepartment is already "" after
    // beginRegister), so reopening never fires a spurious empty-dept load.
    if (m_regDepartment == dept) return;
    m_regDepartment = dept; emit regDepartmentChanged();
    if (!m_regCourse.isEmpty()) { m_regCourse.clear(); emit regCourseChanged(); }
    m_courseTarget = CourseTarget::Register;
    m_editController->loadCourses(dept);   // dependent course list for the register dialog
}

void DatabaseViewModel::setRegPhoto(const QUrl &fileUrl)
{
    m_regPhotoPath = fileUrl.toLocalFile();
    m_regPhotoName = QFileInfo(m_regPhotoPath).fileName();
    emit regPhotoNameChanged();
}

void DatabaseViewModel::clearRegPhoto()
{
    if (m_regPhotoPath.isEmpty() && m_regPhotoName.isEmpty()) return;
    m_regPhotoPath.clear();
    m_regPhotoName.clear();
    emit regPhotoNameChanged();
}

void DatabaseViewModel::registerStudent()
{
    if (m_regInFlight || !canRegister()) return;    // re-entry + precondition guard
    StudentRecord rec;
    rec.schoolId   = m_regSchoolId;
    rec.name       = m_regName;
    rec.code       = m_regCode;
    rec.yearLevel  = m_regYearLevel;
    rec.department = m_regDepartment;
    rec.course     = m_regCourse;
    rec.gender     = m_regGender;
    rec.status     = m_regStatus;
    rec.visits     = 0;                              // new student has no visits
    m_regInFlight = true; emit regBusyChanged();
    m_controller->registerStudent(rec, m_regPhotoPath, AdminSession::instance().key());
}

void DatabaseViewModel::onRegisterFinished(RegisterOutcome outcome, const QString &message)
{
    m_regInFlight = false; emit regBusyChanged();
    switch (outcome) {
    case RegisterOutcome::Success:
        setAuthFailure(false);
        setStatusMessage(m_regName.isEmpty() ? tr("Student registered")
                                             : tr("Registered %1").arg(m_regName));
        reloadTable();                              // re-fetch the current dept/course filter
        emit registerFinished();                    // closes the dialog
        break;
    case RegisterOutcome::Duplicate:
        if (!m_regDuplicate) { m_regDuplicate = true; emit regDuplicateChanged(); }
        break;                                      // inline error; no toast, no reload
    case RegisterOutcome::Error:
        // 401 held-key vs generic — the SAME split delete/bulk use. Dialog stays open.
        applyServerRejection(message, tr("Registration failed."));
        break;
    }
}

void DatabaseViewModel::onRegisterFailed(const QString & /*errorString*/)
{
    m_regInFlight = false; emit regBusyChanged();
    setAuthFailure(false);
    setStatusMessage(tr("Registration failed — check your connection."));
}
```

- [ ] **Step 5: Run the tests to verify they pass**

```powershell
cmake --build C:/b/l42biv --target tst_databaseviewmodel
if ($?) { ctest --test-dir C:/b/l42biv -R tst_databaseviewmodel --output-on-failure }
```
Expected: **PASS** — all new `reg*` tests green; the existing `courseTargetRoutesBulkVsSingle` and all others still green.

- [ ] **Step 6: Commit**

Use the `commit` skill. Expected: `feat(database): add register form-state + slots to DatabaseViewModel`.

---

### Task 3: RegisterStudentDialog QML + DatabaseScreen wiring + QuickTests

**Files:**
- Modify: `qt-app/quick/qml/components/LTextField.qml`
- Create: `qt-app/quick/qml/admin/RegisterStudentDialog.qml`
- Modify: `qt-app/quick/qml/admin/DatabaseScreen.qml`
- Modify: `qt-app/quick/CMakeLists.txt`
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: Task 2's `DatabaseViewModel` register surface (`beginRegister`, `regSchoolId`, `setRegSchoolId`, `canRegister`, `regBusy`, `regDuplicate`, `regDepartment`, `setRegDepartment`, `regCourse`, `regCourses`, `regPhotoName`, `setRegPhoto`, `clearRegPhoto`, `registerStudent`, `registerReady`, `registerFinished`).
- Produces: `RegisterStudentDialog` QML type (URI `LOAMS`); `LTextField` gains `signal accepted()`, `function forceFieldFocus()`, `function selectAllText()`.

- [ ] **Step 1: Add the additive LTextField passthroughs**

In `qt-app/quick/qml/components/LTextField.qml`, add to the root `ColumnLayout` property block (after `property bool isPassword: false`):

```qml
    // Emitted when the user presses Return/Enter in the field (forwarded from
    // the inner TextInput). Lets a dialog wire Enter-to-submit without reaching
    // into the private TextInput.
    signal accepted()
    // Focus/selection passthroughs — the focusable element is the inner
    // TextInput, not this ColumnLayout, so a consumer can't forceActiveFocus()
    // the field directly. Additive; no behavior change for existing callers.
    function forceFieldFocus() { input.forceActiveFocus(); }
    function selectAllText() { input.selectAll(); }
```

In the inner `TextInput { id: input ... }`, add one line inside the block (e.g. after `echoMode: ...`):

```qml
            onAccepted: root.accepted()
```

- [ ] **Step 2: Create RegisterStudentDialog.qml**

Create `qt-app/quick/qml/admin/RegisterStudentDialog.qml`:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import LOAMS

// New-student registration form (Phase 4a.2b-iv). An LDialog-based modal driven
// by plain `visible`. Takes `property var vm` (a DatabaseViewModel, or a
// plain-QML stub in QuickTests). A FRESH form — no prefill — so it carries no
// prefill-guard machinery, only the narrow dept->course re-sync (LComboBox
// severs its own binding on pick). Deliberately parallel to StudentEditDialog so
// a later shared-base extraction is mechanical.
LDialog {
    id: root
    property var vm
    title: qsTr("Register student")

    // `resetting` guards the on-open control reset so pushing placeholder state
    // into the combos/fields doesn't re-enter the vm setters. (selectValue("")
    // EMITS selected(""), and text="" fires onTextChanged.)
    property bool resetting: false

    // Re-sync the Course combo whenever the vm clears/changes regCourse (a real
    // department change sets it to ""); and on a duplicate result, refocus the
    // School ID field and select its text so the fix is one keystroke away.
    Connections {
        target: root.vm ? root.vm : null
        function onRegCourseChanged() { courseCombo.selectValue(root.vm.regCourse); }
        function onRegDuplicateChanged() {
            if (root.vm.regDuplicate) {
                schoolIdField.forceFieldFocus();
                schoolIdField.selectAllText();
            }
        }
    }

    onVisibleChanged: if (visible && root.vm) {
        // Reset the CONTROLS to match the vm's already-clean state (beginRegister
        // ran before this opened). resetting=true so none of these push to the vm.
        root.resetting = true;
        schoolIdField.text = "";
        nameField.text = "";
        codeField.text = "";
        yearField.text = "";
        deptCombo.selectValue("");
        courseCombo.selectValue("");
        genderCombo.selectValue("");
        statusCombo.selectValue("");
        root.resetting = false;
        schoolIdField.forceFieldFocus();   // autofocus School ID (also the wedge-scan target)
    }

    // Enter-to-submit, guarded so a bare School-ID scan (Name still blank ->
    // canRegister false) can't half-submit.
    function trySubmit() {
        if (root.vm && root.vm.canRegister && !root.vm.regBusy)
            root.vm.registerStudent();
    }
    // Esc-to-cancel (blocked mid-request). The scrim is non-dismissing (LDialog),
    // so an accidental click never discards a filled form.
    Keys.onEscapePressed: if (root.vm && !root.vm.regBusy) root.visible = false;

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        LTextField {
            id: schoolIdField
            objectName: "regSchoolIdField"
            Layout.fillWidth: true
            label: qsTr("School ID *")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegSchoolId(text)
            onAccepted: root.trySubmit()
        }
        Text {
            objectName: "regDuplicateError"
            visible: root.vm ? root.vm.regDuplicate : false
            text: qsTr("This School ID already exists.")
            textFormat: Text.PlainText
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        LTextField {
            id: nameField
            objectName: "regNameField"
            Layout.fillWidth: true
            label: qsTr("Name *")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegName(text)
            onAccepted: root.trySubmit()
        }

        LTextField {
            id: codeField
            objectName: "regCodeField"
            Layout.fillWidth: true
            label: qsTr("Code")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegCode(text)
            onAccepted: root.trySubmit()
        }

        LComboBox {
            id: deptCombo
            objectName: "regDeptCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.departments : []
            placeholder: qsTr("Department")
            onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegDepartment(value); }
        }
        LComboBox {
            id: courseCombo
            objectName: "regCourseCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.regCourses : []
            placeholder: qsTr("Course")
            onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegCourse(value); }
        }

        LTextField {
            id: yearField
            objectName: "regYearField"
            Layout.fillWidth: true
            label: qsTr("Year Level")
            placeholder: qsTr("e.g. 1, 2, 3, 4")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegYearLevel(text)
            onAccepted: root.trySubmit()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LComboBox {
                id: genderCombo
                objectName: "regGenderCombo"
                Layout.fillWidth: true
                model: ["Male", "Female"]
                placeholder: qsTr("Gender")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegGender(value); }
            }
            LComboBox {
                id: statusCombo
                objectName: "regStatusCombo"
                Layout.fillWidth: true
                model: ["Active", "Inactive"]
                placeholder: qsTr("Status")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegStatus(value); }
            }
        }

        // Photo (optional) — no preview; only the picked filename + constraints.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "regChoosePhotoButton"
                variant: "Outline"
                compact: true
                text: (root.vm && root.vm.regPhotoName.length > 0)
                      ? qsTr("Change photo…") : qsTr("Choose photo…")
                onClicked: photoDialog.open()
            }
            Text {
                objectName: "regPhotoLabel"
                Layout.fillWidth: true
                text: (root.vm && root.vm.regPhotoName.length > 0)
                      ? root.vm.regPhotoName
                      : qsTr("No photo selected — JPG, PNG, or GIF, up to 5MB")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: (root.vm && root.vm.regPhotoName.length > 0) ? Theme.text : Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            LButton {
                objectName: "regRemovePhotoButton"
                variant: "Ghost"
                compact: true
                text: qsTr("Remove")
                visible: root.vm ? root.vm.regPhotoName.length > 0 : false
                onClicked: if (root.vm) root.vm.clearRegPhoto()
            }
        }

        Text {
            objectName: "regRequiredCaption"
            text: qsTr("* required")
            textFormat: Text.PlainText
            color: Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "regCancelButton"
                variant: "Outline"
                text: qsTr("Cancel")
                enabled: root.vm ? !root.vm.regBusy : true
                onClicked: root.visible = false
            }
            LButton {
                objectName: "regSubmitButton"
                text: (root.vm && root.vm.regBusy) ? qsTr("Registering…") : qsTr("Register")
                enabled: root.vm ? (root.vm.canRegister && !root.vm.regBusy) : false
                onClicked: root.trySubmit()
            }
        }
    }

    FileDialog {
        id: photoDialog
        objectName: "regPhotoDialog"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.gif)"]
        onAccepted: if (root.vm) root.vm.setRegPhoto(selectedFile)
    }
}
```

- [ ] **Step 3: Register the new QML file in CMake**

In `qt-app/quick/CMakeLists.txt`, in `qt_add_qml_module(witsquickmodule ... QML_FILES ...)`, add after `qml/admin/StudentEditDialog.qml`:

```cmake
        qml/admin/RegisterStudentDialog.qml
```

- [ ] **Step 4: Wire DatabaseScreen — button, dialog, connections**

In `qt-app/quick/qml/admin/DatabaseScreen.qml`, add the "＋ Add Student" button inside the actions `RowLayout`, immediately after the `Item { Layout.fillWidth: true }` spacer (so it sits at the left of the right-aligned action group, before Export):

```qml
            LButton {
                objectName: "addStudentButton"
                variant: "Outline"
                compact: true
                text: qsTr("＋ Add Student")
                // Create is independent of selection; only disabled while loading.
                enabled: screen.vm ? !screen.vm.loading : false
                onClicked: if (screen.vm) screen.vm.beginRegister()
            }
```

Add the dialog instance after the `BulkEditDialog { ... }` block:

```qml
    RegisterStudentDialog {
        id: registerDialog
        objectName: "registerDialog"
        vm: screen.vm
    }
```

Extend the edit/bulk `Connections` block (the one with `onEditReady`/`onBulkEditFinished`) with the register handlers:

```qml
        function onRegisterReady() { registerDialog.visible = true; }
        function onRegisterFinished() {
            registerDialog.visible = false;
            studentsTable.forceActiveFocus();   // keep working the list after a register
        }
```

- [ ] **Step 5: Extend the Database stub vm + add the RegisterStudentDialog fixture (failing tests)**

In `qt-app/quick/tests/tst_qml_admin.qml`:

(a) Raise the host height and extend the geometry ledger comment. Change `width: 1100; height: 5900` to `width: 1100; height: 6600`, and in the ledger comment add `| registerDialog 5900..6600`.

(b) In the Database `stubVm` (id: stubVm, around line 1648), add the register surface after the bulk surface (before the closing `}` of stubVm):

```qml
            // Register surface the hosted RegisterStudentDialog binds to.
            property string regSchoolId: ""
            property string regName: ""
            property string regCode: ""
            property string regYearLevel: ""
            property string regGender: ""
            property string regStatus: ""
            property string regDepartment: ""
            property string regCourse: ""
            property var regCourses: []
            property string regPhotoName: ""
            property bool canRegister: false
            property bool regBusy: false
            property bool regDuplicate: false
            property int beginRegisterCount: 0
            property int registerStudentCount: 0
            signal registerReady()
            signal registerFinished()
            function beginRegister() { beginRegisterCount++; registerReady(); }
            function setRegSchoolId(v) { regSchoolId = v; regDuplicate = false; }
            function setRegName(v) { regName = v; }
            function setRegCode(v) { regCode = v; }
            function setRegYearLevel(v) { regYearLevel = v; }
            function setRegGender(v) { regGender = v; }
            function setRegStatus(v) { regStatus = v; }
            function setRegCourse(v) { regCourse = v; }
            function setRegDepartment(v) { regDepartment = v; regCourse = ""; }
            function setRegPhoto(u) { regPhotoName = ("" + u).split("/").pop(); }
            function clearRegPhoto() { regPhotoName = ""; }
            function registerStudent() { registerStudentCount++; }
```

(c) Add a screen-level test to the `DatabaseScreen` TestCase (button opens the dialog):

```qml
            function test_addStudentButtonInvokesBeginRegisterAndOpensDialog() {
                var btn = findChild(databaseScreen, "addStudentButton");
                verify(btn !== null);
                waitForRendering(databaseScreen);
                mouseClick(btn);
                compare(stubVm.beginRegisterCount, 1);
                var d = findChild(databaseScreen, "registerDialog");
                verify(d !== null);
                compare(d.visible, true);
                stubVm.registerFinished();
                compare(d.visible, false);
            }
```

Also add `stubVm.beginRegisterCount = 0; stubVm.registerStudentCount = 0; var rd = findChild(databaseScreen, "registerDialog"); if (rd) rd.visible = false;` to the `DatabaseScreen` TestCase `init()`.

(d) Add a new fixture band + TestCase at the end of the root `Item` (after the `BulkEditDialog` band's closing `}`, before `Component { id: signalSpy; SignalSpy {} }`):

```qml
    // --- RegisterStudentDialog fixture (own band below bulkEdit, y 5900..6600) ---
    Item {
        id: registerBand
        y: 5900
        width: 900; height: 700

        QtObject {
            id: regStub
            property var departments: ["CCS", "CBA"]
            property var regCourses: ["BSIT", "BSCS"]
            property string regSchoolId: ""
            property string regName: ""
            property string regCode: ""
            property string regYearLevel: ""
            property string regGender: ""
            property string regStatus: ""
            property string regDepartment: ""
            property string regCourse: ""
            property string regPhotoName: ""
            property bool canRegister: false
            property bool regBusy: false
            property bool regDuplicate: false
            property int setRegDeptCount: 0
            property string lastRegDept: ""
            property int registerStudentCount: 0
            function setRegSchoolId(v) { regSchoolId = v; regDuplicate = false; }
            function setRegName(v) { regName = v; }
            function setRegCode(v) { regCode = v; }
            function setRegYearLevel(v) { regYearLevel = v; }
            function setRegGender(v) { regGender = v; }
            function setRegStatus(v) { regStatus = v; }
            function setRegCourse(v) { regCourse = v; }
            function setRegDepartment(v) { setRegDeptCount++; lastRegDept = v; regDepartment = v; regCourse = ""; }
            function setRegPhoto(u) { regPhotoName = ("" + u).split("/").pop(); }
            function clearRegPhoto() { regPhotoName = ""; }
            function registerStudent() { registerStudentCount++; }
        }

        RegisterStudentDialog { id: registerDialog2; anchors.fill: parent; vm: regStub }

        TestCase {
            name: "RegisterStudentDialog"; when: windowShown
            function init() {
                regStub.regSchoolId = ""; regStub.regName = ""; regStub.regCourse = "";
                regStub.regDepartment = ""; regStub.regPhotoName = "";
                regStub.canRegister = false; regStub.regBusy = false; regStub.regDuplicate = false;
                regStub.setRegDeptCount = 0; regStub.lastRegDept = "";
                regStub.registerStudentCount = 0;
                registerDialog2.visible = false;
            }
            function test_submitDisabledUntilCanRegister() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var submit = findChild(registerDialog2, "regSubmitButton");
                verify(submit !== null);
                compare(submit.enabled, false);
                regStub.canRegister = true;
                compare(submit.enabled, true);
            }
            function test_submitLabelSwapsWhenBusy() {
                regStub.canRegister = true;
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var submit = findChild(registerDialog2, "regSubmitButton");
                compare(submit.text, "Register");
                regStub.regBusy = true;
                compare(submit.text, "Registering…");
                compare(submit.enabled, false);   // disabled while busy
            }
            function test_submitInvokesVmRegisterStudent() {
                regStub.canRegister = true;
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                mouseClick(findChild(registerDialog2, "regSubmitButton"));
                compare(regStub.registerStudentCount, 1);
            }
            function test_duplicateErrorVisibleAndClearsOnSchoolIdEdit() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var err = findChild(registerDialog2, "regDuplicateError");
                verify(err !== null);
                compare(err.visible, false);
                regStub.regDuplicate = true;
                compare(err.visible, true);
                // Editing School ID clears the duplicate (stub setter drops the flag).
                var idField = findChild(registerDialog2, "regSchoolIdField");
                idField.text = "2023-999";
                compare(err.visible, false);
            }
            function test_departmentPickDrivesSetRegDepartment() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                findChild(registerDialog2, "regDeptCombo").selectValue("CBA");
                compare(regStub.setRegDeptCount, 1);
                compare(regStub.lastRegDept, "CBA");
                // vm cleared regCourse; the re-sync Connections resets the combo.
                compare(findChild(registerDialog2, "regCourseCombo").currentValue, "");
            }
            function test_photoPickShowsFilenameAndRemoveClears() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var label = findChild(registerDialog2, "regPhotoLabel");
                var remove = findChild(registerDialog2, "regRemovePhotoButton");
                compare(remove.visible, false);
                regStub.setRegPhoto("file:///tmp/ana_reyes.png");
                compare(label.text, "ana_reyes.png");
                compare(remove.visible, true);
                mouseClick(remove);
                compare(regStub.regPhotoName, "");
            }
            function test_cancelClosesDialog() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                mouseClick(findChild(registerDialog2, "regCancelButton"));
                compare(registerDialog2.visible, false);
            }
        }
    }
```

- [ ] **Step 6: Run the QuickTest to verify it fails, then passes**

```powershell
cmake --build C:/b/l42biv --target tst_qml_admin
if ($?) { ctest --test-dir C:/b/l42biv -R tst_qml_admin --output-on-failure }
```
Expected: with Steps 1–4 already in place the new tests should PASS. If you sequence the test-first (write Step 5 before the dialog), expect the `RegisterStudentDialog` type to be unresolved (red) until Step 2/3 land. Ensure the whole `tst_qml_admin` suite is green, including the pre-existing Database/edit/bulk tests (the raised host height must not overlap any band).

- [ ] **Step 7: Full build + full ctest**

```powershell
cmake --build C:/b/l42biv
if ($?) { ctest --test-dir C:/b/l42biv --output-on-failure }
```
Expected: clean build (no new warnings) and the ENTIRE suite green.

- [ ] **Step 8: Commit**

Use the `commit` skill. Expected: `feat(database): add RegisterStudentDialog + Add Student flow (LOAMS 2.0 Phase 4a.2b-iv)`.

---

### Task 4: Backend guard + legacy call-site fix + deploy verification

**Files:**
- Modify: `deliverables/loams_api/register_student.php`
- Modify: `qt-app/adminwindow.cpp`

**Interfaces:**
- Consumes: existing `auth_helper.php` `requireAdminAuth($conn)`; existing `m_adminKey` member (`adminwindow.h:85`, set via `setAdminKey`).

- [ ] **Step 1: Add the admin-key guard to register_student.php**

In `deliverables/loams_api/register_student.php`, insert two lines immediately after `include "db.php";` (line 5), so the endpoint 401s before any dup check, file write, or INSERT:

```php
include "auth_helper.php";
requireAdminAuth($conn);   // 401 "Admin authentication required" before anything
```

- [ ] **Step 2: Fix the legacy widgets call-site to send admin_key**

In `qt-app/adminwindow.cpp`, in the register lambda's `addPart(...)` block (after `addPart("status", ui->statusComboBox->currentText());` at line 717), add:

```cpp
        addPart("admin_key", m_adminKey);   // guarded endpoint (4a.2b-iv); m_adminKey held via setAdminKey
```

- [ ] **Step 3: Build both executables**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/l42biv
```
Expected: clean build of both `WITS` (legacy widgets) and `WITSQuick` (LOAMS 2.0).

- [ ] **Step 4: Deploy the backend + negative/positive auth verify**

Copy the updated `register_student.php` to the XAMPP `loams_api` webroot (same deploy the 4a.1 guard used). Then verify from a shell:

```powershell
# Negative — no admin_key must 401.
curl.exe -s -o - -w "`nHTTP %{http_code}`n" -X POST "http://localhost/loams_api/register_student.php" -F "school_id=TEST-NEG" -F "name=Neg Test"
# Expected: HTTP 401, body {"status":"error","message":"Admin authentication required"}

# Positive — with the correct admin_key a NEW id inserts.
curl.exe -s -o - -w "`nHTTP %{http_code}`n" -X POST "http://localhost/loams_api/register_student.php" -F "school_id=TEST-POS-<unique>" -F "name=Pos Test" -F "admin_key=<REAL_ADMIN_KEY>"
# Expected: HTTP 200, body {"status":"success",...}
```
(Use a synthetic id you can delete afterward; do not commit the real admin key anywhere.)

- [ ] **Step 5: GUI smoke — both clients**

- **WITSQuick (LOAMS 2.0):** log in as admin, open Database, click **＋ Add Student**. Register a new student **without** a photo → toast "Registered <name>", table refreshes, focus returns to the table. Register another **with** a JPG/PNG photo → success + the photo lands in the backend `uploads/`. Try a **duplicate** School ID → inline "This School ID already exists." under the field, dialog stays open, School ID refocused. Confirm the **generic-error toast renders ABOVE the register scrim** (the z-order caveat in the spec) — if it is occluded, add an in-dialog error `Text` for the generic-error case before closing the slice.
- **WITS (legacy widgets):** run `WITS.exe`, log in, register a student on the legacy form → it now sends `admin_key` and still succeeds (no 401 regression).

- [ ] **Step 6: Commit**

Use the `commit` skill. Expected: `feat(api): guard register_student.php + fix legacy admin_key call-site`. Do NOT include the real admin key or any real student data in the message.

---

## Self-Review

**Spec coverage** (each spec section → task):
- Backend guard + placement → Task 4 Step 1. Legacy call-site fix → Task 4 Step 2. Breaking-change deploy + neg/pos verify → Task 4 Steps 4–5.
- `StudentController::registerStudent` multipart + `parseRegisterResponse` + `RegisterOutcome` + signals → Task 1.
- `DatabaseViewModel` reg* properties/invokables/slots + `CourseTarget::Register` routing → Task 2.
- `RegisterStudentDialog` (field order, required markers, Gender+Status paired, photo Choose/Change/Remove, "Registering…" busy, autofocus/refocus-on-duplicate/restore-table-focus, Enter-to-submit, Esc-to-cancel, inline duplicate error) → Task 3 Step 2 + DatabaseScreen wiring Step 4.
- Error taxonomy (success toast + reload + close; duplicate inline; generic/auth via shared toast; network toast) → Task 2 `onRegisterFinished`/`onRegisterFailed` + Task 4 GUI smoke for the z-order caveat.
- LTextField `accepted()`/focus passthroughs (needed for Enter-to-submit + autofocus, since the focusable element is the inner TextInput) → Task 3 Step 1.
- Deferred: shared `StudentDialog` base (tracked follow-up); photo display; content-hardening (Phase 6) — correctly out of scope, no task.

**Placeholder scan:** no "TBD"/"handle errors"/"similar to Task N" — every code step carries complete code; every command carries expected output.

**Type consistency:** `RegisterOutcome` (studentdata.h) is used identically in Task 1 (`parseRegisterResponse`, `registerFinished`) and Task 2 (`onRegisterFinished` slot signature). VM method/property names (`beginRegister`, `setRegDepartment`, `regCourses`, `canRegister`, `regBusy`, `regDuplicate`, `registerReady`, `registerFinished`) match between the header (Task 2 Step 3), the .cpp (Step 4), the QML consumer (Task 3 Step 2/4), and the QuickTest stubs (Task 3 Step 5). `objectName`s in the dialog (`regSubmitButton`, `regDeptCombo`, `regCourseCombo`, `regDuplicateError`, `regPhotoLabel`, `regRemovePhotoButton`, `regSchoolIdField`, `addStudentButton`, `registerDialog`) match the QuickTest `findChild` calls. The register combos deliberately use register-scoped objectNames (`regDeptCombo`/`regCourseCombo`) — NOT the shared `cascDept`/`cascCourse` of the filter's `LCascadingSelect` — so the QuickTest seam stays unambiguous.
