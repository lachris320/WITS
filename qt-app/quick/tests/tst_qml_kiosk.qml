import QtQuick
import QtTest
import LOAMS

Item {
    id: host
    width: 420; height: 800

    // Stub VM: same property/method surface BrandPanel consumes.
    QtObject {
        id: stubVm
        property string schoolName: "Maryknoll School of Lupon, Inc."
        property string schoolAddress: "Lupon, Davao Oriental"
        property string libraryHours: "6 AM – 5 PM"
        property string clockTime: "8:04:11"
        property string clockMeridiem: "AM"
        property string clockDate: "Monday, July 6, 2026"
        property bool guestEnabled: true
        property string lastSubmitted: ""
        property int guestRequests: 0
        function submitLogin(v) { lastSubmitted = v }
        function requestGuest() { guestRequests++ }
    }

    BrandPanel { id: panel; anchors.fill: parent; vm: stubVm }

    TestCase {
        name: "BrandPanel"
        when: windowShown

        function test_bindsSchoolInfo() {
            verify(findText(panel, "Maryknoll School of Lupon, Inc.") !== null);
        }
        function test_ctaSubmitsIdField() {
            panel.idText = "202300123";
            panel.submit();                       // the panel's submit hook
            compare(stubVm.lastSubmitted, "202300123");
        }
        function test_guestButtonVisibleWhenEnabled() {
            verify(panel.guestButtonVisible === true);
        }

        // Small helper: depth-first search for a Text whose content contains
        // the given string. Substring match, not exact equality: BrandPanel
        // renders school name + address as one concatenated Text node
        // ("<name> · <address>", per the design's single-line treatment —
        // brief line 3), so no node's text is ever exactly equal to the
        // school name alone. Verified empirically: with exact equality,
        // test_bindsSchoolInfo fails against the brief's own BrandPanel.qml
        // (red), while the other two assertions still pass. A contains-check
        // still proves schoolName is bound and rendered, without weakening
        // what the other two tests assert.
        function findText(root, s) {
            if (root.text !== undefined && root.text !== null
                    && root.text.indexOf(s) !== -1) return root;
            for (var i = 0; i < root.children.length; i++) {
                var f = findText(root.children[i], s);
                if (f !== null) return f;
            }
            return null;
        }
    }
}
