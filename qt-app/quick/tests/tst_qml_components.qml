import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    // Extra height accommodates the three LBarChart motion fixtures (their
    // own non-overlapping vertical bands, see the comment at `bc`) and the
    // LTable row-motion fixture `tAnimated` (Phase 3 Task C), which gets its
    // own band below all of them for the same reason: interactive fixtures
    // sharing a position/z with a later-declared sibling silently absorb
    // synthetic mouse events meant for the earlier one.
    width: 400; height: 1100

    LButton    { id: b;  text: "OK" }
    LCard      { id: c }
    ListModel {
        id: tableFixture
        ListElement { date: "2026-07-13"; name: "Maria Santos"; timeIn: "08:14" }
        ListElement { date: "2026-07-13"; name: "Jose Ramirez"; timeIn: "08:31" }
    }
    LTable {
        id: t
        width: 480; height: 240
        columns: [
            { key: "date",   title: "Date" },
            { key: "name",   title: "Name" },
            { key: "timeIn", title: "Time In" }
        ]
        model: tableFixture
    }

    // --- Motion (Phase 3 Task C) fixtures: row entrance + hover ---
    // 10 rows (indices 0-9) so the stagger-differential test has enough
    // range below Theme.motion.staggerCap (10) to compare a zero-stagger row
    // against a clearly-staggered one. `t` above stays untouched (never sets
    // animateRows) and IS the default-false regression fixture.
    ListModel {
        id: visitRowsAnimatedFixtureA
        ListElement { date: "2026-07-13"; name: "Row 0"; timeIn: "08:00" }
        ListElement { date: "2026-07-13"; name: "Row 1"; timeIn: "08:01" }
        ListElement { date: "2026-07-13"; name: "Row 2"; timeIn: "08:02" }
        ListElement { date: "2026-07-13"; name: "Row 3"; timeIn: "08:03" }
        ListElement { date: "2026-07-13"; name: "Row 4"; timeIn: "08:04" }
        ListElement { date: "2026-07-13"; name: "Row 5"; timeIn: "08:05" }
        ListElement { date: "2026-07-13"; name: "Row 6"; timeIn: "08:06" }
        ListElement { date: "2026-07-13"; name: "Row 7"; timeIn: "08:07" }
        ListElement { date: "2026-07-13"; name: "Row 8"; timeIn: "08:08" }
        ListElement { date: "2026-07-13"; name: "Row 9"; timeIn: "08:09" }
    }
    // A distinct model object (not the same instance re-cleared) so
    // reassigning LTable.model is a genuine "model changed" event for the
    // ListView, the same event QQuickListView treats identically to a
    // QAbstractListModel begin/endResetModel() reset (VisitLogRowsModel's
    // actual refresh mechanism) — both re-fire `populate`.
    ListModel {
        id: visitRowsAnimatedFixtureB
        ListElement { date: "2026-07-14"; name: "Fresh Row"; timeIn: "09:00" }
    }
    LTable {
        id: tAnimated
        objectName: "tAnimated"
        y: 620
        // Tall enough that all 10 rows are simultaneously visible (header 36
        // + 10 * 40px rows) so the initial `populate` batch actually creates
        // every delegate the stagger-differential test below needs — a
        // shorter ListView would only realize the rows that fit the
        // viewport, and index 8 would never exist to observe.
        width: 480; height: 460
        animateRows: true
        columns: [
            { key: "date",   title: "Date" },
            { key: "name",   title: "Name" },
            { key: "timeIn", title: "Time In" }
        ]
        model: visitRowsAnimatedFixtureA
    }
    LSegmented { id: sg }
    LSideNav {
        id: sn
        width: 240; height: 400
        currentPage: "dashboard"
        items: [
            { page: "dashboard", label: "Dashboard", enabled: true },
            { page: "search",    label: "Search",    enabled: true },
            { page: "database",  label: "Database",  enabled: false }
        ]
    }
    // Header-slot fixture: a marker Item assigned to LSideNav.header, same
    // reparent-and-render mechanism the footer slot already uses (compare
    // AdminScreen.qml's `footer: LButton { ... }`). Proves the slot
    // mechanism itself, independent of what AdminScreen puts in it.
    LSideNav {
        id: snWithHeader
        width: 240; height: 400
        header: Rectangle { id: headerMarker; objectName: "headerMarker"; implicitWidth: 10; implicitHeight: 20 }
        items: [ { page: "dashboard", label: "Dashboard", enabled: true } ]
    }
    LToast     { id: to; message: "hi" }
    LDialog    { id: dg; title: "T" }
    LTextField { id: tf; label: "School Name"; text: "Acme" }
    LTextField { id: pf; label: "Admin Key"; isPassword: true; text: "secret" }
    LConfirmDialog { id: cd1; tier: 1; title: "Confirm"; message: "Proceed?" }
    LConfirmDialog { id: cd2; tier: 2; title: "Reset Visits"; message: "Deletes history."; confirmText: "Reset" }
    LComboBox { id: cb; model: ["CE", "IT", "BA"]; placeholder: "Select Department" }
    LStatTile  { id: st; label: "L"; value: "1" }
    ListModel {
        id: barsFixture
        ListElement { label: "8"; value: 12 }
        ListElement { label: "9"; value: 34 }
        ListElement { label: "10"; value: 41 }
    }
    LBarChart {
        id: bc
        // Elevated above the fixtures declared after it (LPageHeader,
        // LPulseDot, LEyebrow, the gradient Rectangle, LSidebarBrand) so the
        // hover test's synthetic mouseMove hit-tests this chart's bars
        // rather than whatever unrelated sibling happens to paint on top by
        // declaration order.
        z: 1000
        width: 320; height: 160
        model: barsFixture
        maxValue: 41
        highlightIndex: 2
    }

    // --- Motion (Phase 3 Task B) fixtures: entrance + refresh-reset guard ---
    // Each of the three chart fixtures gets its OWN vertical band (bc at
    // y:0, then y:200, y:400) rather than relying on z alone to
    // disambiguate: all three default to the same (0,0) position and same
    // z, and a synthetic mouseMove targeting `bc`'s bars was silently
    // hit-testing whichever LATER-declared chart happened to be stacked on
    // top instead (found via host.childAt() during hover-test debugging) —
    // z only breaks ties against fixtures declared BEFORE it, not ones
    // declared after with the same z.
    ListModel {
        id: barsAnimatedFixture
        property real maxValue: 41
        ListElement { label: "8"; value: 12 }
        ListElement { label: "9"; value: 34 }
        ListElement { label: "10"; value: 41 }
    }
    LBarChart {
        id: bcAnimated
        objectName: "bcAnimated"
        y: 200
        z: 1000
        width: 320; height: 160
        orientation: "Vertical"
        animated: true
        model: barsAnimatedFixture
        maxValue: 41
    }
    // Department (Horizontal) fixture: glide-on-value-change only, no
    // entrance of its own (brief B3).
    ListModel {
        id: deptFillFixture
        property real maxValue: 200
        ListElement { label: "CE"; value: 100 }
    }
    LBarChart {
        id: bcDeptFill
        objectName: "bcDeptFill"
        y: 400
        z: 1000
        width: 320; height: 160
        orientation: "Horizontal"
        model: deptFillFixture
        maxValue: 200
    }
    LPageHeader {
        id: ph
        width: 480
        title: "Dashboard"
        subtitle: "Library activity overview"
        dateText: "Monday, July 6, 2026"
        clockText: "8:04:11 AM"
    }
    LPulseDot  { id: pd; color: Theme.secondary; pulseDuration: 900 }
    LEyebrow   { id: eb; text: "EYEBROW"; color: Theme.secondary }
    Rectangle  { id: gr; width: 10; height: 10; gradient: LKioskGradient {} }

    // LSidebarBrand fixtures: no-logo (placeholder path) and a synthetic
    // data-URI PNG (1x1 transparent pixel — no real school asset, no
    // filesystem/network access) exercising the loaded-logo path.
    LSidebarBrand {
        id: brandNoLogo
        schoolName: "Example Community Library"
        hasLogo: false
    }
    LSidebarBrand {
        id: brandWithLogo
        schoolName: "Example Community Library"
        hasLogo: true
        logoUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
    }

    TestCase {
        name: "ComponentsInstantiateAndBindTheme"
        function test_allCreated() {
            verify(b !== null); verify(c !== null); verify(t !== null);
            verify(sg !== null); verify(sn !== null); verify(to !== null);
            verify(dg !== null); verify(st !== null); verify(bc !== null);
            verify(ph !== null); verify(brandNoLogo !== null); verify(brandWithLogo !== null);
        }
        function test_buttonBindsBrandToken() {
            // LButton Primary fill binds Theme.brand.admin — not a local literal.
            compare(b.fillColor, Theme.brand.admin);
        }
        function test_cardBindsCardToken() {
            compare(c.color, Theme.card);
        }
        function test_pulseDotCreatedWithColorAndDuration() {
            verify(pd !== null);
            compare(pd.color, Theme.secondary);
            compare(pd.pulseDuration, 900);
        }
        function test_eyebrowDefaultsAndOverrides() {
            verify(eb !== null);
            compare(eb.text, "EYEBROW");
            compare(eb.color, Theme.secondary);
            // QFont stores letterSpacing in 26.6 fixed-point (1/64px), so a
            // literal 1.4 round-trips as 1.390625 regardless of call site —
            // fuzzyCompare tolerates that quirk without weakening the check.
            fuzzyCompare(eb.font.letterSpacing, 1.4, 0.02);
            compare(eb.font.weight, Font.ExtraBold);
            compare(eb.font.pixelSize, Theme.typography.eyebrow);
        }
        function test_kioskGradientStopsMatchBrandTokens() {
            verify(gr.gradient !== null);
            compare(gr.gradient.stops.length, 2);
            compare(gr.gradient.stops[0].color, Theme.brand.kiosk);
            compare(gr.gradient.stops[1].color, Theme.brand.kioskHover);
        }
    }

    TestCase {
        name: "LSideNavInteraction"
        when: windowShown

        function test_activeItemMatchesCurrentPage() {
            compare(sn.currentPage, "dashboard");
        }
        function test_emitsPageActivatedForEnabledItem() {
            var got = null;
            sn.pageActivated.connect(function(p) { got = p; });
            sn.activate("search");            // test hook: same path a click takes
            compare(got, "search");
        }
        function test_disabledItemDoesNotActivate() {
            var count = 0;
            sn.pageActivated.connect(function(p) { count++; });
            sn.activate("database");          // disabled -> ignored
            compare(count, 0);
        }
        // A plain findChild() presence check is NOT falsifiable here: an
        // inline Item literal assigned to a plain `property Item header`
        // (rather than a default/list property) is never auto-parented by
        // QML, so findChild's QObject-tree search locates the marker either
        // way — verified empirically by nulling out LSideNav.qml's
        // `children: nav.header ? [nav.header] : []` binding and observing
        // findChild() still succeed. What DOES flip is the marker's actual
        // Item.parent: with the binding removed it stays null (never placed
        // in the visual tree at all); with the binding intact it becomes the
        // real wrapper Item inside the ColumnLayout. Asserting parent !==
        // null is therefore the assertion that can actually fail.
        function test_headerSlotReparentsIntoTree() {
            var marker = findChild(snWithHeader, "headerMarker");
            verify(marker !== null);
            verify(marker.parent !== null);
        }
    }

    TestCase {
        name: "LTableRendersRows"
        when: windowShown
        // test_showsEmptyStateWhenNoRows nulls out t.model; reset it before
        // every test (alphabetical order, not declaration) so that mutation
        // can't leak into the motion tests below.
        function init() {
            t.model = tableFixture;
        }
        // Forces a genuine model-changed event on tAnimated regardless of
        // whatever the previous test left it at (fixtureA already assigned
        // is a no-op reassignment QML would skip notifying). Unconditionally
        // assigning fixtureB first guarantees the FOLLOWING fixtureA
        // assignment is always a real value change (fixtureB !== fixtureA),
        // so this always re-fires `populate` for a fresh batch — a t=0 for
        // stagger timing regardless of what state the previous test left.
        function resetAnimatedRows() {
            tAnimated.model = visitRowsAnimatedFixtureB;
            tAnimated.model = visitRowsAnimatedFixtureA;
        }
        function test_rowCountMatchesModel() {
            compare(t.rowCount, 2);
        }
        function test_showsEmptyStateWhenNoRows() {
            t.model = null;
            compare(t.rowCount, 0);
            verify(t.emptyVisible === true);
        }

        // --- Motion (Phase 3 Task C): row entrance (C2) + hover (C3) ---

        // Regression guard for the opt-in default (brief C2 CRITICAL /
        // advisor §6) — the highest-value test in this file: `t` never sets
        // animateRows, so it must stay false, and BOTH ListView Transitions
        // must be disabled. This is a structural check, not a timing one:
        // opacity settles at 1 either way once any entrance (if one ran)
        // finishes, so only the Transition's own `enabled` flag can actually
        // prove the populate/add mechanism never engaged for a static
        // consumer like the existing tst_qml_components.qml fixtures or a
        // future Phase-4 dense grid.
        function test_defaultAnimateRowsFalseDisablesPopulateAndAddTransitions() {
            compare(t.animateRows, false);
            var list = findChild(t, "rowsList");
            verify(list !== null);
            compare(list.populate.enabled, false);
            compare(list.add.enabled, false);
            var row = findChild(t, "tableRow_0");
            verify(row !== null);
            compare(row.opacity, 1);
        }

        // The highest-value row-entrance test: proves populate genuinely
        // (re-)plays on a model reset for an animateRows:true consumer — the
        // whole reason C2 uses populate over SearchScreen's per-delegate
        // Component.onCompleted approach. Swapping to a distinct model
        // object is the QML-test equivalent of
        // VisitLogRowsModel::setRows()'s real begin/endResetModel() call —
        // both are a "model changed" event that re-fires populate for the
        // fresh item batch. Caught right after the very first rendered
        // frame: with populate broken/disabled the fresh row would already
        // sit at its natural opacity of 1 immediately, never having passed
        // through the deliberate `from: 0` start state at all.
        function test_animateRowsTrueRepopulatesFreshRowsOnModelReset() {
            tAnimated.model = visitRowsAnimatedFixtureB;
            waitForRendering(tAnimated);
            var freshRow = findChild(tAnimated, "tableRow_0");
            verify(freshRow !== null);
            verify(freshRow.opacity < 1);
            tryCompare(freshRow, "opacity", 1);
        }

        // Stagger differential (mutation target: Theme.motion.rowStagger /
        // staggerCap). Index 0 has zero PauseAnimation delay and resolves
        // after ~Theme.motion.rowIn (400ms); index 8 (below staggerCap of
        // 10) carries an extra 8 * Theme.motion.rowStagger (200ms at current
        // token values) of pause before its own fade even starts. Waiting
        // 450ms — comfortably past row 0's 400ms settle but well short of
        // row 8's 600ms (200 pause + 400 fade) — must find row 0 fully
        // resolved and row 8 still strictly mid-flight.
        function test_rowStaggerDelaysHigherIndexRows() {
            resetAnimatedRows();
            waitForRendering(tAnimated);
            var row0 = findChild(tAnimated, "tableRow_0");
            var row8 = findChild(tAnimated, "tableRow_8");
            verify(row0 !== null);
            verify(row8 !== null);
            wait(450);
            compare(row0.opacity, 1);
            verify(row8.opacity > 0 && row8.opacity < 1);
            tryCompare(row8, "opacity", 1);
        }

        // Row hover glide (C3). HoverHandler.hovered is not a QQuickItem and
        // is invisible to findChild(), so isHovered (exposed on the row) is
        // asserted instead. The Behavior means color must NOT already equal
        // the hover tint the instant hover starts (a plain binding without
        // a Behavior would jump synchronously); it must still read the
        // resting color at that exact instant, then reach the tint only
        // after tryCompare waits the ColorAnimation out.
        function test_rowHoverColorGlidesInAndOutViaBehavior() {
            var row = findChild(tAnimated, "tableRow_0");
            verify(row !== null);
            tryCompare(row, "opacity", 1); // let any pending entrance settle first
            var restColor = row.color;
            var hoverColor = Qt.alpha(Theme.brand.admin, 0.06);

            mouseMove(tAnimated, 2, 2); // neutral point, away from any row
            compare(row.isHovered, false);

            mouseMove(row, row.width / 2, row.height / 2);
            verify(row.isHovered);
            compare(row.color, restColor); // Behavior hasn't advanced a frame yet
            tryCompare(row, "color", hoverColor);

            mouseMove(tAnimated, 2, 2);
            tryCompare(row, "isHovered", false);
            tryCompare(row, "color", restColor);
        }
    }

    TestCase {
        name: "LBarChartRenders"
        when: windowShown
        // TestCase runs test_ functions in alphabetical order (not declaration
        // order), so test_horizontalOrientationAccepted (h) executes before
        // test_orientationDefaultsVertical (o) and mutates the shared bc
        // instance's orientation. Reset it before every test in this case.
        function init() {
            bc.orientation = "Vertical";
        }
        function test_barCountMatchesModel() {
            compare(bc.barCount, 3);
        }
        function test_orientationDefaultsVertical() {
            compare(bc.orientation, "Vertical");
        }
        function test_horizontalOrientationAccepted() {
            bc.orientation = "Horizontal";
            compare(bc.orientation, "Horizontal");
            compare(bc.barCount, 3);
        }

        // --- Motion (Phase 3 Task B) ---

        // Regression guard for the opt-in default (brief B2 / advisor §6):
        // `bc` never sets `animated`, so it must default to false and its
        // bars must be at full scale immediately — proving the deliberate
        // yScale:0 entrance start state ONLY exists when `animated: true`
        // guarantees something will resolve it.
        function test_defaultAnimatedFalseKeepsBarsAtFullScaleImmediately() {
            compare(bc.animated, false);
            var bar = findChild(bc, "hourBar_0");
            verify(bar !== null);
            compare(bar.transform[0].yScale, 1);
        }

        function test_animatedChartGrowsAllBarsToFullScale() {
            tryCompare(findChild(bcAnimated, "hourBar_0").transform[0], "yScale", 1);
            tryCompare(findChild(bcAnimated, "hourBar_1").transform[0], "yScale", 1);
            tryCompare(findChild(bcAnimated, "hourBar_2").transform[0], "yScale", 1);
            verify(bcAnimated._entranceDone === true);
        }

        // The double-grow guard (advisor §2/§3#1, brief B2 CRITICAL) — the
        // highest-value test here. AdminScreen's Loader recreates the
        // dashboard's chart once per navigation, but its VM refresh()es
        // ~0.3-1s later and resets the model, destroying and recreating
        // every delegate a SECOND time within the same visit. Without the
        // `_entranceDone` latch, that second batch would restart the
        // staggered PauseAnimation-then-grow from yScale 0.
        function test_entranceLatchPreventsRegrowOnModelReset() {
            tryCompare(findChild(bcAnimated, "hourBar_0").transform[0], "yScale", 1);
            verify(bcAnimated._entranceDone === true);

            barsAnimatedFixture.clear();
            barsAnimatedFixture.append({ label: "8", value: 20 });
            barsAnimatedFixture.append({ label: "9", value: 30 });
            barsAnimatedFixture.append({ label: "10", value: 40 });

            // Checked SYNCHRONOUSLY (no wait) — the reset above just
            // destroyed and recreated every delegate. If the latch is
            // working, the fresh delegate's Component.onCompleted snapped it
            // straight to yScale 1; if the guard were missing, the delegate
            // would instead be mid-PauseAnimation with yScale still 0 (the
            // grow leg hasn't started yet, let alone finished).
            var bar0 = findChild(bcAnimated, "hourBar_0");
            verify(bar0 !== null);
            compare(bar0.transform[0].yScale, 1);
        }

        // Department bars glide on value change (brief B3) — a Behavior,
        // not a keyframe entrance, and the fill Rectangle only (never the
        // track). Uses ListModel.setProperty (a targeted dataChanged), not a
        // model reset, so the SAME fill delegate survives and its Behavior
        // can be observed animating in place.
        function test_deptBarFillWidthGlidesToNewValueViaBehavior() {
            var fill = findChild(bcDeptFill, "deptBarFill_0");
            verify(fill !== null);
            var track = fill.parent;
            tryCompare(fill, "width", track.width * (100 / 200));

            deptFillFixture.setProperty(0, "value", 180);
            var targetWidth = track.width * (180 / 200);
            wait(120);
            // Mid-flight: past the old width, not yet at the new one — the
            // signature of an in-progress Behavior, not an instant snap.
            verify(fill.width > track.width * (100 / 200));
            verify(fill.width < targetWidth);
            tryCompare(fill, "width", targetWidth);
        }

        // Hover brighten + "N visits" readout (owner-approved, brief B4):
        // brighten alone is a false affordance, so both ship together.
        function test_hourBarHoverBrightensAndShowsVisitsReadout() {
            waitForRendering(bc);
            var bar = findChild(bc, "hourBar_0");
            verify(bar !== null);
            var readout = findChild(bc, "hourBarReadout_0");
            verify(readout !== null);
            compare(readout.visible, false);
            var restColor = bc.colorFor(0);
            compare(bar.color, restColor);

            // A genuine position DELTA is needed to synthesize a hover-move
            // event — start from a neutral point away from any bar first.
            mouseMove(bc, 2, 2);
            mouseMove(bar, bar.width / 2, bar.height / 2);
            tryCompare(readout, "visible", true);
            compare(findChild(bc, "hourBarReadoutText_0").text, "12 visits");
            // The Behavior on color animates the brighten over
            // Theme.motion.hoverLift — tryCompare waits it out instead of
            // racing an immediate compare against the still-mid-transition
            // value.
            tryCompare(bar, "color", Qt.lighter(restColor, 1.12));

            // Park the pointer over empty space above the bars (the
            // fill-height spacer, not any bar) so later tests see no
            // lingering hover state on this shared fixture.
            mouseMove(bc, 2, 2);
            tryCompare(readout, "visible", false);
            tryCompare(bar, "color", restColor);
        }
    }

    TestCase {
        name: "LPageHeaderShowsBoth"
        when: windowShown
        function test_subtitleAndClockBothPresent() {
            verify(findText(ph, "Library activity overview") !== null);
            verify(findText(ph, "8:04:11 AM") !== null);
        }
        // The date must render as its own text alongside the clock, not
        // merely be swallowed into/duplicated by clockText. Reference
        // format: date, then the clock. Deleting `dateText: hdr.dateText`
        // from LPageHeader.qml's headerDateText Text (or its property
        // declaration) makes this fail: the node either never appears or
        // reports the wrong string.
        function test_dateTextRendersBesideClock() {
            var dateNode = findChild(ph, "headerDateText");
            var clockNode = findChild(ph, "headerClockText");
            verify(dateNode !== null);
            verify(clockNode !== null);
            compare(dateNode.text, "Monday, July 6, 2026");
            compare(clockNode.text, "8:04:11 AM");
            verify(dateNode.visible === true);
        }
        function findText(root, s) {
            if (root.text !== undefined && root.text !== null && root.text.indexOf(s) !== -1)
                return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findText(root.children[i], s);
                if (f !== null) return f;
            }
            return null;
        }
    }

    TestCase {
        name: "LSidebarBrandShowsSchoolInfo"
        when: windowShown

        function test_staticTitleAlwaysShown() {
            var titleNode = findChild(brandNoLogo, "brandTitleText");
            verify(titleNode !== null);
            compare(titleNode.text, "Library Admin");
        }
        function test_schoolNameRendered() {
            var nameNode = findChild(brandNoLogo, "brandSchoolNameText");
            verify(nameNode !== null);
            compare(nameNode.text, "Example Community Library");
        }
        // hasLogo: false must show the placeholder circle, never attempt to
        // load an Image. Deleting the `visible: !logoCanvas.visible` binding
        // on logoPlaceholder (or hardcoding it true) makes this fail.
        function test_noLogoShowsPlaceholder() {
            var placeholder = findChild(brandNoLogo, "logoPlaceholder");
            var image = findChild(brandNoLogo, "logoImage");
            verify(placeholder !== null);
            compare(placeholder.visible, true);
            compare(image.source.toString(), "");
        }
        // hasLogo: true with a real (synthetic) image source must switch to
        // the canvas-rendered circle and hide the placeholder. Deleting
        // `source: brand.hasLogo ? brand.logoUrl : ""` (leaving it always
        // "") makes this fail: the image never reaches Ready and the
        // placeholder never goes away.
        function test_logoLoadsAndHidesPlaceholder() {
            var image = findChild(brandWithLogo, "logoImage");
            var canvas = findChild(brandWithLogo, "logoCanvas");
            var placeholder = findChild(brandWithLogo, "logoPlaceholder");
            verify(image !== null);
            tryCompare(image, "status", Image.Ready, 5000);
            tryCompare(canvas, "visible", true, 5000);
            compare(placeholder.visible, false);
        }
    }

    TestCase {
        name: "LTextFieldBehavior"
        when: windowShown
        // test_editingRoundTripsToTextProperty mutates the shared `tf`
        // fixture's text; TestCase runs test_ functions in alphabetical
        // order (not declaration order — see LTableRendersRows/
        // LBarChartRenders above for the same pattern), so
        // "test_editingRoundTripsToTextProperty" runs before
        // "test_labelAndTextExposed" and would leak "New Value" into it
        // without this reset.
        function init() {
            tf.text = "Acme";
        }
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
        // The re-typed key is deliberate friction in front of an irreversible
        // op — it must not survive one use. Reopening must land on a blank
        // field with Confirm disabled again, for EVERY tier-2 consumer.
        function test_tier2KeyClearedOnReopenSoConfirmIsDisabledAgain() {
            var btn = findChild(cd2, "confirmButton");
            cd2.visible = true; waitForRendering(cd2);
            findChild(cd2, "confirmKeyField").text = "typed-key";
            compare(btn.enabled, true);
            mouseClick(btn);

            cd2.visible = false;
            cd2.visible = true; waitForRendering(cd2);      // reopen
            compare(findChild(cd2, "confirmKeyField").text, "");
            compare(btn.enabled, false);
        }
        function test_clearKeyIsCallableByConsumers() {
            findChild(cd2, "confirmKeyField").text = "typed-key";
            cd2.clearKey();
            compare(findChild(cd2, "confirmKeyField").text, "");
            compare(findChild(cd2, "confirmButton").enabled, false);
        }
    }

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
}
