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

    // Keyboard-operable: Tab reaches it (an Item is not tab-focusable by
    // default) and Space/Enter/Return toggle exactly like a click. A disabled
    // item never takes or acts on focus, so keys can't toggle it either.
    activeFocusOnTab: true

    // Single toggle path shared by the mouse, the keyboard and assistive tech.
    function toggle() {
        if (!root.enabled)
            return;
        root.checked = !root.checked;
        root.toggled(root.checked);
    }

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
            // Focus ring: an outline just outside the box, brand token only.
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: Theme.radius.sm2 + 2
                color: "transparent"
                border.width: 2
                border.color: Theme.brand.base
                visible: root.activeFocus
            }
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
        onClicked: root.toggle()
    }
    // Same semantics as a Qt Quick Controls AbstractButton: key auto-repeat is
    // ignored (holding a key must not machine-gun toggled()), Space acts on
    // RELEASE of a press that began here, Enter/Return act on the first press.
    property bool m_spaceDown: false
    onActiveFocusChanged: if (!activeFocus) m_spaceDown = false
    onEnabledChanged: if (!enabled) m_spaceDown = false

    Keys.onPressed: function(event) {
        if (!root.enabled)
            return;
        if (event.key === Qt.Key_Space) {
            if (!event.isAutoRepeat)
                root.m_spaceDown = true;
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (!event.isAutoRepeat)
                root.toggle();
            event.accepted = true;
        }
    }
    Keys.onReleased: function(event) {
        if (!root.enabled || event.key !== Qt.Key_Space)
            return;
        if (!event.isAutoRepeat && root.m_spaceDown) {
            root.m_spaceDown = false;
            root.toggle();
        }
        event.accepted = true;
    }
    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    Accessible.checked: root.checked
    Accessible.focusable: true
    Accessible.onPressAction: root.toggle()
    Accessible.onToggleAction: root.toggle()
}
