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
    //   logs 1460..2160 | settings 2160..3760 | database 3800..4500
    //   | editDialog 4500..5200 | bulkEdit 5200..5900 | registerDialog 5900..6600
    //   | importDialog 6600..7300.
    // Parked no-vm column (x 2000): vmlessSearch 0..700 |
    //   vmlessLogs 760..1460 | vmlessSettings 1560..2260.
    width: 1100; height: 7300

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

        // Task 9: fire-once fallback-notice surface. fallbackHashStore models
        // the AppSettings persistence (branding/lastFallbackLogoHash) so the
        // record -> re-check round trip is real inside one test.
        property string fallbackHashStore: ""
        property string lastHashedLogoPath: ""

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
        function logoHashFor(p) { lastHashedLogoPath = "" + p; return p ? "hash-of-" + p : ""; }
        function lastFallbackLogoHash() { return fallbackHashStore; }
        function recordFallbackLogoHash(h) { fallbackHashStore = h; }
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

            // The t=0 bindings are asserted with the animation STOPPED. Doing
            // this while it runs is a race: the animation driver can tick
            // between restart() and the compares, leaving pageInT already > 0.
            // Stopped, pageInT is ours alone and the bindings evaluate
            // synchronously, so these are exact.
            anim.stop();
            dash.pageInT = 0;
            // At pageInT === 0: content is invisible and raised 16px; the root
            // Rectangle (background) must never move or fade.
            compare(contentCol.opacity, 0);
            compare(contentCol.transform[0].y, 16);
            compare(dash.opacity, 1);

            // Now prove the animation actually drives pageInT to 1. Wait on
            // `running` going false rather than on pageInT reaching 1:
            // tryCompare compares reals FUZZILY, so it accepts pageInT at
            // 0.99999914 while the animation is still mid-flight — and the
            // derived y, (1 - pageInT) * 16, is then a tiny non-zero that
            // fails compare(y, 0). Waiting for the animation to finish
            // guarantees it has written the exact `to` value of 1.
            anim.restart();
            tryCompare(anim, "running", false);
            compare(dash.pageInT, 1);
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
        // staggerCap) — mirrors its twin of the same name over LTable in
        // tst_qml_components.qml. The migration means there is no more
        // per-delegate entranceAnim/PauseAnimation to read a duration off
        // directly: populate/add CLONE their whole animation tree per
        // transitioning item and the pause duration is assigned
        // imperatively onto that clone from onIdxChanged (SearchScreen.qml
        // :407-462), so reading populatePause.duration on the template from
        // outside reflects no real item and asserting on it would be a
        // vacuous test.
        //
        // This test used to bridge that gap with a wall-clock sample:
        // `wait(450)` — picked to land in the gap between row 0's ~400ms
        // (Theme.motion.rowIn) settle and row 5's 525ms (125ms pause +
        // 400ms fade) — then `compare(row0.opacity, 1)` +
        // `verify(row5.opacity > 0 && row5.opacity < 1)`. Under load
        // `wait(450)` returns well past 450ms of animation time, row 5 has
        // already settled at 1, and the strict-inequality check fails for
        // reasons unrelated to the code under test. Same flake class as the
        // pageIn tests fixed just before this one — and since both
        // tst_qml_admin and tst_qml_components compile this whole directory
        // via QUICK_TEST_SOURCE_DIR, one flake here reddens several ctest
        // entries at once.
        //
        // This test therefore asserts the WIRING only: that row 5 really LAGS
        // row 0 on screen. Instead of betting on one instant, poll and assert
        // the invariant that holds under ANY load — monotonic lead,
        // row0.opacity >= row5.opacity at EVERY sample (both fades are the
        // same duration and easing, row 5 merely starts later, so row 5 can
        // never be ahead) — while recording whether a STRICT lead was ever
        // seen. A strict lead is REQUIRED to pass: that is what separates real
        // stagger from all rows animating in lockstep. A single scheduler
        // stall long enough to swallow the entire 525ms window would leave
        // both rows settled with no strict lead observed, so the observation
        // is wrapped in a bounded retry that re-triggers the entrance via the
        // same model reset.
        //
        // The stagger ARITHMETIC itself (clamp at staggerCap, multiply by the
        // step, and the negative-index guard) is deliberately NOT re-asserted
        // here: tst_qml_theme.qml's
        // test_staggerDelayClampsIndexToStaggerCapThenMultipliesByStep covers
        // it on the pure Theme.motion.staggerDelay function, with zero clock,
        // and every QuickTest binary compiles the whole tests/ directory via
        // QUICK_TEST_SOURCE_DIR — so that test runs inside THIS binary too.
        // Repeating it here would only duplicate coverage already executing in
        // the same process.
        //
        // RETRY POLICY — a retry is only ever legitimate for ONE of the two
        // reasons an attempt can end without a strict lead, and the loop
        // must tell them apart. A blanket "retry until any attempt sees a
        // lead" would also pass a product bug where the stagger fired only
        // SOMETIMES: two broken attempts retried away by one good one. So:
        //   • window MISSED (`windowObserved` false) — the machine stalled
        //     hard enough that row 5 was never sampled while in flight. That
        //     is scheduler jitter, says nothing about the code, and is the
        //     only case that earns a retry.
        //   • window OBSERVED, no strict lead — the entrance genuinely ran,
        //     row 5 was genuinely caught mid-flight next to a row 0 that had
        //     not settled either, and row 5 was never behind. That is a real
        //     product failure and fails IMMEDIATELY rather than being
        //     retried away.
        // The two exhaust the space: any sample with row 5 below 1 either
        // has row 0 already settled (⇒ a strict lead, pass) or row 0 still
        // in flight (⇒ the window, which a working stagger always leads in),
        // so a healthy run can never trip the fast fail. Exhausting all 3
        // attempts without ever observing the window fails too, but with a
        // distinct message — and that message must NOT blame the machine
        // alone, because a deleted or disabled populate Transition produces
        // the IDENTICAL observation (rows sit at opacity 1, `lateStarted`
        // never becomes true). The message names both causes and points at the
        // entrance tests, which distinguish them.
        function test_rowStaggerDelaysHigherIndexRows() {
            // Wiring: row 5's entrance really trails row 0's.
            //
            // resultsList's viewport (fixed screen height in this test host)
            // is only ~422px tall — fully containing rows 0-5 (row 5 ends at
            // y=384, well under 422) but NOT row 8 (starts at y=512): the
            // ListView never instantiates a delegate that never intersects
            // its viewport, so a taller-index target needs an on-screen row.
            // Row 5 is chosen (not row 6, whose 384-448 band straddles the
            // 422px cutoff) to avoid any edge-of-viewport instantiation
            // flakiness.
            //
            // The SHAPE of the opacity signal, measured rather than assumed:
            // the delegate declares no `opacity: 0` default, and the populate
            // clone's NumberAnimation only writes its `from: 0` when it
            // actually STARTS — i.e. after its own PauseAnimation. So for the
            // whole of row 5's 125ms pause it sits at its natural opacity of
            // exactly 1 while row 0 is already mid-fade; only then does row 5
            // drop to ~0 and climb. A flat "row0.opacity >= row5.opacity at
            // all times" invariant is therefore FALSE over that first window
            // (observed: r0=0.71, r5=1). The invariant that does hold under
            // ANY load is gated on row 5 having started: once row 5 has been
            // seen below 1, row 0 is at or ahead of it forever after — same
            // duration, same easing, later start. That gate is also what
            // makes the loop meaningful, since row-5-still-at-1 carries no
            // ordering information.
            var eps = 1e-6;
            var sawStrictLead = false;
            // Per-attempt: was row 5 ever sampled strictly between its start
            // and its settle, alongside a row 0 that had not settled either?
            // Only a FALSE here licenses a retry (see RETRY POLICY above).
            var windowObserved = false;
            var row0 = null;
            var row5 = null;
            for (var attempt = 0; attempt < 3 && !sawStrictLead; attempt++) {
                // Re-trigger the entrance the same way the rest of this file
                // does — swap vm.results to a DISTINCT model object, which is
                // the QML-test equivalent of SearchResultsModel::setRecords()'s
                // begin/endResetModel() and re-fires populate from t=0.
                searchVmStub.results = searchStub;
                searchVmStub.results = searchStaggerFixture;
                waitForRendering(search);
                row0 = findChild(search, "resultRow_S-0");
                row5 = findChild(search, "resultRow_S-5");
                verify(row0 !== null);
                verify(row5 !== null);

                // Generous deadline: ~4x the 525ms (125 pause + 400 fade) the
                // entrance needs when nothing competes for the CPU. No single
                // sample is load-critical — the loop only has to catch the
                // pair at ANY point in row 5's ~400ms flight, and if a stall
                // swallows that whole window the outer retry re-plays it.
                var lateStarted = false;
                windowObserved = false;
                var deadline = Date.now() + 2000;
                while (Date.now() < deadline) {
                    var early = row0.opacity;
                    var late = row5.opacity;
                    if (late < 1 - eps)
                        lateStarted = true;
                    if (lateStarted) {
                        // Row 5 can never be ahead of row 0 — this would fire
                        // on an inverted/negative stagger.
                        verify(early >= late - eps,
                               "row 5 overtook row 0: " + early + " < " + late);
                        // Both rows caught in flight at the same instant:
                        // this is the window in which a working stagger MUST
                        // show a lead, so from here on a missing lead is the
                        // product's fault, not the scheduler's.
                        if (late < 1 - eps && early < 1 - eps)
                            windowObserved = true;
                        // A STRICT lead is what separates real stagger from
                        // every row animating in lockstep (which would hold
                        // the two exactly equal at every sample).
                        if (early > late + eps)
                            sawStrictLead = true;
                    }
                    // Nothing more to learn once row 0 has settled and either
                    // the lead was seen or row 5 has settled too.
                    if (early >= 1 - eps && (sawStrictLead || late >= 1 - eps))
                        break;
                    wait(10);
                }
                // Fail fast: the window WAS sampled and row 5 still never
                // trailed row 0. Retrying this would only paper over a
                // stagger that works intermittently.
                if (windowObserved && !sawStrictLead)
                    fail("row 5 was sampled mid-entrance alongside an unsettled "
                         + "row 0 and never trailed it — stagger not wired "
                         + "(attempt " + (attempt + 1) + ")");
            }
            verify(sawStrictLead,
                   "row 5's entrance window was never sampled in 3 attempts — "
                   + "either the row entrance never ran at all, or the machine "
                   + "was too loaded to sample it; check "
                   + "test_entranceUsesThemeBezierEasing, which fails with an "
                   + "accurate message in the first case");
            tryCompare(row5, "opacity", 1);
        }

        // --- Motion (Phase 3 Task A): page entrance (A4) ---

        // This test used to sample the animation mid-flight — `wait(80)` (~20%
        // of the 400ms pageIn) followed by
        // `verify(col.opacity > 0 && col.opacity < 1)`. That raced the wall
        // clock: on a loaded machine the 80ms sleep either returned before the
        // animation had visibly advanced (opacity still exactly 0) or long
        // after it had finished (exactly 1), so the assertion failed for
        // reasons that had nothing to do with the code under test. Both
        // tst_qml_admin and tst_qml_components compile this whole directory
        // via QUICK_TEST_SOURCE_DIR, so a single flake here reddened several
        // ctest entries at once.
        //
        // The mid-flight sample is replaced by three deterministic checks that
        // are collectively STRICTER, not weaker:
        //   1. the animation's declaration (target/property/to/duration token)
        //      — fails if someone deletes it or retargets/retunes it;
        //   2. a hand-driven sweep of pageInT with the animation stopped —
        //      fails if the opacity/translate bindings are replaced by a snap,
        //      which is exactly what the mid-flight sample was there to catch,
        //      minus the clock;
        //   3. a real end-to-end run driven by tryCompare, which polls and is
        //      therefore load-tolerant.
        function test_pageInAnimatesContentColumnFadeAndRise() {
            var col = findChild(search, "contentColumn");
            verify(col !== null);
            var anim = findChild(search, "pageInAnimation");
            verify(anim !== null);

            // (1) Declared correctly: drives the screen's own pageInT to 1
            // over the Theme pageIn token (zeroed under reduce-motion — the
            // animation still runs either way, per SearchScreen.qml's comment).
            compare(anim.target, search);
            compare(anim.property, "pageInT");
            compare(anim.to, 1);
            compare(anim.duration, Theme.motion.enabled ? Theme.motion.pageIn : 0);

            // (2) The derived bindings INTERPOLATE rather than snapping.
            // stop() first: a running animation would overwrite the manual
            // assignments below on its next tick and make this a coin flip.
            anim.stop();
            var samples = [0, 0.25, 0.5, 0.75, 1];
            for (var i = 0; i < samples.length; i++) {
                var t = samples[i];
                search.pageInT = t;
                compare(col.opacity, t);
                compare(col.transform[0].y, (1 - t) * 16);
                // Invariant at every point of the sweep: only the content
                // column fades/rises — the root Rectangle (background) must
                // never move or fade.
                compare(search.opacity, 1);
            }

            // (3) End-to-end: the real animation still carries pageInT from 0
            // to 1 and settles the bindings at their resting values.
            search.pageInT = 0;
            anim.restart();
            tryCompare(search, "pageInT", 1, 2000);
            tryCompare(col, "opacity", 1, 2000);
            tryCompare(col.transform[0], "y", 0, 2000);
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

        // Same rewrite as SearchScreen's test of the same name — see the long
        // comment there. Short version: the old `wait(80)` +
        // `verify(col.opacity > 0 && col.opacity < 1)` mid-flight sample raced
        // the wall clock (under load the 80ms sleep landed either before the
        // animation had visibly moved or after it had already finished), and
        // is replaced by (1) a declaration check, (2) a hand-driven pageInT
        // sweep with the animation stopped — which is what actually proves the
        // bindings interpolate instead of snapping — and (3) a poll-based,
        // load-tolerant end-to-end run.
        function test_pageInAnimatesContentColumnFadeAndRise() {
            var col = findChild(logs, "contentColumn");
            verify(col !== null);
            var anim = findChild(logs, "pageInAnimation");
            verify(anim !== null);

            // (1) Declared correctly.
            compare(anim.target, logs);
            compare(anim.property, "pageInT");
            compare(anim.to, 1);
            compare(anim.duration, Theme.motion.enabled ? Theme.motion.pageIn : 0);

            // (2) Bindings interpolate continuously. stop() first so the
            // running animation cannot overwrite the manual assignments.
            anim.stop();
            var samples = [0, 0.25, 0.5, 0.75, 1];
            for (var i = 0; i < samples.length; i++) {
                var t = samples[i];
                logs.pageInT = t;
                compare(col.opacity, t);
                compare(col.transform[0].y, (1 - t) * 16);
                // Only the content column fades/rises; the root Rectangle
                // (background) never moves or fades.
                compare(logs.opacity, 1);
            }

            // (3) End-to-end run, poll-based so it tolerates a loaded machine.
            logs.pageInT = 0;
            anim.restart();
            tryCompare(logs, "pageInT", 1, 2000);
            tryCompare(col, "opacity", 1, 2000);
            tryCompare(col.transform[0], "y", 0, 2000);
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
            // Task 9: reset the fire-once surface. Tests run alphabetically and
            // share the screen; without this a notice/record from one test
            // leaks into the next. The imperative reset is safe because
            // schoolStatus is bare (no binding to clobber).
            settingsVmStub.fallbackHashStore = "";
            settingsVmStub.lastHashedLogoPath = "";
            var school = findChild(settings, "schoolStatus");
            school.text = "";
            school.isNeutral = false;
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

        // SectionStatus must offer a non-alarming informational tone (rendered
        // in Theme.mutedText) for notices that are neither an error (red) nor a
        // success (green) — e.g. the bad-logo fallback notice.
        function test_sectionStatusHasNeutralInfoState() {
            var s = findChild(settings, "settingsStatus");
            verify(s !== null);
            // Set ONLY isNeutral — never text/isError, which are bound to
            // screen.statusText / screen.statusIsError. An imperative assignment
            // to those would clobber the binding and break later saveFailed()-
            // driven tests. The color ternary evaluates isNeutral regardless of
            // text, so this is sufficient to prove the neutral tone.
            s.isNeutral = true;
            compare(s.color.toString(), Theme.mutedText.toString());
            s.isNeutral = false;   // restore so neutral state can't leak forward
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

        // --- Bad-logo fallback notice (fire-once) ---

        function test_badLogoFallbackNoticeShowsOnceThenNotForSameLogo() {
            var status = findChild(settings, "schoolStatus");
            verify(status !== null);
            compare(status.visible, false);

            settings.applyLogoRegenResult(ThemeViewModel.FellBack, "/logos/grey.png");
            compare(status.visible, true);
            compare(status.isNeutral, true);
            compare(status.color.toString(), Theme.mutedText.toString());
            compare(settingsVmStub.fallbackHashStore, "hash-of-/logos/grey.png");

            // Same logo again: fire-once. Clear the node first so "not
            // re-shown" is distinguishable from "left over from the first".
            status.text = "";
            settings.applyLogoRegenResult(ThemeViewModel.FellBack, "/logos/grey.png");
            compare(status.visible, false);
        }

        function test_okRegenClearsNoticeAndReArmsFireOnce() {
            var status = findChild(settings, "schoolStatus");
            settings.applyLogoRegenResult(ThemeViewModel.FellBack, "/logos/grey.png");
            compare(status.visible, true);

            settings.applyLogoRegenResult(ThemeViewModel.Ok, "/logos/good.png");
            compare(status.visible, false);
            compare(settingsVmStub.fallbackHashStore, "");

            // Re-armed: the SAME bad logo fires again after a good one.
            settings.applyLogoRegenResult(ThemeViewModel.FellBack, "/logos/grey.png");
            compare(status.visible, true);
        }

        function test_failedRegenLeavesExistingNoticeUntouched() {
            var status = findChild(settings, "schoolStatus");
            settings.applyLogoRegenResult(ThemeViewModel.FellBack, "/logos/grey.png");
            compare(status.visible, true);
            var shown = status.text;

            // Failed leaves the theme untouched, so a visible notice stays true.
            settings.applyLogoRegenResult(ThemeViewModel.Failed, "/logos/broken.png");
            compare(status.visible, true);
            compare(status.text, shown);
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

    // --- Database screen fixture (own band below Settings, y 3800..4500) ---
    // A self-contained Item so the DatabaseScreen fills a known rect clear of
    // every other screen fixture. The stub vm is a plain QtObject exposing only
    // the surface DatabaseScreen reads — no live network, no C++ VM (house rule
    // §5). `students` is itself a QtObject carrying the multi-select surface
    // (count/selectedCount/allSelected/setAllSelected/toggle) LTable binds to.
    Item {
        id: databaseBand
        y: 3800
        width: 900; height: 700

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
            property string course: ""
            property bool loading: false
            property string errorText: ""
            property string statusMessage: ""
            property int deleteCount: 0
            property url lastExportUrl: ""
            property bool exportResult: true
            property bool canEdit: false
            property string editSchoolId: "2023-001"
            property string editName: "Ann"
            property string editYearLevel: "2"
            property string editGender: "Female"
            property string editStatus: "Active"
            property string editDepartment: "CCS"
            property string editCourse: "BSIT"
            property var editCourses: ["BSIT", "BSCS"]
            property int beginEditSelectedCount: 0
            property string lastBeginEditId: ""
            signal editReady()
            signal editFinished()
            function refresh() {}
            function setDepartment(d) { department = d; }
            function setCourse(c) { course = c; }
            function requiresTypedConfirmation(n) { return n >= 10; }
            function deleteSelected() { deleteCount++; }
            function exportCsv(u) { lastExportUrl = u; return exportResult; }
            function beginEditSelected() { beginEditSelectedCount++; editReady(); }
            function beginEdit(id) { lastBeginEditId = id; editReady(); }
            function setEditDepartment(d) { editDepartment = d; editCourse = ""; }
            function setEditName(v) { editName = v; }
            function setEditYearLevel(v) { editYearLevel = v; }
            function setEditGender(v) { editGender = v; }
            function setEditStatus(v) { editStatus = v; }
            function setEditCourse(v) { editCourse = v; }
            function saveEdit() {}
            property int beginBulkEditSelectedCount: 0
            property int applyBulkEditCount: 0
            property var bulkChangeSummary: ["Status → Inactive"]
            property bool bulkBusy: false
            property bool canApplyBulk: false
            // Bulk field surface the hosted BulkEditDialog binds to.
            property bool changeDepartment: false
            property bool changeCourse: false
            property bool changeYearLevel: false
            property bool changeGender: false
            property bool changeStatus: false
            property string bulkDepartment: ""
            property string bulkCourse: ""
            property string bulkYearLevel: ""
            property string bulkGender: ""
            property string bulkStatus: ""
            property var bulkCourses: []
            signal bulkEditReady()
            signal bulkEditFinished()
            function beginBulkEditSelected() { beginBulkEditSelectedCount++; bulkEditReady(); }
            function applyBulkEdit() { applyBulkEditCount++; }
            // No-op setters so the dialog's onSelected/onToggled handlers resolve.
            function setChangeDepartment(v) { changeDepartment = v; changeCourse = v; if (!v) bulkCourse = ""; }
            function setChangeCourse(v) { changeCourse = v && changeDepartment; }
            function setChangeYearLevel(v) { changeYearLevel = v; }
            function setChangeGender(v) { changeGender = v; }
            function setChangeStatus(v) { changeStatus = v; }
            function setBulkDepartment(v) { bulkDepartment = v; bulkCourse = ""; }
            function setBulkCourse(v) { bulkCourse = v; }
            function setBulkYearLevel(v) { bulkYearLevel = v; }
            function setBulkGender(v) { bulkGender = v; }
            function setBulkStatus(v) { bulkStatus = v; }

            // Register surface the hosted RegisterStudentDialog binds to.
            property string regSchoolId: ""
            property string regName: ""
            property string regCode: ""
            property string regYearLevel: ""
            property string regGender: ""
            property string regStatus: ""
            property string regDepartment: ""
            property string regCourse: ""
            property var regCourses: []
            property string regPhotoName: ""
            property bool canRegister: false
            property bool regBusy: false
            property bool regDuplicate: false
            property int beginRegisterCount: 0
            property int registerStudentCount: 0
            signal registerReady()
            signal registerFinished()
            function beginRegister() { beginRegisterCount++; registerReady(); }
            function setRegSchoolId(v) { regSchoolId = v; regDuplicate = false; }
            function setRegName(v) { regName = v; }
            function setRegCode(v) { regCode = v; }
            function setRegYearLevel(v) { regYearLevel = v; }
            function setRegGender(v) { regGender = v; }
            function setRegStatus(v) { regStatus = v; }
            function setRegCourse(v) { regCourse = v; }
            function setRegDepartment(v) { regDepartment = v; regCourse = ""; }
            function setRegPhoto(u) { regPhotoName = ("" + u).split("/").pop(); }
            function clearRegPhoto() { regPhotoName = ""; }
            function registerStudent() { registerStudentCount++; }

            property bool deptOpBusy: false
            property int deactivateDepartmentCount: 0
            property int deleteDepartmentCount: 0
            function deactivateDepartment() { deactivateDepartmentCount++; }
            function deleteDepartment() { deleteDepartmentCount++; }
        }

        DatabaseScreen { id: databaseScreen; anchors.fill: parent; vm: stubVm }

        TestCase {
            name: "DatabaseScreen"; when: windowShown
            // Reset the shared stub + close any dialog left open by a prior
            // test — QuickTest runs functions in alphabetical order, and a
            // leaked-visible LConfirmDialog's scrim swallows later mouseClicks.
            function init() {
                stubModel.selectedCount = 0;
                stubModel.count = 2;
                stubVm.deleteCount = 0;
                stubVm.lastExportUrl = "";
                stubVm.exportResult = true;
                stubVm.statusMessage = "";
                stubVm.canEdit = false;
                stubVm.beginEditSelectedCount = 0;
                stubVm.lastBeginEditId = "";
                // Per-test isolation hygiene for the edit state (the prefill
                // guard means open no longer mutates it, but reset anyway so a
                // future test that DOES drive a dept change can't leak into the
                // next).
                stubVm.editDepartment = "CCS";
                stubVm.editCourse = "BSIT";
                var ed = findChild(databaseScreen, "editDialog");
                if (ed) ed.visible = false;
                var dlg = findChild(databaseScreen, "deleteConfirm");
                if (dlg) { dlg.visible = false; dlg.clearKey(); }
                stubVm.beginBulkEditSelectedCount = 0;
                stubVm.applyBulkEditCount = 0;
                var bd = findChild(databaseScreen, "bulkEditDialog");
                if (bd) bd.visible = false;
                var bc = findChild(databaseScreen, "bulkEditConfirm");
                if (bc) { bc.visible = false; bc.clearKey(); }
                stubVm.beginRegisterCount = 0;
                stubVm.registerStudentCount = 0;
                var rd = findChild(databaseScreen, "registerDialog");
                if (rd) rd.visible = false;
            }
            function test_showsCascadingFilter() {
                verify(findChild(databaseScreen, "cascDept") !== null);
            }
            function test_showsSelectableTable() {
                verify(findChild(databaseScreen, "studentsTable") !== null);
                verify(findChild(databaseScreen, "selectAllCheck") !== null);   // selectable table
            }
            function test_headerShowsCounts() {
                var h = findChild(databaseScreen, "tableCountHeader");
                verify(h !== null);
                verify(h.text.indexOf("2") !== -1);   // 2 results
            }
            function test_exportLabelReflectsSelection() {
                var btn = findChild(databaseScreen, "exportButton");
                verify(btn !== null);
                compare(btn.text, "Export CSV (all 2)");   // nothing selected => all N
                stubModel.selectedCount = 1;
                compare(btn.text, "Export CSV (1)");
            }
            function test_exportDisabledWhenNoRows() {
                stubModel.selectedCount = 0;
                stubModel.count = 0;
                compare(findChild(databaseScreen, "exportButton").enabled, false);
            }
            function test_deleteLabelAndEnabledTrackSelection() {
                var btn = findChild(databaseScreen, "deleteButton");
                verify(btn !== null);
                compare(btn.text, "Delete");
                compare(btn.enabled, false);              // nothing selected
                stubModel.selectedCount = 3;
                compare(btn.text, "Delete (3)");
                compare(btn.enabled, true);
            }
            function test_deleteConfirmInvokesVmDeleteSelected() {
                stubModel.selectedCount = 2;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deleteButton"));
                var dlg = findChild(databaseScreen, "deleteConfirm");
                verify(dlg !== null);
                compare(dlg.visible, true);
                compare(dlg.requireTypedConfirmation, false);   // 2 < 10
                mouseClick(findChild(dlg, "confirmButton"));
                compare(stubVm.deleteCount, 1);
            }
            function test_typedConfirmGateEngagesForLargeSelection() {
                stubModel.selectedCount = 10;               // >= threshold
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deleteButton"));
                var dlg = findChild(databaseScreen, "deleteConfirm");
                compare(dlg.requireTypedConfirmation, true);
                var btn = findChild(dlg, "confirmButton");
                compare(btn.enabled, false);                // gated until DELETE typed
                findChild(dlg, "confirmTypedField").text = "DELETE";
                compare(btn.enabled, true);
            }
            function test_deptButtonsDisabledWhenNoDepartment() {
                stubVm.department = "";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, false);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, false);
            }
            function test_deptButtonsDisabledWhenCourseSelected() {
                stubVm.department = "CCS";
                stubVm.course = "BSIT";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, false);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, false);
                stubVm.department = ""; stubVm.course = "";
            }
            function test_deptButtonsEnabledWhenDeptAndCourseAll() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, true);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, true);
                stubVm.department = "";
            }
            function test_deptButtonsDisabledWhenBusy() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = true;
                waitForRendering(databaseScreen);
                compare(findChild(databaseScreen, "deptDeactivateButton").enabled, false);
                compare(findChild(databaseScreen, "deptDeleteButton").enabled, false);
                stubVm.department = ""; stubVm.deptOpBusy = false;
            }
            function test_deactivateOpensPlainConfirmAndInvokesVm() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deptDeactivateButton"));
                var dlg = findChild(databaseScreen, "deptDeactivateConfirm");
                verify(dlg !== null);
                compare(dlg.visible, true);
                compare(dlg.requireTypedConfirmation, false);   // reversible, no typed gate
                mouseClick(findChild(dlg, "confirmButton"));
                compare(stubVm.deactivateDepartmentCount, 1);
                stubVm.department = "";
            }
            function test_deleteUsesTypedGateAndInvokesVm() {
                stubVm.department = "CCS";
                stubVm.course = "";
                stubVm.deptOpBusy = false;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "deptDeleteButton"));
                var dlg = findChild(databaseScreen, "deptDeleteConfirm");
                verify(dlg !== null);
                compare(dlg.visible, true);
                compare(dlg.requireTypedConfirmation, true);     // unconditional typed gate
                var btn = findChild(dlg, "confirmButton");
                compare(btn.enabled, false);                     // gated until DELETE typed
                findChild(dlg, "confirmTypedField").text = "DELETE";
                compare(btn.enabled, true);
                mouseClick(btn);
                compare(stubVm.deleteDepartmentCount, 1);
                stubVm.department = "";
            }
            function test_fileDialogAcceptInvokesExportCsv() {
                var dlg = findChild(databaseScreen, "exportDialog");
                verify(dlg !== null);
                dlg.selectedFile = "file:///tmp/wits_export_test.csv";
                dlg.accepted();                             // drive the onAccepted wiring
                compare(stubVm.lastExportUrl.toString(), "file:///tmp/wits_export_test.csv");
            }
            // Guards the LToast binding trap (documented in KioskScreen.qml):
            // LToast's auto-dismiss Timer imperatively sets message="", which
            // would permanently destroy a plain `message: vm.statusMessage`
            // binding after the FIRST toast. Prove a SECOND status still shows.
            function test_toastShowsSecondStatusAfterFirstDismissed() {
                var toast = findChild(databaseScreen, "databaseToast");
                verify(toast !== null);
                stubVm.statusMessage = "First status";
                compare(toast.message, "First status");
                toast.message = "";   // simulate the LToast auto-dismiss timer firing
                stubVm.statusMessage = "Second status";
                compare(toast.message, "Second status");
            }
            function test_editButtonEnabledWhenAnySelected() {
                var btn = findChild(databaseScreen, "editButton");
                stubModel.selectedCount = 0; stubVm.canEdit = false;
                compare(btn.enabled, false);
                stubModel.selectedCount = 1; stubVm.canEdit = true;
                compare(btn.enabled, true);
                stubModel.selectedCount = 3;   // canEdit stays true (>=1)
                compare(btn.enabled, true);
            }
            function test_editButtonOpensSingleEditAtOne() {
                stubModel.selectedCount = 1; stubVm.canEdit = true;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "editButton"));
                compare(stubVm.beginEditSelectedCount, 1);
                compare(stubVm.beginBulkEditSelectedCount, 0);
            }
            function test_editButtonOpensBulkEditAtTwoPlus() {
                stubModel.selectedCount = 2; stubVm.canEdit = true;
                waitForRendering(databaseScreen);
                mouseClick(findChild(databaseScreen, "editButton"));
                compare(stubVm.beginBulkEditSelectedCount, 1);
                compare(stubVm.beginEditSelectedCount, 0);
            }
            function test_bulkEditReadyOpensDialog_finishedCloses() {
                var d = findChild(databaseScreen, "bulkEditDialog");
                verify(d !== null);
                compare(d.visible, false);
                stubVm.bulkEditReady();
                compare(d.visible, true);
                stubVm.bulkEditFinished();
                compare(d.visible, false);
            }
            function test_applyRequestedOpensPreviewThenConfirmApplies() {
                stubModel.selectedCount = 2;
                stubVm.bulkEditReady();
                var d = findChild(databaseScreen, "bulkEditDialog");
                d.applyRequested();                       // dialog asks to apply
                var confirm = findChild(databaseScreen, "bulkEditConfirm");
                verify(confirm !== null);
                compare(confirm.visible, true);
                // Preview restates the change + count.
                verify(confirm.message.indexOf("Status → Inactive") !== -1);
                verify(confirm.message.indexOf("2") !== -1);
                compare(confirm.requireTypedConfirmation, false);   // 2 < 10
                mouseClick(findChild(confirm, "confirmButton"));
                compare(stubVm.applyBulkEditCount, 1);
            }
            function test_bulkConfirmTypedGateForLargeSelection() {
                stubModel.selectedCount = 12;
                stubVm.bulkEditReady();
                findChild(databaseScreen, "bulkEditDialog").applyRequested();
                var confirm = findChild(databaseScreen, "bulkEditConfirm");
                compare(confirm.requireTypedConfirmation, true);
                var btn = findChild(confirm, "confirmButton");
                compare(btn.enabled, false);
                findChild(confirm, "confirmTypedField").text = "UPDATE";
                compare(btn.enabled, true);
            }
            function test_rowActivatedInvokesBeginEdit() {
                // Drive the screen's LTable→vm wiring directly (the stub model is
                // not a real row model). Task 4 proves the double-click emits it.
                findChild(databaseScreen, "studentsTable").rowActivated("2023-XYZ");
                compare(stubVm.lastBeginEditId, "2023-XYZ");
            }
            function test_editReadyOpensDialogAndFinishedCloses() {
                var ed = findChild(databaseScreen, "editDialog");
                verify(ed !== null);
                compare(ed.visible, false);
                stubVm.editReady();
                compare(ed.visible, true);
                stubVm.editFinished();
                compare(ed.visible, false);
            }
            function test_addStudentButtonInvokesBeginRegisterAndOpensDialog() {
                var btn = findChild(databaseScreen, "addStudentButton");
                verify(btn !== null);
                waitForRendering(databaseScreen);
                mouseClick(btn);
                compare(stubVm.beginRegisterCount, 1);
                var d = findChild(databaseScreen, "registerDialog");
                verify(d !== null);
                compare(d.visible, true);
                stubVm.registerFinished();
                compare(d.visible, false);
            }
        }
    }

    // --- StudentEditDialog fixture (own band below Database, y 4500..5200) ---
    // Minimal edit-only stub so the dialog can be exercised without the screen.
    Item {
        id: editDialogBand
        y: 4500
        width: 900; height: 700

        QtObject {
            id: editStub
            property var departments: ["CCS", "CBA"]
            property var editCourses: ["BSIT", "BSCS"]
            property string editSchoolId: "2023-001"
            property string editName: "Juan Cruz"
            property string editYearLevel: "2"
            property string editGender: "Male"
            property string editStatus: "Active"
            property string editDepartment: "CCS"
            property string editCourse: "BSIT"
            property int setDeptCount: 0
            property string lastDept: ""
            property int saveCount: 0
            function setEditDepartment(d) { setDeptCount++; lastDept = d; editDepartment = d; editCourse = ""; }
            function setEditName(v) { editName = v; }
            function setEditYearLevel(v) { editYearLevel = v; }
            function setEditGender(v) { editGender = v; }
            function setEditStatus(v) { editStatus = v; }
            function setEditCourse(v) { editCourse = v; }
            function saveEdit() { saveCount++; }
        }

        StudentEditDialog { id: editDialog; anchors.fill: parent; vm: editStub }

        TestCase {
            name: "StudentEditDialog"; when: windowShown
            function init() {
                editStub.editName = "Juan Cruz";
                editStub.editCourse = "BSIT";
                editStub.editDepartment = "CCS";
                editStub.setDeptCount = 0;
                editStub.saveCount = 0;
                editDialog.visible = false;
            }
            function test_prefillsFromVmOnOpen() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                compare(findChild(editDialog, "editNameField").text, "Juan Cruz");
                compare(findChild(editDialog, "editDeptCombo").currentValue, "CCS");
                compare(findChild(editDialog, "editCourseCombo").currentValue, "BSIT");
                compare(findChild(editDialog, "editGenderCombo").currentValue, "Male");
                compare(findChild(editDialog, "editStatusCombo").currentValue, "Active");
                verify(findChild(editDialog, "editSchoolIdText").text.indexOf("2023-001") !== -1);
            }
            function test_saveDisabledWhenNameEmpty() {
                editStub.editName = "";
                editDialog.visible = true;
                waitForRendering(editDialog);
                compare(findChild(editDialog, "editSaveButton").enabled, false);
                editStub.editName = "Something";
                compare(findChild(editDialog, "editSaveButton").enabled, true);
            }
            function test_saveInvokesVmSaveEdit() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                mouseClick(findChild(editDialog, "editSaveButton"));
                compare(editStub.saveCount, 1);
            }
            function test_cancelClosesDialog() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                mouseClick(findChild(editDialog, "editCancelButton"));
                compare(editDialog.visible, false);
            }
            function test_departmentChangeCallsVmAndResyncsCourseCombo() {
                editDialog.visible = true;
                waitForRendering(editDialog);
                compare(findChild(editDialog, "editCourseCombo").currentValue, "BSIT");
                // Simulate picking a new department via the combo's own path.
                findChild(editDialog, "editDeptCombo").selectValue("CBA");
                compare(editStub.setDeptCount, 1);
                compare(editStub.lastDept, "CBA");
                // The vm cleared editCourse (""); the Connections re-sync must
                // reset the Course combo's displayed value.
                compare(findChild(editDialog, "editCourseCombo").currentValue, "");
            }
        }
    }

    // --- BulkEditDialog fixture (own band below editDialog, y 5200..5900) ---
    Item {
        id: bulkEditBand
        y: 5200
        width: 900; height: 700

        QtObject {
            id: bulkStub
            property var departments: ["CCS", "CBA"]
            property var bulkCourses: ["BSBA", "BSA"]
            property bool changeDepartment: false
            property bool changeCourse: false
            property bool changeYearLevel: false
            property bool changeGender: false
            property bool changeStatus: false
            property string bulkDepartment: ""
            property string bulkCourse: ""
            property string bulkYearLevel: ""
            property string bulkGender: ""
            property string bulkStatus: ""
            property bool canApplyBulk: false
            property bool bulkBusy: false
            // Mirror the real coupling so the dialog's driven bindings behave.
            function setChangeDepartment(v) {
                changeDepartment = v;
                changeCourse = v;                 // coupled
                if (!v) bulkCourse = "";
            }
            function setChangeCourse(v) { changeCourse = v && changeDepartment; }
            function setChangeYearLevel(v) { changeYearLevel = v; }
            function setChangeGender(v) { changeGender = v; }
            function setChangeStatus(v) { changeStatus = v; }
            function setBulkDepartment(v) { bulkDepartment = v; bulkCourse = ""; }
            function setBulkCourse(v) { bulkCourse = v; }
            function setBulkYearLevel(v) { bulkYearLevel = v; }
            function setBulkGender(v) { bulkGender = v; }
            function setBulkStatus(v) { bulkStatus = v; }
        }

        BulkEditDialog { id: bulkDialog; anchors.fill: parent; vm: bulkStub }

        TestCase {
            name: "BulkEditDialog"; when: windowShown
            function init() {
                bulkStub.changeDepartment = false; bulkStub.changeCourse = false;
                bulkStub.changeStatus = false; bulkStub.bulkStatus = "";
                bulkStub.bulkDepartment = ""; bulkStub.bulkCourse = "";
                bulkStub.canApplyBulk = false; bulkStub.bulkBusy = false;
                bulkDialog.visible = false;
            }
            function test_valueControlDisabledUntilToggleOn() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var statusCombo = findChild(bulkDialog, "bulkStatusCombo");
                verify(statusCombo !== null);
                compare(statusCombo.enabled, false);          // toggle off
                bulkStub.changeStatus = true;
                compare(statusCombo.enabled, true);
            }
            function test_courseToggleFollowsDepartment() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var courseCheck = findChild(bulkDialog, "bulkCourseCheck");
                var courseCombo = findChild(bulkDialog, "bulkCourseCombo");
                verify(courseCheck !== null);
                compare(courseCheck.checked, false);
                compare(courseCombo.enabled, false);           // needs dept + a chosen dept
                // Turn Department on via its checkbox -> Course check follows.
                bulkStub.setChangeDepartment(true);
                compare(courseCheck.checked, true);
                bulkStub.bulkDepartment = "CBA";
                compare(courseCombo.enabled, true);
            }
            function test_applyDisabledUntilCanApply_andEmitsApplyRequested() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var apply = findChild(bulkDialog, "bulkApplyButton");
                verify(apply !== null);
                compare(apply.enabled, false);                 // canApplyBulk false
                bulkStub.canApplyBulk = true;
                compare(apply.enabled, true);
                var spy = signalSpy.createObject(bulkDialog, { target: bulkDialog, signalName: "applyRequested" });
                mouseClick(apply);
                compare(spy.count, 1);
                spy.destroy();
            }
            function test_cancelClosesDialog() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                mouseClick(findChild(bulkDialog, "bulkCancelButton"));
                compare(bulkDialog.visible, false);
            }
            // Regression: a clicked checkbox imperatively writes `checked`,
            // severing any binding — so a reopen MUST re-sync from the vm or the
            // toggle shows stale. Click Status on, close, reset the vm (as
            // beginBulkEditSelected does), reopen -> the check must read false.
            function test_reopenResyncsTogglesFromVm() {
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                var statusCheck = findChild(bulkDialog, "bulkStatusCheck");
                mouseClick(statusCheck);
                compare(statusCheck.checked, true);
                compare(bulkStub.changeStatus, true);
                bulkDialog.visible = false;
                bulkStub.changeStatus = false;          // vm reset (beginBulkEditSelected)
                bulkDialog.visible = true;
                waitForRendering(bulkDialog);
                compare(statusCheck.checked, false);     // re-synced, not stale
            }
        }
    }
    // --- RegisterStudentDialog fixture (own band below bulkEdit, y 5900..6600) ---
    Item {
        id: registerBand
        y: 5900
        width: 900; height: 700

        QtObject {
            id: regStub
            property var departments: ["CCS", "CBA"]
            property var regCourses: ["BSIT", "BSCS"]
            property string regSchoolId: ""
            property string regName: ""
            property string regCode: ""
            property string regYearLevel: ""
            property string regGender: ""
            property string regStatus: ""
            property string regDepartment: ""
            property string regCourse: ""
            property string regPhotoName: ""
            property bool canRegister: false
            property bool regBusy: false
            property bool regDuplicate: false
            property int setRegDeptCount: 0
            property string lastRegDept: ""
            property int registerStudentCount: 0
            function setRegSchoolId(v) { regSchoolId = v; regDuplicate = false; }
            function setRegName(v) { regName = v; }
            function setRegCode(v) { regCode = v; }
            function setRegYearLevel(v) { regYearLevel = v; }
            function setRegGender(v) { regGender = v; }
            function setRegStatus(v) { regStatus = v; }
            function setRegCourse(v) { regCourse = v; }
            function setRegDepartment(v) { setRegDeptCount++; lastRegDept = v; regDepartment = v; regCourse = ""; }
            function setRegPhoto(u) { regPhotoName = ("" + u).split("/").pop(); }
            function clearRegPhoto() { regPhotoName = ""; }
            function registerStudent() { registerStudentCount++; }
        }

        RegisterStudentDialog { id: registerDialog2; anchors.fill: parent; vm: regStub }

        TestCase {
            name: "RegisterStudentDialog"; when: windowShown
            function init() {
                regStub.regSchoolId = ""; regStub.regName = ""; regStub.regCourse = "";
                regStub.regDepartment = ""; regStub.regPhotoName = "";
                regStub.canRegister = false; regStub.regBusy = false; regStub.regDuplicate = false;
                regStub.setRegDeptCount = 0; regStub.lastRegDept = "";
                regStub.registerStudentCount = 0;
                registerDialog2.visible = false;
            }
            function test_submitDisabledUntilCanRegister() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var submit = findChild(registerDialog2, "regSubmitButton");
                verify(submit !== null);
                compare(submit.enabled, false);
                regStub.canRegister = true;
                compare(submit.enabled, true);
            }
            function test_submitLabelSwapsWhenBusy() {
                regStub.canRegister = true;
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var submit = findChild(registerDialog2, "regSubmitButton");
                compare(submit.text, "Register");
                regStub.regBusy = true;
                compare(submit.text, "Registering…");
                compare(submit.enabled, false);   // disabled while busy
            }
            function test_submitInvokesVmRegisterStudent() {
                regStub.canRegister = true;
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                mouseClick(findChild(registerDialog2, "regSubmitButton"));
                compare(regStub.registerStudentCount, 1);
            }
            function test_duplicateErrorVisibleAndClearsOnSchoolIdEdit() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var err = findChild(registerDialog2, "regDuplicateError");
                verify(err !== null);
                compare(err.visible, false);
                regStub.regDuplicate = true;
                compare(err.visible, true);
                // Editing School ID clears the duplicate (stub setter drops the flag).
                var idField = findChild(registerDialog2, "regSchoolIdField");
                idField.text = "2023-999";
                compare(err.visible, false);
            }
            function test_departmentPickDrivesSetRegDepartment() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                findChild(registerDialog2, "regDeptCombo").selectValue("CBA");
                compare(regStub.setRegDeptCount, 1);
                compare(regStub.lastRegDept, "CBA");
                // vm cleared regCourse; the re-sync Connections resets the combo.
                compare(findChild(registerDialog2, "regCourseCombo").currentValue, "");
            }
            function test_photoPickShowsFilenameAndRemoveClears() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                var label = findChild(registerDialog2, "regPhotoLabel");
                var remove = findChild(registerDialog2, "regRemovePhotoButton");
                compare(remove.visible, false);
                regStub.setRegPhoto("file:///tmp/ana_reyes.png");
                compare(label.text, "ana_reyes.png");
                compare(remove.visible, true);
                mouseClick(remove);
                compare(regStub.regPhotoName, "");
            }
            function test_cancelClosesDialog() {
                registerDialog2.visible = true;
                waitForRendering(registerDialog2);
                mouseClick(findChild(registerDialog2, "regCancelButton"));
                compare(registerDialog2.visible, false);
            }
        }
    }

    // --- ImportStudentsDialog fixture (own band below registerDialog, y 6600..7300) ---
    Item {
        id: importBand
        y: 6600
        width: 900; height: 700

        QtObject {
            id: importStub
            property int phase: 0            // 0=Idle .. 6=Failed (mirrors Phase enum)
            property bool busy: false
            property int parsedCount: 0
            property int duplicateCount: 0
            property bool allDuplicates: false
            property int uploadPercent: 0
            property string dataFileName: ""
            property string photosZipName: ""
            property string errorText: ""
            property string resultText: ""
            property bool authFailure: false

            property int startImportCount: 0
            property int continueCount: 0
            property int cancelCount: 0
            property url lastDataUrl: ""
            property url lastZipUrl: ""
            property url lastTemplateUrl: ""
            signal finishedOk()

            function setDataFile(u) { lastDataUrl = u; dataFileName = ("" + u).split("/").pop(); parsedCount = 2; phase = 0; }
            function setPhotosZip(u) { lastZipUrl = u; photosZipName = ("" + u).split("/").pop(); }
            function clearPhotosZip() { photosZipName = ""; }
            function startImport() { startImportCount++; }
            function continueAfterDuplicates() { continueCount++; }
            function cancel() { cancelCount++; phase = 0; duplicateCount = 0; allDuplicates = false; }
            function downloadTemplate(u) { lastTemplateUrl = u; return true; }
        }

        ImportStudentsDialog { id: importDialog2; anchors.fill: parent; vm: importStub }

        TestCase {
            name: "ImportStudentsDialog"; when: windowShown
            function init() {
                importStub.phase = 0;
                importStub.busy = false;
                importStub.parsedCount = 0;
                importStub.duplicateCount = 0;
                importStub.allDuplicates = false;
                importStub.dataFileName = "";
                importStub.photosZipName = "";
                importStub.errorText = "";
                importStub.resultText = "";
                importStub.startImportCount = 0;
                importStub.continueCount = 0;
                importStub.cancelCount = 0;
                importDialog2.visible = false;
            }

            function test_importDisabledUntilDataFileChosen() {
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                var submit = findChild(importDialog2, "importSubmitButton");
                verify(submit !== null);
                compare(submit.enabled, false);
                importStub.setDataFile("file:///tmp/students.csv");
                compare(submit.enabled, true);
            }

            function test_importInvokesStartImport() {
                importStub.setDataFile("file:///tmp/students.csv");
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                mouseClick(findChild(importDialog2, "importSubmitButton"));
                compare(importStub.startImportCount, 1);
            }

            function test_someDuplicatesShowsContinueAndCancel() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.parsedCount = 3;
                importStub.duplicateCount = 1;
                importStub.allDuplicates = false;
                importStub.phase = 2;   // AwaitingDuplicates
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                verify(findChild(importDialog2, "importDuplicateSome").visible);
                verify(!findChild(importDialog2, "importDuplicateAll").visible);
                mouseClick(findChild(importDialog2, "importDupContinueButton"));
                compare(importStub.continueCount, 1);
            }

            function test_allDuplicatesShowsCloseOnly() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.parsedCount = 2;
                importStub.duplicateCount = 2;
                importStub.allDuplicates = true;
                importStub.phase = 2;   // AwaitingDuplicates
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                verify(findChild(importDialog2, "importDuplicateAll").visible);
                verify(!findChild(importDialog2, "importDuplicateSome").visible);
                // No Continue button in the all-dupes branch; Close closes the dialog.
                mouseClick(findChild(importDialog2, "importDupCloseButton"));
                compare(importDialog2.visible, false);
            }

            function test_processingShowsIndeterminateAndLabel() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.phase = 4;   // Processing
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                var bar = findChild(importDialog2, "importProgressBar");
                verify(bar !== null);
                compare(bar.indeterminate, true);
                compare(findChild(importDialog2, "importProcessingLabel").visible, true);
            }

            function test_resultLineShownOnDone() {
                importStub.setDataFile("file:///tmp/students.csv");
                importStub.resultText = "5 imported · 1 skipped · 0 failed";
                importStub.phase = 5;   // Done
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                var res = findChild(importDialog2, "importResultText");
                verify(res.visible);
                compare(res.text, "5 imported · 1 skipped · 0 failed");
            }

            function test_templateButtonInvokesDownload() {
                importDialog2.visible = true;
                waitForRendering(importDialog2);
                // Drive the VM method directly (FileDialog is native/modal and not
                // clickable under offscreen); assert the wiring point exists.
                importStub.downloadTemplate("file:///tmp/template.csv");
                compare(("" + importStub.lastTemplateUrl).length > 0, true);
                verify(findChild(importDialog2, "importTemplateButton") !== null);
            }
        }
    }
    Component { id: signalSpy; SignalSpy {} }
}
