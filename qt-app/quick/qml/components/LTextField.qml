import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Labelled single-line text field (§11 form primitive). Colors/spacing/radius
// come only from Theme (§12.8) — zero raw hex. Two-way `text` alias so a screen
// can bind `text: vm.schoolName` and read edits back. isPassword flips the
// echo mode for the admin-key entry.
ColumnLayout {
    id: root
    property alias text: input.text
    property string label: ""
    property string placeholder: ""
    property bool isPassword: false
    spacing: Theme.spacing.xs

    Text {
        visible: root.label.length > 0
        text: root.label
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.control
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        radius: Theme.radius.sm
        color: Theme.card
        border.width: 2
        border.color: input.activeFocus ? Theme.brand.admin : Theme.border

        TextInput {
            id: input
            objectName: "fieldInput"
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing.md
            anchors.rightMargin: Theme.spacing.md
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            color: Theme.text
            selectionColor: Theme.brand.admin
            selectedTextColor: Theme.brand.onPrimary
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            echoMode: root.isPassword ? TextInput.Password : TextInput.Normal

            Text {
                anchors.fill: parent
                anchors.leftMargin: 0
                verticalAlignment: Text.AlignVCenter
                visible: input.text.length === 0 && root.placeholder.length > 0
                text: root.placeholder
                color: Theme.mutedTextCaption
                font: input.font
            }
        }
    }
    Accessible.role: Accessible.EditableText
    Accessible.name: root.label
}
