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
    SettingsViewModel  { id: settingsVm }

    // Read-only school identity (logo + name) for the sidebar brand block.
    // Reads QSettings at construction and again on demand via reload().
    SchoolInfoViewModel { id: schoolInfoVm }

    // Phase 4c's Settings screen is the FIRST code that writes school/name and
    // school/logoPath, so the sidebar brand can now go stale mid-session: a
    // rename or logo import re-coloured the palette live while the sidebar
    // kept the old name/logo until restart — a visibly half-applied change.
    // settingsVm.save() is the only moment those keys move (importLogo()
    // deliberately does NOT persist; see SettingsViewModel.cpp), so one
    // handler covers both. reload() is signal-quiet when nothing this VM reads
    // changed, so an unrelated save costs nothing. Named function so the
    // shell QuickTest can drive the same path a save takes.
    function reloadSchoolInfo() { schoolInfoVm.reload() }
    Connections {
        target: settingsVm
        function onSaved() { admin.reloadSchoolInfo() }
    }

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
        case Navigator.Database:   return qsTr("Database");
        case Navigator.Reporting:  return qsTr("Reporting");
        case Navigator.Settings:   return qsTr("Settings");
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
                       : Navigator.adminPage === Navigator.Database  ? "database"
                       : Navigator.adminPage === Navigator.Reporting ? "reporting"
                       : Navigator.adminPage === Navigator.Settings  ? "settings"
                       : "dashboard"
            items: [
                { page: "dashboard", label: qsTr("Dashboard"),  enabled: true },
                { page: "search",    label: qsTr("Search"),     enabled: true },
                { page: "visitlogs", label: qsTr("Visit Logs"), enabled: true },
                { page: "database",  label: qsTr("Database"),   enabled: true },
                { page: "reporting", label: qsTr("Reporting"),  enabled: true },
                { page: "settings",  label: qsTr("Settings"),   enabled: true }
            ]
            // Every key is matched explicitly, including "dashboard". A bare
            // `else -> Dashboard` fallthrough would silently route ANY
            // unrecognized key to the Dashboard: harmless today (LSideNav's
            // own guard blocks any disabled items before they ever emit),
            // but a typo'd key would otherwise land on the Dashboard instead
            // of failing loudly. Warn rather than route.
            onPageActivated: function(page) {
                if (page === "dashboard")
                    Navigator.showAdminPage(Navigator.Dashboard)
                else if (page === "search")
                    Navigator.showAdminPage(Navigator.Search)
                else if (page === "visitlogs")
                    Navigator.showAdminPage(Navigator.VisitLogs)
                else if (page === "database")
                    Navigator.showAdminPage(Navigator.Database)
                else if (page === "reporting")
                    Navigator.showAdminPage(Navigator.Reporting)
                else if (page === "settings")
                    Navigator.showAdminPage(Navigator.Settings)
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
                    case Navigator.Database:  return databaseComponent;
                    case Navigator.Reporting: return reportingComponent;
                    case Navigator.Settings:  return settingsComponent;
                    default:                  return dashboardComponent;
                    }
                }
                // Fetch once when a page is shown (spec §5.1 "fetch on
                // navigation"). Each of the three VMs exposes Q_INVOKABLE
                // refresh(); gated by autoLoad so tests can suppress network.
                // Settings has no refresh(): it reads QSettings via load() and
                // fetches the reset-visits department list via loadDepartments().
                // Each call is feature-detected, so a screen (or a QuickTest
                // stub VM) that lacks the method is simply skipped — the
                // screens themselves deliberately have no Component.onCompleted
                // fetch, which is what keeps stub-driven QuickTests offline.
                onLoaded: {
                    if (admin.autoLoad && item && item.vm && item.vm.refresh)
                        item.vm.refresh()
                    if (admin.autoLoad && item && item.vm && item.vm.load)
                        item.vm.load()
                    if (admin.autoLoad && item && item.vm && item.vm.loadDepartments)
                        item.vm.loadDepartments()
                }
            }
        }
    }

    Component { id: dashboardComponent; DashboardScreen { objectName: "dashboardPage"; vm: dashboardVm } }
    Component { id: searchComponent;    SearchScreen    { objectName: "searchPage";    vm: searchVm } }
    Component { id: visitLogsComponent; VisitLogsScreen { objectName: "visitLogsPage"; vm: visitLogsVm } }
    Component { id: databaseComponent;  DatabaseScreen  { objectName: "databasePage" } }
    Component { id: reportingComponent; ReportingScreen { objectName: "reportingPage" } }
    Component { id: settingsComponent;  SettingsScreen  { objectName: "settingsPage";  vm: settingsVm } }
}
