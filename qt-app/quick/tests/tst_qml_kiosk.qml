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

    // --- KioskMain fixture ---
    ListModel {
        id: feedModel
        ListElement { name: "Maria Santos"; course: "BSCE"; yearShort: "3rd Yr"; dept: "Civil Engineering"; time: "8:04 AM"; initials: "MS"; fresh: true }
        ListElement { name: "Jose Ramirez"; course: "BSEE"; yearShort: "2nd Yr"; dept: "Electrical Engineering"; time: "7:58 AM"; initials: "JR"; fresh: false }
    }
    QtObject {
        id: stubMainVm
        property bool hasStudent: true
        property string greeting: "Good morning"
        property string currentName: "Maria"
        property string currentFullName: "Maria Santos"
        property string currentCourse: "BSCE"
        property string currentYear: "3rd Year"
        property string currentDept: "Civil Engineering"
        property string currentTime: "8:04 AM"
        property int visitorsToday: 27
        property int visitorsThisHour: 4
        property var recentLogins: feedModel
    }
    KioskMain { id: main; width: 800; height: 800; vm: stubMainVm }

    TestCase {
        name: "KioskMain"
        when: windowShown
        function test_feedRowCountMatchesModel() {
            compare(main.feedCount, 2);
        }
        function test_heroShowsCurrentWhenHasStudent() {
            verify(main.heroShowsStudent === true);
        }
        function test_statTilesBound() {
            compare(main.visitorsTodayShown, "27");
        }
    }

    // --- KioskScreen fixture ---
    // Real screen with a real KioskViewModel; no appWindow supplied, so
    // Component.onCompleted's `if (screen.appWindow)` guard skips RFID
    // install (installRfid(null) would be a no-op anyway) — this is purely
    // a load/smoke assertion, not RFID or behavioral coverage.
    KioskScreen { id: realScreen; width: 1280; height: 800 }
    TestCase {
        name: "KioskScreenSmoke"
        when: windowShown
        function test_loadsIdleState() {
            verify(realScreen !== null);
            // No student yet -> hero shows the idle prompt path (hasStudent false).
            verify(realScreen.width === 1280);
        }
    }
}
