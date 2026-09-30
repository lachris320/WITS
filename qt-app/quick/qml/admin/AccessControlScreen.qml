import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Access Control admin page (Sub-plan 4). Two data sources, deliberately split:
//  - `hub` (default: the app-global AccessControl singleton) — LIVE state:
//    the monitoring-intent toggle, the feed connection state and contact age.
//  - `vm` (page-scoped AccessControlViewModel) — the admin-authenticated
//    SNAPSHOT of access_recent.php: recent-entries table, counts, "Updated".
// Both are injectable so QuickTests drive plain-QML stubs. The initial fetch is
// AdminScreen's Loader.onLoaded feature-detected vm.refresh() (never here), so a
// directly-instantiated screen issues no network. Every color is a Theme token.
Rectangle {
    id: screen
    property var vm
    property var hub: AccessControl
    // Presentation clock for the contact-age tile (refinement 4): a plain
    // property so tests can pin "now"; the ageTimer below advances it.
    property var now: new Date()

    readonly property bool monitoringOn: hub ? hub.accessEnabled === true : false
    readonly property bool enableLocked: hub ? hub.enableLocked === true : false
    readonly property int connectionState: hub ? hub.connectionState : 0
    readonly property string connectionLabel: stateLabel(connectionState)
    // Stateless pure formatter on the singleton; the INPUTS come from `hub`
    // (possibly a stub) and the advancing presentation clock.
    readonly property string contactAgeText: hub
        ? AccessControl.contactAgeText(monitoringOn, hub.lastContactAt, now)
        : ""

    // ConnectionState ints (pinned by tst_accesscontrolhub): 0..4.
    function stateLabel(s) {
        switch (s) {
        case 1:  return qsTr("Connecting");
        case 2:  return qsTr("Connected");
        case 3:  return qsTr("Degraded");
        case 4:  return qsTr("Error");
        default: return qsTr("Disconnected");
        }
    }

    // Empty feed vs failed initial load vs auth loss are distinct states
    // (refinement 6) — never conflate them in the table's empty text.
    function tableEmptyText() {
        if (!vm)
            return "";
        if (vm.authFailure)
            return qsTr("Admin authentication failed — re-enter via admin login.");
        if (vm.initialLoadFailed)
            return qsTr("Could not load the access feed. Use Refresh to retry.");
        if (vm.loading && vm.updatedAt === "")
            return qsTr("Loading…");
        // Only a SUCCESSFUL load that returned zero rows is "No entries yet";
        // before any load (or with rows present) there is nothing to announce.
        if (vm.emptyFeed)
            return qsTr("No entries yet");
        return "";
    }

    color: Theme.appBackground

    Timer {
        id: ageTimer
        objectName: "ageTimer"
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: screen.visible && screen.monitoringOn
        onTriggered: screen.now = new Date()
    }

    ColumnLayout {
        id: content
        objectName: "accessContent"
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.xl

        // --- Live: monitoring intent -------------------------------------
        LCard {
            id: monitoringCard
            objectName: "monitoringCard"
            Layout.fillWidth: true
            implicitHeight: monitoringColumn.implicitHeight + 2 * monitoringCard.padding

            ColumnLayout {
                id: monitoringColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: Theme.spacing.sm

                LCheckbox {
                    id: monitorToggle
                    objectName: "monitorToggle"
                    Layout.fillWidth: true
                    label: qsTr("Enable turnstile monitoring")
                    // The control never holds its own diverging state: it
                    // mirrors the hub's persisted INTENT.
                    checked: screen.monitoringOn
                    enabled: screen.hub ? !screen.enableLocked : false
                    onToggled: function(checked) {
                        if (screen.hub)
                            screen.hub.setAccessEnabled(checked)
                        // LCheckbox's MouseArea assigns `checked` imperatively,
                        // which destroys the binding above. Re-assert it so a
                        // refused/locked call snaps back to the hub's value and
                        // later hub changes keep flowing in.
                        monitorToggle.checked = Qt.binding(function() { return screen.monitoringOn })
                    }
                }
                Text {
                    objectName: "monitoringHelp"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    text: qsTr("Controls this app's turnstile event polling and kiosk display. It does not disable the physical gate or stop server-side attendance recording.")
                    color: Theme.mutedText
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.body
                }
                Text {
                    objectName: "lockedNote"
                    Layout.fillWidth: true
                    visible: screen.enableLocked
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    text: qsTr("Monitoring is forced on for this PC by the WITS_ACCESS_CONTROL environment setting, so it can't be turned off here.")
                    color: Theme.error
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.body
                }
            }
        }

        // --- Tiles: snapshot counts (vm) + live feed health (hub) --------
        GridLayout {
            Layout.fillWidth: true
            columns: screen.width < 900 ? 2 : 4
            columnSpacing: Theme.spacing.lg
            rowSpacing: Theme.spacing.lg
            LStatTile {
                objectName: "entriesTodayTile"
                Layout.fillWidth: true
                variant: "Hero"
                label: qsTr("Entries Today")
                value: screen.vm ? String(screen.vm.entriesToday) : "0"
            }
            LStatTile {
                objectName: "lastEntryTile"
                Layout.fillWidth: true
                label: qsTr("Last Entry")
                value: screen.vm && screen.vm.lastEntryAt !== "" ? screen.vm.lastEntryAt : "—"
            }
            LStatTile {
                objectName: "connectionTile"
                Layout.fillWidth: true
                label: qsTr("Feed Connection")
                value: screen.connectionLabel
                caption: qsTr("App ↔ backend feed, not gate hardware")
            }
            LStatTile {
                objectName: "contactTile"
                Layout.fillWidth: true
                label: qsTr("Feed Contact")
                value: screen.contactAgeText
                caption: qsTr("Live — independent of the table below")
            }
        }

        // --- Snapshot header: "Updated" is separate from live contact ----
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            Text {
                text: qsTr("Recent entries")
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.cardTitle
            }
            Text {
                objectName: "updatedLabel"
                textFormat: Text.PlainText
                text: screen.vm && screen.vm.updatedAt !== ""
                      ? qsTr("Updated %1").arg(screen.vm.updatedAt)
                      : qsTr("Not loaded yet")
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
            }
            Rectangle {
                objectName: "staleBadge"
                visible: screen.vm ? screen.vm.stale === true : false
                radius: Theme.radius.pill
                color: Theme.errorSoft
                border.width: 1
                border.color: Theme.errorBorder
                implicitWidth: staleText.implicitWidth + 2 * Theme.spacing.sm
                implicitHeight: staleText.implicitHeight + Theme.spacing.xs
                Text {
                    id: staleText
                    anchors.centerIn: parent
                    text: qsTr("Stale — last refresh failed")
                    color: Theme.error
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                }
            }
            Item { Layout.fillWidth: true }
            LButton {
                objectName: "refreshButton"
                text: screen.vm && screen.vm.loading ? qsTr("Refreshing…") : qsTr("Refresh")
                enabled: screen.vm ? !screen.vm.loading : false
                onClicked: if (screen.vm) screen.vm.refresh()
            }
        }

        Text {
            objectName: "authError"
            Layout.fillWidth: true
            visible: screen.vm ? screen.vm.authFailure === true : false
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            text: qsTr("Admin authentication failed — re-enter via admin login.")
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }

        LTable {
            id: entriesTable
            objectName: "entriesTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: screen.vm ? screen.vm.entries : null
            emptyStateText: screen.tableEmptyText()
            columns: [
                { key: "createdAt",  title: qsTr("Time"),       weight: 1.4 },
                { key: "name",       title: qsTr("Name"),       weight: 2 },
                { key: "schoolId",   title: qsTr("School ID"),  weight: 1.2 },
                { key: "course",     title: qsTr("Course"),     weight: 1.2 },
                { key: "department", title: qsTr("Department"), weight: 1.2 },
                { key: "reader",     title: qsTr("Lane"),       weight: 0.6 },
                { key: "card",       title: qsTr("Card"),       weight: 1 }
            ]
        }
    }

    LToast {
        id: accessToast
        objectName: "accessToast"
        severity: "Error"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.xxl
    }

    // NOT a declarative `message: vm.errorText` binding — LToast's auto-dismiss
    // Timer sets message="" imperatively, which would permanently destroy such
    // a binding after the first toast (the trap documented in KioskScreen.qml /
    // DatabaseScreen.qml). Raise imperatively on every non-empty change.
    Connections {
        target: screen.vm ? screen.vm : null
        function onErrorTextChanged() {
            if (screen.vm.errorText !== "")
                accessToast.message = screen.vm.errorText
        }
    }
}
