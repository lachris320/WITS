import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    // Tall enough to host DashboardScreen (0..760), SearchScreen (760..1460),
    // and VisitLogsScreen (1460..2160) side by side without overlap — QTest
    // mouse synthesis is window-local, and the window tracks this root
    // item's size.
    width: 1100; height: 2160

    // --- Dashboard stub VM ---
    // maxValue is declared on the stubs because the screen binds
    // vm.<model>.maxValue; a bare ListModel has no such property and the
    // binding would resolve to undefined (a QML warning + a silent fall back
    // to LBarChart's default). The real BarsModel exposes it as a NOTIFYing
    // Q_PROPERTY.
    ListModel { id: hourlyStub
        property real maxValue: 41
        ListElement { label: "8"; value: 12 }
        ListElement { label: "10"; value: 41 }
    }
    ListModel { id: deptStub
        property real maxValue: 210
        ListElement { label: "CE"; value: 210 }
        ListElement { label: "IT"; value: 180 }
    }
    QtObject {
        id: dashStub
        property int statToday: 128
        property int statWeek: 812
        property int statStudents: 3450
        property string peakHourLabel: "10 AM"
        property int peakHourIndex: 1
        property var hourlyModel: hourlyStub
        property var departmentModel: deptStub
        property bool loading: false
        property string errorText: ""
        property int refreshCount: 0
        function refresh() { refreshCount++ }
    }
    // Fixed geometry (not anchors.fill: parent) so dash occupies only the top
    // band of the now-taller host and does not extend under the search
    // instances below it — same 0,0 origin and size as before, observably
    // identical to Task 13's anchors.fill for every Dashboard assertion.
    DashboardScreen { id: dash; width: 1100; height: 760; vm: dashStub }

    // Second instance with no vm at all, covering the `vm ? ... : ...` fallback
    // path every binding in the screen is written to defend.
    DashboardScreen { id: vmlessDash; width: 1100; height: 760 }

    // --- Search stub VM ---
    ListModel { id: searchStub
        ListElement { name: "Maria Santos"; schoolId: "2023-0001"; course: "BSCE"; department: "CE"; visits: 42; initials: "MS" }
    }
    QtObject {
        id: searchVmStub
        property var results: searchStub
        property var courses: ["BSCE", "BSEE"]
        property bool loading: false
        property string errorText: ""
        property string lastSearch: ""
        property string lastCourse: ""
        property int searchCount: 0
        function search(s, c) { lastSearch = s; lastCourse = c; searchCount++; }
    }
    // Positioned below dash (y: 760) so the taller host gives it its own band
    // instead of stacking on top of dash's errorBlock/retryButton — see the
    // host comment above.
    SearchScreen { id: search; y: 760; width: 1100; height: 700; vm: searchVmStub }

    // No-vm fallback instance, offset off to the side (never clicked) so it
    // cannot intercept clicks meant for `search` or `dash`.
    SearchScreen { id: vmlessSearch; x: 2000; width: 900; height: 700 }

    // --- Visit Logs stub VM ---
    // A single row carries every union role (student + guest fields) since
    // the real VisitLogRowsModel is one CONSTANT model reused across modes —
    // the column set that reads it switches, not the row data shape.
    ListModel { id: visitRowsStub
        ListElement { date: "2026-07-13"; name: "Maria Santos"; course: "BSCE"; department: "CE"; timeIn: "08:14"; timeOut: "—"; company: ""; purpose: "" }
    }
    QtObject {
        id: visitVmStub
        property int mode: 0        // 0 == VisitLogsViewModel.Student
        property int range: 0
        property int count: 1
        property string rangeLabel: "Today, Jul 13, 2026"
        property var rows: visitRowsStub
        property bool loading: false
        property string errorText: ""
        property int refreshCount: 0
        function refresh() { refreshCount++ }
    }
    // Own band below search (search ends at y 1460) — same fixed-geometry
    // rationale as dash/search above (never anchors.fill, so growing the
    // host cannot silently expand an instance under its neighbors).
    VisitLogsScreen { id: logs; y: 1460; width: 1100; height: 700; vm: visitVmStub }

    // No-vm fallback instance, parked right and below vmlessSearch (which
    // owns x 2000/y 0-700) so it can never intercept a click meant for
    // `logs` or `vmlessSearch`. `visible: false` is NOT used here — Item's
    // visible is *effective* visibility, so a hidden parent would force this
    // instance's children to read visible === false too, turning the
    // no-vm fallback assertions into a false pass.
    VisitLogsScreen { id: vmlessLogs; x: 2000; y: 760; width: 1000; height: 700 }

    TestCase {
        name: "DashboardScreen"
        when: windowShown

        // Test functions run in alphabetical order, and these tests mutate the
        // shared stub, so reset it to a known baseline before each one rather
        // than relying on declaration order.
        function init() {
            dashStub.peakHourLabel = "10 AM";
            dashStub.errorText = "";
            dashStub.loading = false;
            dashStub.refreshCount = 0;
        }

        function test_showsPeakHourLabel() {
            compare(dash.peakShown, "10 AM");
            // peakShown reflects the tile, so it must track the VM live.
            dashStub.peakHourLabel = "2 PM";
            compare(dash.peakShown, "2 PM");
        }

        function test_peakTileRendersVmLabel() {
            compare(findChild(dash, "peakTile").value, "10 AM");
        }

        // --- Error + retry (spec §7.3) ---

        function test_errorBlockHiddenWhenNoError() {
            var block = findChild(dash, "errorBlock");
            verify(block !== null);
            compare(block.visible, false);
        }

        function test_errorBlockVisibleWhenErrorText() {
            var block = findChild(dash, "errorBlock");
            dashStub.errorText = "Network unreachable";
            compare(block.visible, true);
            dashStub.errorText = "";
            compare(block.visible, false);
        }

        function test_retryInvokesRefresh() {
            dashStub.errorText = "Network unreachable";
            var btn = findChild(dash, "retryButton");
            verify(btn !== null);
            waitForRendering(dash);
            compare(dashStub.refreshCount, 0);
            mouseClick(btn);
            compare(dashStub.refreshCount, 1);
        }

        // --- Loading state ---

        function test_loadingDimsContentAndRunsBusyIndicator() {
            var contentCol = findChild(dash, "dashContent");
            var busyInd = findChild(dash, "busyIndicator");
            compare(contentCol.opacity, 1.0);
            compare(busyInd.running, false);
            compare(busyInd.visible, false);

            dashStub.loading = true;
            compare(contentCol.opacity, 0.5);
            compare(busyInd.running, true);
            compare(busyInd.visible, true);

            dashStub.loading = false;
            compare(contentCol.opacity, 1.0);
            compare(busyInd.running, false);
        }

        // --- No-vm fallback path ---

        function test_undefinedVmRendersFallbacksWithoutError() {
            // peakShown reflects the tile, whose no-vm fallback is the em dash
            // (not the empty string) — asserting the real rendered value.
            compare(vmlessDash.peakShown, "—");
            compare(findChild(vmlessDash, "peakTile").value, "—");
            // Neither the error block nor the busy indicator may latch on with
            // no vm to report an error or a load.
            compare(findChild(vmlessDash, "errorBlock").visible, false);
            compare(findChild(vmlessDash, "busyIndicator").running, false);
            compare(findChild(vmlessDash, "dashContent").opacity, 1.0);
        }
    }

    TestCase {
        name: "SearchScreen"
        when: windowShown

        // Test functions run in alphabetical order, and several mutate the
        // shared stub or screen state, so reset everything to a known
        // baseline before each one rather than relying on declaration order.
        function init() {
            searchVmStub.errorText = "";
            searchVmStub.loading = false;
            searchVmStub.lastSearch = "";
            searchVmStub.lastCourse = "";
            searchVmStub.searchCount = 0;
            search.selectedCourse = "";
            findChild(search, "queryField").text = "";
        }

        function findAny(root, s) {
            if (root.text !== undefined && root.text !== null && root.text.indexOf(s) !== -1)
                return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findAny(root.children[i], s);
                if (f !== null) return f;
            }
            return null;
        }

        function test_showsResultCount() {
            compare(search.resultCount, 1);
        }

        function test_rendersTotalVisitsLabel() {
            waitForRendering(search);
            verify(findAny(search, "Total Visits: 42") !== null);
        }

        function test_rendersSchoolId() {
            waitForRendering(search);
            verify(findAny(search, "2023-0001") !== null);
        }

        function test_avatarRendersInitials() {
            waitForRendering(search);
            var avatar = findChild(search, "avatarInitials");
            verify(avatar !== null);
            compare(avatar.text, "MS");
        }

        // --- Search invocation branches ---

        function test_searchButtonInvokesSearch() {
            var field = findChild(search, "queryField");
            field.text = "Santos";
            var btn = findChild(search, "searchButton");
            verify(btn !== null);
            waitForRendering(search);
            mouseClick(btn);
            compare(searchVmStub.lastSearch, "Santos");
            compare(searchVmStub.lastCourse, "");
            compare(searchVmStub.searchCount, 1);
        }

        function test_queryFieldAcceptedInvokesSearch() {
            var field = findChild(search, "queryField");
            field.text = "2023-0001";
            field.forceActiveFocus();
            keyClick(Qt.Key_Return);
            compare(searchVmStub.lastSearch, "2023-0001");
            compare(searchVmStub.searchCount, 1);
        }

        function test_retryButtonInvokesSearch() {
            searchVmStub.errorText = "Network unreachable";
            var btn = findChild(search, "retryButton");
            verify(btn !== null);
            waitForRendering(search);
            compare(searchVmStub.searchCount, 0);
            mouseClick(btn);
            compare(searchVmStub.searchCount, 1);
        }

        // --- Course filter chips (the only non-trivial logic on this screen) ---

        function test_chipClickSetsSelectedCourseAndSearches() {
            var chip = findChild(search, "chip_BSCE");
            verify(chip !== null);
            waitForRendering(search);
            mouseClick(chip);
            compare(search.selectedCourse, "BSCE");
            compare(searchVmStub.lastCourse, "BSCE");
            compare(searchVmStub.searchCount, 1);
        }

        function test_chipToggleOffClearsSelectedCourseAndSearches() {
            var chip = findChild(search, "chip_BSCE");
            waitForRendering(search);
            mouseClick(chip);
            compare(search.selectedCourse, "BSCE");
            mouseClick(chip);
            compare(search.selectedCourse, "");
            compare(searchVmStub.lastCourse, "");
            compare(searchVmStub.searchCount, 2);
        }

        // --- Error + retry ---

        function test_errorBlockHiddenWhenNoError() {
            var block = findChild(search, "errorBlock");
            verify(block !== null);
            compare(block.visible, false);
        }

        function test_errorBlockVisibleWhenErrorText() {
            var block = findChild(search, "errorBlock");
            searchVmStub.errorText = "Network unreachable";
            compare(block.visible, true);
            searchVmStub.errorText = "";
            compare(block.visible, false);
        }

        // --- Loading state ---

        function test_loadingTogglesBusyIndicator() {
            var busy = findChild(search, "busyIndicator");
            compare(busy.running, false);
            compare(busy.visible, false);

            searchVmStub.loading = true;
            compare(busy.running, true);
            compare(busy.visible, true);

            searchVmStub.loading = false;
            compare(busy.running, false);
            compare(busy.visible, false);
        }

        // --- No-vm fallback path ---

        function test_undefinedVmRendersFallbacksWithoutError() {
            compare(vmlessSearch.resultCount, 0);
            compare(findChild(vmlessSearch, "errorBlock").visible, false);
            compare(findChild(vmlessSearch, "busyIndicator").running, false);
        }
    }

    TestCase {
        name: "VisitLogsScreen"
        when: windowShown

        // Test functions run in alphabetical order, and several of them
        // mutate the shared stub (some via a segmented click, which — per
        // LSegmented.qml:42 — permanently breaks that instance's
        // `currentValue` binding, so it is never reset here; only the stub's
        // own fields are), so reset the stub to a known baseline before
        // each one rather than relying on declaration order.
        function init() {
            visitVmStub.mode = 0;
            visitVmStub.range = 0;
            visitVmStub.rangeLabel = "Today, Jul 13, 2026";
            visitVmStub.errorText = "";
            visitVmStub.loading = false;
            visitVmStub.refreshCount = 0;
        }

        function findAny(root, s) {
            if (root.text !== undefined && root.text !== null && root.text.indexOf(s) !== -1)
                return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findAny(root.children[i], s);
                if (f !== null) return f;
            }
            return null;
        }

        // --- Mode switch: real rendered column evidence, not just a count ---

        function test_studentModeHasSixColumns() {
            compare(logs.vm.mode, VisitLogsViewModel.Student);
            compare(logs.activeColumnCount, 6);
            waitForRendering(logs);
            verify(findAny(logs, "Time Out") !== null);
        }

        function test_guestModeHasFourColumns() {
            visitVmStub.mode = VisitLogsViewModel.Guest;
            compare(logs.activeColumnCount, 4);
            waitForRendering(logs);
            verify(findAny(logs, "Time Out") === null);
            verify(findAny(logs, "Company") !== null);
        }

        function test_tableRendersStubRow() {
            var table = findChild(logs, "visitsTable");
            verify(table !== null);
            compare(table.rowCount, 1);
            compare(table.emptyVisible, false);
        }

        // --- rangeLabel ---

        function test_rangeLabelRenders() {
            compare(findChild(logs, "rangeLabel").text, "Today, Jul 13, 2026");
            visitVmStub.rangeLabel = "Jul 13 – Jul 19, 2026";
            compare(findChild(logs, "rangeLabel").text, "Jul 13 – Jul 19, 2026");
        }

        // --- Both LSegmented controls write back to the VM. Asserted on the
        // stub, never on `currentValue` post-click — see LSegmented.qml:42
        // and the init() comment above. ---

        function test_modeSegmentedWritesBackToVm() {
            var seg = findChild(logs, "modeSegmented");
            verify(seg !== null);
            waitForRendering(logs);
            compare(visitVmStub.mode, 0);
            mouseClick(seg, seg.width * 0.75, seg.height / 2);
            compare(visitVmStub.mode, VisitLogsViewModel.Guest);
        }

        function test_rangeSegmentedWritesBackToVm() {
            var seg = findChild(logs, "rangeSegmented");
            verify(seg !== null);
            waitForRendering(logs);
            compare(visitVmStub.range, 0);
            mouseClick(seg, seg.width * 0.75, seg.height / 2);
            compare(visitVmStub.range, VisitLogsViewModel.Week);
        }

        // --- Error + retry ---

        function test_errorBlockHiddenWhenNoError() {
            var block = findChild(logs, "errorBlock");
            verify(block !== null);
            compare(block.visible, false);
        }

        function test_errorBlockVisibleWhenErrorText() {
            var block = findChild(logs, "errorBlock");
            visitVmStub.errorText = "Network unreachable";
            compare(block.visible, true);
            visitVmStub.errorText = "";
            compare(block.visible, false);
        }

        function test_retryInvokesRefresh() {
            visitVmStub.errorText = "Network unreachable";
            var btn = findChild(logs, "retryButton");
            verify(btn !== null);
            waitForRendering(logs);
            compare(visitVmStub.refreshCount, 0);
            mouseClick(btn);
            compare(visitVmStub.refreshCount, 1);
        }

        // --- Loading state ---

        function test_loadingTogglesBusyIndicator() {
            var busy = findChild(logs, "busyIndicator");
            compare(busy.running, false);
            compare(busy.visible, false);

            visitVmStub.loading = true;
            compare(busy.running, true);
            compare(busy.visible, true);

            visitVmStub.loading = false;
            compare(busy.running, false);
            compare(busy.visible, false);
        }

        // --- No-vm fallback path ---

        function test_undefinedVmRendersFallbacksWithoutError() {
            // Student-primary is a frozen owner decision (spec §6.3): with no
            // vm to report a mode, the screen must still default to student.
            compare(vmlessLogs.isStudent, true);
            compare(vmlessLogs.activeColumnCount, 6);
            compare(findChild(vmlessLogs, "errorBlock").visible, false);
            compare(findChild(vmlessLogs, "busyIndicator").running, false);
        }
    }
}
