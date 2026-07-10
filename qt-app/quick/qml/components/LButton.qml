import QtQuick
import QtQuick.Controls
import LOAMS

// The one clickable-action primitive (§11). variant selects the fill role;
// colors come only from Theme (§12.8), never local literals.
Button {
    id: control
    property string variant: "Primary"   // Primary | Accent | Outline | Danger | Ghost
    property bool compact: false
    readonly property color fillColor: variant === "Accent" ? Theme.secondary
                                     : variant === "Danger" ? Theme.error
                                     : Theme.brand.admin

    padding: compact ? Theme.spacing.sm : Theme.spacing.md
    font.family: Theme.typography.sans
    font.pixelSize: Theme.typography.control

    background: Rectangle {
        radius: Theme.radius.md
        color: control.variant === "Outline" || control.variant === "Ghost"
               ? "transparent" : control.fillColor
        border.width: control.variant === "Outline" ? 2 : 0
        border.color: Theme.border
    }
    contentItem: Text {
        text: control.text
        color: control.variant === "Outline" || control.variant === "Ghost"
               ? Theme.text : Theme.brand.onPrimary
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Accessible.role: Accessible.Button
    Accessible.name: control.text
}
