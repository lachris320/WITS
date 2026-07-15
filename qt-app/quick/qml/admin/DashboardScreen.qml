import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Dashboard (spec §6.1). Fetches once on load; manual refresh; states dimmed
// while loading, inline error+retry, empty-state when there is no data.
Rectangle {
    id: screen
    property var vm

    // Introspection for tests.
    readonly property string peakShown: vm ? vm.peakHourLabel : ""

    color: Theme.appBackground

    // Initial fetch is triggered by AdminScreen's Loader.onLoaded (gated by
    // AdminScreen.autoLoad), NOT here — so this screen instantiated directly
    // (the standalone QuickTest, the shell smoke test) issues no network unless
    // a caller opts in. The Retry button still calls vm.refresh() explicitly.

    ColumnLayout {
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
            LStatTile { Layout.fillWidth: true; label: qsTr("Peak Hour")
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
        anchors.centerIn: parent
        visible: vm && vm.errorText.length > 0
        spacing: Theme.spacing.md
        Text {
            text: vm ? vm.errorText : ""
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }
        LButton { text: qsTr("Retry"); onClicked: if (vm) vm.refresh() }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: vm && vm.loading
        visible: running
    }
}
