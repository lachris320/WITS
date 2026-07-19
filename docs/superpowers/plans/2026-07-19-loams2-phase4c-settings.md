# LOAMS 2.0 Phase 4c — Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use the subagent-build skill (`/subagent-build`) to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fill the disabled admin **Settings** sidebar item — school identity + live-re-theme logo import, admin name/position, admin-key change, library hours, guest-login toggle, and the tier-2 destructive **reset-visits-per-department** — as an MVVM presentation layer over existing controllers, and land the shared contract foundation (Navigator enum, nav scaffolding, `AdminSession`, and the form/dialog primitives Settings needs) that 4c is first to require.

**Architecture:** A new `SettingsViewModel` (the only QML-facing C++ for the screen) owns the existing `SettingsController` (QSettings persistence + logo import) and drives async form POSTs through the shared `HttpForm` transport, exactly like `GuestViewModel`. A process-wide `AdminSession` singleton holds the admin key captured at the kiosk login gate in RAM (never QSettings) and supplies it to destructive POSTs. `SettingsScreen.qml` receives its VM as `property var vm` so QuickTests inject a plain-QML stub; every visual token comes from `Theme.qml`.

**Tech Stack:** Qt 6 / C++17, Qt Quick (QML), MVVM over witscore; QtTest + Qt Quick Test under CTest; PHP/MySQL backend.

---

## Build / test commands (this repo — use exactly these)

- **Configure (once):** `cmake -S qt-app -B qt-app/build`
- **Build:** `cmake --build qt-app/build`
- **Run one test:** `ctest --test-dir qt-app/build --output-on-failure -R <name>`
- **Run the whole suite:** `ctest --test-dir qt-app/build --output-on-failure`

Environment notes (from project memory — honor them):
- The Qt 6.11.1 MinGW kit under `C:\Qt` is **not on PATH**; configure with the kit's toolchain (Ninja generator + `CMAKE_PREFIX_PATH` pointing at the kit) the way the existing `qt-app/build` was configured. If `cmake -S qt-app -B qt-app/build` reports "could not find Qt6", pass the kit prefix (e.g. `-DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -G Ninja`) — do not switch kits.
- On Windows/NTFS the default build dir can overflow the 250-char object-path limit for the QML module; if the build fails on a too-long path, reconfigure into a short build dir (e.g. `C:/b/loams-dbg`) and substitute that for `qt-app/build` in the ctest `--test-dir` argument.
- **QML-module edits require a rebuild before QuickTests see them.** Module QML (everything under `qt-app/quick/qml/…`, listed in `qt_add_qml_module(... QML_FILES ...)`) is compiled via `qmlcachegen` into the static module — a `cmake --build` is mandatory after editing it. Only the `tests/tst_*.qml` data files are read **live** from `QUICK_TEST_SOURCE_DIR` at run time. So: edit a component/screen → **build** → ctest; edit only a `tst_*.qml` → ctest directly (still needs the target built once).
- Regression floor: Phase 3's **32 ctest targets / 128 QuickTest cases** must stay green. Never let a task drop below the prior count.

Commit via the `commit` skill in real execution. This plan shows representative `git add <paths>` + `git commit -m "<conventional msg>"` at each task's end.

---

## File Structure

**Created**
- `qt-app/quick/AdminSession.h` / `.cpp` — process-wide singleton holding the admin key in RAM only (T3).
- `qt-app/quick/tests/tst_adminsession.cpp` — AdminSession unit test (T3).
- `qt-app/quick/qml/admin/DatabaseScreen.qml` — placeholder screen for track 4a (T2).
- `qt-app/quick/qml/admin/ReportingScreen.qml` — placeholder screen for track 4b (T2).
- `qt-app/quick/qml/components/LTextField.qml` — Theme-tokened text field with optional password echo (T4).
- `qt-app/quick/qml/components/LConfirmDialog.qml` — tiered confirm dialog (tier 1 / tier 2 + key field) (T5).
- `qt-app/quick/qml/components/LComboBox.qml` — Theme-tokened combo box (flat department picker) (T6).
- `qt-app/quick/viewmodels/SettingsViewModel.h` / `.cpp` — the Settings screen's ViewModel (T7–T13).
- `qt-app/quick/tests/tst_settingsviewmodel.cpp` — SettingsViewModel unit test (T7–T13).
- `qt-app/quick/qml/admin/SettingsScreen.qml` — the real Settings screen (T14).

**Modified**
- `qt-app/quick/viewmodels/Navigator.h` — extend `AdminPage` enum with `Database, Reporting, Settings` (T1).
- `qt-app/quick/tests/tst_navigator.cpp` — cover the new enum routing (T1).
- `qt-app/quick/qml/admin/AdminScreen.qml` — enable the three items, add pageLoader switch cases + routes + titles + VMs (T2, T16).
- `qt-app/quick/tests/tst_qml_adminshell.qml` — route-through tests for the newly-enabled items (T2).
- `qt-app/quick/qml/theme/Theme.qml` — add the `secondaryHover` token only if a task needs one (kept minimal; no change expected).
- `qt-app/quick/viewmodels/KioskViewModel.h` / `.cpp` — capture the admin key into `AdminSession` at the login gate (T3).
- `qt-app/quick/tests/tst_kioskviewmodel.cpp` — cover the capture seam (T3).
- `qt-app/quick/tests/tst_qml_components.qml` — fixtures + cases for LTextField / LConfirmDialog / LComboBox (T4–T6).
- `qt-app/quick/tests/tst_qml_admin.qml` — stub-VM SettingsScreen flows (T15).
- `qt-app/quick/CMakeLists.txt` — register new QML files, C++ SOURCES, and test targets (T2–T14).
- `deliverables/loams_api/reset_visits.php` — add `requireAdminAuth` guard (T17); deploy to xampp.
- `deliverables/loams_api/update_admin_info.php` — add `requireAdminAuth` guard (T17); deploy to xampp.
- `qt-app/adminwindow.h` / `.cpp` — capture + thread the admin key into the legacy `reset_visits` POST (T18).
- `qt-app/mainwindow.cpp` — hand the entered admin key to the legacy admin window at the login gate (T18).

---

## Frozen decisions (do not relitigate)

- **MVVM:** the ViewModel is the only QML-facing C++; the screen receives `property var vm`; QuickTests inject a plain-QML stub. **Zero raw hex outside `Theme.qml`**; opacity variants via `Qt.alpha(Theme.<token>, a)`. Type names PascalCase; C++ members `m_camelCase`. QML module target is `witsquickmodule` (URI `LOAMS`).
- **Cuts — do NOT implement:** font-family/size picker, poster upload, "Book Drop System" checkbox, standalone "clear attendance" checkbox.
- **Reset-visits** is the single **tier-2** op: a re-typed admin key (verified server-side by the T17 guard) + an export-before-destroy CSV.
- **Backend:** adopt the already-shipped `requireAdminAuth` helper on `reset_visits.php` + `update_admin_info.php`; keep the legacy Widgets rollback whole (T18).
- **Scope guard (YAGNI):** build ONLY what Settings uses — a **flat** department combo, not the cascading Dept→Course→Year selector; **no** `LTable` multi-select, **no** `LAvatar`. Those belong to 4a/4d.

**Known limitation to state honestly (T10/T11/T13):** `HttpForm::submit` routes any reply whose `QNetworkReply::error() != NoError` — including an HTTP **401** — to `onNetworkError()` and does **not** hand the 401 body to `onSuccess`. So the VM's auth-failure *decode* seam is unit-tested against synthetic 401-style payloads (as spec §6.1 requires), but in production a 401 currently surfaces through the generic network-error path. Surfacing HTTP-error bodies would be an `HttpForm` change — out of 4c scope (Phase 6). Each destructive-op task notes this at its wiring step; do not silently pretend the 401 UX is live.

---

## CONTRACT FOUNDATION

### Task 1: Extend `Navigator::AdminPage` with Database, Reporting, Settings

**Files:**
- Modify: `qt-app/quick/viewmodels/Navigator.h`
- Test: `qt-app/quick/tests/tst_navigator.cpp`

- [ ] **Write the failing test.** Append these slots to the `private slots:` block in `qt-app/quick/tests/tst_navigator.cpp` (after `showAdminResetsPageToDashboard();`):

```cpp
    void showAdminPageRoutesToSettings();
    void newEnumValuesAreDistinct();
```

and add their bodies before the `QTEST_MAIN` line:

```cpp
void TestNavigator::showAdminPageRoutesToSettings()
{
    Navigator nav;
    QSignalSpy spy(&nav, &Navigator::adminPageChanged);
    nav.showAdminPage(Navigator::Settings);
    QCOMPARE(nav.adminPage(), Navigator::Settings);
    QCOMPARE(spy.count(), 1);
    nav.showAdminPage(Navigator::Settings);   // idempotent — no redundant signal
    QCOMPARE(spy.count(), 1);
}

void TestNavigator::newEnumValuesAreDistinct()
{
    // The three new pages must be distinct values, and distinct from the
    // three originals, or the pageLoader switch (AdminScreen.qml) would alias
    // two sidebar items onto one screen.
    QSet<int> seen{ Navigator::Dashboard, Navigator::Search, Navigator::VisitLogs,
                    Navigator::Database, Navigator::Reporting, Navigator::Settings };
    QCOMPARE(seen.size(), 6);
}
```

Add `#include <QSet>` near the top of the test file if not present.

- [ ] **Run it, expect FAIL** (the enum values don't exist yet → compile error):
```
ctest --test-dir qt-app/build --output-on-failure -R tst_navigator
```
Expected: build fails with `'Settings' is not a member of 'Navigator'` (a red compile is a valid TDD "red" here).

- [ ] **Minimal implementation.** In `qt-app/quick/viewmodels/Navigator.h`, extend the enum (line 19) from:
```cpp
    enum AdminPage { Dashboard, Search, VisitLogs };
```
to:
```cpp
    enum AdminPage { Dashboard, Search, VisitLogs, Database, Reporting, Settings };
```
No `.cpp` change is needed — `showAdminPage(AdminPage)` and `setSurface` are already generic over the enum.

- [ ] **Run it, expect PASS:**
```
ctest --test-dir qt-app/build --output-on-failure -R tst_navigator
```
Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/Navigator.h qt-app/quick/tests/tst_navigator.cpp
git commit -m "feat(loams2): extend Navigator::AdminPage with Database/Reporting/Settings (Phase 4c)"
```

---

### Task 2: Enable the three sidebar items + route them (placeholder screens)

**Files:**
- Create: `qt-app/quick/qml/admin/DatabaseScreen.qml`, `qt-app/quick/qml/admin/ReportingScreen.qml`
- Modify: `qt-app/quick/qml/admin/AdminScreen.qml`, `qt-app/quick/CMakeLists.txt`
- Test: `qt-app/quick/tests/tst_qml_adminshell.qml`

> **DEVIATION from the task-skeleton wording:** the routing test goes in `tst_qml_adminshell.qml`, **not** `tst_qml_admin.qml`. `tst_qml_adminshell.qml` is the only QuickTest that instantiates the real `AdminScreen` shell (sidebar + Loader) with `autoLoad: false`; `tst_qml_admin.qml` instantiates individual screens with hand-computed y-band geometry. Both files are discovered live via the shared `QUICK_TEST_SOURCE_DIR`, so the new cases run under every QuickTest target with no new CMake target. (SettingsScreen's own stub-VM flows DO go in `tst_qml_admin.qml` — that is T15.)

- [ ] **Create the two placeholder screens.** `qt-app/quick/qml/admin/DatabaseScreen.qml`:
```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Placeholder for track 4a (Database + Import). Navigation must work before
// that track fills it in. No `vm` — AdminScreen's Loader.onLoaded guards on
// `item.vm && item.vm.refresh`, so a vm-less screen issues no network.
Rectangle {
    id: screen
    color: Theme.appBackground
    Text {
        anchors.centerIn: parent
        objectName: "databasePlaceholder"
        text: qsTr("Database — coming soon")
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.cardTitle
    }
}
```
`qt-app/quick/qml/admin/ReportingScreen.qml` is identical except `objectName: "reportingPlaceholder"` and `text: qsTr("Reporting — coming soon")`.

- [ ] **Register both QML files.** In `qt-app/quick/CMakeLists.txt`, inside `qt_add_qml_module(witsquickmodule ... QML_FILES ...)`, add after `qml/admin/VisitLogsScreen.qml`:
```cmake
        qml/admin/DatabaseScreen.qml
        qml/admin/ReportingScreen.qml
```

- [ ] **Wire `AdminScreen.qml`.** Make four edits:

  1. Add the two VM-less Components at the bottom (after the `visitLogsComponent` line ~152):
```qml
    Component { id: databaseComponent;  DatabaseScreen  { objectName: "databasePage" } }
    Component { id: reportingComponent; ReportingScreen { objectName: "reportingPage" } }
    // settingsComponent is added in Task 14 (real SettingsScreen). Until then
    // Settings routes to the Database placeholder via the switch default? NO —
    // it gets its own placeholder case here so the route is observable now:
    Component { id: settingsPlaceholderComponent;
                Rectangle { objectName: "settingsPage"; color: Theme.appBackground
                            Text { anchors.centerIn: parent; text: qsTr("Settings — coming soon")
                                   color: Theme.mutedText; font.family: Theme.typography.sans
                                   font.pixelSize: Theme.typography.cardTitle } } }
```

  2. Flip `enabled: false` → `enabled: true` for the three items in the `items:` array (lines 80–82):
```qml
                { page: "database",  label: qsTr("Database"),   enabled: true },
                { page: "reporting", label: qsTr("Reporting"),  enabled: true },
                { page: "settings",  label: qsTr("Settings"),   enabled: true }
```

  3. Extend `onPageActivated` (replace the `else console.warn(...)` tail) so all six keys route:
```qml
                else if (page === "database")
                    Navigator.showAdminPage(Navigator.Database)
                else if (page === "reporting")
                    Navigator.showAdminPage(Navigator.Reporting)
                else if (page === "settings")
                    Navigator.showAdminPage(Navigator.Settings)
                else
                    console.warn("AdminScreen: no route for page key", page)
```

  4. Extend the `currentPage` string-key map (lines 73–75), the `pageTitle` switch (lines 48–54), and the `pageLoader` switch (lines 134–139):
```qml
            // currentPage
            currentPage: Navigator.adminPage === Navigator.Search    ? "search"
                       : Navigator.adminPage === Navigator.VisitLogs ? "visitlogs"
                       : Navigator.adminPage === Navigator.Database  ? "database"
                       : Navigator.adminPage === Navigator.Reporting ? "reporting"
                       : Navigator.adminPage === Navigator.Settings  ? "settings"
                       : "dashboard"
```
```qml
    readonly property string pageTitle: {
        switch (Navigator.adminPage) {
        case Navigator.Search:     return qsTr("Search");
        case Navigator.VisitLogs:  return qsTr("Visit Logs");
        case Navigator.Database:   return qsTr("Database");
        case Navigator.Reporting:  return qsTr("Reporting");
        case Navigator.Settings:   return qsTr("Settings");
        default:                   return qsTr("Dashboard");
        }
    }
```
```qml
                sourceComponent: {
                    switch (Navigator.adminPage) {
                    case Navigator.Search:    return searchComponent;
                    case Navigator.VisitLogs: return visitLogsComponent;
                    case Navigator.Database:  return databaseComponent;
                    case Navigator.Reporting: return reportingComponent;
                    case Navigator.Settings:  return settingsPlaceholderComponent;
                    default:                  return dashboardComponent;
                    }
                }
```

- [ ] **Write the failing test.** Add to `tst_qml_adminshell.qml`, inside the `AdminScreenShell` TestCase:
```qml
        // Phase 4c contract: the three newly-enabled items route to their
        // (placeholder) screens through the real sidebar + Loader.
        function test_databaseItemRoutesToDatabaseScreen() {
            var nav = findChild(shell, "sideNav");
            var loader = findChild(shell, "pageLoader");
            nav.activate("database");
            compare(Navigator.adminPage, Navigator.Database);
            compare(loader.item.objectName, "databasePage");
        }
        function test_reportingItemRoutesToReportingScreen() {
            var nav = findChild(shell, "sideNav");
            var loader = findChild(shell, "pageLoader");
            nav.activate("reporting");
            compare(Navigator.adminPage, Navigator.Reporting);
            compare(loader.item.objectName, "reportingPage");
        }
        function test_settingsItemRoutesToSettingsScreen() {
            var nav = findChild(shell, "sideNav");
            var loader = findChild(shell, "pageLoader");
            nav.activate("settings");
            compare(Navigator.adminPage, Navigator.Settings);
            compare(loader.item.objectName, "settingsPage");
        }
        function test_pageTitleTracksNewPages() {
            var header = findChild(shell, "pageHeader");
            Navigator.showAdminPage(Navigator.Database);
            compare(header.title, "Database");
            Navigator.showAdminPage(Navigator.Reporting);
            compare(header.title, "Reporting");
            Navigator.showAdminPage(Navigator.Settings);
            compare(header.title, "Settings");
        }
```
Also delete the now-obsolete `test_sidebarDisabledItemGuardBlocksActivation` reliance on `"database"` being disabled — **update** that test's disabled-item key from `"database"` to a still-disabled key. Since NO admin item is disabled after this task, that test's premise is gone: replace its body with a positive-control-only assertion, or remove it. Do this:
```qml
        // All six admin items are enabled as of Phase 4c; the disabled-item
        // guard is now exercised by tst_qml_components.qml's LSideNav fixture
        // (which keeps a disabled "database" item). Here, assert every item
        // activates.
        function test_allSidebarItemsActivate() {
            var nav = findChild(shell, "sideNav");
            var keys = ["dashboard","search","visitlogs","database","reporting","settings"];
            for (var i = 0; i < keys.length; i++) {
                activationSpy.clear();
                nav.activate(keys[i]);
                compare(activationSpy.count, 1);
            }
        }
```

- [ ] **Build, then run — expect PASS** (module QML changed → rebuild is mandatory):
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_qml_admin
```
Expected: the `AdminScreenShell` cases (which run under the `tst_qml_admin` target via the shared source dir) pass; `100% tests passed`.

- [ ] **Run the full suite** to confirm the enabled items broke nothing:
```
ctest --test-dir qt-app/build --output-on-failure
```
Expected: no regressions vs the 32-target floor.

- [ ] **Commit:**
```
git add qt-app/quick/qml/admin/DatabaseScreen.qml qt-app/quick/qml/admin/ReportingScreen.qml qt-app/quick/qml/admin/AdminScreen.qml qt-app/quick/CMakeLists.txt qt-app/quick/tests/tst_qml_adminshell.qml
git commit -m "feat(loams2): enable + route Database/Reporting/Settings admin items with placeholders (Phase 4c)"
```

---

### Task 3: `AdminSession` singleton + capture at the kiosk login gate

**Files:**
- Create: `qt-app/quick/AdminSession.h`, `qt-app/quick/AdminSession.cpp`, `qt-app/quick/tests/tst_adminsession.cpp`
- Modify: `qt-app/quick/viewmodels/KioskViewModel.h`, `qt-app/quick/viewmodels/KioskViewModel.cpp`, `qt-app/quick/tests/tst_kioskviewmodel.cpp`, `qt-app/quick/CMakeLists.txt`
- Test targets: `tst_adminsession` (new), `tst_kioskviewmodel` (extended)

- [ ] **Write the failing AdminSession test.** Create `qt-app/quick/tests/tst_adminsession.cpp`:
```cpp
#include <QtTest>
#include <QSignalSpy>
#include "AdminSession.h"

// AdminSession is a process-wide singleton, so every test clears it first.
class TestAdminSession : public QObject
{
    Q_OBJECT
private slots:
    void init() { AdminSession::instance().clear(); }

    void startsEmpty();
    void setKeyStoresAndSignalsAndReportsHasKey();
    void setKeySameValueIsIdempotent();
    void refreshUpdatesHeldKey();
    void clearEmptiesAndSignals();
    void keyIsNeverPersistedToSettings();  // RAM-only guarantee (documentary)
};

void TestAdminSession::startsEmpty()
{
    QVERIFY(!AdminSession::instance().hasKey());
    QVERIFY(AdminSession::instance().key().isEmpty());
}

void TestAdminSession::setKeyStoresAndSignalsAndReportsHasKey()
{
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("SECRET-1"));
    QVERIFY(AdminSession::instance().hasKey());
    QCOMPARE(spy.count(), 1);
}

void TestAdminSession::setKeySameValueIsIdempotent()
{
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));  // unchanged
    QCOMPARE(spy.count(), 0);
}

void TestAdminSession::refreshUpdatesHeldKey()
{
    AdminSession::instance().setKey(QStringLiteral("OLD"));
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().refresh(QStringLiteral("NEW"));
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("NEW"));
    QCOMPARE(spy.count(), 1);
}

void TestAdminSession::clearEmptiesAndSignals()
{
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().clear();
    QVERIFY(!AdminSession::instance().hasKey());
    QCOMPARE(spy.count(), 1);
}

void TestAdminSession::keyIsNeverPersistedToSettings()
{
    // AdminSession has no QSettings dependency at all — the key lives only in
    // the m_key member. Documentary RAM-only check, kept HERMETIC: probe a
    // throwaway INI scope (never the developer's real MyCompany/MyApp registry,
    // matching the SettingsViewModel tests' isolation).
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QSettings s(tmp.path() + QStringLiteral("/probe.ini"), QSettings::IniFormat);
    QVERIFY(!s.allKeys().contains(QStringLiteral("admin/key")));
    QVERIFY(!s.allKeys().contains(QStringLiteral("admin/admin_key")));
}

QTEST_MAIN(TestAdminSession)
#include "tst_adminsession.moc"
```
Add `#include <QSettings>` and `#include <QTemporaryDir>` at the top.

- [ ] **Register the test target.** In `qt-app/quick/CMakeLists.txt`, add after the `tst_navigator` block:
```cmake
# --- AdminSession unit test (C++ QtTest, offscreen: house consistency —
# witsquickmodule PUBLIC-propagates Qt::Gui). ---
wits_add_qttest(tst_adminsession
    SOURCES tests/tst_adminsession.cpp
    LIBS witsquickmodule
    OFFSCREEN)
```

- [ ] **Run it, expect FAIL** (no `AdminSession.h` yet → compile error):
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_adminsession
```
Expected: build error `AdminSession.h: No such file or directory`.

- [ ] **Implement `AdminSession`.** Create `qt-app/quick/AdminSession.h`:
```cpp
#ifndef ADMINSESSION_H
#define ADMINSESSION_H

#include <QObject>
#include <QString>

// In-memory holder for the admin key captured at the kiosk admin-login gate
// (spec §3.3). RAM ONLY — NEVER written to QSettings. A process-wide singleton
// so the capture site (KioskViewModel) and the consumers (SettingsViewModel,
// later DatabaseViewModel) share ONE instance with no QML plumbing. Not
// QML-registered: no Phase-4c QML consumer needs it directly (YAGNI).
class AdminSession : public QObject
{
    Q_OBJECT
public:
    static AdminSession &instance();

    QString key() const { return m_key; }
    bool hasKey() const { return !m_key.isEmpty(); }

    // Capture at the admin-entry gate.
    void setKey(const QString &key);
    // Same-session key change in Settings — refresh so subsequent destructive
    // ops don't 401 on a stale held key (spec §3.3).
    void refresh(const QString &newKey) { setKey(newKey); }
    void clear();

signals:
    void changed();

private:
    explicit AdminSession(QObject *parent = nullptr) : QObject(parent) {}
    Q_DISABLE_COPY(AdminSession)
    QString m_key;
};

#endif // ADMINSESSION_H
```
Create `qt-app/quick/AdminSession.cpp`:
```cpp
#include "AdminSession.h"

AdminSession &AdminSession::instance()
{
    static AdminSession s;   // Meyers singleton: constructed on first use.
    return s;
}

void AdminSession::setKey(const QString &key)
{
    if (m_key == key)
        return;                 // idempotent — no redundant changed()
    m_key = key;
    emit changed();
}

void AdminSession::clear()
{
    if (m_key.isEmpty())
        return;
    m_key.clear();
    emit changed();
}
```

- [ ] **Add `AdminSession` to the module SOURCES.** In `qt-app/quick/CMakeLists.txt`, inside `qt_add_qml_module(witsquickmodule ... SOURCES ...)`, add after `Initials.h Initials.cpp`:
```cmake
        AdminSession.h AdminSession.cpp
```

- [ ] **Run the AdminSession test, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_adminsession
```
Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Write the failing capture test.** In `qt-app/quick/tests/tst_kioskviewmodel.cpp`, add a `#include "AdminSession.h"` and two slots + bodies:
```cpp
void TestKioskViewModel::adminLoginResponseCapturesHeldKey()
{
    AdminSession::instance().clear();
    KioskViewModel vm;
    QSignalSpy admin(&vm, &KioskViewModel::adminRequested);
    // Network-free seam: a parsed admin-success payload (status=success, NO
    // "student" object -> isAdmin), plus the key that was posted.
    vm.applyLoginResponse(R"({"status":"success"})", QStringLiteral("SUPERSECRET"));
    QCOMPARE(admin.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("SUPERSECRET"));
}

void TestKioskViewModel::studentLoginResponseDoesNotCaptureKey()
{
    AdminSession::instance().clear();
    KioskViewModel vm;
    vm.applyLoginResponse(R"({"status":"success","student":{"name":"Ana","course":"BSCE"}})",
                          QStringLiteral("ignored"));
    QVERIFY(!AdminSession::instance().hasKey());   // student branch never captures
}
```
Declare both slots in the test class's `private slots:` section. The test class in `tst_kioskviewmodel.cpp` is `TestKioskViewModel` (confirmed at `tst_kioskviewmodel.cpp:20`) — the slot declarations must land in that exact class.

- [ ] **Run it, expect FAIL** (no `applyLoginResponse` yet → compile error).

- [ ] **Implement the capture seam in KioskViewModel.** In `KioskViewModel.h`, under the existing "Network-free seam" comment (near `applyStudentLogin`), add:
```cpp
    // Network-free seam: decode an admin_login.php / student_login.php
    // response exactly as the reply handler does. On the admin branch it
    // captures heldKey (the admin key that was POSTed) into AdminSession
    // BEFORE emitting adminRequested(). Used by the reply handler AND tests.
    void applyLoginResponse(const QByteArray &json, const QString &heldKey);
```
In `KioskViewModel.cpp`, add `#include "AdminSession.h"` and define the seam, then delegate from `postForm`:
```cpp
void KioskViewModel::applyLoginResponse(const QByteArray &body, const QString &heldKey)
{
    const LoginParser::LoginResult r = LoginParser::parseLoginResponse(body);
    if (r.ok && r.isStudent) {
        applyStudentLogin(r.student);
    } else if (r.ok && r.isAdmin) {
        AdminSession::instance().setKey(heldKey);   // §3.3 capture, before the signal
        emit adminRequested();
    } else {
        setStatus(r.message, QStringLiteral("Error"));
    }
}
```
Then change `postForm`'s non-RFID branch to route through it. Replace the `else` block of the success lambda (currently lines ~96–99) so the lambda captures `value` and delegates:
```cpp
void KioskViewModel::postForm(const QUrl &url, const QString &key,
                              const QString &value, bool rfid)
{
    HttpForm::submit(m_nam, url, {{key, value}}, this,
        [this, rfid, value](const QByteArray &body) {
            if (rfid) {
                const LoginParser::RfidResult r = LoginParser::parseRfidResponse(body);
                if (r.ok) applyStudentLogin(r.student);
                else      setStatus(r.message, QStringLiteral("Error"));
                return;
            }
            // Non-RFID: student_login.php or admin_login.php. `value` is the
            // POSTed credential — for the admin branch it IS the admin key, so
            // the reply handler already has the key in scope (spec §3.3's
            // "thread it through postForm into the reply handler").
            applyLoginResponse(body, value);
        },
        [this]() {
            setStatus(QStringLiteral("Network error. Please try again."),
                      QStringLiteral("Error"));
        });
}
```

- [ ] **Run the capture test, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_kioskviewmodel
```
Expected: `100% tests passed`.

- [ ] **Run the full suite** (KioskViewModel is central — confirm no regression):
```
ctest --test-dir qt-app/build --output-on-failure
```
Expected: 33 targets now (32 floor + `tst_adminsession`), all passing.

- [ ] **Commit:**
```
git add qt-app/quick/AdminSession.h qt-app/quick/AdminSession.cpp qt-app/quick/tests/tst_adminsession.cpp qt-app/quick/viewmodels/KioskViewModel.h qt-app/quick/viewmodels/KioskViewModel.cpp qt-app/quick/tests/tst_kioskviewmodel.cpp qt-app/quick/CMakeLists.txt
git commit -m "feat(loams2): add AdminSession + capture admin key at kiosk login gate (Phase 4c)"
```

---

### Task 4: `LTextField` — Theme-tokened text field

**Files:**
- Create: `qt-app/quick/qml/components/LTextField.qml`
- Modify: `qt-app/quick/CMakeLists.txt`, `qt-app/quick/tests/tst_qml_components.qml`

- [ ] **Register the QML file.** In `qt-app/quick/CMakeLists.txt`, add to `QML_FILES` (after `qml/components/LPageHeader.qml`):
```cmake
        qml/components/LTextField.qml
```

- [ ] **Write the failing test.** In `tst_qml_components.qml`, add a fixture (near the other `L*` fixtures, e.g. after `LDialog { id: dg; title: "T" }`):
```qml
    LTextField { id: tf; label: "School Name"; text: "Acme" }
    LTextField { id: pf; label: "Admin Key"; isPassword: true; text: "secret" }
```
and a TestCase (as a new top-level `TestCase` block inside the root `Item`):
```qml
    TestCase {
        name: "LTextFieldBehavior"
        when: windowShown
        function test_labelAndTextExposed() {
            verify(tf !== null);
            compare(tf.label, "School Name");
            compare(tf.text, "Acme");
        }
        function test_bindsThemeTokensNotLiterals() {
            // Text color must be the Theme token, proving no raw hex leaked in.
            var input = findChild(tf, "fieldInput");
            verify(input !== null);
            compare(input.color, Theme.text);
        }
        function test_passwordModeMasksInput() {
            var input = findChild(pf, "fieldInput");
            verify(input !== null);
            compare(input.echoMode, TextInput.Password);
        }
        function test_defaultModeShowsPlainText() {
            var input = findChild(tf, "fieldInput");
            compare(input.echoMode, TextInput.Normal);
        }
        function test_editingRoundTripsToTextProperty() {
            tf.text = "";
            var input = findChild(tf, "fieldInput");
            input.text = "New Value";
            compare(tf.text, "New Value");
        }
    }
```

- [ ] **Run it, expect FAIL** (build fails: `LTextField is not a type`).

- [ ] **Implement `qt-app/quick/qml/components/LTextField.qml`:**
```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Labelled single-line text field (§11 form primitive). Colors/spacing/radius
// come only from Theme (§12.8) — zero raw hex. Two-way `text` alias so a screen
// can bind `text: vm.schoolName` and read edits back. isPassword flips the
// echo mode for the admin-key entry.
ColumnLayout {
    id: root
    property alias text: input.text
    property string label: ""
    property string placeholder: ""
    property bool isPassword: false
    spacing: Theme.spacing.xs

    Text {
        visible: root.label.length > 0
        text: root.label
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.control
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        radius: Theme.radius.sm
        color: Theme.card
        border.width: 2
        border.color: input.activeFocus ? Theme.brand.admin : Theme.border

        TextInput {
            id: input
            objectName: "fieldInput"
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing.md
            anchors.rightMargin: Theme.spacing.md
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            color: Theme.text
            selectionColor: Theme.brand.admin
            selectedTextColor: Theme.brand.onPrimary
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            echoMode: root.isPassword ? TextInput.Password : TextInput.Normal

            Text {
                anchors.fill: parent
                anchors.leftMargin: 0
                verticalAlignment: Text.AlignVCenter
                visible: input.text.length === 0 && root.placeholder.length > 0
                text: root.placeholder
                color: Theme.mutedTextCaption
                font: input.font
            }
        }
    }
    Accessible.role: Accessible.EditableText
    Accessible.name: root.label
}
```

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_qml_components
```
Expected: `LTextFieldBehavior` cases pass; `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/qml/components/LTextField.qml qt-app/quick/CMakeLists.txt qt-app/quick/tests/tst_qml_components.qml
git commit -m "feat(loams2): add LTextField Theme-tokened field with password mode (Phase 4c)"
```

---

### Task 5: `LConfirmDialog` — tiered destructive-op dialog

**Files:**
- Create: `qt-app/quick/qml/components/LConfirmDialog.qml`
- Modify: `qt-app/quick/CMakeLists.txt`, `qt-app/quick/tests/tst_qml_components.qml`

- [ ] **Register the QML file.** Add `qml/components/LConfirmDialog.qml` to `QML_FILES`.

- [ ] **Write the failing test.** Add fixtures to `tst_qml_components.qml`:
```qml
    LConfirmDialog { id: cd1; tier: 1; title: "Confirm"; message: "Proceed?" }
    LConfirmDialog { id: cd2; tier: 2; title: "Reset Visits"; message: "Deletes history."; confirmText: "Reset" }
```
and a TestCase:
```qml
    TestCase {
        name: "LConfirmDialogTiers"
        when: windowShown
        function init() {
            cd1.visible = false; cd1.busy = false;
            cd2.visible = false; cd2.busy = false;
            var keyField = findChild(cd2, "confirmKeyField");
            if (keyField) keyField.text = "";
        }

        function test_tier1HasNoKeyField() {
            compare(findChild(cd1, "confirmKeyField"), null);
        }
        function test_tier2HasKeyField() {
            verify(findChild(cd2, "confirmKeyField") !== null);
        }
        function test_tier1ConfirmEnabledByDefault() {
            var btn = findChild(cd1, "confirmButton");
            verify(btn !== null);
            compare(btn.enabled, true);
        }
        function test_tier2ConfirmDisabledUntilKeyEntered() {
            var btn = findChild(cd2, "confirmButton");
            compare(btn.enabled, false);            // key empty
            findChild(cd2, "confirmKeyField").text = "mykey";
            compare(btn.enabled, true);
        }
        function test_busyDisablesConfirmBothTiers() {
            var b1 = findChild(cd1, "confirmButton");
            cd1.busy = true;
            compare(b1.enabled, false);
            var b2 = findChild(cd2, "confirmButton");
            findChild(cd2, "confirmKeyField").text = "mykey";
            cd2.busy = true;
            compare(b2.enabled, false);             // in-flight blocks even with a key
        }
        function test_tier2ConfirmedEmitsTypedKey() {
            var got = null;
            cd2.confirmed.connect(function(k) { got = k; });
            cd2.visible = true; waitForRendering(cd2);   // invisible items get no synthesized input
            findChild(cd2, "confirmKeyField").text = "typed-key";
            mouseClick(findChild(cd2, "confirmButton"));
            compare(got, "typed-key");
        }
        function test_tier1ConfirmedEmitsEmptyKey() {
            var got = "sentinel";
            cd1.confirmed.connect(function(k) { got = k; });
            cd1.visible = true; waitForRendering(cd1);   // invisible items get no synthesized input
            mouseClick(findChild(cd1, "confirmButton"));
            compare(got, "");
        }
        function test_cancelHidesAndEmitsCancelled() {
            cd1.visible = true;
            var cancelled = 0;
            cd1.cancelled.connect(function() { cancelled++; });
            mouseClick(findChild(cd1, "cancelButton"));
            compare(cancelled, 1);
        }
    }
```

- [ ] **Run it, expect FAIL** (`LConfirmDialog is not a type`).

- [ ] **Implement `qt-app/quick/qml/components/LConfirmDialog.qml`** (composes the existing `LDialog` scrim/card, `LTextField`, `LButton`):
```qml
import QtQuick
import QtQuick.Layouts
import LOAMS

// Tiered destructive-op confirm (§3.4). tier 1 = title + message +
// Cancel/Confirm. tier 2 = also an admin-key field, and Confirm stays disabled
// until a key is typed. Confirm is disabled while `busy` (request in flight) in
// BOTH tiers. Emits confirmed(key) — key is the typed admin key for tier 2,
// "" for tier 1 (spec §4.1: tier-2 sends a freshly re-typed key).
LDialog {
    id: root
    property int tier: 1
    property bool busy: false
    property string confirmText: qsTr("Confirm")
    property string cancelText: qsTr("Cancel")
    signal confirmed(string key)
    signal cancelled()

    readonly property bool keyReady: tier !== 2 || keyField.text.trim().length > 0

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.lg

        LTextField {
            id: keyField
            objectName: "confirmKeyField"
            visible: root.tier === 2
            Layout.fillWidth: true
            label: qsTr("Re-enter Admin Key")
            isPassword: true
            // A disabled/hidden field must not gate an unrelated tier-1 dialog:
            // keyReady already special-cases tier !== 2, so a hidden field's
            // empty text is irrelevant there.
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "cancelButton"
                variant: "Outline"
                text: root.cancelText
                enabled: !root.busy
                onClicked: { root.visible = false; root.cancelled(); }
            }
            LButton {
                objectName: "confirmButton"
                variant: root.tier === 2 ? "Danger" : "Primary"
                text: root.confirmText
                enabled: !root.busy && root.keyReady
                onClicked: root.confirmed(root.tier === 2 ? keyField.text : "")
            }
        }
    }
}
```
Note: the `confirmed` handler does NOT auto-hide — the screen keeps the dialog visible while `busy` and hides it on the VM's success signal, so the in-flight disable is observable.

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_qml_components
```
Expected: `LConfirmDialogTiers` cases pass.

- [ ] **Commit:**
```
git add qt-app/quick/qml/components/LConfirmDialog.qml qt-app/quick/CMakeLists.txt qt-app/quick/tests/tst_qml_components.qml
git commit -m "feat(loams2): add tiered LConfirmDialog (tier-2 admin-key + in-flight disable) (Phase 4c)"
```

---

### Task 6: `LComboBox` — Theme-tokened flat picker

**Files:**
- Create: `qt-app/quick/qml/components/LComboBox.qml`
- Modify: `qt-app/quick/CMakeLists.txt`, `qt-app/quick/tests/tst_qml_components.qml`

- [ ] **Register the QML file.** Add `qml/components/LComboBox.qml` to `QML_FILES`.

- [ ] **Write the failing test.** Add a fixture:
```qml
    LComboBox { id: cb; model: ["CE", "IT", "BA"]; placeholder: "Select Department" }
```
and a TestCase:
```qml
    TestCase {
        name: "LComboBoxBehavior"
        when: windowShown
        function init() { cb.currentValue = ""; }
        function test_startsWithNoSelection() {
            compare(cb.currentValue, "");
        }
        function test_selectingEmitsCurrentValue() {
            var got = null;
            cb.selected.connect(function(v) { got = v; });
            cb.selectValue("IT");            // test hook == the click path
            compare(cb.currentValue, "IT");
            compare(got, "IT");
        }
        function test_bindsThemeCardTokenNotLiteral() {
            compare(cb.color, Theme.card);
        }
        function test_modelDrivesOptionCount() {
            compare(cb.count, 3);
        }
    }
```

- [ ] **Run it, expect FAIL** (`LComboBox is not a type`).

- [ ] **Implement `qt-app/quick/qml/components/LComboBox.qml`** (wrap Controls `ComboBox`, Theme-token the visuals, expose `selectValue`/`selected` seam mirroring `LSegmented`'s test-hook style):
```qml
import QtQuick
import QtQuick.Controls
import LOAMS

// Flat single-select combo (§11 form primitive) — the reset-visits department
// picker. YAGNI: NOT the cascading Dept->Course->Year selector (that is 4a/4b).
// Model is a plain string list. Colors/radius from Theme only.
ComboBox {
    id: control
    property string placeholder: ""
    property string currentValue: ""
    signal selected(string value)

    // Test/click seam: same single path a delegate click and a test call take
    // (mirrors LSegmented.selectionChanged). Keeps currentValue authoritative.
    function selectValue(v) {
        currentValue = v;
        var i = indexOfValue ? indexOfValue(v) : model.indexOf(v);
        if (i >= 0) currentIndex = i;
        selected(v);
    }

    font.family: Theme.typography.sans
    font.pixelSize: Theme.typography.body

    onActivated: function(index) {
        currentValue = String(model[index]);
        selected(currentValue);
    }

    background: Rectangle {
        implicitHeight: 40
        radius: Theme.radius.sm
        color: Theme.card
        border.width: 2
        border.color: control.activeFocus ? Theme.brand.admin : Theme.border
    }
    contentItem: Text {
        leftPadding: Theme.spacing.md
        rightPadding: Theme.spacing.md
        verticalAlignment: Text.AlignVCenter
        text: control.currentValue.length > 0 ? control.currentValue : control.placeholder
        color: control.currentValue.length > 0 ? Theme.text : Theme.mutedTextCaption
        font: control.font
        elide: Text.ElideRight
    }
    // `color` alias so the theme-token test reads the background fill directly.
    property alias color: control.background.color
}
```
> If `property alias color: control.background.color` fails to resolve (background is an assigned item, not an id'd child), instead add `id: bg` to the `background` Rectangle and write `property alias color: bg.color`. Verify by re-running the test.

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_qml_components
```
Expected: `LComboBoxBehavior` cases pass.

- [ ] **Commit:**
```
git add qt-app/quick/qml/components/LComboBox.qml qt-app/quick/CMakeLists.txt qt-app/quick/tests/tst_qml_components.qml
git commit -m "feat(loams2): add flat LComboBox picker (Phase 4c)"
```

---

## SETTINGS VIEWMODEL + SCREEN

### Task 7: `SettingsViewModel` skeleton — properties, load, dirty tracking

**Files:**
- Create: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

> **Test hermeticity (applies to every SettingsViewModel test T7–T13):** `SettingsController` persists to the fixed `QSettings("MyCompany","MyApp")` scope, which is the real registry on Windows. To keep unit tests off the developer's registry, the test's `initTestCase()` redirects that scope to an INI file in a `QTemporaryDir`:
> ```cpp
> QSettings::setDefaultFormat(QSettings::IniFormat);
> QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmp.path());
> ```
> Because `QSettings("MyCompany","MyApp")` (two-arg) uses `defaultFormat()` + `UserScope`, this lands every read/write in `m_tmp` for the test process. `QStandardPaths::setTestModeEnabled(true)` similarly redirects `AppDataLocation` (used by `importLogo`) — set it too.

- [ ] **Write the failing test.** Create `qt-app/quick/tests/tst_settingsviewmodel.cpp`:
```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "SettingsViewModel.h"

class TestSettingsViewModel : public QObject
{
    Q_OBJECT
    QTemporaryDir m_tmp;
private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmp.path());
    }
    void init()
    {
        // Seed a known baseline in the redirected QSettings before each test.
        QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
        s.clear();
        s.setValue(QStringLiteral("school/name"), QStringLiteral("Acme Library"));
        s.setValue(QStringLiteral("school/address"), QStringLiteral("123 Main St"));
        s.setValue(QStringLiteral("admin/name"), QStringLiteral("J. Rizal"));
        s.setValue(QStringLiteral("admin/position"), QStringLiteral("Head Librarian"));
        s.setValue(QStringLiteral("library/openHour"), 8);
        s.setValue(QStringLiteral("library/closeHour"), 17);
        s.setValue(QStringLiteral("features/guestLogin"), true);
        s.sync();
    }

    void loadPopulatesPropertiesFromSettings();
    void loadStartsClean();
    void editingASettingMarksDirtyAndSignals();
    void resettingToSavedValueClearsDirty();
};

void TestSettingsViewModel::loadPopulatesPropertiesFromSettings()
{
    SettingsViewModel vm;
    vm.load();
    QCOMPARE(vm.schoolName(), QStringLiteral("Acme Library"));
    QCOMPARE(vm.schoolAddress(), QStringLiteral("123 Main St"));
    QCOMPARE(vm.adminName(), QStringLiteral("J. Rizal"));
    QCOMPARE(vm.adminPosition(), QStringLiteral("Head Librarian"));
    QCOMPARE(vm.openHour(), 8);
    QCOMPARE(vm.closeHour(), 17);
    QCOMPARE(vm.guestEnabled(), true);
}

void TestSettingsViewModel::loadStartsClean()
{
    SettingsViewModel vm;
    vm.load();
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::editingASettingMarksDirtyAndSignals()
{
    SettingsViewModel vm;
    vm.load();
    QSignalSpy dirtySpy(&vm, &SettingsViewModel::dirtyChanged);
    vm.setSchoolName(QStringLiteral("New Name"));
    QVERIFY(vm.dirty());
    QVERIFY(dirtySpy.count() >= 1);
}

void TestSettingsViewModel::resettingToSavedValueClearsDirty()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("New Name"));
    QVERIFY(vm.dirty());
    vm.setSchoolName(QStringLiteral("Acme Library"));   // back to the loaded value
    QVERIFY(!vm.dirty());
}

QTEST_MAIN(TestSettingsViewModel)
#include "tst_settingsviewmodel.moc"
```

- [ ] **Register the test target.** In `qt-app/quick/CMakeLists.txt`, add after the `tst_searchviewmodel` block:
```cmake
# --- SettingsViewModel unit test (C++ QtTest, offscreen). Uses Network for
# the async POST paths (T10-T13) and Gui via witsquickmodule propagation. ---
wits_add_qttest(tst_settingsviewmodel
    SOURCES tests/tst_settingsviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)
```

- [ ] **Run it, expect FAIL** (`SettingsViewModel.h` missing).

- [ ] **Implement the skeleton.** Create `qt-app/quick/viewmodels/SettingsViewModel.h`:
```cpp
#ifndef SETTINGSVIEWMODEL_H
#define SETTINGSVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqml.h>
#include "settingscontroller.h"
#include "settingsdata.h"

class QNetworkAccessManager;

// The Settings screen's ViewModel (spec §4.1) — the ONLY QML-facing C++ for
// SettingsScreen. Owns a SettingsController (QSettings persistence + logo
// import) and posts admin-info / key-change / reset-visits via HttpForm, using
// the admin key held in AdminSession. Dirty tracking drives the Save button.
class SettingsViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString schoolName    READ schoolName    WRITE setSchoolName    NOTIFY schoolNameChanged)
    Q_PROPERTY(QString schoolAddress READ schoolAddress WRITE setSchoolAddress NOTIFY schoolAddressChanged)
    Q_PROPERTY(QString logoPath      READ logoPath      NOTIFY logoChanged)
    Q_PROPERTY(QUrl    logoUrl       READ logoUrl       NOTIFY logoChanged)
    Q_PROPERTY(bool    hasLogo       READ hasLogo       NOTIFY logoChanged)
    Q_PROPERTY(QString adminName     READ adminName     WRITE setAdminName     NOTIFY adminNameChanged)
    Q_PROPERTY(QString adminPosition READ adminPosition WRITE setAdminPosition NOTIFY adminPositionChanged)
    Q_PROPERTY(int     openHour      READ openHour      WRITE setOpenHour      NOTIFY openHourChanged)
    Q_PROPERTY(int     closeHour     READ closeHour     WRITE setCloseHour     NOTIFY closeHourChanged)
    Q_PROPERTY(bool    guestEnabled  READ guestEnabled  WRITE setGuestEnabled  NOTIFY guestEnabledChanged)
    Q_PROPERTY(bool    dirty         READ dirty         NOTIFY dirtyChanged)
    Q_PROPERTY(bool    busy          READ busy          NOTIFY busyChanged)
    Q_PROPERTY(QStringList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    explicit SettingsViewModel(QObject *parent = nullptr);

    QString schoolName() const    { return m_cur.schoolName; }
    QString schoolAddress() const { return m_cur.schoolAddress; }
    QString logoPath() const      { return m_cur.logoPath; }
    QUrl    logoUrl() const;
    bool    hasLogo() const;
    QString adminName() const     { return m_cur.adminName; }
    QString adminPosition() const { return m_cur.adminPosition; }
    int     openHour() const      { return m_cur.libraryOpenHour; }
    int     closeHour() const     { return m_cur.libraryCloseHour; }
    bool    guestEnabled() const  { return m_cur.guestLoginEnabled; }
    bool    dirty() const         { return m_dirty; }
    bool    busy() const          { return m_busy; }
    QStringList departments() const { return m_departments; }
    QString statusMessage() const { return m_statusMessage; }

    void setSchoolName(const QString &v);
    void setSchoolAddress(const QString &v);
    void setAdminName(const QString &v);
    void setAdminPosition(const QString &v);
    void setOpenHour(int v);
    void setCloseHour(int v);
    void setGuestEnabled(bool v);

    Q_INVOKABLE void load();

signals:
    void schoolNameChanged();
    void schoolAddressChanged();
    void logoChanged();
    void adminNameChanged();
    void adminPositionChanged();
    void openHourChanged();
    void closeHourChanged();
    void guestEnabledChanged();
    void dirtyChanged();
    void busyChanged();
    void departmentsChanged();
    void statusChanged();

private:
    void recomputeDirty();
    void setBusy(bool v);
    void setStatus(const QString &msg);

    SettingsController m_controller;
    QNetworkAccessManager *m_nam = nullptr;
    SettingsData m_cur;       // edited (live) values
    SettingsData m_saved;     // last loaded/saved baseline (dirty is m_cur != m_saved)
    QStringList m_departments;
    QString m_statusMessage;
    bool m_dirty = false;
    bool m_busy = false;
};

#endif // SETTINGSVIEWMODEL_H
```
Create `qt-app/quick/viewmodels/SettingsViewModel.cpp` (skeleton portion — later tasks add methods):
```cpp
#include "SettingsViewModel.h"

#include <QNetworkAccessManager>
#include <QFileInfo>

SettingsViewModel::SettingsViewModel(QObject *parent)
    : QObject(parent)
    , m_controller(this)
    , m_nam(new QNetworkAccessManager(this))
{
}

QUrl SettingsViewModel::logoUrl() const
{
    // A bare path is not a valid Image.source — mirror SchoolInfoViewModel.
    if (m_cur.logoPath.isEmpty() || !QFileInfo::exists(m_cur.logoPath))
        return {};
    return QUrl::fromLocalFile(m_cur.logoPath);
}

bool SettingsViewModel::hasLogo() const
{
    return !m_cur.logoPath.isEmpty() && QFileInfo::exists(m_cur.logoPath);
}

void SettingsViewModel::load()
{
    m_cur = m_controller.load();
    m_saved = m_cur;
    recomputeDirty();
    emit schoolNameChanged();    emit schoolAddressChanged();
    emit logoChanged();
    emit adminNameChanged();     emit adminPositionChanged();
    emit openHourChanged();      emit closeHourChanged();
    emit guestEnabledChanged();
}

void SettingsViewModel::recomputeDirty()
{
    // Field-by-field compare against the saved baseline. logoPath is included
    // so a logo import (Task 9) also marks the form dirty.
    const bool d =
        m_cur.schoolName        != m_saved.schoolName      ||
        m_cur.schoolAddress     != m_saved.schoolAddress   ||
        m_cur.logoPath          != m_saved.logoPath        ||
        m_cur.adminName         != m_saved.adminName       ||
        m_cur.adminPosition     != m_saved.adminPosition   ||
        m_cur.libraryOpenHour   != m_saved.libraryOpenHour ||
        m_cur.libraryCloseHour  != m_saved.libraryCloseHour||
        m_cur.guestLoginEnabled != m_saved.guestLoginEnabled;
    if (d != m_dirty) { m_dirty = d; emit dirtyChanged(); }
}

void SettingsViewModel::setBusy(bool v)   { if (v != m_busy) { m_busy = v; emit busyChanged(); } }
void SettingsViewModel::setStatus(const QString &msg) { m_statusMessage = msg; emit statusChanged(); }

void SettingsViewModel::setSchoolName(const QString &v)
{ if (v == m_cur.schoolName) return; m_cur.schoolName = v; emit schoolNameChanged(); recomputeDirty(); }
void SettingsViewModel::setSchoolAddress(const QString &v)
{ if (v == m_cur.schoolAddress) return; m_cur.schoolAddress = v; emit schoolAddressChanged(); recomputeDirty(); }
void SettingsViewModel::setAdminName(const QString &v)
{ if (v == m_cur.adminName) return; m_cur.adminName = v; emit adminNameChanged(); recomputeDirty(); }
void SettingsViewModel::setAdminPosition(const QString &v)
{ if (v == m_cur.adminPosition) return; m_cur.adminPosition = v; emit adminPositionChanged(); recomputeDirty(); }
void SettingsViewModel::setOpenHour(int v)
{ if (v == m_cur.libraryOpenHour) return; m_cur.libraryOpenHour = v; emit openHourChanged(); recomputeDirty(); }
void SettingsViewModel::setCloseHour(int v)
{ if (v == m_cur.libraryCloseHour) return; m_cur.libraryCloseHour = v; emit closeHourChanged(); recomputeDirty(); }
void SettingsViewModel::setGuestEnabled(bool v)
{ if (v == m_cur.guestLoginEnabled) return; m_cur.guestLoginEnabled = v; emit guestEnabledChanged(); recomputeDirty(); }
```

- [ ] **Add the VM to module SOURCES.** In `qt-app/quick/CMakeLists.txt`, inside `qt_add_qml_module(... SOURCES ...)`, add after the `SearchViewModel` line:
```cmake
        viewmodels/SettingsViewModel.h viewmodels/SettingsViewModel.cpp
```

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp qt-app/quick/CMakeLists.txt
git commit -m "feat(loams2): SettingsViewModel skeleton — properties, load, dirty tracking (Phase 4c)"
```

---

### Task 8: `SettingsViewModel::save()` + guest-key reconciliation

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`

> **RECONCILE the guest-login key mismatch.** `SettingsController::save()` writes `features/guestLogin` (settingscontroller.cpp:44), but `KioskViewModel` reads `kiosk/guestEnabled` (KioskViewModel.cpp:27). The two never met — the toggle has never reached the 2.0 kiosk. **Decision:** `SettingsViewModel::save()` persists via `SettingsController::save()` (unchanged — the legacy `adminwindow.cpp` still reads `features/guestLogin`, so the controller's key contract must not move) AND additionally mirrors the value onto `kiosk/guestEnabled`, the key the kiosk actually reads. Mirroring in the VM (not the controller) is the least-invasive fix: it leaves the shared controller and legacy reader untouched while making the toggle live for the new kiosk. The test asserts BOTH keys.

- [ ] **Write the failing test.** Add slots + bodies to `tst_settingsviewmodel.cpp`:
```cpp
    void saveePersistsAllFieldsAndEmitsSaved();   // (declare in private slots)
    void saveMirrorsGuestFlagOntoKioskKey();
    void saveClearsDirty();
```
```cpp
void TestSettingsViewModel::saveePersistsAllFieldsAndEmitsSaved()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("Renamed Library"));
    vm.setOpenHour(7);
    QSignalSpy saved(&vm, &SettingsViewModel::saved);
    vm.save();
    QCOMPARE(saved.count(), 1);
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s.value(QStringLiteral("school/name")).toString(), QStringLiteral("Renamed Library"));
    QCOMPARE(s.value(QStringLiteral("library/openHour")).toInt(), 7);
}

void TestSettingsViewModel::saveMirrorsGuestFlagOntoKioskKey()
{
    SettingsViewModel vm;
    vm.load();
    vm.setGuestEnabled(false);
    vm.save();
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    // Both the controller's key AND the key the kiosk reads must be set.
    QCOMPARE(s.value(QStringLiteral("features/guestLogin")).toBool(), false);
    QCOMPARE(s.value(QStringLiteral("kiosk/guestEnabled")).toBool(), false);

    vm.setGuestEnabled(true);
    vm.save();
    QSettings s2(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s2.value(QStringLiteral("features/guestLogin")).toBool(), true);
    QCOMPARE(s2.value(QStringLiteral("kiosk/guestEnabled")).toBool(), true);
}

void TestSettingsViewModel::saveClearsDirty()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("X"));
    QVERIFY(vm.dirty());
    vm.save();
    QVERIFY(!vm.dirty());
}
```

- [ ] **Run it, expect FAIL** (`save()` and the `saved` signal don't exist).

- [ ] **Implement.** In `SettingsViewModel.h`, add to the `Q_INVOKABLE` block `Q_INVOKABLE void save();` and to `signals:`:
```cpp
    void saved();
    void saveFailed(const QString &message);
```
In `SettingsViewModel.cpp`, add `#include <QSettings>` and:
```cpp
void SettingsViewModel::save()
{
    if (!m_controller.save(m_cur)) {
        emit saveFailed(QStringLiteral("Could not save settings."));
        return;
    }
    // RECONCILE (spec §4.1 / plan T8): SettingsController writes
    // features/guestLogin, but KioskViewModel reads kiosk/guestEnabled. Mirror
    // the flag onto the kiosk's key so the toggle actually reaches the kiosk,
    // without changing the shared controller's key contract (legacy
    // adminwindow.cpp still reads features/guestLogin).
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    s.setValue(QStringLiteral("kiosk/guestEnabled"), m_cur.guestLoginEnabled);
    s.sync();

    m_saved = m_cur;
    recomputeDirty();      // -> clears dirty
    emit saved();
}
```

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp
git commit -m "feat(loams2): SettingsViewModel.save() + reconcile guest-login key to kiosk (Phase 4c)"
```

---

### Task 9: Logo import + live re-theme

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`

> `importLogo(path)` calls `SettingsController::importLogo(path)` (copies the file into AppDataLocation, returns the dest path), persists `school/logoPath`, updates `m_cur.logoPath` (so `logoUrl`/`hasLogo`/dirty update), and emits `logoChanged`. Import failure (bad path / non-image) leaves state unchanged and surfaces a status message.
>
> **CRITICAL — the VM does NOT re-theme; that is wired in QML on the Theme singleton's own instance (T14).** `Theme.qml:10` binds every color token to its *private* `readonly property ThemeViewModel _vm: ThemeViewModel {}`. A re-theme only reaches the running UI if **that** instance's `changed()` fires. If the VM constructed its own `ThemeViewModel` and called `regenerateFromImportedLogo` on it, that would update the global `BrandTheme` but never notify `Theme._vm`, so the palette would stay stale while the test went green — a silent no-op of the flagship feature. Therefore the VM only imports+persists; T14 triggers the live re-theme by calling `Theme._vm.regenerateFromImportedLogo(vm.logoPath)` from QML (Auto brand mode; a no-op in Manual mode). There is no existing QML caller of `regenerateFromImportedLogo` — 4c is the first, so this wiring is load-bearing. End-to-end token propagation is proven by the manual GUI smoke in T16 (import a logo → the maroon palette shifts live); the engine step (a saturated logo changes a `BrandTheme` token in Auto mode) is already covered by `tst_themeviewmodel`, and the singleton binding (`Theme.*` reflects `_vm`) by `tst_qml_theme`.

- [ ] **Write the failing test.** The test uses a real generated PNG in the temp dir so `SettingsController::importImageFile` (which does `QImage(path).isNull()`) accepts it:
```cpp
    void importLogoUpdatesPathAndEmitsLogoChanged();   // declare in private slots
    void importBadPathLeavesLogoUnchanged();
```
```cpp
void TestSettingsViewModel::importLogoUpdatesPathAndEmitsLogoChanged()
{
    // Write a genuine 2x2 PNG into the temp dir so importImageFile accepts it.
    const QString src = m_tmp.path() + QStringLiteral("/src_logo.png");
    QImage img(2, 2, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(src, "PNG"));

    SettingsViewModel vm;
    vm.load();
    QSignalSpy logo(&vm, &SettingsViewModel::logoChanged);
    vm.importLogo(src);

    QVERIFY(logo.count() >= 1);
    QVERIFY(!vm.logoPath().isEmpty());
    QVERIFY(vm.hasLogo());
    QVERIFY(vm.logoUrl().isValid());
    QVERIFY(vm.dirty());   // an import is an unsaved change
}

void TestSettingsViewModel::importBadPathLeavesLogoUnchanged()
{
    SettingsViewModel vm;
    vm.load();
    const QString before = vm.logoPath();
    vm.importLogo(m_tmp.path() + QStringLiteral("/does_not_exist.png"));
    QCOMPARE(vm.logoPath(), before);   // unchanged
}
```
Add `#include <QImage>` to the test.

- [ ] **Run it, expect FAIL** (`importLogo` missing).

- [ ] **Implement.** In `SettingsViewModel.h`, add `Q_INVOKABLE void importLogo(const QString &sourcePath);` (NO `ThemeViewModel` member — see the CRITICAL note above; the VM must not own a second instance). In `SettingsViewModel.cpp`:
```cpp
void SettingsViewModel::importLogo(const QString &sourcePath)
{
    const QString dest = m_controller.importLogo(sourcePath);
    if (dest.isEmpty()) {
        // SettingsController already emitted importError; surface a status.
        setStatus(QStringLiteral("Could not import logo."));
        return;
    }
    m_cur.logoPath = dest;
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    s.setValue(QStringLiteral("school/logoPath"), dest);
    s.sync();

    emit logoChanged();
    recomputeDirty();
    // NB: no re-theme here. The live palette re-extraction runs in QML on
    // Theme._vm (T14) — see the CRITICAL note above for why a VM-owned
    // ThemeViewModel would not reach the running UI.
}
```

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp
git commit -m "feat(loams2): SettingsViewModel logo import + live re-theme (Phase 4c)"
```

---

### Task 10: Admin info save (POST `update_admin_info.php`)

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`

> POSTs `admin_name` + `admin_position` + `admin_key` (from `AdminSession`) to `update_admin_info.php` via `HttpForm::submit`. The response decode is a **network-free seam** `applyAdminInfoResponse(const QByteArray&)` classifying `{"status":...,"message":...}` → `adminInfoSaved()` / `adminInfoFailed(msg)`; a 401-style body (`"Invalid admin key"` / `"Admin authentication required"`) → `authFailed()`. **KNOWN LIMITATION (see top):** in production `HttpForm` routes an HTTP-401 reply to `onNetworkError`, so `authFailed()` fires from a live 401 only if the server returns 200 with an error body; the synthetic-payload unit test still validates the decode logic per spec §6.1.

- [ ] **Write the failing test.** Add slots + bodies:
```cpp
    void adminInfoSuccessEmitsSaved();
    void adminInfoErrorEmitsFailedWithMessage();
    void adminInfoAuthFailureEmitsAuthFailed();
```
```cpp
void TestSettingsViewModel::adminInfoSuccessEmitsSaved()
{
    SettingsViewModel vm;
    QSignalSpy ok(&vm, &SettingsViewModel::adminInfoSaved);
    vm.applyAdminInfoResponse(R"({"status":"success","message":"Admin info updated successfully."})");
    QCOMPARE(ok.count(), 1);
}

void TestSettingsViewModel::adminInfoErrorEmitsFailedWithMessage()
{
    SettingsViewModel vm;
    QSignalSpy bad(&vm, &SettingsViewModel::adminInfoFailed);
    vm.applyAdminInfoResponse(R"({"status":"error","message":"Failed to update admin info."})");
    QCOMPARE(bad.count(), 1);
    QCOMPARE(bad.at(0).at(0).toString(), QStringLiteral("Failed to update admin info."));
}

void TestSettingsViewModel::adminInfoAuthFailureEmitsAuthFailed()
{
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyAdminInfoResponse(R"({"status":"error","message":"Invalid admin key"})");
    QCOMPARE(auth.count(), 1);
}
```

- [ ] **Run it, expect FAIL.**

- [ ] **Implement.** In `SettingsViewModel.h`, add `#include <QByteArray>`, add to the invokable block `Q_INVOKABLE void saveAdminInfo();`, add the seam `void applyAdminInfoResponse(const QByteArray &json);`, and to `signals:`:
```cpp
    void adminInfoSaved();
    void adminInfoFailed(const QString &message);
    void authFailed();
    void networkError();
```
Add a small private helper declaration for shared auth detection:
```cpp
    static bool isAuthFailureMessage(const QString &message);
```
In `SettingsViewModel.cpp`, add `#include <QJsonDocument>`, `#include <QJsonObject>`, `#include "apiconfig.h"`, `#include "HttpForm.h"`, `#include "AdminSession.h"`:
```cpp
bool SettingsViewModel::isAuthFailureMessage(const QString &m)
{
    // The two strings requireAdminAuth / the guard return on a bad/missing key.
    return m.contains(QStringLiteral("Invalid admin key"), Qt::CaseInsensitive)
        || m.contains(QStringLiteral("authentication required"), Qt::CaseInsensitive);
}

void SettingsViewModel::applyAdminInfoResponse(const QByteArray &json)
{
    setBusy(false);
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    const QString status = obj.value(QStringLiteral("status")).toString();
    const QString message = obj.value(QStringLiteral("message")).toString();
    if (status == QLatin1String("success")) {
        emit adminInfoSaved();
    } else if (isAuthFailureMessage(message)) {
        emit authFailed();
    } else {
        emit adminInfoFailed(message.isEmpty()
            ? QStringLiteral("Failed to update admin info.") : message);
    }
}

void SettingsViewModel::saveAdminInfo()
{
    setBusy(true);
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("admin_name"), m_cur.adminName},
        {QStringLiteral("admin_position"), m_cur.adminPosition},
        {QStringLiteral("admin_key"), AdminSession::instance().key()},
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("update_admin_info.php")),
                     fields, this,
        [this](const QByteArray &body) { applyAdminInfoResponse(body); },
        [this]() { setBusy(false); emit networkError(); });   // see KNOWN LIMITATION: 401 lands here
}
```

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp
git commit -m "feat(loams2): SettingsViewModel admin-info save + response decode (Phase 4c)"
```

---

### Task 11: Admin key change (POST `update_admin_key.php`) + session refresh

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`

> POSTs `old_key` + `new_key` to `update_admin_key.php` (server bcrypt-verifies the old key). On success, `AdminSession::refresh(newKey)` so subsequent destructive ops use the new key (spec §3.3). `update_admin_key.php` is NOT guarded by `requireAdminAuth` (it already requires the old key), so no `admin_key` field is sent. The decode seam `applyKeyChangeResponse(const QByteArray&, const QString& newKey)` needs the new key to hand to `refresh()` on success.

- [ ] **Write the failing test.** Add slots + bodies:
```cpp
    void keyChangeSuccessRefreshesSession();
    void keyChangeWrongOldKeyEmitsFailedAndKeepsSession();
```
```cpp
void TestSettingsViewModel::keyChangeSuccessRefreshesSession()
{
    AdminSession::instance().setKey(QStringLiteral("OLDKEY"));
    SettingsViewModel vm;
    QSignalSpy ok(&vm, &SettingsViewModel::keyChanged);
    vm.applyKeyChangeResponse(R"({"status":"success","message":"Password updated successfully."})",
                              QStringLiteral("NEWKEY"));
    QCOMPARE(ok.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("NEWKEY"));
}

void TestSettingsViewModel::keyChangeWrongOldKeyEmitsFailedAndKeepsSession()
{
    AdminSession::instance().setKey(QStringLiteral("OLDKEY"));
    SettingsViewModel vm;
    QSignalSpy bad(&vm, &SettingsViewModel::keyChangeFailed);
    vm.applyKeyChangeResponse(R"({"status":"error","message":"Old password is incorrect."})",
                              QStringLiteral("NEWKEY"));
    QCOMPARE(bad.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("OLDKEY"));   // unchanged
}
```
Add `#include "AdminSession.h"` to the test file.

- [ ] **Run it, expect FAIL.**

- [ ] **Implement.** In `SettingsViewModel.h`, add `Q_INVOKABLE void changeAdminKey(const QString &oldKey, const QString &newKey);`, the seam `void applyKeyChangeResponse(const QByteArray &json, const QString &newKey);`, and signals:
```cpp
    void keyChanged();
    void keyChangeFailed(const QString &message);
```
In `SettingsViewModel.cpp`:
```cpp
void SettingsViewModel::applyKeyChangeResponse(const QByteArray &json, const QString &newKey)
{
    setBusy(false);
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    if (obj.value(QStringLiteral("status")).toString() == QLatin1String("success")) {
        AdminSession::instance().refresh(newKey);   // same-session key change (§3.3)
        emit keyChanged();
    } else {
        emit keyChangeFailed(obj.value(QStringLiteral("message")).toString());
    }
}

void SettingsViewModel::changeAdminKey(const QString &oldKey, const QString &newKey)
{
    setBusy(true);
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("old_key"), oldKey},
        {QStringLiteral("new_key"), newKey},
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("update_admin_key.php")),
                     fields, this,
        [this, newKey](const QByteArray &body) { applyKeyChangeResponse(body, newKey); },
        [this]() { setBusy(false); emit networkError(); });
}
```

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp
git commit -m "feat(loams2): SettingsViewModel admin-key change + AdminSession refresh (Phase 4c)"
```

---

### Task 12: Load departments (GET `get_departments.php`)

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`

> GETs `get_departments.php` and reuses the shipped, unit-tested parser `ReportController::parseDepartments(const QByteArray&)` (reportcontroller.cpp:55) to fill the flat department combo. Decode via a network-free seam `applyDepartmentsResponse(const QByteArray&)`. The parser returns `QStringList()` on error/empty → `m_departments` stays empty (the picker shows the placeholder).

- [ ] **Write the failing test.** Add slots + bodies:
```cpp
    void departmentsParseFillsList();
    void departmentsErrorLeavesListEmpty();
```
```cpp
void TestSettingsViewModel::departmentsParseFillsList()
{
    SettingsViewModel vm;
    QSignalSpy spy(&vm, &SettingsViewModel::departmentsChanged);
    vm.applyDepartmentsResponse(R"({"status":"success","departments":["CE","IT","BA"]})");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(vm.departments(), (QStringList{"CE","IT","BA"}));
}

void TestSettingsViewModel::departmentsErrorLeavesListEmpty()
{
    SettingsViewModel vm;
    vm.applyDepartmentsResponse(R"({"status":"error","message":"No departments found"})");
    QVERIFY(vm.departments().isEmpty());
}
```

- [ ] **Run it, expect FAIL.**

- [ ] **Implement.** In `SettingsViewModel.h`, add `Q_INVOKABLE void loadDepartments();` and `void applyDepartmentsResponse(const QByteArray &json);`. In `SettingsViewModel.cpp`, add `#include "reportcontroller.h"` and:
```cpp
void SettingsViewModel::applyDepartmentsResponse(const QByteArray &json)
{
    const QStringList parsed = ReportController::parseDepartments(json);
    if (parsed == m_departments)
        return;
    m_departments = parsed;
    emit departmentsChanged();
}

void SettingsViewModel::loadDepartments()
{
    // GET (no body) — HttpForm is POST-only, so use the NAM directly here,
    // mirroring ReportController's own GET helpers.
    QNetworkReply *reply =
        m_nam->get(QNetworkRequest(ApiConfig::endpoint(QStringLiteral("get_departments.php"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const bool netErr = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (netErr) { emit networkError(); return; }
        applyDepartmentsResponse(body);
    });
}
```
Add `#include <QNetworkReply>` and `#include <QNetworkRequest>` to the `.cpp`.

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp
git commit -m "feat(loams2): SettingsViewModel load departments via parseDepartments (Phase 4c)"
```

---

### Task 13: Reset-visits (tier-2) + export-before-destroy CSV serializer

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h`, `qt-app/quick/viewmodels/SettingsViewModel.cpp`, `qt-app/quick/tests/tst_settingsviewmodel.cpp`

> Three deliverables: (1) a **pure CSV serializer** `serializeCsv(headers, rows)` (RFC-4180-ish quoting), unit-tested standalone; (2) `resetVisits(department, adminKey)` posting `reset_visits.php` with `department` + a **FRESH** `admin_key` (the re-typed key from the tier-2 dialog, NOT the held session key — spec §3.3), guarded by an empty-department check, with the `applyResetVisitsResponse` decode seam; (3) `writeResetManifest(department, fileUrl)` — writes the honest **Reset Manifest** (below).
>
> **Reset Manifest — NOT a visit backup (owner decision 2026-07-19).** 4c has no visits-by-department fetch, so it must NOT fabricate a partial backup and present it as one. The manifest records only what 4c genuinely has: the *operation metadata* — Department, Reset timestamp, Operator (the configured `admin/name`), and a "Reset requested" confirmation. It is named `Reset_Manifest_<Department>_<Timestamp>.csv`, and both the UI copy (T14) and this file state the limitation. The **visit count** and the **full pre-reset visit-row export** are deferred to **Phase 4a**, when the visit-fetch path exists — 4a replaces/extends this manifest with the real export. Do NOT label the file "export" or "backup," and do NOT fake a row fetch here.

- [ ] **Write the failing test.** Add slots + bodies:
```cpp
    void serializeCsvQuotesSpecialCells();
    void resetVisitsEmptyDepartmentIsGuarded();
    void resetVisitsSuccessEmitsReset();
    void resetVisitsAuthFailureEmitsAuthFailed();
    void writeResetManifestContainsMetadataNotBackup();
```
```cpp
void TestSettingsViewModel::serializeCsvQuotesSpecialCells()
{
    const QStringList headers{ "Name", "Note" };
    const QList<QStringList> rows{
        { "Ana Cruz",       "ok" },
        { "O'Hara, Liam",   "has \"quote\" and, comma" },
    };
    const QString csv = SettingsViewModel::serializeCsv(headers, rows);
    const QString expected =
        "Name,Note\r\n"
        "Ana Cruz,ok\r\n"
        "\"O'Hara, Liam\",\"has \"\"quote\"\" and, comma\"\r\n";
    QCOMPARE(csv, expected);
}

void TestSettingsViewModel::resetVisitsEmptyDepartmentIsGuarded()
{
    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    QSignalSpy failed(&vm, &SettingsViewModel::resetFailed);
    vm.resetVisits(QString(), QStringLiteral("key"));   // empty dept -> no POST
    QCOMPARE(reset.count(), 0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(!vm.busy());                                // never went busy
}

void TestSettingsViewModel::resetVisitsSuccessEmitsReset()
{
    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    vm.applyResetVisitsResponse(R"({"status":"success","message":"Visit counts reset and visit history cleared for department: CE"})");
    QCOMPARE(reset.count(), 1);
}

void TestSettingsViewModel::resetVisitsAuthFailureEmitsAuthFailed()
{
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyResetVisitsResponse(R"({"status":"error","message":"Invalid admin key"})");
    QCOMPARE(auth.count(), 1);
}

void TestSettingsViewModel::writeResetManifestContainsMetadataNotBackup()
{
    SettingsViewModel vm;
    vm.load();
    vm.setProperty("adminName", QStringLiteral("Jane Librarian"));   // Operator
    const QString path = m_tmp.path() + QStringLiteral("/manifest.csv");
    QVERIFY(vm.writeResetManifest(QStringLiteral("CE"), QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csv = QString::fromUtf8(f.readAll());
    QVERIFY(csv.contains(QStringLiteral("Department")));
    QVERIFY(csv.contains(QStringLiteral("CE")));               // the reset department
    QVERIFY(csv.contains(QStringLiteral("Jane Librarian")));   // Operator
    QVERIFY(csv.contains(QStringLiteral("Reset requested")));
    QVERIFY(!csv.contains(QStringLiteral("Backup")));          // never labeled a backup
}
```
Add `#include <QFile>`, `#include <QUrl>` to the test if not present.

- [ ] **Run it, expect FAIL.**

- [ ] **Implement.** In `SettingsViewModel.h`, add:
```cpp
    // Pure, network-free CSV serializer (export-before-destroy). RFC-4180-ish:
    // CRLF line endings; a cell is quoted iff it contains a comma, quote, CR,
    // or LF; embedded quotes are doubled. Unit-tested standalone.
    static QString serializeCsv(const QStringList &headers,
                                const QList<QStringList> &rows);

    Q_INVOKABLE void resetVisits(const QString &department, const QString &adminKey);
    void applyResetVisitsResponse(const QByteArray &json);

    // Reset Manifest — operation metadata only, NOT a visit backup (owner
    // decision 2026-07-19). Full pre-reset row export lands in Phase 4a.
    Q_INVOKABLE bool writeResetManifest(const QString &department, const QUrl &fileUrl);
```
and signals:
```cpp
    void visitsReset();
    void resetFailed(const QString &message);
```
In `SettingsViewModel.cpp`:
```cpp
QString SettingsViewModel::serializeCsv(const QStringList &headers,
                                        const QList<QStringList> &rows)
{
    auto cell = [](const QString &v) -> QString {
        const bool needsQuote = v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
                             || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'));
        if (!needsQuote)
            return v;
        QString q = v;
        q.replace(QLatin1Char('"'), QLatin1String("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    };
    auto line = [&cell](const QStringList &cells) -> QString {
        QStringList out;
        out.reserve(cells.size());
        for (const QString &c : cells) out << cell(c);
        return out.join(QLatin1Char(',')) + QLatin1String("\r\n");
    };
    QString csv = line(headers);
    for (const QStringList &r : rows) csv += line(r);
    return csv;
}

void SettingsViewModel::applyResetVisitsResponse(const QByteArray &json)
{
    setBusy(false);
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    const QString status = obj.value(QStringLiteral("status")).toString();
    const QString message = obj.value(QStringLiteral("message")).toString();
    if (status == QLatin1String("success")) {
        emit visitsReset();
    } else if (isAuthFailureMessage(message)) {
        emit authFailed();
    } else {
        emit resetFailed(message.isEmpty() ? QStringLiteral("Reset failed.") : message);
    }
}

void SettingsViewModel::resetVisits(const QString &department, const QString &adminKey)
{
    if (department.trimmed().isEmpty() || department == QLatin1String("All")) {
        emit resetFailed(QStringLiteral("Please select a department."));
        return;
    }
    setBusy(true);
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("department"), department},
        {QStringLiteral("admin_key"), adminKey},   // FRESH re-typed key (§3.3), not the held one
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("reset_visits.php")),
                     fields, this,
        [this](const QByteArray &body) { applyResetVisitsResponse(body); },
        [this]() { setBusy(false); emit networkError(); });
}

bool SettingsViewModel::writeResetManifest(const QString &department, const QUrl &fileUrl)
{
    // Operation metadata only — 4c has no visits-by-department fetch, so we do
    // NOT fabricate a partial backup. The count + full row export land in 4a.
    const QStringList headers{ QStringLiteral("Field"), QStringLiteral("Value") };
    const QList<QStringList> rows{
        { QStringLiteral("Document"),        QStringLiteral("Visit Reset Manifest") },
        { QStringLiteral("Department"),      department },
        { QStringLiteral("Reset timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate) },
        { QStringLiteral("Operator"),        m_cur.adminName },
        { QStringLiteral("Reset requested"), QStringLiteral("yes") },
        { QStringLiteral("Note"), QStringLiteral("Records what was reset. Full pre-reset visit-row export arrives in Phase 4a.") },
    };
    const QString csv = serializeCsv(headers, rows);
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    f.write(csv.toUtf8());
    f.close();
    return true;
}
```
Add `#include <QDateTime>`, `#include <QFile>`, `#include <QUrl>` to `SettingsViewModel.cpp` if not already present.

- [ ] **Build + run, expect PASS:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_settingsviewmodel
```
Expected: `100% tests passed`.

- [ ] **Commit:**
```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/tests/tst_settingsviewmodel.cpp
git commit -m "feat(loams2): SettingsViewModel reset-visits (tier-2) + CSV serializer (Phase 4c)"
```

---

### Task 14: `SettingsScreen.qml` — compose all sections

**Files:**
- Create: `qt-app/quick/qml/admin/SettingsScreen.qml`
- Modify: `qt-app/quick/qml/admin/AdminScreen.qml`, `qt-app/quick/CMakeLists.txt`

> Compose the sections in `LCard`s: **School** (name/address via `LTextField`, logo preview + Import button), **Administrator** (name/position + a change-key subform: old/new/confirm `LTextField`s + Change button), **Library Hours** (open/close via `LComboBox` or `SpinBox`), **Guest Login** (a `Switch`/toggle), **Reset Visits** (department `LComboBox` + Export-CSV + Reset button → `LConfirmDialog` tier 2). A footer **Save** button, disabled while `!vm.dirty || vm.busy`. `property var vm`. Replace the Settings placeholder in `AdminScreen.qml`'s pageLoader with the real screen.

- [ ] **Register the QML file.** Add `qml/admin/SettingsScreen.qml` to `QML_FILES`.

- [ ] **Implement `qt-app/quick/qml/admin/SettingsScreen.qml`** (all colors/spacing from `Theme`; every interactive node gets an `objectName` for T15):
```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Settings screen (spec §4.1). property var vm so QuickTests inject a stub.
Rectangle {
    id: screen
    property var vm
    color: Theme.appBackground

    // NB: no Component.onCompleted network call here — loadDepartments() is
    // gated by AdminScreen's Loader.onLoaded (see the swap-placeholder step
    // below) so QuickTests with a stub vm stay network-free.

    Flickable {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        contentHeight: content.implicitHeight
        clip: true

        ColumnLayout {
            id: content
            objectName: "settingsContent"
            width: parent.width
            spacing: Theme.spacing.xl

            // --- School identity ---
            LCard {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("School"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    LTextField { objectName: "schoolNameField"; Layout.fillWidth: true
                                 label: qsTr("Name"); text: vm ? vm.schoolName : ""
                                 onTextChanged: if (vm) vm.schoolName = text }
                    LTextField { objectName: "schoolAddressField"; Layout.fillWidth: true
                                 label: qsTr("Address"); text: vm ? vm.schoolAddress : ""
                                 onTextChanged: if (vm) vm.schoolAddress = text }
                    RowLayout {
                        spacing: Theme.spacing.md
                        Image {
                            objectName: "logoPreview"
                            width: 48; height: 48; fillMode: Image.PreserveAspectFit
                            source: vm && vm.hasLogo ? vm.logoUrl : ""
                            visible: vm ? vm.hasLogo : false
                        }
                        LButton { objectName: "importLogoButton"; variant: "Outline"
                                  text: qsTr("Import Logo…")
                                  onClicked: logoDialog.open() }
                    }
                }
            }

            // --- Administrator ---
            LCard {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("Administrator"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    LTextField { objectName: "adminNameField"; Layout.fillWidth: true
                                 label: qsTr("Name"); text: vm ? vm.adminName : ""
                                 onTextChanged: if (vm) vm.adminName = text }
                    LTextField { objectName: "adminPositionField"; Layout.fillWidth: true
                                 label: qsTr("Position"); text: vm ? vm.adminPosition : ""
                                 onTextChanged: if (vm) vm.adminPosition = text }
                    LButton { objectName: "saveAdminInfoButton"; variant: "Primary"
                              text: qsTr("Save Admin Info"); enabled: vm ? !vm.busy : false
                              onClicked: if (vm) vm.saveAdminInfo() }

                    // Change-key subform
                    Text { text: qsTr("Change Admin Key"); color: Theme.mutedText
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.control }
                    LTextField { id: oldKeyField; objectName: "oldKeyField"; Layout.fillWidth: true
                                 label: qsTr("Current Key"); isPassword: true }
                    LTextField { id: newKeyField; objectName: "newKeyField"; Layout.fillWidth: true
                                 label: qsTr("New Key"); isPassword: true }
                    LTextField { id: confirmKeyField; objectName: "confirmNewKeyField"; Layout.fillWidth: true
                                 label: qsTr("Confirm New Key"); isPassword: true }
                    LButton {
                        objectName: "changeKeyButton"; variant: "Danger"
                        text: qsTr("Change Key")
                        enabled: vm ? (!vm.busy
                                   && newKeyField.text.length > 0
                                   && newKeyField.text === confirmKeyField.text) : false
                        onClicked: if (vm) vm.changeAdminKey(oldKeyField.text, newKeyField.text)
                    }
                }
            }

            // --- Library hours + guest toggle ---
            LCard {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("Library Hours"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    RowLayout {
                        spacing: Theme.spacing.md
                        SpinBox { objectName: "openHourSpin"; from: 0; to: 23
                                  value: vm ? vm.openHour : 8
                                  onValueModified: if (vm) vm.openHour = value }
                        Text { text: qsTr("to"); color: Theme.mutedText
                               font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
                        SpinBox { objectName: "closeHourSpin"; from: 0; to: 23
                                  value: vm ? vm.closeHour : 17
                                  onValueModified: if (vm) vm.closeHour = value }
                    }
                    RowLayout {
                        spacing: Theme.spacing.md
                        Text { text: qsTr("Guest Login"); color: Theme.text
                               font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
                        Switch { objectName: "guestToggle"
                                 checked: vm ? vm.guestEnabled : false
                                 onToggled: if (vm) vm.guestEnabled = checked }
                    }
                }
            }

            // --- Reset visits (tier-2 destructive) ---
            LCard {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("Reset Visits"); color: Theme.error
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    Text { text: qsTr("Permanently deletes the selected department's visit history and cannot be undone. Download the Reset Manifest first — it records what was reset (department, time, operator). The full pre-reset visit-row export arrives in Phase 4a when the visit-fetch path exists.")
                           color: Theme.mutedText; wrapMode: Text.WordWrap; Layout.fillWidth: true
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
                    LComboBox { id: deptPicker; objectName: "resetDeptPicker"; Layout.fillWidth: true
                                placeholder: qsTr("Select Department")
                                model: vm ? vm.departments : [] }
                    RowLayout {
                        spacing: Theme.spacing.md
                        LButton { objectName: "manifestButton"; variant: "Outline"
                                  text: qsTr("Download Reset Manifest")
                                  enabled: deptPicker.currentValue.length > 0
                                  onClicked: manifestSaveDialog.open() }
                        LButton { objectName: "resetVisitsButton"; variant: "Danger"
                                  text: qsTr("Reset Visits…")
                                  enabled: deptPicker.currentValue.length > 0
                                  onClicked: { resetConfirm.busy = Qt.binding(function(){ return vm ? vm.busy : false });
                                               resetConfirm.visible = true } }
                    }
                }
            }

            // --- Footer save ---
            LButton {
                objectName: "saveButton"
                Layout.alignment: Qt.AlignRight
                variant: "Primary"
                text: qsTr("Save")
                enabled: vm ? (vm.dirty && !vm.busy) : false
                onClicked: if (vm) vm.save()
            }
        }
    }

    // Tier-2 reset-visits confirm. On confirmed(key): fresh re-typed key -> VM.
    LConfirmDialog {
        id: resetConfirm
        objectName: "resetConfirmDialog"
        tier: 2
        title: qsTr("Reset Visits")
        message: qsTr("This permanently deletes the department's visit history and cannot be undone.")
        confirmText: qsTr("Reset")
        onConfirmed: function(key) { if (vm) vm.resetVisits(deptPicker.currentValue, key) }
        onCancelled: visible = false
    }
    // Hide the dialog once the VM reports the reset finished.
    Connections {
        target: vm
        function onVisitsReset() { resetConfirm.visible = false }
    }

    // File dialogs are wired at the shell level (T16) in production; declared
    // here as stubs so the screen is self-contained. QuickTests never open
    // native dialogs — they drive the VM directly (T15).
    FileDialog { id: logoDialog; nameFilters: ["Images (*.png *.jpg *.jpeg *.gif)"]
                 onAccepted: {
                     if (!vm) return;
                     vm.importLogo(selectedFile);
                     // Live re-theme on the SAME instance Theme.qml binds to
                     // (Theme._vm) — NOT a VM-owned ThemeViewModel. See Task 9's
                     // CRITICAL note. Auto brand mode only; no-op in Manual.
                     if (vm.hasLogo) Theme._vm.regenerateFromImportedLogo(vm.logoPath);
                 } }
    // Reset Manifest (NOT a visit backup): records what was reset. onAccepted
    // asks the VM to write the metadata manifest via serializeCsv (T13).
    FileDialog { id: manifestSaveDialog; fileMode: FileDialog.SaveFile; defaultSuffix: "csv"
                 currentFile: "Reset_Manifest_" + deptPicker.currentValue + ".csv"
                 onAccepted: if (vm) vm.writeResetManifest(deptPicker.currentValue, selectedFile) }
}
```
> `FileDialog` is in `QtQuick.Dialogs` — add `import QtQuick.Dialogs` at the top. If the offscreen QuickTest platform cannot construct `FileDialog`, wrap the two dialogs in a `Loader` gated by a `property bool nativeDialogs: true` that T15 sets false, OR move the dialogs behind `Qt.createComponent`. Verify at T15's first run; the simplest fix if it fails is to guard construction with `active: screen.nativeDialogs`.

- [ ] **Swap the placeholder in `AdminScreen.qml`.** Replace the `settingsPlaceholderComponent` (added in T2) with the real screen + VM. Add a VM instance near the others (line ~33):
```qml
    SettingsViewModel { id: settingsVm }
```
and change the settings Component to:
```qml
    Component { id: settingsComponent; SettingsScreen { objectName: "settingsPage"; vm: settingsVm } }
```
and the pageLoader switch case to `case Navigator.Settings: return settingsComponent;`. Gate the initial department load through `AdminScreen`'s existing `Loader.onLoaded` (the established pattern — the screen deliberately has NO `Component.onCompleted` network call, so QuickTests with a stub vm stay network-free). Since `loadDepartments` ≠ `refresh`, add the one extra guarded line:
```qml
                onLoaded: {
                    if (admin.autoLoad && item && item.vm && item.vm.refresh)
                        item.vm.refresh()
                    if (admin.autoLoad && item && item.vm && item.vm.loadDepartments)
                        item.vm.loadDepartments()
                }
```

- [ ] **Build, expect clean compile** (no test yet — T15 adds it):
```
cmake --build qt-app/build
```
Expected: builds with no new warnings.

- [ ] **Commit:**
```
git add qt-app/quick/qml/admin/SettingsScreen.qml qt-app/quick/qml/admin/AdminScreen.qml qt-app/quick/CMakeLists.txt
git commit -m "feat(loams2): SettingsScreen composing all Settings sections (Phase 4c)"
```

---

### Task 15: SettingsScreen QuickTest (stub-VM flows)

**Files:**
- Modify: `qt-app/quick/tests/tst_qml_admin.qml`

> Inject a plain-QML stub VM (mirroring the Dashboard/Search stubs already in this file) and drive save / admin-info / key-change / reset-visits flows. Assert VM calls, the tier-2 dialog, and Save disabled while busy. Give the SettingsScreen its own y-band below `vmlessLogs` per the file's geometry ledger.

- [ ] **Add the stub + instance.** In `tst_qml_admin.qml`, after the Visit Logs fixtures, add:
```qml
    // --- Settings stub VM ---
    QtObject {
        id: settingsVmStub
        property string schoolName: "Acme Library"
        property string schoolAddress: "123 Main St"
        property string logoPath: ""
        property url logoUrl: ""
        property bool hasLogo: false
        property string adminName: "J. Rizal"
        property string adminPosition: "Head Librarian"
        property int openHour: 8
        property int closeHour: 17
        property bool guestEnabled: true
        property bool dirty: false
        property bool busy: false
        property var departments: ["CE", "IT", "BA"]
        property string statusMessage: ""
        // call counters / captured args
        property int saveCount: 0
        property int saveAdminInfoCount: 0
        property string lastOldKey: ""
        property string lastNewKey: ""
        property string lastResetDept: ""
        property string lastResetKey: ""
        property int loadDepartmentsCount: 0
        signal visitsReset()
        function save() { saveCount++ }
        function saveAdminInfo() { saveAdminInfoCount++ }
        function changeAdminKey(o, n) { lastOldKey = o; lastNewKey = n }
        function resetVisits(d, k) { lastResetDept = d; lastResetKey = k }
        function importLogo(p) { logoPath = p }
        function loadDepartments() { loadDepartmentsCount++ }
    }
    SettingsScreen { id: settings; y: 2160; width: 1100; height: 900; vm: settingsVmStub }
    SettingsScreen { id: vmlessSettings; x: 2000; y: 1560; width: 1000; height: 700 }
```
Bump the root `Item { height: ... }` to at least `3100` to give the new band room.

- [ ] **Add the TestCase.** Append a new `TestCase` inside the root `Item`:
```qml
    TestCase {
        name: "SettingsScreen"
        when: windowShown
        function init() {
            settingsVmStub.dirty = false;
            settingsVmStub.busy = false;
            settingsVmStub.saveCount = 0;
            settingsVmStub.saveAdminInfoCount = 0;
            settingsVmStub.lastOldKey = "";
            settingsVmStub.lastNewKey = "";
            settingsVmStub.lastResetDept = "";
            settingsVmStub.lastResetKey = "";
            var d = findChild(settings, "resetConfirmDialog");
            if (d) d.visible = false;
        }

        function test_saveDisabledUntilDirty() {
            var btn = findChild(settings, "saveButton");
            verify(btn !== null);
            compare(btn.enabled, false);          // not dirty
            settingsVmStub.dirty = true;
            compare(btn.enabled, true);
        }
        function test_saveDisabledWhileBusy() {
            settingsVmStub.dirty = true;
            settingsVmStub.busy = true;
            compare(findChild(settings, "saveButton").enabled, false);
        }
        function test_saveInvokesVmSave() {
            settingsVmStub.dirty = true;
            waitForRendering(settings);
            mouseClick(findChild(settings, "saveButton"));
            compare(settingsVmStub.saveCount, 1);
        }
        function test_saveAdminInfoInvokesVm() {
            waitForRendering(settings);
            mouseClick(findChild(settings, "saveAdminInfoButton"));
            compare(settingsVmStub.saveAdminInfoCount, 1);
        }
        function test_changeKeyRequiresMatchingConfirmation() {
            var btn = findChild(settings, "changeKeyButton");
            findChild(settings, "newKeyField").text = "abc";
            findChild(settings, "confirmNewKeyField").text = "xyz";
            compare(btn.enabled, false);          // mismatch
            findChild(settings, "confirmNewKeyField").text = "abc";
            compare(btn.enabled, true);
        }
        function test_changeKeyInvokesVmWithBothKeys() {
            findChild(settings, "oldKeyField").text = "OLD";
            findChild(settings, "newKeyField").text = "NEW";
            findChild(settings, "confirmNewKeyField").text = "NEW";
            waitForRendering(settings);
            mouseClick(findChild(settings, "changeKeyButton"));
            compare(settingsVmStub.lastOldKey, "OLD");
            compare(settingsVmStub.lastNewKey, "NEW");
        }
        function test_resetButtonOpensTier2Dialog() {
            findChild(settings, "resetDeptPicker").selectValue("CE");
            waitForRendering(settings);
            mouseClick(findChild(settings, "resetVisitsButton"));
            var dlg = findChild(settings, "resetConfirmDialog");
            verify(dlg !== null);
            compare(dlg.visible, true);
            compare(dlg.tier, 2);
            // Confirm disabled until a fresh key is typed.
            var confirmBtn = findChild(dlg, "confirmButton");
            compare(confirmBtn.enabled, false);
        }
        function test_resetConfirmedSendsDeptAndFreshKey() {
            findChild(settings, "resetDeptPicker").selectValue("IT");
            waitForRendering(settings);
            mouseClick(findChild(settings, "resetVisitsButton"));
            var dlg = findChild(settings, "resetConfirmDialog");
            findChild(dlg, "confirmKeyField").text = "fresh-key";
            mouseClick(findChild(dlg, "confirmButton"));
            compare(settingsVmStub.lastResetDept, "IT");
            compare(settingsVmStub.lastResetKey, "fresh-key");
        }
        function test_undefinedVmRendersWithoutError() {
            compare(findChild(vmlessSettings, "saveButton").enabled, false);
        }
    }
```

- [ ] **Build + run, expect PASS** (SettingsScreen is module QML → rebuild first):
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure -R tst_qml_admin
```
Expected: `SettingsScreen` cases pass. If `FileDialog` construction fails offscreen, apply the T14 `active: screen.nativeDialogs` guard and set `nativeDialogs: false` on the test instances, then re-run.

- [ ] **Commit:**
```
git add qt-app/quick/tests/tst_qml_admin.qml
git commit -m "test(loams2): SettingsScreen stub-VM QuickTest flows (Phase 4c)"
```

---

### Task 16: Wire the real SettingsViewModel + AdminSession into the shell; full-suite green

**Files:**
- Modify: `qt-app/quick/qml/admin/AdminScreen.qml` (verify wiring), `qt-app/quick/qml/AppShell.qml` (only if it holds session-level state)

> T14 already added `SettingsViewModel { id: settingsVm }` and routed `settingsComponent`. This task verifies the real VM (not a stub) is what production uses, confirms `AdminSession` capture is live end-to-end, runs the full suite, and records the manual GUI smoke.

- [ ] **Verify the wiring** is real-VM (not stub): open `AdminScreen.qml` and confirm `settingsComponent` passes `vm: settingsVm` (the real `SettingsViewModel`), and that `AdminScreen.autoLoad` gates `loadDepartments` (from T14). No stub types appear in module QML.

- [ ] **Confirm `AppShell.qml` needs no change.** `AdminSession` is a process-wide singleton reached via `AdminSession::instance()` from C++ (`KioskViewModel` captures, `SettingsViewModel` consumes) — it needs no QML instantiation. Grep to confirm no QML file references a non-existent `AdminSession` QML type:
```
git grep -n "AdminSession" qt-app/quick/qml || echo "no QML references (correct)"
```
Expected: no QML references.

- [ ] **Full clean build + full suite, expect all green:**
```
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: at least 33 targets (32 floor + `tst_adminsession`; `tst_settingsviewmodel` is new too → 34), `100% tests passed`. Confirm the count never dropped below the Phase-3 floor.

- [ ] **Manual GUI smoke (record, don't automate).** This is a desktop GUI app — a clean build is necessary but not sufficient (project rule §4). Run the `WITS` (or `WITSQuick`) executable and, against a **SYNTHETIC** test database (no real student PII):
  - Enter the admin key at the kiosk → land on the admin surface; open Settings.
  - Change the school name, import a logo, Save; reopen and confirm persistence. **Confirm the live re-theme actually re-colors the UI** — the brand/maroon palette (sidebar, buttons) shifts to the logo's colors, not just the logo image swapping (this is the T9/T14 `Theme._vm` wiring; in Manual brand mode the palette stays and only the image changes, which is expected).
  - Toggle guest login off, Save; return to the kiosk and confirm the guest button is gone (proves the `kiosk/guestEnabled` mirror from T8).
  - Reset a fake department's visits via the tier-2 dialog (re-type the key) and confirm the history clears.
  Note the outcome in the PR description.

- [ ] **Commit any wiring fixes** (if the verify steps forced an edit):
```
git add qt-app/quick/qml/admin/AdminScreen.qml
git commit -m "chore(loams2): wire real SettingsViewModel into admin shell; full suite green (Phase 4c)"
```

---

## BACKEND (4c's own deploy — lockstep with the client)

### Task 17: Guard `reset_visits.php` + `update_admin_info.php` with `requireAdminAuth`

**Files:**
- Modify: `deliverables/loams_api/reset_visits.php`, `deliverables/loams_api/update_admin_info.php`
- Deploy: `C:/xampp/htdocs/loams_api/reset_visits.php`, `.../update_admin_info.php`

> Contract-tests-first (spec §5.3 / parent §8): capture the CURRENT request/response fixture BEFORE editing, add the guard, add a negative-auth (401) fixture, deploy, byte-verify. `reset_visits.php` currently includes only `db.php` — add the `auth_helper.php` include too. `requireAdminAuth` reads `$_POST['admin_key']`, bcrypt-verifies against `admin.admin_key_hash`, and 401s on missing/invalid (auth_helper.php:11).

- [ ] **Capture the current fixtures (before editing).** With XAMPP running and a synthetic DB, record the pre-change behavior into the plan's PR notes (no admin_key yet → still succeeds):
```
curl -s -X POST -d "department=TESTDEPT" http://localhost/loams_api/reset_visits.php
curl -s -X POST -d "admin_name=Test&admin_position=Tester" http://localhost/loams_api/update_admin_info.php
```
Expected (pre-guard): both return `{"status":"success",...}` with NO key. Save these as the "before" fixtures.

- [ ] **Add the guard to `reset_visits.php`.** After `include "db.php";` (line 6) add:
```php
include "auth_helper.php";
```
and immediately after the DB-connection guard / at the top of the `POST` block (right after `if ($_SERVER["REQUEST_METHOD"] === "POST") {`), add:
```php
    requireAdminAuth($conn);
```
Placing it inside the POST block (after the method check, before reading `$department`) ensures the 401 fires before any DB mutation.

- [ ] **Add the guard to `update_admin_info.php`.** After `include "db.php";` (line 6) add `include "auth_helper.php";`, and inside `if ($_SERVER["REQUEST_METHOD"] === "POST") {` as the first statement add `requireAdminAuth($conn);`.

- [ ] **Verify the negative-auth (401) contract.** After editing (still local file), the contract is:
  - Missing/invalid `admin_key` → HTTP 401 + `{"status":"error","message":"Admin authentication required"|"Invalid admin key"}`.
  - Valid `admin_key` + `department` → success as before.

- [ ] **Deploy + byte-verify** (spec §5.4 — Phase 3's bug was undeployed endpoints). Copy both files to xampp and byte-compare:
```
cp deliverables/loams_api/reset_visits.php "C:/xampp/htdocs/loams_api/reset_visits.php"
cp deliverables/loams_api/update_admin_info.php "C:/xampp/htdocs/loams_api/update_admin_info.php"
diff deliverables/loams_api/reset_visits.php "C:/xampp/htdocs/loams_api/reset_visits.php" && echo "reset_visits IDENTICAL"
diff deliverables/loams_api/update_admin_info.php "C:/xampp/htdocs/loams_api/update_admin_info.php" && echo "update_admin_info IDENTICAL"
```
Expected: both print `IDENTICAL`.

- [ ] **Capture the after fixtures** (with a valid synthetic admin key, and the 401 case):
```
curl -s -o /dev/null -w "%{http_code}\n" -X POST -d "department=TESTDEPT" http://localhost/loams_api/reset_visits.php
# expect 401 (no key)
curl -s -X POST -d "department=TESTDEPT&admin_key=<SYNTHETIC_TEST_KEY>" http://localhost/loams_api/reset_visits.php
# expect success
```
Record both in the PR notes. **Do not commit any real admin key** — use the synthetic test DB's key and write `<SYNTHETIC_TEST_KEY>` in notes.

- [ ] **Commit (source only; deployed copies are outside the repo):**
```
git add deliverables/loams_api/reset_visits.php deliverables/loams_api/update_admin_info.php
git commit -m "feat(loams_api): guard reset_visits + update_admin_info with requireAdminAuth (Phase 4c)"
```

---

### Task 18: Legacy rollback — thread the admin key into `adminwindow.cpp`'s reset_visits (lockstep with T17)

**Files:**
- Modify: `qt-app/adminwindow.h`, `qt-app/adminwindow.cpp`, `qt-app/mainwindow.cpp`

> The guard from T17 breaks the legacy Widgets rollback path: `adminwindow.cpp`'s reset_visits POST sends only `department` (adminwindow.cpp:599-602) and the window holds no key. Capture the entered admin key at the legacy login gate (`mainwindow.cpp` — where `admin_login.php` succeeds and `adminWin->show()` is called, mainwindow.cpp:226) and thread it into the reset_visits POST. **Deploy T17 lockstep with this + the client (T16)** or the moment the endpoints deploy, the legacy reset 401s.
>
> **Legacy-app caveat:** this is a Widgets app that `tst_responsive_ui`/`tst_theme` cover only at the UI-load level; there is no unit-test seam around this POST. The change is verified by the manual GUI smoke (T16) exercising the legacy reset against the synthetic DB. Keep the edit minimal and self-contained.

- [ ] **Add a key member + setter to `adminWindow`.** In `qt-app/adminwindow.h`, in the `private:` section (near the other `QString` members ~line 77) add:
```cpp
    QString m_adminKey;   // captured at the login gate; threaded into guarded POSTs (Phase 4c)
```
and in the `public:` section (after the constructor) add:
```cpp
    void setAdminKey(const QString &key) { m_adminKey = key; }
```

- [ ] **Capture the key at the login gate.** In `qt-app/mainwindow.cpp`, in the admin-success branch (the `else { adminWin->show(); }` around line 226), change it to hand the key over first. The admin key is the `input` captured by the reply lambda; extend the lambda capture to include it. Change the lambda header (line 206) from `[this, reply]()` to `[this, reply, input]()`, then in the admin branch:
```cpp
            } else {
                adminWin->setAdminKey(input);   // Phase 4c: retain for guarded POSTs
                adminWin->show();
            }
```

- [ ] **Thread the key into the reset_visits POST.** In `qt-app/adminwindow.cpp`, in the `resetCountBtn` clicked handler, add the `admin_key` field to `postData` (after line 600 `postData.addQueryItem("department", dept);`):
```cpp
        postData.addQueryItem("admin_key", m_adminKey);
```

- [ ] **Build the legacy app, expect clean compile:**
```
cmake --build qt-app/build
```
Expected: builds with no new warnings; existing Widgets tests still pass:
```
ctest --test-dir qt-app/build --output-on-failure -R "tst_responsive_ui|tst_theme"
```

- [ ] **Manual lockstep verification** (with T17 deployed): run the legacy admin window (via `WITS`), log in, and reset a synthetic department's visits — confirm it succeeds (no 401). Without the key it would 401; this proves the rollback stays whole (spec §5.5).

- [ ] **Commit:**
```
git add qt-app/adminwindow.h qt-app/adminwindow.cpp qt-app/mainwindow.cpp
git commit -m "fix(loams2): thread admin key into legacy reset_visits POST for guard lockstep (Phase 4c)"
```

---

## Done criteria (whole plan)

- All 18 tasks committed; `ctest --test-dir qt-app/build --output-on-failure` is fully green with the target count ≥ the Phase-3 floor (32) + the two new targets (`tst_adminsession`, `tst_settingsviewmodel`).
- Debug + Release build clean, no new warnings.
- `reset_visits.php` + `update_admin_info.php` guarded and byte-verified in `C:/xampp/htdocs/loams_api/`; the legacy reset path (T18) works against the guard.
- `/claude-review` APPROVE (the Codex CLI is not installed on this machine — use `/claude-review`, per spec §6.5).
- Manual GUI smoke recorded (T16 + T18) against a synthetic DB — no real student PII.
- Then open the PR with the project `create-pr` skill; merging is the user's call.
