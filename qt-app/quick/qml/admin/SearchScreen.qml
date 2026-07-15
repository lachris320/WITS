import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Search (spec §6.2). Name/ID/course search + course filter chips. Each result
// card shows the lifetime count as an explicit "Total Visits: N" (never bare).
Rectangle {
    id: screen
    property var vm
    property string selectedCourse: ""

    // Introspection for tests: derived from the live model's count, not a
    // test-only mirror, so it cannot silently pass if vm/results is broken.
    readonly property int resultCount: vm && vm.results ? vm.results.count : 0

    color: Theme.appBackground

    function runSearch() { if (vm) vm.search(queryField.text, screen.selectedCourse) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // Query row.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            TextField {
                id: queryField
                objectName: "queryField"
                Layout.fillWidth: true
                placeholderText: qsTr("Search by name, ID, or course")
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                color: Theme.text
                background: Rectangle {
                    radius: Theme.radius.sm
                    color: Theme.card
                    border.width: 1
                    border.color: Theme.border
                }
                onAccepted: screen.runSearch()
            }
            LButton { objectName: "searchButton"; text: qsTr("Search"); onClicked: screen.runSearch() }
        }

        // Course filter chips.
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            Repeater {
                model: vm ? vm.courses : []
                delegate: Rectangle {
                    id: chip
                    required property var modelData
                    objectName: "chip_" + modelData
                    radius: Theme.radius.pill
                    implicitHeight: 28
                    implicitWidth: chipText.implicitWidth + Theme.spacing.xl
                    readonly property bool active: screen.selectedCourse === modelData
                    color: chip.active ? Theme.brand.admin : Theme.card
                    border.width: 2
                    border.color: chip.active ? Theme.brand.admin : Theme.border
                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: chip.modelData
                        color: chip.active ? Theme.brand.onPrimary : Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.control
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            screen.selectedCourse = (screen.selectedCourse === chip.modelData) ? "" : chip.modelData;
                            screen.runSearch();
                        }
                    }
                }
            }
        }

        // Results.
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacing.md
            model: vm ? vm.results : null
            delegate: LCard {
                required property var model
                width: ListView.view ? ListView.view.width : 0
                // LCard's own padding (Theme.spacing.xl) already insets the
                // delegate content; no extra anchors.margins here, or the
                // three text rows overflow implicitHeight (double inset).
                implicitHeight: 110
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacing.xs
                    Text { text: model.name; color: Theme.text
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.cardTitle }
                    Text { text: model.course + " · " + model.department; color: Theme.mutedText
                           font.family: Theme.typography.sans; font.pixelSize: Theme.typography.body }
                    Text {
                        text: qsTr("Total Visits: %1").arg(model.visits)
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        font.weight: Font.DemiBold
                    }
                }
            }
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
        LButton { objectName: "retryButton"; text: qsTr("Retry"); onClicked: screen.runSearch() }
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
