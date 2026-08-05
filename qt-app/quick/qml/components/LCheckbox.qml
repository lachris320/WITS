import QtQuick
import QtQuick.Layouts
import LOAMS

// Binary checkbox form primitive (§11): a `checked` box + label, emits
// toggled(bool). Theme-token styling only. Used for BulkEditDialog's per-field
// "change this" toggles (Phase 4a.2b-iii); reusable by the register form.
Item {
    id: root
    property bool checked: false
    property string label: ""
    // `enabled` is inherited from Item (non-final) — do NOT redeclare it; a
    // disabled parent auto-disables the MouseArea, and the bindings below read
    // the inherited value.
    signal toggled(bool checked)

    implicitHeight: Math.max(box.implicitHeight, labelText.implicitHeight)
    implicitWidth: row.implicitWidth

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Theme.spacing.sm
        Rectangle {
            id: box
            implicitWidth: 20; implicitHeight: 20
            radius: Theme.radius.sm2
            color: root.checked ? Theme.brand.base : Theme.card
            border.width: 2
            border.color: root.checked ? Theme.brand.base : Theme.border
            opacity: root.enabled ? 1 : 0.5
            Text {
                anchors.centerIn: parent
                visible: root.checked
                text: "✓"                       // check mark
                color: Theme.brand.on
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }
        Text {
            id: labelText
            text: root.label
            textFormat: Text.PlainText
            color: Theme.text
            opacity: root.enabled ? 1 : 0.5
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
            Layout.fillWidth: true
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onClicked: { root.checked = !root.checked; root.toggled(root.checked); }
    }
    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    Accessible.checked: root.checked
}
