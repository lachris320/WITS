import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Maroon brand sidebar (design left aside). Binds a KioskViewModel via `vm`.
Rectangle {
    id: panel
    property var vm
    // Test/consumer seams:
    property alias idText: idField.text
    readonly property bool guestButtonVisible: guestBtn.visible
    function submit() { if (vm) vm.submitLogin(idField.text); idField.clear() }

    implicitWidth: 390
    clip: true
    gradient: LKioskGradient {}

    // The reference (Library Kiosk v2.dc.html ~L30) lays the three blocks out
    // as a vertically-centered group with an even 28px gap between them
    // (justify-content:center; gap:28px), not one pinned to the bottom by a
    // fillHeight spacer. Theme.spacing.xxxl (28) matches that gap exactly.
    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.spacing.xxxl
        anchors.rightMargin: Theme.spacing.xxxl
        spacing: Theme.spacing.xxxl

        // Brand block: logo ON TOP of the titles, stacked vertically and
        // left-aligned (reference brandDir:'column' in wide mode, Library
        // Kiosk v2.dc.html ~L34). A horizontal logo-beside-title layout made
        // the block logo(96) + the wide serif pageTitle title exceed the
        // 390px panel content width, so with the panel's clip the title was
        // cut off at the right edge. Stacking narrows the block to the
        // title's own width, fitting inside 390px with margin.
        ColumnLayout {
            spacing: Theme.spacing.xl
            Rectangle {
                Layout.preferredWidth: 96; Layout.preferredHeight: 96
                radius: width / 2
                color: Theme.card
                border.width: 3
                border.color: Theme.secondary
                // Logo image lands in Phase 4 (admin logo import); circle placeholder now.
                Text {
                    anchors.centerIn: parent
                    text: qsTr("LOGO"); color: Theme.mutedText
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                }
            }
            ColumnLayout {
                spacing: Theme.spacing.xs
                Text {
                    text: qsTr("Library Attendance")
                    color: Theme.brand.onKiosk
                    font.family: Theme.typography.serif
                    font.pixelSize: Theme.typography.pageTitle
                    font.weight: Font.Bold
                }
                Text {
                    text: (panel.vm ? panel.vm.schoolName : "") + " · "
                          + (panel.vm ? panel.vm.schoolAddress : "")
                    color: Theme.onBrandMuted
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                    wrapMode: Text.WordWrap
                    Layout.maximumWidth: 240
                }
            }
        }

        // Scan/type block. Capped at 340px (reference's max-width:340px,
        // Library Kiosk v2.dc.html ~L42) and centered so the field/buttons sit
        // inside the panel with margin instead of touching/overflowing the edge.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 340   // reference max-width; no Theme token at this scale
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacing.sm
            RowLayout {
                spacing: Theme.spacing.sm
                LPulseDot { color: Theme.secondary; pulseDuration: 900 }
                LEyebrow {
                    text: qsTr("SCAN OR TYPE YOUR ID")
                    color: Theme.secondary
                    font.letterSpacing: 1.6
                }
            }
            TextField {
                id: idField
                Layout.fillWidth: true
                placeholderText: qsTr("ID number…")
                color: Theme.brand.onKiosk
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.cardTitle
                // Admin keys are alphabetic -> mask; student IDs are digits -> show.
                echoMode: (text.length > 0 && !/^[0-9]/.test(text))
                          ? TextInput.Password : TextInput.Normal
                background: Rectangle {
                    radius: Theme.radius.md
                    color: Qt.alpha(Theme.card, 0.10)
                    border.width: 1
                    border.color: idField.activeFocus
                                  ? Theme.secondary : Qt.alpha(Theme.card, 0.28)
                }
                onAccepted: panel.submit()
                Accessible.role: Accessible.EditableText
                Accessible.name: qsTr("ID number")
            }
            LButton {
                Layout.fillWidth: true
                variant: "Accent"
                text: qsTr("LOG IN →")
                onClicked: panel.submit()
            }
            LButton {
                id: guestBtn
                Layout.fillWidth: true
                variant: "Outline"
                text: qsTr("Guest log in")
                visible: panel.vm ? panel.vm.guestEnabled : false
                onClicked: if (panel.vm) panel.vm.requestGuest()
            }
        }

        // Clock block.
        ColumnLayout {
            spacing: Theme.spacing.xs
            RowLayout {
                spacing: Theme.spacing.sm
                Text {
                    text: panel.vm ? panel.vm.clockTime : ""
                    color: Theme.brand.onKiosk
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.statValue
                    font.weight: Font.ExtraBold
                }
                Text {
                    text: panel.vm ? panel.vm.clockMeridiem : ""
                    color: Theme.secondary
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.cardTitle
                    Layout.alignment: Qt.AlignBottom
                }
            }
            Text {
                text: (panel.vm ? panel.vm.clockDate : "") + qsTr("  ·  Open ")
                      + (panel.vm ? panel.vm.libraryHours : "")
                color: Theme.onBrandMuted
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }
    }
}
