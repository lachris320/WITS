# ReportController + ReportRenderer Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract the Reports tab (~1,500 lines) out of the `adminWindow` God Object into two behavior-identical units — `ReportController` (QObject; network/data/pure logic) and `ReportRenderer` (stateless plain class; charts/PDF/Excel) — with unit tests, no wire-protocol change.

**Architecture:** `adminWindow` stays the View (all `ui->` reads, every `QMessageBox`/`QFileDialog`/`QPrintDialog`, the live-preview `QChartView` tree, device creation, `QSettings` reads). `ReportController` owns the injected-`QNetworkAccessManager` fetches + pure static parsers + `getPalette` + `computeDateRange`. `ReportRenderer` owns the chart-image makers, the multi-page `paintReport`, and the QXlsx writer — driven entirely by parameters (`ReportHeaderInfo` + resolved `ReportPalette`), reading no `QSettings` and no `ui->`.

**Tech Stack:** Qt 6.11 (Widgets, Network, Charts, PrintSupport, Gui), vendored QXlsx, C++17, CMake + Ninja + MinGW, Qt Test via ctest.

## Global Constraints

- `#ifndef` header guards (NOT `#pragma once`); guards `REPORTDATA_H`, `REPORTCONTROLLER_H`, `REPORTRENDERER_H`.
- `m_` prefix for new members. Functor-based `connect` (no `SIGNAL`/`SLOT` macros).
- `QNetworkAccessManager` **injected, not owned** (ctor param); the controller never deletes it. `reply->deleteLater()` on **every** path (success, network error, parse error). Every `connect(reply, &QNetworkReply::finished, this, …)` passes the controller (`this`) as the 3rd-arg context object.
- `ReportRenderer` is **stateless** — no `QSettings`, no `ui->`, no member state. Environment via `ReportHeaderInfo`; palette via `ReportPalette`.
- Purity gates (Task 4 verifies): `reportcontroller.cpp` has no `ui->`, `QMessageBox`, `QFileDialog`, `QChart`/`QChartView`, `QPainter`, `QXlsx`, or `QSettings`. `reportrenderer.cpp` has no `ui->`, `QMessageBox`, `QFileDialog`, or `QSettings`.
- Tests: `tst_reportcontroller` links `Qt::Test` + `Qt::Network` + `Qt::Gui` (Gui for the `QColor` symbols in `getPalette`), **no** offscreen. `tst_reportrenderer` links `Qt::Gui`/`Qt::Charts`/`QXlsx`, with `QT_QPA_PLATFORM=offscreen`.
- Security-hygiene (binding): no secrets/admin keys/credentials/backend URLs in source, tests, or CMake — use placeholders. Synthetic test data only (e.g. `"BSIT"`, `"College of Computing Studies"`, `"2023-00123"`). No hardcoded `C:\Users\<name>` paths. **NEVER stage/modify `qt-app/adminwindow.ui`** (carries the user's own uncommitted edit) or `qt-app/build/`.
- Stage files **by name** — never `git add -A`/`.`/`-u`. Commit via the `commit` skill. No Claude/Anthropic co-author trailers.

## Plan Note A — error-signal severity (faithful refinement of the spec)

The spec sketches `reportError(const QString &message)` and `loadError(const QString &title, const QString &message)`. But the Message/Dialog Inventory mandates **`QMessageBox::critical`** for transport errors (rows 1, 4, 6, 8) and **`QMessageBox::warning`** for parse/non-success errors (rows 2, 3, 4a, 5, 7, 9, 10) — two different icons. To reproduce those icons byte-identically, both signals carry a trailing `bool critical`:

```cpp
void reportError(const QString &message, bool critical);
void loadError(const QString &title, const QString &message, bool critical);
```

The View maps `critical ? QMessageBox::critical : QMessageBox::warning`. This is the only intentional divergence from the spec's signal sketch; it exists solely to preserve the inventory's exact dialog kinds. Everything else follows the spec verbatim.

## File Structure

| File | Responsibility |
|------|----------------|
| `qt-app/reportdata.h` (new) | Plain value types: `ReportPalette` (moved from `adminwindow.h`), `ReportHeaderInfo`, `ReportDataOutcome`, `DateRange`. |
| `qt-app/reportcontroller.h` / `.cpp` (new) | QObject controller: injected NAM, pure statics, five async fetches, signals. |
| `qt-app/reportrenderer.h` / `.cpp` (new) | Stateless renderer: aggregation statics, chart-image makers, `paintReport`, `writeReportToXlsx`. |
| `qt-app/tests/tst_reportcontroller.cpp` (new) | Qt Test — statics (9th target). |
| `qt-app/tests/tst_reportrenderer.cpp` (new) | Qt Test — aggregation + chart images (10th target). |
| `qt-app/CMakeLists.txt` (modify) | Add the three new source pairs to `WITS`. |
| `qt-app/tests/CMakeLists.txt` (modify) | Register the two new test targets. |
| `qt-app/adminwindow.h` / `.cpp` (modify) | Remove `ReportPalette` (include `reportdata.h`); add controller/renderer members + View slots; rewrite handlers to delegate; delete dead code. |

**Source-of-truth for verbatim ports:** the implementer has the repo checked out. Where a step says "copy `adminWindow::<fn>` (cpp:A–B) verbatim and apply these edits," read those exact lines from `qt-app/adminwindow.cpp` and transcribe them, applying only the listed edits. Do **not** paraphrase or "improve" the ported body — behavior must stay byte-identical.

---

## Task 1: `reportdata.h` + `ReportController` skeleton (value types, all pure statics, stubbed network methods)

**Files:**
- Create: `qt-app/reportdata.h`
- Create: `qt-app/reportcontroller.h`
- Create: `qt-app/reportcontroller.cpp`
- Create: `qt-app/tests/tst_reportcontroller.cpp`
- Modify: `qt-app/CMakeLists.txt` (add the three source pairs to `WITS`)
- Modify: `qt-app/tests/CMakeLists.txt` (register `tst_reportcontroller`)

**Interfaces:**
- Produces (consumed by Tasks 2, 3, 4):
  - `struct ReportPalette { QColor headerBg, headerText, rowEvenBg, rowOddBg, rowText; QVector<QColor> chartColors; };`
  - `struct ReportHeaderInfo { QString schoolName, address, logoPath, librarian, position; int openHour = 7; int closeHour = 21; };`
  - `enum class ReportDataOutcome { Success, NotSuccess, InvalidResponse };`
  - `struct DateRange { QString start; QString end; bool valid = false; };`
  - `static ReportPalette ReportController::getPalette(const QString &choice);`
  - `static QStringList ReportController::parseDepartments(const QByteArray &raw);`
  - `static QStringList ReportController::parseYears(const QByteArray &raw);`
  - `static QStringList ReportController::parseCourses(const QByteArray &raw);`
  - `static ReportDataOutcome ReportController::parseReportData(const QByteArray &raw, QJsonArray &outData, QString &outMessage);`
  - `static QJsonArray ReportController::parsePreviewData(const QByteArray &raw);`
  - `static DateRange ReportController::computeDateRange(int durationType, const QDate &day, int month, int monthYear, const QString &semester, int semYear, const QDate &customStart, const QDate &customEnd);`
  - Signals (declared now, emitted in Task 2): `departmentsLoaded`, `yearsLoaded`, `coursesLoaded`, `reportDataReady`, `reportError(msg,critical)`, `previewDataReady`, `loadError(title,msg,critical)`.

- [ ] **Step 1: Create `qt-app/reportdata.h`**

```cpp
#ifndef REPORTDATA_H
#define REPORTDATA_H

#include <QColor>
#include <QString>
#include <QVector>

// Moved verbatim out of adminwindow.h (was adminwindow.h:54-62).
struct ReportPalette {
    QColor headerBg;
    QColor headerText;
    QColor rowEvenBg;
    QColor rowOddBg;
    QColor rowText;
    QVector<QColor> chartColors;
};

// Environment the View reads from QSettings and passes into ReportRenderer,
// so the renderer stays stateless (no QSettings, no ui->). Defaults reproduce
// the QSettings defaults at paintReport (school/admin) and makeLineChartImage
// (library hours).
struct ReportHeaderInfo {
    QString schoolName;        // "school/name"      (default "Your School Name")
    QString address;           // "school/address"   (default "Your Address")
    QString logoPath;          // "school/logoPath"  (default "")
    QString librarian;         // "admin/name"       (default "")
    QString position;          // "admin/position"   (default "")
    int     openHour  = 7;     // "library/openHour"  (default 7)
    int     closeHour = 21;    // "library/closeHour" (default 21)
};

// Result of decoding a get_report_data.php POST response.
enum class ReportDataOutcome {
    Success,          // status == "success"; data array in outData
    NotSuccess,       // status != "success"; message in outMessage
    InvalidResponse   // body is not a JSON object
    // NetworkError is decided by the caller from reply->error(), not here.
};

// Pure output of the duration -> date-range computation.
struct DateRange {
    QString start;   // "yyyy-MM-dd"
    QString end;     // "yyyy-MM-dd"
    bool    valid = false;
};

#endif // REPORTDATA_H
```

- [ ] **Step 2: Create `qt-app/reportcontroller.h`**

```cpp
#ifndef REPORTCONTROLLER_H
#define REPORTCONTROLLER_H

#include <QByteArray>
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "reportdata.h"

class QNetworkAccessManager;

class ReportController : public QObject
{
    Q_OBJECT
public:
    explicit ReportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static ReportPalette getPalette(const QString &choice);
    static QStringList  parseDepartments(const QByteArray &raw);   // get_departments.php
    static QStringList  parseYears(const QByteArray &raw);         // get_years.php
    static QStringList  parseCourses(const QByteArray &raw);       // get_courses.php
    static ReportDataOutcome parseReportData(const QByteArray &raw,
                                             QJsonArray &outData,
                                             QString &outMessage); // get_report_data.php
    static QJsonArray   parsePreviewData(const QByteArray &raw);   // api.php/reports/data

    // Pure duration -> date-range math, extracted from collectReportFilters.
    // durationType: 0=day, 1=month, 2=semester, 3=custom (matches durationTypeBox).
    static DateRange computeDateRange(int durationType,
                                      const QDate &day,                     // case 0
                                      int month, int monthYear,             // case 1 (month 1..12)
                                      const QString &semester, int semYear, // case 2
                                      const QDate &customStart,             // case 3
                                      const QDate &customEnd);

    // Async network methods (implemented in Task 2).
    void loadDepartments();                      // GET get_departments.php
    void loadYears();                            // GET get_years.php
    void loadCourses(const QString &department); // GET get_courses.php?department=..&include_all=true
    void fetchReportRows(const QJsonObject &filters);  // POST get_report_data.php
    void fetchPreviewData(const QJsonObject &filters); // POST api.php/reports/data

signals:
    void departmentsLoaded(const QStringList &departments);
    void yearsLoaded(const QStringList &years);
    void coursesLoaded(const QStringList &courses);
    void reportDataReady(const QJsonArray &data);
    void reportError(const QString &message, bool critical);
    void previewDataReady(const QJsonArray &data);
    void loadError(const QString &title, const QString &message, bool critical);

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // REPORTCONTROLLER_H
```

- [ ] **Step 3: Create `qt-app/reportcontroller.cpp` — ctor, all pure statics, stubbed network methods**

The `getPalette` body is a verbatim port of `adminWindow::getPalette` (cpp:2478–2508). The five network methods are **stubbed** here (empty bodies with a `// Task 2` marker) so the target links; Task 2 fills them.

```cpp
#include "reportcontroller.h"
#include "apiconfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QUrl>
#include <QUrlQuery>

ReportController::ReportController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

// Verbatim port of adminWindow::getPalette (cpp:2478-2508). Blue/Green/Red have
// 8 chart colors; the default palette has 10. Do not change the counts.
ReportPalette ReportController::getPalette(const QString &choice) {
    if (choice == "Blue") {
        return {
            QColor("#2E86C1"), Qt::white, QColor("#EBF5FB"), Qt::white, Qt::black,
            { QColor("#0D47A1"), QColor("#1565C0"), QColor("#1976D2"),
             QColor("#1E88E5"), QColor("#42A5F5"), QColor("#64B5F6"),
             QColor("#90CAF9"), QColor("#BBDEFB") }
        };
    } else if (choice == "Green") {
        return {
            QColor("#27AE60"), Qt::white, QColor("#E9F7EF"), Qt::white, Qt::black,
            { QColor("#1B5E20"), QColor("#2E7D32"), QColor("#388E3C"),
             QColor("#43A047"), QColor("#66BB6A"), QColor("#81C784"),
             QColor("#A5D6A7"), QColor("#C8E6C9") }
        };
    } else if (choice == "Red") {
        return {
            QColor("#C0392B"), Qt::white, QColor("#FDEDEC"), Qt::white, Qt::black,
            { QColor("#7B241C"), QColor("#922B21"), QColor("#A93226"),
             QColor("#C0392B"), QColor("#CD6155"), QColor("#E6B0AA"),
             QColor("#F5B7B1"), QColor("#FADBD8") }
        };
    } else { // Default (multi-color)
        return {
            QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
            { QColor("#1f77b4"), QColor("#ff7f0e"), QColor("#2ca02c"),
             QColor("#d62728"), QColor("#9467bd"), QColor("#8c564b"),
             QColor("#e377c2"), QColor("#7f7f7f"), QColor("#bcbd22"), QColor("#17becf") }
        };
    }
}

// Pure success-extractor. Empty on non-success or non-object. (loadFilterDepartments
// cpp:1841-1848 does NOT skip "all", unlike StudentController::parseDepartments.)
QStringList ReportController::parseDepartments(const QByteArray &raw) {
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["departments"].toArray();
        for (const QJsonValue &v : arr)
            out << v.toString();
    }
    return out;
}

// years come back as ints; emit them as strings (loadAvailableYears cpp:1878-1883).
QStringList ReportController::parseYears(const QByteArray &raw) {
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["years"].toArray();
        for (const QJsonValue &v : arr)
            out << QString::number(v.toInt());
    }
    return out;
}

// courses success-extractor (onFilterDepartmentBoxCurrentIndexChanged cpp:1927-1932).
QStringList ReportController::parseCourses(const QByteArray &raw) {
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["courses"].toArray();
        for (const QJsonValue &v : arr)
            out << v.toString();
    }
    return out;
}

// Pure port of postReportData's decode (cpp:1663-1673). Routes the three
// distinct outcomes so the caller can pick the matching dialog.
ReportDataOutcome ReportController::parseReportData(const QByteArray &raw,
                                                    QJsonArray &outData,
                                                    QString &outMessage) {
    outData = QJsonArray();
    outMessage.clear();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return ReportDataOutcome::InvalidResponse;
    const QJsonObject obj = doc.object();
    if (obj["status"].toString() != "success") {
        outMessage = obj["message"].toString();
        return ReportDataOutcome::NotSuccess;
    }
    outData = obj["data"].toArray();
    return ReportDataOutcome::Success;
}

// Pure port of fetchPreviewData's success check (cpp:2744-2748). Empty array on
// error/non-success. (Note: an empty array is also a valid success payload; the
// network method in Task 2 re-checks success separately to decide whether to emit.)
QJsonArray ReportController::parsePreviewData(const QByteArray &raw) {
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isObject() && doc.object()["status"].toString() == "success")
        return doc.object()["data"].toArray();
    return QJsonArray();
}

// Pure port of the duration switch's date math (collectReportFilters cpp:1732-1802),
// minus widget reads and QMessageBox. Month/year and semester/year INDEX guards
// stay View-side; this is only reached after they pass, so cases 1 & 2 are always
// valid. valid=false for invalid day (case 0), invalid/reversed custom (case 3),
// or out-of-range durationType (default).
DateRange ReportController::computeDateRange(int durationType,
                                             const QDate &day,
                                             int month, int monthYear,
                                             const QString &semester, int semYear,
                                             const QDate &customStart,
                                             const QDate &customEnd) {
    DateRange r;
    switch (durationType) {
    case 0: { // Day
        if (!day.isValid())
            return r;                       // valid stays false (cpp:1735)
        r.start = day.toString("yyyy-MM-dd");
        r.end   = day.toString("yyyy-MM-dd");
        r.valid = true;
        break;
    }
    case 1: { // Month
        QDate start(monthYear, month, 1);   // cpp:1755
        QDate end = start.addMonths(1).addDays(-1);
        r.start = start.toString("yyyy-MM-dd");
        r.end   = end.toString("yyyy-MM-dd");
        r.valid = true;
        break;
    }
    case 2: { // Semester
        if (semester.contains("1")) {       // cpp:1773
            r.start = QString("%1-01-01").arg(semYear);
            r.end   = QString("%1-06-30").arg(semYear);
        } else {
            r.start = QString("%1-07-01").arg(semYear);
            r.end   = QString("%1-12-31").arg(semYear);
        }
        r.valid = true;
        break;
    }
    case 3: { // Custom
        if (!customStart.isValid() || !customEnd.isValid() || customStart > customEnd)
            return r;                       // valid stays false (cpp:1788)
        r.start = customStart.toString("yyyy-MM-dd");
        r.end   = customEnd.toString("yyyy-MM-dd");
        r.valid = true;
        break;
    }
    default:
        return r;                           // valid stays false (cpp:1798)
    }
    return r;
}

// ---- Network methods: stubbed in Task 1, implemented in Task 2 ----
void ReportController::loadDepartments()                      { /* Task 2 */ }
void ReportController::loadYears()                            { /* Task 2 */ }
void ReportController::loadCourses(const QString &)           { /* Task 2 */ }
void ReportController::fetchReportRows(const QJsonObject &)   { /* Task 2 */ }
void ReportController::fetchPreviewData(const QJsonObject &)  { /* Task 2 */ }
```

- [ ] **Step 4: Write the failing test `qt-app/tests/tst_reportcontroller.cpp`**

Uses `QTEST_GUILESS_MAIN` (QCoreApplication — no QPA plugin needed; `QColor` values construct fine). Synthetic data only.

```cpp
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "reportcontroller.h"

class TstReportController : public QObject
{
    Q_OBJECT

private slots:
    // ---- getPalette ----
    void getPalette_blue_hasHeaderAnd8Colors();
    void getPalette_green_hasHeaderAnd8Colors();
    void getPalette_red_hasHeaderAnd8Colors();
    void getPalette_unknown_returnsDefault10Colors();

    // ---- parsers ----
    void parseDepartments_success_returnsList();
    void parseDepartments_notSuccess_returnsEmpty();
    void parseYears_success_intsToStrings();
    void parseYears_notSuccess_returnsEmpty();
    void parseCourses_success_returnsList();
    void parseCourses_notSuccess_returnsEmpty();
    void parseReportData_success_fillsData();
    void parseReportData_notSuccess_setsMessage();
    void parseReportData_nonObject_invalid();
    void parsePreviewData_success_returnsArray();
    void parsePreviewData_error_returnsEmpty();

    // ---- computeDateRange ----
    void computeDateRange_day_valid();
    void computeDateRange_day_invalid();
    void computeDateRange_month_firstToLastDay();
    void computeDateRange_semesterFirst_janToJun();
    void computeDateRange_semesterSecond_julToDec();
    void computeDateRange_customValid();
    void computeDateRange_customReversed_invalid();
    void computeDateRange_outOfRange_invalid();

private:
    static QByteArray obj(const QJsonObject &o) {
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }
};

void TstReportController::getPalette_blue_hasHeaderAnd8Colors() {
    const ReportPalette p = ReportController::getPalette("Blue");
    QCOMPARE(p.headerBg, QColor("#2E86C1"));
    QCOMPARE(p.chartColors.size(), 8);
}
void TstReportController::getPalette_green_hasHeaderAnd8Colors() {
    const ReportPalette p = ReportController::getPalette("Green");
    QCOMPARE(p.headerBg, QColor("#27AE60"));
    QCOMPARE(p.chartColors.size(), 8);
}
void TstReportController::getPalette_red_hasHeaderAnd8Colors() {
    const ReportPalette p = ReportController::getPalette("Red");
    QCOMPARE(p.headerBg, QColor("#C0392B"));
    QCOMPARE(p.chartColors.size(), 8);
}
void TstReportController::getPalette_unknown_returnsDefault10Colors() {
    const ReportPalette p = ReportController::getPalette("Rainbow");
    QCOMPARE(p.headerBg, QColor("#34495E"));
    QCOMPARE(p.chartColors.size(), 10);
}

void TstReportController::parseDepartments_success_returnsList() {
    QJsonObject o{{"status", "success"},
                  {"departments", QJsonArray{"College of Computing Studies",
                                             "College of Engineering"}}};
    const QStringList got = ReportController::parseDepartments(obj(o));
    QCOMPARE(got, (QStringList{"College of Computing Studies", "College of Engineering"}));
}
void TstReportController::parseDepartments_notSuccess_returnsEmpty() {
    QJsonObject o{{"status", "error"}, {"message", "nope"}};
    QVERIFY(ReportController::parseDepartments(obj(o)).isEmpty());
}
void TstReportController::parseYears_success_intsToStrings() {
    QJsonObject o{{"status", "success"}, {"years", QJsonArray{2023, 2024}}};
    QCOMPARE(ReportController::parseYears(obj(o)), (QStringList{"2023", "2024"}));
}
void TstReportController::parseYears_notSuccess_returnsEmpty() {
    QJsonObject o{{"status", "error"}};
    QVERIFY(ReportController::parseYears(obj(o)).isEmpty());
}
void TstReportController::parseCourses_success_returnsList() {
    QJsonObject o{{"status", "success"}, {"courses", QJsonArray{"All", "BSIT", "BSCS"}}};
    QCOMPARE(ReportController::parseCourses(obj(o)), (QStringList{"All", "BSIT", "BSCS"}));
}
void TstReportController::parseCourses_notSuccess_returnsEmpty() {
    QJsonObject o{{"status", "error"}};
    QVERIFY(ReportController::parseCourses(obj(o)).isEmpty());
}
void TstReportController::parseReportData_success_fillsData() {
    QJsonObject o{{"status", "success"},
                  {"data", QJsonArray{QJsonObject{{"course", "BSIT"}, {"visits", 3}}}}};
    QJsonArray data; QString msg;
    QCOMPARE(ReportController::parseReportData(obj(o), data, msg),
             ReportDataOutcome::Success);
    QCOMPARE(data.size(), 1);
    QVERIFY(msg.isEmpty());
}
void TstReportController::parseReportData_notSuccess_setsMessage() {
    QJsonObject o{{"status", "error"}, {"message", "bad filters"}};
    QJsonArray data; QString msg;
    QCOMPARE(ReportController::parseReportData(obj(o), data, msg),
             ReportDataOutcome::NotSuccess);
    QCOMPARE(msg, QString("bad filters"));
}
void TstReportController::parseReportData_nonObject_invalid() {
    QJsonArray data; QString msg;
    QCOMPARE(ReportController::parseReportData(QByteArray("not json"), data, msg),
             ReportDataOutcome::InvalidResponse);
}
void TstReportController::parsePreviewData_success_returnsArray() {
    QJsonObject o{{"status", "success"},
                  {"data", QJsonArray{QJsonObject{{"course", "BSIT"}}}}};
    QCOMPARE(ReportController::parsePreviewData(obj(o)).size(), 1);
}
void TstReportController::parsePreviewData_error_returnsEmpty() {
    QJsonObject o{{"status", "error"}};
    QVERIFY(ReportController::parsePreviewData(obj(o)).isEmpty());
}

void TstReportController::computeDateRange_day_valid() {
    const DateRange r = ReportController::computeDateRange(
        0, QDate(2023, 5, 17), 0, 0, QString(), 0, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-05-17"));
    QCOMPARE(r.end,   QString("2023-05-17"));
}
void TstReportController::computeDateRange_day_invalid() {
    const DateRange r = ReportController::computeDateRange(
        0, QDate(), 0, 0, QString(), 0, QDate(), QDate());
    QVERIFY(!r.valid);
}
void TstReportController::computeDateRange_month_firstToLastDay() {
    const DateRange r = ReportController::computeDateRange(
        1, QDate(), 2 /*Feb*/, 2024, QString(), 0, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2024-02-01"));
    QCOMPARE(r.end,   QString("2024-02-29")); // 2024 is a leap year
}
void TstReportController::computeDateRange_semesterFirst_janToJun() {
    const DateRange r = ReportController::computeDateRange(
        2, QDate(), 0, 0, "1st Semester", 2023, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-01-01"));
    QCOMPARE(r.end,   QString("2023-06-30"));
}
void TstReportController::computeDateRange_semesterSecond_julToDec() {
    const DateRange r = ReportController::computeDateRange(
        2, QDate(), 0, 0, "2nd Semester", 2023, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-07-01"));
    QCOMPARE(r.end,   QString("2023-12-31"));
}
void TstReportController::computeDateRange_customValid() {
    const DateRange r = ReportController::computeDateRange(
        3, QDate(), 0, 0, QString(), 0, QDate(2023, 1, 10), QDate(2023, 3, 20));
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-01-10"));
    QCOMPARE(r.end,   QString("2023-03-20"));
}
void TstReportController::computeDateRange_customReversed_invalid() {
    const DateRange r = ReportController::computeDateRange(
        3, QDate(), 0, 0, QString(), 0, QDate(2023, 3, 20), QDate(2023, 1, 10));
    QVERIFY(!r.valid);
}
void TstReportController::computeDateRange_outOfRange_invalid() {
    const DateRange r = ReportController::computeDateRange(
        9, QDate(2023, 5, 17), 0, 0, QString(), 0, QDate(), QDate());
    QVERIFY(!r.valid);
}

QTEST_GUILESS_MAIN(TstReportController)
#include "tst_reportcontroller.moc"
```

- [ ] **Step 5: Wire CMake — add sources to `WITS` and register `tst_reportcontroller`**

In `qt-app/CMakeLists.txt`, add to the `qt_add_executable(WITS …)` source list, next to the student files:

```cmake
        reportdata.h
        reportcontroller.h reportcontroller.cpp
        reportrenderer.h reportrenderer.cpp
```

> The `reportrenderer.*` files do not exist until Task 3. Listing them now will break the `WITS` configure. **Add only the `reportdata.h` + `reportcontroller.h reportcontroller.cpp` lines in Task 1**; add the `reportrenderer.h reportrenderer.cpp` line in Task 3. (Split intentionally so each task configures cleanly.)

So for Task 1, add exactly:

```cmake
        reportdata.h
        reportcontroller.h reportcontroller.cpp
```

In `qt-app/tests/CMakeLists.txt`, append the 9th target:

```cmake
# 9th — Core+Network+Gui, NO offscreen. Gui is required because getPalette
# returns a ReportPalette (QColor / QVector<QColor>), and QColor is a Qt::Gui
# type — but a QColor *value* under QTEST_GUILESS_MAIN needs no QPA plugin.
qt_add_executable(tst_reportcontroller
    tst_reportcontroller.cpp
    ${CMAKE_SOURCE_DIR}/reportcontroller.cpp
    ${CMAKE_SOURCE_DIR}/reportcontroller.h
    ${CMAKE_SOURCE_DIR}/reportdata.h
)
set_target_properties(tst_reportcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_reportcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_reportcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
)
add_test(NAME tst_reportcontroller COMMAND tst_reportcontroller)
set_tests_properties(tst_reportcontroller PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
```

> `reportcontroller.cpp` includes `apiconfig.h` and references `ApiConfig::endpoint` in the (stubbed) network methods — the stubs don't call it yet, but Task 2 will. **No `apiconfig.cpp` is needed on this target:** `ApiConfig::endpoint`/`baseUrl` are `inline` in `apiconfig.h` (header-only), and the precedent `tst_studentcontroller` links `ApiConfig::endpoint` with only `tst_studentcontroller.cpp` as its source plus the `${CMAKE_SOURCE_DIR}` include dir — no `apiconfig.cpp`. Mirror that: the `${CMAKE_SOURCE_DIR}` include dir on this target is sufficient.

- [ ] **Step 6: Configure + build + run the test — verify RED then GREEN**

Set the toolchain PATH first (tools are not on PATH):

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_reportcontroller
```

Expected: after Step 3+4 the statics are implemented, so the target builds and `tst_reportcontroller` **passes** all cases. (This task's "red" is the compile-failure state before Step 3 — if you want an explicit red, stub `getPalette` to `return {};` first, watch `getPalette_*` fail, then restore.) Expected final: `100% tests passed`.

- [ ] **Step 7: Commit**

Use the `commit` skill. Stage by name:
```
qt-app/reportdata.h
qt-app/reportcontroller.h
qt-app/reportcontroller.cpp
qt-app/tests/tst_reportcontroller.cpp
qt-app/CMakeLists.txt
qt-app/tests/CMakeLists.txt
```
Suggested subject: `feat(report): add ReportController value types, pure statics, test target`

---

## Task 2: Implement the five `ReportController` network methods

**Files:**
- Modify: `qt-app/reportcontroller.cpp` (replace the five stubs from Task 1)

**Interfaces:**
- Consumes: `ApiConfig::endpoint(...)`, the pure parsers + `parseReportData` outcome enum from Task 1, and the signals declared in `reportcontroller.h`.
- Produces (consumed by Task 4's View slots): the runtime emissions —
  `loadDepartments` → `departmentsLoaded` | `loadError`; `loadYears` → `yearsLoaded` | `loadError` (silent on non-object); `loadCourses` → `coursesLoaded` | `loadError`; `fetchReportRows` → `reportDataReady` | `reportError`; `fetchPreviewData` → `previewDataReady` (silent on any error).

- [ ] **Step 1: Replace the `loadDepartments` stub**

Reproduces `loadFilterDepartments` (cpp:1818–1853). Routes rows 4 (critical), 4a (warning `"Invalid server response."`), 5 (warning `"Failed to load departments."`).

```cpp
void ReportController::loadDepartments() {
    QUrl url = ApiConfig::endpoint("get_departments.php");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loadError("Error", reply->errorString(), true);   // row 4
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            emit loadError("Error", "Invalid server response.", false);  // row 4a
            return;
        }
        if (doc.object()["status"].toString() != "success") {
            emit loadError("Error", "Failed to load departments.", false); // row 5
            return;
        }
        emit departmentsLoaded(parseDepartments(resp));
    });
}
```

- [ ] **Step 2: Replace the `loadYears` stub**

Reproduces `loadAvailableYears` (cpp:1855–1888). Non-object path is **silent** (`return;` at cpp:1872 — no dialog, no signal).

```cpp
void ReportController::loadYears() {
    QUrl url = ApiConfig::endpoint("get_years.php");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loadError("Error", reply->errorString(), true);   // row 6
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject())
            return;                                                // silent (cpp:1872)
        if (doc.object()["status"].toString() != "success") {
            emit loadError("Error", doc.object()["message"].toString(), false); // row 7
            return;
        }
        emit yearsLoaded(parseYears(resp));
    });
}
```

- [ ] **Step 3: Replace the `loadCourses` stub**

Reproduces the network half of `onFilterDepartmentBoxCurrentIndexChanged` (cpp:1898–1936). The `index <= 0` reset stays View-side (Task 4).

```cpp
void ReportController::loadCourses(const QString &department) {
    QUrl url = ApiConfig::endpoint("get_courses.php");
    QUrlQuery query;
    query.addQueryItem("department", department);
    query.addQueryItem("include_all", "true");
    url.setQuery(query);
    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loadError("Error", reply->errorString(), true);   // row 8
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            emit loadError("Error", "Invalid response from server.", false); // row 9
            return;
        }
        if (doc.object()["status"].toString() != "success") {
            emit loadError("Error", doc.object()["message"].toString(), false); // row 10
            return;
        }
        emit coursesLoaded(parseCourses(resp));
    });
}
```

- [ ] **Step 4: Replace the `fetchReportRows` stub**

Reproduces `postReportData` (cpp:1646–1675), reusing `parseReportData`. Rows 1 (critical), 2/3 (warning).

```cpp
void ReportController::fetchReportRows(const QJsonObject &filters) {
    QUrl url = ApiConfig::endpoint("get_report_data.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit reportError(reply->errorString(), true);          // row 1
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        QJsonArray data;
        QString message;
        switch (parseReportData(resp, data, message)) {
        case ReportDataOutcome::InvalidResponse:
            emit reportError("Invalid response from server.", false); // row 2
            break;
        case ReportDataOutcome::NotSuccess:
            emit reportError(message, false);                         // row 3
            break;
        case ReportDataOutcome::Success:
            emit reportDataReady(data);
            break;
        }
    });
}
```

- [ ] **Step 5: Replace the `fetchPreviewData` stub**

Reproduces `fetchPreviewData` (cpp:2726–2750): a **different** endpoint (`api.php/reports/data`), silent on error (only `qDebug`), emits only on success (even when the data array is empty).

```cpp
void ReportController::fetchPreviewData(const QJsonObject &filters) {
    QUrl url = ApiConfig::endpoint("api.php/reports/data");   // API router (distinct endpoint)
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Preview fetch error:" << reply->errorString();
            reply->deleteLater();
            return;                                            // silent (cpp:2736)
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        // Emit only on a success object (matches cpp:2745). An empty data array on
        // success is still emitted — the View then clears the preview, as today.
        if (doc.isObject() && doc.object()["status"].toString() == "success")
            emit previewDataReady(doc.object()["data"].toArray());
    });
}
```

Add `#include <QDebug>` to `reportcontroller.cpp` if not already present.

- [ ] **Step 6: Build + run the controller test — verify still GREEN**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_reportcontroller
```
Expected: `100% tests passed`. (The network methods have no automated coverage — per the spec's accepted limitation, their emit routing + `deleteLater` are verified by the Task-4 purity/grep gates and manual QA. This step only confirms the statics still pass and the new code compiles.)

- [ ] **Step 7: Commit**

Use the `commit` skill. Stage `qt-app/reportcontroller.cpp`.
Suggested subject: `feat(report): implement ReportController network methods`

---

## Task 3: `ReportRenderer` — aggregation statics, chart makers, `paintReport`, `writeReportToXlsx`

**Files:**
- Create: `qt-app/reportrenderer.h`
- Create: `qt-app/reportrenderer.cpp`
- Create: `qt-app/tests/tst_reportrenderer.cpp`
- Modify: `qt-app/CMakeLists.txt` (add the `reportrenderer.h reportrenderer.cpp` line)
- Modify: `qt-app/tests/CMakeLists.txt` (register `tst_reportrenderer`)

**Interfaces:**
- Consumes: `ReportPalette`, `ReportHeaderInfo` (from `reportdata.h`).
- Produces (consumed by Task 4's export wrappers):
  - `static QMap<QString,int> ReportRenderer::aggregateVisitsByCourse(const QJsonArray &data);`
  - `static QMap<QString,QMap<int,int>> ReportRenderer::aggregateVisitsByCourseHour(const QJsonArray &data, int openHour, int closeHour);`
  - `static QImage ReportRenderer::makeBarChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette);`
  - `static QImage ReportRenderer::makePieChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette);`
  - `static QImage ReportRenderer::makeLineChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette, int openHour, int closeHour);`
  - `static bool ReportRenderer::paintReport(QPagedPaintDevice *device, int resolution, const QJsonArray &data, const QJsonObject &filters, const ReportPalette &palette, const ReportHeaderInfo &info);`
  - `static bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx, const QJsonArray &rows, const QJsonObject &filters, const ReportHeaderInfo &info);`

- [ ] **Step 1: Create `qt-app/reportrenderer.h`**

```cpp
#ifndef REPORTRENDERER_H
#define REPORTRENDERER_H

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSize>
#include <QString>

#include "reportdata.h"
#include "xlsxdocument.h"   // QXlsx::Document

class QPagedPaintDevice;

// Stateless renderer. No QSettings, no ui->, no member state — every method is a
// pure function of its arguments. Charts/PDF/Excel bodies are verbatim ports from
// adminwindow.cpp with the QSettings reads replaced by ReportHeaderInfo params and
// getPalette(...) resolved by the caller into `palette`.
class ReportRenderer
{
public:
    // Pure aggregation helpers (factored out of the chart makers).
    static QMap<QString, int>           aggregateVisitsByCourse(const QJsonArray &data);
    static QMap<QString, QMap<int, int>> aggregateVisitsByCourseHour(const QJsonArray &data,
                                                                     int openHour, int closeHour);

    static QImage makeBarChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makePieChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makeLineChartImage(const QJsonArray &data, QSize size,
                                     const ReportPalette &palette,
                                     int openHour, int closeHour);

    static bool paintReport(QPagedPaintDevice *device, int resolution,
                            const QJsonArray &data, const QJsonObject &filters,
                            const ReportPalette &palette,
                            const ReportHeaderInfo &info);

    static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                  const QJsonArray &rows,
                                  const QJsonObject &filters,
                                  const ReportHeaderInfo &info);
};

#endif // REPORTRENDERER_H
```

> Verify the QXlsx include name against the existing users (`grep -rn "xlsxdocument.h\|QXlsx/xlsxdocument.h" qt-app/*.cpp`). Match whatever `adminwindow.cpp` currently uses for `QXlsx::Document` — use the identical include path.

- [ ] **Step 2: Create `qt-app/reportrenderer.cpp` — the two aggregation statics**

```cpp
#include "reportrenderer.h"

#include <QChart>
#include <QChartView>
#include <QBarSet>
#include <QBarSeries>
#include <QBarCategoryAxis>
#include <QPieSeries>
#include <QPieSlice>
#include <QLineSeries>
#include <QValueAxis>

#include <QDate>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QMarginsF>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QScopeGuard>
#include <QTime>

#include "xlsxformat.h"    // QXlsx::Format  (match adminwindow.cpp's include)

// Factored out of make{Bar,Pie}ChartImage's aggregation loop (cpp:125-131 / 193-199).
QMap<QString, int> ReportRenderer::aggregateVisitsByCourse(const QJsonArray &data) {
    QMap<QString, int> courseCounts;
    for (const QJsonValue &v : data) {
        const QJsonObject obj = v.toObject();
        courseCounts[obj["course"].toString()] += obj["visits"].toInt();
    }
    return courseCounts;
}

// Factored out of makeLineChartImage's aggregation loop (cpp:253-275), with the
// QSettings library-hours reads replaced by openHour/closeHour params. Skips
// invalid login_time and hours outside [openHour, closeHour], exactly as legacy.
QMap<QString, QMap<int, int>> ReportRenderer::aggregateVisitsByCourseHour(
        const QJsonArray &data, int openHour, int closeHour) {
    QMap<QString, QMap<int, int>> courseTimeCounts;
    for (const QJsonValue &v : data) {
        const QJsonObject obj = v.toObject();
        const QString course = obj["course"].toString();
        const QString loginTime = obj["login_time"].toString();
        const QTime time = QTime::fromString(loginTime, "HH:mm:ss");
        if (!time.isValid()) {
            qDebug() << "Invalid login_time format:" << loginTime;
            continue;
        }
        const int hour = time.hour();
        if (hour < openHour || hour > closeHour)
            continue;
        courseTimeCounts[course][hour] += 1;
    }
    return courseTimeCounts;
}
```

- [ ] **Step 3: Port the three chart makers into `reportrenderer.cpp`**

Copy `adminWindow::makeBarChartImage` (cpp:123–187), `makePieChartImage` (cpp:191–241), and `makeLineChartImage` (cpp:246–end of function) **verbatim** into `ReportRenderer::…`, applying exactly these edits:

- **Bar** (`makeBarChartImage`): change the signature to `ReportRenderer::makeBarChartImage`. Replace the inline aggregation loop (the local `QMap<QString,int> courseCounts;` + `for` loop, cpp:125–131) with:
  ```cpp
  QMap<QString, int> courseCounts = aggregateVisitsByCourse(data);
  ```
  Leave the rest (series/axes/legend/margins/`QChartView`/`render`) byte-for-byte.

- **Pie** (`makePieChartImage`): same — signature to `ReportRenderer::makePieChartImage`, replace the aggregation loop (cpp:193–199) with `QMap<QString, int> courseCounts = aggregateVisitsByCourse(data);`. Rest verbatim.

- **Line** (`makeLineChartImage`): signature to `ReportRenderer::makeLineChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette, int openHour, int closeHour)`. Then:
  - **Delete** the `QSettings settings("MyCompany", "MyApp");` block that reads `library/openHour` / `library/closeHour` (cpp:248–250) — those now arrive as the `openHour`/`closeHour` params.
  - **Replace** the aggregation loop + inline `globalMax` computation (cpp:252–275) with:
    ```cpp
    QMap<QString, QMap<int, int>> courseTimeCounts =
        aggregateVisitsByCourseHour(data, openHour, closeHour);

    int globalMax = 0;
    for (auto it = courseTimeCounts.begin(); it != courseTimeCounts.end(); ++it)
        for (int count : it.value())
            if (count > globalMax)
                globalMax = count;
    ```
    (Same value as the legacy loop: the helper applies the identical `[openHour,closeHour]` filter, so the max over the returned map equals the legacy `globalMax`.)
  - Leave everything downstream (the per-course `QLineSeries` build using `openHour`/`closeHour`, the axes that reference `globalMax`, the `QChartView` render) **verbatim** — the `openHour`/`closeHour` references now resolve to the params instead of the deleted locals.

- [ ] **Step 4: Port `paintReport` into `reportrenderer.cpp`**

Copy `adminWindow::paintReport` (cpp:2018–2291) **verbatim** into `ReportRenderer::paintReport`, applying exactly these edits:

- Signature → `bool ReportRenderer::paintReport(QPagedPaintDevice *device, int resolution, const QJsonArray &data, const QJsonObject &filters, const ReportPalette &palette, const ReportHeaderInfo &info)`.
- **Delete** the top-level `QSettings settings("MyCompany", "MyApp");` + `librarian`/`position` reads (cpp:2047–2049) and replace the two locals with the `info` fields:
  ```cpp
  QString librarian = info.librarian;
  QString position  = info.position;
  ```
- Inside the `drawHeader` lambda, **delete** its `QSettings settings("MyCompany", "MyApp");` + `schoolName`/`address`/`logoPath` reads (cpp:2077–2080) and replace with:
  ```cpp
  QString schoolName = info.schoolName;
  QString address    = info.address;
  QString logoPath   = info.logoPath;
  ```
- **Delete** the local `ReportPalette palette = getPalette(filters["palette"].toString());` (cpp:2132) — the resolved `palette` param is used instead. (Every downstream `palette.` reference now binds to the param.)
- The chart-maker calls (cpp:2247–2259) become the static siblings, and `makeLineChartImage` gains the two hour params:
  ```cpp
  drawFullscreenChart("Bar Chart",  makeBarChartImage(data, barSize, palette));
  drawFullscreenChart("Pie Chart",  makePieChartImage(data, pieSize, palette));
  drawFullscreenChart("Line Chart", makeLineChartImage(data, lineSize, palette, info.openHour, info.closeHour));
  ```
  (Apply the same `, info.openHour, info.closeHour` addition to the `Line Chart` call in the non-"All" branch at cpp:2259.)
- Everything else (fonts, margins, column widths, footer text, page breaks, "Prepared by" page, `qScopeGuard`) stays byte-identical.

- [ ] **Step 5: Port `writeReportToXlsx` into `reportrenderer.cpp`**

Copy the **document-building body** of `adminWindow::exportReportToExcel` (cpp:2369–2467 — from `QXlsx::Document xlsx;`… no, the View owns the `QXlsx::Document`) into `ReportRenderer::writeReportToXlsx`, applying these edits:

- Signature → `bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx, const QJsonArray &rows, const QJsonObject &filters, const ReportHeaderInfo &info)`.
- **Omit** the `QFileDialog`/empty-check wrap (cpp:2358–2367) and the local `QXlsx::Document xlsx;` (cpp:2369) — the View owns them and passes `xlsx` in.
- **Delete** the `QSettings settings("MyCompany", "MyApp");` + `schoolName`/`address`/`librarian`/`position` reads (cpp:2372–2376) and replace with:
  ```cpp
  QString schoolName = info.schoolName;
  QString address    = info.address;
  QString librarian  = info.librarian;
  QString position   = info.position;
  ```
- Copy the body verbatim from the title-format setup (cpp:2378) through the footer writes (cpp:2466) — header rows, filters line, table headers, table rows, column widths, footer, "Prepared by".
- **Omit** the `saveAs` + success/failure `QMessageBox` (cpp:2469–2473) — the View calls `xlsx.saveAs` and reports. End with `return true;`.

- [ ] **Step 6: Write the failing test `qt-app/tests/tst_reportrenderer.cpp`**

Uses `QTEST_MAIN` (needs a QGuiApplication/QApplication for `QChartView` rendering) under `QT_QPA_PLATFORM=offscreen`. Asserts aggregation values exactly and chart images non-null at requested size (integration-level, same convention as the QXlsx-linked targets).

```cpp
#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>

#include "reportrenderer.h"

class TstReportRenderer : public QObject
{
    Q_OBJECT

private slots:
    void aggregateVisitsByCourse_sumsPerCourse();
    void aggregateVisitsByCourseHour_countsInWindow();
    void aggregateVisitsByCourseHour_excludesOutOfWindowAndInvalid();
    void makeBarChartImage_nonNullAtSize();
    void makePieChartImage_nonNullAtSize();
    void makeLineChartImage_nonNullAtSize();

private:
    static QJsonArray sampleVisits() {
        return QJsonArray{
            QJsonObject{{"course", "BSIT"}, {"visits", 3}, {"login_time", "08:15:00"}},
            QJsonObject{{"course", "BSIT"}, {"visits", 2}, {"login_time", "08:45:00"}},
            QJsonObject{{"course", "BSCS"}, {"visits", 5}, {"login_time", "10:00:00"}},
        };
    }
};

void TstReportRenderer::aggregateVisitsByCourse_sumsPerCourse() {
    const QMap<QString, int> got = ReportRenderer::aggregateVisitsByCourse(sampleVisits());
    QCOMPARE(got.value("BSIT"), 5);
    QCOMPARE(got.value("BSCS"), 5);
    QCOMPARE(got.size(), 2);
}

void TstReportRenderer::aggregateVisitsByCourseHour_countsInWindow() {
    const auto got = ReportRenderer::aggregateVisitsByCourseHour(sampleVisits(), 7, 21);
    QCOMPARE(got.value("BSIT").value(8), 2);  // two BSIT rows at hour 8
    QCOMPARE(got.value("BSCS").value(10), 1);
}

void TstReportRenderer::aggregateVisitsByCourseHour_excludesOutOfWindowAndInvalid() {
    QJsonArray data{
        QJsonObject{{"course", "BSIT"}, {"login_time", "05:00:00"}}, // before open (7)
        QJsonObject{{"course", "BSIT"}, {"login_time", "23:30:00"}}, // after close (21)
        QJsonObject{{"course", "BSIT"}, {"login_time", "not-a-time"}}, // invalid
        QJsonObject{{"course", "BSIT"}, {"login_time", "09:00:00"}}, // in window
    };
    const auto got = ReportRenderer::aggregateVisitsByCourseHour(data, 7, 21);
    QCOMPARE(got.value("BSIT").size(), 1);
    QCOMPARE(got.value("BSIT").value(9), 1);
}

void TstReportRenderer::makeBarChartImage_nonNullAtSize() {
    const ReportPalette pal = ReportPalette{
        QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
        { QColor("#1f77b4"), QColor("#ff7f0e") }};
    const QImage img = ReportRenderer::makeBarChartImage(sampleVisits(), QSize(400, 300), pal);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(400, 300));
}

void TstReportRenderer::makePieChartImage_nonNullAtSize() {
    const ReportPalette pal = ReportPalette{
        QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
        { QColor("#1f77b4"), QColor("#ff7f0e") }};
    const QImage img = ReportRenderer::makePieChartImage(sampleVisits(), QSize(300, 300), pal);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(300, 300));
}

void TstReportRenderer::makeLineChartImage_nonNullAtSize() {
    const ReportPalette pal = ReportPalette{
        QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
        { QColor("#1f77b4"), QColor("#ff7f0e") }};
    const QImage img = ReportRenderer::makeLineChartImage(sampleVisits(), QSize(400, 300), pal, 7, 21);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(400, 300));
}

QTEST_MAIN(TstReportRenderer)
#include "tst_reportrenderer.moc"
```

- [ ] **Step 7: Wire CMake — add renderer to `WITS` and register `tst_reportrenderer`**

In `qt-app/CMakeLists.txt`, add the renderer line to the `WITS` source list (next to the `reportcontroller` line added in Task 1):

```cmake
        reportrenderer.h reportrenderer.cpp
```

In `qt-app/tests/CMakeLists.txt`, append the 10th target:

```cmake
# 10th — Gui+Charts+QXlsx, offscreen (like tst_visitorcontroller). Does not link
# reportcontroller.cpp: the test builds ReportPalette literals directly and never
# calls getPalette.
qt_add_executable(tst_reportrenderer
    tst_reportrenderer.cpp
    ${CMAKE_SOURCE_DIR}/reportrenderer.cpp
    ${CMAKE_SOURCE_DIR}/reportrenderer.h
    ${CMAKE_SOURCE_DIR}/reportdata.h
)
set_target_properties(tst_reportrenderer PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_reportrenderer PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_reportrenderer PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Charts
    QXlsx
)
add_test(NAME tst_reportrenderer COMMAND tst_reportrenderer)
set_tests_properties(tst_reportrenderer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

> `reportrenderer.cpp` uses `QChartView` (Widgets) — if the link fails for a `QtWidgets` symbol, add `Qt${QT_VERSION_MAJOR}::Widgets` to this target's link libs. (`tst_visitorcontroller` is the reference for the QXlsx set; mirror whatever module list actually links.)

- [ ] **Step 8: Configure + build + run — verify GREEN**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R "tst_reportrenderer|tst_reportcontroller"
```
Expected: both targets pass. If the renderer test segfaults on `QChartView` under offscreen, confirm `QT_QPA_PLATFORM=offscreen` is set on the target (it is, above) and that `QTEST_MAIN` (not `GUILESS`) is used.

- [ ] **Step 9: Commit**

Use the `commit` skill. Stage by name:
```
qt-app/reportrenderer.h
qt-app/reportrenderer.cpp
qt-app/tests/tst_reportrenderer.cpp
qt-app/CMakeLists.txt
qt-app/tests/CMakeLists.txt
```
Suggested subject: `feat(report): add ReportRenderer (charts, PDF, Excel) + test target`

---

## Task 4: Wire both units into `adminWindow`; delete dead code; purity + no-dead-code gates

**Files:**
- Modify: `qt-app/adminwindow.h`
- Modify: `qt-app/adminwindow.cpp`

**Interfaces:**
- Consumes: everything Tasks 1–3 produced (`ReportController` API + signals, `ReportRenderer` statics, `reportdata.h` value types).
- Produces: the fully-delegated View. No new public interface.

This is the integration task. It touches many call sites; do it in the ordered steps below and build after each cluster.

- [ ] **Step 1: Move `ReportPalette` out of `adminwindow.h`; add includes + members**

In `qt-app/adminwindow.h`:
- **Delete** the `struct ReportPalette { … };` block (h:54–62).
- Add includes near the other controller includes (h:45 area):
  ```cpp
  #include "reportdata.h"
  #include "reportcontroller.h"
  #include "reportrenderer.h"
  ```
- Add members near `m_studentController` (h:105 area):
  ```cpp
  ReportController *m_reportController;   // child of this, created in ctor
  ReportRenderer    m_reportRenderer;     // stateless value member

  enum class ReportAction { None, Pdf, Excel, Print };
  ReportAction m_pendingReportAction = ReportAction::None;
  QJsonObject  m_pendingReportFilters;   // cached between fetchReportRows and onReportDataReady
  ```
- **Delete** the now-dead member decls: `getPalette` (h:109), `renderChartToImage` (h:141), `expandChartPlotArea` (h:142), `fetchReportData` (h:203).
- Keep `paintReport` (h:207) decl **only if** you keep a thin View wrapper; per this plan the View calls `m_reportRenderer.paintReport(...)` directly in the export wrappers, so **delete** the `paintReport` decl (h:207–208) and the `makeBarChartImage`/`makePieChartImage`/`makeLineChartImage` decls (h:152–154) too — they now live on `ReportRenderer`.
- Add the new View slot decls in the `private slots:` block:
  ```cpp
  void onReportDepartmentsLoaded(const QStringList &departments);
  void onReportYearsLoaded(const QStringList &years);
  void onReportCoursesLoaded(const QStringList &courses);
  void onReportDataReady(const QJsonArray &data);
  void onReportError(const QString &message, bool critical);
  void onPreviewDataReady(const QJsonArray &data);
  void onReportLoadError(const QString &title, const QString &message, bool critical);
  ```
  > The `onReport…` prefix is **mandatory**, not optional: `onDepartmentsLoaded` already exists at `adminwindow.h:193` (bound to `StudentController::departmentsLoaded` in the ctor at `adminwindow.cpp:456`) and `onCoursesLoaded` at h:194. Declaring the report slots as `onDepartmentsLoaded`/`onYearsLoaded` would collide/redeclare and break the build. Use `onReportDepartmentsLoaded` / `onReportYearsLoaded` / `onReportCoursesLoaded` exactly as shown here, and identically in Steps 2 (connects) and 4 (definitions).
- Add a header-info helper decl:
  ```cpp
  ReportHeaderInfo collectHeaderInfo() const;
  ```

- [ ] **Step 2: Construct the controller + connect signals in the ctor**

In `qt-app/adminwindow.cpp` ctor, after `networkManager` is created and alongside the other controllers (near the `m_studentController` construction), add:

```cpp
m_reportController = new ReportController(networkManager, this);

connect(m_reportController, &ReportController::departmentsLoaded,
        this, &adminWindow::onReportDepartmentsLoaded);
connect(m_reportController, &ReportController::yearsLoaded,
        this, &adminWindow::onReportYearsLoaded);
connect(m_reportController, &ReportController::coursesLoaded,
        this, &adminWindow::onReportCoursesLoaded);
connect(m_reportController, &ReportController::reportDataReady,
        this, &adminWindow::onReportDataReady);
connect(m_reportController, &ReportController::reportError,
        this, &adminWindow::onReportError);
connect(m_reportController, &ReportController::previewDataReady,
        this, &adminWindow::onPreviewDataReady);
connect(m_reportController, &ReportController::loadError,
        this, &adminWindow::onReportLoadError);
```

(`m_reportRenderer` is a value member — no construction needed.)

- [ ] **Step 3: Repoint the ctor's startup combo loads**

- At cpp:462, replace `loadFilterDepartments();` with `m_reportController->loadDepartments();`.
- At cpp:471, replace `loadAvailableYears();` with `m_reportController->loadYears();`.

- [ ] **Step 4: Add the View slots + `collectHeaderInfo` helper**

Add these definitions in `qt-app/adminwindow.cpp` (near the other report functions):

```cpp
ReportHeaderInfo adminWindow::collectHeaderInfo() const {
    QSettings settings("MyCompany", "MyApp");
    ReportHeaderInfo info;
    info.schoolName = settings.value("school/name", "Your School Name").toString();
    info.address    = settings.value("school/address", "Your Address").toString();
    info.logoPath   = settings.value("school/logoPath", "").toString();
    info.librarian  = settings.value("admin/name", "").toString();
    info.position   = settings.value("admin/position", "").toString();
    info.openHour   = settings.value("library/openHour", 7).toInt();
    info.closeHour  = settings.value("library/closeHour", 21).toInt();
    return info;
}

// Combo population (departments) — reproduces cpp:1842-1848.
void adminWindow::onReportDepartmentsLoaded(const QStringList &departments) {
    ui->filterDepartmentBox->clear();
    ui->filterDepartmentBox->addItem("-- Select Department --");
    for (const QString &d : departments)
        ui->filterDepartmentBox->addItem(d);
}

// Years -> both combos (reproduces cpp:1876-1883).
void adminWindow::onReportYearsLoaded(const QStringList &years) {
    ui->yearComboBox->clear();
    ui->semYearComboBox->clear();
    for (const QString &y : years) {
        ui->yearComboBox->addItem(y);
        ui->semYearComboBox->addItem(y);
    }
}

// Courses (reproduces cpp:1927-1932).
void adminWindow::onReportCoursesLoaded(const QStringList &courses) {
    ui->filterCourseBox->clear();
    for (const QString &c : courses)
        ui->filterCourseBox->addItem(c);
}

// Combo-load dialogs (rows 4/4a/5/6/7/8/9/10).
void adminWindow::onReportLoadError(const QString &title, const QString &message, bool critical) {
    if (critical)
        QMessageBox::critical(this, title, message);
    else
        QMessageBox::warning(this, title, message);
}

// Report-data error (rows 1/2/3), title "Error".
void adminWindow::onReportError(const QString &message, bool critical) {
    if (critical)
        QMessageBox::critical(this, "Error", message);
    else
        QMessageBox::warning(this, "Error", message);
    m_pendingReportAction = ReportAction::None;
}

// Preview refresh (reproduces fetchPreviewData success path cpp:2747).
void adminWindow::onPreviewDataReady(const QJsonArray &data) {
    updateChartsPreview(data);
}

// Export/print dispatch — replaces the three postReportData callbacks.
void adminWindow::onReportDataReady(const QJsonArray &data) {
    const QJsonObject filters = m_pendingReportFilters;
    const ReportAction action = m_pendingReportAction;
    m_pendingReportAction = ReportAction::None;

    switch (action) {
    case ReportAction::Pdf:   exportReportToPDF(data, filters);   break;
    case ReportAction::Excel: exportReportToExcel(data, filters); break;
    case ReportAction::Print: printReport(data, filters);         break;
    case ReportAction::None:  break;
    }
}
```

> `printReport(data, filters)` is a new small View helper carved from the old `onPrintReportBtnClicked` callback body (Step 7). Declare it in `adminwindow.h` `private:` as `void printReport(const QJsonArray &data, const QJsonObject &filters);`.
>
> **Forward-note (accepted, out of scope for this behavior-identical extraction):** the single `m_pendingReportAction` / `m_pendingReportFilters` cache coalesces concurrent export requests — if the user clicks Generate PDF then Generate Excel before the first `get_report_data.php` reply lands, only the last action dispatches (the first reply is dropped). Legacy captured each action in its own per-callback closure, so it would have run both. This is a real (if edge-case) behavior delta introduced by the locked one-cache seam design; the export buttons are not debounced today either, so it is not a regression that blocks this extraction. If it ever matters, debounce/disable the export buttons while a fetch is in flight, or key the cache by action — track as a follow-up, do not solve it here.

- [ ] **Step 5: Rewrite `collectReportFilters` to call `computeDateRange`**

Replace the duration `switch` in `adminWindow::collectReportFilters` (cpp:1731–1802) so the date math is delegated, while widget reads / index guards / dialogs / metadata fields stay View-side. The rest of the function (department, course, palette, chartType, schoolYear) is unchanged. Replace the switch block with:

```cpp
    // --- Duration switch (date math delegated to ReportController::computeDateRange) ---
    const int durationType = ui->durationTypeBox->currentIndex();
    switch (durationType) {
    case 0: { // Day
        DateRange r = ReportController::computeDateRange(
            0, ui->dateEdit->date(), 0, 0, QString(), 0, QDate(), QDate());
        if (!r.valid) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a valid date.");
            return {};
        }
        filters["start"] = r.start;
        filters["end"]   = r.end;
        filters["durationType"] = "day";
        break;
    }
    case 1: { // Month
        int monthIndex = ui->monthComboBox->currentIndex();
        int yearIndex  = ui->yearComboBox->currentIndex();
        if (monthIndex < 0 || yearIndex < 0) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a month and year.");
            return {};
        }
        int month = monthIndex + 1;
        int year  = ui->yearComboBox->currentText().toInt();
        DateRange r = ReportController::computeDateRange(
            1, QDate(), month, year, QString(), 0, QDate(), QDate());
        filters["start"] = r.start;
        filters["end"]   = r.end;
        filters["year"]  = year;
        filters["durationType"] = "month";
        break;
    }
    case 2: { // Semester
        int semIndex  = ui->semesterComboBox->currentIndex();
        int yearIndex = ui->semYearComboBox->currentIndex();
        if (semIndex < 0 || yearIndex < 0) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a semester and year.");
            return {};
        }
        int year = ui->semYearComboBox->currentText().toInt();
        QString sem = ui->semesterComboBox->currentText();
        DateRange r = ReportController::computeDateRange(
            2, QDate(), 0, 0, sem, year, QDate(), QDate());
        filters["start"]    = r.start;
        filters["end"]      = r.end;
        filters["semester"] = sem;
        filters["year"]     = year;
        filters["durationType"] = "semester";
        break;
    }
    case 3: { // Custom
        DateRange r = ReportController::computeDateRange(
            3, QDate(), 0, 0, QString(), 0,
            ui->startDateEdit->date(), ui->endDateEdit->date());
        if (!r.valid) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a valid date range.");
            return {};
        }
        filters["start"] = r.start;
        filters["end"]   = r.end;
        filters["durationType"] = "custom";
        break;
    }
    default:
        if (validateFilters)
            QMessageBox::warning(this, "Invalid Input", "Please select a duration type.");
        return {};
    }
```

(`collectReportFiltersForPreview` stays a thin `return collectReportFilters(false);` wrapper — unchanged.)

- [ ] **Step 6: Rewrite the PDF / Excel generate handlers to use the dispatch seam**

Replace `onGeneratePDFBtnClicked` (cpp:1677–1686):

```cpp
void adminWindow::onGeneratePDFBtnClicked() {
    QJsonObject filters = collectReportFilters(true);
    if (filters.isEmpty()) return;
    emit reportFiltersReady(filters);
    qDebug() << "Generate PDF filters:" << filters;

    m_pendingReportAction  = ReportAction::Pdf;
    m_pendingReportFilters = filters;
    m_reportController->fetchReportRows(filters);
}
```

Replace `onGenerateExcelBtnClicked` (cpp:1688–1698):

```cpp
void adminWindow::onGenerateExcelBtnClicked() {
    QJsonObject filters = collectReportFilters(true);
    if (filters.isEmpty()) return;
    emit reportFiltersReady(filters);
    qDebug() << "Generate Excel filters:" << filters;

    m_pendingReportAction  = ReportAction::Excel;
    m_pendingReportFilters = filters;
    m_reportController->fetchReportRows(filters);
}
```

- [ ] **Step 7: Rewrite the print handler + carve `printReport`**

Replace `onPrintReportBtnClicked` (cpp:2327–2354) with a filter-collect + fetch trigger, and add the `printReport` helper that holds the old callback body (the empty-data guard now lives in `printReport`, invoked from the `onReportDataReady` dispatch — do **not** duplicate it in the handler):

```cpp
void adminWindow::onPrintReportBtnClicked() {
    QJsonObject filters = collectReportFilters(true);
    if (filters.isEmpty()) return;
    emit reportFiltersReady(filters);
    qDebug() << "Print Report filters:" << filters;

    m_pendingReportAction  = ReportAction::Print;
    m_pendingReportFilters = filters;
    m_reportController->fetchReportRows(filters);
}

void adminWindow::printReport(const QJsonArray &data, const QJsonObject &filters) {
    if (data.isEmpty()) {
        QMessageBox::information(this, "No Data",
                                 "No data available for the selected filters. Please check your date range and department selection.");
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ReportHeaderInfo info = collectHeaderInfo();
    ReportPalette palette = ReportController::getPalette(filters["palette"].toString());
    if (!m_reportRenderer.paintReport(&printer, printer.resolution(), data, filters, palette, info)) {
        QMessageBox::critical(this, "Error", "Failed to start painting to printer.");
    }
}
```

- [ ] **Step 8: Rewrite `exportReportToPDF` and `exportReportToExcel` as renderer wrappers**

Replace `exportReportToPDF` (cpp:2293–2325) — keep the debug logs + empty guard + file dialog + `QPdfWriter` setup verbatim; swap the paint call:

```cpp
void adminWindow::exportReportToPDF(const QJsonArray &data, const QJsonObject &filters) {
    qDebug() << "=== PDF EXPORT DEBUG ===";
    qDebug() << "Data array size:" << data.size();
    qDebug() << "Data contents:" << data;
    qDebug() << "Filters received:" << filters;

    if (data.isEmpty()) {
        QMessageBox::information(this, "No Data",
                                 "No data available for the selected filters. Please check your date range and department selection.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Save Report", "", "PDF Files (*.pdf)");
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "Export Canceled", "No file selected.");
        return;
    }

    QPdfWriter pdf(filePath);
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(150);
    pdf.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    ReportHeaderInfo info = collectHeaderInfo();
    ReportPalette palette = ReportController::getPalette(filters["palette"].toString());
    if (!m_reportRenderer.paintReport(&pdf, 150, data, filters, palette, info)) {
        QMessageBox::critical(this, "Error", "Failed to open PDF for writing.");
        return;
    }

    QMessageBox::information(this, "Success", "Report exported to PDF successfully.");
}
```

Replace `exportReportToExcel` (cpp:2356–2474) — keep the empty guard + file dialog + `saveAs`/dialogs; delegate the body:

```cpp
void adminWindow::exportReportToExcel(const QJsonArray &rows, const QJsonObject &filters) {
    if (rows.isEmpty()) {
        QMessageBox::information(this, "No Data",
                                 "No data available for the selected filters.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Save Excel Report", "", "Excel Files (*.xlsx)");
    if (filePath.isEmpty())
        return;

    QXlsx::Document xlsx;
    ReportHeaderInfo info = collectHeaderInfo();
    m_reportRenderer.writeReportToXlsx(xlsx, rows, filters, info);

    if (!xlsx.saveAs(filePath)) {
        QMessageBox::critical(this, "Error", "Failed to save Excel file.");
    } else {
        QMessageBox::information(this, "Success", "Report exported to Excel successfully.");
    }
}
```

- [ ] **Step 9: Rewrite the three combo network handlers to delegate**

Replace `loadFilterDepartments` (cpp:1818–1853) body with a one-line delegate (keep the function so the ctor call — if you left it — resolves; but Step 3 repointed the ctor to `m_reportController->loadDepartments()`, so `loadFilterDepartments` is now **unused**). **Delete** `loadFilterDepartments`, `loadAvailableYears`, and rewrite `onFilterDepartmentBoxCurrentIndexChanged`:

- **Delete** `loadFilterDepartments` (cpp:1818–1853) and remove its decl (h:198).
- **Delete** `loadAvailableYears` (cpp:1855–1888) and remove its decl (h:206).
  > Confirm no other caller remains: `grep -n "loadFilterDepartments\|loadAvailableYears" qt-app/adminwindow.cpp`. Only the ctor (repointed in Step 3) should have referenced them.
- Replace `onFilterDepartmentBoxCurrentIndexChanged` (cpp:1890–1937) with:
  ```cpp
  void adminWindow::onFilterDepartmentBoxCurrentIndexChanged(int index) {
      if (index <= 0) {
          ui->filterCourseBox->clear();
          ui->filterCourseBox->addItem("All Courses");
          return;
      }
      m_reportController->loadCourses(ui->filterDepartmentBox->currentText());
  }
  ```

- [ ] **Step 10: Repoint the preview lambdas + the two surviving `getPalette` callers**

- In `connectFilterSignals` (cpp:2752–2811), replace every `fetchPreviewData(filters);` with `m_reportController->fetchPreviewData(filters);` (4 lambdas: department, course, duration, palette). The `updateChartsPreview(QJsonArray())` guard branches stay unchanged (View-side, no network).
- **Delete** the View's own `fetchPreviewData` member (cpp:2726–2750) and its decl (h:114) — the controller owns it now.
- Repoint the two surviving View-side `getPalette` callers to the static:
  - `updatePalettePreview` (cpp:2512): `ReportPalette palette = ReportController::getPalette(choice);`
  - `updateChartsPreview` (cpp:2556): change `getPalette(...)` to `ReportController::getPalette(...)`.

- [ ] **Step 11: Delete the dead code + the orphaned include**

- **Delete** `adminWindow::fetchReportData` (cpp:1939–2016) and its decl (h:203).
- **Delete** `adminWindow::renderChartToImage` (cpp:96–118) and its decl (h:141).
- **Delete** `adminWindow::expandChartPlotArea` (cpp:70–93) and its decl (h:142).
- **Delete** `adminWindow::paintReport` (cpp:2018–2291) — moved to `ReportRenderer` — and its decl (h:207–208).
- **Delete** `adminWindow::makeBarChartImage`/`makePieChartImage`/`makeLineChartImage` (cpp:123–… three functions) and their decls (h:152–154).
- **Delete** `adminWindow::getPalette` (cpp:2478–2508) and its decl (h:109).
- **Remove** `#include "reportpreviewdialog.h"` (cpp:55). **Keep** the `reportpreviewdialog.h`/`.cpp` files and their `CMakeLists.txt` entry (present-but-unused; deleting them is a documented follow-up).
- Remove now-unused report includes from `adminwindow.cpp` only if the compiler flags them unused (e.g. `<QtCharts/...>`, `QXlsx` headers may still be needed by `updateChartsPreview`/`updatePalettePreview` which stay View-side — **do not** remove those; the live preview still builds `QChartView` widgets). Remove an include only after confirming no remaining code in `adminwindow.cpp` uses it.

- [ ] **Step 12: Build the app + run the full test suite**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: `WITS.exe` builds with no new warnings; **10/10** tests pass.

- [ ] **Step 13: Purity + no-dead-code grep gates**

```bash
# ReportController purity — expect NO matches:
grep -nE "ui->|QMessageBox|QFileDialog|QChart|QChartView|QPainter|QXlsx|QSettings" qt-app/reportcontroller.cpp

# ReportRenderer purity — expect NO matches:
grep -nE "ui->|QMessageBox|QFileDialog|QSettings" qt-app/reportrenderer.cpp

# Dead code gone — expect NO matches (definitions or decls):
grep -rn "fetchReportData\|renderChartToImage\|expandChartPlotArea" qt-app/adminwindow.cpp qt-app/adminwindow.h

# adminWindow no longer POSTs/GETs report endpoints — expect NO matches:
grep -nE "get_report_data.php|get_departments.php|get_years.php|get_courses.php|api.php/reports/data" qt-app/adminwindow.cpp

# reportpreviewdialog include gone — expect NO match:
grep -n "reportpreviewdialog.h" qt-app/adminwindow.cpp

# adminwindow.ui untouched — expect it listed as modified ONLY if it was already
# (the user's pre-existing edit); this task must not add to it:
git status --porcelain qt-app/adminwindow.ui
```
> The `get_departments.php` grep may still match `adminWindow::loadDepartments()` (cpp:1629) and `populateFilters` — those belong to **other tabs** (Student/legacy) and are out of scope (spec "Out of Scope" bullet). Confirm each remaining match is a non-Reports caller before treating the gate as clean; the Reports handlers (`loadFilterDepartments`, `loadAvailableYears`, `onFilterDepartmentBoxCurrentIndexChanged`'s network half, `postReportData`, `fetchPreviewData`) must be gone.

- [ ] **Step 14: Manual smoke test (GUI)**

Run the **rebuilt** binary (not Qt Creator's build):
```powershell
& "qt-app/build/WITS.exe"
```
Verify on the Reports tab: startup populates the department combo (`-- Select Department --` + departments) and both year combos; selecting a department loads its courses (index 0 → `["All Courses"]`); Generate PDF / Excel / Print validate filters (rows 11–17), show the empty-data box when appropriate (rows 18/23), open the file/print dialogs, and produce the same PDF/Excel output + success/failure dialogs (rows 19–25); the live chart preview updates on combo changes (bar/pie/line) and is silent on preview-fetch errors; the palette swatch + chart colors match the selected palette.

- [ ] **Step 15: Commit**

Use the `commit` skill. Stage by name (**never** `qt-app/adminwindow.ui`):
```
qt-app/adminwindow.h
qt-app/adminwindow.cpp
```
Suggested subject: `refactor(report): delegate Reports tab to ReportController + ReportRenderer`

---

## Post-implementation (final gate)

After Task 4, follow the workflow rule: run `/claude-review` (whole-branch), fix Critical/Important findings, then the `create-pr` gate (dry-checker + security-reviewer + general-code-reviewer, diff-scoped) → QA on the rebuilt `WITS.exe` → open the PR. `adminwindow.ui` stays out of every commit.

## Self-Review notes (checked against the spec)

- **Spec coverage:** value types (Task 1) · all 7 pure statics (Task 1) · 5 network methods (Task 2) · 2 aggregation statics + 3 chart makers + `paintReport` + `writeReportToXlsx` (Task 3) · export-dispatch seam + preview seam + combo-population seam + dead-code deletion + palette-caller repoint + purity/dead-code gates (Task 4) · both CMake targets (Tasks 1 & 3). Every Message/Dialog Inventory row (1–25, incl. 4a) is owned by a named function.
- **Deviation (Plan Note A):** `reportError`/`loadError` carry a `bool critical` to preserve the inventory's critical-vs-warning icons — the only departure from the spec's signal sketch.
- **Type consistency:** `computeDateRange` signature, the `ReportAction` enum, and all slot names are used identically across Tasks 1 and 4. Slot names are guarded against the existing Student-tab `onDepartmentsLoaded`/`onCoursesLoaded` (Step 1 note) — use the `onReport…` prefix throughout.
