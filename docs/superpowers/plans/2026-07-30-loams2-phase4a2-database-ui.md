# LOAMS 2.0 Phase 4a.2 — Database Table + Shared Filter/Selection (Implementation Plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the placeholder Database admin screen into a working, filterable, multi-selectable read-only student table — landing the two shared building blocks (a reusable Dept→Course cascading selector and `LTable` multi-select) centrally before the mutating operations ride on them in 4a.2b.

**Architecture:** MVVM as in Phases 2–4c. A new `DatabaseViewModel` (C++) wraps the existing `StudentController` (search/departments/courses — no new endpoint) and owns a new `StudentsTableModel` (`QAbstractListModel` with a per-row selection role that survives a data refresh). QML gets a reusable `LCascadingSelect` (two `LComboBox`es, Dept→Course, dependent-clear) and an `LTable` multi-select mode (checkbox column + select-all + N-selected header). `DatabaseScreen` composes them, injected with `property var vm` so QuickTests use a plain-QML stub. **No mutations in this increment** (register/edit/bulk/delete/dept-ops are 4a.2b) and **no `admin_key`** (nothing destructive here).

**Tech Stack:** Qt 6.11 QML + C++17, QtTest + Qt Quick Test under CTest (`wits_add_qttest`, `OFFSCREEN` for Quick), CMake + Ninja + MinGW.

## Global Constraints

- **MVVM:** ViewModels are the ONLY QML-facing C++; QML never calls a `witscore` controller directly. Screens take `property var vm`. PascalCase QML types + C++ VM/model classes; `m_camelCase` members. QML module target `witsquickmodule` (URI `LOAMS`).
- **Theming:** every visual token via `Theme.qml`; **ZERO raw hex outside `Theme.qml`**; opacity via `Qt.alpha(Theme.<token>, a)`. A `tst_notokenaliases` grep-guard fails on deprecated token names — use only `Theme.brand.*` / `Theme.accent.*` role tokens.
- **Anti-injection:** any `Text` showing server-supplied strings (student name, department, course) MUST set `textFormat: Text.PlainText` (the backend is cleartext HTTP; a tampered value could carry markup). `LComboBox` already does this; new row `Text`s must too.
- **Cascade scope (design decision, deviates from spec §3.4 — owner-approved 2026-07-30):** the shared selector is **Dept→Course only**. `get_years.php` returns *calendar* years from `library_visits` (a Reporting/4b duration concern, not a student attribute), and student `year_level` is deliberately unfiltered (mixed section/number semantics — see `SearchScreen.qml:532-540`). The `Year` tier is deferred to 4b Reporting as a distinct calendar-year/duration control. `LCascadingSelect` is built extensible but ships with two tiers.
- **No live network in unit tests** (house rule); Quick tests inject a plain-QML stub `vm`. Build dir SHORT path (e.g. `C:/b/loams-4a2`) for MAX_PATH; tools not on PATH (prepend `C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja`).
- **Regression floor:** full existing ctest suite stays green; no new build warnings. Commit via the `commit` skill discipline; never `git add -A`; never `--no-verify`.
- **This increment ships NO mutations and NO `admin_key`.** Register/edit/bulk/delete/dept-ops = 4a.2b.

---

## File Structure

**C++ (`qt-app/quick/`):**
- Create `models/StudentsTableModel.{h,cpp}` — `QAbstractListModel` of `StudentRecord` + a `SelectedRole`; selection tracked by `schoolId` in a `QSet<QString>` so it survives `setRecords`. Exposes `count`, `selectedCount`, `anySelected`, `allSelected`; `Q_INVOKABLE toggle(schoolId)`, `setAllSelected(bool)`, `clearSelection()`, `selectedIds()`.
- Create `viewmodels/DatabaseViewModel.{h,cpp}` — owns `QNetworkAccessManager` + `StudentController` + a `StudentsTableModel`; mirrors `SearchViewModel` wiring (departments/courses/results, loading/error, requestId race). Exposes `students` (the model), `departments`, `courses`, `department`, `loading`, `errorText`; `Q_INVOKABLE refresh()`, `setDepartment()`, `setCourse()`, `reloadTable()`.

**QML components (`qt-app/quick/qml/components/`):**
- Create `LCascadingSelect.qml` — reusable Dept→Course selector (two `LComboBox`), "All" semantics, dependent-clear; `departments`/`courses` string-list props, `department`/`course` state, signals `departmentPicked(string)` + `coursePicked(string)`.
- Modify `LTable.qml` — add multi-select mode: a leading checkbox column (header select-all + per-row checkbox) shown when `selectable: true`, driven by the model's `selected` role + `toggle`/`setAllSelected` invokables; keep the default (non-selectable) rendering byte-identical for existing consumers.

**QML screen (`qt-app/quick/qml/admin/`):**
- Modify `DatabaseScreen.qml` — replace the 18-line placeholder with the real screen: `LCascadingSelect` filter card + a multi-select `LTable` bound to `vm.students`, a "N results / M selected" header, loading/empty/error states, page-in animation. Takes `property var vm`.
- Modify `AdminScreen.qml` — instantiate `DatabaseViewModel { id: databaseVm }`, pass it to the Database page's `DatabaseScreen`, and call `databaseVm.refresh()` when the Database page is shown (mirror how Search autoloads).

**CMake / tests:**
- Modify `qt-app/quick/CMakeLists.txt` — add the two new C++ sources to `witsquickmodule`; add the two new QML files to `QML_FILES`.
- Create `qt-app/quick/tests/tst_studentstablemodel.cpp` + `tst_databaseviewmodel.cpp` (register via `wits_add_qttest`, no OFFSCREEN — pure logic).
- Modify `qt-app/quick/tests/tst_qml_components.qml` — `LCascadingSelect` + `LTable` multi-select QuickTests.
- Create `qt-app/quick/tests/tst_qml_database.qml` (or extend `tst_qml_admin.qml`) — `DatabaseScreen` QuickTest with a stub vm. Register with OFFSCREEN.

**Interfaces produced for 4a.2b:**
- `StudentsTableModel::selectedIds() -> QStringList` (feeds multi-delete + bulk-update).
- `DatabaseViewModel` (2b adds register/edit/bulk/delete/dept-op invokables + `AdminSession` key + `LConfirmDialog`).
- `LCascadingSelect` (reused by 4b Reporting, extended with the Year tier there).
- `LTable` multi-select (checkbox column + select-all).

---

## Task 1: `StudentsTableModel` — rows + selection that survives refresh

**Files:**
- Create: `qt-app/quick/models/StudentsTableModel.h`, `qt-app/quick/models/StudentsTableModel.cpp`
- Create: `qt-app/quick/tests/tst_studentstablemodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add sources to `witsquickmodule`; register the test)

**Interfaces:**
- Consumes: `StudentRecord` (`qt-app/core/studentdata.h`).
- Produces: roles `NameRole, SchoolIdRole, CourseRole, DepartmentRole, YearLevelRole, StatusRole, VisitsRole, SelectedRole` (roleNames `name/schoolId/course/department/yearLevel/status/visits/selected`); `int count()`, `int selectedCount()`, `bool allSelected()`, `bool anySelected()`; `void setRecords(const QList<StudentRecord>&)` (preserves selection by schoolId); `Q_INVOKABLE void toggle(const QString &schoolId)`, `Q_INVOKABLE void setAllSelected(bool)`, `Q_INVOKABLE void clearSelection()`, `QStringList selectedIds() const`. Signals `countChanged()`, `selectionChanged()`.

- [ ] **Step 1: Write the failing test (selection survives refresh + counts)**

Create `qt-app/quick/tests/tst_studentstablemodel.cpp`:

```cpp
#include <QtTest>
#include "StudentsTableModel.h"
#include "studentdata.h"

static StudentRecord rec(const QString &id, const QString &name) {
    StudentRecord r; r.schoolId = id; r.name = name; r.course = "BSIT";
    r.department = "CCS"; r.yearLevel = "2"; r.status = "Active"; r.visits = 3;
    return r;
}

class TestStudentsTableModel : public QObject
{
    Q_OBJECT
private slots:
    void emptyByDefault();
    void setRecordsPopulatesCount();
    void toggleMarksSelected();
    void selectionSurvivesRefreshBySchoolId();
    void setAllAndClear();
    void selectedIdsReturnsOnlySelected();
};

void TestStudentsTableModel::emptyByDefault()
{
    StudentsTableModel m;
    QCOMPARE(m.count(), 0);
    QCOMPARE(m.selectedCount(), 0);
    QVERIFY(!m.anySelected());
    QVERIFY(!m.allSelected());
}

void TestStudentsTableModel::setRecordsPopulatesCount()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    QCOMPARE(m.count(), 2);
    QCOMPARE(m.rowCount(), 2);
}

void TestStudentsTableModel::toggleMarksSelected()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.toggle("A");
    QCOMPARE(m.selectedCount(), 1);
    QVERIFY(m.anySelected());
    QVERIFY(!m.allSelected());
    QCOMPARE(m.selectedIds(), QStringList{"A"});
    m.toggle("A");                       // toggle off
    QCOMPARE(m.selectedCount(), 0);
}

void TestStudentsTableModel::selectionSurvivesRefreshBySchoolId()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.toggle("B");
    // A refresh returns B (renamed) + a new row C; A is gone.
    m.setRecords({rec("B","Ben Updated"), rec("C","Cara")});
    QCOMPARE(m.selectedCount(), 1);      // B still selected
    QCOMPARE(m.selectedIds(), QStringList{"B"});
}

void TestStudentsTableModel::setAllAndClear()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.setAllSelected(true);
    QVERIFY(m.allSelected());
    QCOMPARE(m.selectedCount(), 2);
    m.clearSelection();
    QCOMPARE(m.selectedCount(), 0);
}

void TestStudentsTableModel::selectedIdsReturnsOnlySelected()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben"), rec("C","Cara")});
    m.toggle("A"); m.toggle("C");
    QStringList ids = m.selectedIds(); ids.sort();
    QCOMPARE(ids, (QStringList{"A","C"}));
}

QTEST_APPLESS_MAIN(TestStudentsTableModel)
#include "tst_studentstablemodel.moc"
```

- [ ] **Step 2: Register the test (RED — class doesn't exist)**

In `qt-app/quick/CMakeLists.txt`, near the other `wits_add_qttest` model tests, add:
```cmake
wits_add_qttest(tst_studentstablemodel SOURCES tests/tst_studentstablemodel.cpp models/StudentsTableModel.cpp LIBS witsquickmodule)
```
(Match the exact `wits_add_qttest` signature used by a sibling model test such as `tst_searchresultsmodel` if one exists; if model tests link the model source directly rather than the whole module, mirror that. Confirm the real registration style first.)
Run: `cmake -S qt-app -B C:/b/loams-4a2 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"` then `cmake --build C:/b/loams-4a2 --target tst_studentstablemodel` → FAIL (`StudentsTableModel.h` not found).

- [ ] **Step 3: Write the header**

`qt-app/quick/models/StudentsTableModel.h`:

```cpp
#ifndef STUDENTSTABLEMODEL_H
#define STUDENTSTABLEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QStringList>
#include "studentdata.h"

// Multi-select student table model (spec §4.2). Selection is tracked by
// schoolId in a QSet so it SURVIVES a data refresh (setRecords) — a re-filter
// or reload must not silently drop the operator's selection.
class StudentsTableModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool allSelected READ allSelected NOTIFY selectionChanged)
    Q_PROPERTY(bool anySelected READ anySelected NOTIFY selectionChanged)
public:
    enum Roles {
        NameRole = Qt::UserRole + 1, SchoolIdRole, CourseRole, DepartmentRole,
        YearLevelRole, StatusRole, VisitsRole, SelectedRole
    };
    explicit StudentsTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_records.size(); }
    int selectedCount() const { return m_selected.size(); }
    bool anySelected() const { return !m_selected.isEmpty(); }
    bool allSelected() const { return !m_records.isEmpty() && m_selected.size() == m_records.size(); }

    void setRecords(const QList<StudentRecord> &records);
    QStringList selectedIds() const;

    Q_INVOKABLE void toggle(const QString &schoolId);
    Q_INVOKABLE void setAllSelected(bool on);
    Q_INVOKABLE void clearSelection();

signals:
    void countChanged();
    void selectionChanged();

private:
    void emitRowSelectionChanged(int row);
    QList<StudentRecord> m_records;
    QSet<QString> m_selected;   // selected schoolIds
};

#endif // STUDENTSTABLEMODEL_H
```

- [ ] **Step 4: Write the implementation**

`qt-app/quick/models/StudentsTableModel.cpp`:

```cpp
#include "StudentsTableModel.h"

StudentsTableModel::StudentsTableModel(QObject *parent)
    : QAbstractListModel(parent) {}

int StudentsTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

QVariant StudentsTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size())
        return {};
    const StudentRecord &r = m_records.at(index.row());
    switch (role) {
    case NameRole:       return r.name;
    case SchoolIdRole:   return r.schoolId;
    case CourseRole:     return r.course;
    case DepartmentRole: return r.department;
    case YearLevelRole:  return r.yearLevel;
    case StatusRole:     return r.status;
    case VisitsRole:     return r.visits;
    case SelectedRole:   return m_selected.contains(r.schoolId);
    default:             return {};
    }
}

QHash<int, QByteArray> StudentsTableModel::roleNames() const
{
    return {
        {NameRole,"name"}, {SchoolIdRole,"schoolId"}, {CourseRole,"course"},
        {DepartmentRole,"department"}, {YearLevelRole,"yearLevel"},
        {StatusRole,"status"}, {VisitsRole,"visits"}, {SelectedRole,"selected"}
    };
}

void StudentsTableModel::setRecords(const QList<StudentRecord> &records)
{
    beginResetModel();
    m_records = records;
    // Prune selection to ids still present (selection survives by schoolId).
    QSet<QString> present;
    present.reserve(records.size());
    for (const StudentRecord &r : records) present.insert(r.schoolId);
    m_selected.intersect(present);
    endResetModel();
    emit countChanged();
    emit selectionChanged();
}

QStringList StudentsTableModel::selectedIds() const
{
    return QStringList(m_selected.begin(), m_selected.end());
}

void StudentsTableModel::emitRowSelectionChanged(int row)
{
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {SelectedRole});
}

void StudentsTableModel::toggle(const QString &schoolId)
{
    if (m_selected.contains(schoolId)) m_selected.remove(schoolId);
    else m_selected.insert(schoolId);
    for (int i = 0; i < m_records.size(); ++i)
        if (m_records.at(i).schoolId == schoolId) { emitRowSelectionChanged(i); break; }
    emit selectionChanged();
}

void StudentsTableModel::setAllSelected(bool on)
{
    m_selected.clear();
    if (on) for (const StudentRecord &r : m_records) m_selected.insert(r.schoolId);
    if (!m_records.isEmpty())
        emit dataChanged(index(0,0), index(m_records.size()-1,0), {SelectedRole});
    emit selectionChanged();
}

void StudentsTableModel::clearSelection() { setAllSelected(false); }
```

- [ ] **Step 5: Build + run — GREEN**

Run: `cmake --build C:/b/loams-4a2 --target tst_studentstablemodel && ctest --test-dir C:/b/loams-4a2 -R studentstablemodel --output-on-failure`
Expected: PASS (6 cases).

- [ ] **Step 6: Commit**

`commit` skill. Message: `feat(quick): StudentsTableModel with refresh-surviving selection`.

---

## Task 2: `LTable` multi-select mode

**Files:**
- Modify: `qt-app/quick/qml/components/LTable.qml`
- Modify: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Consumes: a model exposing a `selected` role + `toggle(schoolId)`/`setAllSelected(bool)` invokables and `allSelected`/`selectedCount` (i.e. `StudentsTableModel`), plus the row's `schoolId` role for the id.
- Produces: when `selectable: true`, a leading checkbox column (header = select-all, rows = per-row), driven by the model. Default (`selectable: false`) rendering unchanged.

- [ ] **Step 1: Write failing QuickTests**

In `qt-app/quick/tests/tst_qml_components.qml` add a fixture + `TestCase` (match the file's fixture-band pattern — each fixture in its own y-offset region). Fixture uses a tiny plain-QML `ListModel`-backed stub is NOT enough (needs `toggle`/`allSelected`); instead inject a `StudentsTableModel` is C++-only — so the QuickTest drives selection through the table's checkbox items and asserts against a **stub** object exposing `selected` role + `toggle`. Simplest: give the test a `QtObject` stub model API is impractical for a ListView model. Use the real interaction: build an `LTable` with `selectable: true` and a small C++ `StudentsTableModel` is not available in QML tests without registration. **Therefore:** register `StudentsTableModel` as a QML-creatable type is out of scope; instead the QuickTest validates the checkbox WIRING against a minimal QML `ListModel` plus table-level `selectAllChecked`/`selectedCount` reads, and the deep selection semantics are already covered by `tst_studentstablemodel` (Task 1). Concretely, assert:
  1. With `selectable: false` (default), no checkbox column objects exist (`findChild(table,"selectAllCheck") === null`).
  2. With `selectable: true`, a `selectAllCheck` header checkbox exists and per-row `rowCheck_<id>` items exist.
  3. Clicking `selectAllCheck` calls the model's `setAllSelected(true)` (assert via a spy property on a stub model that records the call).

Add this fixture + test:

```qml
// --- LTable multi-select fixtures (own band) ---
QtObject {
    id: selectStub
    property int setAllCalls: 0
    property bool lastSetAll: false
    property int toggleCalls: 0
    function setAllSelected(v) { setAllCalls++; lastSetAll = v; }
    function toggle(id) { toggleCalls++; }
    property int selectedCount: 0
    property bool allSelected: false
    // Minimal list-model surface for the ListView: 2 rows.
    property var rows: [ {schoolId:"A", name:"Ann", selected:false},
                         {schoolId:"B", name:"Ben", selected:false} ]
}

LTable {
    id: plainTable
    y: 2100
    width: 400; height: 160
    selectable: false
    columns: [ {key:"name", title:"Name"} ]
    model: selectStub.rows
}

LTable {
    id: selectTable
    y: 2280
    width: 400; height: 160
    selectable: true
    selectionModel: selectStub          // NEW prop: the object carrying toggle/setAllSelected/allSelected
    columns: [ {key:"name", title:"Name"} ]
    model: selectStub.rows
}

TestCase {
    name: "LTableMultiSelect"; when: windowShown
    function test_noCheckboxWhenNotSelectable() {
        verify(findChild(plainTable, "selectAllCheck") === null);
    }
    function test_selectAllCheckboxExistsWhenSelectable() {
        verify(findChild(selectTable, "selectAllCheck") !== null);
    }
    function test_headerCheckCallsSetAll() {
        var h = findChild(selectTable, "selectAllCheck");
        verify(h !== null);
        var before = selectStub.setAllCalls;
        mouseClick(h);
        compare(selectStub.setAllCalls, before + 1);
        compare(selectStub.lastSetAll, true);
    }
}
```

- [ ] **Step 2: Run — RED**

Run: `cmake --build C:/b/loams-4a2 --target tst_qml_components && ctest --test-dir C:/b/loams-4a2 -R qml_components --output-on-failure`
Expected: FAIL (`selectAllCheck` doesn't exist; `selectionModel` prop unknown).

- [ ] **Step 3: Implement multi-select in `LTable.qml`**

Add to `LTable`'s root a `property var selectionModel: null` (the object carrying `selected`/`toggle`/`setAllSelected`/`allSelected`/`selectedCount`; kept separate from `model` so the row-data model and the selection API can be the SAME `StudentsTableModel` or, in tests, a stub). In the header `RowLayout` (currently the column titles), prepend — only when `table.selectable` — a select-all checkbox; in the row delegate `RowLayout`, prepend a per-row checkbox. Use a themed checkbox (a small `Rectangle` + check glyph, no raw hex). Concretely:

Header (inside the header `RowLayout`, before the `Repeater`):
```qml
Item {
    visible: table.selectable
    Layout.preferredWidth: table.selectable ? 24 : 0
    implicitHeight: 24
    Rectangle {
        objectName: "selectAllCheck"
        anchors.centerIn: parent
        width: 18; height: 18; radius: Theme.radius.sm2
        color: (table.selectionModel && table.selectionModel.allSelected) ? Theme.accent.base : Theme.card
        border.width: 2; border.color: Theme.border
        Text {
            anchors.centerIn: parent
            visible: table.selectionModel && table.selectionModel.allSelected
            text: "✓"; color: Theme.accent.on
            font.family: Theme.typography.sans; font.pixelSize: Theme.typography.eyebrow
        }
        TapHandler {
            onTapped: if (table.selectionModel)
                table.selectionModel.setAllSelected(!table.selectionModel.allSelected)
        }
    }
}
```
Row delegate (inside the row `RowLayout`, before the cells `Repeater`):
```qml
Item {
    visible: table.selectable
    Layout.preferredWidth: table.selectable ? 24 : 0
    implicitHeight: 24
    Rectangle {
        objectName: "rowCheck_" + (rowDelegate.model.schoolId !== undefined ? rowDelegate.model.schoolId : rowDelegate.index)
        anchors.centerIn: parent
        width: 18; height: 18; radius: Theme.radius.sm2
        color: (rowDelegate.model.selected === true) ? Theme.accent.base : Theme.card
        border.width: 2; border.color: Theme.border
        Text {
            anchors.centerIn: parent
            visible: rowDelegate.model.selected === true
            text: "✓"; color: Theme.accent.on
            font.family: Theme.typography.sans; font.pixelSize: Theme.typography.eyebrow
        }
        TapHandler {
            onTapped: if (table.selectionModel)
                table.selectionModel.toggle(rowDelegate.model.schoolId)
        }
    }
}
```
(Guard every `selectionModel` access with a null check so the default non-selectable table with no `selectionModel` never errors.)

- [ ] **Step 4: Run — GREEN + regression**

Run: `cmake --build C:/b/loams-4a2 --target tst_qml_components && ctest --test-dir C:/b/loams-4a2 -R qml_components --output-on-failure`
Expected: PASS incl. the 3 new cases; the pre-existing `LTable` tests (static geometry) unchanged because `selectable` defaults false and the checkbox `Item`s collapse to width 0.

- [ ] **Step 5: Commit**

`commit` skill. Message: `feat(quick): LTable multi-select (checkbox column + select-all)`.

---

## Task 3: `LCascadingSelect` — reusable Dept→Course selector

**Files:**
- Create: `qt-app/quick/qml/components/LCascadingSelect.qml`
- Modify: `qt-app/quick/qml/CMakeLists.txt` QML_FILES (register the component)
- Modify: `qt-app/quick/tests/tst_qml_components.qml`

**Interfaces:**
- Consumes: `LComboBox` (`model`/`currentValue`/`selected(value)`/`selectValue(v)`).
- Produces: props `departments` (string list), `courses` (string list), `department` (string), `course` (string); signals `departmentPicked(string department)`, `coursePicked(string course)`. "All" semantics: a leading `"All"` entry means no filter (empty string emitted). Dependent-clear: picking a department clears `course` and emits `departmentPicked` (the consumer reloads courses); when `courses` changes and the current `course` is no longer present, it self-clears.

- [ ] **Step 1: Write failing QuickTests**

Add to `tst_qml_components.qml` (own fixture band):

```qml
LCascadingSelect {
    id: casc
    y: 2460
    departments: ["CCS","CBA"]
    courses: ["BSIT","BSCS"]
}
TestCase {
    name: "LCascadingSelect"; when: windowShown
    function init() { casc.department = ""; casc.course = ""; }
    function test_pickingDepartmentEmitsAndClearsCourse() {
        casc.course = "BSIT";
        var spy = signalSpy.createObject(casc, {target: casc, signalName: "departmentPicked"});
        var deptCombo = findChild(casc, "cascDept");
        deptCombo.selectValue("CCS");
        compare(spy.count, 1);
        compare(casc.department, "CCS");
        compare(casc.course, "");            // dependent-clear
        spy.destroy();
    }
    function test_courseNoLongerPresentSelfClears() {
        casc.course = "BSIT";
        casc.courses = ["BSCS"];             // BSIT re-scoped out
        compare(casc.course, "");
    }
    function test_allMeansEmptyFilter() {
        var deptCombo = findChild(casc, "cascDept");
        deptCombo.selectValue("All");
        compare(casc.department, "");        // "All" -> empty
    }
}
```
(`signalSpy` is the file's existing `Component { SignalSpy {} }` factory — reuse whatever the file already uses for spies; if none, add `SignalSpy { id: deptSpy; target: casc; signalName: "departmentPicked" }` as a child and read `deptSpy.count`.)

- [ ] **Step 2: Run — RED** (`LCascadingSelect` unknown type).

- [ ] **Step 3: Implement `LCascadingSelect.qml`**

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Reusable cascading filter (spec §3.4, Dept->Course; Year deferred to 4b —
// see the plan's Global Constraints). Presentational: primitive list props,
// no vm. "All" = no filter (emits ""). Picking a department clears the course
// and asks the consumer to re-scope `courses` (departmentPicked); if the new
// `courses` no longer contains the current course, it self-clears.
RowLayout {
    id: root
    property var departments: []
    property var courses: []
    property string department: ""
    property string course: ""
    signal departmentPicked(string department)
    signal coursePicked(string course)
    spacing: Theme.spacing.md

    readonly property var deptModel: ["All"].concat(root.departments)
    readonly property var courseModel: ["All"].concat(root.courses)

    onCoursesChanged: {
        if (root.course !== "" && root.courses.indexOf(root.course) === -1)
            root.course = "";
    }

    LComboBox {
        objectName: "cascDept"
        Layout.fillWidth: true
        model: root.deptModel
        placeholder: qsTr("All Departments")
        currentValue: root.department === "" ? "" : root.department
        onSelected: function(value) {
            var picked = (value === "All") ? "" : value;
            root.department = picked;
            root.course = "";                 // dependent-clear
            root.departmentPicked(picked);
        }
    }
    LComboBox {
        objectName: "cascCourse"
        Layout.fillWidth: true
        model: root.courseModel
        placeholder: qsTr("All Courses")
        currentValue: root.course === "" ? "" : root.course
        onSelected: function(value) {
            var picked = (value === "All") ? "" : value;
            root.course = picked;
            root.coursePicked(picked);
        }
    }
}
```
Register `qml/components/LCascadingSelect.qml` in the `QML_FILES` list (find the components block in `qt-app/quick/CMakeLists.txt`).

- [ ] **Step 4: Run — GREEN** (reconfigure first since a new QML file was added to the module).

Run: `cmake -S qt-app -B C:/b/loams-4a2 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4a2 --target tst_qml_components && ctest --test-dir C:/b/loams-4a2 -R qml_components --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

`commit` skill. Message: `feat(quick): LCascadingSelect (Dept->Course, Year deferred to 4b)`.

---

## Task 4: `DatabaseViewModel` — load + filter + selection

**Files:**
- Create: `qt-app/quick/viewmodels/DatabaseViewModel.h`, `qt-app/quick/viewmodels/DatabaseViewModel.cpp`
- Create: `qt-app/quick/tests/tst_databaseviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add sources to `witsquickmodule`; register the test)

**Interfaces:**
- Consumes: `StudentController` (`searchStudents`, `loadDepartments`, `loadCourses`, `searchFinished`, `searchFailed`, `departmentsLoaded`, `coursesLoaded`), `StudentsTableModel`.
- Produces: `Q_PROPERTY` `StudentsTableModel *students`, `QStringList departments`, `QStringList courses`, `QString department`, `bool loading`, `QString errorText`; `Q_INVOKABLE void refresh()` (load departments + all students), `void setDepartment(const QString&)` (reload courses + reload table), `void setCourse(const QString&)` (reload table), `void reloadTable()`. Test seam ctor `DatabaseViewModel(StudentController*, QObject*)` so tests inject a controller over a `CapturingNam`/stub.

- [ ] **Step 1: Write the failing test**

Mirror `tst_searchviewmodel`'s approach (drive the VM's public slots network-free by feeding controller signals, OR inject a controller over a fake NAM). Since `DatabaseViewModel` reuses `StudentController`, the cleanest network-free test injects a `StudentController` built on a stub NAM and drives the VM through the controller's signals. Create `qt-app/quick/tests/tst_databaseviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "DatabaseViewModel.h"
#include "StudentsTableModel.h"
#include "studentdata.h"

class TestDatabaseViewModel : public QObject
{
    Q_OBJECT
private slots:
    void departmentsLoadedPopulatesProp();
    void studentsLoadedFillTable();
    void setDepartmentReloadsCoursesAndClears();
    void networkErrorSetsErrorAndClearsRows();
};

// A DatabaseViewModel test-ctor takes a StudentController*; but StudentController
// needs a QNetworkAccessManager. Reuse the CapturingNam harness (qt-app/testsupport)
// so no live network is hit; drive results by emitting the controller's signals
// via a friend/test seam is heavy — instead assert the VM's slot handlers directly
// (they are public, like SearchViewModel's onSearchFinished).
void TestDatabaseViewModel::departmentsLoadedPopulatesProp()
{
    DatabaseViewModel vm;
    vm.onDepartmentsLoaded({"CCS","CBA"});
    QCOMPARE(vm.departments(), (QStringList{"CCS","CBA"}));
}

void TestDatabaseViewModel::studentsLoadedFillTable()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId="A"; r.name="Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    QCOMPARE(vm.students()->count(), 1);
    QVERIFY(!vm.loading());
}

void TestDatabaseViewModel::setDepartmentReloadsCoursesAndClears()
{
    DatabaseViewModel vm;
    vm.onCoursesLoaded({"BSIT"});
    QCOMPARE(vm.courses(), (QStringList{"BSIT"}));
    vm.setDepartment("CCS");
    QCOMPARE(vm.department(), QStringLiteral("CCS"));
}

void TestDatabaseViewModel::networkErrorSetsErrorAndClearsRows()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId="A"; r.name="Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.onSearchFailed("boom", 2);
    QVERIFY(!vm.errorText().isEmpty());
    QCOMPARE(vm.students()->count(), 0);   // stale rows cleared
    QVERIFY(!vm.loading());
}

QTEST_APPLESS_MAIN(TestDatabaseViewModel)
#include "tst_databaseviewmodel.moc"
```
(The default ctor builds its own NAM+controller but the tests never call `refresh()`/`setDepartment` network paths that would fire a request against a live server without a running backend — they drive the public slot handlers directly, exactly as the pure-logic seam. `setDepartment` does call `m_controller->loadCourses` which posts to a NAM with no server; that reply simply errors later and is dropped — assert only the synchronous state. If that proves flaky, add the `StudentController*`-injecting ctor and pass one over `CapturingNam`.)

- [ ] **Step 2: Register + build — RED** (`wits_add_qttest tst_databaseviewmodel SOURCES tests/tst_databaseviewmodel.cpp LIBS witsquickmodule`). FAIL: class missing.

- [ ] **Step 3: Write the header**

`qt-app/quick/viewmodels/DatabaseViewModel.h`:

```cpp
#ifndef DATABASEVIEWMODEL_H
#define DATABASEVIEWMODEL_H

#include <QObject>
#include <QStringList>
#include "studentdata.h"
#include "StudentsTableModel.h"

class QNetworkAccessManager;
class StudentController;

// Database screen VM (spec §4.2, increment 4a.2a — read/filter/select only).
// Wraps StudentController (no new endpoint: an empty-search searchStudents with
// the dept/course filter loads the table). Mirrors SearchViewModel's wiring +
// requestId race guard. Mutations (register/edit/bulk/delete/dept-ops) are 4a.2b.
class DatabaseViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(StudentsTableModel *students READ students CONSTANT)
    Q_PROPERTY(QStringList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QStringList courses READ courses NOTIFY coursesChanged)
    Q_PROPERTY(QString department READ department NOTIFY departmentChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
public:
    explicit DatabaseViewModel(QObject *parent = nullptr);

    StudentsTableModel *students() { return &m_students; }
    QStringList departments() const { return m_departments; }
    QStringList courses() const { return m_courses; }
    QString department() const { return m_department; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    Q_INVOKABLE void refresh();                          // load departments + all students
    Q_INVOKABLE void setDepartment(const QString &department);
    Q_INVOKABLE void setCourse(const QString &course);
    Q_INVOKABLE void reloadTable();

    // Public slots (test seam — driven network-free, like SearchViewModel).
    void onSearchFinished(SearchOutcome outcome, const QList<StudentRecord> &records,
                          const QString &message, const QString &searchTerm, quint64 requestId);
    void onSearchFailed(const QString &errorString, quint64 requestId);
    void onDepartmentsLoaded(const QStringList &departments);
    void onCoursesLoaded(const QStringList &courses);

signals:
    void departmentsChanged();
    void coursesChanged();
    void departmentChanged();
    void loadingChanged();
    void errorTextChanged();

private:
    bool acceptRequest(quint64 requestId);
    void setLoading(bool v);
    void setError(const QString &e);

    QNetworkAccessManager *m_nam = nullptr;
    StudentController *m_controller = nullptr;
    StudentsTableModel m_students;
    QStringList m_departments, m_courses;
    QString m_department, m_course, m_errorText;
    bool m_loading = false;
    quint64 m_latestAppliedRequestId = 0;
};

#endif // DATABASEVIEWMODEL_H
```

- [ ] **Step 4: Write the implementation**

`qt-app/quick/viewmodels/DatabaseViewModel.cpp`:

```cpp
#include "DatabaseViewModel.h"

#include <QNetworkAccessManager>
#include "studentcontroller.h"

DatabaseViewModel::DatabaseViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new StudentController(m_nam, this))
{
    connect(m_controller, &StudentController::searchFinished, this, &DatabaseViewModel::onSearchFinished);
    connect(m_controller, &StudentController::searchFailed, this, &DatabaseViewModel::onSearchFailed);
    connect(m_controller, &StudentController::departmentsLoaded, this, &DatabaseViewModel::onDepartmentsLoaded);
    connect(m_controller, &StudentController::coursesLoaded, this, &DatabaseViewModel::onCoursesLoaded);
}

void DatabaseViewModel::refresh()
{
    m_controller->loadDepartments();
    reloadTable();
}

void DatabaseViewModel::reloadTable()
{
    setError(QString());
    setLoading(true);
    // Empty search + current dept/course filter = "all matching students".
    m_controller->searchStudents(QString(), m_department, m_course);
}

void DatabaseViewModel::setDepartment(const QString &department)
{
    if (m_department != department) {
        m_department = department;
        emit departmentChanged();
        m_controller->loadCourses(department);   // re-scope the course list
    }
    reloadTable();
}

void DatabaseViewModel::setCourse(const QString &course)
{
    m_course = course;
    reloadTable();
}

void DatabaseViewModel::onSearchFinished(SearchOutcome outcome, const QList<StudentRecord> &records,
                                         const QString &message, const QString &searchTerm, quint64 requestId)
{
    Q_UNUSED(searchTerm)
    if (!acceptRequest(requestId)) return;
    setLoading(false);
    m_students.setRecords(records);
    if (outcome == SearchOutcome::InvalidResponse)
        setError(QStringLiteral("Invalid server response."));
    else if (outcome == SearchOutcome::NotSuccess)
        setError(message.isEmpty() ? QStringLiteral("No students found.") : message);
    else
        setError(QString());
}

void DatabaseViewModel::onSearchFailed(const QString & /*errorString*/, quint64 requestId)
{
    if (!acceptRequest(requestId)) return;
    setLoading(false);
    m_students.setRecords({});                 // never leave stale rows behind an error
    setError(QStringLiteral("Network error. Please try again."));
}

void DatabaseViewModel::onDepartmentsLoaded(const QStringList &departments)
{
    m_departments = departments; emit departmentsChanged();
}

void DatabaseViewModel::onCoursesLoaded(const QStringList &courses)
{
    m_courses = courses; emit coursesChanged();
}

bool DatabaseViewModel::acceptRequest(quint64 requestId)
{
    if (requestId < m_latestAppliedRequestId) return false;
    m_latestAppliedRequestId = requestId; return true;
}

void DatabaseViewModel::setLoading(bool v) { if (m_loading != v) { m_loading = v; emit loadingChanged(); } }
void DatabaseViewModel::setError(const QString &e) { if (m_errorText != e) { m_errorText = e; emit errorTextChanged(); } }
```

- [ ] **Step 5: Build + run — GREEN**

Run: `cmake --build C:/b/loams-4a2 --target tst_databaseviewmodel && ctest --test-dir C:/b/loams-4a2 -R databaseviewmodel --output-on-failure`
Expected: PASS (4 cases). NOTE: verify `search_students.php` returns all rows for an empty search + empty filters (Step-1 assumption) — if the backend instead requires a non-empty term, `reloadTable()` still works with a dept/course filter, and the "all students, no filter" load may return empty; confirm and, if needed, document that the Database table shows results once a filter is applied. This is a behavior confirmation, not a code change.

- [ ] **Step 6: Commit**

`commit` skill. Message: `feat(quick): DatabaseViewModel (load/filter/select over StudentController)`.

---

## Task 5: `DatabaseScreen` + `AdminScreen` wiring

**Files:**
- Modify: `qt-app/quick/qml/admin/DatabaseScreen.qml` (replace placeholder)
- Modify: `qt-app/quick/qml/admin/AdminScreen.qml` (instantiate + inject + autoload)
- Create: `qt-app/quick/tests/tst_qml_database.qml` (register with OFFSCREEN)
- Modify: `qt-app/quick/CMakeLists.txt` (register the QuickTest)

**Interfaces:**
- Consumes: `DatabaseViewModel` (`students`, `departments`, `courses`, `department`, `loading`, `errorText`, `refresh`, `setDepartment`, `setCourse`), `LCascadingSelect`, `LTable` multi-select.

- [ ] **Step 1: Write the failing QuickTest (stub vm)**

Create `qt-app/quick/tests/tst_qml_database.qml`:

```qml
import QtQuick
import QtTest
import LOAMS

Item {
    width: 900; height: 700

    // Stub vm: a QtObject with the props DatabaseScreen reads. `students` is a
    // plain-QML ListModel-like array with a count + the multi-select surface.
    QtObject {
        id: stubModel
        property int count: 2
        property int selectedCount: 0
        property bool allSelected: false
        function setAllSelected(v) {}
        function toggle(id) {}
        property var rows: [ {schoolId:"A", name:"Ann", course:"BSIT", department:"CCS",
                              yearLevel:"2", status:"Active", visits:3, selected:false},
                             {schoolId:"B", name:"Ben", course:"BSCS", department:"CCS",
                              yearLevel:"3", status:"Active", visits:1, selected:false} ]
    }
    QtObject {
        id: stubVm
        property var students: stubModel
        property var departments: ["CCS","CBA"]
        property var courses: ["BSIT","BSCS"]
        property string department: ""
        property bool loading: false
        property string errorText: ""
        function refresh() {}
        function setDepartment(d) { department = d; }
        function setCourse(c) {}
    }

    DatabaseScreen { id: screen; anchors.fill: parent; vm: stubVm }

    TestCase {
        name: "DatabaseScreen"; when: windowShown
        function test_showsCascadingFilter() {
            verify(findChild(screen, "cascDept") !== null);
        }
        function test_showsSelectableTable() {
            verify(findChild(screen, "studentsTable") !== null);
            verify(findChild(screen, "selectAllCheck") !== null);   // selectable table
        }
        function test_headerShowsCounts() {
            var h = findChild(screen, "tableCountHeader");
            verify(h !== null);
            verify(h.text.indexOf("2") !== -1);   // 2 results
        }
    }
}
```

- [ ] **Step 2: Register + run — RED**

Add `wits_add_qttest(tst_qml_database SOURCES tests/tst_qml_database.qml OFFSCREEN ...)` (match the exact QuickTest registration signature used by `tst_qml_admin`). Run → FAIL (`DatabaseScreen` is still the placeholder; no `cascDept`/`studentsTable`).

- [ ] **Step 3: Replace `DatabaseScreen.qml`**

Replace the whole file with the real screen — a filter card (`LCascadingSelect` wired to `vm`), a header showing "N results · M selected", the multi-select `LTable` bound to `vm.students`, and loading/empty/error states, following `SearchScreen`'s structure (page-in animation, `color: Theme.appBackground`). Full content:

```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Database (spec §4.2, increment 4a.2a): filterable, multi-selectable student
// table. Read-only here; register/edit/bulk/delete/dept-ops are 4a.2b. Takes
// `property var vm` (a DatabaseViewModel, or a plain-QML stub in QuickTests).
Rectangle {
    id: screen
    property var vm
    property real pageInT: 0
    color: Theme.appBackground

    readonly property int resultCount: vm && vm.students ? vm.students.count : 0
    readonly property int selectedCount: vm && vm.students ? vm.students.selectedCount : 0
    readonly property bool isLoading: vm ? vm.loading : false
    readonly property bool isError: vm ? vm.errorText.length > 0 : false
    readonly property bool isEmpty: !screen.isLoading && !screen.isError && screen.resultCount === 0

    NumberAnimation {
        id: pageInAnimation; objectName: "pageInAnimation"
        target: screen; property: "pageInT"; to: 1
        duration: Theme.motion.enabled ? Theme.motion.pageIn : 0
        easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
    }
    Component.onCompleted: pageInAnimation.start()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg
        opacity: screen.pageInT
        transform: Translate { y: (1 - screen.pageInT) * 16 }

        // Filter card.
        Rectangle {
            Layout.fillWidth: true
            color: Theme.card; radius: Theme.radius.card
            border.width: 2; border.color: Theme.border
            implicitHeight: filterRow.implicitHeight + Theme.spacing.xl * 2
            RowLayout {
                id: filterRow
                anchors.fill: parent; anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md
                LCascadingSelect {
                    id: cascade
                    Layout.fillWidth: true
                    departments: screen.vm ? screen.vm.departments : []
                    courses: screen.vm ? screen.vm.courses : []
                    department: screen.vm ? screen.vm.department : ""
                    onDepartmentPicked: function(d) { if (screen.vm) screen.vm.setDepartment(d); }
                    onCoursePicked: function(c) { if (screen.vm) screen.vm.setCourse(c); }
                }
            }
        }

        // Count/selection header.
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

        // The table.
        LTable {
            id: studentsTable
            objectName: "studentsTable"
            Layout.fillWidth: true; Layout.fillHeight: true
            selectable: true
            selectionModel: screen.vm ? screen.vm.students : null
            model: screen.vm ? screen.vm.students : null
            emptyStateText: screen.isError
                            ? (screen.vm ? screen.vm.errorText : qsTr("Error"))
                            : qsTr("No students")
            columns: [
                { key: "name",       title: qsTr("Name"),       weight: 3 },
                { key: "schoolId",   title: qsTr("ID"),         weight: 2 },
                { key: "course",     title: qsTr("Course"),     weight: 2 },
                { key: "department", title: qsTr("Department"), weight: 3 },
                { key: "status",     title: qsTr("Status"),     weight: 1 },
                { key: "visits",     title: qsTr("Visits"),     weight: 1 }
            ]
        }
    }
}
```
NOTE: `LTable`'s cell `Text` renders `rowDelegate.model[key]` — verify `LTable` sets `textFormat: Text.PlainText` on its cell text (anti-injection); if it does not, add it as part of this task (server-supplied name/department must not render as rich text).

- [ ] **Step 4: Wire into `AdminScreen.qml`**

Near the other per-screen VMs (`SettingsViewModel { id: settingsVm }` etc.), add `DatabaseViewModel { id: databaseVm }`. In the `pageLoader` (the `Loader` that switches admin pages), where `Navigator.Database` currently loads the placeholder `DatabaseScreen`, pass the vm: `DatabaseScreen { vm: databaseVm }`. Add an autoload so the table populates when the Database page is shown — mirror how Search refreshes on navigation (e.g. a `Connections` on `Navigator` calling `databaseVm.refresh()` when `Navigator.adminPage === Navigator.Database`, or the existing autoload mechanism this shell uses). Read the current `AdminScreen.qml` Database case + how Search autoloads before editing, and match that mechanism exactly.

- [ ] **Step 5: Build + run — GREEN + full regression**

Run: `cmake -S qt-app -B C:/b/loams-4a2 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" && cmake --build C:/b/loams-4a2 && ctest --test-dir C:/b/loams-4a2 --output-on-failure`
Expected: `tst_qml_database` passes; full suite green (prior targets + the new model/VM/component/screen tests). No new warnings.

- [ ] **Step 6: Commit**

`commit` skill. Message: `feat(quick): Database screen — filterable multi-select student table`.

---

## Review gate (after all tasks)

- Full ctest green; no new build warnings.
- `create-pr` project 3-agent gate (dry-checker / security-reviewer / general-code-reviewer — if a 4th agent name appears, re-read `.claude/skills/create-pr/SKILL.md` per CLAUDE.md precedence). Security-reviewer: confirm anti-injection `PlainText` on all server-supplied row/combo text; no `admin_key` or mutation introduced (this is a read-only increment).
- GUI walkthrough (WITSQuick): navigate to Database → the student table loads, the Dept→Course cascade filters it (dependent-clear works), row + select-all checkboxes select/deselect, and the "N selected" header updates. Selection survives a re-filter.

## Self-Review checklist (run before handoff)

1. **Spec coverage (§4.2, §3.4, §6.2):** multi-select `LTable` (Task 2) ✓; student table (Task 5) ✓; cascading selector — **Dept→Course**, Year deferred per the owner-approved deviation (Tasks 3, Global Constraints) ✓; selection-survives-refresh (Task 1) ✓. Register/edit/bulk/delete/dept-ops are 4a.2b (out of scope, stated). Export CSV = 4a.2b.
2. **Placeholders:** none — every code step is full content. The two "verify a fact" notes (`wits_add_qttest`/QuickTest registration signature; `search_students` empty-filter behavior; `LTable` cell `PlainText`) are confirmation steps against existing code, not deferred implementation.
3. **Type consistency:** `StudentsTableModel` roles/`selectedIds()`/`toggle`/`setAllSelected` match between Task 1, the `selectionModel` usage in Task 2, and `DatabaseViewModel::students()` in Task 4/5; `LCascadingSelect` signals `departmentPicked`/`coursePicked` match between Task 3 and the `DatabaseScreen` handlers in Task 5; `DatabaseViewModel` prop names (`students`/`departments`/`courses`/`department`/`loading`/`errorText`) match between Task 4 and Task 5's bindings.
