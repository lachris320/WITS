# LOAMS 2.0 Phase 6a — Admin Read-Endpoint Authentication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Require a valid `admin_key` on the six admin-only read endpoints (`search_students`, `get_library_visits`, `get_visitors`, `get_report_data`, `get_report_time_data`, `dashboard_summary`), closing the unauthenticated student-PII enumeration hole, and thread the key from the LOAMS 2.0 (`WITSQuick`) client.

**Architecture:** Server — make `requireAdminAuth()` payload-aware ($_POST OR JSON body, never the query string), then add one guard call to each of the six reads. Client — the view-model reads `AdminSession::instance().key()` and passes it to its controller (mirroring the write paths); two GET reads switch to POST so the key rides in the body while their filters stay in the query string. Legacy `WITS.exe` reads are intentionally left unauthenticated (owner-approved, spec §6).

**Tech Stack:** PHP 7/8 + mysqli (backend, `deliverables/loams_api/`); Qt 6.11.1 / C++17 / QML (client); Qt Test + ctest (`wits_add_qttest()`); the `CapturingNam` test double (`qt-app/testsupport/capturingnam.h`).

## Global Constraints

- **Spec:** `docs/superpowers/specs/2026-09-14-loams2-phase6a-read-endpoint-auth-design.md` (claude-review APPROVED, 3 rounds; owner sign-off recorded).
- **`admin_key` never in the query string** — a secret in a URL leaks into server/proxy logs (security-hygiene rule). It goes in `$_POST` (urlencoded/multipart) or a JSON body field only.
- **`admin_key` is never logged** — no `qDebug()`/`qWarning()` of the key or of a request body that carries it.
- **No new PHP test harness.** Server guards are verified by a documented manual curl checklist (Task 6). Client changes are full red→green ctest TDD.
- **Do NOT touch the public/kiosk endpoints** (spec §3.5): `student_login`, `guest_login`, `rfid_login`, `admin_login`, `get_departments`, `get_courses`, `get_courses_by_department`, `get_years`, `get_branding`.
- **Do NOT thread keys into the legacy Widgets app** (`adminwindow.cpp`, `VisitorController::fetchVisitors`, `ReportController::fetchPreviewData`). Where a call signature changes, the legacy caller passes an explicit empty key.
- **Zero raw hex / MVVM / naming** conventions from `CLAUDE.md` still apply. Commit via the `commit` skill.
- **Build/test toolchain:** Qt kit is NOT on PATH — prepend `C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;`. Configure into a SHORT external dir: `cmake -S <worktree>/qt-app -B C:/b/loams-6a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64`. Run targeted tests with `ctest --test-dir C:/b/loams-6a -R <name> --output-on-failure` (the two QuickTest targets flake only under full-parallel load; use `-R`/`-j1`).

---

## File Structure

**Server (`deliverables/loams_api/`):**
- `auth_helper.php` — MODIFY: `requireAdminAuth()` becomes payload-aware via a new `extractAdminKey()`.
- `search_students.php`, `get_library_visits.php`, `get_visitors.php`, `get_report_data.php`, `get_report_time_data.php`, `dashboard_summary.php` — MODIFY: add `require_once 'auth_helper.php'; requireAdminAuth($conn);` after the DB connect.

**Client (`qt-app/`):**
- `core/studentcontroller.{h,cpp}` — MODIFY: `searchStudents()` gains an `adminKey` param.
- `core/reportcontroller.{h,cpp}` — MODIFY: `fetchReportRows()` / `fetchTimeAnalytics()` gain an `adminKey` param (legacy `fetchPreviewData()` unchanged).
- `quick/viewmodels/SearchViewModel.cpp`, `quick/viewmodels/DatabaseViewModel.cpp`, `quick/viewmodels/ReportingViewModel.cpp` — MODIFY: pass `AdminSession::instance().key()`.
- `quick/viewmodels/VisitLogsViewModel.{h,cpp}`, `quick/viewmodels/DashboardViewModel.{h,cpp}` — MODIFY: NAM-injection ctor seam; GET→POST with `admin_key` in the body (Dashboard; VisitLogs student branch); `admin_key` into the JSON payload (VisitLogs guest branch).
- `adminwindow.cpp` — MODIFY: one legacy `searchStudents(...)` call passes an explicit empty key (compile-compat only).

**Tests (`qt-app/`):**
- `tests/tst_studentcontroller.cpp`, `tests/tst_reportcontroller.cpp` — ADD request-assembly + 401 cases.
- `quick/tests/tst_visitlogsviewmodel.cpp`, `quick/tests/tst_dashboardviewmodel.cpp` — ADD request-assembly + 401 cases (via the new ctor seam).

**Docs:**
- `deliverables/loams_api/PHASE6A_VERIFICATION.md` — CREATE: the manual curl checklist.

### Test-double reference (used by every client task)

`CapturingNam` (`qt-app/testsupport/capturingnam.h`) records `lastOp`, `lastUrl`, `lastContentType`, `lastBody` and returns a canned reply. Construct `CapturingNam(body, QNetworkReply::AuthenticationRequiredError, 401)` to simulate a guard rejection that still carries a decodable body. Controllers take a NAM in their ctor; the two inline VMs get a NAM-injection ctor in Tasks 4–5. The admin key comes from the process-wide `AdminSession::instance()` singleton — tests set it with `AdminSession::instance().setKey("test-key")` and clear it with `AdminSession::instance().clear()`.

---

### Task 1: Payload-aware `requireAdminAuth()` (server)

Foundation for every guard: the key must resolve whether it arrives as a form field (writes, and the two GET→POST reads) or inside a JSON body (the four JSON reads).

**Files:**
- Modify: `deliverables/loams_api/auth_helper.php`

**Interfaces:**
- Produces: `requireAdminAuth($conn)` — unchanged signature; now reads the key from `$_POST['admin_key']` first, else the `admin_key` field of the JSON request body. Never reads `$_GET`. Callers in Tasks 2–5 rely on this dual-source behavior.

- [ ] **Step 1: Add `extractAdminKey()` and use it in `requireAdminAuth()`**

Replace the current key-read line in `auth_helper.php` (`$admin_key = isset($_POST['admin_key']) ? $_POST['admin_key'] : '';`) so the function body begins:

```php
/**
 * Extract the admin key from the request: $_POST first (urlencoded/multipart),
 * else the admin_key field of a JSON body. NEVER $_GET — a secret in the query
 * string would leak into access logs (security-hygiene rule). php://input is
 * re-readable for JSON bodies, so this does not disturb endpoints that decode
 * their own JSON payload.
 */
function extractAdminKey() {
    if (isset($_POST['admin_key']) && $_POST['admin_key'] !== '') {
        return (string) $_POST['admin_key'];
    }
    $raw = file_get_contents('php://input');
    if ($raw !== false && $raw !== '') {
        $body = json_decode($raw, true);
        if (is_array($body) && isset($body['admin_key'])) {
            return (string) $body['admin_key'];
        }
    }
    return '';
}

function requireAdminAuth($conn) {
    $admin_key = extractAdminKey();

    if (empty($admin_key)) {
        http_response_code(401);
        echo json_encode(["status" => "error", "message" => "Admin authentication required"]);
        exit;
    }
    // ... existing password_verify block unchanged ...
```

Leave the `password_verify` / prepared-statement / 401 / 500 block below exactly as-is.

- [ ] **Step 2: Manual verification (no PHP harness)**

Confirm by reading the diff: `extractAdminKey()` returns the `$_POST` value when present (writes + form reads keep working — backward compatible), and falls back to the JSON body's `admin_key`. Full endpoint curl checks are consolidated in Task 6. Note here: the write endpoints already send `admin_key` in `$_POST`, so branch 1 preserves them.

- [ ] **Step 3: Commit**

```bash
git add deliverables/loams_api/auth_helper.php
git commit -m "feat(api): make requireAdminAuth payload-aware (form or JSON body)"
```

---

### Task 2: Guard `search_students.php` + thread the key from the Search/Database VMs

**Files:**
- Modify: `qt-app/core/studentcontroller.h` (searchStudents decl, ~line 60), `qt-app/core/studentcontroller.cpp` (searchStudents body, ~line 214)
- Modify: `qt-app/quick/viewmodels/SearchViewModel.cpp:33`, `qt-app/quick/viewmodels/DatabaseViewModel.cpp:61`
- Modify: `qt-app/adminwindow.cpp:2428` (legacy — explicit empty key)
- Modify: `deliverables/loams_api/search_students.php`
- Test: `qt-app/tests/tst_studentcontroller.cpp`

**Interfaces:**
- Consumes: `AdminSession::instance().key()` (`qt-app/quick/AdminSession.h`), `requireAdminAuth($conn)` (Task 1).
- Produces: `StudentController::searchStudents(const QString &search, const QString &department, const QString &course, const QString &adminKey)` — the trailing `adminKey` is new; it is inserted into the JSON body as `admin_key`.

- [ ] **Step 1: Write the failing test (request carries admin_key)**

Add to `tst_studentcontroller.cpp` (declare the two slots in the class body alongside the other request-assembly tests):

```cpp
void TestStudentController::searchStudents_buildsJsonBodyWithAdminKey()
{
    CapturingNam nam;
    StudentController ctrl(&nam);

    ctrl.searchStudents("cruz", "CCS", "BSIT", "test-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/json"));
    const QJsonObject body = QJsonDocument::fromJson(nam.lastBody).object();
    QCOMPARE(body.value("admin_key").toString(), QStringLiteral("test-key"));
    QCOMPARE(body.value("search").toString(), QStringLiteral("cruz"));
}

void TestStudentController::searchStudents_guard401_emitsSearchFailed()
{
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"error\",\"message\":\"Admin authentication required\"}"),
                     QNetworkReply::AuthenticationRequiredError, 401);
    StudentController ctrl(&nam);
    QSignalSpy failed(&ctrl, &StudentController::searchFailed);

    ctrl.searchStudents("x", "", "", "");
    QVERIFY(failed.wait(1000));
    QCOMPARE(failed.count(), 1);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir C:/b/loams-6a -R tst_studentcontroller --output-on-failure`
Expected: FAIL — `searchStudents` does not yet accept a 4th argument (compile error).

- [ ] **Step 3: Add the `adminKey` param and body field**

In `studentcontroller.h`, extend the declaration:

```cpp
    quint64 searchStudents(const QString &search,
                           const QString &department,
                           const QString &course,
                           const QString &adminKey);
```

In `studentcontroller.cpp`, update the signature to match and add the field after the existing `filters` are set:

```cpp
    filters["search"]     = search;
    filters["department"] = normalizeFilter(department);
    filters["course"]     = normalizeFilter(course);
    filters["admin_key"]  = adminKey;   // guard field (spec §3.3) — never logged
```

- [ ] **Step 4: Update the callers**

`SearchViewModel.cpp` — add `#include "AdminSession.h"` and pass the key:

```cpp
    m_controller->searchStudents(search, m_department, course, AdminSession::instance().key());
```

`DatabaseViewModel.cpp` — add `#include "AdminSession.h"` and pass the key:

```cpp
    m_controller->searchStudents(QString(), m_department, m_course, AdminSession::instance().key());
```

`adminwindow.cpp:2428` (legacy — do NOT thread a key): pass an explicit empty string so it compiles, with a comment:

```cpp
    m_studentController->searchStudents(
        /* ...existing args... */,
        QString());   // legacy WITS.exe — unauthenticated read, breaks per spec §6
```

- [ ] **Step 5: Run to verify the tests pass**

Run: `ctest --test-dir C:/b/loams-6a -R tst_studentcontroller --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Add the server guard**

In `deliverables/loams_api/search_students.php`, immediately after the `if ($conn->connect_error) { ... exit; }` block (~line 13), before the JSON body is read:

```php
require_once 'auth_helper.php';
requireAdminAuth($conn);
```

- [ ] **Step 7: Commit**

```bash
git add qt-app/core/studentcontroller.h qt-app/core/studentcontroller.cpp \
        qt-app/quick/viewmodels/SearchViewModel.cpp qt-app/quick/viewmodels/DatabaseViewModel.cpp \
        qt-app/adminwindow.cpp deliverables/loams_api/search_students.php \
        qt-app/tests/tst_studentcontroller.cpp
git commit -m "feat(api): require admin_key on search_students; thread key from Search/Database VMs"
```

---

### Task 3: Guard the two report reads + thread the key from ReportingViewModel

**Files:**
- Modify: `qt-app/core/reportcontroller.h` (fetchReportRows/fetchTimeAnalytics decls, lines 52 & 54), `qt-app/core/reportcontroller.cpp` (both bodies, ~lines 359 & 397)
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.cpp:450-451`
- Modify: `deliverables/loams_api/get_report_data.php`, `deliverables/loams_api/get_report_time_data.php`
- Test: `qt-app/tests/tst_reportcontroller.cpp`

**Interfaces:**
- Consumes: `AdminSession::instance().key()`, `requireAdminAuth($conn)`.
- Produces: `ReportController::fetchReportRows(const QJsonObject &filters, const QString &adminKey)` and `fetchTimeAnalytics(const QJsonObject &filters, const QString &adminKey)` — each merges `admin_key` into the posted JSON. `fetchPreviewData(const QJsonObject &)` is unchanged (legacy).

- [ ] **Step 1: Write the failing tests**

Add to `tst_reportcontroller.cpp` (declare slots; include `capturingnam.h`, `<QJsonDocument>`, `<QJsonObject>`, `<QSignalSpy>`):

```cpp
void TstReportController::fetchReportRows_mergesAdminKeyIntoJsonBody()
{
    CapturingNam nam;
    ReportController ctrl(&nam);
    QJsonObject filters; filters["department"] = "CCS";

    ctrl.fetchReportRows(filters, "test-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/json"));
    const QJsonObject body = QJsonDocument::fromJson(nam.lastBody).object();
    QCOMPARE(body.value("admin_key").toString(), QStringLiteral("test-key"));
    QCOMPARE(body.value("department").toString(), QStringLiteral("CCS"));
}

void TstReportController::fetchTimeAnalytics_mergesAdminKeyIntoJsonBody()
{
    CapturingNam nam;
    ReportController ctrl(&nam);
    ctrl.fetchTimeAnalytics(QJsonObject{}, "test-key");
    const QJsonObject body = QJsonDocument::fromJson(nam.lastBody).object();
    QCOMPARE(body.value("admin_key").toString(), QStringLiteral("test-key"));
}

void TstReportController::fetchReportRows_guard401_emitsReportError()
{
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"error\"}"),
                     QNetworkReply::AuthenticationRequiredError, 401);
    ReportController ctrl(&nam);
    QSignalSpy err(&ctrl, &ReportController::reportError);
    ctrl.fetchReportRows(QJsonObject{}, "");
    QVERIFY(err.wait(1000));
    QCOMPARE(err.count(), 1);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir C:/b/loams-6a -R tst_reportcontroller --output-on-failure`
Expected: FAIL — the fetch methods don't accept a 2nd argument yet.

- [ ] **Step 3: Add the `adminKey` param and merge it**

In `reportcontroller.h`:

```cpp
    void fetchReportRows(const QJsonObject &filters, const QString &adminKey);   // POST get_report_data.php
    void fetchPreviewData(const QJsonObject &filters); // POST api.php/reports/data (legacy, unauthenticated)
    void fetchTimeAnalytics(const QJsonObject &filters, const QString &adminKey); // POST get_report_time_data.php
```

In `reportcontroller.cpp`, in **both** `fetchReportRows` and `fetchTimeAnalytics`, update the signature and merge the key before posting (replace the `m_nam->post(request, QJsonDocument(filters).toJson())` line):

```cpp
    QJsonObject body = filters;
    body.insert("admin_key", adminKey);   // guard field (spec §3.3) — never logged
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
```

Leave `fetchPreviewData` untouched.

- [ ] **Step 4: Update the caller**

`ReportingViewModel.cpp` (~lines 450-451) — add `#include "AdminSession.h"` and pass the key to both:

```cpp
    const QString key = AdminSession::instance().key();
    m_controller->fetchReportRows(filters, key);
    m_controller->fetchTimeAnalytics(filters, key);   // parallel, same filters
```

- [ ] **Step 5: Run to verify the tests pass**

Run: `ctest --test-dir C:/b/loams-6a -R tst_reportcontroller --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Add the server guards**

In BOTH `get_report_data.php` and `get_report_time_data.php`, immediately after `include 'db.php';` (line 3), before the `REQUEST_METHOD` check:

```php
require_once 'auth_helper.php';
requireAdminAuth($conn);
```

(The client sends `admin_key` inside the same JSON body the endpoint already decodes; the guard reads it via the JSON branch, the endpoint ignores it.)

- [ ] **Step 7: Commit**

```bash
git add qt-app/core/reportcontroller.h qt-app/core/reportcontroller.cpp \
        qt-app/quick/viewmodels/ReportingViewModel.cpp \
        deliverables/loams_api/get_report_data.php deliverables/loams_api/get_report_time_data.php \
        qt-app/tests/tst_reportcontroller.cpp
git commit -m "feat(api): require admin_key on report reads; thread key from ReportingViewModel"
```

---

### Task 4: Guard the two Visit-Logs reads + thread the key from VisitLogsViewModel

`VisitLogsViewModel::refresh()` has two branches: **student** (`get_library_visits.php`, currently `GET` with `range` in the query string) and **guest** (`get_visitors.php`, inline JSON `POST`). The student branch switches to `POST` (key in the urlencoded body, `range` kept in the query string); the guest branch adds `admin_key` to its JSON payload. A NAM-injection ctor seam makes both testable.

**Files:**
- Modify: `qt-app/quick/viewmodels/VisitLogsViewModel.h` (ctor + member init), `qt-app/quick/viewmodels/VisitLogsViewModel.cpp` (ctor, both branches of `refresh()`, ~lines 78-135)
- Test: `qt-app/quick/tests/tst_visitlogsviewmodel.cpp`

**Interfaces:**
- Consumes: `AdminSession::instance().key()`, `requireAdminAuth($conn)`.
- Produces: `VisitLogsViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr)` — when `nam` is null it owns a fresh one (production, QML uses the parent-only form); when injected it uses that one un-owned (tests).

- [ ] **Step 1: Write the failing tests**

Add to `tst_visitlogsviewmodel.cpp` (include `capturingnam.h`, `AdminSession.h`, `<QUrlQuery>`, `<QJsonDocument>`):

```cpp
void TestVisitLogsViewModel::studentRefresh_postsWithAdminKeyBodyAndRangeInQuery()
{
    AdminSession::instance().setKey("test-key");
    CapturingNam nam;
    VisitLogsViewModel vm(nullptr, &nam);
    vm.setMode(VisitLogsViewModel::Student);   // if a setter exists; else default is Student
    vm.refresh();

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    QVERIFY(nam.lastUrl.query().contains("range="));               // filter stays in the query string
    const QUrlQuery form(QString::fromUtf8(nam.lastBody));
    QCOMPARE(form.queryItemValue("admin_key"), QStringLiteral("test-key"));
    AdminSession::instance().clear();
}

void TestVisitLogsViewModel::guestRefresh_addsAdminKeyToJsonPayload()
{
    AdminSession::instance().setKey("test-key");
    CapturingNam nam;
    VisitLogsViewModel vm(nullptr, &nam);
    vm.setMode(VisitLogsViewModel::Guest);
    vm.refresh();

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    const QJsonObject body = QJsonDocument::fromJson(nam.lastBody).object();
    QCOMPARE(body.value("admin_key").toString(), QStringLiteral("test-key"));
    AdminSession::instance().clear();
}
```

(If `setMode` is not a public method, drive the mode the way existing `tst_visitlogsviewmodel` tests do — check the file for the existing mode-setting seam and mirror it.)

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir C:/b/loams-6a -R tst_visitlogsviewmodel --output-on-failure`
Expected: FAIL — the 2-arg ctor doesn't exist yet.

- [ ] **Step 3: Add the NAM-injection ctor seam**

In `VisitLogsViewModel.h`:

```cpp
    explicit VisitLogsViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr);
```

In `VisitLogsViewModel.cpp` ctor, initialize `m_nam` from the arg or a fresh owned instance:

```cpp
VisitLogsViewModel::VisitLogsViewModel(QObject *parent, QNetworkAccessManager *nam)
    : QObject(parent)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
}
```

Add `#include "AdminSession.h"` to the .cpp.

- [ ] **Step 4: Switch the student branch GET→POST (keep range in the query string)**

Replace the student-branch request build in `refresh()`:

```cpp
        QUrl url = ApiConfig::endpoint(QStringLiteral("get_library_visits.php"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("range"),
                       m_range == Week ? QStringLiteral("week") : QStringLiteral("today"));
        url.setQuery(q);                       // filters stay in $_GET
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("admin_key"), AdminSession::instance().key());
        QNetworkReply *reply = m_nam->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
```

- [ ] **Step 5: Add the key to the guest-branch JSON payload**

In the guest branch, after the `payload` object is built and before the request, add:

```cpp
    payload[QStringLiteral("admin_key")] = AdminSession::instance().key();   // guard field — never logged
```

- [ ] **Step 6: Run to verify the tests pass**

Run: `ctest --test-dir C:/b/loams-6a -R tst_visitlogsviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Add the server guards**

`get_library_visits.php` — after the `if ($conn->connect_error) { ... exit; }` block (~line 9):

```php
require_once 'auth_helper.php';
requireAdminAuth($conn);
```

`get_visitors.php` — immediately after `include "db.php";` (line 3), before the JSON body is decoded:

```php
require_once 'auth_helper.php';
requireAdminAuth($conn);
```

- [ ] **Step 8: Commit**

```bash
git add qt-app/quick/viewmodels/VisitLogsViewModel.h qt-app/quick/viewmodels/VisitLogsViewModel.cpp \
        deliverables/loams_api/get_library_visits.php deliverables/loams_api/get_visitors.php \
        qt-app/quick/tests/tst_visitlogsviewmodel.cpp
git commit -m "feat(api): require admin_key on visit-log reads; thread key from VisitLogsViewModel"
```

---

### Task 5: Guard `dashboard_summary.php` + thread the key from DashboardViewModel

**Files:**
- Modify: `qt-app/quick/viewmodels/DashboardViewModel.h` (ctor + member init), `qt-app/quick/viewmodels/DashboardViewModel.cpp` (ctor + `refresh()`, ~lines 9-28)
- Modify: `deliverables/loams_api/dashboard_summary.php`
- Test: `qt-app/quick/tests/tst_dashboardviewmodel.cpp`

**Interfaces:**
- Consumes: `AdminSession::instance().key()`, `requireAdminAuth($conn)`.
- Produces: `DashboardViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr)` — same NAM seam as Task 4.

- [ ] **Step 1: Write the failing tests**

Add to `tst_dashboardviewmodel.cpp` (include `capturingnam.h`, `AdminSession.h`, `<QUrlQuery>`):

```cpp
void TestDashboardViewModel::refresh_postsWithAdminKeyInBody()
{
    AdminSession::instance().setKey("test-key");
    CapturingNam nam;
    DashboardViewModel vm(nullptr, &nam);
    vm.refresh();

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    const QUrlQuery form(QString::fromUtf8(nam.lastBody));
    QCOMPARE(form.queryItemValue("admin_key"), QStringLiteral("test-key"));
    AdminSession::instance().clear();
}

void TestDashboardViewModel::refresh_guard401_setsError()
{
    AdminSession::instance().setKey("");
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"error\"}"),
                     QNetworkReply::AuthenticationRequiredError, 401);
    DashboardViewModel vm(nullptr, &nam);
    QSignalSpy dc(&vm, &DashboardViewModel::dataChanged);
    vm.refresh();
    QVERIFY(dc.wait(1000));
    QVERIFY(!vm.error().isEmpty());   // 401 lands in the error state, not empty success
}
```

(Match the actual error-notify signal/accessor names in `DashboardViewModel.h` — adjust `dataChanged`/`error()` if the file names them differently.)

- [ ] **Step 2: Run to verify it fails**

Run: `ctest --test-dir C:/b/loams-6a -R tst_dashboardviewmodel --output-on-failure`
Expected: FAIL — 2-arg ctor doesn't exist.

- [ ] **Step 3: Add the NAM-injection ctor seam**

In `DashboardViewModel.h`:

```cpp
    explicit DashboardViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr);
```

In `DashboardViewModel.cpp`:

```cpp
DashboardViewModel::DashboardViewModel(QObject *parent, QNetworkAccessManager *nam)
    : QObject(parent)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
}
```

Add `#include "AdminSession.h"`.

- [ ] **Step 4: Switch `refresh()` GET→POST with the key in the body**

Replace the `m_nam->get(...)` request build:

```cpp
    QNetworkRequest req(ApiConfig::endpoint(QStringLiteral("dashboard_summary.php")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("admin_key"), AdminSession::instance().key());
    QNetworkReply *reply = m_nam->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
```

- [ ] **Step 5: Run to verify the tests pass**

Run: `ctest --test-dir C:/b/loams-6a -R tst_dashboardviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Add the server guard**

In `dashboard_summary.php`, after the `if ($conn->connect_error) { ... exit; }` block (~line 9), before `week_window()`:

```php
require_once 'auth_helper.php';
requireAdminAuth($conn);
```

- [ ] **Step 7: Commit**

```bash
git add qt-app/quick/viewmodels/DashboardViewModel.h qt-app/quick/viewmodels/DashboardViewModel.cpp \
        deliverables/loams_api/dashboard_summary.php \
        qt-app/quick/tests/tst_dashboardviewmodel.cpp
git commit -m "feat(api): require admin_key on dashboard_summary; thread key from DashboardViewModel"
```

---

### Task 6: Manual server-verification checklist + full-suite green

Server guards have no automated harness (approved decision); this task captures the repeatable manual proof and confirms nothing regressed client-side.

**Files:**
- Create: `deliverables/loams_api/PHASE6A_VERIFICATION.md`

- [ ] **Step 1: Write the curl checklist**

Create `PHASE6A_VERIFICATION.md` with a runnable checklist for a local/staging PHP instance (`BASE` = the deployed API root; `KEY` = a valid admin key — supplied by the operator, never committed):

```markdown
# Phase 6a — manual server verification

Run against a deployed instance. Do NOT paste a real admin key into this file or any commit.

For each of the six reads, a request WITHOUT a key must return HTTP 401 + {"status":"error"},
and WITH a valid key must return 200 + data.

- [ ] search_students (JSON):        curl -sS -o /dev/null -w '%{http_code}\n' -X POST $BASE/search_students.php -H 'Content-Type: application/json' -d '{"search":""}'                 → 401
      curl ... -d '{"search":"","admin_key":"'"$KEY"'"}'                                                                                                                                → 200
- [ ] get_visitors (JSON):           -X POST -H 'Content-Type: application/json' -d '{}'  → 401 ;  -d '{"admin_key":"'"$KEY"'"}' → 200
- [ ] get_report_data (JSON):        -X POST -H 'Content-Type: application/json' -d '{}'  → 401 ;  with admin_key → 200
- [ ] get_report_time_data (JSON):   -X POST -H 'Content-Type: application/json' -d '{}'  → 401 ;  with admin_key → 200
- [ ] get_library_visits (form):     -X POST -d 'range=today'                             → 401 ;  -d 'range=today&admin_key='"$KEY" (range may also be in the query string) → 200
- [ ] dashboard_summary (form):      -X POST                                              → 401 ;  -d 'admin_key='"$KEY" → 200

Regression (payload-aware helper's $_POST branch):
- [ ] A guarded WRITE still succeeds with admin_key in $_POST (e.g. delete_students with a throwaway id) → 200/expected error, NOT 401.

Kiosk/public must stay open (no key):
- [ ] student_login / guest_login / rfid_login / get_departments / get_branding → work with no key.
```

- [ ] **Step 2: Configure + build the client**

Run: `cmake -S <worktree>/qt-app -B C:/b/loams-6a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 && cmake --build C:/b/loams-6a`
Expected: clean build, no new warnings.

- [ ] **Step 3: Run the full ctest suite serially**

Run: `ctest --test-dir C:/b/loams-6a -j1 --output-on-failure`
Expected: all tests pass (the four touched targets plus the rest). If a QuickTest target flakes under load, re-run it with `-R <name>`.

- [ ] **Step 4: Commit**

```bash
git add deliverables/loams_api/PHASE6A_VERIFICATION.md
git commit -m "docs(api): add Phase 6a manual server-verification checklist"
```

---

## Notes for the reviewer / PR

- The guarded reads also cover `api.php`'s `reports/data` route (it `require_once`s `get_report_data.php`), so the legacy `WITS.exe` report preview 401s silently after deploy — **accepted** per spec §6.
- Deploy the PHP and the `WITSQuick` build **together** (spec §6): a gated server with an un-updated Quick client would 401 every admin read.
- `SECURITY_UPDATES.md` is left stale by design (info-disclosure slice, not 6a).
