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
    LSideNav   { id: sn }
    LToast     { id: to; message: "hi" }
    LDialog    { id: dg; title: "T" }
    LStatTile  { id: st; label: "L"; value: "1" }
    LBarChart  { id: bc }
    LPageHeader{ id: ph; title: "P" }

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
    }
}
