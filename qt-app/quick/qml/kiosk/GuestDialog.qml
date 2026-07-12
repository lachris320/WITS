import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Guest sign-in modal. Show with show(); binds a GuestViewModel via `vm`.
LDialog {
    id: dlg
    property var vm
    title: qsTr("Guest Sign-In")

    function show() { clearFields(); visible = true; nameField.forceActiveFocus() }
    function hide() { visible = false }
    function clearFields() {
        nameField.text = ""; contactField.text = "";
        companyField.text = ""; purposeField.text = "";
    }

    // Inline component (Qt 6.3+): each row is a real object with a `text`
    // alias — no objectName / Component.onCompleted handle-juggling.
    component Field: ColumnLayout {
        property alias text: input.text
        property string label
        Layout.fillWidth: true
        spacing: Theme.spacing.xs
        Text {
            text: label; color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }
        TextField {
            id: input
            Layout.fillWidth: true
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            color: Theme.text
            background: Rectangle {
                radius: Theme.radius.sm; color: Theme.card
                border.width: 1; border.color: Theme.border
            }
        }
    }

    ColumnLayout {
        // width (not Layout.fillWidth): this ColumnLayout lands inside
        // LDialog's plain `Item body` via the default content alias, not
        // inside a Layout — Layout.fillWidth is a no-op there. Bind directly
        // to body's width (exposed here as `parent.width`) so the guest
        // fields stretch to the card instead of rendering at implicit width.
        width: parent.width
        spacing: Theme.spacing.md

        Field { id: nameField;    label: qsTr("Full name *") }
        Field { id: contactField; label: qsTr("Contact") }
        Field { id: companyField; label: qsTr("Company *") }
        Field { id: purposeField; label: qsTr("Purpose *") }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.sm
            LButton { variant: "Ghost"; text: qsTr("Cancel"); onClicked: dlg.hide() }
            LButton {
                variant: "Accent"; text: qsTr("Submit")
                onClicked: if (dlg.vm) dlg.vm.submitGuest(
                    nameField.text, contactField.text,
                    companyField.text, purposeField.text)
            }
        }
    }

    Connections {
        target: dlg.vm
        function onGuestSucceeded(message) { dlg.hide() }
        // onGuestFailed surfaces via the kiosk toast (wired in Task 10).
    }
}
