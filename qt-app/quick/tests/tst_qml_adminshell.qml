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
        }
        function cleanup() {
            Navigator.showKiosk();
            Navigator.showAdminPage(Navigator.Dashboard);
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

        // LSideNav's disabled guard (Task 9) must still block activation of
        // the Phase-4 placeholder items reached through the shell's own item
        // list.
        function test_sidebarDisabledItemGuardBlocksActivation() {
            var nav = findChild(shell, "sideNav");
            nav.activate("database");
            compare(Navigator.adminPage, Navigator.Dashboard);
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
    }
}
