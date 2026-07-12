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
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.brand.kiosk }
        GradientStop { position: 1.0; color: Theme.brand.kioskHover }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxxl
        spacing: Theme.spacing.xxl

        // Brand block: logo + titles.
        RowLayout {
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

        // Scan/type block.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            RowLayout {
                spacing: Theme.spacing.sm
                Rectangle {
                    width: 7; height: 7; radius: 3.5; color: Theme.secondary
                    SequentialAnimation on opacity {
                        running: Theme.motion.enabled; loops: Animation.Infinite
                        NumberAnimation { to: 1.0; duration: 900 }
                        NumberAnimation { to: 0.45; duration: 900 }
                    }
                }
                Text {
                    text: qsTr("SCAN OR TYPE YOUR ID")
                    color: Theme.secondary
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                    font.weight: Font.ExtraBold
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

        Item { Layout.fillHeight: true }   // push the clock to the bottom

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
