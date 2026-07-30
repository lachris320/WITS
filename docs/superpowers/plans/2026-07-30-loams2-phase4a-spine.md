# LOAMS 2.0 Phase 4a — Auth Spine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Guard the four destructive student/department endpoints with the shipped `requireAdminAuth` helper, harmonize `delete_students`/`bulk_update_students` to the house FORM-post style so the guard applies uniformly, move the shared `StudentController` delete/bulk request-building from JSON to FORM-with-`admin_key` (with the request-side tests it never had), thread the already-retained legacy `m_adminKey` into the newly-guarded calls, and deploy lockstep — leaving both the Quick and legacy clients working against the guarded backend.

**Architecture:** This is the first of three increments of Phase 4 track 4a (spec `docs/superpowers/specs/2026-07-19-loams2-phase4-admin-part2-design.md`). It builds **no UI** — it is the backend + shared-controller + legacy foundation that the Database CRUD UI (increment 2) and Import (increment 3) ride on. The one behavioral change to a shared class (`StudentController`) is covered by new request-assembly tests, since that path is driven by both the future `DatabaseViewModel` and the existing legacy `adminwindow.cpp`.

**Tech Stack:** Qt 6.11 C++17, QtTest under CTest (`wits_add_qttest`), CMake + Ninja + MinGW; PHP 5.x-compatible endpoints under `deliverables/loams_api/` deployed to `C:/xampp/htdocs/loams_api/`.

## Global Constraints

- **`admin_key` always travels as a FORM field** (`application/x-www-form-urlencoded`), read server-side via `$_POST['admin_key']` — never JSON body, never query string. (spec §5.3)
- **The auth guard is a BREAKING contract change** (missing/invalid key → HTTP 401). Deploy MUST be lockstep with the client + legacy edits (§5.4) or any not-yet-updated caller 401s the instant an endpoint deploys.
- **No secrets/PII in commits, fixtures, or messages.** Synthetic data only. `admin_key` values in tests are fakes (e.g. `"test-key"`).
- **Backend house style:** `include "db.php";` (gives `$conn`) + `include "auth_helper.php";` then `requireAdminAuth($conn);`. No hardcoded `new mysqli(...)`. PHP 5.x-compatible (`isset()` not `??`).
- **Every edited endpoint is deployed to `C:/xampp/htdocs/loams_api/` and byte-verified** (EOL-normalized) against its `deliverables/loams_api/` master (§5.4). Keep a `.pre-4a.bak` of each until the success path is confirmed.
- **Commit via the `commit` skill; never `git add -A`; never `--no-verify`.** Build dir is a SHORT path (e.g. `C:/b/loams-4a`) to dodge Windows MAX_PATH. Tools are not on PATH — prepend `C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja` every invocation.
- **Regression floor:** the full existing ctest suite stays green (Phase 3's 32+ targets plus everything added since). No new build warnings.
- **This increment ships NO QML and NO new ViewModel.** `DatabaseViewModel` is increment 2.

---

## File Structure

**Backend (`deliverables/loams_api/`, each also deployed to xampp):**
- Modify `deactivate_department.php` — add the auth guard (2 lines).
- Modify `delete_department.php` — add the auth guard (2 lines).
- Modify `delete_students.php` — rewrite to FORM + `db.php` + guard + transaction + visit cascade.
- Modify `bulk_update_students.php` — rewrite to FORM + `db.php` + guard; `students` arrives as a JSON string in one form field.

**Client — shared controller (`qt-app/core/`):**
- Modify `studentcontroller.h` — add a trailing `const QString &adminKey` parameter to `deleteStudents` / `bulkUpdateStudents`.
- Modify `studentcontroller.cpp` — both methods build an `application/x-www-form-urlencoded` body carrying `admin_key`; `deleteStudents` sends repeated `school_ids[]` items; `bulkUpdateStudents` sends the student array as a JSON string in a single `students` field.

**Client — test infrastructure (`qt-app/testsupport/`):**
- Create `capturingnam.h` / `capturingnam.cpp` — a `QNetworkAccessManager` subclass that captures the operation, request headers, and outgoing body of the next POST and returns a canned reply, so request-assembly can be asserted offline.

**Client — tests (`qt-app/tests/`):**
- Modify `tst_studentcontroller.cpp` — add request-assembly tests for the harmonized `deleteStudents` / `bulkUpdateStudents` (body shape + `admin_key` + content-type), using the capturing NAM.
- Modify `qt-app/CMakeLists.txt` (or the test's `wits_add_qttest` registration) so `tst_studentcontroller` links the new `capturingnam` translation unit.

**Client — legacy (`qt-app/adminwindow.cpp`):**
- Modify the `deactivate_department` POST (~L573) — add `admin_key` + adopt the `isServerAnswer` 401 handling.
- Modify the `delete_department` POST (~L681) — same.
- Modify the two shared-controller calls — `bulkUpdateStudents(...)` (~L2189) and `deleteStudents(...)` (the delete-records path) — to pass `m_adminKey`.

**Contract record (`docs/superpowers/contracts/`):**
- Create `2026-07-30-phase4a-endpoints.md` — the before/after request+response fixture for each of the four endpoints, including the negative-auth (401) case (§5.3, §6.3).

**Interfaces produced for later increments:**
- `StudentController::deleteStudents(const QStringList &schoolIds, const QString &adminKey)`
- `StudentController::bulkUpdateStudents(const QList<StudentRecord> &updates, const QString &adminKey)`
- Guarded endpoints: `delete_students.php`, `bulk_update_students.php`, `deactivate_department.php`, `delete_department.php` all require `admin_key` and 401 without it. `delete_students.php` now cascades `library_visits` in a transaction.
- Testsupport: `CapturingNam` (reusable request-assembly harness for increment 2's `DatabaseViewModel` controller calls).

---

## Task 1: Capture the endpoint contract fixtures (before any edit)

Per spec §5.3/§8 — record the CURRENT request/response shape of each endpoint before hardening, plus the intended after-shape and the negative-auth case. This is the contract the C++ tests and the deploy byte-verify are checked against. No code yet — this task locks the contract.

**Files:**
- Create: `docs/superpowers/contracts/2026-07-30-phase4a-endpoints.md`

- [ ] **Step 1: Write the contract record**

Create the file with, for each of the four endpoints, three blocks: **BEFORE** (current request body + response), **AFTER** (new FORM request incl. `admin_key` + response), and **NEGATIVE AUTH** (no/invalid key → HTTP 401 body). Use the verified current sources as the BEFORE. Exact content:

````markdown
# Phase 4a endpoint contract (captured 2026-07-30, before hardening)

`admin_key` always a FORM field; guard = `requireAdminAuth($conn)` → 401 `{"status":"error","message":"Admin authentication required"}` (missing) / `{"status":"error","message":"Invalid admin key"}` (wrong).

## delete_students.php
BEFORE (request): `application/json` body `{"school_ids":["2023-001","2023-002"]}`; opens hardcoded `new mysqli("localhost","root","","wits_app")`; no visit cascade.
BEFORE (response): `{"status":"success","deleted":<n>}`.
AFTER (request): `application/x-www-form-urlencoded` body `school_ids%5B%5D=2023-001&school_ids%5B%5D=2023-002&admin_key=<key>`; `include db.php` + `auth_helper.php`; wrapped in a transaction that also deletes `library_visits WHERE student_id IN (...)`.
AFTER (response): unchanged `{"status":"success","deleted":<n>}`.
NEGATIVE: no `admin_key` → 401; DB untouched (guard rejects before any DELETE).

## bulk_update_students.php
BEFORE (request): `application/json` body `{"students":[{school_id,code,name,department,course,year_level,gender,status}, ...]}`.
BEFORE (response): `{"status":"success","updated":<n>,"errors":[...]}` or `{"status":"error","message":...}`.
AFTER (request): `application/x-www-form-urlencoded`; `admin_key=<key>` + `students=<JSON-string of the same array>` (single field; server `json_decode`s it).
AFTER (response): unchanged.
NEGATIVE: no `admin_key` → 401; no UPDATE runs.

## deactivate_department.php
BEFORE (request): FORM `department=<dept>`. Response `{"status":"success","message":...}`.
AFTER (request): FORM `department=<dept>&admin_key=<key>`. Response unchanged.
NEGATIVE: no `admin_key` → 401.

## delete_department.php
BEFORE (request): FORM `department=<dept>` (already cascades visits in a transaction). Response `{"status":"success","message":...}`.
AFTER (request): FORM `department=<dept>&admin_key=<key>`. Response unchanged.
NEGATIVE: no `admin_key` → 401.
````

- [ ] **Step 2: Commit**

Use the `commit` skill. Message: `docs(phase4a): capture destructive-endpoint contract before hardening`.

---

## Task 2: Guard the two department endpoints

Smallest, lowest-risk backend edit — both already read `$_POST['department']` and `include "db.php"` (so `$conn` exists). Adding the guard is a 2-line change each.

**Files:**
- Modify: `deliverables/loams_api/deactivate_department.php`
- Modify: `deliverables/loams_api/delete_department.php`

- [ ] **Step 1: Add the guard to `deactivate_department.php`**

Immediately after `include "db.php";`, insert the guard so it runs before any query:

```php
<?php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401s on missing/invalid admin_key before any mutation

if ($_SERVER["REQUEST_METHOD"] === "POST") {
```

(Leave the rest of the file unchanged.)

- [ ] **Step 2: Add the guard to `delete_department.php`**

After its `include "db.php";`:

```php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401s on missing/invalid admin_key before any mutation

if ($_SERVER["REQUEST_METHOD"] === "POST") {
```

- [ ] **Step 3: Lint both**

Run: `php -l deliverables/loams_api/deactivate_department.php && php -l deliverables/loams_api/delete_department.php`
Expected: `No syntax errors detected` for both. (If `php` is not on PATH, use the XAMPP php: `C:/xampp/php/php.exe -l <file>`.)

- [ ] **Step 4: Commit**

`commit` skill. Message: `feat(api): require admin auth on deactivate/delete_department`. Body: note the guard is the shipped `requireAdminAuth` helper, breaking (401 without key), deployed in Task 10 lockstep with the legacy key-threading (Task 8).

---

## Task 3: Harmonize `delete_students.php` (FORM + guard + transaction + cascade)

Rewrite from JSON+hardcoded-mysqli to the house style, add the guard, wrap in a transaction, and cascade-delete `library_visits` (today it orphans them). The response shape is unchanged.

**Files:**
- Modify: `deliverables/loams_api/delete_students.php`

- [ ] **Step 1: Rewrite the endpoint**

Replace the entire file with:

```php
<?php
header('Content-Type: application/json');
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401s before any read of the payload / any DELETE

// school_ids arrives as a repeated form field: school_ids[]=A&school_ids[]=B
$schoolIds = isset($_POST['school_ids']) ? $_POST['school_ids'] : null;
if (!is_array($schoolIds) || count($schoolIds) === 0) {
    echo json_encode(['status' => 'error', 'message' => 'Invalid data']);
    exit;
}

$conn->begin_transaction();
try {
    // 1. Cascade: delete the affected students' visit history first.
    $delVisits = $conn->prepare("DELETE FROM library_visits WHERE student_id = ?");
    $delStudent = $conn->prepare("DELETE FROM students WHERE school_id = ?");

    $deleted = 0;
    foreach ($schoolIds as $id) {
        $delVisits->bind_param("s", $id);
        $delVisits->execute();

        $delStudent->bind_param("s", $id);
        if ($delStudent->execute() && $delStudent->affected_rows > 0) {
            $deleted++;
        }
    }

    $conn->commit();
    echo json_encode(['status' => 'success', 'deleted' => $deleted]);

    $delVisits->close();
    $delStudent->close();
} catch (Exception $e) {
    $conn->rollback();
    echo json_encode(['status' => 'error', 'message' => 'Failed to delete students.']);
}

$conn->close();
?>
```

Notes for the implementer: `library_visits.student_id` joins on `students.school_id` (confirmed by `delete_department.php`'s cascade, which joins `lv.student_id = s.school_id`). Deleting visits first avoids leaving orphans if the student delete succeeds; both run inside one transaction so a failure rolls back cleanly. `affected_rows` on the STUDENT delete preserves the existing "deleted count" semantics.

- [ ] **Step 2: Lint**

Run: `php -l deliverables/loams_api/delete_students.php`
Expected: `No syntax errors detected`.

- [ ] **Step 3: Commit**

`commit` skill. Message: `feat(api): harmonize delete_students to FORM+auth, cascade visits`. Body: FORM `school_ids[]`+`admin_key`, house `db.php`, transaction + `library_visits` cascade (was orphaning), response unchanged.

---

## Task 4: Harmonize `bulk_update_students.php` (FORM + guard; `students` as JSON string)

The 8-field student array does not urlencode cleanly, so it travels as a JSON string in a single `students` form field (`admin_key` a sibling field). The existing update SQL, per-row error collection, and transaction stay.

**Files:**
- Modify: `deliverables/loams_api/bulk_update_students.php`

- [ ] **Step 1: Replace the header + input-decode block**

Change the top of the file from the JSON-body read to the FORM + guard + `students`-field decode. Replace everything from `<?php` down to and including the `$conn = new mysqli(...)` block and its `connect_error` check with:

```php
<?php
header('Content-Type: application/json');
error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);

include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401s before any read of the payload / any UPDATE

// The student array is a JSON string in a single form field (the 8-field
// objects do not urlencode cleanly); admin_key is a sibling form field.
$studentsJson = isset($_POST['students']) ? $_POST['students'] : '';
$data = array('students' => json_decode($studentsJson, true));

if (!isset($data['students']) || !is_array($data['students'])) {
    echo json_encode(array(
        'status' => 'error',
        'message' => 'Invalid data format. Expected students array.'
    ));
    exit;
}

if (count($data['students']) === 0) {
    echo json_encode(array(
        'status' => 'error',
        'message' => 'No students provided for update.'
    ));
    exit;
}
```

Everything from `$updated = 0;` onward (the `autocommit(FALSE)`, the prepared `UPDATE`, the per-row loop, the commit, the response) stays exactly as-is — it already reads `$data['students']`, which now comes from the decoded form field. **Delete** the old `$conn = new mysqli("localhost","root","","wits_app");` block and its `connect_error` check (now provided by `db.php`).

- [ ] **Step 2: Lint**

Run: `php -l deliverables/loams_api/bulk_update_students.php`
Expected: `No syntax errors detected`.

- [ ] **Step 3: Commit**

`commit` skill. Message: `feat(api): harmonize bulk_update_students to FORM+auth`. Body: `admin_key` + `students` (JSON string) form fields, house `db.php`, guard; update logic unchanged.

---

## Task 5: Create the request-capturing NAM test harness

`StudentController` request-building has never been tested (`tst_studentcontroller.cpp` covers only response parsers). To assert the new FORM body offline, add a `QNetworkAccessManager` subclass that captures the next POST's operation, headers, and body, and returns a canned reply.

**Files:**
- Create: `qt-app/testsupport/capturingnam.h`
- Create: `qt-app/testsupport/capturingnam.cpp`

**Interfaces:**
- Produces: `class CapturingNam : public QNetworkAccessManager` with public members after a POST: `QByteArray lastBody;`, `QString lastContentType;`, `QUrl lastUrl;`, `QNetworkAccessManager::Operation lastOp;`. Constructed with an optional canned response `QByteArray` (default `{"status":"success"}`).

- [ ] **Step 1: Write `capturingnam.h`**

```cpp
#ifndef CAPTURINGNAM_H
#define CAPTURINGNAM_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QUrl>

// Test-only QNetworkAccessManager that records the next request's operation,
// URL, Content-Type, and full outgoing body, then returns a reply that finishes
// immediately with a canned payload. Lets request-assembly be asserted with no
// live network (project house rule: no live network in unit tests).
class CapturingNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit CapturingNam(const QByteArray &cannedResponse =
                              QByteArrayLiteral("{\"status\":\"success\"}"),
                          QObject *parent = nullptr);

    QNetworkAccessManager::Operation lastOp = QNetworkAccessManager::UnknownOperation;
    QUrl lastUrl;
    QString lastContentType;
    QByteArray lastBody;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    QByteArray m_canned;
};

#endif // CAPTURINGNAM_H
```

- [ ] **Step 2: Write `capturingnam.cpp`**

```cpp
#include "capturingnam.h"

#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {
// Minimal QNetworkReply that reports success and hands back canned bytes on
// readAll(), emitting finished() on the next event-loop turn (so consumers that
// connect to finished() after createRequest() returns still receive it).
class CannedReply : public QNetworkReply
{
public:
    CannedReply(const QByteArray &data, QObject *parent) : QNetworkReply(parent)
    {
        m_buf.setData(data);
        m_buf.open(QIODevice::ReadOnly);
        setOpenMode(QIODevice::ReadOnly);
        setError(QNetworkReply::NoError, QString());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        QTimer::singleShot(0, this, [this]() {
            emit finished();
        });
    }
    void abort() override {}
    qint64 bytesAvailable() const override
    {
        return m_buf.bytesAvailable() + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char *data, qint64 maxlen) override { return m_buf.read(data, maxlen); }
private:
    QBuffer m_buf;
};
} // namespace

CapturingNam::CapturingNam(const QByteArray &cannedResponse, QObject *parent)
    : QNetworkAccessManager(parent), m_canned(cannedResponse)
{}

QNetworkReply *CapturingNam::createRequest(Operation op, const QNetworkRequest &request,
                                           QIODevice *outgoingData)
{
    lastOp = op;
    lastUrl = request.url();
    lastContentType =
        request.header(QNetworkRequest::ContentTypeHeader).toString();
    lastBody = outgoingData ? outgoingData->readAll() : QByteArray();
    return new CannedReply(m_canned, this);
}
```

- [ ] **Step 3: Register the harness with the controller test target**

CORRECTION (advisor-verified): `tst_studentcontroller` is NOT a `wits_add_qttest` target — it is a **raw `qt_add_executable` block** in `qt-app/tests/CMakeLists.txt` (around lines 106-121), alongside a `target_include_directories` and a `set_tests_properties`. Extend that existing block: add the harness `.cpp` **and** `.h` (list the `.h` so AUTOMOC processes its `Q_OBJECT`) to the sources, and add `${CMAKE_SOURCE_DIR}/testsupport` to its include dirs. `Qt::Network` is already linked on this target — nothing else to add. Example (adapt to the real block):

```cmake
qt_add_executable(tst_studentcontroller
    tst_studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/core/studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/core/studentcontroller.h
    ${CMAKE_SOURCE_DIR}/core/studentdata.h
    ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
    ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h)
# ... (keep the existing link + AUTOMOC + add_test lines) ...
target_include_directories(tst_studentcontroller PRIVATE
    ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
```

Do NOT migrate it to `wits_add_qttest` (that macro would pull in the settings-isolation TU and does NOT auto-link Qt::Network — the raw-block edit is lower-risk). Also add `setFinished(true);` inside `CannedReply`'s singleShot lambda before `emit finished();` (harmless here, correct for the increment-2 reuse).

- [ ] **Step 4: Build the (unchanged) test to prove the harness compiles and links**

Run (PowerShell, PATH prepended):
```
cmake -S qt-app -B C:/b/loams-4a -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-4a --target tst_studentcontroller
```
Expected: compiles and links clean (the harness is unused so far).

- [ ] **Step 5: Commit**

`commit` skill. Message: `test(support): add CapturingNam request-assembly harness`.

---

## Task 6: `StudentController::deleteStudents` → FORM + `admin_key` (RED first)

Add the `adminKey` parameter and switch the body from JSON to `application/x-www-form-urlencoded` with repeated `school_ids[]` items + `admin_key`. Write the request-assembly test first.

**Files:**
- Modify: `qt-app/tests/tst_studentcontroller.cpp`
- Modify: `qt-app/core/studentcontroller.h:? ` (the `deleteStudents` declaration)
- Modify: `qt-app/core/studentcontroller.cpp:216-245`

**Interfaces:**
- Produces: `void deleteStudents(const QStringList &schoolIds, const QString &adminKey);`

- [ ] **Step 1: Write the failing request-assembly test**

Add to the `private slots:` block: `void deleteStudents_buildsFormBodyWithAdminKey();` and, in the includes, add `#include "capturingnam.h"`, `#include <QUrlQuery>`, `#include <QSignalSpy>`. Then add the test body:

```cpp
void TestStudentController::deleteStudents_buildsFormBodyWithAdminKey()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    ctrl.deleteStudents(QStringList() << "2023-001" << "2023-002", "test-key");

    // Form-encoded POST (not JSON), carrying admin_key + repeated school_ids[].
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));

    const QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QCOMPARE(q.queryItemValue("admin_key"), QStringLiteral("test-key"));
    const QStringList ids = q.allQueryItemValues("school_ids[]", QUrl::FullyDecoded);
    QCOMPARE(ids.size(), 2);
    QVERIFY(ids.contains("2023-001"));
    QVERIFY(ids.contains("2023-002"));
    QVERIFY(!nam.lastBody.contains("{"));   // not a JSON body
}
```

- [ ] **Step 2: Run it — verify it FAILS to compile**

Run: `cmake --build C:/b/loams-4a --target tst_studentcontroller`
Expected: FAIL — `deleteStudents` does not take 2 arguments (the new `adminKey` param doesn't exist yet).

- [ ] **Step 3: Update the declaration**

In `studentcontroller.h`, change the `deleteStudents` declaration to:

```cpp
    // Async — result arrives via deleteFinished / deleteFailed. adminKey is
    // sent as a FORM field (delete_students.php is requireAdminAuth-guarded).
    void deleteStudents(const QStringList &schoolIds, const QString &adminKey);
```

- [ ] **Step 4: Rewrite the implementation**

In `studentcontroller.cpp`, replace the body of `deleteStudents` (currently building a JSON `school_ids` array) with a form-encoded body. Add `#include <QUrlQuery>` at the top if not present:

```cpp
void StudentController::deleteStudents(const QStringList &schoolIds, const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("delete_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery body;
    for (const QString &id : schoolIds)
        body.addQueryItem(QStringLiteral("school_ids[]"), id);
    body.addQueryItem(QStringLiteral("admin_key"), adminKey);

    const int requestedCount = schoolIds.size();
    QNetworkReply *reply =
        m_nam->post(request, body.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestedCount]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit deleteFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        QString message;
        const bool ok = parseDeleteResponse(resp, message);
        emit deleteFinished(ok, requestedCount, message);
    });
}
```

- [ ] **Step 5: Run the test — verify it PASSES**

Run: `cmake --build C:/b/loams-4a --target tst_studentcontroller && ctest --test-dir C:/b/loams-4a -R studentcontroller --output-on-failure`
Expected: PASS (all pre-existing parser tests + the new request test).

- [ ] **Step 6: Commit**

`commit` skill. Message: `feat(studentcontroller): deleteStudents posts FORM with admin_key`. Body: JSON→form-encoded `school_ids[]`+`admin_key` to match the guarded/harmonized endpoint; adds the first request-assembly test for this shared controller (drives both Quick + legacy).

---

## Task 7: `StudentController::bulkUpdateStudents` → FORM + `admin_key` (RED first)

Same treatment: add `adminKey`, send the student array as a JSON string in a `students` form field.

**Files:**
- Modify: `qt-app/tests/tst_studentcontroller.cpp`
- Modify: `qt-app/core/studentcontroller.h` (the `bulkUpdateStudents` declaration)
- Modify: `qt-app/core/studentcontroller.cpp:177-214`

**Interfaces:**
- Produces: `void bulkUpdateStudents(const QList<StudentRecord> &updates, const QString &adminKey);`

- [ ] **Step 1: Write the failing test**

Add slot `void bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey();` and body:

```cpp
void TestStudentController::bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    StudentRecord r;
    r.schoolId = "2023-001"; r.code = "C1"; r.name = "Juan Cruz";
    r.department = "CCS"; r.course = "BSIT"; r.yearLevel = "2";
    r.gender = "Male"; r.status = "Active";
    ctrl.bulkUpdateStudents(QList<StudentRecord>() << r, "test-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));

    const QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QCOMPARE(q.queryItemValue("admin_key"), QStringLiteral("test-key"));

    // students is a JSON string in one field; decode it back and check a field.
    const QString studentsJson = q.queryItemValue("students", QUrl::FullyDecoded);
    const QJsonArray arr = QJsonDocument::fromJson(studentsJson.toUtf8()).array();
    QCOMPARE(arr.size(), 1);
    QCOMPARE(arr.at(0).toObject().value("school_id").toString(), QStringLiteral("2023-001"));
    QCOMPARE(arr.at(0).toObject().value("year_level").toString(), QStringLiteral("2"));
}
```

(Add `#include <QJsonArray>`, `<QJsonDocument>`, `<QJsonObject>` to the test if not already present.)

- [ ] **Step 2: Run it — verify it FAILS to compile** (`bulkUpdateStudents` takes 1 arg).

Run: `cmake --build C:/b/loams-4a --target tst_studentcontroller` → FAIL.

- [ ] **Step 3: Update the declaration**

```cpp
    // Async — result arrives via bulkUpdateFinished / bulkUpdateFailed. The
    // updates array is sent as a JSON string in a single `students` FORM field
    // (8-field objects don't urlencode cleanly); adminKey is a sibling field.
    void bulkUpdateStudents(const QList<StudentRecord> &updates, const QString &adminKey);
```

- [ ] **Step 4: Rewrite the implementation**

```cpp
void StudentController::bulkUpdateStudents(const QList<StudentRecord> &updates,
                                           const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("bulk_update_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QJsonArray studentsArray;
    for (const StudentRecord &rec : updates) {
        QJsonObject student;
        student["school_id"]  = rec.schoolId;
        student["code"]       = rec.code;
        student["name"]       = rec.name;
        student["department"] = rec.department;
        student["course"]     = rec.course;
        student["year_level"] = rec.yearLevel;
        student["gender"]     = rec.gender;
        student["status"]     = rec.status;
        studentsArray.append(student);
    }
    const QByteArray studentsJson =
        QJsonDocument(studentsArray).toJson(QJsonDocument::Compact);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("students"), QString::fromUtf8(studentsJson));
    body.addQueryItem(QStringLiteral("admin_key"), adminKey);

    QNetworkReply *reply =
        m_nam->post(request, body.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit bulkUpdateFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        emit bulkUpdateFinished(parseBulkUpdateResponse(resp));
    });
}
```

- [ ] **Step 5: Run the test — verify it PASSES**

Run: `cmake --build C:/b/loams-4a --target tst_studentcontroller && ctest --test-dir C:/b/loams-4a -R studentcontroller --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

`commit` skill. Message: `feat(studentcontroller): bulkUpdateStudents posts FORM with admin_key`.

---

## Task 8: Thread `m_adminKey` into the legacy department POSTs

The legacy window already retains `m_adminKey` and already keys `reset_visits` with the `isServerAnswer` 401 pattern (`adminwindow.cpp:617-654`). Bring `deactivate_department` and `delete_department` up to the same standard so they don't 401 after deploy.

**Files:**
- Modify: `qt-app/adminwindow.cpp` (the `deactivate_department` POST ~L573; the `delete_department` POST ~L681)

- [ ] **Step 1: Add `admin_key` to the `deactivate_department` POST**

In the `deactivate_department.php` handler, after `postData.addQueryItem("department", dept);`:

```cpp
        QUrlQuery postData;
        postData.addQueryItem("department", dept);
        // deactivate_department.php is now requireAdminAuth-guarded.
        postData.addQueryItem("admin_key", m_adminKey);
```

- [ ] **Step 2: Adopt the `isServerAnswer` 401 handling in that reply lambda**

Replace the `deactivate_department` reply lambda's error handling with the same shape `reset_visits` uses (so a 401 shows the server message, not a transport error). Change:

```cpp
        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Error", reply->errorString());
                reply->deleteLater();
                return;
            }
            QByteArray resp = reply->readAll();
            reply->deleteLater();
            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isObject() && doc["status"].toString() == "success") {
                QMessageBox::information(this, "Success", doc["message"].toString());
            } else {
                QMessageBox::warning(this, "Failed", doc["message"].toString());
            }
        });
```

to:

```cpp
        connect(reply, &QNetworkReply::finished, this, [=]() {
            const QByteArray resp = reply->readAll();
            const bool replyHadError = reply->error() != QNetworkReply::NoError;
            const int httpStatus = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString transportError = reply->errorString();
            reply->deleteLater();

            if (!isServerAnswer(replyHadError, httpStatus, resp)) {
                QMessageBox::critical(this, "Error", transportError);
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isObject() && doc["status"].toString() == "success") {
                QMessageBox::information(this, "Success", doc["message"].toString());
            } else {
                const QString message = doc.isObject() ? doc["message"].toString() : QString();
                QMessageBox::warning(this, "Failed",
                                     message.isEmpty()
                                         ? QStringLiteral("The server rejected the request.")
                                         : message);
            }
        });
```

- [ ] **Step 3: Do the same for the `delete_department` POST**

Add `postData.addQueryItem("admin_key", m_adminKey);` after its `department` item (~L682), and replace its reply lambda's error handling with the identical `isServerAnswer` block from Step 2.

- [ ] **Step 4: Build the legacy app**

Run: `cmake --build C:/b/loams-4a --target WITS`
Expected: compiles clean, no new warnings. (`isServerAnswer` already exists in `adminwindow.cpp` — reused, not redefined.)

- [ ] **Step 5: Commit**

`commit` skill. Message: `feat(legacy-admin): key + 401-aware handling on dept deactivate/delete`. Body: thread the retained `m_adminKey` into the two department POSTs and adopt `isServerAnswer` so a guard 401 surfaces the server message, matching the `reset_visits` path.

---

## Task 9: Pass `m_adminKey` into the legacy shared-controller calls

The signature change from Tasks 6–7 broke the legacy `m_studentController->bulkUpdateStudents(...)` call (`adminwindow.cpp:2203`, inside the wrapper `adminWindow::bulkUpdateStudents` whose own self-call is at `:2189`) and `m_studentController->deleteStudents(...)` (`:2282`). Thread the retained key (`m_adminKey`, captured at login via `setAdminKey`, `adminwindow.h:67`) through. Do NOT change the wrapper `adminWindow::bulkUpdateStudents(updates)` signature — only the controller call it makes.

**Files:**
- Modify: `qt-app/adminwindow.cpp:2203` (the `m_studentController->bulkUpdateStudents(...)` call); `qt-app/adminwindow.cpp:2282` (the `m_studentController->deleteStudents(...)` call)

- [ ] **Step 1: Confirm the build is currently RED from the signature change**

Run: `cmake --build C:/b/loams-4a --target WITS`
Expected: FAIL — `bulkUpdateStudents`/`deleteStudents` "candidate expects 2 arguments, 1 provided". (This confirms these are the only remaining legacy callers.)

- [ ] **Step 2: Update the `bulkUpdateStudents` call**

At `adminwindow.cpp:2203`, change `m_studentController->bulkUpdateStudents(updates);` to:

```cpp
    m_studentController->bulkUpdateStudents(updates, m_adminKey);
```

(The wrapper `adminWindow::bulkUpdateStudents(const QList<StudentRecord> &updates)` at `:2201` keeps its signature; only the controller call inside it changes.)

- [ ] **Step 3: Update the `deleteStudents` call**

At `adminwindow.cpp:2282`, change `m_studentController->deleteStudents(selectedIds);` to:

```cpp
        m_studentController->deleteStudents(selectedIds, m_adminKey);
```

- [ ] **Step 4: Build — verify GREEN**

Run: `cmake --build C:/b/loams-4a --target WITS`
Expected: compiles clean, no new warnings.

- [ ] **Step 5: Full suite regression**

Run: `cmake --build C:/b/loams-4a && ctest --test-dir C:/b/loams-4a --output-on-failure`
Expected: 100% pass (the whole existing suite + the two new controller request tests).

- [ ] **Step 6: Commit**

`commit` skill. Message: `feat(legacy-admin): pass admin_key to shared delete/bulk calls`. Body: the shared `StudentController` now requires the key (Tasks 6–7); thread the retained `m_adminKey` so the legacy rollback's delete/bulk paths keep working post-deploy.

---

## Task 10: Deploy the four endpoints lockstep + verify

All client + legacy edits are now in. Deploy the guarded/harmonized endpoints to xampp, byte-verify, and confirm the negative-auth path. **Do this only after Tasks 2–9 are committed** (lockstep — the moment an endpoint deploys, an un-keyed caller 401s).

**Files:**
- Deploy: `deactivate_department.php`, `delete_department.php`, `delete_students.php`, `bulk_update_students.php` → `C:/xampp/htdocs/loams_api/`

- [ ] **Step 1: Back up the current web-root copies**

Run (PowerShell), for each of the four files:
```
Copy-Item C:/xampp/htdocs/loams_api/<name>.php C:/xampp/htdocs/loams_api/<name>.php.pre-4a.bak
```

- [ ] **Step 2: Copy master sources into the web root**

Copy each of the four `deliverables/loams_api/*.php` to `C:/xampp/htdocs/loams_api/`. Confirm `auth_helper.php` and `db.php` are already present in the web root (they are — deployed in 4c); if `auth_helper.php` is somehow absent, deploy it too, or every guarded call 401s.

- [ ] **Step 3: Byte-verify (EOL-normalized) each deployed copy matches master**

For each file, compare ignoring EOL. Example (PowerShell):
```
$a = (Get-Content deliverables/loams_api/delete_students.php -Raw) -replace "`r`n","`n"
$b = (Get-Content C:/xampp/htdocs/loams_api/delete_students.php -Raw) -replace "`r`n","`n"
if ($a -eq $b) { "MATCH" } else { "DIFFER" }
```
Expected: `MATCH` for all four. Also `php -l` each deployed copy → `No syntax errors detected`.

- [ ] **Step 4: Verify the negative-auth path (no DB mutation)**

With XAMPP Apache running (start it first — no auto-start on this machine), POST each endpoint with NO `admin_key` and confirm HTTP 401 + `{"status":"error","message":"Admin authentication required"}`. Example:
```
curl -s -o - -w " [%{http_code}]" -X POST -d "department=__nope__" http://localhost/loams_api/deactivate_department.php
curl -s -o - -w " [%{http_code}]" -X POST -d "school_ids[]=__nope__" http://localhost/loams_api/delete_students.php
curl -s -o - -w " [%{http_code}]" -X POST -d "students=[]" http://localhost/loams_api/bulk_update_students.php
```
Expected: each prints the 401 JSON and ` [401]`. Because the guard runs before any query, the DB is untouched — verify no rows changed on a synthetic dept.

- [ ] **Step 5: Record the deploy + hand off the success-path check**

The **success path** (log in with the REAL admin key, run a delete/deactivate → succeeds) requires the owner's admin key and a synthetic test DB — it cannot be automated here. Note in the PR body that it is the owner's GUI walkthrough item (§6.5), to be run from a current build (`WITS`/`WITSQuick` from this branch), never a stale pre-4a binary. Keep the `.pre-4a.bak` copies until the owner confirms the success path.

- [ ] **Step 6: Commit any deploy notes**

If the plan tracks deploy state in a doc, update it. Otherwise nothing to commit here (the web root is not in git). Proceed to the review gate.

---

## Review gate (after all tasks)

- Full ctest suite green (Debug); Release build clean, no new warnings.
- Run the project review gate via the `create-pr` skill (3 agents — dry-checker, security-reviewer, general-code-reviewer; if the loaded skill names a 4th agent, re-read `.claude/skills/create-pr/SKILL.md` per CLAUDE.md precedence). The `security-reviewer` should specifically confirm: the guard runs before any mutation on all four endpoints; `admin_key` is never logged or persisted client-side; no hardcoded credentials reintroduced.
- Owner GUI walkthrough (success path) against a synthetic DB: a keyed delete cascades visits; a keyed deactivate/delete-department succeeds; a wrong/absent key surfaces "Invalid admin key" (not "Network error") on BOTH the Quick and legacy clients.

## Self-Review checklist (run before handing off to build)

1. **Spec coverage:** §5.2 endpoint table — deactivate/delete_department guard (Task 2) ✓; delete_students harmonize+cascade (Task 3) ✓; bulk_update harmonize (Task 4) ✓; §3.2 StudentController request-format change + request-side tests (Tasks 5–7) ✓; §5.5 legacy direct calls (Task 8) + shared-controller calls (Task 9) ✓; §5.4 lockstep deploy + byte-verify (Task 10) ✓. `search_students` `photo` field is 4d, NOT this increment — correctly excluded.
2. **Placeholders:** none — every code step shows full content or an exact before/after hunk.
3. **Type consistency:** `deleteStudents(QStringList, QString)` and `bulkUpdateStudents(QList<StudentRecord>, QString)` used identically in the header, cpp, tests, and both legacy call sites; `CapturingNam` members (`lastOp`/`lastUrl`/`lastContentType`/`lastBody`) match between header, cpp, and tests.
