import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Visit Logs (spec §6.3). Student attendance is the primary/default view; Guest
// is a secondary toggle. The LTable column set switches on vm.mode; the student
// Time Out column is always "—" (login-only — the backend has no logout event).
Rectangle {
    id: screen
    property var vm

    readonly property var studentColumns: [
        { key: "date",       title: qsTr("Date") },
        { key: "name",       title: qsTr("Name") },
        { key: "course",     title: qsTr("Course") },
        { key: "department", title: qsTr("Department") },
        { key: "timeIn",     title: qsTr("Time In") },
        { key: "timeOut",    title: qsTr("Time Out") }
    ]
    readonly property var guestColumns: [
        { key: "name",    title: qsTr("Name") },
        { key: "company", title: qsTr("Company") },
        { key: "purpose", title: qsTr("Purpose") },
        { key: "timeIn",  title: qsTr("Time In") }
    ]
    readonly property bool isStudent: !vm || vm.mode === VisitLogsViewModel.Student
    readonly property var activeColumns: isStudent ? studentColumns : guestColumns
    readonly property int activeColumnCount: activeColumns.length

    color: Theme.appBackground

    // Initial fetch is triggered by AdminScreen's Loader.onLoaded (gated by
    // AdminScreen.autoLoad), NOT here — so this screen instantiated directly
    // (the standalone QuickTest, the shell smoke test) issues no network unless
    // a caller opts in. The Retry button still calls vm.refresh() explicitly.

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // Controls row: Student/Guest mode (student primary) + Today/Week range.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.lg
            LSegmented {
                objectName: "modeSegmented"
                options: [ { value: VisitLogsViewModel.Student, label: qsTr("Students") },
                           { value: VisitLogsViewModel.Guest,   label: qsTr("Guests") } ]
                currentValue: vm ? vm.mode : VisitLogsViewModel.Student
                onSelectionChanged: function(value) { if (vm) vm.mode = value }
            }
            Item { Layout.fillWidth: true }
            Text {
                objectName: "rangeLabel"
                text: vm ? vm.rangeLabel : ""
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
            }
            LSegmented {
                objectName: "rangeSegmented"
                options: [ { value: VisitLogsViewModel.Today, label: qsTr("Today") },
                           { value: VisitLogsViewModel.Week,  label: qsTr("This Week") } ]
                currentValue: vm ? vm.range : VisitLogsViewModel.Today
                onSelectionChanged: function(value) { if (vm) vm.range = value }
            }
        }

        LTable {
            id: visitsTable
            objectName: "visitsTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: screen.activeColumns
            model: vm ? vm.rows : null
            emptyStateText: qsTr("No visits in this range")
        }
    }

    // Error + retry.
    ColumnLayout {
        id: errorBlock
        objectName: "errorBlock"
        anchors.centerIn: parent
        // Ternary, not `vm && ...`: with no vm the latter evaluates to
        // undefined, and a binding yielding undefined leaves a bool property
        // at its default — which for `visible` is true, flashing an empty
        // error block with a Retry button over the screen.
        visible: vm ? vm.errorText.length > 0 : false
        spacing: Theme.spacing.md
        Text { text: vm ? vm.errorText : ""; color: Theme.error
               font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
        LButton { objectName: "retryButton"; text: qsTr("Retry"); onClicked: if (vm) vm.refresh() }
    }

    BusyIndicator {
        id: busyIndicator
        objectName: "busyIndicator"
        anchors.centerIn: parent
        // Ternary for the same reason as errorBlock.visible above:
        // BusyIndicator.running also defaults to true.
        running: vm ? vm.loading : false
        visible: running
    }
}
