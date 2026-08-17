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
    // synthetic mouse events meant for the earlier one. Also accommodates
    // `rowActBand` (4a.2b-ii Task 4), the LTable.rowActivated double-click
    // fixture, in its own band below `casc`. And `datePicker` (Phase 4b-i
    // Task 7), the LDatePicker fixture, in its own band below `chk`.
    width: 400; height: 3600

    LButton    { id: b;  text: "OK" }
    LButton {
        id: bTip
        text: "Delete ( 3 )"
        tooltipText: "Exports selected rows, or all filtered rows if none are selected."
        accessibleName: "Delete 3 selected rows"
    }
    LCard      { id: c }
    // Outline-variant factory for the onBrand label-colour test: each case
    // needs a fresh instance so onBrand can be set at construction, matching
    // how BrandPanel's guest button opts in.
    Component {
        id: outlineBtnComponent
        LButton { variant: "Outline"; text: "Guest" }
    }
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
    // Typed-confirmation gate fixture (Phase 4a.2b-i Task 7): opt-in via
    // requireTypedConfirmation, independent of cd1/cd2's default-false tiers.
    LConfirmDialog {
        id: cdTyped; tier: 1; title: "Delete students?"
        message: "This cannot be undone."; confirmText: "Delete"
        requireTypedConfirmation: true; confirmationWord: "DELETE"
    }
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
    LPulseDot  { id: pd; color: Theme.accent.base; pulseDuration: 900 }
    LEyebrow   { id: eb; text: "EYEBROW"; color: Theme.accent.base }
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

    // LLogoCircle fixtures — the badge LSidebarBrand and the kiosk BrandPanel
    // now share. Same synthetic data-URI PNG as above (1x1 transparent pixel:
    // no real school asset, no filesystem/network access).
    LLogoCircle { id: logoCircleEmpty;  hasLogo: false }
    LLogoCircle {
        id: logoCircleLoaded
        hasLogo: true
        size: 96
        ringWidth: 3
        logoUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
    }
    // hasLogo:false must veto a set logoUrl — the VM reports hasLogo:false for
    // a configured-but-missing file, and loading it anyway would flash a
    // broken image where the placeholder belongs.
    LLogoCircle {
        id: logoCircleVetoed
        hasLogo: false
        logoUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
    }

    // Pixel-proof fixture for LLogoCircle. The other LLogoCircle fixtures use a
    // 1x1 transparent PNG, which can only prove *visibility* — a canvas that
    // paints nothing at all looks identical. This one is a synthetic 24x8
    // (3:1 landscape) PNG whose horizontal thirds are solid blue | red | green,
    // so a grabImage() pixel sample can prove three separate things at once:
    //   - something was actually painted (centre pixel is opaque red, not blank),
    //   - the paint is a centred "cover" crop, not a stretch (a pixel well left
    //     of centre is still red; a stretch would put the blue third there),
    //   - the circular clip still holds (a bounding-box corner is not red).
    // Synthetic test data, not a school asset: no filesystem/network access.
    LLogoCircle {
        id: logoCirclePixel
        objectName: "logoCirclePixel"
        y: 1500
        size: 96
        ringWidth: 3
        hasLogo: true
        logoUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAICAIAAABsw6g0AAAAGUlEQVR42mNgYPiPFf3HIcGAU2LUoBFsEAA0Dr9B3WEGXAAAAABJRU5ErkJggg=="
    }

    // Kiosk BrandPanel fixture with a stub vm, in its own vertical band below
    // every other fixture: an interactive fixture sharing a position with a
    // sibling silently absorbs synthetic mouse events meant for the other one
    // (see the LBarChart/LTable notes above).
    QtObject {
        id: kioskStubVm
        property string schoolName: "Example Community Library"
        property string schoolAddress: "Test City"
        property string libraryHours: "6 AM – 5 PM"
        property string clockTime: "8:04:11"
        property string clockMeridiem: "AM"
        property string clockDate: "Monday, July 6, 2026"
        property bool guestEnabled: false
        property url logoUrl: ""
        property bool hasLogo: false
        function submitLogin(v) {}
        function requestGuest() {}
    }
    BrandPanel {
        id: kioskPanel
        objectName: "kioskPanelFixture"
        y: 1150
        width: 390; height: 340
        vm: kioskStubVm
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
            // LButton Primary fill binds Theme.brand.base — not a local literal.
            compare(b.fillColor, Theme.brand.base);
        }
        function test_outlineButtonOnBrandUsesCreamLabel() {
            // Outline on a maroon brand panel must render its label in brand.on
            // (cream), not the light-bg Theme.text (dark slate) which is illegible.
            var onB = createTemporaryObject(outlineBtnComponent, host, { onBrand: true });
            verify(onB !== null);
            compare(onB.contentItem.color.toString(), Theme.brand.on.toString());
            var offB = createTemporaryObject(outlineBtnComponent, host, { onBrand: false });
            compare(offB.contentItem.color.toString(), Theme.text.toString());
        }
        function test_cardBindsCardToken() {
            compare(c.color, Theme.card);
        }
        function test_pulseDotCreatedWithColorAndDuration() {
            verify(pd !== null);
            compare(pd.color, Theme.accent.base);
            compare(pd.pulseDuration, 900);
        }
        function test_eyebrowDefaultsAndOverrides() {
            verify(eb !== null);
            compare(eb.text, "EYEBROW");
            compare(eb.color, Theme.accent.base);
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
            // Phase 4d role map: kiosk structure flipped gold->maroon; the gradient
            // now derives from brand.base->brand.deep.
            compare(gr.gradient.stops[0].color, Theme.brand.base);
            compare(gr.gradient.stops[1].color, Theme.brand.deep);
        }
    }

    TestCase {
        name: "LToastPlainText"
        when: windowShown
        // The Database delete flow pipes a server-supplied error `message`
        // (DatabaseViewModel::onDeleteFinished's generic-error branch) into
        // vm.statusMessage, which lands here as toast.message. Text defaults
        // to AutoText, which auto-detects and RENDERS rich text (including
        // remote <img> fetches) — over a backend reached via cleartext HTTP.
        // Pinned plain in the primitive so no consumer has to remember
        // (mirrors LDialog.qml's dialogTitleText/dialogMessageText guard).
        function test_contentTextIsPlainNotRichText() {
            var content = findChild(to, "toastText");
            verify(content !== null);
            compare(content.textFormat, Text.PlainText);
        }
    }

    TestCase {
        name: "LStatTileValuePlainText"
        when: windowShown
        // Reporting's topCourse tile pipes a server-echoed course name into
        // LStatTile.value over cleartext HTTP. Text defaults to AutoText,
        // which auto-detects and RENDERS rich text (including a beaconing
        // remote <img>) — pinned plain here so no consumer has to remember
        // (same guard as LToastPlainText above).
        function test_statTileValueIsPlainText() {
            var t = findChild(st, "statTileValue");
            verify(t !== null);
            compare(t.textFormat, Text.PlainText);
        }

        // GUI-smoke bug: Reporting's "Top Course" tile overflowed the fixed
        // tile size on long course names. The value Text must shrink-to-fit
        // then elide rather than spilling out of the card.
        function test_statTileValueElidesLongText() {
            st.value = "Bachelor of Science in Information Technology and Computing";
            var t = findChild(st, "statTileValue");
            verify(t !== null);
            compare(t.elide, Text.ElideRight);
            compare(t.fontSizeMode, Text.HorizontalFit);
            st.value = "1";   // restore fixture default for later tests
        }
    }

    TestCase {
        name: "LButtonTooltipAndAccessibleName"
        when: windowShown
        function test_accessibleNameOverridesTextWhenSet() {
            compare(bTip.Accessible.name, "Delete 3 selected rows");
        }
        function test_accessibleNameFallsBackToTextWhenEmpty() {
            // The default fixture `b` (LButton { text: "OK" }) sets no
            // accessibleName, so Accessible.name must still be the label.
            compare(b.Accessible.name, "OK");
        }
        function test_tooltipTextIsHeldButHiddenUntilHover() {
            var tip = findChild(bTip, "lbuttonTooltip");
            verify(tip !== null);
            compare(tip.text, "Exports selected rows, or all filtered rows if none are selected.");
            compare(tip.visible, false);        // not hovered => hidden
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

        // Anti-injection: emptyStateText carries server-supplied error
        // messages (e.g. vm.errorText) over cleartext HTTP; the empty-state
        // Text must pin PlainText so a tampered <img>/markup value can't be
        // rendered (mirrors the cell PlainText guard).
        function test_emptyStateIsPlainText() {
            compare(findChild(t, "tableEmptyState").textFormat, Text.PlainText);
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
        // staggerCap). populate/add CLONE their whole animation tree per
        // transitioning item and the pause duration is assigned imperatively
        // onto that clone from onIdxChanged (LTable.qml:87-149), so reading
        // populatePause.duration on the template from outside reflects no
        // real item — asserting on it would be a vacuous test.
        //
        // This test used to bridge that gap with a wall-clock sample:
        // `wait(450)` — picked to land in the gap between index 0's ~400ms
        // (Theme.motion.rowIn) settle and index 8's 600ms (200ms pause +
        // 400ms fade) — then `compare(row0.opacity, 1)` +
        // `verify(row8.opacity > 0 && row8.opacity < 1)`. Under load
        // `wait(450)` returns well past 450ms of animation time, row 8 has
        // already settled at 1, and the strict-inequality check fails for
        // reasons unrelated to the code under test. Same flake class as the
        // pageIn entrance tests de-flaked just before this one — and since
        // both tst_qml_admin and tst_qml_components compile this whole
        // directory via QUICK_TEST_SOURCE_DIR, one flake here reddens several
        // ctest entries at once.
        //
        // The timed sample is replaced by a polling WIRING proof — see the
        // twin comment on SearchScreen's test of the same name
        // (tst_qml_admin.qml) for the full reasoning — asserting the
        // invariant that holds under ANY load: monotonic lead,
        // row0.opacity >= row8.opacity at EVERY sample (same duration and
        // easing for both, index 8 merely starts later), requiring that a
        // STRICT lead be observed at least once, which is what separates real
        // stagger from all rows animating in lockstep, inside a bounded retry
        // so one scheduler stall swallowing the whole 600ms window can't
        // redden the suite.
        //
        // The stagger ARITHMETIC itself (clamp at staggerCap, multiply by the
        // step, and the negative-index guard) is deliberately NOT re-asserted
        // here: tst_qml_theme.qml's
        // test_staggerDelayClampsIndexToStaggerCapThenMultipliesByStep covers
        // it on the pure Theme.motion.staggerDelay function, with zero clock,
        // and QUICK_TEST_SOURCE_DIR means that test runs inside THIS binary
        // too — repeating it here would only duplicate coverage already
        // executing in the same process.
        //
        // RETRY POLICY (twin of the one in tst_qml_admin.qml): a retry is
        // legitimate ONLY when the entrance window was missed entirely —
        // index 8 never sampled in flight, i.e. scheduler jitter. If the
        // window WAS sampled (index 8 below 1 next to an index 0 that had
        // not settled either) and no strict lead appeared, the entrance
        // really ran and really failed to stagger: that fails IMMEDIATELY,
        // because retrying it would also pass a product bug where the
        // stagger fires only sometimes. Running out of attempts without ever
        // sampling the window fails as well, but with a distinct message —
        // one that must NOT blame the machine alone, because a deleted or
        // disabled populate Transition produces the IDENTICAL observation
        // (rows sit at opacity 1, `lateStarted` never becomes true). The
        // message names both causes and points at the row-entrance tests,
        // which distinguish them.
        function test_rowStaggerDelaysHigherIndexRows() {
            // Wiring: index 8's entrance really trails index 0's.
            //
            // The SHAPE of the opacity signal, measured rather than assumed:
            // the delegate declares no `opacity: 0` default (see the populate
            // comment in LTable.qml), and the populate clone's
            // NumberAnimation only writes its `from: 0` when it actually
            // STARTS — i.e. after its own PauseAnimation. So for the whole of
            // index 8's 200ms pause the row sits at its natural opacity of
            // exactly 1 while index 0 is already mid-fade; only then does it
            // drop to ~0 and climb. A flat "row0.opacity >= row8.opacity at
            // all times" invariant is therefore FALSE over that first window.
            // The invariant that does hold under ANY load is gated on index 8
            // having started: once it has been seen below 1, index 0 is at or
            // ahead of it forever after — same duration, same easing, later
            // start.
            var eps = 1e-6;
            var sawStrictLead = false;
            // Per-attempt: was index 8 ever sampled strictly between its
            // start and its settle, alongside an index 0 that had not settled
            // either? Only a FALSE here licenses a retry (see RETRY POLICY).
            var windowObserved = false;
            var row0 = null;
            var row8 = null;
            for (var attempt = 0; attempt < 3 && !sawStrictLead; attempt++) {
                // resetAnimatedRows() forces a genuine model-changed event,
                // which re-fires populate from t=0 for the whole batch.
                resetAnimatedRows();
                waitForRendering(tAnimated);
                row0 = findChild(tAnimated, "tableRow_0");
                row8 = findChild(tAnimated, "tableRow_8");
                verify(row0 !== null);
                verify(row8 !== null);

                // Generous deadline: ~3x the 600ms (200 pause + 400 fade) the
                // entrance needs when nothing competes for the CPU. No single
                // sample is load-critical — the loop only has to catch the
                // pair at ANY point in index 8's ~400ms flight, and if a
                // stall swallows that whole window the outer retry re-plays
                // it.
                var lateStarted = false;
                windowObserved = false;
                var deadline = Date.now() + 2000;
                while (Date.now() < deadline) {
                    var early = row0.opacity;
                    var late = row8.opacity;
                    if (late < 1 - eps)
                        lateStarted = true;
                    if (lateStarted) {
                        // Index 8 can never be ahead of index 0 — this would
                        // fire on an inverted/negative stagger.
                        verify(early >= late - eps,
                               "row 8 overtook row 0: " + early + " < " + late);
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
                    // Nothing more to learn once index 0 has settled and
                    // either the lead was seen or index 8 has settled too.
                    if (early >= 1 - eps && (sawStrictLead || late >= 1 - eps))
                        break;
                    wait(10);
                }
                // Fail fast: the window WAS sampled and index 8 still never
                // trailed index 0. Retrying this would only paper over a
                // stagger that works intermittently.
                if (windowObserved && !sawStrictLead)
                    fail("row 8 was sampled mid-entrance alongside an unsettled "
                         + "row 0 and never trailed it — stagger not wired "
                         + "(attempt " + (attempt + 1) + ")");
            }
            verify(sawStrictLead,
                   "row 8's entrance window was never sampled in 3 attempts — "
                   + "either the row entrance never ran at all, or the machine "
                   + "was too loaded to sample it; check "
                   + "test_animateRowsTrueRepopulatesFreshRowsOnModelReset, "
                   + "which fails with an accurate message in the first case");
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
            var hoverColor = Qt.alpha(Theme.brand.base, 0.06);

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
        //
        // This test used to sample the glide at a fixed wall-clock offset:
        // `wait(120)` (~15% of the 800ms Theme.motion.deptBarFill) then
        // `verify(fill.width > oldWidth)` + `verify(fill.width <
        // targetWidth)`. The second half is two-sided and becomes FALSE the
        // moment the glide finishes, so any stall that pushes `wait(120)`
        // past the Behavior's duration reddens it for reasons unrelated to
        // the code under test — the last member of the flake class already
        // de-flaked in the pageIn entrance tests and the row-stagger tests
        // above. Both tst_qml_admin and
        // tst_qml_components compile this whole directory via
        // QUICK_TEST_SOURCE_DIR, so one flake here reddens several ctest
        // entries at once.
        //
        // The timed sample is replaced by three checks that are collectively
        // STRICTER — in particular (a), which the original missed entirely:
        //   (a) ZERO-CLOCK proof that the width does not SNAP. With a
        //       Behavior in place a binding change cannot take effect
        //       synchronously: the interceptor starts an animation from the
        //       current value, so the property still reads the OLD width at
        //       the instant of the write and only moves on the next tick.
        //       Strip the Behavior and the plain binding updates
        //       synchronously, so this fails. Same idiom as
        //       test_rowHoverColorGlidesInAndOutViaBehavior above ("must
        //       still read the resting value at that exact instant"); no
        //       clock is involved at all, which makes it load-immune and the
        //       single strongest assertion available here.
        //   (b) proof it genuinely INTERPOLATES rather than doing a delayed
        //       jump — (a) alone would still pass a `duration: 0` Behavior,
        //       whose width would simply change one tick later. So poll for
        //       a width strictly between the old and target widths, rather
        //       than betting on one fixed offset. A single stall long enough
        //       to swallow the whole 800ms flight would see no intermediate
        //       value, so the observation is wrapped in a bounded retry that
        //       re-triggers the glide (the same bounded-retry idiom the
        //       row-stagger tests above use); only "no attempt ever saw an
        //       intermediate width" fails.
        //   (c) the end state, via the existing poll-based (hence
        //       load-tolerant) tryCompare.
        // The two tryCompares also pin the resting-width arithmetic
        // (track.width * value / chart.maxValue) at two distinct values.
        //
        // Both expected widths are DERIVED, not spelled out: the resting
        // value is read back from deptFillFixture and the divisor from
        // bcDeptFill.maxValue. Writing `track.width * (100 / 200)` would
        // duplicate the fixture's `value: 100` and the chart's
        // `maxValue: 200`, so editing either would silently desync the
        // expectation from the thing under test and the test would fail (or
        // worse, pass) for a reason that has nothing to do with the
        // Behavior. The only literal left is the TARGET value, which is not
        // fixture data — this test chooses it itself, via setProperty, and
        // the expected target width is computed from it.
        //
        // Fixture note: deptFillFixture / bcDeptFill are referenced by no
        // other test in this file, but TestCase runs test_ functions in
        // alphabetical order, so the end state is kept deterministic anyway
        // — every path through the retry leaves value at 180, exactly as the
        // previous version of this test did.
        function test_deptBarFillWidthGlidesToNewValueViaBehavior() {
            var fill = findChild(bcDeptFill, "deptBarFill_0");
            verify(fill !== null);
            var track = fill.parent;
            // Read the resting value off the fixture BEFORE changing it, and
            // the divisor off the chart — never re-typed here.
            var restValue = deptFillFixture.get(0).value;
            var maxValue = bcDeptFill.maxValue;
            // The only literal: this test's own choice of where to glide to.
            var targetValue = 180;
            verify(targetValue !== restValue);
            var restWidth = track.width * (restValue / maxValue);
            var targetWidth = track.width * (targetValue / maxValue);
            tryCompare(fill, "width", restWidth);

            // (a) No snap: checked SYNCHRONOUSLY, with no wait and no
            // event-loop turn between the write and the read.
            deptFillFixture.setProperty(0, "value", targetValue);
            compare(fill.width, restWidth);

            // (b) Real interpolation: some width strictly inside the open
            // interval (restWidth, targetWidth) must be observable.
            var eps = 1e-6;
            var sawIntermediate = false;
            for (var attempt = 0; attempt < 3 && !sawIntermediate; attempt++) {
                if (attempt > 0) {
                    // Re-trigger: glide back down, let it fully settle, then
                    // re-issue the same value change this test is about.
                    deptFillFixture.setProperty(0, "value", restValue);
                    tryCompare(fill, "width", restWidth);
                    deptFillFixture.setProperty(0, "value", targetValue);
                }
                // Generous deadline: ~2.5x the 800ms the glide needs when
                // nothing competes for the CPU. No single sample is
                // load-critical — the loop only has to catch the fill at ANY
                // point of its flight, and if a stall swallows the whole
                // window the outer retry re-plays it.
                var deadline = Date.now() + 2000;
                while (Date.now() < deadline) {
                    var w = fill.width;
                    if (w > restWidth + eps && w < targetWidth - eps) {
                        sawIntermediate = true;
                        break;
                    }
                    // Nothing more to learn once the glide has landed.
                    if (w >= targetWidth - eps)
                        break;
                    wait(10);
                }
            }
            verify(sawIntermediate,
                   "fill width never took an intermediate value in 3 attempts "
                   + "— the dept bar snaps instead of gliding");

            // (c) End state, poll-based so it tolerates a loaded machine.
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

        // LDialog is the only modal primitive, so anything a consumer ever
        // pipes into title/message (backend "message" fields reach the UI over
        // cleartext HTTP elsewhere in this screen) must not be parsed as
        // markup. Text defaults to AutoText, which auto-detects and RENDERS
        // rich text — including remote <img> fetches.
        function test_dialogTextIsPlainNotRichText() {
            compare(findChild(cd2, "dialogTitleText").textFormat, Text.PlainText);
            compare(findChild(cd2, "dialogMessageText").textFormat, Text.PlainText);
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
        // The Confirm gate keys off text.trim(), so a key typed with stray
        // whitespace passed the client check and then 401'd server-side —
        // surfacing as "Admin key rejected. Please sign in again." on the most
        // destructive operation in the app. Gate and payload must agree.
        function test_tier2ConfirmedEmitsTrimmedKey() {
            var got = null;
            cd2.confirmed.connect(function(k) { got = k; });
            cd2.visible = true; waitForRendering(cd2);
            findChild(cd2, "confirmKeyField").text = "  typed-key  ";
            mouseClick(findChild(cd2, "confirmButton"));
            compare(got, "typed-key");
        }
        // Whitespace-only is not a key: the gate already rejects it, and the
        // payload must never become an empty admin_key that reads as "no key".
        function test_tier2WhitespaceOnlyKeyLeavesConfirmDisabled() {
            var btn = findChild(cd2, "confirmButton");
            cd2.visible = true; waitForRendering(cd2);
            findChild(cd2, "confirmKeyField").text = "   ";
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
        name: "LConfirmDialogTypedConfirmation"
        when: windowShown
        function init() {
            cdTyped.visible = false; cdTyped.busy = false;
            var f = findChild(cdTyped, "confirmTypedField");
            if (f) f.text = "";
        }
        function test_typedFieldExistsWhenRequired() {
            verify(findChild(cdTyped, "confirmTypedField") !== null);
        }
        function test_defaultDialogHasNoTypedField() {
            // cd1 keeps the default requireTypedConfirmation:false.
            compare(findChild(cd1, "confirmTypedField"), null);
        }
        function test_confirmDisabledUntilWordTypedExactly() {
            cdTyped.visible = true; waitForRendering(cdTyped);
            var btn = findChild(cdTyped, "confirmButton");
            compare(btn.enabled, false);                 // empty
            findChild(cdTyped, "confirmTypedField").text = "delete";
            compare(btn.enabled, false);                 // wrong case — exact match required
            findChild(cdTyped, "confirmTypedField").text = "DELETE";
            compare(btn.enabled, true);
        }
        function test_typedFieldClearedOnReopen() {
            cdTyped.visible = true; waitForRendering(cdTyped);
            findChild(cdTyped, "confirmTypedField").text = "DELETE";
            cdTyped.visible = false;
            cdTyped.visible = true; waitForRendering(cdTyped);
            compare(findChild(cdTyped, "confirmTypedField").text, "");
            compare(findChild(cdTyped, "confirmButton").enabled, false);
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
        // The model is server-supplied (get_departments.php), so the value
        // label must never be parsed as markup — Text defaults to AutoText.
        function test_valueLabelIsPlainNotRichText() {
            compare(findChild(cb, "comboValueText").textFormat, Text.PlainText);
        }
        function test_modelDrivesOptionCount() {
            compare(cb.count, 3);
        }
    }

    TestCase {
        name: "LLogoCircleRendersLogoOrPlaceholder"
        when: windowShown

        // No logo: the placeholder circle carries the block, and nothing is
        // ever handed to the Image. Deleting the `visible: !logoCanvas.visible`
        // binding on logoPlaceholder (or hardcoding it true) makes this fail.
        function test_noLogoShowsPlaceholderAndLoadsNothing() {
            var placeholder = findChild(logoCircleEmpty, "logoPlaceholder");
            var canvas = findChild(logoCircleEmpty, "logoCanvas");
            var image = findChild(logoCircleEmpty, "logoImage");
            verify(placeholder !== null);
            verify(canvas !== null);
            compare(placeholder.visible, true);
            compare(canvas.visible, false);
            compare(image.source.toString(), "");
        }

        // A loadable source must switch to the canvas-rendered circular crop
        // and retire the placeholder. Deleting
        // `source: hasLogo ? logoUrl : ""` (leaving it always "") makes this
        // fail: the image never reaches Ready and the placeholder never goes away.
        function test_loadedLogoHidesPlaceholder() {
            var image = findChild(logoCircleLoaded, "logoImage");
            var canvas = findChild(logoCircleLoaded, "logoCanvas");
            var placeholder = findChild(logoCircleLoaded, "logoPlaceholder");
            verify(image !== null);
            tryCompare(image, "status", Image.Ready, 5000);
            tryCompare(canvas, "visible", true, 5000);
            compare(placeholder.visible, false);
        }

        // hasLogo:false wins over a set logoUrl — the veto, not just the
        // absence of a URL, is what keeps the placeholder up.
        function test_hasLogoFalseVetoesSetUrl() {
            var image = findChild(logoCircleVetoed, "logoImage");
            var placeholder = findChild(logoCircleVetoed, "logoPlaceholder");
            compare(image.source.toString(), "");
            compare(placeholder.visible, true);
        }

        // size drives the badge box; ringWidth drives the gold ring so the
        // 52/2 sidebar and the 96/3 kiosk badge stay one component.
        function test_sizeAndRingWidthAreConsumerControlled() {
            compare(logoCircleEmpty.implicitWidth, 52);
            compare(logoCircleEmpty.implicitHeight, 52);
            compare(findChild(logoCircleEmpty, "logoPlaceholder").border.width, 2);
            compare(logoCircleLoaded.implicitWidth, 96);
            compare(findChild(logoCircleLoaded, "logoPlaceholder").border.width, 3);
        }
    }

    TestCase {
        name: "LLogoCirclePaintsCroppedPixels"
        when: windowShown

        // Sampling helper: grabImage()'s image object exposes per-channel
        // accessors, which compare far more legibly than a packed colour.
        function isRed(img, x, y) {
            return img.red(x, y) > 200 && img.green(x, y) < 60 && img.blue(x, y) < 60;
        }

        // THE regression guard for "the circle is empty". Every other logo test
        // asserts visibility only, so a Canvas that paints nothing whatsoever
        // passes them all. Grab the real rendered item and require the centre
        // pixel to be the source image's colour.
        function test_canvasActuallyPaintsThePixels() {
            var canvas = findChild(logoCirclePixel, "logoCanvas");
            tryCompare(canvas, "visible", true, 5000);
            wait(50);   // let the queued requestPaint() land before grabbing
            var img = grabImage(logoCirclePixel);
            compare(img.width, 96);
            verify(isRed(img, 48, 48));
        }

        // Aspect: the source is 3:1 landscape, so a naive
        // drawImage(src, 0, 0, w, h) stretch would squash all three colour
        // bands into the circle and paint x=14 blue. A centred cover-crop takes
        // only the middle (red) square, so every in-circle pixel is red.
        function test_paintIsCenterCroppedNotStretched() {
            var canvas = findChild(logoCirclePixel, "logoCanvas");
            tryCompare(canvas, "visible", true, 5000);
            wait(50);
            var img = grabImage(logoCirclePixel);
            verify(isRed(img, 14, 48));   // left of centre, still inside r=48
            verify(isRed(img, 82, 48));   // right of centre, still inside r=48
        }

        // ...and the crop is still a *circle*: a bounding-box corner sits
        // outside r=48 and must never receive image pixels.
        function test_paintStaysInsideTheCircle() {
            var canvas = findChild(logoCirclePixel, "logoCanvas");
            tryCompare(canvas, "visible", true, 5000);
            wait(50);
            var img = grabImage(logoCirclePixel);
            verify(!isRed(img, 3, 3));
        }
    }

    TestCase {
        name: "BrandPanelSurfacesSchoolLogo"
        when: windowShown

        function init() {
            kioskStubVm.hasLogo = false;
            kioskStubVm.logoUrl = "";
        }

        // The kiosk used to hardcode a "LOGO" circle Rectangle that could
        // never show the school's logo. It must now render the shared
        // LLogoCircle, bound to the vm.
        function test_kioskUsesSharedLogoCircleBoundToVm() {
            var placeholder = findChild(kioskPanel, "logoPlaceholder");
            var canvas = findChild(kioskPanel, "logoCanvas");
            verify(placeholder !== null);
            verify(canvas !== null);
            compare(placeholder.visible, true);   // vm.hasLogo === false
            compare(canvas.visible, false);
        }

        // The hero badge keeps the kiosk's 96px / 3px-ring proportions.
        function test_kioskBadgeKeepsItsSizeAndRing() {
            compare(findChild(kioskPanel, "logoPlaceholder").border.width, 3);
            compare(findChild(kioskPanel, "logoCanvas").width, 96);
        }

        // Flipping the vm's logo state must reach the badge through the
        // binding — proof the panel reads the vm rather than a literal.
        function test_vmLogoStateDrivesTheBadge() {
            var image = findChild(kioskPanel, "logoImage");
            var canvas = findChild(kioskPanel, "logoCanvas");
            var placeholder = findChild(kioskPanel, "logoPlaceholder");
            kioskStubVm.logoUrl = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";
            kioskStubVm.hasLogo = true;
            tryCompare(image, "status", Image.Ready, 5000);
            tryCompare(canvas, "visible", true, 5000);
            compare(placeholder.visible, false);
        }
    }

    // --- LTable multi-select fixtures (own band) ---
    QtObject {
        id: selectStub
        property int setAllCalls: 0
        property bool lastSetAll: false
        property int toggleCalls: 0
        property string lastToggleId: ""
        function setAllSelected(v) { setAllCalls++; lastSetAll = v; }
        function toggle(id) { toggleCalls++; lastToggleId = id; }
        property int selectedCount: 0
        property bool allSelected: false
    }
    // A real ListModel (not a JS array) so per-row `schoolId`/`selected` roles resolve.
    ListModel {
        id: selectRows
        ListElement { schoolId: "A"; name: "Ann"; selected: false }
        ListElement { schoolId: "B"; name: "Ben"; selected: true  }
    }

    LTable {
        id: plainTable
        y: 2100
        width: 400; height: 160
        selectable: false
        columns: [ {key:"name", title:"Name"} ]
        model: selectRows
    }

    LTable {
        id: selectTable
        y: 2280
        width: 400; height: 160
        selectable: true
        selectionModel: selectStub          // NEW prop: carries toggle/setAllSelected/allSelected
        columns: [ {key:"name", title:"Name"} ]
        model: selectRows
    }

    TestCase {
        name: "LTableMultiSelect"; when: windowShown
        function test_noCheckboxWhenNotSelectable() {
            verify(findChild(plainTable, "selectAllCheck") === null);
        }
        function test_selectAllCheckboxExistsWhenSelectable() {
            verify(findChild(selectTable, "selectAllCheck") !== null);
            verify(findChild(selectTable, "rowCheck_A") !== null);
        }
        function test_headerCheckCallsSetAll() {
            var h = findChild(selectTable, "selectAllCheck");
            verify(h !== null);
            var before = selectStub.setAllCalls;
            mouseClick(h);
            compare(selectStub.setAllCalls, before + 1);
            compare(selectStub.lastSetAll, true);
        }
        function test_rowCheckCallsToggle() {
            var r = findChild(selectTable, "rowCheck_A");
            verify(r !== null);
            var before = selectStub.toggleCalls;
            mouseClick(r);
            compare(selectStub.toggleCalls, before + 1);
            compare(selectStub.lastToggleId, "A");
        }
    }

    // --- LCascadingSelect fixtures (own band) ---
    LCascadingSelect {
        id: casc
        y: 2660
        departments: ["CCS","CBA"]
        courses: ["BSIT","BSCS"]
    }
    // Declared child spy (the file has no signalSpy.createObject factory).
    SignalSpy { id: deptSpy; target: casc; signalName: "departmentPicked" }

    TestCase {
        name: "LCascadingSelect"; when: windowShown
        function init() { casc.department = ""; casc.course = ""; deptSpy.clear(); }
        function test_pickingDepartmentEmitsAndClearsCourse() {
            casc.course = "BSIT";
            var deptCombo = findChild(casc, "cascDept");
            deptCombo.selectValue("CCS");
            compare(deptSpy.count, 1);
            compare(casc.department, "CCS");
            compare(casc.course, "");            // dependent-clear
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

    // --- LTable.rowActivated double-click (4a.2b-ii) ---
    // Own vertical band below `casc` (y:2660..2660+its own height): an
    // interactive fixture sharing a position/z with a sibling silently
    // absorbs synthetic mouse events meant for the other one (see the
    // LBarChart/LTable notes above).
    Item {
        id: rowActBand
        y: 2820
        width: 400; height: 200
        LTable {
            id: rowActTable
            anchors.fill: parent
            selectable: true
            columns: [ { key: "name", title: "Name" } ]
            model: ListModel {
                ListElement { schoolId: "S1"; name: "Ann"; selected: false }
                ListElement { schoolId: "S2"; name: "Ben"; selected: false }
            }
        }
        SignalSpy {
            id: rowActSpy
            target: rowActTable
            signalName: "rowActivated"
        }
        TestCase {
            name: "LTableRowActivated"; when: windowShown
            function init() { rowActSpy.clear(); }
            function test_doubleClickEmitsRowActivatedWithSchoolId() {
                var row = findChild(rowActTable, "tableRow_0");
                verify(row !== null);
                // Double-click on the row BODY (far right), away from the
                // ~18x18 checkbox at the left edge. mouseDoubleClickSequence
                // (not mouseDoubleClick) is required here: TapHandler.
                // onDoubleTapped only fires on real press/release pairs,
                // which mouseDoubleClick does not synthesize.
                mouseDoubleClickSequence(row, row.width - 12, row.height / 2);
                compare(rowActSpy.count, 1);
                compare(rowActSpy.signalArguments[0][0], "S1");
            }
        }
    }

    // --- LCheckbox fixture (own band below rowActBand) ---
    LCheckbox { id: chk; objectName: "chk"; y: 3090; label: "Change Status" }

    TestCase {
        name: "LCheckbox"; when: windowShown
        function init() { chk.checked = false; }
        function test_clickTogglesCheckedAndEmits() {
            var spy = signalSpy.createObject(chk, { target: chk, signalName: "toggled" });
            compare(chk.checked, false);
            mouseClick(chk);
            compare(chk.checked, true);
            compare(spy.count, 1);
            mouseClick(chk);
            compare(chk.checked, false);
            compare(spy.count, 2);
            spy.destroy();
        }
        function test_disabledDoesNotToggle() {
            chk.enabled = false;
            mouseClick(chk);
            compare(chk.checked, false);
            chk.enabled = true;
        }
    }
    Component { id: signalSpy; SignalSpy {} }

    // --- LDatePicker fixture (own band below chk) ---
    LDatePicker { id: datePicker; y: 3200; width: 280; height: 44 }
    SignalSpy { id: datePickerSpy; target: datePicker; signalName: "picked" }

    TestCase {
        name: "LDatePicker"; when: windowShown
        function init() { datePickerSpy.clear(); datePicker.selectedDate = ""; }

        function test_selectDayEmitsIsoDateAndSetsSelection() {
            datePicker.showMonth(2026, 8);       // August 2026
            datePicker.selectDay(14);
            compare(datePicker.selectedDate, "2026-08-14");
            compare(datePickerSpy.count, 1);
            compare(datePickerSpy.signalArguments[0][0], "2026-08-14");
        }

        function test_navigatesMonths() {
            datePicker.showMonth(2026, 8);
            datePicker.nextMonth();
            compare(datePicker.displayYear, 2026);
            compare(datePicker.displayMonth, 9);
            datePicker.prevMonth();
            datePicker.prevMonth();
            compare(datePicker.displayMonth, 7);
        }

        function test_navigatesAcrossYearBoundary() {
            datePicker.showMonth(2026, 12);
            datePicker.nextMonth();
            compare(datePicker.displayYear, 2027);
            compare(datePicker.displayMonth, 1);
        }

        function test_fieldShowsSelection() {
            datePicker.selectedDate = "2026-08-14";
            var field = findChild(datePicker, "datePickerField");   // a root child, not in the popup
            verify(field, "field text element exists");
            compare(field.text, "2026-08-14");
        }
    }
}
