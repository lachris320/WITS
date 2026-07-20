import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    // Tall enough to host DashboardScreen (0..760), SearchScreen (760..1460),
    // VisitLogsScreen (1460..2160) and SettingsScreen (2160..3760) side by
    // side without overlap — QTest mouse synthesis is window-local, and the
    // window tracks this root item's size.
    //
    // Geometry ledger (x 0 column):   dash 0..760 | search 760..1460 |
    //   logs 1460..2160 | settings 2160..3760.
    // Parked no-vm column (x 2000): vmlessSearch 0..700 |
    //   vmlessLogs 760..1460 | vmlessSettings 1560..2260.
    width: 1100; height: 3760

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
        property var departments: ["CE", "IT"]
        property string department: ""
        property bool loading: false
        property string errorText: ""
        property string lastSearch: ""
        property string lastCourse: ""
        property string lastDepartment: ""
        property int searchCount: 0
        property int setDepartmentCount: 0
        function search(s, c) { lastSearch = s; lastCourse = c; searchCount++; }
        // Mirrors the real SearchViewModel::setDepartment() contract: no-op on
        // an unchanged value, otherwise re-scopes `courses` (fires
        // coursesChanged) — the trigger the screen must reconcile a
        // now-invalid course selection against.
        function setDepartment(d) {
            if (d === department) return;
            department = d;
            lastDepartment = d;
            setDepartmentCount++;
            courses = d === "CE" ? ["BSCE"] : d === "IT" ? ["BSIT"] : ["BSCE", "BSEE"];
        }
    }
    // --- Search row-motion fixtures (Phase 3 gatefix Part 1): populate/add ---
    // A DISTINCT model object (not the same instance re-cleared) so
    // reassigning searchVmStub.results is a genuine "model changed" event for
    // the ListView — the same distinction LTable's visitRowsAnimatedFixtureA/B
    // pair relies on (tst_qml_components.qml): clear()+append() on the SAME
    // ListModel only fires add/remove, never populate. In production,
    // SearchResultsModel::setRecords() always does a genuine
    // beginResetModel/endResetModel (a fresh search replaces the whole result
    // set), so populate — not add — is the transition that actually matters
    // for a real search; this fixture proves it genuinely re-plays.
    ListModel { id: searchMotionFixtureB
        ListElement { name: "Fresh Result"; schoolId: "2023-9999"; course: "BSIT"; department: "IT"; visits: 7; initials: "FR" }
    }
    // 9 rows (indices 0-8) so the stagger-differential test has range below
    // Theme.motion.staggerCap (10) to compare a zero-stagger row against a
    // clearly mid-flight one — same rationale as LTable's
    // visitRowsAnimatedFixtureA (tst_qml_components.qml).
    ListModel { id: searchStaggerFixture
        ListElement { name: "Row 0"; schoolId: "S-0"; course: "BSCE"; department: "CE"; visits: 1; initials: "R0" }
        ListElement { name: "Row 1"; schoolId: "S-1"; course: "BSCE"; department: "CE"; visits: 1; initials: "R1" }
        ListElement { name: "Row 2"; schoolId: "S-2"; course: "BSCE"; department: "CE"; visits: 1; initials: "R2" }
        ListElement { name: "Row 3"; schoolId: "S-3"; course: "BSCE"; department: "CE"; visits: 1; initials: "R3" }
        ListElement { name: "Row 4"; schoolId: "S-4"; course: "BSCE"; department: "CE"; visits: 1; initials: "R4" }
        ListElement { name: "Row 5"; schoolId: "S-5"; course: "BSCE"; department: "CE"; visits: 1; initials: "R5" }
        ListElement { name: "Row 6"; schoolId: "S-6"; course: "BSCE"; department: "CE"; visits: 1; initials: "R6" }
        ListElement { name: "Row 7"; schoolId: "S-7"; course: "BSCE"; department: "CE"; visits: 1; initials: "R7" }
        ListElement { name: "Row 8"; schoolId: "S-8"; course: "BSCE"; department: "CE"; visits: 1; initials: "R8" }
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

    // --- Settings stub VM ---
    // Mirrors SettingsViewModel's QML-facing surface as it stands AFTER the
    // T14 review fixes (commit 231dd3f), not as the plan sketched it:
    //   * importLogo takes the QUrl FileDialog hands back (not a path string),
    //   * defaultManifestUrl(dept) exists and openManifestDialog() calls it,
    //   * writeResetManifest returns bool and the screen consumes it,
    //   * every outcome signal the screen's Connections block listens on is
    //     declared here, because a signal that is not declared on the stub
    //     cannot be emitted from a test — and `ignoreUnknownSignals: true`
    //     means a missing one fails silently instead of warning.
    // Deliberately a plain QtObject: instantiating the real SettingsViewModel
    // here would drag QNetworkAccessManager and QSettings into a QuickTest.
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

        // Call counters / captured arguments.
        property int loadCount: 0
        property int saveCount: 0
        property int saveAdminInfoCount: 0
        property int loadDepartmentsCount: 0
        property string lastOldKey: ""
        property string lastNewKey: ""
        property string lastResetDept: ""
        property string lastResetKey: ""
        property string lastManifestDept: ""
        property url lastImportedLogo: ""
        property bool manifestWriteResult: true

        signal saved()
        signal saveFailed(string message)
        signal adminInfoSaved()
        signal adminInfoFailed(string message)
        signal keyChanged()
        signal keyChangeFailed(string message)
        signal visitsReset()
        signal resetFailed(string message)
        signal authFailed()
        signal networkError()
        // The real VM's statusMessage NOTIFY is statusChanged (not the
        // auto-generated statusMessageChanged), and the screen's Connections
        // block handles onStatusChanged — so it must be declared explicitly.
        signal statusChanged()

        function load() { loadCount++ }
        function save() { saveCount++ }
        function saveAdminInfo() { saveAdminInfoCount++ }
        function changeAdminKey(o, n) { lastOldKey = o; lastNewKey = n }
        function resetVisits(d, k) { lastResetDept = d; lastResetKey = k }
        function importLogo(u) { lastImportedLogo = u }
        function loadDepartments() { loadDepartmentsCount++ }
        function defaultManifestUrl(d) {
            lastManifestDept = d;
            return "file:///manifests/Reset_Manifest_" + d + ".csv";
        }
        function writeResetManifest(d, u) {
            lastManifestDept = d;
            return manifestWriteResult;
        }
    }
    // Own band below logs. Height 1600 (not the plan's 900) because the whole
    // settings column has to fit INSIDE the screen's clipping Flickable: the
    // footer Save button is the last item in a ~1340px content column, and a
    // shorter fixture would scroll it out of the viewport, where mouseClick()
    // maps to a point no longer over the button. test_fixtureFitsWholeContent
    // pins that invariant so a future card cannot silently break the clicks.
    SettingsScreen { id: settings; y: 2160; width: 1100; height: 1600; vm: settingsVmStub }

    // No-vm fallback instance, parked in the right-hand column below
    // vmlessLogs (which owns y 760..1460) so it can never intercept a click.
    SettingsScreen { id: vmlessSettings; x: 2000; y: 1560; width: 1000; height: 700 }

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
            // test_pageInAnimatesContentAndPreservesLoadingDim drives this
            // down to 0 and back up; restore the resting state so every
            // other test sees a fully-opaque, unmoved content column
            // regardless of test order (alphabetical, not declaration).
            dash.pageInT = 1;
            vmlessDash.pageInT = 1;
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

        // --- Motion (Phase 3 Task B): page entrance + the opacity-ownership
        // collision with the loading dim ---

        function test_pageInAnimatesContentAndPreservesLoadingDim() {
            var contentCol = findChild(dash, "dashContent");
            var anim = findChild(dash, "pageInAnimation");
            verify(anim !== null);

            dash.pageInT = 0;
            anim.restart();
            // Immediately after reset: content is invisible and raised 16px;
            // the root Rectangle (background) must never move or fade.
            compare(contentCol.opacity, 0);
            compare(contentCol.transform[0].y, 16);
            compare(dash.opacity, 1);

            tryCompare(dash, "pageInT", 1);
            compare(contentCol.opacity, 1);
            compare(contentCol.transform[0].y, 0);

            // The subtlest line in the task: pageInT multiplies into the
            // EXISTING loading-dim binding rather than replacing it, so the
            // dim still works once the entrance has settled at pageInT===1.
            dashStub.loading = true;
            compare(contentCol.opacity, 0.5);
            dashStub.loading = false;
            compare(contentCol.opacity, 1);
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
            searchVmStub.lastDepartment = "";
            searchVmStub.searchCount = 0;
            searchVmStub.setDepartmentCount = 0;
            searchVmStub.department = "";
            searchVmStub.courses = ["BSCE", "BSEE"];
            searchVmStub.departments = ["CE", "IT"];
            // test_emptyStateShowsWhenSearchReturnsNothing empties this model;
            // restore it so every other test sees the usual single row.
            if (searchStub.count !== 1) {
                searchStub.clear();
                searchStub.append({ name: "Maria Santos", schoolId: "2023-0001", course: "BSCE", department: "CE", visits: 42, initials: "MS" });
            }
            // The row-motion tests swap vm.results to a distinct model object
            // to force a genuine populate re-fire; restore the usual stub so
            // every other test (and re-runs of the motion tests themselves)
            // sees the standard single-row fixture regardless of test order.
            searchVmStub.results = searchStub;
            search.selectedCourse = "";
            search.selectedDepartment = "";
            search.hasSearched = false;
            search.debounceMs = 300;
            // The two page-entrance tests drive this down to 0; stop the
            // animation and restore the resting state so every other test
            // sees a fully-opaque, unmoved content column regardless of test
            // order (test functions run alphabetically, not declaration
            // order) and with nothing left animating over it.
            findChild(search, "pageInAnimation").stop();
            search.pageInT = 1;
            findChild(search, "queryField").text = "";
            // Setting queryField.text above restarts the debounce Timer; stop
            // it so a leftover pending fire cannot land mid-way through a
            // LATER test's own wait()/tryCompare() and skew its search count.
            findChild(search, "debounceTimer").stop();
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

        function test_avatarRendersInitials() {
            waitForRendering(search);
            var avatar = findChild(search, "avatarInitials");
            verify(avatar !== null);
            compare(avatar.text, "MS");
        }

        // --- Course filter chips (department-scoped) ---

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

        // --- Active filter pills ---

        function test_clearAllButtonClearsBothFiltersAndSearches() {
            waitForRendering(search);
            mouseClick(findChild(search, "deptChip_CE"));
            mouseClick(findChild(search, "chip_BSCE"));
            compare(search.selectedDepartment, "CE");
            compare(search.selectedCourse, "BSCE");

            // The pill Repeater's model is a freshly-computed JS array on
            // every selection change (activePills), so adding the second
            // (course) pill above fully recreates the Repeater's delegates —
            // give Flow a render tick to position them before clicking one,
            // or the click can land on whichever pill hasn't moved off (0,0)
            // yet instead of the one findChild() actually located.
            waitForRendering(search);
            var clearBtn = findChild(search, "clearAllButton");
            verify(clearBtn !== null);
            mouseClick(clearBtn);

            compare(search.selectedDepartment, "");
            compare(search.selectedCourse, "");
            compare(searchVmStub.lastDepartment, "");
            compare(searchVmStub.lastCourse, "");
        }

        // --- Department -> course reconciliation (the non-trivial logic here) ---

        function test_courseSelectionResetsWhenDepartmentChangeInvalidatesIt() {
            var courseChip = findChild(search, "chip_BSEE");
            waitForRendering(search);
            mouseClick(courseChip);
            compare(search.selectedCourse, "BSEE");

            // Selecting department "CE" re-scopes the stub's course list to
            // just ["BSCE"] (mirrors the real VM's setDepartment() re-scoping
            // `courses` via loadCourses) — BSEE is no longer a valid
            // selection, and the screen must reconcile it back to "All
            // Courses" (empty) instead of continuing to search on a course
            // that no longer exists under the new department.
            var deptChip = findChild(search, "deptChip_CE");
            mouseClick(deptChip);

            compare(search.selectedCourse, "");
            compare(searchVmStub.lastCourse, "");
        }

        function test_departmentChipInvokesSetDepartmentAndSearches() {
            var chip = findChild(search, "deptChip_CE");
            verify(chip !== null);
            waitForRendering(search);
            mouseClick(chip);
            compare(search.selectedDepartment, "CE");
            compare(searchVmStub.lastDepartment, "CE");
            compare(searchVmStub.setDepartmentCount, 1);
            verify(searchVmStub.searchCount >= 1);
        }

        // --- Empty state (distinct from the error state) ---

        function test_emptyStateShowsWhenSearchReturnsNothing() {
            var empty = findChild(search, "emptyState");
            verify(empty !== null);
            compare(empty.visible, false);   // no search has run yet this test

            searchStub.clear();
            search.runSearch();
            waitForRendering(search);
            compare(empty.visible, true);
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

        function test_pillRemoveClearsOnlyThatFilter() {
            waitForRendering(search);
            mouseClick(findChild(search, "deptChip_CE"));
            mouseClick(findChild(search, "chip_BSCE"));
            compare(search.selectedDepartment, "CE");
            compare(search.selectedCourse, "BSCE");

            // See the comment in test_clearAllButtonClearsBothFiltersAndSearches:
            // the pill Repeater's model array is recreated wholesale by the
            // course-chip click above, so give Flow a render tick before
            // clicking a pill.
            waitForRendering(search);
            var deptPill = findChild(search, "pill_department");
            verify(deptPill !== null);
            mouseClick(deptPill);

            compare(search.selectedDepartment, "");
            compare(searchVmStub.lastDepartment, "");
            // Removing only the department pill must not also drop the
            // (still department-valid) course filter.
            compare(search.selectedCourse, "BSCE");
        }

        // --- Search invocation branches ---

        function test_queryFieldAcceptedInvokesSearch() {
            var field = findChild(search, "queryField");
            field.text = "2023-0001";
            field.forceActiveFocus();
            keyClick(Qt.Key_Return);
            compare(searchVmStub.lastSearch, "2023-0001");
            compare(searchVmStub.searchCount, 1);
        }

        function test_rendersSchoolId() {
            waitForRendering(search);
            verify(findAny(search, "2023-0001") !== null);
        }

        function test_rendersTotalVisitsLabel() {
            waitForRendering(search);
            verify(findAny(search, "Total Visits: 42") !== null);
        }

        function test_resultCountHeaderRendersCount() {
            waitForRendering(search);
            var header = findChild(search, "resultCountHeader");
            verify(header !== null);
            compare(header.text, "1 results");
        }

        // --- Staggered row entrance (populate/add, mechanism not exact timing) ---

        // The highest-value row-entrance test: proves populate genuinely
        // (re-)plays on a model reset — the whole reason the gatefix migrates
        // to populate/add over the old per-delegate Component.onCompleted
        // approach. A ListView RECYCLES delegates, so the old approach
        // re-ran the entrance (blank row + full stagger) every time a row
        // merely scrolled back into view; populate/add only fire on a
        // genuine model change, never on pure scroll-driven recycling.
        // Reassigning vm.results to a DISTINCT model object is the QML-test
        // equivalent of SearchResultsModel::setRecords()'s real
        // begin/endResetModel() call — both are a "model changed" event that
        // re-fires populate for the fresh row. Caught right after the first
        // rendered frame: with populate broken/disabled the fresh row would
        // already sit at its natural opacity of 1 immediately, never having
        // passed through the deliberate `from: 0` start state at all.
        function test_resultRowsFadeInToFullOpacity() {
            searchVmStub.results = searchMotionFixtureB;
            waitForRendering(search);
            var row = findChild(search, "resultRow_2023-9999");
            verify(row !== null);
            verify(row.opacity < 1);
            tryCompare(row, "opacity", 1);
        }

        // --- Motion (Phase 3 Task A): tuned easing + Theme stagger tokens ---

        // Structural check on the populate Transition's own declared
        // template (not a per-item ViewTransition clone) — reading
        // resultsList.populate here is the same kind of static property-path
        // access LTable.qml's onIdxChanged uses to reach populatePause/
        // populateY by id, just read from the test side instead.
        function test_entranceUsesThemeBezierEasing() {
            waitForRendering(search);
            var list = findChild(search, "resultsList");
            verify(list !== null);
            var seq = list.populate.animations[0];
            verify(seq !== null);
            // seq is PauseAnimation, ParallelAnimation[opacity, y] — inspect
            // the opacity leg's easing directly rather than timing samples,
            // so this can't flake under offscreen CI.
            var opacityAnim = seq.animations[1].animations[0];
            compare(opacityAnim.easing.type, Easing.BezierSpline);
            compare(opacityAnim.easing.bezierCurve.length, Theme.motion.easing.length);
            for (var i = 0; i < Theme.motion.easing.length; i++)
                compare(opacityAnim.easing.bezierCurve[i], Theme.motion.easing[i]);
        }

        // Stagger differential (mutation target: Theme.motion.rowStagger /
        // staggerCap) — mirrors LTable's test_rowStaggerDelaysHigherIndexRows
        // (tst_qml_components.qml) exactly, since the migration means there
        // is no more per-delegate entranceAnim/PauseAnimation to read a
        // duration off directly (populate/add clone their animation tree per
        // transitioning item, so reading the template's own nested
        // properties from outside does not reflect a specific item's
        // resolved values — the differential-timing approach is the
        // reliable one). Row 0 has zero PauseAnimation delay and resolves
        // after ~Theme.motion.rowIn (400ms); row 5 (below staggerCap of 10,
        // and still on-screen in this test host — see below) carries an
        // extra 5 * Theme.motion.rowStagger (125ms at current token values)
        // of pause before its own fade even starts. Waiting 450ms —
        // comfortably past row 0's 400ms settle but well short of row 5's
        // 525ms (125 pause + 400 fade) — must find row 0 fully resolved and
        // row 5 still strictly mid-flight.
        function test_staggerDelayReadsThemeRowStaggerToken() {
            searchVmStub.results = searchStaggerFixture;
            waitForRendering(search);

            // resultsList's viewport (fixed screen height in this test host)
            // is only ~422px tall — fully containing rows 0-5 (row 5 ends at
            // y=384, well under 422) but NOT row 8 (starts at y=512): the
            // ListView never instantiates a delegate that never intersects
            // its viewport, so a taller-index target needs an on-screen row.
            // Row 5 is chosen (not row 6, whose 384-448 band straddles the
            // 422px cutoff) to avoid any edge-of-viewport instantiation
            // flakiness.
            var row0 = findChild(search, "resultRow_S-0");
            var row5 = findChild(search, "resultRow_S-5");
            verify(row0 !== null);
            verify(row5 !== null);
            wait(450);
            compare(row0.opacity, 1);
            verify(row5.opacity > 0 && row5.opacity < 1);
            tryCompare(row5, "opacity", 1);
        }

        // --- Motion (Phase 3 Task A): page entrance (A4) ---

        // Deliberately split in two. The fade/rise is a pure *binding* on
        // screen.pageInT (opacity: pageInT, Translate.y: (1 - pageInT) * 16),
        // so its correctness is provable with ZERO wall-clock dependence by
        // driving pageInT by hand; the animation's job — that it actually
        // runs and lands on 1 — is a separate, poll-based assertion. The
        // previous single test sampled a mid-flight opacity after a fixed
        // wait(80) against a 400 ms animation and failed outright whenever
        // the machine was loaded enough for the animation to finish inside
        // that window.
        function test_pageInBindingsTrackProgress() {
            var col = findChild(search, "contentColumn");
            verify(col !== null);
            var anim = findChild(search, "pageInAnimation");
            verify(anim !== null);

            // The animation targets the very property being set below, so it
            // has to be stopped first or it would overwrite each assignment
            // on the next frame.
            anim.stop();

            search.pageInT = 0;
            // Reset state: content column starts invisible/raised, the root
            // Rectangle itself never moves (only the content column does).
            compare(col.opacity, 0);
            compare(col.transform[0].y, 16);
            compare(search.opacity, 1);

            search.pageInT = 0.5;
            fuzzyCompare(col.opacity, 0.5, 0.0001);
            fuzzyCompare(col.transform[0].y, 8, 0.0001);
            compare(search.opacity, 1);

            // Monotonic across the whole sweep: opacity rises 0 -> 1 as y
            // falls 16 -> 0, with no step in the wrong direction.
            var prevOpacity = -1;
            var prevY = Number.MAX_VALUE;
            for (var i = 0; i <= 10; i++) {
                search.pageInT = i / 10;
                verify(col.opacity > prevOpacity);
                verify(col.transform[0].y < prevY);
                prevOpacity = col.opacity;
                prevY = col.transform[0].y;
            }
            compare(prevOpacity, 1);
            compare(prevY, 0);
        }

        function test_pageInAnimationRunsToCompletion() {
            var col = findChild(search, "contentColumn");
            verify(col !== null);
            var anim = findChild(search, "pageInAnimation");
            verify(anim !== null);

            anim.stop();
            search.pageInT = 0;
            compare(col.opacity, 0);
            compare(col.transform[0].y, 16);

            anim.start();
            // Poll, never sleep: nothing here depends on how far the
            // animation happens to have travelled at a given wall-clock
            // instant, so machine load cannot change the outcome.
            tryCompare(search, "pageInT", 1, 1000);
            tryCompare(col, "opacity", 1, 1000);
            tryCompare(col.transform[0], "y", 0, 1000);
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

        function test_showsResultCount() {
            compare(search.resultCount, 1);
        }

        // --- Loading state ---

        function test_skeletonRowsVisibleWhileLoading() {
            var skeleton = findChild(search, "skeletonColumn");
            verify(skeleton !== null);
            compare(skeleton.visible, false);

            searchVmStub.loading = true;
            compare(skeleton.visible, true);

            searchVmStub.loading = false;
            compare(skeleton.visible, false);
        }

        // --- Debounced live search ---

        function test_typingDebouncesSearchToOneCall() {
            search.debounceMs = 30;
            var field = findChild(search, "queryField");
            field.text = "M";
            wait(10);
            field.text = "Ma";
            wait(10);
            field.text = "Mar";
            // Still inside the (restarted) debounce window — no search yet.
            compare(searchVmStub.searchCount, 0);
            tryCompare(searchVmStub, "searchCount", 1);
            compare(searchVmStub.lastSearch, "Mar");
        }

        // --- No-vm fallback path ---

        function test_undefinedVmRendersFallbacksWithoutError() {
            compare(vmlessSearch.resultCount, 0);
            compare(findChild(vmlessSearch, "errorBlock").visible, false);
            compare(findChild(vmlessSearch, "skeletonColumn").visible, false);
            compare(findChild(vmlessSearch, "emptyState").visible, false);
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
            // The two page-entrance tests drive this down to 0; stop the
            // animation and restore the resting state so every other test
            // sees a fully-opaque, unmoved content column regardless of test
            // order (alphabetical, not declaration) and with nothing left
            // animating over it.
            findChild(logs, "pageInAnimation").stop();
            logs.pageInT = 1;
            vmlessLogs.pageInT = 1;
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

        // --- Motion (Phase 3 Task C): page entrance (C1) ---

        // Split for the same reason as SearchScreen's pair above: the
        // fade/rise is a pure binding on screen.pageInT and is asserted
        // without any wall-clock dependence, while "the animation runs and
        // settles" is a separate poll-based test. No fixed-wait mid-flight
        // sample, which used to fail whenever the 400 ms animation completed
        // inside the sample window under load.
        function test_pageInBindingsTrackProgress() {
            var col = findChild(logs, "contentColumn");
            verify(col !== null);
            var anim = findChild(logs, "pageInAnimation");
            verify(anim !== null);

            // Stop first: the animation drives the same property.
            anim.stop();

            logs.pageInT = 0;
            // Reset state: content column starts invisible/raised, the root
            // Rectangle itself never moves (only the content column does).
            compare(col.opacity, 0);
            compare(col.transform[0].y, 16);
            compare(logs.opacity, 1);

            logs.pageInT = 0.5;
            fuzzyCompare(col.opacity, 0.5, 0.0001);
            fuzzyCompare(col.transform[0].y, 8, 0.0001);
            compare(logs.opacity, 1);

            var prevOpacity = -1;
            var prevY = Number.MAX_VALUE;
            for (var i = 0; i <= 10; i++) {
                logs.pageInT = i / 10;
                verify(col.opacity > prevOpacity);
                verify(col.transform[0].y < prevY);
                prevOpacity = col.opacity;
                prevY = col.transform[0].y;
            }
            compare(prevOpacity, 1);
            compare(prevY, 0);
        }

        function test_pageInAnimationRunsToCompletion() {
            var col = findChild(logs, "contentColumn");
            verify(col !== null);
            var anim = findChild(logs, "pageInAnimation");
            verify(anim !== null);

            anim.stop();
            logs.pageInT = 0;
            compare(col.opacity, 0);
            compare(col.transform[0].y, 16);

            anim.start();
            tryCompare(logs, "pageInT", 1, 1000);
            tryCompare(col, "opacity", 1, 1000);
            tryCompare(col.transform[0], "y", 0, 1000);
        }
    }

    TestCase {
        name: "SettingsScreen"
        when: windowShown

        // Test functions run in alphabetical order and every one of these
        // mutates shared state (the stub, the three key fields, the tier-2
        // dialog, the status line), so reset to a known baseline here rather
        // than relying on declaration order. Leaving the tier-2 dialog open
        // would be the worst of these: its scrim fills the whole screen and
        // swallows every click meant for the cards underneath.
        function init() {
            settingsVmStub.dirty = false;
            settingsVmStub.busy = false;
            settingsVmStub.statusMessage = "";
            settingsVmStub.manifestWriteResult = true;
            settingsVmStub.saveCount = 0;
            settingsVmStub.saveAdminInfoCount = 0;
            settingsVmStub.lastOldKey = "";
            settingsVmStub.lastNewKey = "";
            settingsVmStub.lastResetDept = "";
            settingsVmStub.lastResetKey = "";
            settingsVmStub.lastManifestDept = "";
            settings.statusText = "";
            settings.statusIsError = false;
            settings.adminStatusText = "";
            settings.adminStatusIsError = false;
            settings.resetStatusText = "";
            settings.resetStatusIsError = false;
            settings.activeSection = "";
            findChild(settings, "oldKeyField").text = "";
            findChild(settings, "newKeyField").text = "";
            findChild(settings, "confirmNewKeyField").text = "";
            findChild(settings, "resetDeptPicker").selectValue("");
            var dlg = findChild(settings, "resetConfirmDialog");
            dlg.visible = false;
            dlg.clearKey();
        }

        // Opens the tier-2 dialog for `dept` and returns it, ready to type in.
        function openResetDialog(dept) {
            findChild(settings, "resetDeptPicker").selectValue(dept);
            waitForRendering(settings);
            mouseClick(findChild(settings, "resetVisitsButton"));
            var dlg = findChild(settings, "resetConfirmDialog");
            verify(dlg !== null);
            compare(dlg.visible, true);
            return dlg;
        }

        // --- Fixture invariant ---

        // The screen clips its content (Flickable { clip: true }), so any
        // control that does not fit the fixture's viewport cannot be clicked
        // by mouseClick() — it would silently synthesise a press at a point
        // the control has scrolled away from. Guard the whole band here
        // instead of debugging a mystery "button didn't fire" later.
        function test_fixtureFitsWholeContentColumn() {
            var content = findChild(settings, "settingsContent");
            verify(content !== null);
            verify(settings.height >= content.implicitHeight + Theme.spacing.xxl * 2);
        }

        // --- Footer save gating (dirty + busy) ---

        function test_saveDisabledUntilDirty() {
            var btn = findChild(settings, "saveButton");
            verify(btn !== null);
            compare(btn.enabled, false);
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

        // A save that reports nothing is indistinguishable from a save that
        // silently failed — the status line must confirm success too.
        function test_savedSignalShowsSuccessStatus() {
            var status = findChild(settings, "settingsStatus");
            verify(status !== null);
            compare(status.visible, false);
            settingsVmStub.saved();
            compare(status.visible, true);
            compare(status.text, "Settings saved.");
            compare(status.color, Theme.success);
        }

        // The status line renders the backend's "message" field verbatim over
        // cleartext HTTP. Text defaults to AutoText, which auto-detects and
        // RENDERS rich text — a tampered response carrying
        // "<img src=http://attacker/beacon>" would be fetched by the kiosk.
        function test_statusLineRendersServerTextAsPlainNotRichText() {
            var status = findChild(settings, "settingsStatus");
            compare(status.textFormat, Text.PlainText);
            settingsVmStub.saveFailed("<b>bold</b> <img src='http://attacker/beacon'>");
            // Rendered verbatim, tags and all — never parsed as markup.
            compare(status.text, "<b>bold</b> <img src='http://attacker/beacon'>");
        }

        // Department names come from get_departments.php, so the picker's
        // value label is server-controlled text too.
        function test_deptPickerRendersServerTextAsPlainNotRichText() {
            compare(findChild(settings, "comboValueText").textFormat, Text.PlainText);
        }

        function test_saveFailedShowsErrorStatus() {
            var status = findChild(settings, "settingsStatus");
            settingsVmStub.saveFailed("Disk is read-only.");
            compare(status.visible, true);
            compare(status.text, "Disk is read-only.");
            compare(status.color, Theme.error);
        }

        // --- Admin info ---

        function test_saveAdminInfoInvokesVm() {
            waitForRendering(settings);
            mouseClick(findChild(settings, "saveAdminInfoButton"));
            compare(settingsVmStub.saveAdminInfoCount, 1);
        }

        function test_saveAdminInfoDisabledWhileBusy() {
            settingsVmStub.busy = true;
            compare(findChild(settings, "saveAdminInfoButton").enabled, false);
        }

        // Admin outcomes report in the Administrator card, NOT the footer: the
        // screen is a tall Flickable and the footer line sits two cards below
        // the fold, where an error reads as "nothing happened".
        function test_adminInfoOutcomesReachAdminCardNotFooter() {
            var status = findChild(settings, "adminStatus");
            var footer = findChild(settings, "settingsStatus");
            verify(status !== null);
            compare(status.visible, false);
            settingsVmStub.adminInfoSaved();
            compare(status.visible, true);
            compare(status.text, "Admin info saved.");
            compare(status.color, Theme.success);
            settingsVmStub.adminInfoFailed("Server rejected the update.");
            compare(status.text, "Server rejected the update.");
            compare(status.color, Theme.error);
            // Nothing leaked into the other sections' lines.
            compare(footer.visible, false);
            compare(findChild(settings, "resetStatus").visible, false);
        }

        // The mirror of the above: the footer keeps the whole-form save()
        // outcome and must not push it into a card that did not raise it.
        function test_saveOutcomeStaysInFooterNotAdminCard() {
            settingsVmStub.saveFailed("Disk is read-only.");
            compare(findChild(settings, "settingsStatus").text, "Disk is read-only.");
            compare(findChild(settings, "adminStatus").visible, false);
            compare(findChild(settings, "resetStatus").visible, false);
        }

        // Per-section lines carry the same verbatim backend "message" fields as
        // the footer, so they need the same AutoText defence.
        function test_sectionStatusLinesRenderServerTextAsPlainNotRichText() {
            var adminStatus = findChild(settings, "adminStatus");
            var resetStatus = findChild(settings, "resetStatus");
            compare(adminStatus.textFormat, Text.PlainText);
            compare(resetStatus.textFormat, Text.PlainText);
            settingsVmStub.adminInfoFailed("<b>bold</b> <img src='http://attacker/beacon'>");
            compare(adminStatus.text, "<b>bold</b> <img src='http://attacker/beacon'>");
            settingsVmStub.resetFailed("<b>bold</b>");
            compare(resetStatus.text, "<b>bold</b>");
        }

        // A stale outcome under a button whose new request is in flight reads as
        // this attempt's result. Starting an action must drop the old message.
        function test_startingAnAdminActionClearsTheStaleAdminStatus() {
            var status = findChild(settings, "adminStatus");
            settingsVmStub.adminInfoFailed("Server rejected the update.");
            compare(status.visible, true);
            waitForRendering(settings);
            mouseClick(findChild(settings, "saveAdminInfoButton"));
            compare(settingsVmStub.saveAdminInfoCount, 1);
            compare(status.visible, false);
        }

        // authFailed/networkError are shared by all three POSTs — they must come
        // back to the card whose button was pressed, never to another one.
        function test_sharedFailureFollowsTheSectionThatStartedTheCall() {
            var adminStatus = findChild(settings, "adminStatus");
            waitForRendering(settings);
            mouseClick(findChild(settings, "saveAdminInfoButton"));
            settingsVmStub.networkError();
            compare(adminStatus.visible, true);
            compare(adminStatus.text, "Could not reach the server. Check the connection and try again.");
            compare(adminStatus.color, Theme.error);
            compare(findChild(settings, "resetStatus").visible, false);
            compare(findChild(settings, "settingsStatus").visible, false);
        }

        // --- Change admin key ---

        function test_changeKeyRequiresMatchingConfirmation() {
            var btn = findChild(settings, "changeKeyButton");
            verify(btn !== null);
            compare(btn.enabled, false);                    // both fields empty
            findChild(settings, "newKeyField").text = "abc";
            findChild(settings, "confirmNewKeyField").text = "xyz";
            compare(btn.enabled, false);                    // mismatch
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

        // The old and new admin keys must not sit in the UI for the rest of
        // the session once the change has landed.
        function test_changeKeyFieldsClearedOnKeyChanged() {
            findChild(settings, "oldKeyField").text = "OLD";
            findChild(settings, "newKeyField").text = "NEW";
            findChild(settings, "confirmNewKeyField").text = "NEW";
            settingsVmStub.keyChanged();
            compare(findChild(settings, "oldKeyField").text, "");
            compare(findChild(settings, "newKeyField").text, "");
            compare(findChild(settings, "confirmNewKeyField").text, "");
            compare(findChild(settings, "adminStatus").text, "Admin key changed.");
        }

        // An empty failure message still has to say something — next to the
        // Change Key button, which is the case the owner hit in smoke testing.
        function test_changeKeyFailureFallsBackToGenericMessageInAdminCard() {
            var status = findChild(settings, "adminStatus");
            settingsVmStub.keyChangeFailed("");
            compare(status.visible, true);
            compare(status.text, "Could not change the admin key.");
            compare(status.color, Theme.error);
            compare(findChild(settings, "settingsStatus").visible, false);
            compare(findChild(settings, "resetStatus").visible, false);
        }

        // --- Reset visits: department gating ---

        function test_resetControlsRequireADepartment() {
            var resetBtn = findChild(settings, "resetVisitsButton");
            var manifestBtn = findChild(settings, "manifestButton");
            compare(resetBtn.enabled, false);
            compare(manifestBtn.enabled, false);
            findChild(settings, "resetDeptPicker").selectValue("CE");
            compare(resetBtn.enabled, true);
            compare(manifestBtn.enabled, true);
        }

        // --- Reset visits: the tier-2 dialog ---

        function test_resetButtonOpensTier2DialogWithConfirmDisabled() {
            var dlg = openResetDialog("CE");
            compare(dlg.tier, 2);
            var confirmBtn = findChild(dlg, "confirmButton");
            verify(confirmBtn !== null);
            // No key typed yet — the re-typed key is the whole point of tier 2.
            compare(confirmBtn.enabled, false);
            findChild(dlg, "confirmKeyField").text = "typed";
            compare(confirmBtn.enabled, true);
        }

        function test_resetConfirmedSendsDeptAndFreshKey() {
            var dlg = openResetDialog("IT");
            findChild(dlg, "confirmKeyField").text = "fresh-key";
            waitForRendering(settings);
            mouseClick(findChild(dlg, "confirmButton"));
            compare(settingsVmStub.lastResetDept, "IT");
            compare(settingsVmStub.lastResetKey, "fresh-key");
        }

        function test_resetConfirmDisabledWhileBusy() {
            var dlg = openResetDialog("CE");
            findChild(dlg, "confirmKeyField").text = "typed";
            compare(findChild(dlg, "confirmButton").enabled, true);
            settingsVmStub.busy = true;
            compare(findChild(dlg, "confirmButton").enabled, false);
        }

        // The safeguard a T14 review found broken: the re-typed key is
        // deliberate friction in front of an irreversible mass delete, so it
        // must NOT survive a single use. Without the clear-on-show, the second
        // reset of a session opens with Confirm already enabled and no
        // re-typing at all.
        function test_tier2KeyDoesNotSurviveAReopen() {
            var dlg = openResetDialog("CE");
            findChild(dlg, "confirmKeyField").text = "first-key";
            waitForRendering(settings);
            mouseClick(findChild(dlg, "confirmButton"));
            compare(settingsVmStub.lastResetKey, "first-key");

            settingsVmStub.visitsReset();           // VM reports success
            compare(dlg.visible, false);

            waitForRendering(settings);
            mouseClick(findChild(settings, "resetVisitsButton"));
            compare(dlg.visible, true);
            compare(findChild(dlg, "confirmKeyField").text, "");
            compare(findChild(dlg, "confirmButton").enabled, false);
        }

        // --- Reset visits: outcome surfacing ---

        function test_visitsResetClosesDialogAndConfirmsDepartment() {
            var dlg = openResetDialog("BA");
            settingsVmStub.visitsReset();
            compare(dlg.visible, false);
            var status = findChild(settings, "resetStatus");
            compare(status.text, "Visits reset for BA.");
            compare(status.color, Theme.success);
            // The dialog vanishes on both outcomes; what is left behind is this
            // card, so the explanation belongs here and not in the footer.
            compare(findChild(settings, "settingsStatus").visible, false);
            compare(findChild(settings, "adminStatus").visible, false);
        }

        // A failed reset used to leave the dialog hanging open with no
        // explanation, which reads exactly like a no-op.
        function test_resetFailedClosesDialogAndShowsError() {
            var dlg = openResetDialog("CE");
            findChild(dlg, "confirmKeyField").text = "typed";
            settingsVmStub.resetFailed("Department not found.");
            compare(dlg.visible, false);
            var status = findChild(settings, "resetStatus");
            compare(status.visible, true);
            compare(status.text, "Department not found.");
            compare(status.color, Theme.error);
            compare(findChild(dlg, "confirmKeyField").text, "");
        }

        function test_resetFailedWithEmptyMessageFallsBack() {
            openResetDialog("CE");
            settingsVmStub.resetFailed("");
            compare(findChild(settings, "resetStatus").text, "Reset failed.");
        }

        function test_authFailedClosesDialogAndShowsError() {
            var dlg = openResetDialog("CE");
            findChild(dlg, "confirmKeyField").text = "wrong";
            settingsVmStub.authFailed();
            compare(dlg.visible, false);
            // openResetDialog() clicked the Reset Visits button, so the reset
            // card owns the in-flight call and the shared failure lands there.
            var status = findChild(settings, "resetStatus");
            compare(status.visible, true);
            compare(status.text, "Admin key rejected. Please sign in again.");
            compare(status.color, Theme.error);
            compare(findChild(dlg, "confirmKeyField").text, "");
        }

        function test_networkErrorClosesDialogAndShowsError() {
            var dlg = openResetDialog("CE");
            findChild(dlg, "confirmKeyField").text = "typed";
            settingsVmStub.networkError();
            compare(dlg.visible, false);
            var status = findChild(settings, "resetStatus");
            compare(status.visible, true);
            compare(status.text, "Could not reach the server. Check the connection and try again.");
            compare(findChild(dlg, "confirmKeyField").text, "");
        }

        // statusMessage carries the VM-side failures with no dedicated signal
        // (currently the logo import) — its NOTIFY is statusChanged.
        function test_vmStatusMessageSurfacesAsError() {
            var status = findChild(settings, "settingsStatus");
            settingsVmStub.statusMessage = "Could not copy the logo file.";
            settingsVmStub.statusChanged();
            compare(status.visible, true);
            compare(status.text, "Could not copy the logo file.");
            compare(status.color, Theme.error);
        }

        // --- No-vm fallback path ---

        function test_undefinedVmRendersFallbacksWithoutError() {
            // Every action gate must fail closed with nothing to act on.
            compare(findChild(vmlessSettings, "saveButton").enabled, false);
            compare(findChild(vmlessSettings, "saveAdminInfoButton").enabled, false);
            compare(findChild(vmlessSettings, "changeKeyButton").enabled, false);
            compare(findChild(vmlessSettings, "resetVisitsButton").enabled, false);
            compare(findChild(vmlessSettings, "manifestButton").enabled, false);
            // No vm means no outcome to report, and no tier-2 dialog on screen.
            compare(findChild(vmlessSettings, "settingsStatus").visible, false);
            compare(findChild(vmlessSettings, "adminStatus").visible, false);
            compare(findChild(vmlessSettings, "resetStatus").visible, false);
            compare(findChild(vmlessSettings, "resetConfirmDialog").visible, false);
            // The logo slot must not latch on with no vm to supply one.
            compare(findChild(vmlessSettings, "logoPreview").visible, false);
        }
    }
}
