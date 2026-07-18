import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Admin shell (spec §7.2): sidebar + page header (with live clock) + a Loader
// keyed on Navigator.adminPage. Replaces the Phase-2 placeholder.
Rectangle {
    id: admin
    color: Theme.appBackground

    // Load-on-navigation gate. Default true (production fetches when a page
    // is shown). The shell QuickTest sets this false so the real VMs
    // instantiated below issue NO network — honoring the house "no live
    // network in tests" rule (spec §5), which instantiating the real VMs
    // would otherwise break.
    property bool autoLoad: true

    // Test hooks (mirror the click/footer paths). activatePage takes a
    // Navigator.AdminPage enum value — the sidebar itself speaks string keys
    // (see currentPage/items below), the enum mapping stays confined here.
    function activatePage(page) { Navigator.showAdminPage(page) }
    function backToKiosk() { Navigator.showKiosk() }

    // One VM per screen, instantiated once and reused across page switches.
    DashboardViewModel { id: dashboardVm }
    SearchViewModel    { id: searchVm }
    VisitLogsViewModel { id: visitLogsVm }

    // Read-only school identity (logo + name) for the sidebar brand block.
    // Reads QSettings once at construction (see SchoolInfoViewModel.cpp) —
    // no refresh() to gate, unlike the three VMs above.
    SchoolInfoViewModel { id: schoolInfoVm }

    // Live clock + date for the header. Same "dddd, MMMM d, yyyy" format
    // KioskViewModel::tickClock() already uses for clockDate, so the date
    // reads identically across kiosk and admin.
    property string clockText: ""
    property string dateText: ""
    function tickClock() {
        var now = new Date();
        clockText = Qt.formatDateTime(now, "h:mm:ss AP");
        dateText = Qt.formatDateTime(now, "dddd, MMMM d, yyyy");
    }
    Component.onCompleted: tickClock()
    Timer { interval: 1000; running: true; repeat: true; onTriggered: admin.tickClock() }

    readonly property string pageTitle: {
        switch (Navigator.adminPage) {
        case Navigator.Search:     return qsTr("Search");
        case Navigator.VisitLogs:  return qsTr("Visit Logs");
        default:                   return qsTr("Dashboard");
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        LSideNav {
            id: sideNav
            objectName: "sideNav"
            Layout.fillHeight: true
            Layout.preferredWidth: 240

            // LSideNav.currentPage is a `property string` (LSideNav.qml:10);
            // binding it directly to the int Navigator.adminPage enum would
            // silently coerce to "0"/"1"/"2" and the delegate's strict ===
            // match (LSideNav.qml:44) would never match any item's string
            // `page` key, so no row would ever highlight. Map to string keys
            // here instead — LSideNav itself stays untouched, and this
            // matches the string keys Task 9 already built/tested it with.
            currentPage: Navigator.adminPage === Navigator.Search    ? "search"
                       : Navigator.adminPage === Navigator.VisitLogs ? "visitlogs"
                       : "dashboard"
            items: [
                { page: "dashboard", label: qsTr("Dashboard"),  enabled: true },
                { page: "search",    label: qsTr("Search"),     enabled: true },
                { page: "visitlogs", label: qsTr("Visit Logs"), enabled: true },
                { page: "database",  label: qsTr("Database"),   enabled: false },
                { page: "reporting", label: qsTr("Reporting"),  enabled: false },
                { page: "settings",  label: qsTr("Settings"),   enabled: false }
            ]
            // Every key is matched explicitly, including "dashboard". A bare
            // `else -> Dashboard` fallthrough would silently route ANY
            // unrecognized key to the Dashboard: harmless today (LSideNav's
            // own guard blocks the disabled Phase-4 items before they ever
            // emit), but the moment Phase 4 enables "database"/"reporting"/
            // "settings" they would land on the Dashboard instead of failing
            // loudly. Warn rather than route. Phase 4 adds the real cases.
            onPageActivated: function(page) {
                if (page === "dashboard")
                    Navigator.showAdminPage(Navigator.Dashboard)
                else if (page === "search")
                    Navigator.showAdminPage(Navigator.Search)
                else if (page === "visitlogs")
                    Navigator.showAdminPage(Navigator.VisitLogs)
                else
                    console.warn("AdminScreen: no route for page key", page)
            }
            header: LSidebarBrand {
                schoolName: schoolInfoVm.schoolName
                logoUrl: schoolInfoVm.logoUrl
                hasLogo: schoolInfoVm.hasLogo
            }
            footer: LButton {
                objectName: "backToKioskButton"
                text: qsTr("← Back to Kiosk")
                onClicked: Navigator.showKiosk()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            LPageHeader {
                id: pageHeader
                objectName: "pageHeader"
                Layout.fillWidth: true
                Layout.margins: Theme.spacing.xxl
                Layout.bottomMargin: 0
                title: admin.pageTitle
                dateText: admin.dateText
                clockText: admin.clockText
            }

            Loader {
                id: pageLoader
                objectName: "pageLoader"
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: {
                    switch (Navigator.adminPage) {
                    case Navigator.Search:    return searchComponent;
                    case Navigator.VisitLogs: return visitLogsComponent;
                    default:                  return dashboardComponent;
                    }
                }
                // Fetch once when a page is shown (spec §5.1 "fetch on
                // navigation"). Each of the three VMs exposes Q_INVOKABLE
                // refresh(); gated by autoLoad so tests can suppress network.
                onLoaded: if (admin.autoLoad && item && item.vm && item.vm.refresh)
                              item.vm.refresh()
            }
        }
    }

    Component { id: dashboardComponent; DashboardScreen { objectName: "dashboardPage"; vm: dashboardVm } }
    Component { id: searchComponent;    SearchScreen    { objectName: "searchPage";    vm: searchVm } }
    Component { id: visitLogsComponent; VisitLogsScreen { objectName: "visitLogsPage"; vm: visitLogsVm } }
}
