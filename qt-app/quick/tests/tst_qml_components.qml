import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    width: 400; height: 400

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
        width: 320; height: 160
        model: barsFixture
        maxValue: 41
        highlightIndex: 2
    }
    LPageHeader {
        id: ph
        width: 480
        title: "Dashboard"
        subtitle: "Library activity overview"
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
    }

    TestCase {
        name: "LPageHeaderShowsBoth"
        when: windowShown
        function test_subtitleAndClockBothPresent() {
            verify(findText(ph, "Library activity overview") !== null);
            verify(findText(ph, "8:04:11 AM") !== null);
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
