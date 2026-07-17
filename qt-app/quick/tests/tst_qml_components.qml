import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    // Extra height accommodates the three LBarChart motion fixtures below in
    // their own non-overlapping vertical bands (see the comment at `bc`).
    width: 400; height: 700

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
        function test_rowCountMatchesModel() {
            compare(t.rowCount, 2);
        }
        function test_showsEmptyStateWhenNoRows() {
            t.model = null;
            compare(t.rowCount, 0);
            verify(t.emptyVisible === true);
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
}
