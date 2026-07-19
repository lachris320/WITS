import QtQuick
import QtQuick.Layouts
import LOAMS

// Tiered destructive-op confirm (§3.4). tier 1 = title + message +
// Cancel/Confirm. tier 2 = also an admin-key field, and Confirm stays disabled
// until a key is typed. Confirm is disabled while `busy` (request in flight) in
// BOTH tiers. Emits confirmed(key) — key is the typed admin key for tier 2,
// "" for tier 1 (spec §4.1: tier-2 sends a freshly re-typed key).
LDialog {
    id: root
    property int tier: 1
    property bool busy: false
    property string confirmText: qsTr("Confirm")
    property string cancelText: qsTr("Cancel")
    signal confirmed(string key)
    signal cancelled()

    // Tier 1 must have NO key-field object in the tree at all (not merely a
    // hidden one) — a QuickTest findChild() searches the object tree
    // regardless of `visible`, so a Loader that only instantiates the field
    // for tier 2 is required, not a plain LTextField gated by `visible`.
    readonly property bool keyReady: tier !== 2
        || (keyFieldLoader.item !== null && keyFieldLoader.item.text.trim().length > 0)

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.lg

        Loader {
            id: keyFieldLoader
            Layout.fillWidth: true
            active: root.tier === 2
            sourceComponent: LTextField {
                objectName: "confirmKeyField"
                label: qsTr("Re-enter Admin Key")
                isPassword: true
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "cancelButton"
                variant: "Outline"
                text: root.cancelText
                enabled: !root.busy
                onClicked: { root.visible = false; root.cancelled(); }
            }
            LButton {
                objectName: "confirmButton"
                variant: root.tier === 2 ? "Danger" : "Primary"
                text: root.confirmText
                enabled: !root.busy && root.keyReady
                onClicked: root.confirmed(root.tier === 2 ? keyFieldLoader.item.text : "")
            }
        }
    }
}
