import QtQuick
import QtQuick.Controls
import LOAMS

// The one clickable-action primitive (§11). variant selects the fill role;
// colors come only from Theme (§12.8), never local literals.
Button {
    id: control
    property string variant: "Primary"   // Primary | Accent | Outline | Danger | Ghost
    property bool compact: false
    readonly property color fillColor: variant === "Accent" ? Theme.accent.base
                                     : variant === "Danger" ? Theme.error
                                     : Theme.brand.base

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
        // Phase 4d role map: Accent button label = accent.on (brand-deep) on the
        // gold fill (Admin Dashboard.dc.html:201, Library Kiosk v2.dc.html:48);
        // Primary/Danger keep brand.on (cream). Was a flat brand.onPrimary.
        color: control.variant === "Outline" || control.variant === "Ghost"
               ? Theme.text
               : control.variant === "Accent" ? Theme.accent.on : Theme.brand.on
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Accessible.role: Accessible.Button
    Accessible.name: control.text
}
