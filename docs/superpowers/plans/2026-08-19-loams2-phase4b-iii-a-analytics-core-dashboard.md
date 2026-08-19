# Phase 4b-iii-a — Reporting Analytics Core + On-Screen Dashboard — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the Reporting screen an analytics-first dashboard — a KPI band and Top-10 rankings (students / courses / departments) computed client-side over the already-fetched rows — without touching the export renderer (that is slice 4b-iii-b).

**Architecture:** A new **presentation-agnostic** `witscore` aggregator (`ReportAnalytics::compute`) turns normalized report rows into plain value types (`ReportKpis` + `RankingEntry` lists). The `ReportingViewModel` calls it once per result and exposes KPIs as scalar `Q_PROPERTY`s and rankings via a new `RankingModel : QAbstractListModel`; `ReportingScreen.qml` renders the dashboard grid and demotes the full roster behind a "View full roster" toggle. One computed truth, consumed only by QML in this slice.

**Tech Stack:** Qt 6.11 / C++17, QML (URI `LOAMS`), QtTest + Qt Quick Test under ctest, CMake + Ninja.

## Global Constraints

- **Spec:** `docs/superpowers/specs/2026-08-18-loams2-phase4b-iii-reporting-analytics-design.md` (claude-review APPROVED, 2 rounds). This slice is **4b-iii-a ONLY** — NO changes to `reportrenderer.{h,cpp}` or the export path.
- **MVVM:** `ReportingViewModel` is the ONLY QML-facing C++. KPIs are **scalar `Q_PROPERTY`s** (not a `Q_GADGET`). Aggregators are pure statics in witscore; QML never calls them directly.
- **Aggregator purity:** `ReportAnalytics` lives in `qt-app/core/` (witscore), is `static`, network-free, and unit-tested directly. It **cannot** include `quick/models/ReportRowsModel.h`, so it does **not** call `reportVisits()` — its input contract is **already-normalized rows (numeric `visits`)**; a raw-string `visits` is a caller error.
- **Theme:** all visual tokens via `Theme.qml`; ZERO raw hex outside `Theme.qml`; `Qt.alpha(Theme.<token>, a)` for opacity.
- **PlainText:** every server/name-derived text (KPI values, ranking-row labels) renders `Text.PlainText` (cleartext HTTP).
- **No real student PII** in any test/fixture — synthetic names/IDs only.
- **Build (PowerShell; Qt tools NOT on PATH; external short build dir avoids Windows MAX_PATH on the QML module):**
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B C:/b/loams-4biii -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build C:/b/loams-4biii
  ctest --test-dir C:/b/loams-4biii --output-on-failure
  ```
  Baseline at branch start: **42/42 green**. Ignore the "LF will be replaced by CRLF" and the pre-existing QXlsx "GuiPrivate target" warnings. `WITSQuick.exe` may need a running instance closed before it relinks.

## File Structure

- **Create** `qt-app/core/reportanalytics.h` / `.cpp` — value types (`ReportKpis`, `RankingEntry`, `ReportAnalytics`) + `static ReportAnalytics compute(const QJsonArray&, int topN=10)`.
- **Modify** `qt-app/core/CMakeLists.txt` — add `reportanalytics.h reportanalytics.cpp` (bare, un-prefixed) to the `witscore` `add_library(...)` list, next to `reportrenderer.h reportrenderer.cpp` (line ~36). **This is where `witscore` is defined — NOT `qt-app/CMakeLists.txt`.** Without it, `witsquickmodule` and `tst_reportingviewmodel` fail to *link* (`undefined reference to ReportAnalytics::compute`).
- **Create** `qt-app/tests/tst_reportanalytics.cpp` — pure-core unit tests; register in `qt-app/tests/CMakeLists.txt` via `wits_add_qttest`.
- **Create** `qt-app/quick/models/RankingModel.h` / `.cpp` — `QAbstractListModel` for a ranking table.
- **Modify** `qt-app/quick/viewmodels/ReportingViewModel.h` / `.cpp` — new KPI `Q_PROPERTY`s + three `RankingModel` members; compute `ReportAnalytics` in `applyResult`.
- **Modify** `qt-app/quick/tests/tst_reportingviewmodel.cpp` — VM analytics tests.
- **Modify** `qt-app/quick/qml/admin/ReportingScreen.qml` — 4-tile KPI band (with a zero-row "No report data" state) + three ranking tables + "View full roster" toggle.
- **Modify** `qt-app/quick/tests/tst_qml_admin.qml` — dashboard QuickTests (extend the reportingStub; the reporting fixtures are statically instantiated — see Task 5).
- **Modify** `qt-app/quick/CMakeLists.txt` — add `models/RankingModel.{h,cpp}` to the QML module sources.

---

### Task 1: Core aggregator — value types + KPIs

**Files:**
- Create: `qt-app/core/reportanalytics.h`, `qt-app/core/reportanalytics.cpp`
- Create/Test: `qt-app/tests/tst_reportanalytics.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (register `tst_reportanalytics`)

**Interfaces:**
- Produces: `struct ReportKpis`, `struct RankingEntry`, `struct ReportAnalytics`; `static ReportAnalytics ReportAnalytics::compute(const QJsonArray &normalizedRows, int topN = 10)`. Rankings are filled in Task 2 — this task delivers the value types + KPI fields.

- [ ] **Step 1: Write the header**

`qt-app/core/reportanalytics.h`:
```cpp
#ifndef REPORTANALYTICS_H
#define REPORTANALYTICS_H

#include <QJsonArray>
#include <QList>
#include <QString>

// KPI band values ("How much?"). Presentation-agnostic — no formatting, no QML.
struct ReportKpis {
    int     totalVisits         = 0;   // Σ visits
    int     uniqueVisitors      = 0;   // count of distinct school_id groups
    double  avgVisitsPerVisitor = 0.0; // totalVisits / uniqueVisitors (0 when none)
    QString topDepartment;             // busiest department ("" when no data)
    int     topDepartmentVisits = 0;
    bool    hasData             = false; // false when zero rows -> empty-states
};

// One ranked row ("Who?"/"Which?").
struct RankingEntry {
    int     rank           = 0;   // 1-based
    QString label;                // student name / course / department
    QString sublabel;             // students: their course; else ""
    int     visits         = 0;
    double  percentOfTotal = 0.0; // visits / totalVisits * 100 (0 when total 0)
};

struct ReportAnalytics {
    ReportKpis          kpis;
    QList<RankingEntry> topStudents;
    QList<RankingEntry> topCourses;
    QList<RankingEntry> topDepartments;

    // The one entry point. `normalizedRows` MUST carry NUMERIC `visits`
    // (4b-ii's normalizeExportRows is the normalizer). A raw-string `visits`
    // is a caller error and counts as 0. Aggregation is BY KEY, summing visits:
    // students group on school_id, courses on course, departments on department.
    // Blank/whitespace name/course/department normalize to "(Unspecified)".
    static ReportAnalytics compute(const QJsonArray &normalizedRows, int topN = 10);
};

#endif // REPORTANALYTICS_H
```

- [ ] **Step 2: Write the failing KPI tests**

`qt-app/tests/tst_reportanalytics.cpp`:
```cpp
#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include "reportanalytics.h"

class TstReportAnalytics : public QObject
{
    Q_OBJECT
private slots:
    void empty_hasNoData();
    void kpis_sumUniqueAvgTopDept();
    void kpis_oneStudent();
    void kpis_blankDepartmentBucketsUnspecified();
    void kpis_rawStringVisitsIsCallerError();

private:
    // Synthetic rows only. `visits` is numeric (already normalized).
    static QJsonObject row(const QString &id, const QString &name, const QString &course,
                           const QString &dept, int visits) {
        return QJsonObject{{"school_id", id}, {"name", name}, {"course", course},
                           {"department", dept}, {"visits", visits}};
    }
};

void TstReportAnalytics::empty_hasNoData() {
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{});
    QVERIFY(!a.kpis.hasData);
    QCOMPARE(a.kpis.totalVisits, 0);
    QCOMPARE(a.kpis.uniqueVisitors, 0);
    QVERIFY(a.topStudents.isEmpty());
    QVERIFY(a.topDepartments.isEmpty());
}

void TstReportAnalytics::kpis_sumUniqueAvgTopDept() {
    const QJsonArray rows{
        row("1", "Ana",  "BSIT",  "CCS", 5),
        row("2", "Ben",  "BSCS",  "CCS", 3),
        row("3", "Cara", "BSEcE", "CoE", 4),
    };
    const ReportAnalytics a = ReportAnalytics::compute(rows);
    QVERIFY(a.kpis.hasData);
    QCOMPARE(a.kpis.totalVisits, 12);
    QCOMPARE(a.kpis.uniqueVisitors, 3);
    QCOMPARE(a.kpis.avgVisitsPerVisitor, 4.0);
    QCOMPARE(a.kpis.topDepartment, QStringLiteral("CCS")); // 8 > CoE 4
    QCOMPARE(a.kpis.topDepartmentVisits, 8);
}

void TstReportAnalytics::kpis_oneStudent() {
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{ row("1","Ana","BSIT","CCS",7) });
    QVERIFY(a.kpis.hasData);
    QCOMPARE(a.kpis.totalVisits, 7);
    QCOMPARE(a.kpis.uniqueVisitors, 1);
    QCOMPARE(a.kpis.avgVisitsPerVisitor, 7.0);
    QCOMPARE(a.kpis.topDepartment, QStringLiteral("CCS"));
}

void TstReportAnalytics::kpis_blankDepartmentBucketsUnspecified() {
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{ row("1","Ana","BSIT","",9) });
    QCOMPARE(a.kpis.topDepartment, QStringLiteral("(Unspecified)"));
    QCOMPARE(a.kpis.topDepartmentVisits, 9);
}

void TstReportAnalytics::kpis_rawStringVisitsIsCallerError() {
    // Contract: numeric visits required; a raw string counts as 0 (QJsonValue::toInt()).
    QJsonObject bad{{"school_id","1"},{"name","Ana"},{"course","BSIT"},
                    {"department","CCS"},{"visits","5"}};  // STRING, not int
    const ReportAnalytics a = ReportAnalytics::compute(QJsonArray{ bad });
    QCOMPARE(a.kpis.totalVisits, 0);
}

QTEST_APPLESS_MAIN(TstReportAnalytics)
#include "tst_reportanalytics.moc"
```

- [ ] **Step 3: Register the test (fails to build until compute exists)**

Add to `qt-app/tests/CMakeLists.txt` (after the other `wits_add_qttest` blocks, ~line 253):
```cmake
# --- Reporting analytics aggregator (pure core, no offscreen) ---
wits_add_qttest(tst_reportanalytics
    SOURCES
        tst_reportanalytics.cpp
        ${CMAKE_SOURCE_DIR}/core/reportanalytics.cpp
        ${CMAKE_SOURCE_DIR}/core/reportanalytics.h
    INCLUDES ${CMAKE_SOURCE_DIR}/core)
```

- [ ] **Step 4: Run — verify RED**

Run: `cmake -S qt-app -B C:/b/loams-4biii -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"` then `cmake --build C:/b/loams-4biii --target tst_reportanalytics`
Expected: FAIL — `reportanalytics.cpp` does not exist / `compute` unresolved.

- [ ] **Step 5: Implement `compute` (KPIs; rankings stubbed empty for now)**

`qt-app/core/reportanalytics.cpp`:
```cpp
#include "reportanalytics.h"

#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace {
QString orUnspecified(const QString &s) {
    const QString t = s.trimmed();
    return t.isEmpty() ? QStringLiteral("(Unspecified)") : t;
}
// A pre-ranking group: a display label (+ optional sublabel) and summed visits.
struct Group { QString label; QString sublabel; int visits = 0; };

// Rank a group list into `out`: descending by visits, ties broken alphabetically
// by label (deterministic), keep the top `topN`, assign 1-based rank + percent.
void rankInto(QList<RankingEntry> &out, QList<Group> groups, int totalVisits, int topN) {
    std::stable_sort(groups.begin(), groups.end(), [](const Group &a, const Group &b) {
        if (a.visits != b.visits) return a.visits > b.visits;
        return a.label < b.label;
    });
    const int n = qMin(topN, int(groups.size()));
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        RankingEntry e;
        e.rank = i + 1;
        e.label = groups[i].label;
        e.sublabel = groups[i].sublabel;
        e.visits = groups[i].visits;
        e.percentOfTotal = totalVisits > 0 ? (100.0 * groups[i].visits / totalVisits) : 0.0;
        out.append(e);
    }
}
} // namespace

ReportAnalytics ReportAnalytics::compute(const QJsonArray &rows, int topN) {
    ReportAnalytics a;
    if (rows.isEmpty())
        return a;                 // hasData=false, all lists empty
    a.kpis.hasData = true;

    QMap<QString, Group> students;      // key: school_id
    QMap<QString, int>   courses;       // key: course label
    QMap<QString, int>   departments;   // key: department label
    QSet<QString>        ids;
    int total = 0;

    for (const QJsonValue &v : rows) {
        const QJsonObject o = v.toObject();
        const int visits = o.value("visits").toInt();   // numeric per contract; string -> 0
        total += visits;

        const QString id = o.value("school_id").toString();
        ids.insert(id);
        Group &g = students[id];
        if (g.label.isEmpty()) {
            g.label = orUnspecified(o.value("name").toString());
            g.sublabel = orUnspecified(o.value("course").toString());
        }
        g.visits += visits;

        courses[orUnspecified(o.value("course").toString())] += visits;
        departments[orUnspecified(o.value("department").toString())] += visits;
    }

    a.kpis.totalVisits = total;
    a.kpis.uniqueVisitors = ids.size();
    a.kpis.avgVisitsPerVisitor = ids.isEmpty() ? 0.0 : double(total) / ids.size();

    // Rankings (Task 2 asserts these in detail; computed here so the KPI
    // top-department can reuse the tie-broken department ranking).
    QList<Group> stu = students.values();
    QList<Group> crs, dep;
    for (auto it = courses.cbegin(); it != courses.cend(); ++it)
        crs.append({ it.key(), QString(), it.value() });
    for (auto it = departments.cbegin(); it != departments.cend(); ++it)
        dep.append({ it.key(), QString(), it.value() });

    rankInto(a.topStudents, stu, total, topN);
    rankInto(a.topCourses, crs, total, topN);
    rankInto(a.topDepartments, dep, total, topN);

    if (!a.topDepartments.isEmpty()) {
        a.kpis.topDepartment = a.topDepartments.first().label;
        a.kpis.topDepartmentVisits = a.topDepartments.first().visits;
    }
    return a;
}
```

- [ ] **Step 6: Run — verify GREEN**

Run: `cmake --build C:/b/loams-4biii --target tst_reportanalytics` then `ctest --test-dir C:/b/loams-4biii -R tst_reportanalytics --output-on-failure`
Expected: PASS (5/5).

- [ ] **Step 7: Commit**

```bash
git add qt-app/core/reportanalytics.h qt-app/core/reportanalytics.cpp qt-app/tests/tst_reportanalytics.cpp qt-app/tests/CMakeLists.txt
git commit -m "feat(reporting): ReportAnalytics core aggregator — KPIs"
```

---

### Task 2: Core aggregator — rankings

**Files:**
- Modify/Test: `qt-app/tests/tst_reportanalytics.cpp` (add ranking tests)
- (Implementation already present from Task 1 — this task PINS the ranking behavior with tests; only touch `reportanalytics.cpp` if a test exposes a gap.)

**Interfaces:**
- Consumes: `ReportAnalytics::compute` (Task 1).
- Produces: verified `topStudents` / `topCourses` / `topDepartments` semantics (top-N, %, alpha tie-break) that Task 4's `RankingModel` wiring relies on.

- [ ] **Step 1: Add the failing ranking tests**

Add these slots to `TstReportAnalytics` (declare in `private slots:` and implement):
```cpp
void ranks_coursesDescendingWithPercent();
void ranks_alphabeticalTieBreak();
void ranks_topNCapAndFewerThanN();
void ranks_studentsCarryCourseSublabel();
```
Implementations:
```cpp
void TstReportAnalytics::ranks_coursesDescendingWithPercent() {
    const QJsonArray rows{
        row("1","Ana","BSIT","CCS",6), row("2","Ben","BSCS","CCS",3),
        row("3","Cara","BSIT","CCS",1),  // BSIT total 7, BSCS 3, total 10
    };
    const ReportAnalytics a = ReportAnalytics::compute(rows);
    QCOMPARE(a.topCourses.size(), 2);
    QCOMPARE(a.topCourses[0].label, QStringLiteral("BSIT"));
    QCOMPARE(a.topCourses[0].rank, 1);
    QCOMPARE(a.topCourses[0].visits, 7);
    QCOMPARE(a.topCourses[0].percentOfTotal, 70.0);
    QCOMPARE(a.topCourses[1].label, QStringLiteral("BSCS"));
    QCOMPARE(a.topCourses[1].rank, 2);
}

void TstReportAnalytics::ranks_alphabeticalTieBreak() {
    // Equal visits -> alphabetical by label, deterministic.
    const QJsonArray rows{
        row("1","Ana","Zeta","D",5), row("2","Ben","Alpha","D",5),
    };
    const ReportAnalytics a = ReportAnalytics::compute(rows);
    QCOMPARE(a.topCourses[0].label, QStringLiteral("Alpha"));
    QCOMPARE(a.topCourses[1].label, QStringLiteral("Zeta"));
}

void TstReportAnalytics::ranks_topNCapAndFewerThanN() {
    QJsonArray rows;
    for (int i = 0; i < 12; ++i)
        rows.append(row(QString::number(i), QStringLiteral("S%1").arg(i),
                        QStringLiteral("C%1").arg(i), "D", i + 1));
    const ReportAnalytics a = ReportAnalytics::compute(rows, 10);
    QCOMPARE(a.topStudents.size(), 10);                 // capped at N
    QCOMPARE(a.topStudents.first().visits, 12);         // highest first
    QCOMPARE(a.topStudents.first().rank, 1);
    const ReportAnalytics few = ReportAnalytics::compute(
        QJsonArray{ row("1","Ana","BSIT","D",3) }, 10);
    QCOMPARE(few.topStudents.size(), 1);                // fewer than N -> show what exists
}

void TstReportAnalytics::ranks_studentsCarryCourseSublabel() {
    const ReportAnalytics a = ReportAnalytics::compute(
        QJsonArray{ row("1","Ana Cruz","BSIT","CCS",4) });
    QCOMPARE(a.topStudents.first().label, QStringLiteral("Ana Cruz"));
    QCOMPARE(a.topStudents.first().sublabel, QStringLiteral("BSIT"));
}
```

- [ ] **Step 2: Run — verify (expect PASS; Task 1 already implements rankings)**

Run: `cmake --build C:/b/loams-4biii --target tst_reportanalytics` then `ctest --test-dir C:/b/loams-4biii -R tst_reportanalytics --output-on-failure`
Expected: PASS (9/9). If any ranking test FAILS, fix `rankInto`/`compute` in `reportanalytics.cpp` minimally until green (do not touch KPI logic).

- [ ] **Step 3: Commit**

```bash
git add qt-app/tests/tst_reportanalytics.cpp qt-app/core/reportanalytics.cpp
git commit -m "test(reporting): pin ReportAnalytics ranking semantics"
```

---

### Task 3: `RankingModel` (QAbstractListModel)

**Files:**
- Create: `qt-app/quick/models/RankingModel.h`, `qt-app/quick/models/RankingModel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add the two files to the `witsquickmodule` sources)
- Test: covered via the VM tests in Task 4 (the model is exercised through `ReportingViewModel`; a standalone model test is optional and not required here).

**Interfaces:**
- Produces: `class RankingModel : public QAbstractListModel` with `void setEntries(const QList<RankingEntry> &)`, roles `RankRole`/`LabelRole`/`SublabelRole`/`VisitsRole`/`PercentRole`, and `int count()` (`Q_PROPERTY count NOTIFY countChanged`). Consumed by Task 4.

- [ ] **Step 1: Write the header**

`qt-app/quick/models/RankingModel.h`:
```cpp
#ifndef RANKINGMODEL_H
#define RANKINGMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "reportanalytics.h"   // RankingEntry

// One ranking table (Top 10 Students / Courses / Departments). Mirrors the
// BarsModel/ReportRowsModel pattern: roles enum, roleNames(), count property.
// Fed a QList<RankingEntry> computed by ReportAnalytics; no logic of its own.
class RankingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles { RankRole = Qt::UserRole + 1, LabelRole, SublabelRole, VisitsRole, PercentRole };
    explicit RankingModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_entries.size()); }
    void setEntries(const QList<RankingEntry> &entries);

signals:
    void countChanged();

private:
    QList<RankingEntry> m_entries;
};

#endif // RANKINGMODEL_H
```

- [ ] **Step 2: Write the implementation**

`qt-app/quick/models/RankingModel.cpp`:
```cpp
#include "RankingModel.h"

RankingModel::RankingModel(QObject *parent) : QAbstractListModel(parent) {}

int RankingModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant RankingModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const RankingEntry &e = m_entries.at(index.row());
    switch (role) {
    case RankRole:     return e.rank;
    case LabelRole:    return e.label;
    case SublabelRole: return e.sublabel;
    case VisitsRole:   return e.visits;
    case PercentRole:  return e.percentOfTotal;
    default:           return {};
    }
}

QHash<int, QByteArray> RankingModel::roleNames() const {
    return { {RankRole, "rank"}, {LabelRole, "label"}, {SublabelRole, "sublabel"},
             {VisitsRole, "visits"}, {PercentRole, "percent"} };
}

void RankingModel::setEntries(const QList<RankingEntry> &entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
    emit countChanged();
}
```

- [ ] **Step 3: Register in the QML module**

In `qt-app/quick/CMakeLists.txt`, add to the `witsquickmodule` source list (next to `models/BarsModel.cpp` / `models/ReportRowsModel.cpp`):
```cmake
        models/RankingModel.cpp
        models/RankingModel.h
```
Also compile the aggregator into `witscore`. `witscore` is defined in **`qt-app/core/CMakeLists.txt`** (`add_library(witscore STATIC …)`, line ~19), and its entries are **bare / un-prefixed**. Add to that `add_library` list, next to `reportrenderer.h reportrenderer.cpp` (line ~36):
```cmake
    reportanalytics.h reportanalytics.cpp
```
> NOT `qt-app/CMakeLists.txt`, and NOT `core/reportanalytics.cpp`. Both `witsquickmodule` and `tst_reportingviewmodel` link `witscore`, so this is the single place the symbol must live. Skipping it produces a link error, not a compile error — the header alone won't flag it.

- [ ] **Step 4: Build — verify it compiles into the module**

Run: `cmake -S qt-app -B C:/b/loams-4biii -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"` then `cmake --build C:/b/loams-4biii --target witsquickmodule`
Expected: builds clean (no test yet — behavior verified in Task 4).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/models/RankingModel.h qt-app/quick/models/RankingModel.cpp qt-app/quick/CMakeLists.txt qt-app/core/CMakeLists.txt
git commit -m "feat(reporting): RankingModel for the analytics ranking tables"
```

---

### Task 4: ViewModel wiring — KPI properties + ranking models

**Files:**
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h`, `qt-app/quick/viewmodels/ReportingViewModel.cpp`
- Test: `qt-app/quick/tests/tst_reportingviewmodel.cpp`

**Interfaces:**
- Consumes: `ReportAnalytics::compute` (Task 1/2), `RankingModel` (Task 3).
- Produces (QML-facing, scalar `Q_PROPERTY`s + models): `int uniqueVisitors`, `double avgVisitsPerVisitor`, `QString topDepartment`, `int topDepartmentVisits` (all `NOTIFY resultChanged`), plus `RankingModel* topStudents`/`topCourses`/`topDepartments` (`CONSTANT`). The existing `totalVisits` stays.

- [ ] **Step 1: Write the failing VM test**

Add to `qt-app/quick/tests/tst_reportingviewmodel.cpp` (a new slot; register it in `private slots:`). Follow the file's existing pattern for driving `onReportDataReady` with a synthetic array:
```cpp
void analytics_populatedFromResult() {
    ReportingViewModel vm;
    QJsonArray rows{
        QJsonObject{{"school_id","1"},{"name","Ana"},{"course","BSIT"},
                    {"department","CCS"},{"year_level","1"},{"visits",5}},
        QJsonObject{{"school_id","2"},{"name","Ben"},{"course","BSCS"},
                    {"department","CCS"},{"year_level","1"},{"visits",3}},
    };
    vm.onReportDataReady(rows);                       // same seam the controller signal uses

    QCOMPARE(vm.uniqueVisitors(), 2);
    QCOMPARE(vm.avgVisitsPerVisitor(), 4.0);
    QCOMPARE(vm.topDepartment(), QStringLiteral("CCS"));
    QCOMPARE(vm.topDepartmentVisits(), 8);
    QVERIFY(vm.topStudents() != nullptr);
    QCOMPARE(vm.topStudents()->count(), 2);
    QCOMPARE(vm.topCourses()->count(), 2);
    QCOMPARE(vm.topDepartments()->count(), 1);        // single dept CCS
}
```
> Note: `onReportDataReady` normalizes rows via the existing path; if `applyResult` does not already normalize, pass `visits` as numbers (above) — the analytics contract requires numeric visits, and `m_exportRows` is normalized in `applyResult`.

- [ ] **Step 2: Run — verify RED**

Run: `cmake --build C:/b/loams-4biii --target tst_reportingviewmodel`
Expected: FAIL — `uniqueVisitors()`/`topStudents()` not members.

- [ ] **Step 3: Extend the header**

In `ReportingViewModel.h`: add includes `#include "RankingModel.h"`; add `Q_PROPERTY`s near the existing `totalVisits`/`studentsShown`:
```cpp
    Q_PROPERTY(int uniqueVisitors READ uniqueVisitors NOTIFY resultChanged)
    Q_PROPERTY(double avgVisitsPerVisitor READ avgVisitsPerVisitor NOTIFY resultChanged)
    Q_PROPERTY(QString topDepartment READ topDepartment NOTIFY resultChanged)
    Q_PROPERTY(int topDepartmentVisits READ topDepartmentVisits NOTIFY resultChanged)
    Q_PROPERTY(RankingModel *topStudents READ topStudents CONSTANT)
    Q_PROPERTY(RankingModel *topCourses READ topCourses CONSTANT)
    Q_PROPERTY(RankingModel *topDepartments READ topDepartments CONSTANT)
```
Add accessors:
```cpp
    int uniqueVisitors() const { return m_uniqueVisitors; }
    double avgVisitsPerVisitor() const { return m_avgVisitsPerVisitor; }
    QString topDepartment() const { return m_topDepartment; }
    int topDepartmentVisits() const { return m_topDepartmentVisits; }
    RankingModel *topStudents() { return &m_topStudents; }
    RankingModel *topCourses() { return &m_topCourses; }
    RankingModel *topDepartments() { return &m_topDepartments; }
```
Add members (private):
```cpp
    int m_uniqueVisitors = 0;
    double m_avgVisitsPerVisitor = 0.0;
    QString m_topDepartment = QStringLiteral("—");
    int m_topDepartmentVisits = 0;
    RankingModel m_topStudents, m_topCourses, m_topDepartments;
```

- [ ] **Step 4: Compute analytics in `applyResult`**

In `ReportingViewModel.cpp`, add `#include "reportanalytics.h"`. In `applyResult(...)` (the method that stores `m_exportRows` and sets the tiles), after the rows are normalized into `m_exportRows`, add:
```cpp
    const ReportAnalytics an = ReportAnalytics::compute(m_exportRows);
    m_uniqueVisitors = an.kpis.uniqueVisitors;
    m_avgVisitsPerVisitor = an.kpis.avgVisitsPerVisitor;
    m_topDepartment = an.kpis.hasData ? an.kpis.topDepartment : QStringLiteral("—");
    m_topDepartmentVisits = an.kpis.topDepartmentVisits;
    m_topStudents.setEntries(an.topStudents);
    m_topCourses.setEntries(an.topCourses);
    m_topDepartments.setEntries(an.topDepartments);
```
> `m_totalVisits` is already set by the existing `deriveTiles` path — leave it. `resultChanged()` is already emitted at the end of `applyResult`; the new KPI properties notify on it.

- [ ] **Step 5: Run — verify GREEN**

Run: `cmake --build C:/b/loams-4biii --target tst_reportingviewmodel` then `ctest --test-dir C:/b/loams-4biii -R tst_reportingviewmodel --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add qt-app/quick/viewmodels/ReportingViewModel.h qt-app/quick/viewmodels/ReportingViewModel.cpp qt-app/quick/tests/tst_reportingviewmodel.cpp
git commit -m "feat(reporting): VM exposes KPI properties + ranking models"
```

---

### Task 5: Dashboard screen + full-roster toggle + QuickTests

**Files:**
- Modify: `qt-app/quick/qml/admin/ReportingScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: VM `uniqueVisitors`/`avgVisitsPerVisitor`/`topDepartment`/`topDepartmentVisits` + `topStudents`/`topCourses`/`topDepartments` (Task 4).

- [ ] **Step 1: Add failing QuickTests (extend the reportingStub + assertions)**

The reporting fixtures are **statically instantiated**, not created per-test. In `tst_qml_admin.qml` the existing seam is `ReportingScreen { id: reporting; x:0; y:7300; width:1100; height:1000; vm: reportingStub }` (line ~2571) plus a vm-less `vmlessReporting` (line ~2574); tests drive it via `findChild(reporting, "objectName")` inside the `TestCase { name: "ReportingScreen" }` block. There is **no** `createTemporaryObject`/`reportingComponent`/`testRoot` — do NOT introduce them.

First, extend the existing `reportingStub` (the `QtObject` at line ~2515) with the new VM surface — scalar KPI props + three ranking models (plain `ListModel`s with role-named rows; `ListModel` already has a `count`):
```qml
        property int uniqueVisitors: 2
        property real avgVisitsPerVisitor: 4.0
        property string topDepartment: "CCS"
        property int topDepartmentVisits: 8
        property var topStudents: topStudentsStub
        property var topCourses: topCoursesStub
        property var topDepartments: topDepartmentsStub
```
Add the three backing models next to `reportRowsStub` (line ~2505):
```qml
    ListModel { id: topStudentsStub
        ListElement { rank: 1; label: "Ana"; sublabel: "BSIT"; visits: 5; percent: 62.5 }
        ListElement { rank: 2; label: "Ben"; sublabel: "BSCS"; visits: 3; percent: 37.5 }
    }
    ListModel { id: topCoursesStub
        ListElement { rank: 1; label: "BSIT"; sublabel: ""; visits: 5; percent: 62.5 }
        ListElement { rank: 2; label: "BSCS"; sublabel: ""; visits: 3; percent: 37.5 }
    }
    ListModel { id: topDepartmentsStub
        ListElement { rank: 1; label: "CCS"; sublabel: ""; visits: 8; percent: 100.0 }
    }
```
Then add a test to the `TestCase { name: "ReportingScreen" }` block. `init()` re-populates `reportRowsStub` when empty, so rows start non-empty:
```qml
function test_dashboard_showsKpisAndRankings() {
    reportingStub.hasResult = true;
    var uniqueTile = findChild(reporting, "uniqueVisitorsTile");
    verify(uniqueTile, "unique-visitors KPI tile exists");
    var deptTile = findChild(reporting, "topDepartmentTile");
    verify(deptTile, "top-department KPI tile exists");
    var studentsTable = findChild(reporting, "topStudentsTable");
    verify(studentsTable, "top-students ranking table exists");
    var rosterToggle = findChild(reporting, "viewRosterToggle");
    verify(rosterToggle, "view-full-roster toggle exists");
    var roster = findChild(reporting, "reportTable");
    compare(roster.visible, false, "full roster starts collapsed");
}

function test_dashboard_kpiBandCollapsesOnEmptyResult() {
    // Spec §6: a generated-but-empty result shows ONE "No report data" state,
    // not four zeroed tiles. Mutate the model to count 0 (read-only prop).
    reportingStub.hasResult = true;
    reportRowsStub.clear();
    var band = findChild(reporting, "kpiBand");
    var emptyState = findChild(reporting, "kpiEmptyState");
    verify(emptyState, "KPI empty-state element exists");
    compare(band.visible, false, "the 4-tile band hides on 0 rows");
    compare(emptyState.visible, true, "the single 'No report data' state shows");
    // restore for later tests
    reportRowsStub.append({ name: "Maria Santos", course: "BSCE", year: "3", visits: 42 });
    reportRowsStub.append({ name: "Jose Cruz", course: "BSIT", year: "1", visits: 7 });
}
```
> Follow the existing conventions: static `reporting` instance, `findChild(reporting, objectName)`, the y-band (`y:7300`) / height rules noted in project memory for QuickTest fixtures. `ListElement` values must be literals (no bindings) — that's why the stub rows are inline.

- [ ] **Step 2: Run — verify RED**

Run: `ctest --test-dir C:/b/loams-4biii -R tst_qml_admin --output-on-failure`
Expected: FAIL — the new objectNames don't exist yet.

- [ ] **Step 3: Restructure the preview into the dashboard grid**

In `ReportingScreen.qml`, inside the preview `ColumnLayout` (the 3-tile row + chart + table, lines ~189-243). The preview is gated by `showPreview` (`vm.hasResult && !isError`, line 21), which is **true even for a generated-but-empty result** — so the KPI band needs its own zero-row branch (see step 1 below). Add a local `readonly property bool hasRows: vm && vm.rows && vm.rows.count > 0` on the screen root to drive it.

1. Replace the 3-tile `RowLayout` with a **KPI section** that has two mutually-exclusive states (spec §6):
   - A `RowLayout { objectName: "kpiBand"; visible: screen.hasRows; Layout.fillWidth: true }` holding **four** `LStatTile`s (all `Layout.fillWidth: true`). **Every value binding keeps the `screen.vm ? … : "…"` guard** the existing tiles use (line 199/205/211) — the `vmlessReporting` fixture renders with no vm:
     - `totalVisitsTile` — label `TOTAL VISITS`, value `screen.vm ? String(screen.vm.totalVisits) : "0"`.
     - `uniqueVisitorsTile` — label `UNIQUE VISITORS`, value `screen.vm ? String(screen.vm.uniqueVisitors) : "0"`.
     - `avgVisitsTile` — label `AVG. VISITS / VISITOR`, value `screen.vm ? screen.vm.avgVisitsPerVisitor.toFixed(1) : "0"`.
     - `topDepartmentTile` — label `TOP DEPARTMENT`, value `screen.vm ? screen.vm.topDepartment : "—"`, **caption** `screen.vm ? (String(screen.vm.topDepartmentVisits) + " visits") : ""` (LStatTile has a `caption` slot — name primary, visits supporting).
   - A single empty-state element `objectName: "kpiEmptyState"; visible: !screen.hasRows` — an `LCard`/`Label` reading `qsTr("No report data")` (NOT four "0"/"—" tiles). This is the §6 "No report data" KPI state.
2. Keep the existing `LCard`/`LBarChart` (Top Courses) directly under the KPI section (glanceable, high on screen). Leave its existing visibility as-is.
3. Add a **rankings row** — a `RowLayout` (or `GridLayout`) of three ranking tables with `objectName` `topStudentsTable` / `topCoursesTable` / `topDepartmentsTable`, bound to `screen.vm.topStudents` / `topCourses` / `topDepartments`.
   > **Do NOT use `LTable` for these.** `LTable` renders the **raw** role value per cell (`LTable.qml:278`) with no per-cell formatter and no table-title property — so `percent.toFixed(0) + "%"` and the `"Top 10 …"` headings can't come from `LTable`, and a bare `62.5` would render. Build each ranking table as a small **`ColumnLayout` + `Repeater`** (`model: screen.vm ? screen.vm.topStudents : null`) over the model's roles, exactly how the delegate can format the percent string and show a literal heading.
   - Students table columns: rank, name (`label`), course (`sublabel`), visits. Courses/departments: rank, name (`label`), visits, and percent rendered as `percent.toFixed(0) + "%"`.
   - Each table has a literal heading `Text { text: qsTr("Top 10 Students") }` (resp. Courses / Departments).
   - **Empty-state per table:** when `count === 0`, show `Text { text: qsTr("No data available.") }` instead of the rows (spec §6). Since these tables only appear once `hasRows` is true, the per-table empty-state covers a ranking that is legitimately empty within a non-empty result.
   - All row text is `Text { textFormat: Text.PlainText }`; all colors `Theme.*` (ZERO raw hex; opacity via `Qt.alpha(Theme.<token>, a)`).
4. Demote the existing full-roster `LTable` (`objectName: "reportTable"`) behind a toggle: add an `LButton`/switch `objectName: "viewRosterToggle"` (text `qsTr("View full roster")`), and bind `reportTable.visible` to a local `property bool showRoster: false` flipped by the toggle. Roster starts collapsed (matches the Step-1 assertion `roster.visible === false`).
> Grid geometry is tunable; the **hierarchy is fixed** (spec §7): KPI section → chart → rankings → roster toggle. Use `Theme.spacing.*`, no raw sizes beyond existing patterns.

- [ ] **Step 4: Run — verify GREEN (and full suite)**

Run: `cmake --build C:/b/loams-4biii` then `ctest --test-dir C:/b/loams-4biii --output-on-failure`
Expected: PASS — the new QuickTests pass and all prior tests stay green. Only **one** new ctest target is added (`tst_reportanalytics`); the VM and QuickTest cases live inside existing targets, so the suite goes **42 → 43** ctest targets (not a per-case count).

- [ ] **Step 5: Commit**

```bash
git add qt-app/quick/qml/admin/ReportingScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(reporting): analytics-first dashboard + full-roster toggle"
```

---

## After the plan

- Run `/claude-review` (phase mode — this branch will have ~6 commits) before finishing.
- Manual smoke on `WITSQuick.exe`: generate a report and confirm the KPI band (4 tiles incl. Top Department name+visits), the three ranking tables, and the "View full roster" toggle behave; confirm empty-states on a zero-row range.
- **Slice 4b-iii-b is out of scope here** — no `reportrenderer`/export changes. The export still emits the 4b-ii layout until slice b.

## Self-Review notes (author)

- **Spec coverage:** §3 value types + aggregation-by-key (T1/T2), §4 KPIs (T1), §5 rankings + tie-break + % (T2), §6 empty-states — core `(Unspecified)`/hasData (T1) + KPI-band "No report data" collapse on 0 rows + per-table "No data available." (T5), §7 dashboard + scalar `Q_PROPERTY`s + PlainText (T4/T5), roster toggle §9-screen-half (T5). Export halves of §8/§9 are **deferred to 4b-iii-b** by design.
- **Types consistent** across tasks: `ReportAnalytics`/`RankingEntry` (T1) → `RankingModel::setEntries` (T3) → VM `topStudents()` etc. (T4) → QML `objectName`s (T5).
- **No renderer touch** — honors the slice boundary.
