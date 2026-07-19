import QtQuick
import QtTest
import LOAMS

// AdminScreen shell test (Task 16). Deliberately split out of
// tst_qml_admin.qml (see Task 16 brief DEVIATION 1): that file already
// carries a hand-computed y-band geometry ledger for its stub-VM screen
// instances, and AdminScreen is the largest surface yet (sidebar + header +
// Loader). A dedicated tst_*.qml is discovered at runtime via
// QUICK_TEST_SOURCE_DIR, needs no new CMake target, and gets its own window
// with no geometry bookkeeping to maintain.
//
// This is the first QuickTest to instantiate AdminScreen, which in turn
// instantiates the three REAL ViewModels (not QML stubs). autoLoad: false
// suppresses the Loader.onLoaded -> vm.refresh() call so construction is the
// only thing exercised — and construction alone issues no network for any of
// the three real VMs (verified against DashboardViewModel.cpp:9-13,
// SearchViewModel.cpp:6-19, VisitLogsViewModel.cpp:15-20 — see the Task 16
// advisor note). Never write pageLoader.item.vm.mode/.range here: the real
// VisitLogsViewModel::setMode/setRange fire refresh() unconditionally
// (VisitLogsViewModel.cpp:41-57), bypassing autoLoad entirely.
Item {
    id: host
    width: 1100; height: 720

    AdminScreen { id: shell; width: 1100; height: 720; autoLoad: false }

    // Watches the REAL sidebar's pageActivated signal so the disabled-item
    // guard can be asserted on the signal itself rather than on a downstream
    // side effect a broken guard would not disturb. `target` is bound in
    // init() via the existing objectName seam — no test-only property is
    // added to AdminScreen just to reach it.
    SignalSpy {
        id: activationSpy
        signalName: "pageActivated"
    }

    TestCase {
        name: "AdminScreenShell"
        when: windowShown

        // Navigator is a singleton whose state leaks across every tst_*.qml
        // file sharing this QuickTest target's process (and, since all four
        // QuickTest targets share QUICK_TEST_SOURCE_DIR, this file itself
        // runs under all four). Reset both surface and page before AND after
        // every test — TestCase functions run alphabetically, not in
        // declaration order, so init() cannot rely on a "previous" test
        // having left things clean.
        function init() {
            Navigator.showAdminPage(Navigator.Dashboard);
            Navigator.showKiosk();
            activationSpy.target = findChild(shell, "sideNav");
            activationSpy.clear();
        }
        function cleanup() {
            Navigator.showKiosk();
            Navigator.showAdminPage(Navigator.Dashboard);
            activationSpy.clear();
        }

        // Depth-first search for a sidebar delegate row whose modelData.page
        // matches pageKey. Used to inspect the real isActive state a click
        // (or a test call to LSideNav.activate()) would produce — not a
        // test-only mirror property.
        function findSidebarRow(root, pageKey) {
            if (root.modelData !== undefined && root.modelData !== null
                    && root.modelData.page === pageKey)
                return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findSidebarRow(root.children[i], pageKey);
                if (f !== null) return f;
            }
            return null;
        }

        // --- Floor tests from the plan (thin wrappers over Navigator;
        // Task 1's showAdminPage/showKiosk behavior is already covered by
        // tst_navigator — these just pin that the shell's hooks exist and
        // reach it). ---

        function test_sidebarSwitchesActivePage() {
            shell.activatePage(Navigator.VisitLogs);
            compare(Navigator.adminPage, Navigator.VisitLogs);
        }

        function test_backToKioskReturnsToKiosk() {
            Navigator.showAdmin();
            shell.backToKiosk();
            compare(Navigator.currentSurface, Navigator.Kiosk);
        }

        // --- The shell's own behavior ---

        // The Loader is the shell's core job: it must actually swap to the
        // right screen component, not just track Navigator.adminPage as a
        // number. Deleting/mis-wiring a switch branch turns this red.
        function test_loaderSwapsToVisitLogsScreen() {
            var loader = findChild(shell, "pageLoader");
            verify(loader !== null);
            compare(loader.item.objectName, "dashboardPage");

            Navigator.showAdminPage(Navigator.VisitLogs);
            compare(loader.item.objectName, "visitLogsPage");
            // Duck-typed belt-and-braces: VisitLogsScreen (student mode, the
            // default) exposes exactly 6 columns; DashboardScreen/SearchScreen
            // have no such property, so this could not pass against the wrong
            // screen type by accident.
            compare(loader.item.activeColumnCount, 6);
        }

        function test_loaderSwapsToSearchScreen() {
            var loader = findChild(shell, "pageLoader");
            Navigator.showAdminPage(Navigator.Search);
            compare(loader.item.objectName, "searchPage");
            compare(loader.item.resultCount, 0);
        }

        // pageTitle must track Navigator.adminPage across all three pages
        // plus the default branch, asserted through the real rendered
        // LPageHeader, not shell.pageTitle alone.
        function test_pageTitleTracksNavigation() {
            var header = findChild(shell, "pageHeader");
            verify(header !== null);
            compare(header.title, "Dashboard");

            Navigator.showAdminPage(Navigator.Search);
            compare(header.title, "Search");

            Navigator.showAdminPage(Navigator.VisitLogs);
            compare(header.title, "Visit Logs");

            Navigator.showAdminPage(Navigator.Dashboard);
            compare(header.title, "Dashboard");
        }

        // The header's live clock (Component.onCompleted: tickClock()) must
        // populate BOTH dateText and clockText on the real LPageHeader, not
        // just clockText — exact wall-clock strings aren't asserted (real
        // Date(), not injectable here) but non-emptiness is exactly what a
        // missing `dateText: admin.dateText` binding (AdminScreen.qml) would
        // break: dateText would stay "" forever.
        function test_pageHeaderShowsDateAlongsideClock() {
            var header = findChild(shell, "pageHeader");
            verify(header !== null);
            verify(header.dateText.length > 0);
            verify(header.clockText.length > 0);
        }

        // Drives LSideNav.activate() (Task 9's sanctioned click-equivalent
        // hook — "the single path both a click and a test call take") rather
        // than only the shell's own activatePage() hook, so this proves the
        // shell's onPageActivated handler actually reaches Navigator.
        function test_sidebarActivateReachesNavigator() {
            var nav = findChild(shell, "sideNav");
            var loader = findChild(shell, "pageLoader");
            verify(nav !== null);

            nav.activate("visitlogs");
            compare(Navigator.adminPage, Navigator.VisitLogs);
            compare(loader.item.objectName, "visitLogsPage");
        }

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

        // CRITICAL 1 regression test: LSideNav.currentPage is a
        // `property string`. Binding it directly to the int
        // Navigator.adminPage enum coerces to "0"/"1"/"2" and the delegate's
        // strict === match (LSideNav.qml:44) never matches any item's
        // string `page` key, so no row highlights. This asserts BOTH that
        // the shell publishes the right string key AND that the resulting
        // delegate row actually reports isActive — real component state, not
        // a mirror property.
        function test_sidebarHighlightsActivePage() {
            var nav = findChild(shell, "sideNav");
            verify(nav !== null);

            Navigator.showAdminPage(Navigator.VisitLogs);
            compare(nav.currentPage, "visitlogs");

            var activeRow = findSidebarRow(nav, "visitlogs");
            var dashboardRow = findSidebarRow(nav, "dashboard");
            verify(activeRow !== null);
            verify(dashboardRow !== null);
            compare(activeRow.isActive, true);
            compare(dashboardRow.isActive, false);
        }

        // The autoLoad gate is the network-safety mechanism for this whole
        // file: if it silently broke, every run of this test (and the ones
        // above that navigate) would fire a real GET against
        // http://localhost/loams_api/. Both DashboardViewModel::refresh()
        // and VisitLogsViewModel::refresh() set loading=true synchronously
        // before touching the network, so vm.loading staying false after
        // navigation is proof no refresh() ran.
        function test_autoLoadGateSuppressesRefresh() {
            var loader = findChild(shell, "pageLoader");
            // Initial load (Dashboard, from shell construction) must not
            // have fired a refresh either.
            compare(loader.item.vm.loading, false);
            compare(loader.item.vm.errorText, "");

            Navigator.showAdminPage(Navigator.VisitLogs);
            compare(loader.item.vm.loading, false);
            compare(loader.item.vm.errorText, "");
        }

        // Back-to-Kiosk via the actual footer button path (the reparented
        // LSideNav.footer slot), not only the backToKiosk() hook.
        function test_backToKioskFooterButtonReturnsToKiosk() {
            Navigator.showAdmin();
            waitForRendering(shell);
            var btn = findChild(shell, "backToKioskButton");
            verify(btn !== null);
            mouseClick(btn);
            compare(Navigator.currentSurface, Navigator.Kiosk);
        }

        // Sidebar brand block (school logo + details): proves LSidebarBrand
        // is actually wired into the real sideNav.header slot, not merely
        // that the component works in isolation (tst_qml_components.qml
        // covers that with literal fixtures). "Library Admin" is a static
        // qsTr() literal in LSidebarBrand.qml, not machine-configured data,
        // so this assertion holds on every dev machine regardless of what
        // that machine's real QSettings school/name happens to be — the
        // real (this-machine, non-empty) schoolName is deliberately NOT
        // asserted here per the security-hygiene rule against encoding real
        // configured data into a committed test. Deleting the `header:
        // LSidebarBrand { ... }` assignment in AdminScreen.qml (or the
        // SchoolInfoViewModel instantiation it depends on) makes this fail:
        // findChild would come back null.
        function test_sidebarShowsBrandBlock() {
            var titleNode = findChild(shell, "brandTitleText");
            verify(titleNode !== null);
            compare(titleNode.text, "Library Admin");
        }

        // Phase 4c writes school/name + school/logoPath for the first time, so
        // the sidebar brand has to be re-readable mid-session (SettingsScreen
        // saved -> AdminScreen.reloadSchoolInfo -> SchoolInfoViewModel.reload).
        // The reload's own re-read/NOTIFY semantics are pinned in C++ by
        // tst_schoolinfoviewmodel; this asserts the SHELL end of the wire
        // exists and is callable, without writing to this machine's real
        // QSettings (security-hygiene: no real configured data in tests).
        // Deleting the hook, the Connections block, or reload() fails here.
        function test_shellExposesSchoolInfoReloadHook() {
            verify(typeof shell.reloadSchoolInfo === "function");
            shell.reloadSchoolInfo();               // re-read must not throw
            var titleNode = findChild(shell, "brandTitleText");
            compare(titleNode.text, "Library Admin");
        }

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
    }
}
