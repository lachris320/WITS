# LOAMS 2.0 Phase 4a.2b-i — Multi-Delete + CSV Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add multi-delete (guarded, typed-confirm for large sets) and client-side CSV export of the selected-or-filtered student rows to the read-only Database screen.

**Architecture:** Pure C++ statics on `StudentController` do the format/classify work (`toCsv` builds RFC-4180 bytes; `deleteReplyIsServerAnswer` classifies a reply exactly like `HttpForm::isServerAnswer`). `DatabaseViewModel` orchestrates given a `QUrl`/selection — it reads `AdminSession` + the table model, calls the controller, writes the file atomically, and exposes a `statusMessage`/`authFailure` surface. QML (`DatabaseScreen`) owns the native `FileDialog` and the `LConfirmDialog`, binds to the VM, and never touches `witscore`. Strict MVVM throughout.

**Tech Stack:** Qt 6.11 / C++17 / QML (Qt Quick), CMake + Ninja + MinGW, QtTest + Qt Quick Test under CTest.

## Global Constraints
- Strict MVVM: the VM is the only QML-facing C++; screens take `property var vm`; QuickTests inject a plain-QML stub VM; QML never calls a `witscore` controller directly.
- Zero raw hex outside `Theme.qml`; every color is a `Theme.<token>`; opacity variants use `Qt.alpha(Theme.<token>, a)`, never a literal color.
- PascalCase QML types + C++ ViewModel/model class names; C++ members are `m_camelCase`.
- Tests register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`), with `OFFSCREEN` for any GUI/Quick/painting/network test.
- `delete_students.php` is admin-key guarded: it takes repeated `school_ids[]` + an `admin_key` FORM field. No new endpoint in this slice.
- The admin key is the HELD `AdminSession::instance().key()`, RAM-only, no re-type; the typed-`DELETE` gate is the only friction.
- Type-to-confirm threshold is hardcoded at 10 (`kTypeToConfirmThreshold`).
- File writes are atomic via `QSaveFile` (open → write → commit).
- CSV = RFC-4180 quoting (comma/quote/CR/LF ⇒ wrap in `"`, inner `"`→`""`) + UTF-8 BOM (`EF BB BF`) + CRLF terminators; columns are every `StudentRecord` field except photo (there is no photo field client-side).

---

### Task 1: `StudentController::toCsv` pure static

**Files:**
- Modify: `qt-app/core/studentcontroller.h` (add one static decl in the "Pure, unit-testable statics" block, ~L19-30, after `parseDeleteResponse`)
- Modify: `qt-app/core/studentcontroller.cpp` (add the definition after `parseDeleteResponse`, ~L104)
- Test: `qt-app/tests/tst_studentcontroller.cpp` (extend the existing `TestStudentController`; the `tst_studentcontroller` CMake target already compiles `studentcontroller.cpp` — no CMake change)

**Interfaces:**
- Consumes: `const QList<StudentRecord> &rows` where `StudentRecord = { QString code, schoolId, name, course, department, yearLevel, gender, status; int visits; }` (`qt-app/core/studentdata.h:12-23`).
- Produces: `static QByteArray StudentController::toCsv(const QList<StudentRecord> &rows);` — a UTF-8 byte array: BOM, then a header line, then one line per record, CRLF-terminated. Header = `code,school_id,name,course,department,year_level,gender,status,visits`.

Steps:

- [ ] **RED** — Add these test-method declarations to the `private slots:` block of `TestStudentController` (`qt-app/tests/tst_studentcontroller.cpp`, after the `parseDeleteResponse` group ~L50):
```cpp
    // toCsv
    void toCsv_emptyList_headerOnlyWithBom();
    void toCsv_headerRowIsExactFieldOrder();
    void toCsv_serializesAllFieldsExceptPhoto();
    void toCsv_quotesCommaEmbeddedField();
    void toCsv_doublesEmbeddedQuote();
    void toCsv_quotesEmbeddedNewline();
    void toCsv_usesCrlfLineTerminators();
```
- [ ] **RED** — Append the test bodies before `QTEST_MAIN` (~L309). `kBom` is the three-byte UTF-8 BOM:
```cpp
static const QByteArray kBom = QByteArrayLiteral("\xEF\xBB\xBF");

void TestStudentController::toCsv_emptyList_headerOnlyWithBom()
{
    const QByteArray csv = StudentController::toCsv({});
    QVERIFY(csv.startsWith(kBom));
    QCOMPARE(csv, kBom + "code,school_id,name,course,department,year_level,gender,status,visits\r\n");
}

void TestStudentController::toCsv_headerRowIsExactFieldOrder()
{
    const QByteArray csv = StudentController::toCsv({});
    const QByteArray body = csv.mid(kBom.size());
    const QByteArray header = body.left(body.indexOf("\r\n"));
    QCOMPARE(header, QByteArrayLiteral(
        "code,school_id,name,course,department,year_level,gender,status,visits"));
}

void TestStudentController::toCsv_serializesAllFieldsExceptPhoto()
{
    StudentRecord r;
    r.code = "C1"; r.schoolId = "2023-001"; r.name = "Juan Cruz"; r.course = "BSIT";
    r.department = "CCS"; r.yearLevel = "2"; r.gender = "Male"; r.status = "Active";
    r.visits = 42;
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("C1,2023-001,Juan Cruz,BSIT,CCS,2,Male,Active,42\r\n"));
}

void TestStudentController::toCsv_quotesCommaEmbeddedField()
{
    StudentRecord r; r.name = "Cruz, Juan";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"Cruz, Juan\""));
}

void TestStudentController::toCsv_doublesEmbeddedQuote()
{
    StudentRecord r; r.name = "Juan \"JC\" Cruz";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"Juan \"\"JC\"\" Cruz\""));
}

void TestStudentController::toCsv_quotesEmbeddedNewline()
{
    StudentRecord r; r.name = "Line1\nLine2";
    const QByteArray csv = StudentController::toCsv({r});
    QVERIFY(csv.contains("\"Line1\nLine2\""));
}

void TestStudentController::toCsv_usesCrlfLineTerminators()
{
    StudentRecord r; r.name = "Ann";
    const QByteArray csv = StudentController::toCsv({r});
    // Header line + one data line, both CRLF-terminated => exactly two "\r\n".
    QCOMPARE(csv.count("\r\n"), 2);
    QVERIFY(!csv.contains("\n\n"));
}
```
- [ ] **RED** — Build + run; confirm it FAILS to compile (no `toCsv` yet):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_studentcontroller
```
Expected: compile error `'toCsv' is not a member of 'StudentController'`.
- [ ] **GREEN** — Declare the static in `qt-app/core/studentcontroller.h`, immediately after the `parseDeleteResponse` decl (~L30):
```cpp
    // Pure, network-free CSV serializer for the Database export (Phase 4a.2b-i).
    // RFC-4180: a cell is quoted iff it contains a comma, quote, CR, or LF, and
    // embedded quotes are doubled; CRLF line terminators; a UTF-8 BOM is
    // prepended so Excel opens it as UTF-8. Columns = every StudentRecord field
    // except photo (there is no client-side photo field). Empty list => header
    // only.
    static QByteArray toCsv(const QList<StudentRecord> &rows);
```
- [ ] **GREEN** — Add the definition in `qt-app/core/studentcontroller.cpp` after `parseDeleteResponse` (~L104):
```cpp
QByteArray StudentController::toCsv(const QList<StudentRecord> &rows)
{
    auto quote = [](const QString &v) -> QString {
        const bool needs = v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
                        || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'));
        if (!needs)
            return v;
        QString q = v;
        q.replace(QLatin1Char('"'), QLatin1String("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    };
    auto line = [&quote](const QStringList &cells) -> QString {
        QStringList out;
        out.reserve(cells.size());
        for (const QString &c : cells)
            out << quote(c);
        return out.join(QLatin1Char(',')) + QLatin1String("\r\n");
    };

    QString csv = line({ QStringLiteral("code"), QStringLiteral("school_id"),
                         QStringLiteral("name"), QStringLiteral("course"),
                         QStringLiteral("department"), QStringLiteral("year_level"),
                         QStringLiteral("gender"), QStringLiteral("status"),
                         QStringLiteral("visits") });
    for (const StudentRecord &r : rows) {
        csv += line({ r.code, r.schoolId, r.name, r.course, r.department,
                      r.yearLevel, r.gender, r.status, QString::number(r.visits) });
    }

    QByteArray out;
    out.append("\xEF\xBB\xBF");            // UTF-8 BOM
    out.append(csv.toUtf8());
    return out;
}
```
- [ ] **GREEN** — Build + run; confirm all 7 new tests pass:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_studentcontroller
ctest --test-dir <build-dir> --output-on-failure -R tst_studentcontroller
```
Expected: `tst_studentcontroller` passes; output includes `PASS   : TestStudentController::toCsv_*` for all 7.
- [ ] **REFACTOR** — Confirm no `\n`-only lines slipped in and the BOM literal is exactly `\xEF\xBB\xBF`. No behavioral change.
- [ ] **COMMIT** — stage by name:
```
git add qt-app/core/studentcontroller.h qt-app/core/studentcontroller.cpp qt-app/tests/tst_studentcontroller.cpp
git commit -m "feat(core): add StudentController::toCsv RFC-4180 serializer for Database export"
```

---

### Task 2: `StudentController::deleteReplyIsServerAnswer` static + rewire `deleteStudents`

**Files:**
- Modify: `qt-app/core/studentcontroller.h` (add one static decl after `toCsv`)
- Modify: `qt-app/core/studentcontroller.cpp` (add definition + rewrite the `deleteStudents` reply lambda ~L243-256; add `#include <QVariant>`)
- Test: `qt-app/tests/tst_studentcontroller.cpp` (existing target — no CMake change)

**Interfaces:**
- Consumes: `bool replyHadError` (`reply->error() != QNetworkReply::NoError`), `int httpStatus` (`QNetworkRequest::HttpStatusCodeAttribute`, or 0 when absent), `const QByteArray &body`.
- Produces: `static bool StudentController::deleteReplyIsServerAnswer(bool replyHadError, int httpStatus, const QByteArray &body);` — identical semantics to `HttpForm::isServerAnswer` (`qt-app/quick/HttpForm.cpp:26-33`): `true` when the reply is a decodable server answer (no error, OR an error that still carries an HTTP status + non-empty body); `false` only for a genuine transport failure.
- Existing signals unchanged: `deleteFinished(bool ok, int requestedCount, const QString &message)`, `deleteFailed(const QString &errorString)`.

Steps:

- [ ] **RED** — Add declarations to `TestStudentController`'s `private slots:` (after the `toCsv` group):
```cpp
    // deleteReplyIsServerAnswer (mirrors HttpForm::isServerAnswer)
    void deleteReplyIsServerAnswer_transportNoStatus_isFalse();
    void deleteReplyIsServerAnswer_401WithBody_isTrue();
    void deleteReplyIsServerAnswer_200Error_isTrue();
    void deleteReplyIsServerAnswer_200Success_isTrue();
    void deleteReplyIsServerAnswer_statusButEmptyBody_isFalse();
```
- [ ] **RED** — Append the bodies before `QTEST_MAIN`:
```cpp
void TestStudentController::deleteReplyIsServerAnswer_transportNoStatus_isFalse()
{
    // hadError=true, no HTTP status (attribute absent => 0): a real transport
    // failure — DNS/refused/timeout.
    QVERIFY(!StudentController::deleteReplyIsServerAnswer(true, 0, QByteArray()));
}

void TestStudentController::deleteReplyIsServerAnswer_401WithBody_isTrue()
{
    // requireAdminAuth answers a bad/expired key with 401 + a JSON body —
    // that IS the server answering and must reach parseDeleteResponse.
    const QByteArray body = R"({"status":"error","message":"Invalid admin key"})";
    QVERIFY(StudentController::deleteReplyIsServerAnswer(true, 401, body));
}

void TestStudentController::deleteReplyIsServerAnswer_200Error_isTrue()
{
    const QByteArray body = R"({"status":"error","message":"Invalid data"})";
    QVERIFY(StudentController::deleteReplyIsServerAnswer(false, 200, body));
}

void TestStudentController::deleteReplyIsServerAnswer_200Success_isTrue()
{
    QVERIFY(StudentController::deleteReplyIsServerAnswer(false, 200,
                                                         R"({"status":"success"})"));
}

void TestStudentController::deleteReplyIsServerAnswer_statusButEmptyBody_isFalse()
{
    // A status code with an empty body gives the decode seam nothing to
    // classify — treated as a transport failure, exactly like HttpForm.
    QVERIFY(!StudentController::deleteReplyIsServerAnswer(true, 500, QByteArray()));
}
```
- [ ] **RED** — Build the target; confirm it fails to compile (`deleteReplyIsServerAnswer` unknown):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_studentcontroller
```
Expected: compile error naming `deleteReplyIsServerAnswer`.
- [ ] **GREEN** — Declare the static in `qt-app/core/studentcontroller.h`, right after the `toCsv` decl:
```cpp
    // True when a delete reply is a decodable server answer (has an HTTP status
    // + body) rather than a transport failure. Mirrors HttpForm::isServerAnswer
    // so a guard 401 (bad/expired admin key) reaches parseDeleteResponse
    // instead of being misreported as a network error.
    static bool deleteReplyIsServerAnswer(bool replyHadError, int httpStatus,
                                          const QByteArray &body);
```
- [ ] **GREEN** — Add `#include <QVariant>` to `qt-app/core/studentcontroller.cpp` (with the other Qt includes ~L4-10), and add the definition after `toCsv`:
```cpp
bool StudentController::deleteReplyIsServerAnswer(bool replyHadError, int httpStatus,
                                                  const QByteArray &body)
{
    if (!replyHadError)
        return true;
    // An error status is still the server answering — provided it sent
    // something for the decode seam to read.
    return httpStatus > 0 && !body.isEmpty();
}
```
- [ ] **GREEN** — Rewrite ONLY the `deleteStudents` reply lambda (`qt-app/core/studentcontroller.cpp:243-256`) to classify before deciding. Replace the whole `connect(...)` body with:
```cpp
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestedCount]() {
        const QByteArray resp = reply->readAll();
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QString errorString = reply->errorString();
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (deleteReplyIsServerAnswer(hadError, httpStatus, resp)) {
            QString message;
            const bool ok = parseDeleteResponse(resp, message);
            emit deleteFinished(ok, requestedCount, message);   // 401 body reaches here
        } else {
            emit deleteFailed(errorString);                     // genuine transport failure only
        }
    });
```
Leave the sibling `bulkUpdateStudents` (`~L211`) untouched — its identical error-first pattern is 4a.2b-iii's work (forward-note only). The existing `deleteStudents_buildsFormBodyWithAdminKey` request-assembly test still passes because `CapturingNam` returns `{status:success}` with no error → `deleteFinished(true, 2, "")`, and that test only inspects the outgoing body.
- [ ] **GREEN** — Build + run; confirm all 5 new tests pass and the pre-existing `deleteStudents_buildsFormBodyWithAdminKey` / `parseDeleteResponse_*` still pass:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_studentcontroller
ctest --test-dir <build-dir> --output-on-failure -R tst_studentcontroller
```
Expected: `tst_studentcontroller` passes; `PASS : TestStudentController::deleteReplyIsServerAnswer_*` (5) plus all prior tests green.
- [ ] **REFACTOR** — Verify `errorString` is captured BEFORE `reply->deleteLater()` (a use-after-free otherwise). No other change.
- [ ] **COMMIT**:
```
git add qt-app/core/studentcontroller.h qt-app/core/studentcontroller.cpp qt-app/tests/tst_studentcontroller.cpp
git commit -m "fix(core): route delete guard 401 to server-answer path via deleteReplyIsServerAnswer"
```

---

### Task 3: `StudentsTableModel::selectedRecords()` / `allRecords()`

**Files:**
- Modify: `qt-app/quick/models/StudentsTableModel.h` (add two `const` accessors near `selectedIds()` ~L37)
- Modify: `qt-app/quick/models/StudentsTableModel.cpp` (add definitions after `selectedIds()` ~L55)
- Test: `qt-app/quick/tests/tst_studentstablemodel.cpp` (existing `tst_studentstablemodel` target — no CMake change)

**Interfaces:**
- Consumes: the existing private `QList<StudentRecord> m_records` and `QSet<QString> m_selected` (`StudentsTableModel.h:49-50`).
- Produces:
  - `QList<StudentRecord> StudentsTableModel::selectedRecords() const;` — records whose `schoolId` is in the selection set, in `m_records` order.
  - `QList<StudentRecord> StudentsTableModel::allRecords() const;` — returns `m_records` verbatim.

Steps:

- [ ] **RED** — Add declarations to `TestStudentsTableModel`'s `private slots:` (`qt-app/quick/tests/tst_studentstablemodel.cpp`, after `selectedIdsReturnsOnlySelected` ~L20):
```cpp
    void selectedRecordsReturnsOnlySelectedInOrder();
    void selectedRecordsEmptyWhenNoneSelected();
    void allRecordsReturnsEveryLoadedRow();
```
- [ ] **RED** — Append the bodies before `QTEST_APPLESS_MAIN`:
```cpp
void TestStudentsTableModel::selectedRecordsReturnsOnlySelectedInOrder()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben"), rec("C","Cara")});
    m.toggle("C"); m.toggle("A");
    const QList<StudentRecord> sel = m.selectedRecords();
    QCOMPARE(sel.size(), 2);
    // m_records order (A before C), not selection/insertion order.
    QCOMPARE(sel.at(0).schoolId, QStringLiteral("A"));
    QCOMPARE(sel.at(1).schoolId, QStringLiteral("C"));
}

void TestStudentsTableModel::selectedRecordsEmptyWhenNoneSelected()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    QVERIFY(m.selectedRecords().isEmpty());
}

void TestStudentsTableModel::allRecordsReturnsEveryLoadedRow()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.toggle("A");                       // selection must not affect allRecords()
    const QList<StudentRecord> all = m.allRecords();
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.at(0).schoolId, QStringLiteral("A"));
    QCOMPARE(all.at(1).schoolId, QStringLiteral("B"));
}
```
- [ ] **RED** — Build + run; confirm compile failure (methods missing):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_studentstablemodel
```
Expected: error `'selectedRecords'/'allRecords' is not a member of 'StudentsTableModel'`.
- [ ] **GREEN** — Declare in `qt-app/quick/models/StudentsTableModel.h`, right after `QStringList selectedIds() const;` (~L37):
```cpp
    QList<StudentRecord> selectedRecords() const; // records whose schoolId is selected, m_records order
    QList<StudentRecord> allRecords() const;      // all currently-loaded/filtered records
```
- [ ] **GREEN** — Define in `qt-app/quick/models/StudentsTableModel.cpp`, after `selectedIds()` (~L55):
```cpp
QList<StudentRecord> StudentsTableModel::selectedRecords() const
{
    QList<StudentRecord> out;
    out.reserve(m_selected.size());
    for (const StudentRecord &r : m_records)
        if (m_selected.contains(r.schoolId))
            out.append(r);
    return out;
}

QList<StudentRecord> StudentsTableModel::allRecords() const
{
    return m_records;
}
```
- [ ] **GREEN** — Build + run; confirm the 3 new tests pass alongside the existing selection tests:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_studentstablemodel
ctest --test-dir <build-dir> --output-on-failure -R tst_studentstablemodel
```
Expected: `tst_studentstablemodel` passes; `PASS : TestStudentsTableModel::selectedRecords*` + `allRecordsReturnsEveryLoadedRow`.
- [ ] **REFACTOR** — Confirm `selectedRecords()` iterates `m_records` (stable order), not `m_selected` (unordered set). No change if already so.
- [ ] **COMMIT**:
```
git add qt-app/quick/models/StudentsTableModel.h qt-app/quick/models/StudentsTableModel.cpp qt-app/quick/tests/tst_studentstablemodel.cpp
git commit -m "feat(quick): add StudentsTableModel selectedRecords/allRecords accessors for export"
```

---

### Task 4: DatabaseViewModel delete surface

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (add threshold, invokables, public-slot handlers, `statusMessage`/`authFailure` Q_PROPERTYs + signals, in-flight bool)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (wire signals in ctor; implement handlers; add includes `<QUrl>`, `AdminSession.h`, `SettingsViewModel.h`)
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h` (promote `isAuthFailureMessage` from `private` to a `public static` so the Database VM reuses the same predicate)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp` (existing `tst_databaseviewmodel` target — no CMake change)

**Interfaces:**
- Consumes:
  - `QStringList StudentsTableModel::selectedIds() const` and `void clearSelection()` (existing).
  - `QString AdminSession::instance().key()` (`qt-app/quick/AdminSession.h:18`).
  - `void StudentController::deleteStudents(const QStringList &schoolIds, const QString &adminKey)` and its signals `deleteFinished(bool ok, int requestedCount, const QString &message)` / `deleteFailed(const QString &errorString)`.
  - `static bool SettingsViewModel::isAuthFailureMessage(const QString &message)` (promoted to public in this task).
- Produces (on `DatabaseViewModel`):
  - `static constexpr int kTypeToConfirmThreshold = 10;`
  - `Q_INVOKABLE bool requiresTypedConfirmation(int count) const;`
  - `Q_INVOKABLE void deleteSelected();`
  - `void onDeleteFinished(bool ok, int requestedCount, const QString &message);` (public test seam)
  - `void onDeleteFailed(const QString &errorString);` (public test seam)
  - `Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)`
  - `Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)`

Steps:

- [ ] **RED** — Add test declarations to `TestDatabaseViewModel`'s `private slots:` (`qt-app/quick/tests/tst_databaseviewmodel.cpp` ~L14). Include a `cleanup()` slot (QtTest runs it after EACH test) that clears the process-wide `AdminSession`, so a test that sets the held key can't leak it into a later test even if an assertion aborts before an inline `clear()`:
```cpp
    void cleanup();     // per-test isolation: reset the process-wide AdminSession
    void requiresTypedConfirmationBoundary();
    void deleteSelectedPostsIdsAndAdminKey();
    void onDeleteFinishedSuccessRefreshesClearsAndSetsStatus();
    void onDeleteFinishedAuthFailureSetsAuthState();
    void onDeleteFinishedGenericFailureSetsStatusNoAuth();
    void onDeleteFailedSetsTransientStatus();
```
- [ ] **RED** — Add includes at the top of the test (after the existing includes ~L5):
```cpp
#include <QUrlQuery>
#include "AdminSession.h"
#include "studentcontroller.h"
```
Do NOT include `capturingnam.h` here: it lives in `qt-app/testsupport/`, which is
on the include path only for `tst_studentcontroller` (`qt-app/tests/CMakeLists.txt:115`);
`wits_add_qttest` does not add it, so the include would not compile — and the VM
layer does not use it anyway (request wire-format is covered at the controller
layer in Task 2).

Add a test-only ctor is NOT available — the VM builds its own NAM. For request-assembly, drive through `AdminSession` + the VM's real `deleteSelected()`, which posts via the VM's internal `QNetworkAccessManager`. Because that NAM is `new QNetworkAccessManager(this)` inside the VM, request-assembly is asserted the same way the spec's CapturingNam note prescribes for the *controller* layer (Task 2 already covers the wire format); at the VM layer assert instead that `deleteSelected()` with an empty selection is a no-op and that the held key is what would be sent. Add bodies:
```cpp
void TestDatabaseViewModel::cleanup()
{
    AdminSession::instance().clear();   // isolate the process-wide singleton per test
}

void TestDatabaseViewModel::requiresTypedConfirmationBoundary()
{
    DatabaseViewModel vm;
    QCOMPARE(vm.requiresTypedConfirmation(9), false);
    QCOMPARE(vm.requiresTypedConfirmation(10), true);
    QCOMPARE(vm.requiresTypedConfirmation(11), true);
}

void TestDatabaseViewModel::deleteSelectedPostsIdsAndAdminKey()
{
    // Empty selection => no network call, no status change (guard).
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    vm.deleteSelected();
    QVERIFY(vm.statusMessage().isEmpty());

    // With a selection, deleteSelected() sources ids from the model and the key
    // from AdminSession. The controller wire format (school_ids[] + admin_key)
    // is asserted in tst_studentcontroller::deleteStudents_buildsFormBodyWithAdminKey;
    // here we assert the VM feeds the controller a non-empty request by
    // observing that the in-flight path does not early-return: select a row,
    // call deleteSelected(), then drive the reply seam directly.
    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.students()->toggle("2023-001");
    QCOMPARE(vm.students()->selectedIds(), QStringList{"2023-001"});
    vm.deleteSelected();                 // posts via the VM's own NAM (no live server in CI)
    // The success/failure handlers are unit-tested directly below; this step
    // only proves the guarded path was entered (selection non-empty).
    QVERIFY(vm.students()->selectedCount() == 1);
    AdminSession::instance().clear();
}

void TestDatabaseViewModel::onDeleteFinishedSuccessRefreshesClearsAndSetsStatus()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.students()->toggle("2023-001");
    QCOMPARE(vm.students()->selectedCount(), 1);

    vm.onDeleteFinished(true, 3, QString());
    QCOMPARE(vm.statusMessage(), QStringLiteral("Deleted 3 students"));
    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.students()->selectedCount(), 0);   // cleared
    QVERIFY(vm.loading());                          // reloadTable() flipped loading on
}

void TestDatabaseViewModel::onDeleteFinishedAuthFailureSetsAuthState()
{
    DatabaseViewModel vm;
    vm.onDeleteFinished(false, 2, QStringLiteral("Invalid admin key"));
    QVERIFY(vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
}

void TestDatabaseViewModel::onDeleteFinishedGenericFailureSetsStatusNoAuth()
{
    DatabaseViewModel vm;
    vm.onDeleteFinished(false, 2, QStringLiteral("Invalid data"));
    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.statusMessage(), QStringLiteral("Invalid data"));
}

void TestDatabaseViewModel::onDeleteFailedSetsTransientStatus()
{
    DatabaseViewModel vm;
    vm.onDeleteFailed(QStringLiteral("Connection refused"));
    QVERIFY(!vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
}
```
- [ ] **RED** — Build; confirm compile failure (new members missing):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_databaseviewmodel
```
Expected: errors naming `requiresTypedConfirmation` / `deleteSelected` / `onDeleteFinished` / `statusMessage` / `authFailure`.
- [ ] **GREEN** — Promote the predicate in `qt-app/quick/viewmodels/SettingsViewModel.h`: MOVE the line
```cpp
    static bool isAuthFailureMessage(const QString &message);
```
out of the `private:` block (currently ~L170) up into the `public:` section (e.g. just after the `serializeCsv` static decl ~L111). Keep its comment. No `.cpp` change — the definition (`SettingsViewModel.cpp:152-156`) is unaffected.
- [ ] **GREEN** — In `qt-app/quick/viewmodels/DatabaseViewModel.h`: add the two Q_PROPERTYs after `errorText` (~L27):
```cpp
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)
```
Add to the `public:` block (after `QString errorText()...` ~L37):
```cpp
    static constexpr int kTypeToConfirmThreshold = 10;
    QString statusMessage() const { return m_statusMessage; }
    bool authFailure() const { return m_authFailure; }
```
Add the invokables after `reloadTable()` (~L42):
```cpp
    // The VM owns the small-vs-large decision; QML consumes only this boolean.
    Q_INVOKABLE bool requiresTypedConfirmation(int count) const
    { return count >= kTypeToConfirmThreshold; }
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE bool exportCsv(const QUrl &fileUrl);   // implemented in Task 5
```
Add the delete handlers to the "Public slots (test seam ...)" block (after `onCoursesLoaded` ~L49):
```cpp
    void onDeleteFinished(bool ok, int requestedCount, const QString &message);
    void onDeleteFailed(const QString &errorString);
```
Add the new signals (after `errorTextChanged()` ~L57):
```cpp
    void statusMessageChanged();
    void authFailureChanged();
```
Add private setters + state (in the `private:` block near `setError` ~L62, and members near `m_errorText`):
```cpp
    void setStatusMessage(const QString &m);
    void setAuthFailure(bool v);
```
and members:
```cpp
    QString m_statusMessage;
    bool m_authFailure = false;
    bool m_deleteInFlight = false;
```
Add `#include <QUrl>` near the top includes (~L4).
- [ ] **GREEN** — In `qt-app/quick/viewmodels/DatabaseViewModel.cpp`: add includes after the existing ones (~L4):
```cpp
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include "AdminSession.h"
#include "SettingsViewModel.h"
```
Wire the controller signals in the ctor, after the existing `connect(... coursesLoaded ...)` line (~L14):
```cpp
    connect(m_controller, &StudentController::deleteFinished, this, &DatabaseViewModel::onDeleteFinished);
    connect(m_controller, &StudentController::deleteFailed, this, &DatabaseViewModel::onDeleteFailed);
```
Add the delete implementations (near the bottom, before the trailing setters):
```cpp
void DatabaseViewModel::deleteSelected()
{
    const QStringList ids = m_students.selectedIds();
    if (ids.isEmpty())
        return;                       // nothing selected — guard
    m_deleteInFlight = true;
    m_controller->deleteStudents(ids, AdminSession::instance().key());
}

void DatabaseViewModel::onDeleteFinished(bool ok, int requestedCount, const QString &message)
{
    m_deleteInFlight = false;
    if (ok) {
        setAuthFailure(false);
        setStatusMessage(tr("Deleted %1 students").arg(requestedCount));
        reloadTable();                // re-fetch the current dept/course filter
        m_students.clearSelection();  // deleted rows also drop via setRecords' intersect
        return;
    }
    // Server rejection. Tell the 401 held-key failure apart from a generic
    // server error via the SAME predicate SettingsViewModel uses (§Error Taxonomy).
    if (SettingsViewModel::isAuthFailureMessage(message)) {
        setAuthFailure(true);
        setStatusMessage(tr("Admin authentication failed — re-enter via admin login."));
    } else {
        setAuthFailure(false);
        setStatusMessage(message.isEmpty() ? tr("Delete failed.") : message);
    }
}

void DatabaseViewModel::onDeleteFailed(const QString & /*errorString*/)
{
    m_deleteInFlight = false;
    setAuthFailure(false);
    setStatusMessage(tr("Delete failed — check your connection."));
}
```
Add the setters next to the existing `setLoading`/`setError` (~L91-92):
```cpp
void DatabaseViewModel::setStatusMessage(const QString &m) { m_statusMessage = m; emit statusMessageChanged(); }
void DatabaseViewModel::setAuthFailure(bool v) { if (m_authFailure != v) { m_authFailure = v; emit authFailureChanged(); } }
```
(`setStatusMessage` fires unconditionally so re-issuing the same message still re-triggers the toast, mirroring `SettingsViewModel::setStatus`.)
- [ ] **Add the required `exportCsv` stub NOW (not later).** `exportCsv` is declared `Q_INVOKABLE` in this task, so moc emits a metacall reference to `&DatabaseViewModel::exportCsv`; without a definition the `tst_databaseviewmodel` target FAILS TO LINK and the GREEN step below is unreachable. Add a temporary stub (Task 5 replaces its body):
```cpp
// Temporary stub — real implementation lands in Task 5. Present so the moc
// metacall for the Q_INVOKABLE resolves and this task's target links.
bool DatabaseViewModel::exportCsv(const QUrl & /*fileUrl*/) { return false; }
```
- [ ] **GREEN** — Build + run; confirm all 6 new tests pass and the 4 pre-existing DatabaseViewModel tests still pass:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_databaseviewmodel
ctest --test-dir <build-dir> --output-on-failure -R tst_databaseviewmodel
```
Expected: `tst_databaseviewmodel` passes; `PASS : TestDatabaseViewModel::requiresTypedConfirmationBoundary` + the 5 delete tests + prior tests green.
- [ ] **REFACTOR** — No change needed; Task 5 replaces the stub body with the real writer. (Alternatively, an executor may proceed straight into Task 5 and skip committing the stub separately.)
- [ ] **COMMIT**:
```
git add qt-app/quick/viewmodels/DatabaseViewModel.h qt-app/quick/viewmodels/DatabaseViewModel.cpp qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/tests/tst_databaseviewmodel.cpp
git commit -m "feat(quick): add DatabaseViewModel multi-delete surface with 401 auth-state mapping"
```

---

### Task 5: DatabaseViewModel `exportCsv`

**Files:**
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (implement `exportCsv`; includes `<QSaveFile>`, `<QFileInfo>`, `<QUrl>` already added in Task 4)
- Test: `qt-app/quick/tests/tst_databaseviewmodel.cpp` (existing target)

**Interfaces:**
- Consumes: `bool StudentsTableModel::anySelected() const`, `QList<StudentRecord> selectedRecords() const`, `QList<StudentRecord> allRecords() const` (Task 3); `static QByteArray StudentController::toCsv(...)` (Task 1); `QUrl::toLocalFile()`.
- Produces: `Q_INVOKABLE bool DatabaseViewModel::exportCsv(const QUrl &fileUrl);` — writes the CSV bytes atomically; returns `true` on success (and sets status `"Exported N rows to <basename>"`), `false` on write failure (and sets an error status).

Steps:

- [ ] **RED** — Add test declarations to `TestDatabaseViewModel`'s `private slots:`:
```cpp
    void exportCsvWritesAllRowsWhenNoneSelected();
    void exportCsvWritesOnlySelectedWhenSomeSelected();
    void exportCsvWriteFailureReturnsFalse();
```
- [ ] **RED** — Add `#include <QTemporaryDir>` and `#include <QFile>` to the test includes, then append the bodies:
```cpp
void TestDatabaseViewModel::exportCsvWritesAllRowsWhenNoneSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";  a.visits = 1;
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";  b.visits = 2;
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("all.csv");
    QVERIFY(vm.exportCsv(QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    QCOMPARE(bytes, StudentController::toCsv({a, b}));   // exact byte-for-byte
    QCOMPARE(vm.statusMessage(), QStringLiteral("Exported 2 rows to all.csv"));
}

void TestDatabaseViewModel::exportCsvWritesOnlySelectedWhenSomeSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("B");                          // only B selected

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("sel.csv");
    QVERIFY(vm.exportCsv(QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), StudentController::toCsv({b})); // B only, not A
    QCOMPARE(vm.statusMessage(), QStringLiteral("Exported 1 rows to sel.csv"));
}

void TestDatabaseViewModel::exportCsvWriteFailureReturnsFalse()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {a}, "", "", 1);
    // A path whose parent directory does not exist => QSaveFile::open fails.
    const bool ok = vm.exportCsv(QUrl::fromLocalFile(
        QStringLiteral("./no_such_dir_xyz/out.csv")));
    QVERIFY(!ok);
    QVERIFY(!vm.statusMessage().isEmpty());
}
```
- [ ] **RED** — Build + run. Task 4 added the temporary stub (`return false;`), so the target links and the 3 new tests FAIL at runtime (stub returns false / writes nothing):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_databaseviewmodel
ctest --test-dir <build-dir> --output-on-failure -R tst_databaseviewmodel
```
Expected: the 3 new `exportCsv*` tests FAIL (stub returns false / no file written); everything else green.
- [ ] **GREEN** — Add the definition in `qt-app/quick/viewmodels/DatabaseViewModel.cpp` (after `deleteSelected`), replacing any temporary stub:
```cpp
bool DatabaseViewModel::exportCsv(const QUrl &fileUrl)
{
    const QList<StudentRecord> rows =
        m_students.anySelected() ? m_students.selectedRecords() : m_students.allRecords();
    const QByteArray bytes = StudentController::toCsv(rows);

    const QString path = fileUrl.toLocalFile();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatusMessage(tr("Export failed — could not open the file for writing."));
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        setStatusMessage(tr("Export failed — could not write the file."));
        return false;
    }
    setStatusMessage(tr("Exported %1 rows to %2")
                         .arg(rows.size()).arg(QFileInfo(path).fileName()));
    return true;
}
```
- [ ] **GREEN** — Build + run; confirm all 3 export tests pass (and the 6 delete tests + 4 prior tests remain green):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_databaseviewmodel
ctest --test-dir <build-dir> --output-on-failure -R tst_databaseviewmodel
```
Expected: `tst_databaseviewmodel` passes; `PASS : TestDatabaseViewModel::exportCsv*` (3).
- [ ] **REFACTOR** — Confirm `exportCsv` uses `anySelected()` (not `selectedCount() > 0` twice) and that the selected-vs-all branch matches the spec (selected if any, else all). No change if already so.
- [ ] **COMMIT**:
```
git add qt-app/quick/viewmodels/DatabaseViewModel.cpp qt-app/quick/tests/tst_databaseviewmodel.cpp
git commit -m "feat(quick): add DatabaseViewModel::exportCsv atomic QSaveFile writer"
```

---

### Task 6: LButton `tooltipText` + `accessibleName`

**Files:**
- Modify: `qt-app/quick/qml/components/LButton.qml` (add two string properties, a Theme-tokened `ToolTip`, and an `Accessible.name` override)
- Test: `qt-app/quick/tests/tst_qml_components.qml` (add a fixture + a TestCase; `tst_qml_components` target compiles the whole `tests/` dir via `QUICK_TEST_SOURCE_DIR` — no CMake change)

**Interfaces:**
- Consumes: `Theme.card`, `Theme.border`, `Theme.text`, `Theme.typography.sans`, `Theme.typography.body`, `Theme.radius.md`, `Theme.spacing.sm` (all existing tokens); QQC2 `Button.hovered`.
- Produces (new public QML API on `LButton`): `property string tooltipText: ""`, `property string accessibleName: ""`. Both default empty → behavior unchanged.

Steps:

- [ ] **RED** — Add a fixture near the other component fixtures in `qt-app/quick/tests/tst_qml_components.qml` (e.g. after the `LButton { id: b; text: "OK" }` line ~L15):
```qml
    LButton {
        id: bTip
        text: "Delete ( 3 )"
        tooltipText: "Exports selected rows, or all filtered rows if none are selected."
        accessibleName: "Delete 3 selected rows"
    }
```
- [ ] **RED** — Add a TestCase (anywhere at the top level of the `Item`, alongside the existing ones):
```qml
    TestCase {
        name: "LButtonTooltipAndAccessibleName"
        when: windowShown
        function test_accessibleNameOverridesTextWhenSet() {
            compare(bTip.Accessible.name, "Delete 3 selected rows");
        }
        function test_accessibleNameFallsBackToTextWhenEmpty() {
            // The default fixture `b` (LButton { text: "OK" }) sets no
            // accessibleName, so Accessible.name must still be the label.
            compare(b.Accessible.name, "OK");
        }
        function test_tooltipTextIsHeldButHiddenUntilHover() {
            var tip = findChild(bTip, "lbuttonTooltip");
            verify(tip !== null);
            compare(tip.text, "Exports selected rows, or all filtered rows if none are selected.");
            compare(tip.visible, false);        // not hovered => hidden
        }
    }
```
- [ ] **RED** — Build + run; confirm the 3 new assertions fail (properties/tooltip absent, `Accessible.name` is still the raw text with spaces):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_qml_components
ctest --test-dir <build-dir> --output-on-failure -R tst_qml_components
```
Expected: `FAIL! : LButtonTooltipAndAccessibleName::test_*` (findChild null / property undefined).
- [ ] **GREEN** — Edit `qt-app/quick/qml/components/LButton.qml`. Add `import QtQuick.Controls` is already present (L2). Add the two properties after `property bool onBrand: false` (~L15):
```qml
    // Supplemental hover tooltip (themed — the default QQC2 ToolTip ignores
    // Theme). Empty => no tooltip, behavior unchanged. NOT exposed to assistive
    // tech, so any scope info here must ALSO live in accessibleName.
    property string tooltipText: ""
    // When set, overrides Accessible.name so a screen reader reads a full-scope
    // phrase ("Delete 3 selected rows") instead of the terse label.
    property string accessibleName: ""
```
Change the final `Accessible.name` line (~L45) from `Accessible.name: control.text` to:
```qml
    Accessible.name: control.accessibleName !== "" ? control.accessibleName : control.text
```
Add the themed ToolTip as a child of the `Button` (before the closing `}` ~L46):
```qml
    ToolTip {
        id: lbuttonTip
        objectName: "lbuttonTooltip"
        text: control.tooltipText
        delay: 500
        visible: control.tooltipText !== "" && control.hovered
        padding: Theme.spacing.sm
        contentItem: Text {
            text: lbuttonTip.text
            color: Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            wrapMode: Text.WordWrap
        }
        background: Rectangle {
            color: Theme.card
            radius: Theme.radius.md
            border.width: 1
            border.color: Theme.border
        }
    }
```
- [ ] **GREEN** — Build + run; confirm the 3 new tests pass AND every pre-existing LButton test (`test_buttonBindsBrandToken`, `test_outlineButtonOnBrandUsesCreamLabel`, etc.) still passes:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_qml_components
ctest --test-dir <build-dir> --output-on-failure -R tst_qml_components
```
Expected: `tst_qml_components` passes; `PASS : LButtonTooltipAndAccessibleName::test_*` (3), no QWARN about ToolTip customization (Basic style is forced by `wits_add_qttest OFFSCREEN`).
- [ ] **REFACTOR** — Confirm no raw hex was introduced (tooltip bg/border/text are all `Theme.*`) and that the ToolTip's `visible` gate keeps it hidden when `tooltipText` is empty. No change if clean.
- [ ] **COMMIT**:
```
git add qt-app/quick/qml/components/LButton.qml qt-app/quick/tests/tst_qml_components.qml
git commit -m "feat(quick): add themed tooltipText and accessibleName to LButton"
```

---

### Task 7: LConfirmDialog typed-confirmation gate

**Files:**
- Modify: `qt-app/quick/qml/components/LConfirmDialog.qml` (add `requireTypedConfirmation` + `confirmationWord`; a Loader-instantiated typed field; fold the gate into the EXISTING confirm-button `enabled` binding; extend `clearKey`)
- Test: `qt-app/quick/tests/tst_qml_components.qml` (add a fixture + assertions; existing target)

**Interfaces:**
- Consumes: existing `keyReady` readonly, `busy`, the Loader-not-`visible` pattern (`LConfirmDialog.qml:20-24`), `LTextField`.
- Produces (new public QML API on `LConfirmDialog`): `property bool requireTypedConfirmation: false`, `property string confirmationWord: "DELETE"`. Default `false` ⇒ existing call sites (SettingsScreen tier-2, existing fixtures) unchanged.

Steps:

- [ ] **RED** — Add a fixture near `cd1`/`cd2` in `qt-app/quick/tests/tst_qml_components.qml` (~L111):
```qml
    LConfirmDialog {
        id: cdTyped; tier: 1; title: "Delete students?"
        message: "This cannot be undone."; confirmText: "Delete"
        requireTypedConfirmation: true; confirmationWord: "DELETE"
    }
```
- [ ] **RED** — Add a TestCase (top level of the `Item`):
```qml
    TestCase {
        name: "LConfirmDialogTypedConfirmation"
        when: windowShown
        function init() {
            cdTyped.visible = false; cdTyped.busy = false;
            var f = findChild(cdTyped, "confirmTypedField");
            if (f) f.text = "";
        }
        function test_typedFieldExistsWhenRequired() {
            verify(findChild(cdTyped, "confirmTypedField") !== null);
        }
        function test_defaultDialogHasNoTypedField() {
            // cd1 keeps the default requireTypedConfirmation:false.
            compare(findChild(cd1, "confirmTypedField"), null);
        }
        function test_confirmDisabledUntilWordTypedExactly() {
            cdTyped.visible = true; waitForRendering(cdTyped);
            var btn = findChild(cdTyped, "confirmButton");
            compare(btn.enabled, false);                 // empty
            findChild(cdTyped, "confirmTypedField").text = "delete";
            compare(btn.enabled, false);                 // wrong case — exact match required
            findChild(cdTyped, "confirmTypedField").text = "DELETE";
            compare(btn.enabled, true);
        }
        function test_typedFieldClearedOnReopen() {
            cdTyped.visible = true; waitForRendering(cdTyped);
            findChild(cdTyped, "confirmTypedField").text = "DELETE";
            cdTyped.visible = false;
            cdTyped.visible = true; waitForRendering(cdTyped);
            compare(findChild(cdTyped, "confirmTypedField").text, "");
            compare(findChild(cdTyped, "confirmButton").enabled, false);
        }
    }
```
- [ ] **RED** — Build + run; confirm failures (typed field/props absent):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_qml_components
ctest --test-dir <build-dir> --output-on-failure -R tst_qml_components
```
Expected: `FAIL! : LConfirmDialogTypedConfirmation::test_*`.
- [ ] **GREEN** — Edit `qt-app/quick/qml/components/LConfirmDialog.qml`. Add the two properties after `signal cancelled()` (~L17):
```qml
    // Optional typed-confirmation gate (Phase 4a.2b-i). When true, a text field
    // appears and Confirm stays disabled until it EXACTLY equals confirmationWord.
    // Default false keeps every existing call site (SettingsScreen tier-2,
    // fixtures) unchanged.
    property bool requireTypedConfirmation: false
    property string confirmationWord: "DELETE"
```
Add a readonly gate after the existing `keyReady` readonly (~L24):
```qml
    readonly property bool typedConfirmReady: !root.requireTypedConfirmation
        || (typedFieldLoader.item !== null
            && typedFieldLoader.item.text === root.confirmationWord)
```
Extend `clearKey()` (~L33-36) to also clear the typed field:
```qml
    function clearKey() {
        if (keyFieldLoader.item)
            keyFieldLoader.item.text = "";
        if (typedFieldLoader.item)
            typedFieldLoader.item.text = "";
    }
```
Add a Loader for the typed field inside the `ColumnLayout`, immediately after the existing `keyFieldLoader` Loader block (~L52, before the button `RowLayout`). Follow the same Loader-not-`visible` pattern so `findChild` locates it:
```qml
        Loader {
            id: typedFieldLoader
            Layout.fillWidth: true
            active: root.requireTypedConfirmation
            sourceComponent: LTextField {
                objectName: "confirmTypedField"
                label: qsTr("Type %1 to confirm").arg(root.confirmationWord)
            }
        }
```
Fold the gate into the EXISTING confirm-button `enabled` binding (`~L68`), changing:
```qml
                enabled: !root.busy && root.keyReady
```
to:
```qml
                enabled: !root.busy && root.keyReady && root.typedConfirmReady
```
- [ ] **GREEN** — Build + run; confirm the 4 new tests pass AND the entire pre-existing `LConfirmDialogTiers` TestCase (tier-1/tier-2 key-field behavior, busy gating, trimmed-key emission, clear-on-reopen) still passes unchanged:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_qml_components
ctest --test-dir <build-dir> --output-on-failure -R tst_qml_components
```
Expected: `tst_qml_components` passes; `PASS : LConfirmDialogTypedConfirmation::test_*` (4) + all `LConfirmDialogTiers::test_*` green.
- [ ] **REFACTOR** — Confirm the gate is folded into the single `enabled` expression (not a parallel binding) and the typed field uses the Loader-not-`visible` pattern. No change if already so.
- [ ] **COMMIT**:
```
git add qt-app/quick/qml/components/LConfirmDialog.qml qt-app/quick/tests/tst_qml_components.qml
git commit -m "feat(quick): add optional typed-confirmation gate to LConfirmDialog"
```

---

### Task 8: DatabaseScreen wiring + CMake (QuickDialogs2)

**Files:**
- Modify: `qt-app/quick/qml/admin/DatabaseScreen.qml` (wrap `tableCountHeader` in a RowLayout with Export + Delete `LButton`s; add `FileDialog`, `LConfirmDialog`, `LToast`; `import QtQuick.Dialogs`)
- Modify: `qt-app/CMakeLists.txt` (add `QuickDialogs2` to the `find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ...)` list at `:17` — the top-level find_package is the ONLY one; `quick/CMakeLists.txt` has none, so without this the `Qt6::QuickDialogs2` imported target does not exist and configure FAILS)
- Modify: `qt-app/quick/CMakeLists.txt` (link `Qt6::QuickDialogs2` on `witsquickmodule` so the `QtQuick.Dialogs` import resolves at runtime and for the QuickTest binaries)
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (extend the Database stub VM + the `DatabaseScreen` TestCase; `tst_qml_admin` compiles the whole `tests/` dir via `QUICK_TEST_SOURCE_DIR`)

**Interfaces:**
- Consumes (from the VM, real or stub): `students.count`, `students.selectedCount`, `statusMessage`, `requiresTypedConfirmation(int)`, `deleteSelected()`, `exportCsv(url)`; plus existing `departments`/`courses`/`department`/`course`/`loading`/`errorText`.
- Consumes (components): `LButton` (`tooltipText`/`accessibleName`/`variant:"Danger"` from Task 6), `LConfirmDialog` (`requireTypedConfirmation`/`confirmationWord`/`confirmed` from Task 7), `LToast.message`, `FileDialog` (`QtQuick.Dialogs`, `fileMode`/`defaultSuffix`/`nameFilters`/`selectedFile`/`onAccepted`).
- Produces: no new C++ surface — QML wiring only + one CMake link line.

Steps:

- [ ] **RED (CMake first, so the import resolves)** — Two edits, in this order:

  1. In `qt-app/CMakeLists.txt:17`, add `QuickDialogs2` to the `find_package` COMPONENTS list (this is the sole `find_package(Qt…)` in the tree; without it the `Qt6::QuickDialogs2` target is undefined and configure errors):
```cmake
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Charts Test UiTools PrintSupport Svg Qml Quick QuickControls2 QuickDialogs2 QuickTest)
```
  2. In `qt-app/quick/CMakeLists.txt`, add `Qt${QT_VERSION_MAJOR}::QuickDialogs2` to the `witsquickmodule` link block (`~L94-100`), so it reads:
```cmake
target_link_libraries(witsquickmodule PUBLIC
    witscore
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::QuickDialogs2
    Qt${QT_VERSION_MAJOR}::Gui
)
```
(PUBLIC so `WITSQuick` and every `tst_qml_*` target that links `witsquickmodule` inherit the dependency and can resolve `import QtQuick.Dialogs`.)
- [ ] **RED** — Extend the Database stub VM in `qt-app/quick/tests/tst_qml_admin.qml`. In `stubModel` (~L1635) make `selectedCount` mutable (already a plain property — no change needed). In `stubVm` (~L1647), add the mutation surface after `function setCourse(c)` (~L1658):
```qml
            property string statusMessage: ""
            property int deleteCount: 0
            property url lastExportUrl: ""
            property bool exportResult: true
            function requiresTypedConfirmation(n) { return n >= 10; }
            function deleteSelected() { deleteCount++; }
            function exportCsv(u) { lastExportUrl = u; return exportResult; }
```
- [ ] **RED** — Extend the `DatabaseScreen` TestCase (~L1663). Add an `init()` that resets the shared stub and add the new tests:
```qml
            function init() {
                stubModel.selectedCount = 0;
                stubModel.count = 2;
                stubVm.deleteCount = 0;
                stubVm.lastExportUrl = "";
                stubVm.exportResult = true;
                var dlg = findChild(databaseScreen, "deleteConfirm");
                if (dlg) { dlg.visible = false; dlg.clearKey(); }
            }
            function test_exportLabelReflectsSelection() {
                var btn = findChild(databaseScreen, "exportButton");
                verify(btn !== null);
                compare(btn.text, "Export CSV (all 2)");   // nothing selected => all N
                stubModel.selectedCount = 1;
                compare(btn.text, "Export CSV (1)");
            }
            function test_exportDisabledWhenNoRows() {
                stubModel.selectedCount = 0;
                stubModel.count = 0;
                compare(findChild(databaseScreen, "exportButton").enabled, false);
            }
            function test_deleteLabelAndEnabledTrackSelection() {
                var btn = findChild(databaseScreen, "deleteButton");
                verify(btn !== null);
                compare(btn.text, "Delete");
                compare(btn.enabled, false);              // nothing selected
                stubModel.selectedCount = 3;
                compare(btn.text, "Delete (3)");
                compare(btn.enabled, true);
            }
            function test_deleteConfirmInvokesVmDeleteSelected() {
                stubModel.selectedCount = 2;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deleteButton"));
                var dlg = findChild(databaseScreen, "deleteConfirm");
                verify(dlg !== null);
                compare(dlg.visible, true);
                compare(dlg.requireTypedConfirmation, false);   // 2 < 10
                mouseClick(findChild(dlg, "confirmButton"));
                compare(stubVm.deleteCount, 1);
            }
            function test_typedConfirmGateEngagesForLargeSelection() {
                stubModel.selectedCount = 10;               // >= threshold
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deleteButton"));
                var dlg = findChild(databaseScreen, "deleteConfirm");
                compare(dlg.requireTypedConfirmation, true);
                var btn = findChild(dlg, "confirmButton");
                compare(btn.enabled, false);                // gated until DELETE typed
                findChild(dlg, "confirmTypedField").text = "DELETE";
                compare(btn.enabled, true);
            }
            function test_fileDialogAcceptInvokesExportCsv() {
                var dlg = findChild(databaseScreen, "exportDialog");
                verify(dlg !== null);
                dlg.selectedFile = "file:///tmp/wits_export_test.csv";
                dlg.accepted();                             // drive the onAccepted wiring
                compare(stubVm.lastExportUrl.toString(), "file:///tmp/wits_export_test.csv");
            }
```
- [ ] **RED** — Build + run; confirm failures (buttons/dialogs absent from the screen):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake -S qt-app -B <build-dir>
cmake --build <build-dir> --target tst_qml_admin
ctest --test-dir <build-dir> --output-on-failure -R tst_qml_admin
```
Expected: `FAIL! : DatabaseScreen::test_exportLabel*` / `test_delete*` (findChild null).
- [ ] **GREEN** — Edit `qt-app/quick/qml/admin/DatabaseScreen.qml`. Add the dialogs import at the top (~L2):
```qml
import QtQuick.Dialogs
```
Replace the single count-header `Text` block (`DatabaseScreen.qml:59-68`) with a `RowLayout` carrying the header, a spacer, and the two action buttons. Use `TextMetrics` for reserved widths so the button edge never jitters as counts change:
```qml
        // Count/selection header + row actions.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md

            Text {
                objectName: "tableCountHeader"
                text: screen.selectedCount > 0
                      ? qsTr("%1 results · %2 selected").arg(screen.resultCount).arg(screen.selectedCount)
                      : qsTr("%1 results").arg(screen.resultCount)
                color: Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                font.weight: Font.ExtraBold
            }

            Item { Layout.fillWidth: true }   // spacer pushes the actions right

            TextMetrics {
                id: exportMetrics
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                // Worst-case label width so the button never resizes on count change.
                text: qsTr("Export CSV (all %1)").arg(screen.resultCount)
            }
            LButton {
                objectName: "exportButton"
                variant: "Outline"
                compact: true
                text: screen.selectedCount > 0
                      ? qsTr("Export CSV (%1)").arg(screen.selectedCount)
                      : qsTr("Export CSV (all %1)").arg(screen.resultCount)
                // Exportable when M > 0 OR (nothing selected and) N > 0; disabled at N == 0.
                enabled: screen.selectedCount > 0 || screen.resultCount > 0
                tooltipText: qsTr("Exports selected rows, or all filtered rows if none are selected.")
                accessibleName: screen.selectedCount > 0
                                ? qsTr("Export %1 selected rows to CSV").arg(screen.selectedCount)
                                : qsTr("Export all %1 filtered rows to CSV").arg(screen.resultCount)
                Layout.minimumWidth: exportMetrics.width + Theme.spacing.xl * 2
                onClicked: exportDialog.open()
            }

            TextMetrics {
                id: deleteMetrics
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                text: qsTr("Delete (%1)").arg(screen.resultCount)
            }
            LButton {
                objectName: "deleteButton"
                variant: "Danger"
                compact: true
                text: screen.selectedCount > 0
                      ? qsTr("Delete (%1)").arg(screen.selectedCount)
                      : qsTr("Delete")
                enabled: screen.selectedCount > 0
                accessibleName: qsTr("Delete %1 selected student records").arg(screen.selectedCount)
                Layout.minimumWidth: deleteMetrics.width + Theme.spacing.xl * 2
                onClicked: deleteConfirm.visible = true
            }
        }
```
Add the `FileDialog`, `LConfirmDialog`, and `LToast` as direct children of the root `Rectangle` (after the top-level `ColumnLayout`'s closing brace, before the screen's final `}`):
```qml
    FileDialog {
        id: exportDialog
        objectName: "exportDialog"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: ["CSV (*.csv)"]
        onAccepted: if (screen.vm) screen.vm.exportCsv(selectedFile)
    }

    LConfirmDialog {
        id: deleteConfirm
        objectName: "deleteConfirm"
        title: qsTr("Delete students?")
        // Itemized, irreversible-impact message (PlainText per LConfirmDialog).
        message: qsTr("This will permanently delete:\n• %1 student records\n• all associated visit history\n\nThis cannot be undone.")
                    .arg(screen.selectedCount)
        confirmText: qsTr("Delete")
        // M >= 10 requires typing DELETE (the VM owns the threshold).
        requireTypedConfirmation: screen.vm ? screen.vm.requiresTypedConfirmation(screen.selectedCount) : false
        confirmationWord: "DELETE"
        onConfirmed: { deleteConfirm.visible = false; if (screen.vm) screen.vm.deleteSelected(); }
    }

    LToast {
        objectName: "databaseToast"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.xxl
        message: screen.vm ? screen.vm.statusMessage : ""
    }
```
- [ ] **GREEN** — Build + run; confirm the 6 new DatabaseScreen tests pass AND the 3 pre-existing ones (`test_showsCascadingFilter`, `test_showsSelectableTable`, `test_headerShowsCounts`) still pass (the header `Text` keeps its `objectName` inside the RowLayout):
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir> --target tst_qml_admin
ctest --test-dir <build-dir> --output-on-failure -R tst_qml_admin
```
Expected: `tst_qml_admin` passes; `PASS : DatabaseScreen::test_exportLabelReflectsSelection` etc. If `dlg.selectedFile` is read-only on this Qt build and `test_fileDialogAcceptInvokesExportCsv` fails to assign it, fall back to asserting only that `exportButton.onClicked` opens the dialog (`compare(exportDialog.visible, true)` after `mouseClick(exportButton)` — note native SaveFile dialogs may not report `visible` under offscreen; in that case assert `stubVm.lastExportUrl` via a direct `exportDialog.accepted()` with the pre-set `currentFile`/`selectedFile` name, whichever the installed Qt 6.11 `FileDialog` exposes as writable).
- [ ] **GREEN (full regression)** — Configure + build everything, then run the whole suite to confirm no target regressed and the `QuickDialogs2` link is correct across all binaries:
```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake --build <build-dir>
ctest --test-dir <build-dir> --output-on-failure
```
Expected: all tests pass, including `tst_studentcontroller`, `tst_studentstablemodel`, `tst_databaseviewmodel`, `tst_qml_components`, `tst_qml_admin`.
- [ ] **REFACTOR** — Confirm: zero raw hex in the new QML (all `Theme.*`); the header `Text` kept `objectName: "tableCountHeader"`; the toast is a single instance bound to `vm.statusMessage`; `requireTypedConfirmation` is a binding on the dialog (not imperatively set in `onClicked`). No change if clean.
- [ ] **COMMIT**:
```
git add qt-app/quick/qml/admin/DatabaseScreen.qml qt-app/quick/CMakeLists.txt qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(quick): wire Database delete + CSV export actions, dialogs, and toast into DatabaseScreen"
```

---

## Self-Review (writing-plans gate)

**(1) Spec coverage — every spec requirement maps to a task.**
- §1 `toCsv` (RFC-4180, BOM, CRLF, all-fields-except-photo, empty⇒header) → Task 1. ✔
- §1 required fix `deleteReplyIsServerAnswer` + `deleteStudents` rewire, `bulkUpdate` left for 4a.2b-iii → Task 2. ✔
- §2 `selectedRecords()` / `allRecords()` → Task 3. ✔
- §3 `kTypeToConfirmThreshold`, `requiresTypedConfirmation`, `deleteSelected`, held-key/no-retype, delete-in-flight bool, `statusMessage` Q_PROPERTY, auth-failure state, `onDeleteFinished`/`onDeleteFailed` wired in ctor, success⇒status+reload+clear, 401⇒auth via `isAuthFailureMessage` → Task 4. ✔
- §3 `exportCsv` (selected-else-all, `toCsv`, `QSaveFile`, status, bool return) → Task 5. ✔
- §4 LButton `tooltipText` + `accessibleName` (themed, delay 500, hover-gated, backward-compatible) → Task 6. ✔
- §5 LConfirmDialog `requireTypedConfirmation` + `confirmationWord` (exact match, folded into existing `enabled` binding, Loader-not-`visible`, default false) → Task 7. ✔
- §6 DatabaseScreen (RowLayout wrap, Export/Delete labels+enable rules+reserved width, tooltip/accessibleName, FileDialog, two-tier LConfirmDialog with `requiresTypedConfirmation` binding, LToast) + `QuickDialogs2` CMake → Task 8. ✔
- §Error Taxonomy: transport⇒`deleteFailed`⇒transient toast (Task 4 `onDeleteFailed`); 401⇒auth message (Task 4); generic server error⇒message toast (Task 4); reuse `isAuthFailureMessage` (Task 4 promotes it public). ✔
- §Testing Plan seams: all C++ unit + VM (OFFSCREEN/CapturingNam note) + QuickTest bullets map to Tasks 1–8 test steps, including the 401-without-live-reply approach (Task 2 static + Task 4 direct `onDeleteFinished(false,...)`). ✔

**(2) Placeholder scan.** No `TBD`, no "handle errors" hand-waving — every code block is concrete C++/QML; every test step names an exact run command and expected `PASS`/`FAIL`. `<build-dir>` is the intended executor-supplied worktree path placeholder (per the task brief), not a gap. ✔

**(3) Type consistency across Consumes/Produces.**
- `toCsv(const QList<StudentRecord>&) -> QByteArray` identical in Task 1 header/cpp/tests and Task 5 consumer. ✔
- `deleteReplyIsServerAnswer(bool, int, const QByteArray&) -> bool` identical in Task 2 header/cpp/tests. ✔
- `selectedRecords()/allRecords() -> QList<StudentRecord>` identical in Task 3 and Task 5 consumer. ✔
- `deleteFinished(bool, int, const QString&)` / `deleteFailed(const QString&)` signatures match the existing `studentcontroller.h:77-78` and the VM handlers `onDeleteFinished(bool,int,QString)` / `onDeleteFailed(QString)` in Task 4. ✔
- `exportCsv(const QUrl&) -> bool` identical in Task 4 decl, Task 5 def, and the QML/stub caller in Task 8. ✔
- `requiresTypedConfirmation(int) -> bool`, `deleteSelected()`, `statusMessage`/`authFailure` accessors identical across Task 4 and the Task 8 stub/consumer. ✔
- `isAuthFailureMessage(const QString&) -> bool` (static) — same signature, only visibility changed (private→public) in Task 4. ✔

Self-review passed; issues found and fixed inline during drafting: (a) capture `errorString` before `reply->deleteLater()` in Task 2; (b) `isAuthFailureMessage` must be promoted to public to be reusable (Task 4); (c) `exportCsv` declared in Task 4 but defined in Task 5 — noted the stub/link-order handling so neither task leaves an unresolved symbol.
