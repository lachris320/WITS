import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Dashboard (spec §6.1). Manual refresh; content dimmed while loading, with
// an inline error+retry state. See the note above the ColumnLayout for why the
// initial fetch is NOT issued here.
Rectangle {
    id: screen
    property var vm

    // Introspection for tests: reflects the rendered tile rather than
    // recomputing its binding, so this cannot silently pass if the tile's own
    // value binding is broken or removed.
    readonly property string peakShown: peakTile.value

    color: Theme.appBackground

    // Initial fetch is triggered by AdminScreen's Loader.onLoaded (gated by
    // AdminScreen.autoLoad), NOT here — so this screen instantiated directly
    // (the standalone QuickTest, the shell smoke test) issues no network unless
    // a caller opts in. The Retry button still calls vm.refresh() explicitly.

    ColumnLayout {
        id: content
        objectName: "dashContent"
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.xl
        opacity: (vm && vm.loading) ? 0.5 : 1.0

        // Stat cards.
        GridLayout {
            Layout.fillWidth: true
            columns: screen.width < 900 ? 2 : 4
            columnSpacing: Theme.spacing.lg
            rowSpacing: Theme.spacing.lg
            LStatTile { Layout.fillWidth: true; label: qsTr("Visitors Today")
                        value: vm ? vm.statToday.toString() : "0"; variant: "Hero" }
            LStatTile { Layout.fillWidth: true; label: qsTr("This Week")
                        value: vm ? vm.statWeek.toString() : "0" }
            LStatTile { Layout.fillWidth: true; label: qsTr("Registered Students")
                        value: vm ? vm.statStudents.toString() : "0" }
            LStatTile { id: peakTile; objectName: "peakTile"
                        Layout.fillWidth: true; label: qsTr("Peak Hour")
                        value: vm ? vm.peakHourLabel : "—" }
        }

        // Charts.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.xl
            LCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("Visitors by Hour"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    LBarChart {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        orientation: "Vertical"
                        model: vm ? vm.hourlyModel : null
                        maxValue: vm && vm.hourlyModel ? vm.hourlyModel.maxValue : 1
                        highlightIndex: vm ? vm.peakHourIndex : -1
                    }
                }
            }
            LCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Text { text: qsTr("By Department"); color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    LBarChart {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        orientation: "Horizontal"
                        model: vm ? vm.departmentModel : null
                        maxValue: vm && vm.departmentModel ? vm.departmentModel.maxValue : 1
                    }
                }
            }
        }
    }

    // Error + retry (spec §7.3): a failed fetch never renders as empty.
    ColumnLayout {
        id: errorBlock
        objectName: "errorBlock"
        anchors.centerIn: parent
        // Ternary, not `vm && ...`: with no vm the latter evaluates to
        // undefined, and a binding yielding undefined leaves a bool property at
        // its default — which for `visible` is true, flashing an empty error
        // block with a Retry button over the dashboard.
        visible: vm ? vm.errorText.length > 0 : false
        spacing: Theme.spacing.md
        Text {
            text: vm ? vm.errorText : ""
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }
        LButton { objectName: "retryButton"; text: qsTr("Retry")
                  onClicked: if (vm) vm.refresh() }
    }

    BusyIndicator {
        id: busy
        objectName: "busyIndicator"
        anchors.centerIn: parent
        // Ternary for the same reason as errorBlock.visible above:
        // BusyIndicator.running also defaults to true.
        running: vm ? vm.loading : false
        visible: running
    }
}
