# LOAMS 2.0 Phase 4a.3 — Excel/ZIP Student Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to execute this plan — one fresh subagent per Task, each owning its full TDD cycle (red → green → refactor → commit) end-to-end. Every step below uses checkbox (`- [ ]`) syntax; check a box only when its command has actually been run and its stated expectation observed (never claim a green suite you did not run). Tasks are ordered dependency-ascending: do them in order. Each Task's **Interfaces** block is the contract later Tasks consume — do not change a signature once a downstream Task depends on it.

**Goal:** Bring bulk student import into the Quick admin app (`WITSQuick.exe`) via a new `ImportViewModel` + `ImportStudentsDialog`, and rework the client request + backend so the client sends already-parsed, header-mapped rows to two admin-guarded endpoints that return honest counts.

**Architecture:** The client already parses Excel/CSV into structured rows for the duplicate check; it now serializes those header-mapped rows to JSON and POSTs them (instead of the raw Excel file), so the server stops parsing spreadsheets (PhpSpreadsheet/Composer removed) and becomes a guarded row inserter honoring `skip_ids`. A new `ImportViewModel` (owns an `ImportController` + its own `QNetworkAccessManager`) is the sole QML-facing surface; a new `ImportStudentsDialog.qml` drives the idle → parse → validate → duplicates → upload → result flow. Both `upload_students_zip.php` and `check_duplicates.php` gain `requireAdminAuth` — a **BREAKING** change that must be deployed WITH this client.

**Tech Stack:** Qt 6.11 / C++17 / QML (Qt Quick, MVVM); QtTest + Qt Quick Test (`qmltest`) orchestrated by ctest; PHP / MySQL backend (core `ext-zip` only — no Composer).

---

## How to build & test

Qt tools are **NOT** on `PATH`. Every shell (the Bash tool resets cwd between calls) must first export the prefix:

```bash
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Qt/Tools/CMake_64/bin:$PATH"
```

- **Configure** (only needed after a `CMakeLists.txt` change): `cmake -S qt-app -B C:/b/loams-4a3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`
- **Build:** `cmake --build C:/b/loams-4a3`
- **Test (all):** `ctest --test-dir C:/b/loams-4a3 --output-on-failure`
- **Test (one):** `ctest --test-dir C:/b/loams-4a3 -R <name> --output-on-failure`

Use the **short build dir `C:/b/loams-4a3`** — the default in-tree build dir overflows the Windows `MAX_PATH` object-path limit for the QML module. **Always rebuild before `ctest`** (stale-binary trap: ctest runs the last-built `.exe`, not your latest edit). Editing a **production** `.qml` does nothing until you rebuild (it is compiled into `witsquickmodule`); only `qt-app/quick/tests/*.qml` data files are read live at test time.

**Baseline:** the suite is **39 ctest targets green** before this plan. This plan adds exactly **one** new ctest target (`tst_importviewmodel`, Task 10) → **40 green** at the end. The `ImportStudentsDialog` QuickTest fixtures fold into the existing `tst_qml_admin` target (compiled from the whole `qt-app/quick/tests/` dir via `QUICK_TEST_SOURCE_DIR`); the new `ImportController` / template slots fold into the existing `tst_importcontroller` target.

**Commit rule:** NEVER `git add -A`. Stage only the files you changed, by name. Use Conventional Commits (`feat(...)`, `test(...)`, `refactor(...)`, `docs(...)`). **Do NOT add a `Co-Authored-By: Claude` / Anthropic trailer.** Each Task's final step is a commit of exactly that Task's files. Commit via the `commit` skill where available; otherwise `git add <files>` + `git commit`.

---

## Global Constraints (from the spec — do not relitigate)

- **MVVM:** ViewModels in `qt-app/quick/viewmodels/` are the ONLY QML-facing C++. QML never calls a `witscore` controller directly.
- **Theming:** ZERO raw hex outside `Theme.qml`. Use `Theme.<token>`; opacity variants via `Qt.alpha(Theme.<token>, a)`, never a literal color.
- **Naming:** QML types + C++ ViewModel/model classes are `PascalCase`; C++ member variables are `m_camelCase`.
- **Untrusted output:** every server-provided string renders `textFormat: Text.PlainText` in QML.
- **admin key:** RAM-only (held by `AdminSession`), sent in the POST body only, NEVER logged, NEVER persisted.
- **Guard-before-mutation:** both endpoints call `requireAdminAuth($conn)` before any DB read, ZIP extract, or insert.
- **Fields written:** the **7 core columns** (`school_id, name, course, department, year_level, gender, status`) **plus `photo`** (ZIP-matched path for that `school_id`, else `NULL`). `code` and `visits` are **EXCLUDED** even if present in the file.
- **Column handling:** extra/unrecognized columns are **ignored** (not an error); only `school_id` + `name` columns are **required**; column order does not matter (header-mapped, not positional).
- **No overwrite:** the backend can only **SKIP** duplicates. Do not offer an overwrite path.
- **Backend is BREAKING:** deploy the two endpoints WITH this client — an old client without `admin_key` threading will 401.
- **No Composer:** drop PhpSpreadsheet / `vendor/autoload.php` from `upload_students_zip.php`; keep only core `ext-zip` for the optional photos ZIP.

---

## Task 1 — `UploadResult.skippedCount` + `parseUploadResponse` real counts

Add the `skippedCount` field to the result POD and make `parseUploadResponse` read the four real JSON fields (`status`, `message`, `success_count`, `skipped_count`, `error_count`) while keeping the plain-text fallback. This is a pure change with no network — the existing `tst_importcontroller` target already compiles `importcontroller.cpp` directly.

**Files:**
- Modify `qt-app/core/importdata.h` (the `UploadResult` struct, ~lines 21-29 — add one field).
- Modify `qt-app/core/importcontroller.cpp` (`parseUploadResponse`, ~lines 313-336).
- Modify `qt-app/tests/tst_importcontroller.cpp` (extend `parseUploadResponseSuccess`; add one new slot + its declaration).

**Interfaces:**
- Produces: `struct UploadResult { bool ok; QString message; int successCount; int skippedCount; int errorCount; bool plainText; QString rawText; };` (adds `int skippedCount = 0;`).
- Produces: `static UploadResult ImportController::parseUploadResponse(const QByteArray &raw);` (signature unchanged; now populates `skippedCount`).
- Consumed later by: Task 4 (`uploadFinished`), Task 10 (`ImportViewModel::onUploadFinished` → `resultText`).

**Steps:**

- [ ] **Write the failing test.** Add a new slot declaration `void parseUploadResponseReadsSkippedCount();` to the `// parseUploadResponse` group in the `private slots:` block of `qt-app/tests/tst_importcontroller.cpp` (after `parseUploadResponseSuccess`). Extend the existing `parseUploadResponseSuccess` body to also assert the new field, and add the new slot body:

```cpp
void TestImportController::parseUploadResponseSuccess()
{
    const QByteArray json = R"({
        "status": "success",
        "message": "All good.",
        "success_count": 10,
        "skipped_count": 2,
        "error_count": 1
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(result.ok);
    QVERIFY(!result.plainText);
    QCOMPARE(result.message, QString("All good."));
    QCOMPARE(result.successCount, 10);
    QCOMPARE(result.skippedCount, 2);
    QCOMPARE(result.errorCount, 1);
}

void TestImportController::parseUploadResponseReadsSkippedCount()
{
    // skipped_count present and non-zero is read verbatim (regression guard:
    // the field did not exist before 4a.3).
    const QByteArray json = R"({
        "status": "success",
        "message": "Partial import.",
        "success_count": 5,
        "skipped_count": 4,
        "error_count": 0
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(result.ok);
    QCOMPARE(result.successCount, 5);
    QCOMPARE(result.skippedCount, 4);
    QCOMPARE(result.errorCount, 0);
}
```

- [ ] **Run — expect FAIL to build.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: compile error (`'struct UploadResult' has no member named 'skippedCount'`) or, once the field is added but the parser isn't, `QCOMPARE(result.skippedCount, 2)` FAILS with `Actual: 0`.

- [ ] **Implement — struct field.** In `qt-app/core/importdata.h`, add `int skippedCount = 0;` immediately after the `int successCount = 0;` line:

```cpp
struct UploadResult
{
    bool    ok = false;
    QString message;
    int     successCount = 0;
    int     skippedCount = 0;
    int     errorCount = 0;
    bool    plainText = false;   // true when the response was not a JSON object
    QString rawText;             // populated only when plainText is true
};
```

- [ ] **Implement — parser.** In `qt-app/core/importcontroller.cpp`, replace the body of `parseUploadResponse` (keep the plain-text fallback exactly):

```cpp
UploadResult ImportController::parseUploadResponse(const QByteArray &raw)
{
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        UploadResult result;
        result.plainText = true;
        result.rawText   = QString::fromUtf8(raw);
        return result;
    }

    const QJsonObject obj = doc.object();
    UploadResult result;
    result.message      = obj[QLatin1String("message")].toString();
    result.successCount = obj[QLatin1String("success_count")].toInt();
    result.skippedCount = obj[QLatin1String("skipped_count")].toInt();
    result.errorCount   = obj[QLatin1String("error_count")].toInt();
    result.plainText    = false;
    result.ok           = (obj[QLatin1String("status")].toString() == QLatin1String("success"));
    return result;
}
```

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: `100% tests passed`, all `tst_importcontroller` slots (incl. the two above and the still-passing `parseUploadResponsePlainTextFallback`) green.

- [ ] **Commit.** Stage `qt-app/core/importdata.h`, `qt-app/core/importcontroller.cpp`, `qt-app/tests/tst_importcontroller.cpp`. Message: `feat(import): read skipped_count in parseUploadResponse (4a.3 T1)`.

---

## Task 2 — `serializeRows` pure static

Add a pure, network-free serializer that turns a `ParsedTable` into the JSON array of 7-key row objects the reworked endpoint expects. This is the primary new unit-test seam.

**Files:**
- Modify `qt-app/core/importcontroller.h` (add the static declaration in the "Pure, unit-testable statics" group, ~after line 21).
- Modify `qt-app/core/importcontroller.cpp` (add the implementation; needs `<QJsonArray>`/`<QJsonObject>`/`<QJsonDocument>` — already included).
- Modify `qt-app/tests/tst_importcontroller.cpp` (new slot group + declarations).

**Interfaces:**
- Produces: `static QByteArray ImportController::serializeRows(const ParsedTable &table);`
  - Returns a **compact JSON array** of objects, one per data row, each with exactly the keys `school_id`, `name`, `course`, `department`, `year_level`, `gender`, `status`.
  - Each value is read from `table.rows[r][ table.headerIndex[key] ]`, **trimmed**. A key whose column is absent from `headerIndex`, or whose column index is out of range for that (ragged/short) row, contributes `""`.
  - `code`, `visits`, and any `col_N`/unrecognized columns are **never** emitted.
  - Empty table (no rows) → the two bytes `[]`.
- Consumed later by: Task 4 (`uploadStudents` calls it internally), Task 5 (legacy path feeds it a `ParsedTable`).

**Steps:**

- [ ] **Write the failing test.** Add these slot declarations under a new `// serializeRows` comment in `private slots:` of `tst_importcontroller.cpp`:

```cpp
    // serializeRows
    void serializeRowsMapsSevenCoreKeys();
    void serializeRowsExcludesCodeAndVisits();
    void serializeRowsIgnoresUnrecognizedColumn();
    void serializeRowsTrimsValues();
    void serializeRowsShortRowFillsEmpty();
    void serializeRowsEmptyTableIsEmptyArray();
```

Add a helper + the slot bodies at the end of the file (before `QTEST_MAIN`). The helper decodes the produced bytes back to a `QJsonArray` so assertions read structurally:

```cpp
static QJsonArray decodeRows(const QByteArray &bytes)
{
    return QJsonDocument::fromJson(bytes).array();
}

void TestImportController::serializeRowsMapsSevenCoreKeys()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Course", "Year Level",
                 "Department", "Gender", "Status"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz", "BSIT", "1",
                          "CCS", "Male", "Active"};

    const QJsonArray arr = decodeRows(ImportController::serializeRows(t));
    QCOMPARE(arr.size(), 1);
    const QJsonObject o = arr.at(0).toObject();
    QCOMPARE(o.value("school_id").toString(),  QString("21-1-0001"));
    QCOMPARE(o.value("name").toString(),       QString("Juan Dela Cruz"));
    QCOMPARE(o.value("course").toString(),     QString("BSIT"));
    QCOMPARE(o.value("year_level").toString(), QString("1"));
    QCOMPARE(o.value("department").toString(), QString("CCS"));
    QCOMPARE(o.value("gender").toString(),     QString("Male"));
    QCOMPARE(o.value("status").toString(),     QString("Active"));
    // Exactly the 7 core keys — nothing else.
    QCOMPARE(o.keys().size(), 7);
}

void TestImportController::serializeRowsExcludesCodeAndVisits()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Code", "Visits"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // maps code + visits
    t.rows << QStringList{"21-1-0002", "Maria Clara", "RFID-9", "42"};

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QVERIFY(!o.contains("code"));
    QVERIFY(!o.contains("visits"));
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0002"));
    QCOMPARE(o.value("name").toString(),      QString("Maria Clara"));
    // Unmapped core columns still present, empty.
    QCOMPARE(o.value("course").toString(), QString());
}

void TestImportController::serializeRowsIgnoresUnrecognizedColumn()
{
    ParsedTable t;
    t.headers = {"School ID", "Notes"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // Notes -> col_1
    t.rows << QStringList{"21-1-0003", "ignore me"};

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QVERIFY(!o.contains("col_1"));
    QVERIFY(!o.contains("notes"));
    QCOMPARE(o.keys().size(), 7);
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0003"));
}

void TestImportController::serializeRowsTrimsValues()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"  21-1-0004  ", "\tAna Reyes \n"};

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0004"));
    QCOMPARE(o.value("name").toString(),      QString("Ana Reyes"));
}

void TestImportController::serializeRowsShortRowFillsEmpty()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Course"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // course -> index 2
    t.rows << QStringList{"21-1-0005", "Jose Rizal"};         // only 2 cells (ragged)

    const QJsonObject o = decodeRows(ImportController::serializeRows(t)).at(0).toObject();
    QCOMPARE(o.value("school_id").toString(), QString("21-1-0005"));
    QCOMPARE(o.value("name").toString(),      QString("Jose Rizal"));
    QCOMPARE(o.value("course").toString(),    QString());   // index 2 out of range -> ""
}

void TestImportController::serializeRowsEmptyTableIsEmptyArray()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    // no rows appended
    QCOMPARE(ImportController::serializeRows(t), QByteArray("[]"));
}
```

Ensure `#include <QJsonArray>`, `#include <QJsonObject>`, `#include <QJsonDocument>` are present at the top of `tst_importcontroller.cpp` (add any that are missing).

- [ ] **Run — expect FAIL.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: compile error (`serializeRows` undeclared).

- [ ] **Implement — header.** In `qt-app/core/importcontroller.h`, add to the pure-statics group (right after `static ParsedTable parseCsv(const QString &rawText);`):

```cpp
    // Pure serializer (4a.3): the parsed table -> a compact JSON array of
    // 7-key row objects (school_id/name/course/department/year_level/gender/
    // status), mapped through table.headerIndex. code/visits/unrecognized
    // columns are excluded; each value is trimmed; a missing/out-of-range
    // column contributes "". Empty table -> "[]". Primary upload-body seam.
    static QByteArray serializeRows(const ParsedTable &table);
```

- [ ] **Implement — cpp.** Add to `qt-app/core/importcontroller.cpp` (after `parseCsv`):

```cpp
QByteArray ImportController::serializeRows(const ParsedTable &table)
{
    static const QStringList kKeys = {
        QStringLiteral("school_id"), QStringLiteral("name"),
        QStringLiteral("course"),    QStringLiteral("department"),
        QStringLiteral("year_level"),QStringLiteral("gender"),
        QStringLiteral("status")
    };

    QJsonArray out;
    for (const QStringList &row : table.rows) {
        QJsonObject obj;
        for (const QString &key : kKeys) {
            QString value;
            if (table.headerIndex.contains(key)) {
                const int col = table.headerIndex.value(key);
                if (col >= 0 && col < row.size())
                    value = row.at(col).trimmed();
            }
            obj[key] = value;   // absent/out-of-range column -> "" (key still present)
        }
        out.append(obj);
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}
```

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: `100% tests passed`.

- [ ] **Commit.** Stage `qt-app/core/importcontroller.h`, `qt-app/core/importcontroller.cpp`, `qt-app/tests/tst_importcontroller.cpp`. Message: `feat(import): add serializeRows pure static (4a.3 T2)`.

---

## Task 3 — `checkDuplicates(schoolIds, adminKey)` form transport + 401 routing

Change `checkDuplicates` to add `const QString &adminKey`, POST `application/x-www-form-urlencoded` with `school_ids` = a JSON-array string + `admin_key`, and route a 401-with-body to a clear auth message via `StudentController::replyIsServerAnswer`. This is the first Task that needs the request-assembly test seam, so it extends the `tst_importcontroller` build to compile `studentcontroller.cpp` + `capturingnam.cpp`.

**Files:**
- Modify `qt-app/core/importcontroller.h` (`checkDuplicates` signature, line 29).
- Modify `qt-app/core/importcontroller.cpp` (`checkDuplicates` body, ~lines 155-196; add `#include <QUrlQuery>` and `#include "studentcontroller.h"`).
- Modify `qt-app/tests/CMakeLists.txt` (add `studentcontroller`/`csvutil`/`capturingnam` sources + testsupport include to the `tst_importcontroller` target — **reconfigure required**).
- Modify `qt-app/tests/tst_importcontroller.cpp` (add `#include "capturingnam.h"`, `#include <QSignalSpy>`, `<QUrlQuery>`; new slots).

**Interfaces:**
- Produces: `void ImportController::checkDuplicates(const QStringList &schoolIds, const QString &adminKey);`
- Consumes: `static bool StudentController::replyIsServerAnswer(bool replyHadError, int httpStatus, const QByteArray &body);` (existing, PUBLIC).
- Emits (unchanged): `duplicatesResolved(QStringList)` on success; `importError(QString title, QString message, ImportSeverity)` on failure. A 401-with-body emits `importError("Authentication", "Admin authentication required — re-enter via admin login.", ImportSeverity::Critical)`.
- Consumed later by: Task 5 (legacy call site), Task 10 (`ImportViewModel`).

**Steps:**

- [ ] **Extend the test target (build wiring).** In `qt-app/tests/CMakeLists.txt`, edit the `tst_importcontroller` target so it compiles the extra sources and sees `testsupport/`. Replace its `qt_add_executable(...)`, `target_include_directories(...)`, and `target_link_libraries(...)` blocks with:

```cmake
qt_add_executable(tst_importcontroller
    tst_importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/core/importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/core/importcontroller.h
    ${CMAKE_SOURCE_DIR}/core/importdata.h
    ${CMAKE_SOURCE_DIR}/core/studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/core/studentcontroller.h
    ${CMAKE_SOURCE_DIR}/core/studentdata.h
    ${CMAKE_SOURCE_DIR}/core/csvutil.cpp
    ${CMAKE_SOURCE_DIR}/core/csvutil.h
    ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
    ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
)
set_target_properties(tst_importcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_importcontroller PRIVATE
    ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
target_link_libraries(tst_importcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    QXlsx
)
add_test(NAME tst_importcontroller COMMAND tst_importcontroller)
set_tests_properties(tst_importcontroller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

(`studentcontroller.cpp` pulls in `csvutil` via `toCsv`; both are compiled directly here to keep this target self-contained rather than linking `witscore`.)

- [ ] **Write the failing test.** In `tst_importcontroller.cpp` add includes `#include "capturingnam.h"`, `#include <QSignalSpy>`, `#include <QUrlQuery>`, and slot declarations:

```cpp
    // checkDuplicates (request assembly + 401 routing)
    void checkDuplicatesPostsFormWithSchoolIdsAndAdminKey();
    void checkDuplicates401RoutesToAuthError();
```

Bodies:

```cpp
void TestImportController::checkDuplicatesPostsFormWithSchoolIdsAndAdminKey()
{
    CapturingNam nam(R"({"status":"success","duplicates":[]})");
    ImportController controller(&nam);

    controller.checkDuplicates({"21-1-0001", "21-1-0002"}, "s3cr3t-key");

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QVERIFY(nam.lastUrl.toString().endsWith("check_duplicates.php"));
    QCOMPARE(nam.lastContentType, QString("application/x-www-form-urlencoded"));

    // school_ids is a JSON-array string field; admin_key is a sibling field.
    QUrlQuery q(QString::fromUtf8(nam.lastBody));
    QVERIFY(q.hasQueryItem("school_ids"));
    QVERIFY(q.hasQueryItem("admin_key"));
    QCOMPARE(q.queryItemValue("admin_key", QUrl::FullyDecoded), QString("s3cr3t-key"));
    const QString idsField = q.queryItemValue("school_ids", QUrl::FullyDecoded);
    const QJsonArray ids = QJsonDocument::fromJson(idsField.toUtf8()).array();
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0).toString(), QString("21-1-0001"));
    QCOMPARE(ids.at(1).toString(), QString("21-1-0002"));
}

void TestImportController::checkDuplicates401RoutesToAuthError()
{
    // A guard rejection: AuthenticationRequiredError + HTTP 401 + a decodable body.
    CapturingNam nam(R"({"status":"error","message":"Admin authentication required"})",
                     QNetworkReply::AuthenticationRequiredError, 401);
    ImportController controller(&nam);

    QSignalSpy errSpy(&controller, &ImportController::importError);
    QSignalSpy okSpy(&controller, &ImportController::duplicatesResolved);

    controller.checkDuplicates({"21-1-0001"}, "bad-key");
    QVERIFY(errSpy.wait(1000));            // CannedReply finishes on the next loop turn
    QCOMPARE(okSpy.count(), 0);            // NOT reported as a normal resolve
    QCOMPARE(errSpy.count(), 1);
    const QString message = errSpy.at(0).at(1).toString();
    QVERIFY(message.contains("authentication", Qt::CaseInsensitive));
}
```

- [ ] **Run — expect FAIL.** Reconfigure then build+test: `cmake -S qt-app -B C:/b/loams-4a3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: compile error (`checkDuplicates` takes 1 arg, called with 2).

- [ ] **Implement — header.** In `qt-app/core/importcontroller.h`, change line 29 to:

```cpp
    // Async — result arrives via duplicatesResolved / importError. adminKey is
    // sent as a form field (check_duplicates.php is requireAdminAuth-guarded);
    // a 401-with-body routes to importError with a clear auth message.
    void checkDuplicates(const QStringList &schoolIds, const QString &adminKey);
```

- [ ] **Implement — cpp.** Add `#include <QUrlQuery>` and `#include "studentcontroller.h"` to the includes of `importcontroller.cpp`, then replace the whole `checkDuplicates` body:

```cpp
void ImportController::checkDuplicates(const QStringList &schoolIds, const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("check_duplicates.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    // school_ids as a JSON-array string in one form field (mirrors how
    // bulk_update_students sends `students`), admin_key as a sibling field.
    QJsonArray idsArray;
    for (const QString &id : schoolIds)
        idsArray.append(id);
    const QByteArray idsJson = QJsonDocument(idsArray).toJson(QJsonDocument::Compact);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("school_ids"), QString::fromUtf8(idsJson));
    body.addQueryItem(QStringLiteral("admin_key"), adminKey);

    QNetworkReply *reply =
        m_nam->post(request, body.toString(QUrl::FullyEncoded).toUtf8());

    // `this` as the context object: auto-disconnect if the controller dies
    // while the reply (owned by the shared NAM) is still in flight.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray resp = reply->readAll();
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QString errorString = reply->errorString();
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (!StudentController::replyIsServerAnswer(hadError, httpStatus, resp)) {
            // Genuine transport failure (no server answer). Legacy title "Error".
            emit importError(QStringLiteral("Error"), errorString, ImportSeverity::Critical);
            return;
        }

        QStringList duplicates;
        QString errorMsg;
        if (!parseDuplicateResponse(resp, &duplicates, &errorMsg)) {
            // A 401-with-body is a server answer; surface it as a clear auth error.
            if (httpStatus == 401)
                emit importError(QStringLiteral("Authentication"),
                                 QStringLiteral("Admin authentication required — re-enter via admin login."),
                                 ImportSeverity::Critical);
            else
                emit importError(QStringLiteral("Error"), errorMsg, ImportSeverity::Warning);
            return;
        }

        emit duplicatesResolved(duplicates);   // empty list = no duplicates found
    });
}
```

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: `100% tests passed` (the two new slots + all Task 1/2 slots).

- [ ] **Commit.** Stage `qt-app/core/importcontroller.h`, `qt-app/core/importcontroller.cpp`, `qt-app/tests/CMakeLists.txt`, `qt-app/tests/tst_importcontroller.cpp`. Message: `feat(import): guard checkDuplicates with admin_key form transport (4a.3 T3)`.

---

## Task 4 — `uploadStudents(table, zipPath, skipIds, adminKey)` sends rows JSON

Rework `uploadStudents` to accept the parsed `ParsedTable` (calls `serializeRows` internally — **the single seam**), plus `zipPath` + `skipIds` + `adminKey`. Build a `QHttpMultiPart` with a `rows` JSON field, `admin_key`, an optional `skip_ids` field, and an optional `photos_zip` file. Remove the fatal Excel-open branch; keep the non-fatal ZIP-open warning; fire `uploadStarted` immediately before the POST; route 401 to `uploadFailed`.

**Files:**
- Modify `qt-app/core/importcontroller.h` (`uploadStudents` signature, lines 31-33; `uploadStarted` doc comment line 44).
- Modify `qt-app/core/importcontroller.cpp` (`uploadStudents` body, ~lines 198-287).
- Modify `qt-app/tests/tst_importcontroller.cpp` (new slots).

**Interfaces:**
- Produces: `void ImportController::uploadStudents(const ParsedTable &table, const QString &zipPath, const QStringList &skipIds, const QString &adminKey);`
  - **Decision:** takes `const ParsedTable &table` (NOT pre-serialized bytes) so `serializeRows` is exercised from exactly one call site, and the legacy path (Task 5) hands the same struct the Quick path builds.
- Emits (unchanged names): `uploadStarted()` (immediately before `post`), `uploadProgress(int)`, `uploadFinished(UploadResult)`, `uploadFailed(QString)`, `importError(...)` (ZIP-open warning only). A 401-with-body emits `uploadFailed("Admin authentication required — re-enter via admin login.")`.
- Consumed later by: Task 5 (legacy), Task 10 (`ImportViewModel`).

**Steps:**

- [ ] **Write the failing test.** Add slot declarations to `tst_importcontroller.cpp`:

```cpp
    // uploadStudents (multipart assembly)
    void uploadStudentsPostsRowsAndAdminKeyMultipart();
    void uploadStudentsOmitsSkipIdsWhenEmpty();
    void uploadStudents401RoutesToUploadFailed();
```

Bodies (multipart body is captured via `CapturingNam::createRequest(outgoingData)`):

```cpp
static ParsedTable oneRowTable()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz"};
    return t;
}

void TestImportController::uploadStudentsPostsRowsAndAdminKeyMultipart()
{
    CapturingNam nam(R"({"status":"success","success_count":1,"skipped_count":1,"error_count":0})");
    ImportController controller(&nam);
    QSignalSpy startedSpy(&controller, &ImportController::uploadStarted);

    controller.uploadStudents(oneRowTable(), QString(), {"21-1-0002"}, "s3cr3t-key");

    QCOMPARE(startedSpy.count(), 1);   // fires immediately before the POST
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QVERIFY(nam.lastUrl.toString().endsWith("upload_students_zip.php"));

    const QString body = QString::fromUtf8(nam.lastBody);
    QVERIFY(body.contains("name=\"rows\""));
    QVERIFY(body.contains("name=\"admin_key\""));
    QVERIFY(body.contains("name=\"skip_ids\""));   // skipIds non-empty -> present
    QVERIFY(body.contains("21-1-0001"));            // the serialized row payload
    QVERIFY(body.contains("s3cr3t-key"));
    QVERIFY(!body.contains("name=\"excel\""));      // raw Excel part removed
}

void TestImportController::uploadStudentsOmitsSkipIdsWhenEmpty()
{
    CapturingNam nam(R"({"status":"success","success_count":1,"skipped_count":0,"error_count":0})");
    ImportController controller(&nam);

    controller.uploadStudents(oneRowTable(), QString(), QStringList{}, "s3cr3t-key");

    const QString body = QString::fromUtf8(nam.lastBody);
    QVERIFY(body.contains("name=\"rows\""));
    QVERIFY(body.contains("name=\"admin_key\""));
    QVERIFY(!body.contains("name=\"skip_ids\""));   // empty -> absent
}

void TestImportController::uploadStudents401RoutesToUploadFailed()
{
    CapturingNam nam(R"({"status":"error","message":"Admin authentication required"})",
                     QNetworkReply::AuthenticationRequiredError, 401);
    ImportController controller(&nam);
    QSignalSpy failSpy(&controller, &ImportController::uploadFailed);
    QSignalSpy finSpy(&controller, &ImportController::uploadFinished);

    controller.uploadStudents(oneRowTable(), QString(), QStringList{}, "bad-key");
    QVERIFY(failSpy.wait(1000));
    QCOMPARE(finSpy.count(), 0);
    QVERIFY(failSpy.at(0).at(0).toString().contains("authentication", Qt::CaseInsensitive));
}
```

- [ ] **Run — expect FAIL.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: compile error (`uploadStudents` old 3-arg signature vs new call).

- [ ] **Implement — header.** In `qt-app/core/importcontroller.h`, replace lines 31-33:

```cpp
    // Async — result arrives via uploadStarted / uploadProgress /
    // uploadFinished / uploadFailed. Serializes `table` to a `rows` JSON form
    // field (via serializeRows) + admin_key + optional skip_ids + optional
    // photos_zip file. A 401-with-body routes to uploadFailed(auth message).
    void uploadStudents(const ParsedTable &table, const QString &zipPath,
                        const QStringList &skipIds, const QString &adminKey);
```

Update the `uploadStarted` doc comment (line 44) to drop the "excel file opened OK" wording:

```cpp
    void uploadStarted();                        // request about to post
```

- [ ] **Implement — cpp.** Replace the whole `uploadStudents` body in `importcontroller.cpp`:

```cpp
void ImportController::uploadStudents(const ParsedTable &table, const QString &zipPath,
                                      const QStringList &skipIds, const QString &adminKey)
{
    QNetworkRequest uploadRequest(ApiConfig::endpoint(QStringLiteral("upload_students_zip.php")));
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto addText = [multiPart](const QString &name, const QByteArray &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"%1\"").arg(name)));
        part.setBody(value);
        multiPart->append(part);
    };

    // rows: the client-parsed, header-mapped payload (the ONLY serializeRows
    // call site). Replaces the old raw-Excel file part.
    addText(QStringLiteral("rows"), serializeRows(table));
    addText(QStringLiteral("admin_key"), adminKey.toUtf8());   // guard field — never logged

    // skip_ids — only appended when non-empty (comma-joined; preserved).
    if (!skipIds.isEmpty())
        addText(QStringLiteral("skip_ids"), skipIds.join(QLatin1Char(',')).toUtf8());

    // photos_zip (optional, non-fatal on open failure).
    if (!zipPath.isEmpty()) {
        QHttpPart zipPart;
        zipPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"photos_zip\"; filename=\"" +
                                   QFileInfo(zipPath).fileName() + "\""));
        QFile *zipFile = new QFile(zipPath);
        if (!zipFile->open(QIODevice::ReadOnly)) {
            emit importError(QStringLiteral("Warning"),
                             QStringLiteral("Cannot open ZIP file. Proceeding without photos."),
                             ImportSeverity::Warning);
            delete zipFile;
        } else {
            zipPart.setBodyDevice(zipFile);
            zipFile->setParent(multiPart);
            multiPart->append(zipPart);
        }
    }

    // The client already parsed the file, so there is no fatal open branch:
    // fire uploadStarted immediately before the POST.
    emit uploadStarted();

    QNetworkReply *uploadReply = m_nam->post(uploadRequest, multiPart);
    multiPart->setParent(uploadReply);

    connect(uploadReply, &QNetworkReply::uploadProgress, this,
            [this](qint64 bytesSent, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    const int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
                    emit uploadProgress(percent);
                }
            });

    connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply]() {
        const QByteArray response = uploadReply->readAll();
        const bool hadError = uploadReply->error() != QNetworkReply::NoError;
        const QString errorString = uploadReply->errorString();
        const QVariant statusAttr =
            uploadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        uploadReply->deleteLater();

        if (!StudentController::replyIsServerAnswer(hadError, httpStatus, response)) {
            emit uploadFailed(errorString);                    // genuine transport failure
            return;
        }
        if (httpStatus == 401) {
            emit uploadFailed(QStringLiteral(
                "Admin authentication required — re-enter via admin login."));
            return;
        }
        emit uploadFinished(parseUploadResponse(response));    // real JSON counts
    });
}
```

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: `100% tests passed`.

- [ ] **Commit.** Stage `qt-app/core/importcontroller.h`, `qt-app/core/importcontroller.cpp`, `qt-app/tests/tst_importcontroller.cpp`. Message: `feat(import): uploadStudents sends serialized rows + admin_key (4a.3 T4)`.

---

## Task 5 — Legacy `adminwindow.cpp`: build rows from the LIVE table + update 3 call sites

The legacy Widgets app holds no `ParsedTable` — the editable `ui->bulkTable` (a `QTableWidget`) is the source of truth (it deliberately reads the live table so post-preview hand-edits are picked up). Add a private helper that reconstructs a `ParsedTable` from `ui->bulkTable` + `bulkHeaderIndex` (do **not** re-parse `selectedExcelPath` — that would drop hand-edits), and update the three call sites to thread the built table + `m_adminKey`.

> **Testing note (be honest):** this is legacy Widgets glue with **no existing `adminWindow` unit-test harness**, and the row-builder is inseparable from `ui->bulkTable` (a live widget), so there is no clean pure seam to unit-test without standing up a widget host — out of scope for this slice. This Task therefore has **no automated test**; verify by the manual checklist below. Everything else (`serializeRows`, transport) is already unit-tested in Tasks 2-4.

**Files:**
- Modify `qt-app/adminwindow.h` (declare the private helper).
- Modify `qt-app/adminwindow.cpp` (implement the helper; update lines ~918, ~931, ~981).

**Interfaces:**
- Consumes: `ImportController::checkDuplicates(QStringList, QString)` (T3), `ImportController::uploadStudents(ParsedTable, QString, QStringList, QString)` (T4), `ImportController::serializeRows` (indirectly, via the built table), `m_adminKey` (retained via `setAdminKey`).
- Produces (private): `ParsedTable adminWindow::buildBulkTableParsed() const;`

**Steps:**

- [ ] **Declare the helper.** In `qt-app/adminwindow.h`, add to the private methods section (near the other bulk-import members):

```cpp
    // Reconstructs a ParsedTable from the LIVE, user-editable ui->bulkTable +
    // bulkHeaderIndex (NOT by re-parsing selectedExcelPath — that would discard
    // hand-edits made after preview). Feeds the shared serializeRows seam.
    ParsedTable buildBulkTableParsed() const;
```

Ensure `#include "core/importdata.h"` is visible in `adminwindow.h` (it already is transitively via `importcontroller.h`; add it explicitly if the header does not compile).

- [ ] **Implement the helper.** In `qt-app/adminwindow.cpp`, add (near `onUpdateDatabaseBtnClicked`):

```cpp
ParsedTable adminWindow::buildBulkTableParsed() const
{
    ParsedTable table;
    table.headerIndex = bulkHeaderIndex;   // the mapping captured at preview load

    const int cols = ui->bulkTable->columnCount();
    const int rows = ui->bulkTable->rowCount();

    // Header row from the live widget's horizontal header labels, so
    // table.headers stays consistent with headerIndex's column positions.
    for (int c = 0; c < cols; ++c) {
        QTableWidgetItem *h = ui->bulkTable->horizontalHeaderItem(c);
        table.headers << (h ? h->text() : QString());
    }

    for (int r = 0; r < rows; ++r) {
        QStringList row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c) {
            QTableWidgetItem *it = ui->bulkTable->item(r, c);
            row << (it ? it->text().trimmed() : QString());
        }
        table.rows << row;
    }
    return table;
}
```

- [ ] **Update call site 1 — `checkDuplicates` (~line 918).** In `onUpdateDatabaseBtnClicked`, change:

```cpp
    m_importController->checkDuplicates(schoolIds);
```
to
```cpp
    m_importController->checkDuplicates(schoolIds, m_adminKey);
```

- [ ] **Update call site 2 — no-duplicates upload (~line 931).** In `onImportDuplicatesResolved`, change:

```cpp
        m_importController->uploadStudents(selectedExcelPath, selectedZipPath, QStringList{});
        return;
```
to
```cpp
        m_importController->uploadStudents(buildBulkTableParsed(), selectedZipPath,
                                           QStringList{}, m_adminKey);
        return;
```

- [ ] **Update call site 3 — skip-duplicates upload (~line 981).** Change:

```cpp
    m_importController->uploadStudents(selectedExcelPath, selectedZipPath, duplicates);
```
to
```cpp
    m_importController->uploadStudents(buildBulkTableParsed(), selectedZipPath,
                                       duplicates, m_adminKey);
```

Leave every other legacy behavior (the duplicate dialog, the ZIP question, the progress/cancel wiring) untouched.

- [ ] **Build — expect PASS (compile).** `cmake --build C:/b/loams-4a3` — expected: `WITS` (and everything else) compiles with no new warnings. Run the full suite to confirm nothing regressed: `ctest --test-dir C:/b/loams-4a3 --output-on-failure` — expected: all currently-registered targets green.

- [ ] **Manual verification (legacy, since there is no adminWindow test).** Run `WITS.exe`, open admin, go to bulk registration, attach a `.csv` (or `.xlsx`) with an out-of-A–G column order, **hand-edit a cell in the preview table**, then Update Database. Confirm: (a) the hand-edited value is what lands (not the original file value), (b) the duplicate prompt still works, (c) import succeeds against the guarded endpoint with the held admin key, (d) importing with no admin key set 401s cleanly. (Backend must be the Task 6/7 version, deployed.)

- [ ] **Commit.** Stage `qt-app/adminwindow.h`, `qt-app/adminwindow.cpp`. Message: `feat(import): legacy adminwindow builds rows from live table + threads admin_key (4a.3 T5)`.

---

## Task 6 — Backend `upload_students_zip.php` rework

Guard first, read client-sent `rows`, honor `skip_ids`, extract the optional photos ZIP with core `ZipArchive`, per-row validate + glob-match photo + checked INSERT of the 7 core columns plus `photo`, and return JSON with real counts. Remove PhpSpreadsheet / Composer.

> **Testing note:** there is **no PHP unit-test harness** in this repo. This Task's verification is the manual checklist below — do not fabricate a PHP test target.

**Files:**
- Rewrite `deliverables/loams_api/upload_students_zip.php`.

**Interfaces (HTTP contract):**
- Consumes (POST `multipart/form-data`): `rows` (JSON-array string of 7-key objects), `admin_key`, optional `skip_ids` (comma-joined), optional `photos_zip` file. Matches Task 4's request assembly.
- Produces (200 JSON): `{ "status":"success", "success_count":<int>, "skipped_count":<int>, "error_count":<int>, "message":"..." }`. On missing/invalid key: 401 `{ "status":"error", "message":"..." }` (from `requireAdminAuth`).

**Steps:**

- [ ] **Rewrite the endpoint** to the following (full file):

```php
<?php
error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401 before any DB read, ZIP extract, or insert

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    echo json_encode(["status" => "error", "message" => "POST required"]);
    exit;
}

// === 1. Client-sent, header-mapped rows (JSON array of 7-key objects) ===
$rowsJson = isset($_POST['rows']) ? $_POST['rows'] : '';
$rows = json_decode($rowsJson, true);
if (!is_array($rows)) {
    echo json_encode(["status" => "error", "message" => "Invalid rows payload."]);
    exit;
}

// === 2. Skip-set from skip_ids (comma-joined) ===
$skipSet = [];
if (!empty($_POST['skip_ids'])) {
    foreach (explode(',', $_POST['skip_ids']) as $sid) {
        $sid = trim($sid);
        if ($sid !== '') $skipSet[$sid] = true;
    }
}

// === 3. Optional photos ZIP — extracted ONCE, before the row loop (core ext-zip) ===
$uploadDir = "uploads/temp/";
if (!is_dir($uploadDir)) mkdir($uploadDir, 0777, true);
$photoDir = $uploadDir . "photos/";
$zipExtracted = false;

if (!empty($_FILES['photos_zip']['tmp_name'])) {
    $zipPath = $uploadDir . basename($_FILES['photos_zip']['name']);
    move_uploaded_file($_FILES['photos_zip']['tmp_name'], $zipPath);
    $zip = new ZipArchive;
    if ($zip->open($zipPath) === TRUE) {
        $zip->extractTo($photoDir);
        $zip->close();
        $zipExtracted = true;
    }
    // A failed ZIP open is non-fatal: import proceeds with photo = NULL.
}

if (!is_dir("uploads/students/")) mkdir("uploads/students/", 0777, true);

// === 4. Per-row insert ===
$success_count = 0;
$skipped_count = 0;
$error_count   = 0;

$stmt = $conn->prepare("INSERT INTO students
    (school_id, name, course, department, year_level, gender, status, photo)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

if (!$stmt) {
    echo json_encode(["status" => "error", "message" => "Prepare failed: " . $conn->error]);
    exit;
}

foreach ($rows as $row) {
    $school_id  = isset($row['school_id'])  ? trim($row['school_id'])  : '';
    $name       = isset($row['name'])       ? trim($row['name'])       : '';
    $course     = isset($row['course'])     ? trim($row['course'])     : '';
    $department = isset($row['department'])  ? trim($row['department']) : '';
    $year_level = isset($row['year_level'])  ? trim($row['year_level']) : '';
    $gender     = isset($row['gender'])      ? trim($row['gender'])     : '';
    $status     = isset($row['status'])      ? trim($row['status'])     : '';

    // Server-side re-validate: school_id + name required (the data guarantee).
    if ($school_id === '' || $name === '') {
        $error_count++;
        continue;
    }
    // Client-resolved duplicates are skipped here.
    if (isset($skipSet[$school_id])) {
        $skipped_count++;
        continue;
    }

    // Photo comes ONLY from the ZIP match (never from a file column).
    $photoPath = null;
    if ($zipExtracted) {
        $candidates = glob($photoDir . "*" . $school_id . "*.*");
        if ($candidates && count($candidates) > 0) {
            $targetPhoto = "uploads/students/" . $school_id . ".jpg";
            if (copy($candidates[0], $targetPhoto)) {
                $photoPath = $targetPhoto;
            }
        }
    }

    $stmt->bind_param("ssssssss",
        $school_id, $name, $course, $department,
        $year_level, $gender, $status, $photoPath);

    if ($stmt->execute()) {
        $success_count++;
    } else {
        $error_count++;
    }
}

$stmt->close();

echo json_encode([
    "status"        => "success",
    "success_count" => $success_count,
    "skipped_count" => $skipped_count,
    "error_count"   => $error_count,
    "message"       => "Imported $success_count, skipped $skipped_count, failed $error_count."
]);
?>
```

Note: `use PhpOffice\PhpSpreadsheet\IOFactory;` and `require 'vendor/autoload.php';` are **removed** — this file no longer parses spreadsheets. `code` and `visits` are omitted from the INSERT (DB defaults). The `*school_id*` glob is the pre-existing path-shape concern noted in spec §9 — now admin-gated; do **not** attempt to fully sanitize here (deferred to Phase 6).

- [ ] **Manual verification checklist** (no unit harness — run these against a deployed copy):
  - `php -l deliverables/loams_api/upload_students_zip.php` → "No syntax errors detected".
  - POST with **no** `admin_key` → HTTP **401** `{"status":"error",...}`, zero rows inserted.
  - POST valid `rows` + valid key, **no** ZIP → `success_count` matches inserted rows, `photo` NULL.
  - POST valid `rows` + valid key **with** a ZIP whose filenames contain the school IDs → matched rows get a `photo` path.
  - Include a `skip_ids` value present in `rows` → that row increments `skipped_count`, not `success_count`.
  - Include a row with empty `school_id` → increments `error_count`, not inserted.

- [ ] **Commit.** Stage `deliverables/loams_api/upload_students_zip.php`. Message: `feat(api): rework upload_students_zip to guarded row inserter, drop Composer (4a.3 T6)`.

---

## Task 7 — Backend `check_duplicates.php` guard + form input

Guard the endpoint and read `school_ids` from a `json_decode`'d `$_POST['school_ids']` form field (moved off the raw JSON body). Response shape unchanged.

> **Testing note:** no PHP unit harness — verify via `php -l` + the manual checklist.

**Files:**
- Rewrite `deliverables/loams_api/check_duplicates.php`.

**Interfaces (HTTP contract):**
- Consumes (POST `x-www-form-urlencoded`): `school_ids` (JSON-array string) + `admin_key`. Matches Task 3's request assembly.
- Produces (200 JSON, **unchanged shape**): `{ "status":"success", "duplicates":[...] }`. 401 on missing/invalid key.

**Steps:**

- [ ] **Rewrite the endpoint** (full file):

```php
<?php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";
requireAdminAuth($conn);   // 401 before any read

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    echo json_encode(["status" => "error", "message" => "POST required"]);
    exit;
}

// school_ids arrives as a JSON-array string in a form field (moved off the raw
// JSON body so requireAdminAuth can read admin_key from $_POST).
$idsJson = isset($_POST['school_ids']) ? $_POST['school_ids'] : '';
$school_ids = json_decode($idsJson, true);

if (!is_array($school_ids)) {
    echo json_encode(["status" => "error", "message" => "Invalid input"]);
    exit;
}

$duplicates = [];
$stmt = $conn->prepare("SELECT id FROM students WHERE school_id = ?");
foreach ($school_ids as $school_id) {
    $stmt->bind_param("s", $school_id);
    $stmt->execute();
    $result = $stmt->get_result();
    if ($result->num_rows > 0) {
        $duplicates[] = $school_id;
    }
}
$stmt->close();

echo json_encode([
    "status" => "success",
    "duplicates" => $duplicates
]);
?>
```

- [ ] **Manual verification checklist:**
  - `php -l deliverables/loams_api/check_duplicates.php` → "No syntax errors detected".
  - POST with no `admin_key` → 401.
  - POST valid key + `school_ids=["<existing>","<new>"]` → `{"status":"success","duplicates":["<existing>"]}`.

- [ ] **Commit.** Stage `deliverables/loams_api/check_duplicates.php`. Message: `feat(api): guard check_duplicates + read school_ids form field (4a.3 T7)`.

---

## Task 8 — Contract doc

Document the before/after request/response for both endpoints, the "Composer no longer required" note, and the BREAKING deploy note.

**Files:**
- Create `docs/superpowers/contracts/2026-08-11-phase4a3-import-endpoints.md`.

**Interfaces:** Documentation only. No code, no test.

**Steps:**

- [ ] **Create the doc** with this content (placeholders only — no real keys/PII):

```markdown
# Phase 4a.3 — Import Endpoints Contract

> Date: 2026-08-11 · Track 4a (Database + Import) · **BREAKING** — deploy WITH the client.
> `<ADMIN_KEY>` denotes the RAM-only admin key. No real keys or student PII here.

Both endpoints are now guarded by `requireAdminAuth($conn)` (bcrypt-verified
`$_POST['admin_key']`) and reject a missing/invalid key with HTTP **401**. An old
client that does not thread `admin_key` will 401 — ship client + endpoints together.

## check_duplicates.php

**Before** — JSON body, unguarded:

    POST check_duplicates.php
    Content-Type: application/json
    { "school_ids": ["21-1-0001", "21-1-0002"] }
    -> 200 { "status":"success", "duplicates":["21-1-0001"] }

**After** — form fields, guarded (response shape unchanged):

    POST check_duplicates.php
    Content-Type: application/x-www-form-urlencoded
    school_ids=["21-1-0001","21-1-0002"]   (JSON-array string)
    admin_key=<ADMIN_KEY>
    -> 200 { "status":"success", "duplicates":["21-1-0001"] }
    -> 401 { "status":"error", "message":"..." }

## upload_students_zip.php

**Before** — raw Excel file part; server re-parsed via PhpSpreadsheet (Composer);
`skip_ids` ignored; plain-text reply; fabricated counts; unguarded.

    POST upload_students_zip.php  (multipart/form-data)
      excel=<students.xlsx>       (server re-parsed positional A-G)
      photos_zip=<photos.zip>     (optional)
      skip_ids=21-1-0001,...      (IGNORED)
    -> 200 text/plain "OK Upload complete!"   (no real counts)

**After** — client-sent rows; guarded; honors skip_ids; real JSON counts; no Composer.

    POST upload_students_zip.php  (multipart/form-data)
      rows=[{"school_id":"21-1-0001","name":"...","course":"...","department":"...",
             "year_level":"...","gender":"...","status":"..."}, ...]   (JSON-array string field)
      admin_key=<ADMIN_KEY>
      skip_ids=21-1-0001,...      (comma-joined; honored)
      photos_zip=<photos.zip>     (optional; core ZipArchive, glob *school_id*)
    -> 200 { "status":"success",
             "success_count":<int>, "skipped_count":<int>, "error_count":<int>,
             "message":"..." }
    -> 401 { "status":"error", "message":"..." }

INSERT writes: `school_id, name, course, department, year_level, gender, status, photo`
(`photo` = the ZIP-matched path for the row's `school_id`, else NULL). `code` and
`visits` are excluded and left to their DB defaults.

## Composer no longer required

`upload_students_zip.php` no longer requires `PhpOffice\PhpSpreadsheet` or
`vendor/autoload.php`. The only PHP extension it needs is core **ext-zip**, and
only for the optional photos ZIP.
```

- [ ] **Commit.** Stage `docs/superpowers/contracts/2026-08-11-phase4a3-import-endpoints.md`. Message: `docs(api): 4a.3 import endpoint contract (before/after, BREAKING) (4a.3 T8)`.

---

## Task 9 — Client-side validation (pure/testable)

Add a pure static that validates a `ParsedTable` for import (required columns present, each row has a non-empty `school_id`), returning an empty string on OK else a friendly message and optionally listing offending rows.

**Files:**
- Modify `qt-app/core/importcontroller.h` (add the static declaration).
- Modify `qt-app/core/importcontroller.cpp` (add the implementation).
- Modify `qt-app/tests/tst_importcontroller.cpp` (new slots).

**Interfaces:**
- Produces: `static QString ImportController::validateForImport(const ParsedTable &table, QStringList *badRowsOut = nullptr);`
  - Returns `""` when the table is importable.
  - If `headerIndex` lacks `school_id` → `"Missing required column: School ID.\nFound columns: <headers joined by ', '>"`.
  - Else if `headerIndex` lacks `name` → `"Missing required column: Name.\nFound columns: <...>"`.
  - Else, for each data row whose `school_id` cell is empty/out-of-range, append the human label `"Row N"` (N is 1-based over data rows) to `*badRowsOut` (when non-null). If any offenders → return `"Some rows have no School ID: <first 3 labels, comma-joined><, and more if >3>"`.
  - Extra columns are ignored (never an error).
- Consumed later by: Task 10 (`ImportViewModel::startImport`).

**Steps:**

- [ ] **Write the failing test.** Add slot declarations:

```cpp
    // validateForImport
    void validateForImportOkOnGoodTable();
    void validateForImportMissingSchoolIdColumn();
    void validateForImportMissingNameColumn();
    void validateForImportEmptySchoolIdRowsReported();
    void validateForImportIgnoresExtraColumns();
```

Bodies:

```cpp
void TestImportController::validateForImportOkOnGoodTable()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz"};

    QStringList bad;
    QCOMPARE(ImportController::validateForImport(t, &bad), QString());   // "" == OK
    QVERIFY(bad.isEmpty());
}

void TestImportController::validateForImportMissingSchoolIdColumn()
{
    ParsedTable t;
    t.headers = {"Full Name", "Course"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // no school_id
    t.rows << QStringList{"Juan Dela Cruz", "BSIT"};

    const QString msg = ImportController::validateForImport(t, nullptr);
    QVERIFY(msg.contains("School ID"));
    QVERIFY(msg.contains("Found columns"));
}

void TestImportController::validateForImportMissingNameColumn()
{
    ParsedTable t;
    t.headers = {"School ID", "Course"};
    ImportController::mapHeaders(t.headers, t.headerIndex);   // no name
    t.rows << QStringList{"21-1-0001", "BSIT"};

    const QString msg = ImportController::validateForImport(t, nullptr);
    QVERIFY(msg.contains("Name"));
}

void TestImportController::validateForImportEmptySchoolIdRowsReported()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name"};
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz"};
    t.rows << QStringList{"", "Maria Clara"};          // row 2: empty school_id

    QStringList bad;
    const QString msg = ImportController::validateForImport(t, &bad);
    QVERIFY(!msg.isEmpty());
    QCOMPARE(bad, QStringList({"Row 2"}));
}

void TestImportController::validateForImportIgnoresExtraColumns()
{
    ParsedTable t;
    t.headers = {"School ID", "Full Name", "Notes"};   // Notes -> col_2, ignored
    ImportController::mapHeaders(t.headers, t.headerIndex);
    t.rows << QStringList{"21-1-0001", "Juan Dela Cruz", "vip"};

    QStringList bad;
    QCOMPARE(ImportController::validateForImport(t, &bad), QString());   // extra col is fine
}
```

- [ ] **Run — expect FAIL.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: compile error (`validateForImport` undeclared).

- [ ] **Implement — header.** In `qt-app/core/importcontroller.h`, after `serializeRows`:

```cpp
    // Pure client-side validation (4a.3, spec §7). Returns "" when importable;
    // else a friendly message. Requires school_id + name columns (else names
    // the found columns); each data row must have a non-empty school_id (empty
    // ones are appended to *badRowsOut as "Row N", 1-based over data rows).
    // Extra columns are ignored, never an error.
    static QString validateForImport(const ParsedTable &table,
                                     QStringList *badRowsOut = nullptr);
```

- [ ] **Implement — cpp.** Add to `importcontroller.cpp` (after `serializeRows`):

```cpp
QString ImportController::validateForImport(const ParsedTable &table, QStringList *badRowsOut)
{
    if (badRowsOut)
        badRowsOut->clear();

    const QString found = QStringLiteral("\nFound columns: ") + table.headers.join(QStringLiteral(", "));

    if (!table.headerIndex.contains(QStringLiteral("school_id")))
        return QStringLiteral("Missing required column: School ID.") + found;
    if (!table.headerIndex.contains(QStringLiteral("name")))
        return QStringLiteral("Missing required column: Name.") + found;

    const int idCol = table.headerIndex.value(QStringLiteral("school_id"));
    QStringList offenders;
    for (int r = 0; r < table.rows.size(); ++r) {
        const QStringList &row = table.rows.at(r);
        const QString id = (idCol >= 0 && idCol < row.size()) ? row.at(idCol).trimmed() : QString();
        if (id.isEmpty())
            offenders << QStringLiteral("Row %1").arg(r + 1);   // 1-based over data rows
    }

    if (badRowsOut)
        *badRowsOut = offenders;

    if (!offenders.isEmpty()) {
        const QStringList head = offenders.mid(0, 3);
        QString msg = QStringLiteral("Some rows have no School ID: ") + head.join(QStringLiteral(", "));
        if (offenders.size() > 3)
            msg += QStringLiteral(", and more");
        return msg;
    }
    return QString();   // OK
}
```

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: `100% tests passed`.

- [ ] **Commit.** Stage `qt-app/core/importcontroller.h`, `qt-app/core/importcontroller.cpp`, `qt-app/tests/tst_importcontroller.cpp`. Message: `feat(import): add validateForImport pure static (4a.3 T9)`.

---

## Task 10 — `ImportViewModel` state machine

Create the sole QML-facing surface for import: owns an `ImportController` + its own `QNetworkAccessManager` (mirroring `DatabaseViewModel`), drives the idle → check-duplicates → upload → result flow, and exposes state as `Q_PROPERTY` + actions as `Q_INVOKABLE`. Register it for QML the same way `DatabaseViewModel` is (via `QML_ELEMENT` + adding the sources to `witsquickmodule`). Tests drive it network-free through public `on*` slots (the `DatabaseViewModel` precedent).

**Files:**
- Create `qt-app/quick/viewmodels/ImportViewModel.h`.
- Create `qt-app/quick/viewmodels/ImportViewModel.cpp`.
- Modify `qt-app/quick/CMakeLists.txt` (add the two sources to `witsquickmodule` SOURCES; register the new `tst_importviewmodel` test target).
- Create `qt-app/quick/tests/tst_importviewmodel.cpp`.

**Interfaces (later Tasks + QML depend on these EXACT names):**
- Type: `ImportViewModel` (QML `import LOAMS; ImportViewModel {}`).
- Enum: `enum class Phase { Idle, CheckingDuplicates, AwaitingDuplicates, Uploading, Processing, Done, Failed }` (`Q_ENUM`).
- Q_PROPERTY (all NOTIFYing): `Phase phase`, `bool busy`, `int parsedCount`, `int duplicateCount`, `bool allDuplicates`, `int uploadPercent`, `QString dataFileName`, `QString photosZipName`, `QString errorText`, `QString resultText`, `bool authFailure`.
- Q_INVOKABLE: `void setDataFile(const QUrl&)`, `void setPhotosZip(const QUrl&)`, `void clearPhotosZip()`, `void startImport()`, `void continueAfterDuplicates()`, `void cancel()`, `bool downloadTemplate(const QUrl&)` (Task 11 implements the body).
- Public test-seam slots: `onDuplicatesResolved(QStringList)`, `onImportError(QString,QString,ImportSeverity)`, `onUploadStarted()`, `onUploadProgress(int)`, `onUploadFinished(UploadResult)`, `onUploadFailed(QString)`.
- Consumes: `ImportController` (T2/T3/T4/T9 API), `AdminSession::instance().key()`, `SettingsViewModel::isAuthFailureMessage(QString)` (existing static, used by `DatabaseViewModel`).

**Steps:**

- [ ] **Write the failing test.** Create `qt-app/quick/tests/tst_importviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include "ImportViewModel.h"
#include "importdata.h"
#include "AdminSession.h"

class TestImportViewModel : public QObject
{
    Q_OBJECT
private slots:
    void idleToDuplicatesToUploadToResult();
    void allDuplicatesBlocksContinue();
    void validationFailureStaysIdle();
    void uploadFailedAuthSetsAuthFailure();

private:
    // Writes a minimal CSV and returns its file:// URL.
    static QUrl writeCsv(QTemporaryDir &dir, const QString &name, const QString &text)
    {
        const QString path = dir.filePath(name);
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(text.toUtf8());
        f.close();
        return QUrl::fromLocalFile(path);
    }
};

void TestImportViewModel::idleToDuplicatesToUploadToResult()
{
    AdminSession::instance().setKey("s3cr3t-key");
    QTemporaryDir dir;
    ImportViewModel vm;
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Idle);

    vm.setDataFile(writeCsv(dir, "in.csv",
        "School ID,Full Name\n21-1-0001,Juan Dela Cruz\n21-1-0002,Maria Clara\n"));
    QCOMPARE(vm.parsedCount(), 2);

    vm.startImport();
    QCOMPARE(vm.phase(), ImportViewModel::Phase::CheckingDuplicates);

    // Simulate the controller answering "no duplicates" -> upload begins.
    vm.onDuplicatesResolved({});
    vm.onUploadStarted();
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Uploading);
    vm.onUploadProgress(100);
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Processing);

    UploadResult r; r.ok = true; r.successCount = 2; r.skippedCount = 0; r.errorCount = 0;
    vm.onUploadFinished(r);
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Done);
    QVERIFY(vm.resultText().contains("2"));
    QVERIFY(vm.resultText().contains("imported", Qt::CaseInsensitive));
}

void TestImportViewModel::allDuplicatesBlocksContinue()
{
    AdminSession::instance().setKey("s3cr3t-key");
    QTemporaryDir dir;
    ImportViewModel vm;
    vm.setDataFile(writeCsv(dir, "in.csv",
        "School ID,Full Name\n21-1-0001,Juan\n21-1-0002,Maria\n"));
    vm.startImport();

    vm.onDuplicatesResolved({"21-1-0001", "21-1-0002"});   // ALL duplicates
    QCOMPARE(vm.phase(), ImportViewModel::Phase::AwaitingDuplicates);
    QVERIFY(vm.allDuplicates());

    // continueAfterDuplicates must be a no-op when everything is a duplicate.
    vm.continueAfterDuplicates();
    QVERIFY(vm.phase() != ImportViewModel::Phase::Uploading);
}

void TestImportViewModel::validationFailureStaysIdle()
{
    AdminSession::instance().setKey("s3cr3t-key");
    QTemporaryDir dir;
    ImportViewModel vm;
    vm.setDataFile(writeCsv(dir, "bad.csv",
        "Full Name,Course\nJuan,BSIT\n"));   // no School ID column
    vm.startImport();

    QCOMPARE(vm.phase(), ImportViewModel::Phase::Idle);
    QVERIFY(vm.errorText().contains("School ID"));
}

void TestImportViewModel::uploadFailedAuthSetsAuthFailure()
{
    AdminSession::instance().setKey("bad-key");
    ImportViewModel vm;
    vm.onUploadFailed(QStringLiteral(
        "Admin authentication required — re-enter via admin login."));
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Failed);
    QVERIFY(vm.authFailure());
    QVERIFY(!vm.errorText().isEmpty());
}

QTEST_MAIN(TestImportViewModel)
#include "tst_importviewmodel.moc"
```

- [ ] **Register the test target + module sources.** In `qt-app/quick/CMakeLists.txt`:
  1. In the `qt_add_qml_module(witsquickmodule ... SOURCES ...)` list, add after the `DatabaseViewModel` line:
     ```cmake
        viewmodels/ImportViewModel.cpp viewmodels/ImportViewModel.h
     ```
  2. Add a new test target near `tst_databaseviewmodel`:
     ```cmake
     # --- ImportViewModel unit test (C++ QtTest, offscreen). The VM builds a real
     # QNetworkAccessManager + ImportController; startImport() fires a
     # fire-and-forget post() (harmless, never resolves in-test), and the state
     # machine is driven via the public on* slots — mirrors tst_databaseviewmodel. ---
     wits_add_qttest(tst_importviewmodel
         SOURCES tests/tst_importviewmodel.cpp
         LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
         OFFSCREEN)
     ```

- [ ] **Run — expect FAIL.** Reconfigure + build: `cmake -S qt-app -B C:/b/loams-4a3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importviewmodel --output-on-failure` — expected: compile error (`ImportViewModel.h` not found).

- [ ] **Implement — header.** Create `qt-app/quick/viewmodels/ImportViewModel.h`:

```cpp
#ifndef IMPORTVIEWMODEL_H
#define IMPORTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqml.h>                 // QML_ELEMENT — DatabaseScreen instantiates this type
#include "importdata.h"

class QNetworkAccessManager;
class ImportController;

// Import screen VM (spec §4.1, increment 4a.3). The ONLY QML-facing surface for
// bulk import. Owns an ImportController + its own QNetworkAccessManager
// (mirrors DatabaseViewModel's ownership). Drives the idle -> check-duplicates
// -> upload -> result flow; parse + client validation are delegated to the
// ImportController pure statics. cancel() is pre-upload only (there is no
// in-flight abort path — spec §4.1).
class ImportViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Phase phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int parsedCount READ parsedCount NOTIFY parsedCountChanged)
    Q_PROPERTY(int duplicateCount READ duplicateCount NOTIFY duplicateCountChanged)
    Q_PROPERTY(bool allDuplicates READ allDuplicates NOTIFY allDuplicatesChanged)
    Q_PROPERTY(int uploadPercent READ uploadPercent NOTIFY uploadPercentChanged)
    Q_PROPERTY(QString dataFileName READ dataFileName NOTIFY dataFileNameChanged)
    Q_PROPERTY(QString photosZipName READ photosZipName NOTIFY photosZipNameChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultTextChanged)
    Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)
public:
    explicit ImportViewModel(QObject *parent = nullptr);

    enum class Phase { Idle, CheckingDuplicates, AwaitingDuplicates,
                       Uploading, Processing, Done, Failed };
    Q_ENUM(Phase)

    Phase phase() const { return m_phase; }
    bool busy() const { return m_phase == Phase::CheckingDuplicates
                            || m_phase == Phase::Uploading
                            || m_phase == Phase::Processing; }
    int parsedCount() const { return m_table.rows.size(); }
    int duplicateCount() const { return m_duplicates.size(); }
    bool allDuplicates() const { return parsedCount() > 0
                                     && m_duplicates.size() == parsedCount(); }
    int uploadPercent() const { return m_uploadPercent; }
    QString dataFileName() const { return m_dataFileName; }
    QString photosZipName() const { return m_photosZipName; }
    QString errorText() const { return m_errorText; }
    QString resultText() const { return m_resultText; }
    bool authFailure() const { return m_authFailure; }

    Q_INVOKABLE void setDataFile(const QUrl &fileUrl);
    Q_INVOKABLE void setPhotosZip(const QUrl &fileUrl);
    Q_INVOKABLE void clearPhotosZip();
    Q_INVOKABLE void startImport();
    Q_INVOKABLE void continueAfterDuplicates();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool downloadTemplate(const QUrl &fileUrl);   // body: Task 11

    // Public slots (test seam — driven network-free, like DatabaseViewModel).
    void onDuplicatesResolved(const QStringList &duplicates);
    void onImportError(const QString &title, const QString &message, ImportSeverity severity);
    void onUploadStarted();
    void onUploadProgress(int percent);
    void onUploadFinished(const UploadResult &result);
    void onUploadFailed(const QString &message);

signals:
    void phaseChanged();
    void busyChanged();
    void parsedCountChanged();
    void duplicateCountChanged();
    void allDuplicatesChanged();
    void uploadPercentChanged();
    void dataFileNameChanged();
    void photosZipNameChanged();
    void errorTextChanged();
    void resultTextChanged();
    void authFailureChanged();
    void finishedOk();    // upload succeeded — the dialog can auto-close/toast

private:
    void setPhase(Phase p);
    void setError(const QString &e);
    void beginUpload();

    QNetworkAccessManager *m_nam = nullptr;
    ImportController *m_controller = nullptr;

    QString m_dataFilePath, m_dataFileName;
    QString m_zipPath, m_photosZipName;
    ParsedTable m_table;
    QStringList m_duplicates;

    Phase m_phase = Phase::Idle;
    int m_uploadPercent = 0;
    QString m_errorText, m_resultText;
    bool m_authFailure = false;
};

#endif // IMPORTVIEWMODEL_H
```

- [ ] **Implement — cpp.** Create `qt-app/quick/viewmodels/ImportViewModel.cpp` (the `downloadTemplate` body is filled in Task 11 — leave the stub returning `false` here so it compiles):

```cpp
#include "ImportViewModel.h"

#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include "importcontroller.h"
#include "AdminSession.h"
#include "SettingsViewModel.h"

ImportViewModel::ImportViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new ImportController(m_nam, this))
{
    connect(m_controller, &ImportController::duplicatesResolved,
            this, &ImportViewModel::onDuplicatesResolved);
    connect(m_controller, &ImportController::importError,
            this, &ImportViewModel::onImportError);
    connect(m_controller, &ImportController::uploadStarted,
            this, &ImportViewModel::onUploadStarted);
    connect(m_controller, &ImportController::uploadProgress,
            this, &ImportViewModel::onUploadProgress);
    connect(m_controller, &ImportController::uploadFinished,
            this, &ImportViewModel::onUploadFinished);
    connect(m_controller, &ImportController::uploadFailed,
            this, &ImportViewModel::onUploadFailed);
}

void ImportViewModel::setPhase(Phase p)
{
    if (m_phase == p) return;
    const bool wasBusy = busy();
    m_phase = p;
    emit phaseChanged();
    if (busy() != wasBusy) emit busyChanged();
}

void ImportViewModel::setError(const QString &e)
{
    if (m_errorText != e) { m_errorText = e; emit errorTextChanged(); }
}

void ImportViewModel::setDataFile(const QUrl &fileUrl)
{
    m_dataFilePath = fileUrl.toLocalFile();
    m_dataFileName = QFileInfo(m_dataFilePath).fileName();
    emit dataFileNameChanged();

    // Parse eagerly so parsedCount is available for the Import button/hint.
    ParsedTable parsed;
    if (m_dataFilePath.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive)) {
        parsed = m_controller->parseExcel(m_dataFilePath, nullptr);
    } else if (m_dataFilePath.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        QFile f(m_dataFilePath);
        if (f.open(QIODevice::ReadOnly))
            parsed = ImportController::parseCsv(QString::fromUtf8(f.readAll()));
    }
    m_table = parsed;
    m_duplicates.clear();
    setError(QString());
    if (m_authFailure) { m_authFailure = false; emit authFailureChanged(); }
    if (!m_resultText.isEmpty()) { m_resultText.clear(); emit resultTextChanged(); }
    setPhase(Phase::Idle);
    emit parsedCountChanged();
    emit duplicateCountChanged();
    emit allDuplicatesChanged();
}

void ImportViewModel::setPhotosZip(const QUrl &fileUrl)
{
    m_zipPath = fileUrl.toLocalFile();
    m_photosZipName = QFileInfo(m_zipPath).fileName();
    emit photosZipNameChanged();
}

void ImportViewModel::clearPhotosZip()
{
    if (m_zipPath.isEmpty() && m_photosZipName.isEmpty()) return;
    m_zipPath.clear();
    m_photosZipName.clear();
    emit photosZipNameChanged();
}

void ImportViewModel::startImport()
{
    if (busy() || m_dataFilePath.isEmpty()) return;   // re-entry + precondition guard

    QStringList bad;
    const QString problem = ImportController::validateForImport(m_table, &bad);
    if (!problem.isEmpty()) {
        setError(problem);
        setPhase(Phase::Idle);      // stay in idle on a validation failure
        return;
    }
    setError(QString());

    const int idCol = m_table.headerIndex.value(QStringLiteral("school_id"));
    QStringList schoolIds;
    for (const QStringList &row : m_table.rows) {
        const QString id = (idCol >= 0 && idCol < row.size()) ? row.at(idCol).trimmed() : QString();
        if (!id.isEmpty())
            schoolIds << id;
    }

    setPhase(Phase::CheckingDuplicates);
    m_controller->checkDuplicates(schoolIds, AdminSession::instance().key());
}

void ImportViewModel::onDuplicatesResolved(const QStringList &duplicates)
{
    m_duplicates = duplicates;
    emit duplicateCountChanged();
    emit allDuplicatesChanged();

    if (duplicates.isEmpty()) {
        beginUpload();                       // (a) none -> upload
        return;
    }
    setPhase(Phase::AwaitingDuplicates);     // (b) some / (c) all -> inline confirm
}

void ImportViewModel::continueAfterDuplicates()
{
    if (m_phase != Phase::AwaitingDuplicates) return;
    if (allDuplicates()) return;             // (c) all-dupes -> Close only, no upload
    beginUpload();
}

void ImportViewModel::beginUpload()
{
    // skipIds = the client-resolved duplicates. uploadStarted flips the phase.
    m_controller->uploadStudents(m_table, m_zipPath, m_duplicates,
                                 AdminSession::instance().key());
}

void ImportViewModel::onUploadStarted()
{
    m_uploadPercent = 0; emit uploadPercentChanged();
    setPhase(Phase::Uploading);
}

void ImportViewModel::onUploadProgress(int percent)
{
    m_uploadPercent = percent; emit uploadPercentChanged();
    // Bytes are on the wire; once sent, the real wait is the server row insert.
    if (percent >= 100)
        setPhase(Phase::Processing);
}

void ImportViewModel::onUploadFinished(const UploadResult &result)
{
    if (result.plainText) {
        m_resultText = result.rawText;       // older/partly-deployed endpoint
    } else {
        m_resultText = tr("%1 imported \u00B7 %2 skipped \u00B7 %3 failed")
                           .arg(result.successCount).arg(result.skippedCount).arg(result.errorCount);
    }
    emit resultTextChanged();
    setPhase(Phase::Done);
    emit finishedOk();
}

void ImportViewModel::onUploadFailed(const QString &message)
{
    setError(message);
    const bool auth = SettingsViewModel::isAuthFailureMessage(message);
    if (m_authFailure != auth) { m_authFailure = auth; emit authFailureChanged(); }
    setPhase(Phase::Failed);
}

void ImportViewModel::onImportError(const QString & /*title*/, const QString &message,
                                    ImportSeverity severity)
{
    if (severity == ImportSeverity::Warning)
        return;   // e.g. ZIP-open warning — upload proceeds; nothing to surface fatally
    setError(message);
    const bool auth = SettingsViewModel::isAuthFailureMessage(message);
    if (m_authFailure != auth) { m_authFailure = auth; emit authFailureChanged(); }
    setPhase(Phase::Failed);
}

void ImportViewModel::cancel()
{
    // Pre-upload only (spec §4.1): return to idle from file-select or duplicate
    // confirm. Does NOT abort an in-flight upload.
    if (m_phase == Phase::Uploading || m_phase == Phase::Processing) return;
    m_duplicates.clear();
    emit duplicateCountChanged();
    emit allDuplicatesChanged();
    setError(QString());
    setPhase(Phase::Idle);
}

bool ImportViewModel::downloadTemplate(const QUrl & /*fileUrl*/)
{
    return false;   // implemented in Task 11
}
```

> **Note on `isAuthFailureMessage` (VERIFIED — orchestrator resolution of OQ1):** `SettingsViewModel::isAuthFailureMessage(const QString&)` is a public static ([SettingsViewModel.h:115](qt-app/quick/viewmodels/SettingsViewModel.h)) returning true when the message contains `"Invalid admin key"` OR `"authentication required"` (case-insensitive, [SettingsViewModel.cpp:153-157](qt-app/quick/viewmodels/SettingsViewModel.cpp)). The ImportController 401 messages (Tasks 3 & 4) are deliberately worded `"Admin authentication required — re-enter via admin login."`, which contains `"authentication required"`, so this predicate classifies them true and `m_authFailure` fires. Do NOT change the predicate; keep the controller wording containing "authentication required" (this mirrors `DatabaseViewModel.cpp:218`).

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importviewmodel --output-on-failure` — expected: `100% tests passed` (4 slots).

- [ ] **Commit.** Stage `qt-app/quick/viewmodels/ImportViewModel.h`, `qt-app/quick/viewmodels/ImportViewModel.cpp`, `qt-app/quick/CMakeLists.txt`, `qt-app/quick/tests/tst_importviewmodel.cpp`. Message: `feat(import): ImportViewModel state machine (4a.3 T10)`.

---

## Task 11 — Download Template helper

Implement the template CSV: a pure static that produces the sample content (recognized headers + one example row using an opaque hyphenated school ID like `21-1-0001`), and wire `ImportViewModel::downloadTemplate` to write it.

**Files:**
- Modify `qt-app/core/importcontroller.h` (add the pure static).
- Modify `qt-app/core/importcontroller.cpp` (implement it).
- Modify `qt-app/tests/tst_importcontroller.cpp` (test the pure content).
- Modify `qt-app/quick/viewmodels/ImportViewModel.cpp` (fill in `downloadTemplate`).

**Interfaces:**
- Produces: `static QByteArray ImportController::importTemplateCsv();` — a UTF-8 CSV: one header line with the recognized columns, then one example data row.
- `ImportViewModel::downloadTemplate(const QUrl&)` writes `importTemplateCsv()` to `fileUrl.toLocalFile()` via `QSaveFile`, returns `true` on success.

**Steps:**

- [ ] **Write the failing test.** Add slot declaration + body to `tst_importcontroller.cpp`:

```cpp
    // importTemplateCsv
    void importTemplateCsvHasHeadersAndExampleRow();
```
```cpp
void TestImportController::importTemplateCsvHasHeadersAndExampleRow()
{
    const QString csv = QString::fromUtf8(ImportController::importTemplateCsv());
    const QStringList lines = csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVERIFY(lines.size() >= 2);   // header + at least one example row

    const QString header = lines.first();
    QVERIFY(header.contains("School ID"));
    QVERIFY(header.contains("Name"));
    QVERIFY(header.contains("Course"));
    QVERIFY(header.contains("Department"));
    QVERIFY(header.contains("Year Level"));
    QVERIFY(header.contains("Gender"));
    QVERIFY(header.contains("Status"));

    // The example row uses an opaque hyphenated School ID (reinforces that it is
    // a School ID, not an admin key) and must NOT leak real PII.
    QVERIFY(lines.at(1).contains("21-1-0001"));
}
```

- [ ] **Run — expect FAIL.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R tst_importcontroller --output-on-failure` — expected: compile error (`importTemplateCsv` undeclared).

- [ ] **Implement — header.** In `qt-app/core/importcontroller.h`, after `validateForImport`:

```cpp
    // Pure sample-CSV generator for the "Download Template" button (spec §7).
    // Header line of the recognized columns + one synthetic example row (opaque
    // hyphenated school_id like 21-1-0001; no real PII). UTF-8, no BOM.
    static QByteArray importTemplateCsv();
```

- [ ] **Implement — cpp.** Add to `importcontroller.cpp`:

```cpp
QByteArray ImportController::importTemplateCsv()
{
    // Recognized columns, in a natural order. Extra columns are ignored on
    // import and column order does not matter, but this is the friendly shape.
    const QString header = QStringLiteral(
        "School ID,Name,Course,Department,Year Level,Gender,Status\n");
    const QString example = QStringLiteral(
        "21-1-0001,Juan Dela Cruz,BSIT,CCS,1,Male,Active\n");
    return (header + example).toUtf8();
}
```

- [ ] **Implement — VM wiring.** In `qt-app/quick/viewmodels/ImportViewModel.cpp`, add `#include <QSaveFile>` and replace the `downloadTemplate` stub:

```cpp
bool ImportViewModel::downloadTemplate(const QUrl &fileUrl)
{
    const QByteArray bytes = ImportController::importTemplateCsv();
    QSaveFile file(fileUrl.toLocalFile());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(bytes) != bytes.size() || !file.commit())
        return false;
    return true;
}
```

- [ ] **Run — expect PASS.** `cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 -R "tst_importcontroller|tst_importviewmodel" --output-on-failure` — expected: `100% tests passed`.

- [ ] **Commit.** Stage `qt-app/core/importcontroller.h`, `qt-app/core/importcontroller.cpp`, `qt-app/tests/tst_importcontroller.cpp`, `qt-app/quick/viewmodels/ImportViewModel.cpp`. Message: `feat(import): download-template CSV helper (4a.3 T11)`.

---

## Task 12 — `ImportStudentsDialog.qml`

Create the modal dialog parallel to `RegisterStudentDialog.qml`: FileDialog pickers (data required, photos ZIP optional), a format hint listing recognized columns, a Download Template button, an Import button (enabled once a data file is chosen), inline duplicate skip-confirm (Continue/Cancel; all-dupes → Close only), byte-progress + "Processing…", and a result line — all server strings `Text.PlainText`, all objectNames `import`-scoped. QuickTest fixtures inject a plain-QML stub `vm`.

**Files:**
- Create `qt-app/quick/qml/admin/ImportStudentsDialog.qml`.
- Modify `qt-app/quick/tests/tst_qml_admin.qml` (add a new fixture band + `TestCase`; raise the host `height`).
- (CMake registration of the new `.qml` happens in Task 13.)

**Interfaces:**
- Consumes VM surface (Task 10 property/invokable names): `phase` (enum ints), `busy`, `parsedCount`, `duplicateCount`, `allDuplicates`, `uploadPercent`, `dataFileName`, `photosZipName`, `errorText`, `resultText`; `setDataFile(url)`, `setPhotosZip(url)`, `clearPhotosZip()`, `startImport()`, `continueAfterDuplicates()`, `cancel()`, `downloadTemplate(url)`; signal `finishedOk()`.
- The dialog takes `property var vm` (a real `ImportViewModel` in the app, a plain-QML stub in QuickTests) — the house `property var vm` convention.

> **Phase enum in QML:** since `Phase` is `Q_ENUM`, QML reads it as `ImportViewModel.Idle`, `ImportViewModel.CheckingDuplicates`, etc. **But** the QuickTest stub is a plain `QtObject` with a plain `int phase` — so the dialog must compare against **plain integers** matching the enum order (`0=Idle, 1=CheckingDuplicates, 2=AwaitingDuplicates, 3=Uploading, 4=Processing, 5=Done, 6=Failed`). Define readonly int aliases at the top of the dialog (e.g. `readonly property int phaseAwaitingDuplicates: 2`) and compare `vm.phase === root.phaseAwaitingDuplicates`, so the same code works against both the C++ enum (which coerces to int) and the stub.

**Steps:**

- [ ] **Create the dialog.** Write `qt-app/quick/qml/admin/ImportStudentsDialog.qml`:

```qml
import QtQuick
import QtQuick.Controls          // ProgressBar
import QtQuick.Layouts
import QtQuick.Dialogs
import LOAMS

// Bulk student import (Phase 4a.3). LDialog-based modal driven by plain
// `visible`. Takes `property var vm` (an ImportViewModel, or a plain-QML stub
// in QuickTests). Every server-provided string renders Text.PlainText.
LDialog {
    id: root
    property var vm
    title: qsTr("Import students")
    maxWidth: 560

    // Phase constants (mirror ImportViewModel::Phase order) so the same
    // comparisons work against the C++ Q_ENUM AND the plain-int QuickTest stub.
    readonly property int phaseIdle: 0
    readonly property int phaseCheckingDuplicates: 1
    readonly property int phaseAwaitingDuplicates: 2
    readonly property int phaseUploading: 3
    readonly property int phaseProcessing: 4
    readonly property int phaseDone: 5
    readonly property int phaseFailed: 6

    readonly property int vmPhase: root.vm ? root.vm.phase : root.phaseIdle
    readonly property bool hasDataFile: root.vm ? root.vm.dataFileName.length > 0 : false

    Connections {
        target: root.vm ? root.vm : null
        function onFinishedOk() {
            // Result is shown briefly; leave the dialog open on the Done phase so
            // the operator sees the counts, then they close it.
        }
    }

    Keys.onEscapePressed: if (root.vm && !root.vm.busy) root.visible = false;

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        // Format hint — recognized columns.
        Text {
            objectName: "importFormatHint"
            Layout.fillWidth: true
            text: qsTr("Recognized columns: School ID*, Name*, Course, Department, Year Level, Gender, Status. Extra columns are ignored; only School ID and Name are required.")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        // Row 1: data file (required).
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "importChooseDataButton"
                variant: "Outline"
                compact: true
                text: root.hasDataFile ? qsTr("Change data file…") : qsTr("Choose data file…")
                enabled: root.vm ? !root.vm.busy : true
                onClicked: dataDialog.open()
            }
            Text {
                objectName: "importDataLabel"
                Layout.fillWidth: true
                text: root.hasDataFile ? root.vm.dataFileName : qsTr("Excel (.xlsx) or CSV (.csv)")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: root.hasDataFile ? Theme.text : Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }

        // Row 2: photos ZIP (optional).
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "importChooseZipButton"
                variant: "Outline"
                compact: true
                text: (root.vm && root.vm.photosZipName.length > 0)
                      ? qsTr("Change photos ZIP…") : qsTr("Choose photos ZIP…")
                enabled: root.vm ? !root.vm.busy : true
                onClicked: zipDialog.open()
            }
            Text {
                objectName: "importZipLabel"
                Layout.fillWidth: true
                text: (root.vm && root.vm.photosZipName.length > 0)
                      ? root.vm.photosZipName : qsTr("Optional · ZIP of photos named by School ID")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: (root.vm && root.vm.photosZipName.length > 0) ? Theme.text : Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            LButton {
                objectName: "importRemoveZipButton"
                variant: "Ghost"
                compact: true
                text: qsTr("Remove")
                visible: root.vm ? root.vm.photosZipName.length > 0 : false
                onClicked: if (root.vm) root.vm.clearPhotosZip()
            }
        }

        // Inline error (validation / server / auth).
        Text {
            objectName: "importErrorText"
            visible: root.vm ? root.vm.errorText.length > 0 : false
            Layout.fillWidth: true
            text: root.vm ? root.vm.errorText : ""
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        // Duplicate skip-confirm — SOME duplicates (Continue / Cancel).
        ColumnLayout {
            objectName: "importDuplicateSome"
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            visible: root.vmPhase === root.phaseAwaitingDuplicates
                     && root.vm && !root.vm.allDuplicates
            Text {
                Layout.fillWidth: true
                text: root.vm
                      ? qsTr("%1 of %2 rows already exist and will be skipped.")
                          .arg(root.vm.duplicateCount).arg(root.vm.parsedCount)
                      : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.spacing.md
                LButton {
                    objectName: "importDupCancelButton"
                    variant: "Outline"
                    text: qsTr("Cancel")
                    onClicked: if (root.vm) root.vm.cancel()
                }
                LButton {
                    objectName: "importDupContinueButton"
                    text: qsTr("Continue")
                    onClicked: if (root.vm) root.vm.continueAfterDuplicates()
                }
            }
        }

        // Duplicate — ALL duplicates (Close only, nothing to import).
        ColumnLayout {
            objectName: "importDuplicateAll"
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            visible: root.vmPhase === root.phaseAwaitingDuplicates
                     && root.vm && root.vm.allDuplicates
            Text {
                Layout.fillWidth: true
                text: qsTr("Nothing to import — every row already exists.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            LButton {
                objectName: "importDupCloseButton"
                Layout.alignment: Qt.AlignRight
                variant: "Outline"
                text: qsTr("Close")
                onClicked: root.visible = false
            }
        }

        // Progress: byte bar while uploading, indeterminate "Processing…" after.
        ColumnLayout {
            objectName: "importProgress"
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            visible: root.vmPhase === root.phaseUploading || root.vmPhase === root.phaseProcessing
            ProgressBar {
                objectName: "importProgressBar"
                Layout.fillWidth: true
                indeterminate: root.vmPhase === root.phaseProcessing
                from: 0; to: 100
                value: root.vm ? root.vm.uploadPercent : 0
            }
            Text {
                objectName: "importProcessingLabel"
                visible: root.vmPhase === root.phaseProcessing
                text: qsTr("Processing…")
                textFormat: Text.PlainText
                color: Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }

        // Result line — real counts.
        Text {
            objectName: "importResultText"
            visible: root.vmPhase === root.phaseDone
            Layout.fillWidth: true
            text: root.vm ? root.vm.resultText : ""
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        // Footer: Download Template · Cancel/Close · Import.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "importTemplateButton"
                variant: "Ghost"
                compact: true
                text: qsTr("Download template")
                onClicked: templateDialog.open()
            }
            Item { Layout.fillWidth: true }
            LButton {
                objectName: "importCloseButton"
                variant: "Outline"
                text: (root.vmPhase === root.phaseDone) ? qsTr("Close") : qsTr("Cancel")
                enabled: root.vm ? !root.vm.busy : true
                onClicked: root.visible = false
            }
            LButton {
                objectName: "importSubmitButton"
                text: qsTr("Import")
                // Enabled once a data file is chosen and we are idle (not busy/awaiting).
                enabled: root.hasDataFile && root.vm
                         && !root.vm.busy && root.vmPhase === root.phaseIdle
                onClicked: if (root.vm) root.vm.startImport()
            }
        }
    }

    FileDialog {
        id: dataDialog
        objectName: "importDataDialog"
        nameFilters: ["Student data (*.xlsx *.csv)"]
        onAccepted: if (root.vm) root.vm.setDataFile(selectedFile)
    }
    FileDialog {
        id: zipDialog
        objectName: "importZipDialog"
        nameFilters: ["Photos ZIP (*.zip)"]
        onAccepted: if (root.vm) root.vm.setPhotosZip(selectedFile)
    }
    FileDialog {
        id: templateDialog
        objectName: "importTemplateDialog"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: ["CSV (*.csv)"]
        onAccepted: if (root.vm) root.vm.downloadTemplate(selectedFile)
    }
}
```

> Verify the `Theme` tokens referenced (`Theme.spacing.sm`, `Theme.mutedTextCaption`, `Theme.error`, `Theme.text`, `Theme.card`, `Theme.typography.control`, `Theme.typography.sans`) exist in `qml/theme/Theme.qml` before building; if `Theme.spacing.sm` is absent, use an existing token (e.g. `Theme.spacing.xs`). Do **not** introduce raw hex. `ProgressBar` comes from `QtQuick.Controls` — if `import QtQuick.Controls` is needed for it to resolve, add that import (mirror whatever the existing screens use for `BusyIndicator`/`ProgressBar`).

- [ ] **Add the QuickTest fixture.** In `qt-app/quick/tests/tst_qml_admin.qml`:
  1. Raise the host height: change `width: 1100; height: 6600` to `width: 1100; height: 7300` and extend the geometry-ledger comment with `| importDialog 6600..7300`.
  2. Add a new fixture band + `TestCase` before the final `Component { id: signalSpy; SignalSpy {} }` line:

```qml
    // --- ImportStudentsDialog fixture (own band below registerDialog, y 6600..7300) ---
    Item {
        id: importBand
        y: 6600
        width: 900; height: 700

        QtObject {
            id: importStub
            property int phase: 0            // 0=Idle .. 6=Failed (mirrors Phase enum)
            property bool busy: false
            property int parsedCount: 0
            property int duplicateCount: 0
            property bool allDuplicates: false
            property int uploadPercent: 0
            property string dataFileName: ""
            property string photosZipName: ""
            property string errorText: ""
            property string resultText: ""
            property bool authFailure: false

            property int startImportCount: 0
            property int continueCount: 0
            property int cancelCount: 0
            property url lastDataUrl: ""
            property url lastZipUrl: ""
            property url lastTemplateUrl: ""
            signal finishedOk()

            function setDataFile(u) { lastDataUrl = u; dataFileName = ("" + u).split("/").pop(); parsedCount = 2; phase = 0; }
            function setPhotosZip(u) { lastZipUrl = u; photosZipName = ("" + u).split("/").pop(); }
            function clearPhotosZip() { photosZipName = ""; }
            function startImport() { startImportCount++; }
            function continueAfterDuplicates() { continueCount++; }
            function cancel() { cancelCount++; phase = 0; duplicateCount = 0; allDuplicates = false; }
            function downloadTemplate(u) { lastTemplateUrl = u; return true; }
        }

        ImportStudentsDialog { id: importDialog2; anchors.fill: parent; vm: importStub }

        TestCase {
            name: "ImportStudentsDialog"; when: windowShown
            function init() {
                importStub.phase = 0;
                importStub.busy = false;
                importStub.parsedCount = 0;
                importStub.duplicateCount = 0;
                importStub.allDuplicates = false;
                importStub.dataFileName = "";
                importStub.photosZipName = "";
                importStub.errorText = "";
                importStub.resultText = "";
                importStub.startImportCount = 0;
                importStub.continueCount = 0;
                importStub.cancelCount = 0;
                importDialog2.visible = false;
            }

            function test_importDisabledUntilDataFileChosen() {
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                var submit = findChild(importDialog2, "importSubmitButton");
                verify(submit !== null);
                compare(submit.enabled, false);
                importStub.setDataFile("file:///tmp/students.csv");
                compare(submit.enabled, true);
            }

            function test_importInvokesStartImport() {
                importStub.setDataFile("file:///tmp/students.csv");
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                mouseClick(findChild(importDialog2, "importSubmitButton"));
                compare(importStub.startImportCount, 1);
            }

            function test_someDuplicatesShowsContinueAndCancel() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.parsedCount = 3;
                importStub.duplicateCount = 1;
                importStub.allDuplicates = false;
                importStub.phase = 2;   // AwaitingDuplicates
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                verify(findChild(importDialog2, "importDuplicateSome").visible);
                verify(!findChild(importDialog2, "importDuplicateAll").visible);
                mouseClick(findChild(importDialog2, "importDupContinueButton"));
                compare(importStub.continueCount, 1);
            }

            function test_allDuplicatesShowsCloseOnly() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.parsedCount = 2;
                importStub.duplicateCount = 2;
                importStub.allDuplicates = true;
                importStub.phase = 2;   // AwaitingDuplicates
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                verify(findChild(importDialog2, "importDuplicateAll").visible);
                verify(!findChild(importDialog2, "importDuplicateSome").visible);
                // No Continue button in the all-dupes branch; Close closes the dialog.
                mouseClick(findChild(importDialog2, "importDupCloseButton"));
                compare(importDialog2.visible, false);
            }

            function test_processingShowsIndeterminateAndLabel() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.phase = 4;   // Processing
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                var bar = findChild(importDialog2, "importProgressBar");
                verify(bar !== null);
                compare(bar.indeterminate, true);
                compare(findChild(importDialog2, "importProcessingLabel").visible, true);
            }

            function test_resultLineShownOnDone() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.resultText = "5 imported · 1 skipped · 0 failed";
                importStub.phase = 5;   // Done
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                var res = findChild(importDialog2, "importResultText");
                verify(res.visible);
                compare(res.text, "5 imported · 1 skipped · 0 failed");
            }

            function test_templateButtonInvokesDownload() {
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                // Drive the VM method directly (FileDialog is native/modal and not
                // clickable under offscreen); assert the wiring point exists.
                importStub.downloadTemplate("file:///tmp/template.csv");
                compare(("" + importStub.lastTemplateUrl).length > 0, true);
                verify(findChild(importDialog2, "importTemplateButton") !== null);
            }
        }
    }
```

> **QuickTest conventions honored:** own y-band (6600..7300) with the host root height raised to match; a plain-QML stub `vm` (house rule §5 — no C++ VM, no live network); `objectName`s `import`-scoped so they never collide with the register/cascade/bulk fixtures. No double-click is needed here; if one were, it would use `MouseArea.onDoubleClicked` (never `TapHandler`, per QTBUG-102441). Editing this `.qml` data file needs no rebuild of the module — but the `ImportStudentsDialog.qml` under `qml/admin/` DOES (it is compiled into `witsquickmodule` once Task 13 registers it).

- [ ] **Build + run — expect FAIL then PASS across Task 13.** The dialog fixture cannot compile/resolve `ImportStudentsDialog` until Task 13 registers the production `.qml` in `witsquickmodule`. So: complete the CMake registration in Task 13, then run `ctest -R tst_qml_admin`. (If you prefer to see red first, run `ctest -R tst_qml_admin` now and expect a QML "ImportStudentsDialog is not a type" error.)

- [ ] **Commit.** Stage `qt-app/quick/qml/admin/ImportStudentsDialog.qml`, `qt-app/quick/tests/tst_qml_admin.qml`. Message: `feat(import): ImportStudentsDialog + QuickTest fixtures (4a.3 T12)`.

---

## Task 13 — `DatabaseScreen.qml` ＋Import button + wiring + CMake registration + full green

Add the Import toolbar button next to ＋Add Student, instantiate `ImportStudentsDialog` + an `ImportViewModel` inside `DatabaseScreen`, wire open/close, register the new production `.qml` in `witsquickmodule`, and land the full suite green.

**Files:**
- Modify `qt-app/quick/qml/admin/DatabaseScreen.qml` (button + dialog instance + VM instance + wiring).
- Modify `qt-app/quick/CMakeLists.txt` (add `qml/admin/ImportStudentsDialog.qml` to the `QML_FILES` list).
- Modify `qt-app/quick/tests/tst_qml_admin.qml` (add one DatabaseScreen test that the Import button opens the dialog; extend the DatabaseScreen stub with the import surface).

**Interfaces:**
- Consumes: `ImportViewModel` (Task 10, `import LOAMS`), `ImportStudentsDialog` (Task 12).

**Steps:**

- [ ] **Register the production QML.** In `qt-app/quick/CMakeLists.txt`, add to the `QML_FILES` list (after `qml/admin/RegisterStudentDialog.qml`):

```cmake
        qml/admin/ImportStudentsDialog.qml
```

- [ ] **Add the Import button.** In `qt-app/quick/qml/admin/DatabaseScreen.qml`, in the count/selection header `RowLayout`, add an Import button immediately after the `addStudentButton` `LButton` (before the `exportMetrics` `TextMetrics`):

```qml
            LButton {
                objectName: "importStudentsButton"
                variant: "Outline"
                compact: true
                text: qsTr("Import")
                enabled: screen.vm ? !screen.vm.loading : false
                onClicked: { importDialog.vm = importVm; importDialog.visible = true; }
            }
```

- [ ] **Instantiate the VM + dialog.** In `DatabaseScreen.qml`, after the `RegisterStudentDialog { ... }` block, add:

```qml
    // Import owns its OWN ViewModel (separate from the DatabaseViewModel `vm`):
    // it wraps a distinct ImportController + QNetworkAccessManager. On a
    // successful import, refresh the student table so new rows appear.
    ImportViewModel { id: importVm }

    ImportStudentsDialog {
        id: importDialog
        objectName: "importDialog"
        vm: importVm
    }

    Connections {
        target: importVm
        function onFinishedOk() {
            if (screen.vm) screen.vm.reloadTable();
        }
    }
```

> **Test-injection note:** in the QuickTest DatabaseScreen fixture the screen `vm` is a stub, but `importVm` is a real `ImportViewModel`. To keep the DatabaseScreen fixture network-free and simple, the button-opens-dialog test only asserts the dialog becomes visible (it does not drive an import through the real `ImportViewModel`). If instantiating a real `ImportViewModel` inside the fixture proves noisy, override `importDialog.vm` to a stub in the test's `init()` — but the default real VM constructs cleanly (owns its own idle NAM, fires nothing until `startImport`).

- [ ] **Add the DatabaseScreen open test.** In `qt-app/quick/tests/tst_qml_admin.qml`, extend the DatabaseScreen `stubVm` if needed (it already exposes `loading`) and add a test in the DatabaseScreen `TestCase`:

```qml
            function test_importButtonOpensDialog() {
                var btn = findChild(databaseScreen, "importStudentsButton");
                verify(btn !== null);
                var dlg = findChild(databaseScreen, "importDialog");
                verify(dlg !== null);
                compare(dlg.visible, false);
                mouseClick(btn);
                compare(dlg.visible, true);
                dlg.visible = false;   // reset for later tests
            }
```

Also add `var idlg = findChild(databaseScreen, "importDialog"); if (idlg) idlg.visible = false;` to the DatabaseScreen `init()` so a leaked-open import dialog scrim cannot swallow later clicks.

- [ ] **Reconfigure + build + full suite — expect ALL GREEN.**
  `cmake -S qt-app -B C:/b/loams-4a3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4a3 && ctest --test-dir C:/b/loams-4a3 --output-on-failure`
  — expected: **40/40 tests passed** (the 39 baseline targets + the new `tst_importviewmodel`; `tst_importcontroller` and `tst_qml_admin` grew slots but remain single targets). No new build warnings.

- [ ] **Manual smoke (GUI app — build is necessary but not sufficient).** Run `WITSQuick.exe`, log in as admin, open Database, click **Import**: the dialog opens; choosing a CSV enables Import; the format hint + Download Template work; a run against the deployed Task 6/7 endpoints shows real counts. (This exercises the real `ImportViewModel` end-to-end.)

- [ ] **Commit.** Stage `qt-app/quick/qml/admin/DatabaseScreen.qml`, `qt-app/quick/CMakeLists.txt`, `qt-app/quick/tests/tst_qml_admin.qml`. Message: `feat(import): wire Import button + dialog into DatabaseScreen (4a.3 T13)`.

---

## Open questions — RESOLVED (orchestrator, pre-execution)

All three were resolved during the plan self-review; no open blockers remain.

1. **`SettingsViewModel::isAuthFailureMessage` coverage — RESOLVED.** Verified against source: the predicate matches `"Invalid admin key"` OR `"authentication required"` (case-insensitive, [SettingsViewModel.cpp:153-157](qt-app/quick/viewmodels/SettingsViewModel.cpp)). The ImportController 401 messages were changed to `"Admin authentication required — re-enter via admin login."` (contains `"authentication required"`), so the predicate classifies them and `m_authFailure` fires. No predicate change; stays consistent with `DatabaseViewModel.cpp:218`. The Task-10 401 test's `authFailure` assertion therefore passes.
2. **`ProgressBar` import — RESOLVED (verified available).** `QtQuick.Controls` is already used in the project's QML (confirmed via grep), so `ProgressBar` resolves. `ImportStudentsDialog.qml` imports `QtQuick.Controls`. Keep the bar's colors on `Theme.*` tokens (zero raw hex); if the default control chrome looks off-brand during the GUI walkthrough, swap to a two-`Rectangle` themed bar (track + fill) — a cosmetic swap, no logic change.
3. **`finishedOk()` in-dialog summary vs. auto-close+toast — RESOLVED: keep in-dialog summary.** On `Phase::Done` the dialog stays open showing "X imported · Y skipped · Z failed" (with a Close button), and `DatabaseScreen`'s `Connections` refreshes the table via `reloadTable()`. Rationale: the result carries a **failed** count the admin should see persistently, which a 2.5s toast would hide — richer than register's single-line toast. Owner may flip this to auto-close+toast (move `resultText` to `databaseToast`, close on `finishedOk()`) if preferred; flagged in the handoff.
```
