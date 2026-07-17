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

    // Page entrance progress (Phase 3 Task C, matches Task A/B exactly):
    // fades + rises the content column once per navigation (this screen is
    // destroyed/recreated by AdminScreen's Loader on every sidebar click, so
    // Component.onCompleted below fires exactly once per visit). Plain
    // property (not readonly) so tests can drive it directly.
    property real pageInT: 0

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

    // Page entrance (C1): applied only to the content column below (never
    // this root Rectangle — the background must not move — and never
    // errorBlock/busyIndicator, siblings that must appear instantly). Unlike
    // DashboardScreen, this ColumnLayout has no competing opacity binding, so
    // a direct `opacity: screen.pageInT` needs no multiplier. Duration
    // respects the reduce-motion switch; the animation still runs either way
    // (see LTable's row entrance for why duration-zeroing, not
    // `running:`-gating, is the correct pattern for a deliberate invisible
    // start state).
    NumberAnimation {
        id: pageInAnimation
        objectName: "pageInAnimation"
        target: screen
        property: "pageInT"
        to: 1
        duration: Theme.motion.enabled ? Theme.motion.pageIn : 0
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Theme.motion.easing
    }
    Component.onCompleted: pageInAnimation.start()

    ColumnLayout {
        id: contentColumn
        objectName: "contentColumn"
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg
        opacity: screen.pageInT
        transform: Translate { id: contentColumnTranslate; y: (1 - screen.pageInT) * 16 }

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
            // C2: the one consumer that opts into LTable's row entrance.
            animateRows: true
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
