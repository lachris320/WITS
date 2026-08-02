import QtQuick
import QtQuick.Controls
import LOAMS

// The one clickable-action primitive (§11). variant selects the fill role;
// colors come only from Theme (§12.8), never local literals.
Button {
    id: control
    property string variant: "Primary"   // Primary | Accent | Outline | Danger | Ghost
    property bool compact: false
    // Opt-in for Outline/Ghost sitting on a dark brand fill (e.g. the maroon
    // kiosk BrandPanel): renders a cream (brand.on) label + a legible
    // translucent light border. Default false keeps every light-background
    // call site rendering Theme.text on a Theme.border edge, unchanged.
    property bool onBrand: false
    // Supplemental hover tooltip (themed — the default QQC2 ToolTip ignores
    // Theme). Empty => no tooltip, behavior unchanged. NOT exposed to assistive
    // tech, so any scope info here must ALSO live in accessibleName.
    property string tooltipText: ""
    // When set, overrides Accessible.name so a screen reader reads a full-scope
    // phrase ("Delete 3 selected rows") instead of the terse label.
    property string accessibleName: ""
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
        border.color: control.onBrand ? Qt.alpha(Theme.brand.on, 0.5) : Theme.border
    }
    contentItem: Text {
        text: control.text
        // Phase 4d role map: Accent button label = accent.on (brand-deep) on the
        // gold fill (Admin Dashboard.dc.html:201, Library Kiosk v2.dc.html:48);
        // Primary/Danger keep brand.on (cream). Was a flat brand.onPrimary.
        color: control.variant === "Outline" || control.variant === "Ghost"
               ? (control.onBrand ? Theme.brand.on : Theme.text)
               : control.variant === "Accent" ? Theme.accent.on : Theme.brand.on
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Accessible.role: Accessible.Button
    Accessible.name: control.accessibleName !== "" ? control.accessibleName : control.text

    ToolTip {
        id: lbuttonTip
        objectName: "lbuttonTooltip"
        text: control.tooltipText
        delay: 500
        visible: control.tooltipText !== "" && control.hovered
        padding: Theme.spacing.sm
        contentItem: Text {
            text: lbuttonTip.text
            color: Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            wrapMode: Text.WordWrap
        }
        background: Rectangle {
            color: Theme.card
            radius: Theme.radius.md
            border.width: 1
            border.color: Theme.border
        }
    }
}
