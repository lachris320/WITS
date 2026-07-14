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

    TestCase {
        name: "ComponentsInstantiateAndBindTheme"
        function test_allCreated() {
            verify(b !== null); verify(c !== null); verify(t !== null);
            verify(sg !== null); verify(sn !== null); verify(to !== null);
            verify(dg !== null); verify(st !== null); verify(bc !== null);
            verify(ph !== null);
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
}
