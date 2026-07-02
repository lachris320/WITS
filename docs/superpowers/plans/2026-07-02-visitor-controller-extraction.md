# VisitorController Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract all Visitor Logs tab logic (network fetch, response parsing, date-range math, filename derivation, QXlsx export) from `adminWindow` into a `VisitorController : QObject` plus plain value types, leaving `adminWindow` a pure View for that tab — behavior-identical, with unit tests for every pure piece.

**Architecture:** Plain value types (`DateFilterType`, `VisitorRecord`, `VisitorFilter` in `visitordata.h`) carry visitor data between the layers. `VisitorController` borrows the shared `QNetworkAccessManager`, POSTs to `get_visitors.php`, parses the response with a pure static, and reports back via `visitorsLoaded`/`fetchError` signals; it also owns the QXlsx export as a synchronous call over `QList<VisitorRecord>`. `adminWindow` keeps only widget work: it maps filter widgets to a `VisitorFilter` at the edge, populates the table from the signal payload, and shows all dialogs/message boxes.

**Tech Stack:** Qt 6.11 Widgets, Qt Network, Qt Test, QXlsx (vendored), CMake+Ninja.

## Global Constraints

- **Behavior-preserving extraction** — every wire string (`"all"`, `"Specific Day"`, `"Month"`, `"Date Range"`), payload key (`search`, `date_type`, `start_date`, `end_date`), and user-facing message text is transcribed verbatim from the current code; no wire-protocol or UI-text change of any kind.
- **`#ifndef` include guards** (`VISITORDATA_H`, `VISITORCONTROLLER_H`) — never `#pragma once` (repo convention, see commit 176d39c and `settingscontroller.h`).
- **Controller purity** — `visitorcontroller.h/.cpp` must contain no `ui->` access, no `QFileDialog`, and no `QMessageBox`; dialogs stay in the View.
- **`QNetworkAccessManager` is injected, not owned** — the controller stores the pointer to `adminWindow`'s `networkManager`; ownership stays with `adminWindow` (Qt parent).
- **Context object mandatory on `reply->finished`** — `connect(reply, &QNetworkReply::finished, this, [this, reply]{...})` with `this` = the controller, so the connection auto-disconnects if the controller dies while a reply is in flight; `reply->deleteLater()` on every path.
- **Test target environment** — `tst_visitorcontroller` sets `ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"` (QXlsx propagates `Qt::Gui`, so `QTEST_MAIN` instantiates a `QGuiApplication`, which needs a platform plugin headlessly).
- **No live-network tests** — `fetchVisitors` is exercised only through `parseVisitorsResponse` unit tests plus manual app verification; tests never hit `http://localhost`.
- **Commits only via the `commit` skill** (project rule) — never raw `git add` / `git commit`.
- **No secrets, no real student/visitor PII, no personal machine paths** in any committed file; all test data is synthetic (`"Juan Dela Cruz"`, `"Acme Corp"`, ...).
- **Do not modify `qt-app/libs/QXlsx/`** — vendored third-party code.
- Build + test commands below; run them exactly (Qt tools are not on PATH on this machine).

## Build & Test Commands (Windows, tools not on PATH)

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"   # configure (once)
cmake --build qt-app/build                                                              # build
ctest --test-dir qt-app/build --output-on-failure                                       # all tests
ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure              # just the new target
```
(Ignore "LF will be replaced by CRLF" warnings and the QXlsx GuiPrivate CMake warning.)

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `qt-app/visitordata.h` | **Create** (Task 1) | `DateFilterType`, `VisitorRecord`, `VisitorFilter` value types |
| `qt-app/visitorcontroller.h` | **Create** (Task 1, final in Task 1) | `VisitorController` declaration |
| `qt-app/visitorcontroller.cpp` | **Create** (Task 1), complete (Task 2) | Statics in Task 1; `fetchVisitors`/`exportToExcel` in Task 2 |
| `qt-app/tests/tst_visitorcontroller.cpp` | **Create** (Task 1), extend (Task 2) | Qt Test coverage for the controller |
| `qt-app/CMakeLists.txt` | **Modify** (Task 1) | Add the three new files to the `WITS` target |
| `qt-app/tests/CMakeLists.txt` | **Modify** (Task 1) | Register the `tst_visitorcontroller` target |
| `qt-app/adminwindow.h` | **Modify** (Task 3) | New members/slots; remove `loadVisitorLogs` declaration |
| `qt-app/adminwindow.cpp` | **Modify** (Task 3) | Wire controller; delete `loadVisitorLogs`; rewrite extract slot |

---

## Task 1: Value types + pure static helpers + test target

`visitordata.h`, the **final** `visitorcontroller.h`, a `visitorcontroller.cpp` whose four public statics (and the private `datePartForFilter`) are implemented test-first, and the `tst_visitorcontroller` CMake target. `fetchVisitors`/`exportToExcel` are declared in the header now (the header does not change again after this task) but get only minimal stub bodies here — their real implementations land in Task 2. The stubs exist so the `.cpp` links in Task 1 and the red phase fails on assertions, not on link errors.

**Files:**
- Create: `qt-app/visitordata.h`
- Create: `qt-app/visitorcontroller.h`
- Create: `qt-app/visitorcontroller.cpp`
- Create: `qt-app/tests/tst_visitorcontroller.cpp`
- Modify: `qt-app/CMakeLists.txt`
- Modify: `qt-app/tests/CMakeLists.txt`
- Test: `ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure`

**Interfaces:**
- Consumes: nothing new — `ApiConfig` and QXlsx are referenced only from Task 2 onward.
- Produces (used by Tasks 2 and 3):
  - `enum class DateFilterType { All, SpecificDay, Month, DateRange };`
  - `struct VisitorRecord { QString name; QString company; QString purpose; QString date; QString time; };`
  - `struct VisitorFilter { QString search; DateFilterType dateType = DateFilterType::All; QString startDate; QString endDate; };`
  - `explicit VisitorController(QNetworkAccessManager *nam, QObject *parent = nullptr);`
  - `void fetchVisitors(const VisitorFilter &filter);` *(stub until Task 2)*
  - `bool exportToExcel(const QList<VisitorRecord> &records, const VisitorFilter &filter, const QString &schoolName, const QString &filePath);` *(stub until Task 2)*
  - `static QString wireDateType(DateFilterType t);`
  - `static QPair<QString, QString> monthRange(int month, int year);`
  - `static QString defaultExportFileName(const VisitorFilter &f);`
  - `static bool parseVisitorsResponse(const QByteArray &json, QList<VisitorRecord> *out, int *totalCount, QString *errorMsg);`
  - Signals: `visitorsLoaded(const QList<VisitorRecord> &records, int totalCount)`, `fetchError(const QString &title, const QString &message)`
  - CMake: `tst_visitorcontroller` test target registered with ctest

- [ ] **Step 1: Create `qt-app/visitordata.h`**

```cpp
#ifndef VISITORDATA_H
#define VISITORDATA_H
#include <QString>

enum class DateFilterType { All, SpecificDay, Month, DateRange };

struct VisitorRecord
{
    QString name;
    QString company;
    QString purpose;
    QString date;   // "yyyy-MM-dd" or "N/A"
    QString time;   // "hh:mm AP"  or "N/A"
};

struct VisitorFilter
{
    QString        search;
    DateFilterType dateType = DateFilterType::All;
    QString        startDate;   // "yyyy-MM-dd", empty for All
    QString        endDate;     // "yyyy-MM-dd", empty for All
};

#endif // VISITORDATA_H
```

A default-constructed `VisitorFilter{}` means "all visitors, no search" — the exact equivalent of today's `loadVisitorLogs("", "all", "", "")` call (adminwindow.cpp:713).

- [ ] **Step 2: Create `qt-app/visitorcontroller.h` (final — never edited again after this task)**

```cpp
#ifndef VISITORCONTROLLER_H
#define VISITORCONTROLLER_H
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include "visitordata.h"

class QNetworkAccessManager;

class VisitorController : public QObject
{
    Q_OBJECT
public:
    explicit VisitorController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Async — result arrives via visitorsLoaded / fetchError.
    void fetchVisitors(const VisitorFilter &filter);

    // Synchronous QXlsx export. Returns xlsx.saveAs(filePath).
    bool exportToExcel(const QList<VisitorRecord> &records,
                       const VisitorFilter &filter,
                       const QString &schoolName,
                       const QString &filePath);

    // Pure, unit-testable statics
    static QString wireDateType(DateFilterType t);
    static QPair<QString, QString> monthRange(int month, int year);
    static QString defaultExportFileName(const VisitorFilter &f);
    static bool parseVisitorsResponse(const QByteArray &json,
                                      QList<VisitorRecord> *out,
                                      int *totalCount,
                                      QString *errorMsg);

signals:
    void visitorsLoaded(const QList<VisitorRecord> &records, int totalCount);
    void fetchError(const QString &title, const QString &message);

private:
    // Shared by defaultExportFileName and exportToExcel so the filename and
    // the "Filter Applied" cell can never drift apart.
    static QString datePartForFilter(const VisitorFilter &f);

    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // VISITORCONTROLLER_H
```

- [ ] **Step 3: Create `qt-app/visitorcontroller.cpp` — red-phase skeleton**

Every body below except the constructor is a deliberate red-phase skeleton. The four statics (and `datePartForFilter`) are replaced with real implementations in **Step 7 of this task**; `fetchVisitors` and `exportToExcel` are replaced in **Task 2**. They exist so the test target links and the new tests fail on assertions, not on link errors.

```cpp
#include "visitorcontroller.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

VisitorController::VisitorController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

void VisitorController::fetchVisitors(const VisitorFilter &filter)
{
    Q_UNUSED(filter);   // Stub — real network fetch lands in Task 2, Step 3.
}

bool VisitorController::exportToExcel(const QList<VisitorRecord> &records,
                                      const VisitorFilter &filter,
                                      const QString &schoolName,
                                      const QString &filePath)
{
    Q_UNUSED(records);
    Q_UNUSED(filter);
    Q_UNUSED(schoolName);
    Q_UNUSED(filePath);
    return false;       // Stub — real QXlsx export lands in Task 2, Step 4.
}

QString VisitorController::wireDateType(DateFilterType t)
{
    Q_UNUSED(t);
    return {};          // Red — implemented in Task 1, Step 7.
}

QPair<QString, QString> VisitorController::monthRange(int month, int year)
{
    Q_UNUSED(month);
    Q_UNUSED(year);
    return {};          // Red — implemented in Task 1, Step 7.
}

QString VisitorController::defaultExportFileName(const VisitorFilter &f)
{
    Q_UNUSED(f);
    return {};          // Red — implemented in Task 1, Step 7.
}

bool VisitorController::parseVisitorsResponse(const QByteArray &json,
                                              QList<VisitorRecord> *out,
                                              int *totalCount,
                                              QString *errorMsg)
{
    Q_UNUSED(json);
    Q_UNUSED(out);
    Q_UNUSED(totalCount);
    Q_UNUSED(errorMsg);
    return false;       // Red — implemented in Task 1, Step 7.
}

QString VisitorController::datePartForFilter(const VisitorFilter &f)
{
    Q_UNUSED(f);
    return {};          // Red — implemented in Task 1, Step 7.
}
```

- [ ] **Step 4: Register the new sources in `qt-app/CMakeLists.txt`**

The `qt_add_executable(WITS ...)` block currently ends with the settings files. Add the visitor files right after them:

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
    )
```

No new Qt modules — `Qt::Network` and `QXlsx` are already linked to `WITS`.

- [ ] **Step 5: Register the test target in `qt-app/tests/CMakeLists.txt`**

Append this block after the existing `tst_responsive_ui` block (the sixth target, following the same pattern the five current targets use — compiling the unit under test directly from `${CMAKE_SOURCE_DIR}`):

```cmake
qt_add_executable(tst_visitorcontroller
    tst_visitorcontroller.cpp
    ${CMAKE_SOURCE_DIR}/visitorcontroller.cpp
    ${CMAKE_SOURCE_DIR}/visitorcontroller.h
    ${CMAKE_SOURCE_DIR}/visitordata.h
)
set_target_properties(tst_visitorcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_visitorcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_visitorcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    QXlsx
)
add_test(NAME tst_visitorcontroller COMMAND tst_visitorcontroller)
set_tests_properties(tst_visitorcontroller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

`QT_QPA_PLATFORM=offscreen` is required, not optional: QXlsx links `Qt::Gui` and propagates it, so `QTEST_MAIN` instantiates a `QGuiApplication`, which needs a platform plugin on a headless runner (same reason `tst_rfidkeyboardfilter`, `tst_theme`, and `tst_responsive_ui` set it). Do **not** "optimize" this target down to Core-only — the `exportToExcel` test in Task 2 needs the `QGuiApplication` that the Gui link provides for `QXlsx::Format` font/color handling. The `QXlsx` target is available because the parent `CMakeLists.txt` runs `add_subdirectory(libs/QXlsx)` before `add_subdirectory(tests)`.

- [ ] **Step 6: Write the failing tests — create `qt-app/tests/tst_visitorcontroller.cpp`**

All payloads are synthetic (no real visitor PII). `QTEST_MAIN` (not `QTEST_APPLESS_MAIN`) because the target links `Qt::Gui` via QXlsx, so `QTEST_MAIN` expands to a `QGuiApplication` — required for Task 2's export test; harmless for the statics.

```cpp
#include <QtTest>
#include <QByteArray>
#include <QList>
#include <QPair>
#include "visitorcontroller.h"
#include "visitordata.h"

class TestVisitorController : public QObject
{
    Q_OBJECT
private slots:
    // parseVisitorsResponse
    void parseSuccessPayload();
    void parseCountPropagation();
    void parseInvalidJson();
    void parseStatusNotSuccess();
    void parseMalformedTimeIn();
    void parseMissingFields();

    // wireDateType
    void wireDateTypeAllVariants();

    // monthRange
    void monthRangeJune2026();
    void monthRangeFebruaryLeapYear();
    void monthRangeDecember();

    // defaultExportFileName
    void fileNameAllNoSearch();
    void fileNameSpecificDay();
    void fileNameMonth();
    void fileNameDateRange();
    void fileNameWithSearchSpacesToUnderscores();
    void fileNameSearchWithoutSpaces();
};

void TestVisitorController::parseSuccessPayload()
{
    const QByteArray json = R"({
        "status": "success",
        "count": 2,
        "visitors": [
            {"name": "Juan Dela Cruz", "company": "Acme Corp",
             "purpose": "Research", "time_in": "2026-06-15 14:30:00"},
            {"name": "Maria Clara", "company": "Globex Inc",
             "purpose": "Book Donation", "time_in": "2026-06-16 09:15:00"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 2);
    QCOMPARE(records[0].name,    QString("Juan Dela Cruz"));
    QCOMPARE(records[0].company, QString("Acme Corp"));
    QCOMPARE(records[0].purpose, QString("Research"));
    QCOMPARE(records[0].date,    QString("2026-06-15"));
    QCOMPARE(records[0].time,    QString("02:30 PM"));
    QCOMPARE(records[1].date,    QString("2026-06-16"));
    QCOMPARE(records[1].time,    QString("09:15 AM"));
    QCOMPARE(totalCount, 2);
}

void TestVisitorController::parseCountPropagation()
{
    // The server "count" field is propagated even when it differs from the
    // number of returned rows (e.g. a capped result set).
    const QByteArray json = R"({
        "status": "success",
        "count": 25,
        "visitors": [
            {"name": "Juan Dela Cruz", "company": "Acme Corp",
             "purpose": "Research", "time_in": "2026-06-15 14:30:00"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 1);
    QCOMPARE(totalCount, 25);
}

void TestVisitorController::parseInvalidJson()
{
    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(!VisitorController::parseVisitorsResponse(QByteArray("<html>502 Bad Gateway</html>"),
                                                      &records, &totalCount, &errorMsg));
    QCOMPARE(errorMsg, QString("Invalid server response."));
}

void TestVisitorController::parseStatusNotSuccess()
{
    const QByteArray json = R"({"status": "error", "message": "No database connection."})";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(!VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(errorMsg, QString("No database connection."));
}

void TestVisitorController::parseMalformedTimeIn()
{
    const QByteArray json = R"({
        "status": "success",
        "count": 1,
        "visitors": [
            {"name": "Juan Dela Cruz", "company": "Acme Corp",
             "purpose": "Research", "time_in": "15/06/2026 at 2pm"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].date, QString("N/A"));
    QCOMPARE(records[0].time, QString("N/A"));
}

void TestVisitorController::parseMissingFields()
{
    const QByteArray json = R"({
        "status": "success",
        "count": 1,
        "visitors": [
            {"time_in": "2026-06-15 08:00:00"}
        ]
    })";

    QList<VisitorRecord> records;
    int totalCount = 0;
    QString errorMsg;
    QVERIFY(VisitorController::parseVisitorsResponse(json, &records, &totalCount, &errorMsg));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].name,    QString(""));
    QCOMPARE(records[0].company, QString(""));
    QCOMPARE(records[0].purpose, QString(""));
    QCOMPARE(records[0].date,    QString("2026-06-15"));
    QCOMPARE(records[0].time,    QString("08:00 AM"));
}

void TestVisitorController::wireDateTypeAllVariants()
{
    QCOMPARE(VisitorController::wireDateType(DateFilterType::All),         QString("all"));
    QCOMPARE(VisitorController::wireDateType(DateFilterType::SpecificDay), QString("Specific Day"));
    QCOMPARE(VisitorController::wireDateType(DateFilterType::Month),       QString("Month"));
    QCOMPARE(VisitorController::wireDateType(DateFilterType::DateRange),   QString("Date Range"));
}

void TestVisitorController::monthRangeJune2026()
{
    const QPair<QString, QString> range = VisitorController::monthRange(6, 2026);
    QCOMPARE(range.first,  QString("2026-06-01"));
    QCOMPARE(range.second, QString("2026-06-30"));
}

void TestVisitorController::monthRangeFebruaryLeapYear()
{
    const QPair<QString, QString> range = VisitorController::monthRange(2, 2024);
    QCOMPARE(range.first,  QString("2024-02-01"));
    QCOMPARE(range.second, QString("2024-02-29"));
}

void TestVisitorController::monthRangeDecember()
{
    const QPair<QString, QString> range = VisitorController::monthRange(12, 2025);
    QCOMPARE(range.first,  QString("2025-12-01"));
    QCOMPARE(range.second, QString("2025-12-31"));
}

void TestVisitorController::fileNameAllNoSearch()
{
    QCOMPARE(VisitorController::defaultExportFileName(VisitorFilter{}),
             QString("VisitorLogs_All.xlsx"));
}

void TestVisitorController::fileNameSpecificDay()
{
    VisitorFilter f;
    f.dateType  = DateFilterType::SpecificDay;
    f.startDate = "2026-06-15";
    f.endDate   = "2026-06-15";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_2026-06-15.xlsx"));
}

void TestVisitorController::fileNameMonth()
{
    VisitorFilter f;
    f.dateType  = DateFilterType::Month;
    f.startDate = "2026-01-01";
    f.endDate   = "2026-01-31";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_January_2026.xlsx"));
}

void TestVisitorController::fileNameDateRange()
{
    VisitorFilter f;
    f.dateType  = DateFilterType::DateRange;
    f.startDate = "2026-06-01";
    f.endDate   = "2026-06-15";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_Range_2026-06-01_to_2026-06-15.xlsx"));
}

void TestVisitorController::fileNameWithSearchSpacesToUnderscores()
{
    VisitorFilter f;
    f.search = "Juan Dela Cruz";   // dateType stays All
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_Juan_Dela_Cruz_All.xlsx"));
}

void TestVisitorController::fileNameSearchWithoutSpaces()
{
    VisitorFilter f;
    f.search    = "Acme";
    f.dateType  = DateFilterType::Month;
    f.startDate = "2026-02-01";
    f.endDate   = "2026-02-28";
    QCOMPARE(VisitorController::defaultExportFileName(f),
             QString("VisitorLogs_Acme_February_2026.xlsx"));
}

QTEST_MAIN(TestVisitorController)
#include "tst_visitorcontroller.moc"
```

Build and run — this is the **red** step (the build regenerates CMake automatically on the `CMakeLists.txt` changes):

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure
```

Expected: the target compiles and links, and `tst_visitorcontroller` **FAILS** — every slot except none passes (all 16 slots assert against the skeleton `return {};` / `return false;` bodies). If the failure is a compile or link error instead, fix that first; the red phase must fail on `QCOMPARE`/`QVERIFY`.

- [ ] **Step 7: Implement the statics (green) — replace the five red skeletons in `qt-app/visitorcontroller.cpp`**

Leave the constructor and the `fetchVisitors`/`exportToExcel` stubs untouched. Replace the bodies of `wireDateType`, `monthRange`, `defaultExportFileName`, `parseVisitorsResponse`, and `datePartForFilter` with:

```cpp
QString VisitorController::wireDateType(DateFilterType t)
{
    // Legacy wire strings, verbatim — the combo box display text used to be
    // sent as-is (adminwindow.cpp:681, 700-702). Do not "normalize" these.
    switch (t) {
    case DateFilterType::SpecificDay: return QStringLiteral("Specific Day");
    case DateFilterType::Month:       return QStringLiteral("Month");
    case DateFilterType::DateRange:   return QStringLiteral("Date Range");
    case DateFilterType::All:         break;
    }
    return QStringLiteral("all");
}

QPair<QString, QString> VisitorController::monthRange(int month, int year)
{
    const QDate firstDay(year, month, 1);
    const QDate lastDay(year, month, firstDay.daysInMonth());
    return { firstDay.toString(QStringLiteral("yyyy-MM-dd")),
             lastDay.toString(QStringLiteral("yyyy-MM-dd")) };
}

QString VisitorController::defaultExportFileName(const VisitorFilter &f)
{
    QString baseName = QStringLiteral("VisitorLogs");
    if (!f.search.isEmpty()) {
        QString searchTerm = f.search;   // already trimmed by the View
        searchTerm.replace(QLatin1Char(' '), QLatin1Char('_'));
        baseName += QLatin1Char('_') + searchTerm;
    }
    return QStringLiteral("%1_%2.xlsx").arg(baseName, datePartForFilter(f));
}

bool VisitorController::parseVisitorsResponse(const QByteArray &json,
                                              QList<VisitorRecord> *out,
                                              int *totalCount,
                                              QString *errorMsg)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        *errorMsg = QStringLiteral("Invalid server response.");
        return false;
    }

    const QJsonObject obj = doc.object();
    if (obj[QLatin1String("status")].toString() != QLatin1String("success")) {
        *errorMsg = obj[QLatin1String("message")].toString();
        return false;
    }

    const QJsonArray logs = obj[QLatin1String("visitors")].toArray();
    *totalCount = obj[QLatin1String("count")].toInt();

    out->clear();
    out->reserve(logs.size());
    for (const QJsonValue &value : logs) {
        const QJsonObject log = value.toObject();

        VisitorRecord rec;
        rec.name    = log[QLatin1String("name")].toString();
        rec.company = log[QLatin1String("company")].toString();
        rec.purpose = log[QLatin1String("purpose")].toString();

        const QString timeIn = log[QLatin1String("time_in")].toString();
        const QDateTime dt =
            QDateTime::fromString(timeIn, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        rec.date = dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd"))
                                : QStringLiteral("N/A");
        rec.time = dt.isValid() ? dt.toString(QStringLiteral("hh:mm AP"))
                                : QStringLiteral("N/A");

        out->append(rec);
    }
    return true;
}

QString VisitorController::datePartForFilter(const VisitorFilter &f)
{
    switch (f.dateType) {
    case DateFilterType::SpecificDay:
        return f.startDate;
    case DateFilterType::Month: {
        // In Month mode startDate is always "yyyy-MM-01". QLocale::c()
        // guarantees the same English month names the combo box hardcodes
        // (adminwindow.cpp:643-644), regardless of the system locale.
        const QDate d = QDate::fromString(f.startDate, QStringLiteral("yyyy-MM-dd"));
        return QStringLiteral("%1_%2")
            .arg(QLocale::c().toString(d, QStringLiteral("MMMM")),
                 QString::number(d.year()));
    }
    case DateFilterType::DateRange:
        return QStringLiteral("Range_%1_to_%2").arg(f.startDate, f.endDate);
    case DateFilterType::All:
        break;
    }
    return QStringLiteral("All");
}
```

- [ ] **Step 8: Build and run the new target — expect green**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure
```

Expected output ends with `100% tests passed, 0 tests failed out of 1` and the Qt Test summary reports 16 passed slots (plus `initTestCase`/`cleanupTestCase`), 0 failed.

- [ ] **Step 9: Run the full suite to confirm nothing else broke**

```powershell
ctest --test-dir qt-app/build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 6` (the 5 existing targets plus `tst_visitorcontroller`).

- [ ] **Step 10: Commit via the `commit` skill**

Invoke the `commit` skill (never raw `git add`/`git commit`). Proposed commit:

```
feat(visitor): add VisitorController value types and pure helpers

Introduce visitordata.h (DateFilterType, VisitorRecord, VisitorFilter)
and VisitorController with its four unit-tested statics: wireDateType
(legacy wire strings preserved verbatim), monthRange,
defaultExportFileName, and parseVisitorsResponse. fetchVisitors and
exportToExcel are declared but stubbed; they land in the next commit.
Registers the tst_visitorcontroller Qt Test target (16 cases, all
synthetic data, no live network).
```

---

## Task 2: fetchVisitors + exportToExcel (controller completion + export test)

Replace the two Task 1 stubs in `qt-app/visitorcontroller.cpp` with the real network fetch and the real QXlsx export, driving the export with a new failing round-trip test first. The header does not change.

**Files:**
- Modify: `qt-app/visitorcontroller.cpp` (replace the two stub bodies; extend the include list)
- Modify: `qt-app/tests/tst_visitorcontroller.cpp` (add two export test slots)
- Test: `ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure`

**Interfaces:**
- Consumes (from Task 1): the final `visitorcontroller.h` declarations — `fetchVisitors(const VisitorFilter &)`, `exportToExcel(const QList<VisitorRecord> &, const VisitorFilter &, const QString &, const QString &)`, `wireDateType`, `datePartForFilter`, the signals `visitorsLoaded(const QList<VisitorRecord> &, int)` / `fetchError(const QString &, const QString &)`, and `m_nam`. Also consumes `ApiConfig::endpoint(const QString &)` (existing, `qt-app/apiconfig.h`) and QXlsx (`xlsxdocument.h`, `xlsxformat.h`, already linked).
- Produces (used by Task 3): working `fetchVisitors` (emits `visitorsLoaded`/`fetchError`) and working `exportToExcel` (returns `xlsx.saveAs(filePath)`).

**Note on `fetchVisitors` testing:** `fetchVisitors` is network-bound and gets **no live-network test** — the project workflow rule forbids hitting the real backend from unit tests, and a mock-server harness is out of scope for a behavior-preserving extraction. Its parse logic is already fully covered by the Task 1 `parseVisitorsResponse` cases; the request/emit plumbing is verified manually in Task 3 against the running app (including the backend-down "Network Error" path). State this in the commit body rather than inventing a mock-heavy test.

- [ ] **Step 1: Write the failing export tests (red) — extend `qt-app/tests/tst_visitorcontroller.cpp`**

Add two includes at the top, next to the existing ones:

```cpp
#include <QTemporaryDir>
#include "xlsxdocument.h"
```

Add two slot declarations at the end of the `private slots:` section (after `fileNameSearchWithoutSpaces();`):

```cpp
    // exportToExcel
    void exportToExcelWritesLayout();
    void exportToExcelSchoolNameFallback();
```

Add the two test bodies before `QTEST_MAIN(TestVisitorController)`:

```cpp
void TestVisitorController::exportToExcelWritesLayout()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("visitor_export_test.xlsx"));

    QList<VisitorRecord> records;
    records.append({QStringLiteral("Juan Dela Cruz"), QStringLiteral("Acme Corp"),
                    QStringLiteral("Research"), QStringLiteral("2026-06-15"),
                    QStringLiteral("02:30 PM")});
    records.append({QStringLiteral("Maria Clara"), QStringLiteral("Globex Inc"),
                    QStringLiteral("Book Donation"), QStringLiteral("2026-06-16"),
                    QStringLiteral("09:15 AM")});

    VisitorFilter filter;
    filter.dateType  = DateFilterType::Month;
    filter.startDate = QStringLiteral("2026-06-01");
    filter.endDate   = QStringLiteral("2026-06-30");
    filter.search    = QStringLiteral("Juan_Santos");   // underscore exercises the legacy quirk

    VisitorController controller(nullptr);   // exportToExcel never touches the manager
    QVERIFY(controller.exportToExcel(records, filter,
                                     QStringLiteral("Sample University"), filePath));

    QXlsx::Document readBack(filePath);
    QVERIFY(readBack.isLoadPackage());

    QCOMPARE(readBack.read("A1").toString(), QString("Sample University"));
    QCOMPARE(readBack.read("A2").toString(), QString("Library Visitor Logs Report"));
    QCOMPARE(readBack.read("A4").toString(), QString("Generated On:"));
    QCOMPARE(readBack.read("A5").toString(), QString("Filter Applied:"));
    QCOMPARE(readBack.read("B5").toString(), QString("June_2026"));     // underscore kept
    QCOMPARE(readBack.read("A6").toString(), QString("Search Keyword:"));
    QCOMPARE(readBack.read("B6").toString(), QString("Juan Santos"));   // underscore→space quirk

    QCOMPARE(readBack.read(8, 1).toString(), QString("Name"));
    QCOMPARE(readBack.read(8, 2).toString(), QString("Company"));
    QCOMPARE(readBack.read(8, 3).toString(), QString("Purpose"));
    QCOMPARE(readBack.read(8, 4).toString(), QString("Date"));
    QCOMPARE(readBack.read(8, 5).toString(), QString("Time"));

    QCOMPARE(readBack.read(9, 1).toString(),  QString("Juan Dela Cruz"));
    QCOMPARE(readBack.read(9, 5).toString(),  QString("02:30 PM"));
    QCOMPARE(readBack.read(10, 4).toString(), QString("2026-06-16"));

    const int totalRow = 8 + records.size() + 2;   // startRow + rows + 2 = 12
    QCOMPARE(readBack.read(totalRow, 1).toString(), QString("Total Visitors:"));
    QCOMPARE(readBack.read(totalRow, 2).toInt(), 2);
}

void TestVisitorController::exportToExcelSchoolNameFallback()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("visitor_export_fallback.xlsx"));

    QList<VisitorRecord> records;
    records.append({QStringLiteral("Juan Dela Cruz"), QStringLiteral("Acme Corp"),
                    QStringLiteral("Research"), QStringLiteral("2026-06-15"),
                    QStringLiteral("02:30 PM")});

    VisitorController controller(nullptr);
    QVERIFY(controller.exportToExcel(records, VisitorFilter{}, QString(), filePath));

    QXlsx::Document readBack(filePath);
    QVERIFY(readBack.isLoadPackage());
    QCOMPARE(readBack.read("A1").toString(), QString("School Name"));   // empty-name fallback
    QVERIFY(readBack.read("A5").toString().isEmpty());   // All filter → no "Filter Applied:" row
    QVERIFY(readBack.read("A6").toString().isEmpty());   // no search → no "Search Keyword:" row
}
```

Build and run — red:

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure
```

Expected: the 16 Task 1 slots still pass; `exportToExcelWritesLayout` and `exportToExcelSchoolNameFallback` **FAIL** on the first `QVERIFY(controller.exportToExcel(...))` because the stub returns `false`.

- [ ] **Step 2: Extend the include list in `qt-app/visitorcontroller.cpp`**

Replace the current include block:

```cpp
#include "visitorcontroller.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
```

with:

```cpp
#include "visitorcontroller.h"
#include "apiconfig.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>

#include "xlsxdocument.h"
#include "xlsxformat.h"
```

(`xlsxdocument.h`/`xlsxformat.h` are the same include form `adminwindow.cpp` already uses, lines 52–53.)

- [ ] **Step 3: Replace the `fetchVisitors` stub with the real implementation**

The payload keys, header, and endpoint are verbatim from the current `loadVisitorLogs` (adminwindow.cpp:3680–3690). The spec is explicit about the connect call: **"The `finished` connection MUST pass the controller as the 3rd-arg context object: `connect(reply, &QNetworkReply::finished, this, [this, reply]{...})` where `this` is the `VisitorController`."** Today's code gets lifetime safety from `this` = `adminWindow`; a reply parented to the *shared* `QNetworkAccessManager` can outlive the controller during `~adminWindow`, and a context-less lambda would fire after controller destruction and dereference a dangling pointer. The context arg preserves the auto-disconnect. Overlapping in-flight fetches (rapid Search clicks) are preserved as-is — each call creates an independent reply and last-to-finish wins, exactly as today; no cancellation is added.

```cpp
void VisitorController::fetchVisitors(const VisitorFilter &filter)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("get_visitors.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonObject payload;
    payload[QLatin1String("search")]     = filter.search;
    payload[QLatin1String("date_type")]  = wireDateType(filter.dateType);
    payload[QLatin1String("start_date")] = filter.startDate;
    payload[QLatin1String("end_date")]   = filter.endDate;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());

    // `this` (the controller) as the context object is mandatory: the
    // connection auto-disconnects if the controller is destroyed while the
    // reply — owned by the shared QNetworkAccessManager — is still in flight.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(QStringLiteral("Network Error"), reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QList<VisitorRecord> records;
        int totalCount = 0;
        QString errorMsg;
        if (!parseVisitorsResponse(resp, &records, &totalCount, &errorMsg)) {
            emit fetchError(QStringLiteral("Error"), errorMsg);
            return;
        }

        emit visitorsLoaded(records, totalCount);
    });
}
```

`reply->deleteLater()` is called on every path (matching adminwindow.cpp:3695, 3700). The two error titles map today's three failure paths exactly: network error → `"Network Error"` (shown as `QMessageBox::critical` by the View), invalid JSON and `status != "success"` → `"Error"` (shown as `QMessageBox::warning`).

- [ ] **Step 4: Replace the `exportToExcel` stub with the real implementation**

Behavior-verbatim from adminwindow.cpp:3810–3875 — same formats, same cells, same conditionals, same widths, same total-row math. The only translation: cell values come from `records`/`filter` instead of `ui->visitorTable` and the filter widgets.

```cpp
bool VisitorController::exportToExcel(const QList<VisitorRecord> &records,
                                      const VisitorFilter &filter,
                                      const QString &schoolName,
                                      const QString &filePath)
{
    QXlsx::Document xlsx;

    QXlsx::Format boldCenter;
    boldCenter.setFontBold(true);
    boldCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setPatternBackgroundColor(Qt::lightGray);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    QXlsx::Format normalCenter;
    normalCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    normalCenter.setBorderStyle(QXlsx::Format::BorderThin);

    // The View passes ui->schoolName->text() unmodified; the fallback lives here.
    const QString effectiveSchoolName =
        schoolName.isEmpty() ? QStringLiteral("School Name") : schoolName;
    const QString reportTitle   = QStringLiteral("Library Visitor Logs Report");
    const QString generatedDate =
        QDateTime::currentDateTime().toString(QStringLiteral("MMMM dd, yyyy hh:mm AP"));
    const QString datePart = datePartForFilter(filter);

    xlsx.mergeCells("A1:E1");
    xlsx.mergeCells("A2:E2");
    xlsx.write("A1", effectiveSchoolName, boldCenter);
    xlsx.write("A2", reportTitle, boldCenter);

    xlsx.write("A4", "Generated On:", boldCenter);
    xlsx.write("B4", generatedDate, normalCenter);

    // Written only when datePart != "All"; the Month datePart keeps its
    // underscore ("January_2026"), exactly as today (adminwindow.cpp:3842-3844).
    if (datePart != QLatin1String("All")) {
        xlsx.write("A5", "Filter Applied:", boldCenter);
        xlsx.write("B5", datePart, normalCenter);
    }

    // Written only when a search term exists. The underscore→space replace
    // preserves the pre-existing quirk (adminwindow.cpp:3846-3848): the old
    // code space→underscored the term for the filename and underscored→spaced
    // it back for this cell, which also turned any ORIGINAL underscores into
    // spaces.
    if (!filter.search.isEmpty()) {
        QString keyword = filter.search;
        keyword.replace(QLatin1Char('_'), QLatin1Char(' '));
        xlsx.write("A6", "Search Keyword:", boldCenter);
        xlsx.write("B6", keyword, normalCenter);
    }

    const int startRow = 8;
    const QStringList headers = {"Name", "Company", "Purpose", "Date", "Time"};
    for (int col = 0; col < headers.size(); ++col)
        xlsx.write(startRow, col + 1, headers.at(col), headerFormat);

    for (int row = 0; row < records.size(); ++row) {
        const VisitorRecord &rec = records.at(row);
        xlsx.write(startRow + row + 1, 1, rec.name,    normalCenter);
        xlsx.write(startRow + row + 1, 2, rec.company, normalCenter);
        xlsx.write(startRow + row + 1, 3, rec.purpose, normalCenter);
        xlsx.write(startRow + row + 1, 4, rec.date,    normalCenter);
        xlsx.write(startRow + row + 1, 5, rec.time,    normalCenter);
    }

    for (int col = 1; col <= 5; ++col)
        xlsx.setColumnWidth(col, 25);

    // Row count, not the server "count" field — matching adminwindow.cpp:3873.
    const int lastRow = startRow + records.size() + 2;
    xlsx.write(lastRow, 1, "Total Visitors:", boldCenter);
    xlsx.write(lastRow, 2, records.size(), normalCenter);

    return xlsx.saveAs(filePath);
}
```

No dialogs, no message boxes, no `ui->` — the View handles all of that.

- [ ] **Step 5: Build and run the new target — expect green**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_visitorcontroller --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 1`; the Qt Test summary reports 18 passed slots, 0 failed.

- [ ] **Step 6: Run the full suite**

```powershell
ctest --test-dir qt-app/build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 6`.

- [ ] **Step 7: Commit via the `commit` skill**

Proposed commit:

```
feat(visitor): implement visitor fetch and Excel export in controller

fetchVisitors POSTs the legacy {search, date_type, start_date,
end_date} payload to get_visitors.php via the injected shared
QNetworkAccessManager, with the controller as the connect context
object so an in-flight reply can never fire into a destroyed
controller. exportToExcel reproduces the report layout verbatim
(merged title rows, conditional filter/search rows, headers at row 8,
column width 25, row-count total) and is covered by a QTemporaryDir
round-trip test. fetchVisitors has no live-network test by project
rule; its parse logic is covered by the parseVisitorsResponse cases.
```

---

## Task 3: Wire VisitorController into adminWindow (View-only remainder)

Make `adminWindow` a pure View for the Visitor Logs tab: construct the controller, connect its signals, collapse the search lambda and sidebar auto-load to `fetchVisitors`, delete `loadVisitorLogs`, and rewrite `on_extractVisitorBtn_clicked` as a thin dialog wrapper.

**Files:**
- Modify: `qt-app/adminwindow.h`
- Modify: `qt-app/adminwindow.cpp`
- Test: full build + `ctest --test-dir qt-app/build --output-on-failure` + grep checks + manual app smoke test

**Interfaces:**
- Consumes (from Tasks 1–2): `VisitorController(QNetworkAccessManager *, QObject *)`, `fetchVisitors(const VisitorFilter &)`, `exportToExcel(const QList<VisitorRecord> &, const VisitorFilter &, const QString &, const QString &)`, `VisitorController::monthRange(int, int)`, `VisitorController::defaultExportFileName(const VisitorFilter &)`, signals `visitorsLoaded(const QList<VisitorRecord> &, int)` / `fetchError(const QString &, const QString &)`, types `VisitorFilter`, `VisitorRecord`, `DateFilterType`.
- Produces: `adminWindow::collectVisitorFilter() const → VisitorFilter`, slots `populateVisitorTable(const QList<VisitorRecord> &, int)` and `onVisitorFetchError(const QString &, const QString &)`, members `m_visitorController` / `m_visitorRecords`.

- [ ] **Step 1: Update `qt-app/adminwindow.h`**

Add the two includes directly after the existing controller includes (lines 39–40):

```cpp
#include "settingsdata.h"
#include "settingscontroller.h"
```

becomes:

```cpp
#include "settingsdata.h"
#include "settingscontroller.h"
#include "visitordata.h"
#include "visitorcontroller.h"
```

In the `private:` section, directly after the settings block (currently lines 88–93):

```cpp
    SettingsController* const m_settingsController;
    SettingsData              m_currentSettings;

    void bindSettingsSignals();
    void populateSettingsForm(const SettingsData &data);
    SettingsData collectSettingsForm();
```

add:

```cpp
    VisitorController *m_visitorController;   // child of this, created in ctor
    QList<VisitorRecord> m_visitorRecords;    // cache of the last visitorsLoaded payload

    VisitorFilter collectVisitorFilter() const;
```

In the `private slots:` section, after `void onSettingsImportError(const QString &message);`, add:

```cpp
    void populateVisitorTable(const QList<VisitorRecord> &records, int totalCount);
    void onVisitorFetchError(const QString &title, const QString &message);
```

**Delete** the `loadVisitorLogs` declaration (currently line 147):

```cpp
    void loadVisitorLogs(const QString &search, const QString &dateType, const QString &startDate, const QString &endDate);   // ← DELETE THIS LINE
```

- [ ] **Step 2: Construct and wire the controller in the constructor (`qt-app/adminwindow.cpp`)**

Locate the `networkManager` initialization (line 424):

```cpp
    networkManager = new QNetworkAccessManager(this);
```

Directly after it (the controller borrows `networkManager`, so it must be created after that line), add:

```cpp
    m_visitorController = new VisitorController(networkManager, this);
    connect(m_visitorController, &VisitorController::visitorsLoaded,
            this, &adminWindow::populateVisitorTable);
    connect(m_visitorController, &VisitorController::fetchError,
            this, &adminWindow::onVisitorFetchError);
```

Same-thread direct connections — no `Q_DECLARE_METATYPE` / `qRegisterMetaType` needed for `QList<VisitorRecord>`.

- [ ] **Step 3: Collapse the search-button lambda**

The widget-visibility toggle lambda on `ui->visitorDateFilterBox` (lines 658–677) **stays untouched** — it is pure widget choreography. Replace only the search-button lambda. Current code (lines 679–708):

```cpp
    connect(ui->visitorSearchBtn, &QPushButton::clicked, this, [=]() {
        QString search = ui->visitorSearchLineEdit->text().trimmed();
        QString dateType = ui->visitorDateFilterBox->currentText();
        QString startDate, endDate;

        if (dateType == "Specific Day") {
            startDate = ui->visitorDateEdit->date().toString("yyyy-MM-dd");
            endDate = startDate;
        }
        else if (dateType == "Month") {
            int month = ui->visitorMonthCombo->currentIndex() + 1; // assuming 0-based
            int year = ui->visitorYearSpin->value();
            QDate firstDay(year, month, 1);
            QDate lastDay(year, month, firstDay.daysInMonth());
            startDate = firstDay.toString("yyyy-MM-dd");
            endDate = lastDay.toString("yyyy-MM-dd");
        }
        else if (dateType == "Date Range") {
            startDate = ui->visitorStartDate->date().toString("yyyy-MM-dd");
            endDate = ui->visitorEndDate->date().toString("yyyy-MM-dd");
        }
        else {
            // Default fallback: show all
            dateType = "all";
            startDate = "";
            endDate = "";
        }

        loadVisitorLogs(search, dateType, startDate, endDate);
    });
```

Replacement:

```cpp
    connect(ui->visitorSearchBtn, &QPushButton::clicked, this, [=]() {
        m_visitorController->fetchVisitors(collectVisitorFilter());
    });
```

- [ ] **Step 4: Replace the sidebar auto-load call**

Current code (lines 710–714):

```cpp
    connect(ui->visitorBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->visitorPage);
        setActiveSidebar(ui->visitorBtn);
        loadVisitorLogs("", "all", "", ""); // Auto-load all logs on open
    });
```

Replacement (a default `VisitorFilter{}` is the exact equivalent of `("", "all", "", "")`):

```cpp
    connect(ui->visitorBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->visitorPage);
        setActiveSidebar(ui->visitorBtn);
        m_visitorController->fetchVisitors(VisitorFilter{}); // Auto-load all logs on open
    });
```

- [ ] **Step 5: Delete `loadVisitorLogs` and add the three new View methods in its place**

Delete the entire `loadVisitorLogs` definition (lines 3677–3759 — from `void adminWindow::loadVisitorLogs(const QString &search, const QString &dateType,` down to its closing `}` just before `void adminWindow::on_extractVisitorBtn_clicked() {`). In its place, add:

```cpp
VisitorFilter adminWindow::collectVisitorFilter() const
{
    VisitorFilter f;
    f.search = ui->visitorSearchLineEdit->text().trimmed();

    const QString dateType = ui->visitorDateFilterBox->currentText();
    if (dateType == "Specific Day") {
        f.dateType  = DateFilterType::SpecificDay;
        f.startDate = ui->visitorDateEdit->date().toString("yyyy-MM-dd");
        f.endDate   = f.startDate;
    }
    else if (dateType == "Month") {
        f.dateType = DateFilterType::Month;
        const QPair<QString, QString> range =
            VisitorController::monthRange(ui->visitorMonthCombo->currentIndex() + 1,
                                          ui->visitorYearSpin->value());
        f.startDate = range.first;
        f.endDate   = range.second;
    }
    else if (dateType == "Date Range") {
        f.dateType  = DateFilterType::DateRange;
        f.startDate = ui->visitorStartDate->date().toString("yyyy-MM-dd");
        f.endDate   = ui->visitorEndDate->date().toString("yyyy-MM-dd");
    }
    else {
        // Default fallback: show all — dates stay empty
        f.dateType = DateFilterType::All;
    }
    return f;
}

void adminWindow::populateVisitorTable(const QList<VisitorRecord> &records, int totalCount)
{
    m_visitorRecords = records;

    ui->visitorTable->setRowCount(records.size());
    ui->visitorTable->setColumnCount(5);
    QStringList headers = {"Name", "Company", "Purpose", "Date", "Time"};
    ui->visitorTable->setHorizontalHeaderLabels(headers);

    for (int i = 0; i < records.size(); ++i) {
        const VisitorRecord &rec = records.at(i);
        ui->visitorTable->setItem(i, 0, new QTableWidgetItem(rec.name));
        ui->visitorTable->setItem(i, 1, new QTableWidgetItem(rec.company));
        ui->visitorTable->setItem(i, 2, new QTableWidgetItem(rec.purpose));
        ui->visitorTable->setItem(i, 3, new QTableWidgetItem(rec.date));
        ui->visitorTable->setItem(i, 4, new QTableWidgetItem(rec.time));
    }

    // Per-load table re-setup — redundant on repeat loads but kept exactly as
    // today to stay behavior-preserving.
    ui->visitorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->visitorTable->setAlternatingRowColors(true);
    ui->visitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->visitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->visitorTable->setSelectionMode(QAbstractItemView::SingleSelection);

    if (ui->visitorTotalLabel) {
        ui->visitorTotalLabel->setText(QString("📊 Total Visitors: %1").arg(totalCount));
    }

    if (records.isEmpty()) {
        ui->visitorTable->setRowCount(0);
        showTemporaryOverlay(ui->visitorTable, "No visitors found for the selected filters.");
        if (ui->visitorTotalLabel)
            ui->visitorTotalLabel->setText("📊 Total Visitors: 0");
        return;
    }
}

void adminWindow::onVisitorFetchError(const QString &title, const QString &message)
{
    // Reproduces today's three cases exactly (old adminwindow.cpp:3694, 3704,
    // 3710): network error → critical, everything else → warning.
    if (title == QLatin1String("Network Error"))
        QMessageBox::critical(this, title, message);
    else
        QMessageBox::warning(this, title, message);
}
```

The `"📊 Total Visitors: %1"` emoji label, the `"No visitors found for the selected filters."` overlay text, the empty-result label force to `"📊 Total Visitors: 0"`, and the date/time strings (already formatted by the controller — `"yyyy-MM-dd"` / `"hh:mm AP"` / `"N/A"`) are preserved exactly.

- [ ] **Step 6: Rewrite `on_extractVisitorBtn_clicked`**

Replace the entire current body (lines 3761–3881 — the guard on `ui->visitorTable->rowCount()`, the filename-derivation block reading the filter widgets, and all the QXlsx code) with:

```cpp
void adminWindow::on_extractVisitorBtn_clicked() {
    // Guard moves from visitorTable->rowCount() == 0 to the cache —
    // equivalent, since the table always mirrors the cache.
    if (m_visitorRecords.isEmpty()) {
        QMessageBox::information(this, "No Data", "There are no visitor logs to export.");
        return;
    }

    // Read at export time, matching today's behavior of deriving the
    // filename/filter rows from the CURRENT widget state even if the user
    // changed filters without re-searching.
    const VisitorFilter filter = collectVisitorFilter();

    // --- Ask user where to save ---
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Visitor Logs",
        QDir::homePath() + "/" + VisitorController::defaultExportFileName(filter),
        "Excel Files (*.xlsx)"
        );
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".xlsx"))
        filePath += ".xlsx";

    if (m_visitorController->exportToExcel(m_visitorRecords, filter,
                                           ui->schoolName->text(), filePath)) {
        QMessageBox::information(this, "Export Successful",
                                 QString("Visitor logs exported successfully!\n\nSaved as:\n%1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Error", "Failed to save the Excel file.");
    }
}
```

All four message texts (`"No Data"` / `"There are no visitor logs to export."`, `"Export Visitor Logs"`, `"Export Successful"` / `"Visitor logs exported successfully!\n\nSaved as:\n%1"`, `"Error"` / `"Failed to save the Excel file."`) are verbatim from the current code (old lines 3763, 3800, 3876–3879). `QFileDialog` stays in the View; `ui->schoolName->text()` is passed unmodified (the `"School Name"` fallback lives in the controller).

- [ ] **Step 7: Build and run all tests**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```

Expected: build completes with **no new warnings or errors**, and `100% tests passed, 0 tests failed out of 6`.

- [ ] **Step 8: Grep checks (layer purity)**

Run these from the repo root (Git Bash) and confirm the expected output for each:

```bash
grep -rn "loadVisitorLogs" qt-app --include="*.cpp" --include="*.h"
# expect: NO output — declaration and definition are gone, no call sites remain

grep -n "ui->" qt-app/visitorcontroller.cpp
# expect: NO output — the controller never touches a widget

grep -n "QFileDialog\|QMessageBox" qt-app/visitorcontroller.cpp
# expect: NO output — dialogs live only in the View

grep -n "visitorTable" qt-app/visitorcontroller.cpp
# expect: NO output

sed -n '/void adminWindow::on_extractVisitorBtn_clicked/,/^}/p' qt-app/adminwindow.cpp | grep -c "QXlsx"
# expect: 0 — no QXlsx code remains in the visitor export slot
```

Scope note on the QXlsx check: `grep -n "QXlsx" qt-app/adminwindow.cpp` will still hit — the `xlsxdocument.h`/`xlsxformat.h`/`xlsxcellrange.h` includes (lines ~52–54), `loadExcelToTable` (~line 1408), and `exportReportToExcel` all legitimately keep QXlsx for the Reports/Database features, which are out of scope. The check that matters is the `sed` range above: the visitor export slot itself must contain zero QXlsx tokens. Likewise, `adminWindow` still POSTs and parses JSON for other tabs; criterion 3 is specifically that the *visitor* tab's POST/parse (`loadVisitorLogs`) is gone, which the first grep proves.

- [ ] **Step 9: Manual smoke test of the running app**

Launch `qt-app/build/WITS.exe` — the binary this plan's build just produced, **not** a stale Qt Creator build from another checkout. With the PHP backend running, walk the spec's behavior criteria:

1. Open the Visitor Logs tab from the sidebar → all visitor logs auto-load; total label reads `📊 Total Visitors: <n>`.
2. Each date filter mode ("All", "Specific Day", "Month", "Date Range") shows/hides the correct widgets and fetches the correct range on Search.
3. Search by keyword works, combined with any date filter.
4. A filter with no matches shows the "No visitors found for the selected filters." overlay and `📊 Total Visitors: 0`.
5. Export produces an `.xlsx` with the identical layout (merged title rows, Generated On, conditional Filter Applied / Search Keyword rows, headers at row 8, column width 25, total row) and the correct default filename for every filter/search combination — spot-check at least "All, no search" (`VisitorLogs_All.xlsx`) and "Month + search" (`VisitorLogs_<term>_<MonthName>_<year>.xlsx`).
6. Export with an empty table state (fresh app, backend returning zero rows) → "No Data" info box.
7. Stop the backend, click Search → `"Network Error"` **critical** box with the network error string.

- [ ] **Step 10: Commit via the `commit` skill**

Proposed commit:

```
refactor(visitor): wire VisitorController into adminWindow

adminWindow is now a pure View for the Visitor Logs tab: the search
lambda and sidebar auto-load delegate to fetchVisitors, table
population and error boxes are signal-driven slots, and the extract
slot shrinks to guard + file dialog + exportToExcel with identical
message texts. loadVisitorLogs is deleted outright; the date-range
math lives in collectVisitorFilter/monthRange and the QXlsx report in
the controller. Behavior-identical — wire strings, message texts, and
the report layout are unchanged.
```

---

## Post-Task: Run /claude-review

After all three tasks are committed, the build is clean, all 6 ctest targets are green, and the manual smoke test passes, run `/claude-review` (via the Skill tool) before finishing the branch. Fix any Critical or Important findings, re-submit until APPROVE, then use `superpowers:finishing-a-development-branch` and the project-scoped `create-pr` skill.
