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
            // test_pageInAnimatesContentColumnFadeAndRise drives this down to
            // 0 and back up; restore the resting state so every other test
            // sees a fully-opaque, unmoved content column regardless of test
            // order (test functions run alphabetically, not declaration
            // order).
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
        // (tst_qml_components.qml). The migration means there is no more
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
        // The job is split into two parts that are collectively STRICTER:
        //   (a) the token arithmetic, asserted directly on
        //       Theme.motion.staggerDelay — a pure function on the singleton
        //       and the actual named mutation target. Zero clock, so it is
        //       load-immune, and it pins the clamp and the negative-index
        //       guard that the timing sample never reached at all.
        //   (b) the wiring proof: that row 5 really LAGS row 0 on screen.
        //       Instead of betting on one instant, poll and assert the
        //       invariant that holds under ANY load — monotonic lead,
        //       row0.opacity >= row5.opacity at EVERY sample (both fades are
        //       the same duration and easing, row 5 merely starts later, so
        //       row 5 can never be ahead) — while recording whether a STRICT
        //       lead was ever seen. A strict lead is REQUIRED to pass: that
        //       is what separates real stagger from all rows animating in
        //       lockstep. A single scheduler stall long enough to swallow
        //       the entire 525ms window would leave both rows settled with
        //       no strict lead observed, so the observation is wrapped in a
        //       bounded retry that re-triggers the entrance via the same
        //       model reset; only "no attempt ever saw a strict lead" fails.
        function test_staggerDelayReadsThemeRowStaggerToken() {
            // (a) Token arithmetic — fully deterministic, no clock involved.
            var step = Theme.motion.rowStagger;
            compare(Theme.motion.staggerDelay(0, step), 0);
            compare(Theme.motion.staggerDelay(5, step), 5 * step);
            // Above the cap the delay stops growing, so a long list's
            // entrance can't take seconds.
            compare(Theme.motion.staggerDelay(Theme.motion.staggerCap + 7, step),
                    Theme.motion.staggerCap * step);
            // A delegate's index transiently goes to -1 during model-reset
            // teardown; the max(0, ...) guard must keep that out of a
            // PauseAnimation duration.
            compare(Theme.motion.staggerDelay(-1, step), 0);

            // (b) Wiring: row 5's entrance really trails row 0's.
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
            }
            verify(sawStrictLead,
                   "row 5 never trailed row 0 in 3 attempts — stagger not wired");
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
            // test_pageInAnimatesContentColumnFadeAndRise drives this down to
            // 0 and back up; restore the resting state so every other test
            // sees a fully-opaque, unmoved content column regardless of test
            // order (alphabetical, not declaration).
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
}
