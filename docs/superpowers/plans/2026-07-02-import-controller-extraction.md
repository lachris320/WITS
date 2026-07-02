# ImportController Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract all Database Import tab logic (CSV/Excel preview parsing, header-alias mapping, duplicate-check network call, multipart upload, response parsing) from `adminWindow` into an `ImportController : QObject` plus plain value types, leaving `adminWindow` a pure View for that tab — behavior-identical, with unit tests for every pure piece. This is the **third** controller extraction, following `SettingsController` (PR #8) and `VisitorController` (PR #10).

**Architecture:** Plain value types (`ImportSeverity`, `ExcelParseError`, `ParsedTable`, `UploadResult` in `importdata.h`) carry import data between the layers. `ImportController` borrows the shared `QNetworkAccessManager`, POSTs to `check_duplicates.php` and `upload_students_zip.php`, parses both file formats and both responses with pure statics, and reports back via `duplicatesResolved`/`importError`/`uploadStarted`/`uploadProgress`/`uploadFinished`/`uploadFailed` signals. `adminWindow` keeps only widget work: it dispatches file parsing by extension, populates `ui->bulkTable` from the returned `ParsedTable` using each source's exact legacy sequence, owns the entire duplicate-resolution dialog, drives the progress-bar/button state machine off the signals, and shows every dialog.

**Tech Stack:** Qt 6.11 Widgets, Qt Network, Qt Test, QXlsx (vendored), CMake+Ninja.

## Global Constraints

- Qt 6.11, C++17, CMake+Ninja+MinGW. `#ifndef` header guards (NOT `#pragma once`). `m_` prefix for new members. Functor-based `connect` (no `SIGNAL`/`SLOT` macros). `QNetworkAccessManager` injected, not owned. `reply->deleteLater()` on every path; `finished` connected with the controller as the context object. QXlsx (vendored, do NOT modify) links `Qt::GuiPrivate` → the test target sets `QT_QPA_PLATFORM=offscreen`.
- Security-hygiene (binding): no secrets/admin keys/credentials/backend URLs in source, tests, CMake, or this plan; use placeholders. No real student PII in fixtures — synthetic only. No hardcoded `C:\Users\<name>` personal paths anywhere in committed files. NEVER stage/commit `qt-app/adminwindow.ui` (carries the user's own uncommitted edit) or `qt-app/build/`.
- Commits only via the `commit` skill (project rule) — never raw `git add` / `git commit`. Conventional Commits, files staged by name, no Claude/Anthropic co-author trailer.
- Build + test commands below; run them exactly (Qt tools are not on PATH on this machine).

## Build & Test Commands (Windows, tools not on PATH)

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"   # configure (once)
cmake --build qt-app/build                                                              # build
ctest --test-dir qt-app/build --output-on-failure                                       # all tests
ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure               # just the new target
```
(Ignore "LF will be replaced by CRLF" warnings and the QXlsx GuiPrivate CMake warning.)

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `qt-app/importdata.h` | **Create** (Task 1) | `ImportSeverity`, `ExcelParseError`, `ParsedTable`, `UploadResult` value types |
| `qt-app/importcontroller.h` | **Create** (Task 1, final in Task 1) | `ImportController` declaration |
| `qt-app/importcontroller.cpp` | **Create** (Task 1), complete (Task 2) | Statics in Task 1; `checkDuplicates`/`uploadStudents`/`parseExcel` in Task 2 |
| `qt-app/tests/tst_importcontroller.cpp` | **Create** (Task 1), extend (Task 2) | Qt Test coverage for the controller |
| `qt-app/CMakeLists.txt` | **Modify** (Task 1) | Add the three new files to the `WITS` target |
| `qt-app/tests/CMakeLists.txt` | **Modify** (Task 1) | Register the `tst_importcontroller` target |
| `qt-app/adminwindow.h` | **Modify** (Task 3) | New member/slots; remove `loadCSVtoTable`/`loadExcelToTable`'s network+parse bodies (methods stay, bodies rewritten), delete free `normalizeHeader` |
| `qt-app/adminwindow.cpp` | **Modify** (Task 3) | Wire controller; rewrite the two click slots and the two loaders; delete duplicated header-mapping chains |

---

## Task 1: Value types + pure static helpers + test target

`importdata.h`, the **final** `importcontroller.h`, an `importcontroller.cpp` whose five public statics (`normalizeHeader`, `mapHeaders`, `parseCsv`, `parseDuplicateResponse`, `parseUploadResponse`) are implemented test-first, and the `tst_importcontroller` CMake target. `checkDuplicates`, `uploadStudents`, and `parseExcel` are declared in the header now (the header does not change again after this task) but get only minimal stub bodies here — their real implementations land in Task 2. The stubs exist so the `.cpp` links in Task 1 and the red phase fails on assertions, not on link errors.

**Files:**
- Create: `qt-app/importdata.h`
- Create: `qt-app/importcontroller.h`
- Create: `qt-app/importcontroller.cpp`
- Create: `qt-app/tests/tst_importcontroller.cpp`
- Modify: `qt-app/CMakeLists.txt`
- Modify: `qt-app/tests/CMakeLists.txt`
- Test: `ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure`

**Interfaces:**
- Consumes: nothing new — `ApiConfig` and QXlsx are referenced only from Task 2 onward.
- Produces (used by Tasks 2 and 3):
  - `enum class ImportSeverity { Warning, Critical };`
  - `enum class ExcelParseError { None, OpenFailed, NoSheets, EmptySheet };`
  - `struct ParsedTable { QStringList headers; QList<QStringList> rows; QMap<QString, int> headerIndex; };`
  - `struct UploadResult { bool ok; QString message; int successCount; int errorCount; bool plainText; QString rawText; };`
  - `explicit ImportController(QNetworkAccessManager *nam, QObject *parent = nullptr);`
  - `static QString normalizeHeader(const QString &raw);`
  - `static void mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut);`
  - `static ParsedTable parseCsv(const QString &rawText);`
  - `ParsedTable parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr);` *(stub until Task 2)*
  - `void checkDuplicates(const QStringList &schoolIds);` *(stub until Task 2)*
  - `void uploadStudents(const QString &excelPath, const QString &zipPath, const QStringList &skipIds);` *(stub until Task 2)*
  - `static bool parseDuplicateResponse(const QByteArray &raw, QStringList *duplicatesOut, QString *errorOut);`
  - `static UploadResult parseUploadResponse(const QByteArray &raw);`
  - Signals: `duplicatesResolved(const QStringList &duplicates)`, `importError(const QString &title, const QString &message, ImportSeverity severity)`, `uploadStarted()`, `uploadProgress(int percent)`, `uploadFinished(const UploadResult &result)`, `uploadFailed(const QString &message)`
  - CMake: `tst_importcontroller` test target registered with ctest

- [ ] **Step 1: Create `qt-app/importdata.h`**

```cpp
#ifndef IMPORTDATA_H
#define IMPORTDATA_H
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

enum class ImportSeverity { Warning, Critical };

// Distinguishes the three legacy Excel-load failure dialogs so the View can
// show the exact original message + severity. None = a usable table was read.
enum class ExcelParseError { None, OpenFailed, NoSheets, EmptySheet };

struct ParsedTable
{
    QStringList headers;
    QList<QStringList> rows;
    QMap<QString, int> headerIndex;
};

struct UploadResult
{
    bool    ok = false;
    QString message;
    int     successCount = 0;
    int     errorCount = 0;
    bool    plainText = false;   // true when the response was not a JSON object
    QString rawText;             // populated only when plainText is true
};

#endif // IMPORTDATA_H
```

A default-constructed `ParsedTable{}` (empty `headers`/`rows`/`headerIndex`) is the "no usable data" result shared by `parseCsv` (empty input) and `parseExcel` (any of the three failure cases).

- [ ] **Step 2: Create `qt-app/importcontroller.h` (final — never edited again after this task)**

```cpp
#ifndef IMPORTCONTROLLER_H
#define IMPORTCONTROLLER_H
#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include "importdata.h"

class QNetworkAccessManager;

class ImportController : public QObject
{
    Q_OBJECT
public:
    explicit ImportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeHeader(const QString &raw);
    static void    mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut);
    static ParsedTable parseCsv(const QString &rawText);

    // Synchronous QXlsx parse. Requires a QGuiApplication (see Testing).
    // errorOut (when non-null) reports which of the three legacy failure
    // cases occurred so the View can show the exact original dialog.
    ParsedTable parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr);

    // Async — result arrives via duplicatesResolved / importError.
    void checkDuplicates(const QStringList &schoolIds);

    // Async — result arrives via uploadStarted / uploadProgress / uploadFinished / uploadFailed.
    void uploadStudents(const QString &excelPath, const QString &zipPath,
                        const QStringList &skipIds);

    // Pure response parsers
    static bool parseDuplicateResponse(const QByteArray &raw,
                                       QStringList *duplicatesOut,
                                       QString *errorOut);
    static UploadResult parseUploadResponse(const QByteArray &raw);

signals:
    void duplicatesResolved(const QStringList &duplicates);   // empty = none found
    void importError(const QString &title, const QString &message, ImportSeverity severity);
    void uploadStarted();                        // excel file opened OK; request about to post
    void uploadProgress(int percent);
    void uploadFinished(const UploadResult &result);
    void uploadFailed(const QString &message);   // always critical, title "Upload Failed"

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // IMPORTCONTROLLER_H
```

- [ ] **Step 3: Create `qt-app/importcontroller.cpp` — red-phase skeleton**

Every body below except the constructor is a deliberate red-phase skeleton for the network/QXlsx methods. The five statics are implemented for real in **Step 7 of this task**; `checkDuplicates`, `uploadStudents`, and `parseExcel` are replaced in **Task 2**. They exist so the test target links and the new tests fail on assertions, not on link errors.

```cpp
#include "importcontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ImportController::ImportController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

QString ImportController::normalizeHeader(const QString &raw)
{
    Q_UNUSED(raw);
    return {};          // Red — implemented in Task 1, Step 7.
}

void ImportController::mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut)
{
    Q_UNUSED(headers);
    Q_UNUSED(indexOut); // Red — implemented in Task 1, Step 7.
}

ParsedTable ImportController::parseCsv(const QString &rawText)
{
    Q_UNUSED(rawText);
    return {};          // Red — implemented in Task 1, Step 7.
}

ParsedTable ImportController::parseExcel(const QString &filePath, ExcelParseError *errorOut)
{
    Q_UNUSED(filePath);
    if (errorOut)
        *errorOut = ExcelParseError::OpenFailed;
    return {};          // Stub — real QXlsx parse lands in Task 2, Step 4.
}

void ImportController::checkDuplicates(const QStringList &schoolIds)
{
    Q_UNUSED(schoolIds);   // Stub — real network call lands in Task 2, Step 2.
}

void ImportController::uploadStudents(const QString &excelPath, const QString &zipPath,
                                      const QStringList &skipIds)
{
    Q_UNUSED(excelPath);
    Q_UNUSED(zipPath);
    Q_UNUSED(skipIds);     // Stub — real network call lands in Task 2, Step 3.
}

bool ImportController::parseDuplicateResponse(const QByteArray &raw,
                                              QStringList *duplicatesOut,
                                              QString *errorOut)
{
    Q_UNUSED(raw);
    Q_UNUSED(duplicatesOut);
    Q_UNUSED(errorOut);
    return false;       // Red — implemented in Task 1, Step 7.
}

UploadResult ImportController::parseUploadResponse(const QByteArray &raw)
{
    Q_UNUSED(raw);
    return {};          // Red — implemented in Task 1, Step 7.
}
```

- [ ] **Step 4: Register the new sources in `qt-app/CMakeLists.txt`**

The `qt_add_executable(WITS ...)` block currently ends with the visitor files. Add the import files right after them:

```cmake
    qt_add_executable(WITS
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        guestwindow.h guestwindow.cpp guestwindow.ui
        reportpreviewdialog.h reportpreviewdialog.cpp
        busyindicator.h busyindicator.cpp
        attachFilesDialog.ui
        attachfilesdialog.h attachfilesdialog.cpp
        settingsdata.h
        settingscontroller.h settingscontroller.cpp
        visitordata.h
        visitorcontroller.h visitorcontroller.cpp
        importdata.h
        importcontroller.h importcontroller.cpp
    )
```

No new Qt modules — `Qt::Network` and `QXlsx` are already linked to `WITS`.

- [ ] **Step 5: Register the test target in `qt-app/tests/CMakeLists.txt`**

Append this block after the existing `tst_visitorcontroller` block (the seventh target, following the exact same pattern the six current targets use — compiling the unit under test directly from `${CMAKE_SOURCE_DIR}`):

```cmake
qt_add_executable(tst_importcontroller
    tst_importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/importcontroller.h
    ${CMAKE_SOURCE_DIR}/importdata.h
)
set_target_properties(tst_importcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_importcontroller PRIVATE ${CMAKE_SOURCE_DIR})
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

`QT_QPA_PLATFORM=offscreen` is required, not optional: QXlsx links `Qt::Gui` and propagates it, so `QTEST_MAIN` instantiates a `QGuiApplication`, which needs a platform plugin on a headless runner (same reason `tst_visitorcontroller` sets it). Do **not** "optimize" this target down to Core-only — the `parseExcel` round-trip test in Task 2 needs the `QGuiApplication` that the Gui link provides. The `QXlsx` target is available because the parent `CMakeLists.txt` runs `add_subdirectory(libs/QXlsx)` before `add_subdirectory(tests)`.

- [ ] **Step 6: Write the failing tests — create `qt-app/tests/tst_importcontroller.cpp`**

All payloads are synthetic (no real student PII). `QTEST_MAIN` (not `QTEST_APPLESS_MAIN`) because the target links `Qt::Gui` via QXlsx, so `QTEST_MAIN` expands to a `QGuiApplication` — required for Task 2's `parseExcel` test; harmless for the statics.

```cpp
#include <QtTest>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include "importcontroller.h"
#include "importdata.h"

class TestImportController : public QObject
{
    Q_OBJECT
private slots:
    // normalizeHeader
    void normalizeHeaderTrimsLowersStrips();
    void normalizeHeaderIdempotent();

    // mapHeaders
    void mapHeadersSchoolIdFamily();
    void mapHeadersNameFamily();
    void mapHeadersCodeFamily();
    void mapHeadersCourse();
    void mapHeadersYearLevel();
    void mapHeadersDepartmentFamily();
    void mapHeadersGender();
    void mapHeadersStatus();
    void mapHeadersVisits();
    void mapHeadersUnrecognizedFallsBackToColN();
    void mapHeadersFullRealisticRow();

    // parseCsv
    void parseCsvHeaderAndRows();
    void parseCsvSkipsEmptyLine();
    void parseCsvRaggedRowKeptAsIs();
    void parseCsvEmptyTextReturnsEmptyTable();
    void parseCsvColNFallbackEndToEnd();

    // parseDuplicateResponse
    void parseDuplicateResponseSuccessWithDuplicates();
    void parseDuplicateResponseSuccessEmpty();
    void parseDuplicateResponseStatusNotSuccess();
    void parseDuplicateResponseNotAnObject();

    // parseUploadResponse
    void parseUploadResponseSuccess();
    void parseUploadResponseStatusNotSuccess();
    void parseUploadResponsePlainTextFallback();
};

void TestImportController::normalizeHeaderTrimsLowersStrips()
{
    QCOMPARE(ImportController::normalizeHeader(" School ID "), QString("schoolid"));
    QCOMPARE(ImportController::normalizeHeader("Full_Name"),   QString("fullname"));
    QCOMPARE(ImportController::normalizeHeader("Year-Level"),  QString("yearlevel"));
}

// NOTE: idempotence is trivially satisfied by the Step 3 stub ("" -> "") —
// this slot is expected to PASS in the red phase; it earns its keep only
// once the real implementation lands.
void TestImportController::normalizeHeaderIdempotent()
{
    const QString already = ImportController::normalizeHeader("Full_Name");
    QCOMPARE(ImportController::normalizeHeader(already), already);
}

void TestImportController::mapHeadersSchoolIdFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"School ID", "Student ID", "id"}, idx);
    QCOMPARE(idx.value("school_id"), 2);   // last match wins (same column key)
}

void TestImportController::mapHeadersNameFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Name", "Full Name"}, idx);
    QCOMPARE(idx.value("name"), 1);
}

void TestImportController::mapHeadersCodeFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Code", "Student Code"}, idx);
    QCOMPARE(idx.value("code"), 1);
}

// NOTE: the single-alias tests below deliberately place the target header at
// a NON-ZERO column (behind an unrecognized "Notes" header) and assert
// contains() first — QMap::value() returns 0 for a missing key, so asserting
// index 0 on an empty map would vacuously pass against the Step 3 no-op stub.
void TestImportController::mapHeadersCourse()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Course"}, idx);
    QVERIFY(idx.contains("course"));
    QCOMPARE(idx.value("course"), 1);
}

void TestImportController::mapHeadersYearLevel()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Year Level"}, idx);
    QVERIFY(idx.contains("year_level"));
    QCOMPARE(idx.value("year_level"), 1);
}

void TestImportController::mapHeadersDepartmentFamily()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Department", "Dept"}, idx);
    QCOMPARE(idx.value("department"), 1);
}

void TestImportController::mapHeadersGender()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Gender"}, idx);
    QVERIFY(idx.contains("gender"));
    QCOMPARE(idx.value("gender"), 1);
}

void TestImportController::mapHeadersStatus()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Status"}, idx);
    QVERIFY(idx.contains("status"));
    QCOMPARE(idx.value("status"), 1);
}

void TestImportController::mapHeadersVisits()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Notes", "Visits"}, idx);
    QVERIFY(idx.contains("visits"));
    QCOMPARE(idx.value("visits"), 1);
}

void TestImportController::mapHeadersUnrecognizedFallsBackToColN()
{
    QMap<QString, int> idx;
    ImportController::mapHeaders({"Course", "Notes"}, idx);
    QVERIFY(idx.contains("col_1"));   // unrecognized header at NON-ZERO column
    QCOMPARE(idx.value("col_1"), 1);
    QVERIFY(!idx.contains("notes"));
}

void TestImportController::mapHeadersFullRealisticRow()
{
    QMap<QString, int> idx;
    const QStringList headers = {"School ID", "Full Name", "Course", "Year Level",
                                 "Department", "Gender", "Status", "Visits", "Notes"};
    ImportController::mapHeaders(headers, idx);
    QCOMPARE(idx.value("school_id"),   0);
    QCOMPARE(idx.value("name"),        1);
    QCOMPARE(idx.value("course"),      2);
    QCOMPARE(idx.value("year_level"),  3);
    QCOMPARE(idx.value("department"),  4);
    QCOMPARE(idx.value("gender"),      5);
    QCOMPARE(idx.value("status"),      6);
    QCOMPARE(idx.value("visits"),      7);
    QCOMPARE(idx.value("col_8"),       8);
}

void TestImportController::parseCsvHeaderAndRows()
{
    const QString text =
        "School ID,Full Name,Course\n"
        "2023-00123,Juan Dela Cruz,BSIT\n"
        "2023-00456,Maria Clara,BSCS\n";

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.headers, QStringList({"School ID", "Full Name", "Course"}));
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz", "BSIT"}));
    QCOMPARE(table.rows[1], QStringList({"2023-00456", "Maria Clara", "BSCS"}));
    QCOMPARE(table.headerIndex.value("school_id"), 0);
    QCOMPARE(table.headerIndex.value("name"),      1);
    QCOMPARE(table.headerIndex.value("course"),    2);
}

void TestImportController::parseCsvSkipsEmptyLine()
{
    const QString text =
        "School ID,Full Name\n"
        "2023-00123,Juan Dela Cruz\n"
        "\n"
        "2023-00456,Maria Clara\n";

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz"}));
    QCOMPARE(table.rows[1], QStringList({"2023-00456", "Maria Clara"}));
}

void TestImportController::parseCsvRaggedRowKeptAsIs()
{
    const QString text =
        "School ID,Full Name,Course\n"
        "2023-00123,Juan Dela Cruz\n";   // missing the Course cell

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.rows.size(), 1);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz"}));   // natural length 2, not padded to 3
}

// NOTE: an empty stub ParsedTable{} already satisfies all three isEmpty()
// checks — this slot is expected to PASS in the red phase; it guards the
// empty-input contract once the real implementation lands.
void TestImportController::parseCsvEmptyTextReturnsEmptyTable()
{
    const ParsedTable table = ImportController::parseCsv(QString());
    QVERIFY(table.headers.isEmpty());
    QVERIFY(table.rows.isEmpty());
    QVERIFY(table.headerIndex.isEmpty());
}

void TestImportController::parseCsvColNFallbackEndToEnd()
{
    const QString text =
        "School ID,Notes\n"
        "2023-00123,Some note\n";

    const ParsedTable table = ImportController::parseCsv(text);
    QCOMPARE(table.headerIndex.value("school_id"), 0);
    QCOMPARE(table.headerIndex.value("col_1"), 1);
    QVERIFY(!table.headerIndex.contains("notes"));
}

void TestImportController::parseDuplicateResponseSuccessWithDuplicates()
{
    const QByteArray json = R"({
        "status": "success",
        "duplicates": ["2023-00123", "2023-00456"]
    })";

    QStringList duplicates;
    QString errorMsg;
    QVERIFY(ImportController::parseDuplicateResponse(json, &duplicates, &errorMsg));
    QCOMPARE(duplicates, QStringList({"2023-00123", "2023-00456"}));
}

void TestImportController::parseDuplicateResponseSuccessEmpty()
{
    const QByteArray json = R"({"status": "success", "duplicates": []})";

    QStringList duplicates;
    QString errorMsg;
    QVERIFY(ImportController::parseDuplicateResponse(json, &duplicates, &errorMsg));
    QVERIFY(duplicates.isEmpty());
}

void TestImportController::parseDuplicateResponseStatusNotSuccess()
{
    const QByteArray json = R"({"status": "error"})";

    QStringList duplicates;
    QString errorMsg;
    QVERIFY(!ImportController::parseDuplicateResponse(json, &duplicates, &errorMsg));
    QCOMPARE(errorMsg, QString("Duplicate check failed."));
}

void TestImportController::parseDuplicateResponseNotAnObject()
{
    QStringList duplicates;
    QString errorMsg;
    QVERIFY(!ImportController::parseDuplicateResponse(QByteArray("[1,2,3]"), &duplicates, &errorMsg));
    QCOMPARE(errorMsg, QString("Invalid duplicate check response."));

    errorMsg.clear();
    QVERIFY(!ImportController::parseDuplicateResponse(QByteArray("not json at all"), &duplicates, &errorMsg));
    QCOMPARE(errorMsg, QString("Invalid duplicate check response."));
}

void TestImportController::parseUploadResponseSuccess()
{
    const QByteArray json = R"({
        "status": "success",
        "message": "All good.",
        "success_count": 10,
        "error_count": 1
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(result.ok);
    QVERIFY(!result.plainText);
    QCOMPARE(result.message, QString("All good."));
    QCOMPARE(result.successCount, 10);
    QCOMPARE(result.errorCount, 1);
}

void TestImportController::parseUploadResponseStatusNotSuccess()
{
    const QByteArray json = R"({
        "status": "error",
        "message": "Some rows were invalid."
    })";

    const UploadResult result = ImportController::parseUploadResponse(json);
    QVERIFY(!result.ok);
    QVERIFY(!result.plainText);
    QCOMPARE(result.message, QString("Some rows were invalid."));
}

void TestImportController::parseUploadResponsePlainTextFallback()
{
    const QByteArray raw = "Upload finished (legacy plain-text handler).";

    const UploadResult result = ImportController::parseUploadResponse(raw);
    QVERIFY(result.plainText);
    QCOMPARE(result.rawText, QString::fromUtf8(raw));
}

QTEST_MAIN(TestImportController)
#include "tst_importcontroller.moc"
```

Build and run — this is the **red** step (the build regenerates CMake automatically on the `CMakeLists.txt` changes):

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure
```

Expected: the target compiles and links, and `tst_importcontroller` **FAILS** — **22 of the 24 slots fail on assertions** against the skeleton `return {};` / `return false;` / no-op bodies. Exactly **2 slots PASS in the red phase, by design** (both are annotated in the test file):
- `normalizeHeaderIdempotent` — idempotence is trivially satisfied by the stub (`"" -> ""`);
- `parseCsvEmptyTextReturnsEmptyTable` — the stub's empty `ParsedTable{}` already satisfies the three `isEmpty()` checks.

Do **not** "fix" those two — they are contract guards that become meaningful once the real implementations land in Step 7. Every other slot must fail on `QCOMPARE`/`QVERIFY`; if the run instead shows a compile or link error, fix that first. (The `mapHeaders` slots place their target headers at non-zero columns with `contains()` checks precisely so the no-op stub cannot vacuously pass them — `QMap::value()` returns `0` for a missing key.)

- [ ] **Step 7: Implement the statics (green) — replace the five red skeletons in `qt-app/importcontroller.cpp`**

Leave the constructor and the `parseExcel`/`checkDuplicates`/`uploadStudents` stubs untouched. Replace the bodies of `normalizeHeader`, `mapHeaders`, `parseCsv`, `parseDuplicateResponse`, and `parseUploadResponse` with:

```cpp
QString ImportController::normalizeHeader(const QString &raw)
{
    // Direct port of the free function at adminwindow.cpp:1376-1382.
    QString s = raw.trimmed().toLower();
    s.remove(' ');
    s.remove('_');
    s.remove('-');
    return s;
}

void ImportController::mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut)
{
    // Unifies the two duplicated if/else chains at adminwindow.cpp:1424-1443
    // and 1500-1519 into one function. First match wins, else-chain order.
    for (int tableCol = 0; tableCol < headers.size(); ++tableCol) {
        const QString n = normalizeHeader(headers.at(tableCol));

        if (n.contains("schoolid") || n.contains("studentid") || (n == "id"))
            indexOut["school_id"] = tableCol;
        else if (n.contains("name") || n.contains("fullname") || n.contains("full"))
            indexOut["name"] = tableCol;
        else if (n.contains("code") || n.contains("studentcode"))
            indexOut["code"] = tableCol;
        else if (n.contains("course"))
            indexOut["course"] = tableCol;
        else if (n.contains("year"))
            indexOut["year_level"] = tableCol;
        else if (n.contains("department") || n.contains("dept"))
            indexOut["department"] = tableCol;
        else if (n.contains("gender"))
            indexOut["gender"] = tableCol;
        else if (n.contains("status"))
            indexOut["status"] = tableCol;
        else if (n.contains("visit"))
            indexOut["visits"] = tableCol;
        else
            indexOut[QString("col_%1").arg(tableCol)] = tableCol;
    }
}

ParsedTable ImportController::parseCsv(const QString &rawText)
{
    // Pure port of loadCSVtoTable (adminwindow.cpp:1465-1542), minus file I/O.
    ParsedTable table;

    QStringList lines = rawText.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
    }

    // Header line (matches lines.first().split(",", Qt::SkipEmptyParts), line 1488-1490).
    QStringList headers = lines.first().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString &h : headers)
        h = h.trimmed();

    if (headers.isEmpty())
        return table;   // empty ParsedTable — View shows "CSV file has no headers." (dead-in-practice)

    table.headers = headers;
    mapHeaders(headers, table.headerIndex);

    for (int i = 1; i < lines.size(); ++i) {
        // Data rows split WITHOUT Qt::SkipEmptyParts (line 1524) — deliberate
        // asymmetry from the header split.
        const QStringList rowData = lines.at(i).split(QLatin1Char(','));

        if (rowData.isEmpty() || (rowData.size() == 1 && rowData.first().trimmed().isEmpty()))
            continue;   // matches the empty-line guard at lines 1527-1529

        QStringList row;
        row.reserve(rowData.size());
        for (const QString &cell : rowData)
            row << cell.trimmed();
        table.rows << row;   // ragged rows preserved as-is — no padding/truncation here
    }

    return table;
}

bool ImportController::parseDuplicateResponse(const QByteArray &raw,
                                              QStringList *duplicatesOut,
                                              QString *errorOut)
{
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        *errorOut = QStringLiteral("Invalid duplicate check response.");
        return false;
    }

    const QJsonObject obj = doc.object();
    if (obj[QLatin1String("status")].toString() != QLatin1String("success")) {
        *errorOut = QStringLiteral("Duplicate check failed.");
        return false;
    }

    const QJsonArray dupArray = obj[QLatin1String("duplicates")].toArray();
    duplicatesOut->clear();
    for (const QJsonValue &v : dupArray)
        *duplicatesOut << v.toString();

    return true;
}

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
    const QString status       = obj[QLatin1String("status")].toString();
    const QString message      = obj[QLatin1String("message")].toString();
    const int successCount     = obj[QLatin1String("success_count")].toInt();
    const int errorCount       = obj[QLatin1String("error_count")].toInt();

    UploadResult result;
    result.message      = message;
    result.successCount = successCount;
    result.errorCount   = errorCount;
    result.plainText    = false;
    result.ok           = (status == QLatin1String("success"));
    return result;
}
```

- [ ] **Step 8: Build and run the new target — expect green**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure
```

Expected output ends with `100% tests passed, 0 tests failed out of 1` and the Qt Test summary reports 24 passed slots (plus `initTestCase`/`cleanupTestCase`), 0 failed.

- [ ] **Step 9: Run the full suite to confirm nothing else broke**

```powershell
ctest --test-dir qt-app/build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 7` (the 6 existing targets plus `tst_importcontroller`).

- [ ] **Step 10: Commit via the `commit` skill**

Invoke the `commit` skill (never raw `git add`/`git commit`). Proposed commit:

```
feat(import): add ImportController value types and pure helpers

Introduce importdata.h (ImportSeverity, ExcelParseError, ParsedTable,
UploadResult) and ImportController with its five unit-tested statics:
normalizeHeader, mapHeaders (unifying the two duplicated header-alias
if/else chains), parseCsv, parseDuplicateResponse, and
parseUploadResponse. parseExcel/checkDuplicates/uploadStudents are
declared but stubbed; they land in the next commit. Registers the
tst_importcontroller Qt Test target (24 cases, all synthetic data, no
live network).
```

---

## Task 2: checkDuplicates + uploadStudents + parseExcel (controller completion + tests)

Replace the three Task 1 stubs in `qt-app/importcontroller.cpp` with the real network calls and the real QXlsx parse, driving `parseExcel` with new failing round-trip tests first. The header does not change.

**Files:**
- Modify: `qt-app/importcontroller.cpp` (replace the three stub bodies; extend the include list)
- Modify: `qt-app/tests/tst_importcontroller.cpp` (add the `parseExcel` test slots)
- Test: `ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure`

**Interfaces:**
- Consumes (from Task 1): the final `importcontroller.h` declarations — `parseExcel(const QString &, ExcelParseError *)`, `checkDuplicates(const QStringList &)`, `uploadStudents(const QString &, const QString &, const QStringList &)`, `mapHeaders`, `parseDuplicateResponse`, `parseUploadResponse`, the signals `duplicatesResolved`/`importError`/`uploadStarted`/`uploadProgress`/`uploadFinished`/`uploadFailed`, and `m_nam`. Also consumes `ApiConfig::endpoint(const QString &)` (existing, `qt-app/apiconfig.h`) and QXlsx (`xlsxdocument.h`, already linked).
- Produces (used by Task 3): working `checkDuplicates` (emits `duplicatesResolved`/`importError`), working `uploadStudents` (emits `uploadStarted`/`uploadProgress`/`uploadFinished`/`uploadFailed`/`importError`), and working `parseExcel` (returns a populated `ParsedTable` + `ExcelParseError` out-param).

**Note on network-method testing:** `checkDuplicates` and `uploadStudents` are network-bound and get **no live-network test** — the project workflow rule forbids hitting the real backend from unit tests, and a mock-server harness is out of scope for a behavior-preserving extraction. Their parsing logic is already fully covered by the Task 1 `parseDuplicateResponse`/`parseUploadResponse` cases; the request/emit plumbing (including the `uploadStarted` state-machine seam and the fatal/non-fatal file-open paths) is verified manually in Task 3 against the running app. State this in the commit body rather than inventing a mock-heavy test.

- [ ] **Step 1: Write the failing `parseExcel` tests (red) — extend `qt-app/tests/tst_importcontroller.cpp`**

Add two includes at the top, next to the existing ones:

```cpp
#include <QTemporaryDir>
#include "xlsxdocument.h"
```

Add three slot declarations at the end of the `private slots:` section (after `parseUploadResponsePlainTextFallback();`):

```cpp
    // parseExcel
    void parseExcelRoundTrip();
    void parseExcelHeaderOnlyNoDataRows();
    void parseExcelOpenFailedOnBadPath();
```

Add the three test bodies before `QTEST_MAIN(TestImportController)`:

```cpp
void TestImportController::parseExcelRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("import_test.xlsx"));

    {
        QXlsx::Document doc;
        doc.write("A1", "School ID");
        doc.write("B1", "Full Name");
        doc.write("C1", "Course");
        doc.write("A2", "2023-00123");
        doc.write("B2", "Juan Dela Cruz");
        doc.write("C2", "BSIT");
        doc.write("A3", "2023-00456");
        doc.write("B3", "Maria Clara");
        doc.write("C3", "BSCS");
        QVERIFY(doc.saveAs(filePath));
    }

    ImportController controller(nullptr);   // parseExcel never touches the manager
    ExcelParseError err = ExcelParseError::OpenFailed;
    const ParsedTable table = controller.parseExcel(filePath, &err);

    QCOMPARE(err, ExcelParseError::None);
    QCOMPARE(table.headers, QStringList({"School ID", "Full Name", "Course"}));
    QCOMPARE(table.rows.size(), 2);
    QCOMPARE(table.rows[0], QStringList({"2023-00123", "Juan Dela Cruz", "BSIT"}));
    QCOMPARE(table.rows[1], QStringList({"2023-00456", "Maria Clara", "BSCS"}));
    QCOMPARE(table.headerIndex.value("school_id"), 0);
    QCOMPARE(table.headerIndex.value("name"),      1);
    QCOMPARE(table.headerIndex.value("course"),    2);
}

void TestImportController::parseExcelHeaderOnlyNoDataRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("import_header_only.xlsx"));

    {
        QXlsx::Document doc;
        doc.write("A1", "School ID");
        doc.write("B1", "Full Name");
        QVERIFY(doc.saveAs(filePath));
    }

    ImportController controller(nullptr);
    ExcelParseError err = ExcelParseError::OpenFailed;
    const ParsedTable table = controller.parseExcel(filePath, &err);

    QCOMPARE(err, ExcelParseError::None);
    QCOMPARE(table.headers, QStringList({"School ID", "Full Name"}));
    QVERIFY(table.rows.isEmpty());
}

void TestImportController::parseExcelOpenFailedOnBadPath()
{
    ImportController controller(nullptr);
    ExcelParseError err = ExcelParseError::None;
    const ParsedTable table = controller.parseExcel(
        QStringLiteral("nonexistent_path_does_not_exist.xlsx"), &err);

    QCOMPARE(err, ExcelParseError::OpenFailed);
    QVERIFY(table.headers.isEmpty());
    QVERIFY(table.rows.isEmpty());
}
```

Build and run — red:

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure
```

Expected: the 24 Task 1 slots still pass; `parseExcelRoundTrip` and `parseExcelHeaderOnlyNoDataRows` **FAIL** on the `QCOMPARE(err, ExcelParseError::None)` assertion because the stub always sets `ExcelParseError::OpenFailed`; `parseExcelOpenFailedOnBadPath` **PASSES already** (the stub's hardcoded behavior happens to match this one case) — that is expected and fine, it will keep passing once the real implementation lands.

- [ ] **Step 2: Extend the include list in `qt-app/importcontroller.cpp`**

Replace the current include block:

```cpp
#include "importcontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
```

with:

```cpp
#include "importcontroller.h"
#include "apiconfig.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "xlsxdocument.h"
#include "xlsxcellrange.h"
```

(`xlsxdocument.h`/`xlsxcellrange.h` are the same include form `adminwindow.cpp` already uses, lines 52–54.)

- [ ] **Step 3: Replace the `checkDuplicates` stub with the real implementation**

Port of adminwindow.cpp:1164–1203, minus everything past duplicate-detection (that half is the async dialog seam, which stays in the View per the spec).

```cpp
void ImportController::checkDuplicates(const QStringList &schoolIds)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("check_duplicates.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonArray idsArray;
    for (const QString &id : schoolIds)
        idsArray.append(id);

    QJsonObject payload;
    payload[QLatin1String("school_ids")] = idsArray;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());

    // `this` (the controller) as the context object is mandatory: the
    // connection auto-disconnects if the controller is destroyed while the
    // reply — owned by the shared QNetworkAccessManager — is still in flight.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            // Legacy uses "Error" as the title here, not "Network Error"
            // (adminwindow.cpp:1180) — preserved verbatim, not unified with
            // VisitorController's "Network Error" convention.
            emit importError(QStringLiteral("Error"), reply->errorString(),
                             ImportSeverity::Critical);
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QStringList duplicates;
        QString errorMsg;
        if (!parseDuplicateResponse(resp, &duplicates, &errorMsg)) {
            emit importError(QStringLiteral("Error"), errorMsg, ImportSeverity::Warning);
            return;
        }

        emit duplicatesResolved(duplicates);   // empty list = no duplicates found
    });
}
```

- [ ] **Step 4: Replace the `uploadStudents` stub with the real implementation**

Port of adminwindow.cpp:1250–1366 (Step 4 onward of the legacy flow). This is the method with the state-machine seam: `uploadStarted()` fires immediately after the excel `QFile` opens successfully, matching the spec's "state revert on failure" analysis exactly.

```cpp
void ImportController::uploadStudents(const QString &excelPath, const QString &zipPath,
                                      const QStringList &skipIds)
{
    QNetworkRequest uploadRequest(ApiConfig::endpoint(QStringLiteral("upload_students_zip.php")));
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // Excel part (mandatory, fatal on failure).
    QHttpPart excelPart;
    excelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"excel\"; filename=\"" +
                                 QFileInfo(excelPath).fileName() + "\""));
    QFile *excelFile = new QFile(excelPath);
    if (!excelFile->open(QIODevice::ReadOnly)) {
        // Fatal, exactly as legacy (adminwindow.cpp:1270-1278): free what was
        // already allocated and abort before sending anything. uploadStarted
        // never fires here, so the View never sets progress/button state —
        // nothing to revert, reproducing legacy's set-then-revert net effect.
        emit importError(QStringLiteral("Error"), QStringLiteral("Cannot open Excel file."),
                         ImportSeverity::Critical);
        delete excelFile;
        delete multiPart;
        return;
    }
    excelPart.setBodyDevice(excelFile);
    excelFile->setParent(multiPart);
    multiPart->append(excelPart);

    // The excel file opened OK — this is the exact point legacy passed line
    // 1270 and had already set the progress/button state at lines 1251-1258.
    // The View sets that state in its onUploadStarted slot.
    emit uploadStarted();

    // ZIP part (optional, non-fatal on failure).
    if (!zipPath.isEmpty()) {
        QHttpPart zipPart;
        zipPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"photos_zip\"; filename=\"" +
                                   QFileInfo(zipPath).fileName() + "\""));
        QFile *zipFile = new QFile(zipPath);
        if (!zipFile->open(QIODevice::ReadOnly)) {
            // Non-fatal as legacy (adminwindow.cpp:1291): warn and continue
            // the upload without the ZIP part. The delete below fixes a
            // pre-existing zipFile leak (legacy never freed it on this
            // branch); no observable behavior change.
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

    // skip_ids part — only appended when non-empty (adminwindow.cpp:1300-1306).
    if (!skipIds.isEmpty()) {
        QHttpPart dupPart;
        dupPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"skip_ids\""));
        dupPart.setBody(skipIds.join(",").toUtf8());
        multiPart->append(dupPart);
    }

    QNetworkReply *uploadReply = m_nam->post(uploadRequest, multiPart);
    multiPart->setParent(uploadReply);

    connect(uploadReply, &QNetworkReply::uploadProgress, this,
            [this](qint64 bytesSent, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    const int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
                    emit uploadProgress(percent);
                }
            });

    // `this` (the controller) as the context object — same rationale as
    // checkDuplicates and VisitorController::fetchVisitors.
    connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply]() {
        if (uploadReply->error() != QNetworkReply::NoError) {
            emit uploadFailed(uploadReply->errorString());
            uploadReply->deleteLater();
            return;
        }

        const QByteArray response = uploadReply->readAll();
        uploadReply->deleteLater();

        emit uploadFinished(parseUploadResponse(response));
    });
}
```

- [ ] **Step 5: Replace the `parseExcel` stub with the real implementation**

Port of `loadExcelToTable` (adminwindow.cpp:1384–1463), minus all `ui->` and `QMessageBox` calls. Table-clear-on-`EmptySheet` is a View concern (Task 3) — `parseExcel` itself never touches a widget.

```cpp
ParsedTable ImportController::parseExcel(const QString &filePath, ExcelParseError *errorOut)
{
    ParsedTable table;

    QXlsx::Document xlsx(filePath);
    if (!xlsx.isLoadPackage()) {
        if (errorOut)
            *errorOut = ExcelParseError::OpenFailed;
        return table;
    }

    const QStringList sheets = xlsx.sheetNames();
    if (sheets.isEmpty()) {
        if (errorOut)
            *errorOut = ExcelParseError::NoSheets;
        return table;
    }
    xlsx.selectSheet(sheets.first());

    const QXlsx::CellRange rng = xlsx.dimension();
    if (!rng.isValid()) {
        if (errorOut)
            *errorOut = ExcelParseError::EmptySheet;
        return table;
    }

    const int firstRow = rng.firstRow();
    const int lastRow  = rng.lastRow();
    const int firstCol = rng.firstColumn();
    const int lastCol  = rng.lastColumn();

    QStringList headers;
    for (int c = firstCol; c <= lastCol; ++c)
        headers << xlsx.read(firstRow, c).toString();

    table.headers = headers;
    mapHeaders(headers, table.headerIndex);   // tableCol = c - firstCol, matching line 1422

    // rows = lastRow - firstRow (excludes header row) — off-by-one convention
    // preserved verbatim from line 1446, NOT lastRow - firstRow + 1.
    for (int r = firstRow + 1; r <= lastRow; ++r) {
        QStringList row;
        for (int c = firstCol; c <= lastCol; ++c)
            row << xlsx.read(r, c).toString();
        table.rows << row;
    }

    if (errorOut)
        *errorOut = ExcelParseError::None;
    return table;
}
```

- [ ] **Step 6: Build and run the new target — expect green**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_importcontroller --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`; the Qt Test summary reports 27 passed slots, 0 failed.

- [ ] **Step 7: Run the full suite**

```powershell
ctest --test-dir qt-app/build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 7`.

- [ ] **Step 8: Commit via the `commit` skill**

Proposed commit:

```
feat(import): implement duplicate check and upload in controller

checkDuplicates POSTs {school_ids} to check_duplicates.php via the
injected shared QNetworkAccessManager and reports back through
duplicatesResolved/importError, preserving the legacy "Error" title
on both the network-failure and bad-response paths. uploadStudents
builds the excel/zip/skip_ids multipart, treating excel-open failure
as fatal (with resource cleanup) and zip-open failure as non-fatal;
it emits uploadStarted immediately after the excel file opens so the
View can drive the progress-bar/button state machine off a single
signal instead of duplicating legacy's set-then-revert logic.
parseExcel ports loadExcelToTable's QXlsx read loop verbatim,
including the off-by-one row count and the ExcelParseError out-param
for the three legacy failure dialogs, covered by a QTemporaryDir
round-trip test. Neither network method has a live-network test by
project rule; their parse halves are covered by the Task 1 cases.
```

---

## Task 3: Wire ImportController into adminWindow (View-only remainder)

Make `adminWindow` a pure View for the Database Import tab: construct the controller, connect its six signals, rewrite `onAttachFileBtnClicked`/`onUpdateDatabaseBtnClicked`/`loadExcelToTable`/`loadCSVtoTable` to call into the controller and own only widget/dialog work, delete the free-function `normalizeHeader` and both duplicated header-mapping chains, keep `onCancelUploadBtnClicked` untouched.

**Files:**
- Modify: `qt-app/adminwindow.h`
- Modify: `qt-app/adminwindow.cpp`
- Test: full build + `ctest --test-dir qt-app/build --output-on-failure` + grep checks + manual app smoke test

**Interfaces:**
- Consumes (from Tasks 1–2): `ImportController(QNetworkAccessManager *, QObject *)`, `parseCsv(const QString &)`, `parseExcel(const QString &, ExcelParseError *)`, `checkDuplicates(const QStringList &)`, `uploadStudents(const QString &, const QString &, const QStringList &)`, signals `duplicatesResolved`/`importError`/`uploadStarted`/`uploadProgress`/`uploadFinished`/`uploadFailed`, types `ParsedTable`, `UploadResult`, `ImportSeverity`, `ExcelParseError`.
- Produces: slots `onImportDuplicatesResolved`, `onImportError`, `onUploadStarted`, `onUploadProgress`, `onUploadFinished`, `onUploadFailed`; member `m_importController`; rewritten bodies of `onAttachFileBtnClicked`, `onUpdateDatabaseBtnClicked`, `loadExcelToTable`, `loadCSVtoTable`.

- [ ] **Step 1: Update `qt-app/adminwindow.h`**

Add the two includes directly after the visitor includes (currently lines 41–42):

```cpp
#include "visitordata.h"
#include "visitorcontroller.h"
```

becomes:

```cpp
#include "visitordata.h"
#include "visitorcontroller.h"
#include "importdata.h"
#include "importcontroller.h"
```

In the `private:` section, directly after the visitor block (currently lines 97–100):

```cpp
    VisitorController *m_visitorController;   // child of this, created in ctor
    QList<VisitorRecord> m_visitorRecords;    // cache of the last visitorsLoaded payload

    VisitorFilter collectVisitorFilter() const;
```

add:

```cpp
    ImportController *m_importController;   // child of this, created in ctor
```

`selectedExcelPath`, `selectedZipPath`, `cancelUpload`, and `bulkHeaderIndex` (currently lines 83–84, 101, 128) are **unchanged** — they stay View-owned state per the spec's "What stays in the View" section.

In the `private slots:` section, after `void onVisitorFetchError(const QString &title, const QString &message);` (currently line 176), add:

```cpp
    void onImportDuplicatesResolved(const QStringList &duplicates);
    void onImportError(const QString &title, const QString &message, ImportSeverity severity);
    void onUploadStarted();
    void onUploadProgress(int percent);
    void onUploadFinished(const UploadResult &result);
    void onUploadFailed(const QString &message);
```

`loadCSVtoTable(const QString &filePath)` and `loadExcelToTable(const QString &filePath)` (currently lines 163–164) **stay declared as-is** — their bodies are rewritten in Step 5/6 below to call into the controller, but the signatures and slot-ness are unchanged (both are called synchronously from `onAttachFileBtnClicked`, not connected to any signal).

- [ ] **Step 2: Construct and wire the controller in the constructor (`qt-app/adminwindow.cpp`)**

Locate the visitor controller wiring (lines 424–429):

```cpp
    networkManager = new QNetworkAccessManager(this);
    m_visitorController = new VisitorController(networkManager, this);
    connect(m_visitorController, &VisitorController::visitorsLoaded,
            this, &adminWindow::populateVisitorTable);
    connect(m_visitorController, &VisitorController::fetchError,
            this, &adminWindow::onVisitorFetchError);
```

Directly after it, add:

```cpp
    m_importController = new ImportController(networkManager, this);
    connect(m_importController, &ImportController::duplicatesResolved,
            this, &adminWindow::onImportDuplicatesResolved);
    connect(m_importController, &ImportController::importError,
            this, &adminWindow::onImportError);
    connect(m_importController, &ImportController::uploadStarted,
            this, &adminWindow::onUploadStarted);
    connect(m_importController, &ImportController::uploadProgress,
            this, &adminWindow::onUploadProgress);
    connect(m_importController, &ImportController::uploadFinished,
            this, &adminWindow::onUploadFinished);
    connect(m_importController, &ImportController::uploadFailed,
            this, &adminWindow::onUploadFailed);
```

Same-thread direct connections — no `Q_DECLARE_METATYPE`/`qRegisterMetaType` needed for `UploadResult` or `ImportSeverity` since all six signals are emitted and received on the GUI thread (matching the `VisitorController` precedent for `QList<VisitorRecord>`).

- [ ] **Step 3: Rewrite `onAttachFileBtnClicked`**

Current code (adminwindow.cpp:1090–1121):

```cpp
void adminWindow::onAttachFileBtnClicked()
{
    AttachFilesDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        selectedExcelPath = dialog.getExcelPath();
        selectedZipPath = dialog.getZipPath();

        if (selectedExcelPath.isEmpty()) {
            QMessageBox::warning(this, "Missing Data File", "You must select a student data file (Excel or CSV).");
            return;
        }

        // Preview file contents in your table
        if (selectedExcelPath.endsWith(".csv", Qt::CaseInsensitive)) {
            loadCSVtoTable(selectedExcelPath);
        } else if (selectedExcelPath.endsWith(".xlsx", Qt::CaseInsensitive)) {
            loadExcelToTable(selectedExcelPath);
        } else {
            QMessageBox::warning(this, "Invalid File", "Please select a CSV or Excel file.");
            return;
        }

        // Optionally show attached filenames in your UI
        QString summary = QString("Data: %1 | Photos: %2")
                              .arg(QFileInfo(selectedExcelPath).fileName())
                              .arg(selectedZipPath.isEmpty() ? "None" : QFileInfo(selectedZipPath).fileName());

        //if (ui->attachedFilesLabel)
        //    ui->attachedFilesLabel->setText(summary);
    }
}
```

Replacement (the dispatch/validation texts are unchanged — rows 1/2 of the message inventory stay View-owned; the dispatch itself is unchanged since `loadCSVtoTable`/`loadExcelToTable` keep their existing call sites and signatures):

```cpp
void adminWindow::onAttachFileBtnClicked()
{
    AttachFilesDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        selectedExcelPath = dialog.getExcelPath();
        selectedZipPath = dialog.getZipPath();

        if (selectedExcelPath.isEmpty()) {
            QMessageBox::warning(this, "Missing Data File", "You must select a student data file (Excel or CSV).");
            return;
        }

        // Preview file contents in your table
        if (selectedExcelPath.endsWith(".csv", Qt::CaseInsensitive)) {
            loadCSVtoTable(selectedExcelPath);
        } else if (selectedExcelPath.endsWith(".xlsx", Qt::CaseInsensitive)) {
            loadExcelToTable(selectedExcelPath);
        } else {
            QMessageBox::warning(this, "Invalid File", "Please select a CSV or Excel file.");
            return;
        }

        // Optionally show attached filenames in your UI
        QString summary = QString("Data: %1 | Photos: %2")
                              .arg(QFileInfo(selectedExcelPath).fileName())
                              .arg(selectedZipPath.isEmpty() ? "None" : QFileInfo(selectedZipPath).fileName());

        //if (ui->attachedFilesLabel)
        //    ui->attachedFilesLabel->setText(summary);
    }
}
```

This slot is **unchanged verbatim** — it already only validates and dispatches; the network/parse logic it calls into (`loadCSVtoTable`/`loadExcelToTable`) is what gets rewritten in Steps 5–6. Leaving it untouched here documents that no edit is needed, rather than silently skipping it.

- [ ] **Step 4: Rewrite `onUpdateDatabaseBtnClicked`**

Current code (adminwindow.cpp:1123–1367) is the full legacy flow shown in the spec context. Replace the **entire** method body with the View-only remainder: guards, live-table school-ID collection, `checkDuplicates` call. The duplicate-dialog seam and the upload call move to `onImportDuplicatesResolved` (Step 7).

```cpp
void adminWindow::onUpdateDatabaseBtnClicked()
{
    if (selectedExcelPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Data File", "Please attach the student Excel/CSV file first.");
        return;
    }

    if (selectedZipPath.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "No Photo ZIP",
            "No ZIP file was selected. Continue uploading without photos?",
            QMessageBox::Yes | QMessageBox::No
            );
        if (reply == QMessageBox::No) return;
    }

    int rowCount = ui->bulkTable->rowCount();
    if (rowCount == 0) {
        QMessageBox::warning(this, "Error", "No data to upload.");
        return;
    }

    // helper lambda to extract cell safely — reads the LIVE table so any
    // hand-edit the user made after the preview load is picked up (bulkTable
    // is editable, unlike visitorTable — this is why school IDs are NOT
    // cached from the ParsedTable, only bulkHeaderIndex is).
    auto getCell = [&](int row, const QString &key, int fallbackIndex = -1) -> QString {
        int col = -1;
        if (bulkHeaderIndex.contains(key)) col = bulkHeaderIndex[key];
        else if (fallbackIndex >= 0 && fallbackIndex < ui->bulkTable->columnCount()) col = fallbackIndex;
        if (col < 0) return QString();
        QTableWidgetItem *it = ui->bulkTable->item(row, col);
        return it ? it->text().trimmed() : QString();
    };

    QStringList schoolIds;
    for (int i = 0; i < rowCount; i++) {
        QString sid = getCell(i, "school_id", 1);
        if (!sid.isEmpty())
            schoolIds << sid;
    }

    m_importController->checkDuplicates(schoolIds);
}
```

- [ ] **Step 5: Rewrite `loadExcelToTable`**

Current code (adminwindow.cpp:1384–1463). Replace the entire body — the QXlsx parse moves to `ImportController::parseExcel`; the View keeps the exact table-population sequence and the three failure dialogs, clearing the table only on `EmptySheet`.

```cpp
void adminWindow::loadExcelToTable(const QString &filePath)
{
    ExcelParseError err = ExcelParseError::None;
    const ParsedTable table = m_importController->parseExcel(filePath, &err);

    if (err == ExcelParseError::OpenFailed) {
        QMessageBox::warning(this, "Error", "Failed to open Excel file.");
        return;   // table left untouched, matching adminwindow.cpp:1387-1390
    }
    if (err == ExcelParseError::NoSheets) {
        QMessageBox::warning(this, "Error", "This workbook has no sheets.");
        return;   // table left untouched, matching adminwindow.cpp:1393-1396
    }
    if (err == ExcelParseError::EmptySheet) {
        QMessageBox::information(this, "Excel", "Sheet is empty.");
        ui->bulkTable->clear();
        ui->bulkTable->setRowCount(0);
        ui->bulkTable->setColumnCount(0);
        return;   // matches adminwindow.cpp:1401-1406
    }

    bulkHeaderIndex = table.headerIndex;

    // Excel population — port of loadExcelToTable lines 1449-1462 verbatim.
    ui->bulkTable->clear();
    ui->bulkTable->setRowCount(table.rows.size());
    ui->bulkTable->setColumnCount(table.headers.size());
    ui->bulkTable->setHorizontalHeaderLabels(table.headers);

    for (int r = 0; r < table.rows.size(); ++r) {
        const QStringList &row = table.rows.at(r);
        for (int c = 0; c < row.size(); ++c)
            ui->bulkTable->setItem(r, c, new QTableWidgetItem(row.at(c)));
    }

    ui->bulkTable->resizeColumnsToContents();
}
```

- [ ] **Step 6: Rewrite `loadCSVtoTable`**

Current code (adminwindow.cpp:1465–1542). The View keeps its own `QFile::open` check (row 6 of the message inventory — `parseCsv` takes raw text, not a path) and the two dead/live dialogs (rows 7/8), plus the exact "fill against stale column count, widen afterward" sequence.

```cpp
void adminWindow::loadCSVtoTable(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + filePath);
        return;
    }

    QTextStream in(&file);
    const QString rawText = in.readAll();
    file.close();

    // CSV clears the table up front — before any failure dialog — unlike
    // Excel's case-specific clear. This is why a failed CSV load clears the
    // table but a failed Excel OpenFailed/NoSheets does not.
    ui->bulkTable->clear();
    ui->bulkTable->setRowCount(0);

    if (rawText.isEmpty()) {
        QMessageBox::information(this, "CSV Preview", "File is empty.");
        return;
    }

    const ParsedTable table = m_importController->parseCsv(rawText);

    if (table.headers.isEmpty()) {
        QMessageBox::warning(this, "CSV Error", "CSV file has no headers.");
        return;
    }

    bulkHeaderIndex = table.headerIndex;

    // CSV population — port of loadCSVtoTable lines 1523-1541 verbatim,
    // including the "fill against stale column count, widen afterward" quirk.
    for (const QStringList &rowData : table.rows) {
        const int row = ui->bulkTable->rowCount();
        ui->bulkTable->insertRow(row);

        for (int j = 0; j < rowData.size() && j < ui->bulkTable->columnCount(); j++) {
            ui->bulkTable->setItem(row, j, new QTableWidgetItem(rowData.at(j)));
        }
    }

    ui->bulkTable->setColumnCount(qMax(ui->bulkTable->columnCount(), table.headers.size()));
    ui->bulkTable->setHorizontalHeaderLabels(table.headers);
    ui->bulkTable->resizeColumnsToContents();
}
```

- [ ] **Step 7: Delete the free-function `normalizeHeader` and add the six new slots**

Delete the free function entirely (adminwindow.cpp:1376–1382):

```cpp
static QString normalizeHeader(const QString &h) {
    QString s = h.trimmed().toLower();
    s.remove(' ');
    s.remove('_');
    s.remove('-');
    return s;
}
```

`ImportController::normalizeHeader`/`mapHeaders` (Task 1) are now the only copy of this logic — `adminwindow.cpp` never calls `normalizeHeader` directly, since both loaders now go through `parseCsv`/`parseExcel`, which call `mapHeaders` internally.

In its place (i.e. directly before `void adminWindow::loadExcelToTable`, or anywhere else in the Bulk Registration section — placement inside the `// --- CSV Bulk Registration ---` block is preferred for locality), add the six new slot bodies:

```cpp
void adminWindow::onImportDuplicatesResolved(const QStringList &duplicates)
{
    if (duplicates.isEmpty()) {
        m_importController->uploadStudents(selectedExcelPath, selectedZipPath, QStringList{});
        return;
    }

    // Show only first 3 in the main message
    QString previewList = duplicates.mid(0, 3).join("\n");
    QString msg = "The following School IDs already exist:\n\n" + previewList;

    if (duplicates.size() > 3) {
        msg += "\n...and more.\n\nDo you want to skip them and continue?";
    } else {
        msg += "\n\nDo you want to skip them and continue?";
    }

    QMessageBox box(QMessageBox::Question,
                    "Duplicates Found",
                    msg,
                    QMessageBox::Yes | QMessageBox::No,
                    this);

    // Add "Show More" button if there are more than 3
    QPushButton *showMoreBtn = nullptr;
    if (duplicates.size() > 3) {
        showMoreBtn = box.addButton("Show More", QMessageBox::ActionRole);
    }

    box.exec();

    if (box.clickedButton() == showMoreBtn) {
        // Show full list in a new dialog
        QString fullMsg = "Full list of duplicate School IDs:\n\n" + duplicates.join("\n");
        QMessageBox::information(this, "All Duplicates", fullMsg);

        // Re-ask the original question after showing full list
        QMessageBox::StandardButton choice =
            QMessageBox::question(this, "Proceed?",
                                  "Do you want to skip duplicates and continue?",
                                  QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            QMessageBox::information(this, "Cancelled", "Bulk upload cancelled.");
            return;
        }
    } else if (box.standardButton(box.clickedButton()) == QMessageBox::No) {
        QMessageBox::information(this, "Cancelled", "Bulk upload cancelled.");
        return;
    }

    // Yes (either directly or via the Show More re-ask) — the entire
    // duplicates list becomes skipIds, matching legacy line 1300's guard
    // which always sends the full list.
    m_importController->uploadStudents(selectedExcelPath, selectedZipPath, duplicates);
}

void adminWindow::onImportError(const QString &title, const QString &message, ImportSeverity severity)
{
    if (severity == ImportSeverity::Critical)
        QMessageBox::critical(this, title, message);
    else
        QMessageBox::warning(this, title, message);
}

void adminWindow::onUploadStarted()
{
    cancelUpload = false;
    ui->cancelUploadBtn->setEnabled(true);
    ui->updateDatabaseBtn->setEnabled(false);

    ui->bulkProgressBar->setVisible(true);
    ui->bulkProgressBar->setMinimum(0);
    ui->bulkProgressBar->setMaximum(0); // Indeterminate progress during upload
    ui->bulkProgressBar->setValue(0);
}

void adminWindow::onUploadProgress(int percent)
{
    // Every emission re-asserts determinate mode, matching the legacy lambda
    // which called setMaximum(100) unconditionally inside uploadProgress
    // (adminwindow.cpp:1362) — cheap, idempotent, preserved exactly.
    ui->bulkProgressBar->setMaximum(100);
    ui->bulkProgressBar->setValue(percent);
}

void adminWindow::onUploadFinished(const UploadResult &result)
{
    ui->bulkProgressBar->setVisible(false);
    ui->updateDatabaseBtn->setEnabled(true);
    ui->cancelUploadBtn->setEnabled(false);

    if (result.plainText) {
        QMessageBox::information(this, "Upload Complete", result.rawText);
        return;
    }

    if (result.ok) {
        QMessageBox::information(this, "Upload Complete",
                                 QString("Bulk upload finished!\n\n"
                                         "Successfully inserted: %1\n"
                                         "Errors/Skipped: %2\n\n%3")
                                     .arg(result.successCount)
                                     .arg(result.errorCount)
                                     .arg(result.message));

        // Clear the table after successful upload — the ONLY outcome that
        // clears bulkTable/selectedExcelPath/selectedZipPath.
        ui->bulkTable->setRowCount(0);
        selectedExcelPath.clear();
        selectedZipPath.clear();
    } else {
        QMessageBox::warning(this, "Upload Issue", result.message);
    }
}

void adminWindow::onUploadFailed(const QString &message)
{
    ui->bulkProgressBar->setVisible(false);
    ui->updateDatabaseBtn->setEnabled(true);
    ui->cancelUploadBtn->setEnabled(false);

    QMessageBox::critical(this, "Upload Failed", message);
}
```

`onCancelUploadBtnClicked` (adminwindow.cpp:1369–1374) is **unchanged**:

```cpp
void adminWindow::onCancelUploadBtnClicked()
{
    cancelUpload = true;
    QMessageBox::information(this, "Cancelled", "Bulk upload has been cancelled.");
    ui->bulkTable->clearContents();
}
```

- [ ] **Step 8: Build and run all tests**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```

Expected: build completes with **no new warnings or errors**, and `100% tests passed, 0 tests failed out of 7`.

- [ ] **Step 9: Grep checks (layer purity + no dead code)**

Run these from the repo root (Git Bash) and confirm the expected output for each:

```bash
grep -n "^static QString normalizeHeader" qt-app/adminwindow.cpp
# expect: NO output — the free function is deleted

grep -c "n.contains(\"schoolid\")" qt-app/adminwindow.cpp
# expect: 0 — both duplicated header-mapping chains are gone from adminwindow.cpp

grep -n "ui->" qt-app/importcontroller.cpp
# expect: NO output — the controller never touches a widget

grep -n "QFileDialog\|QMessageBox\|AttachFilesDialog" qt-app/importcontroller.cpp
# expect: NO output — dialogs and the attach dialog live only in the View

grep -n "bulkTable" qt-app/importcontroller.cpp
# expect: NO output

grep -n "QNetworkAccessManager\|QJsonDocument::fromJson" qt-app/adminwindow.cpp | grep -i "onUpdateDatabaseBtnClicked\|check_duplicates\|upload_students_zip"
# expect: NO output — the import tab's POSTs and JSON parsing are gone from adminwindow.cpp
```

Scope note on the QXlsx check: `grep -n "QXlsx" qt-app/adminwindow.cpp` will still hit — the `xlsxdocument.h`/`xlsxformat.h`/`xlsxcellrange.h` includes (lines ~52–54) and `exportReportToExcel` legitimately keep QXlsx for the Reports tab, which is out of scope for this extraction. The checks that matter are the targeted greps above: `importcontroller.cpp` has zero `ui->`/dialog/`bulkTable` tokens, and `adminwindow.cpp`'s `loadExcelToTable`/`loadCSVtoTable` bodies (verified by reading Steps 5–6's replacement, already applied) contain no `QXlsx::Document` construction — only the View-side table-population loop remains, which is plain `QTableWidget` code, not QXlsx.

- [ ] **Step 10: Manual smoke test of the running app**

Launch `qt-app/build/WITS.exe` — the binary this plan's build just produced, **not** a stale Qt Creator build from another checkout. With the PHP backend running, walk the spec's verification criteria:

1. Attach File with no data file selected shows "Missing Data File"; an unrecognized extension shows "Invalid File".
2. A valid `.csv` file previews into `bulkTable` with the same headers, row count, and cell contents as before, including a ragged row (fewer cells than headers) and the `col_N` fallback for an unrecognized header column.
3. A valid `.xlsx` file previews identically (Excel-specific: header + data rows, `resizeColumnsToContents()` applied).
4. Each Excel-load failure still shows its exact original dialog: a corrupted/non-package `.xlsx` → "Failed to open Excel file."; a workbook with the header row deleted so the sheet's dimension is invalid → "Sheet is empty." (and the table clears this time, unlike the open-failed/no-sheets cases).
5. Update Database with no attachment → "Missing Data File"; with no ZIP → "No Photo ZIP" question; with an empty table → "No data to upload."
6. Trigger a duplicate school ID (upload the same CSV twice) → "Duplicates Found" dialog with the first-3 preview; with more than 3 duplicates, click "Show More" → "All Duplicates" full list → "Proceed?" re-ask → both Yes and No paths work, including "Cancelled" on No.
7. Uploading with no ZIP prompts row 10; a ZIP that fails to open (e.g. rename a non-zip file to `.zip` and select it) shows the "Warning"/"Cannot open ZIP file..." box and proceeds without photos; an Excel file that fails to open (delete it after attaching, before clicking Update Database) aborts with the critical "Cannot open Excel file." box and re-enables the buttons without ever showing the progress bar.
8. The progress bar goes indeterminate then tracks percentage exactly as before; button enable/disable state matches at every stage (disabled Update Database / enabled Cancel Upload during upload, reverted after).
9. Success, issue, and plain-text-fallback upload outcomes show the identical dialog text; only the success case clears the table and the two selected-path members (attach a new file afterward to confirm `selectedExcelPath`/`selectedZipPath` were actually cleared — Update Database should show "Missing Data File" again).
10. Cancel Upload behaves unchanged (info box + `bulkTable->clearContents()`).

- [ ] **Step 11: Commit via the `commit` skill**

Proposed commit:

```
refactor(import): wire ImportController into adminWindow

adminWindow is now a pure View for the Database Import tab:
onUpdateDatabaseBtnClicked shrinks to guards + live-table school-ID
collection + checkDuplicates, the duplicate-dialog seam and the
upload call move to onImportDuplicatesResolved, and the
progress-bar/button state machine is driven by onUploadStarted/
onUploadProgress/onUploadFinished/onUploadFailed. loadExcelToTable
and loadCSVtoTable keep their signatures but now call parseExcel/
parseCsv and only perform table-population plumbing, preserving each
loader's distinct sequence (Excel's clear-then-fill-upfront vs CSV's
clear-then-fill-against-stale-columncount-then-widen) and the
Excel-only EmptySheet table clear. The free normalizeHeader function
and both duplicated header-mapping if/else chains are deleted;
ImportController::mapHeaders is now the single copy. Behavior-
identical — every dialog title/message/severity and the wire payload
shapes are unchanged.
```

---

## Post-Task: Run /claude-review

After all three tasks are committed, the build is clean, all 7 ctest targets are green, and the manual smoke test passes, run `/claude-review` (via the Skill tool) before finishing the branch. Fix any Critical or Important findings, re-submit until APPROVE, then use `superpowers:finishing-a-development-branch` and the project-scoped `create-pr` skill.
