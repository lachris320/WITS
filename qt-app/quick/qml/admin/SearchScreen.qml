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
                id: resultCard
                required property var model
                width: ListView.view ? ListView.view.width : 0
                // LCard's own padding (Theme.spacing.xl) already insets the
                // delegate content; no extra anchors.margins here.
                //
                // Height derivation: the row's tallest element is the
                // circular avatar, not the two-line text stack beside it
                // (name ~21px + xs gap + the merged ID/course/department
                // line ~17px ≈ 42px, comfortably under the 44px avatar), so
                // the card only needs avatarDiameter + top/bottom padding —
                // no separate magic total. This is meaningfully shorter than
                // the old fixed 132 (four stacked text rows, one of which
                // was "Total Visits") now that visits moved beside the name
                // instead of under it — more students fit on screen at once.
                readonly property int avatarDiameter: 44
                implicitHeight: avatarDiameter + Theme.spacing.xl * 2

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    // Circle avatar. No student photo data exists yet (spec
                    // owner decision, Phase 3: search_students.php returns
                    // no photo column) — initials-only, same chip pattern as
                    // the kiosk login feed (KioskFeedRow.qml), recolored for
                    // the admin surface. Swapping in a real photo later only
                    // means adding an Image behind this Text, not a redesign.
                    Rectangle {
                        Layout.preferredWidth: resultCard.avatarDiameter
                        Layout.preferredHeight: resultCard.avatarDiameter
                        Layout.alignment: Qt.AlignVCenter
                        radius: width / 2
                        color: Theme.brand.adminSoft
                        Text {
                            objectName: "avatarInitials"
                            anchors.centerIn: parent
                            text: model.initials
                            color: Theme.brand.admin
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.control
                            font.weight: Font.ExtraBold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Theme.spacing.xs
                        Text {
                            Layout.fillWidth: true
                            text: model.name
                            color: Theme.text
                            elide: Text.ElideRight
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.cardTitle
                        }
                        // Same information as before (ID, course, department)
                        // on one line instead of two separate rows — the ID
                        // itself is unchanged (spec keeps it rendered).
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("ID: %1 · %2 · %3").arg(model.schoolId).arg(model.course).arg(model.department)
                            color: Theme.mutedText
                            elide: Text.ElideRight
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
                    }

                    // "Total Visits: N" — frozen label (spec §6.2), moved to
                    // the side. RowLayout naturally pushes it to the far
                    // right since the middle column above fills the
                    // remaining width.
                    Text {
                        Layout.alignment: Qt.AlignVCenter
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
