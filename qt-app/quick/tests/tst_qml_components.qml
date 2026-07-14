import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    width: 400; height: 400

    LButton    { id: b;  text: "OK" }
    LCard      { id: c }
    LTable     { id: t }
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
    LBarChart  { id: bc }
    LPageHeader{ id: ph; title: "P" }
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
}
