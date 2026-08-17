import QtQuick
import QtQuick.Layouts
import LOAMS

// Dashboard/kiosk stat card (§11): label + value + caption, Neutral or Hero.
Rectangle {
    id: tile
    property string label: ""
    property string value: ""
    property string caption: ""
    property string variant: "Neutral"   // Neutral | Hero

    color: variant === "Hero" ? Theme.brand.base : Theme.card
    radius: Theme.radius.card
    border.width: variant === "Hero" ? 0 : 2
    border.color: Theme.border
    implicitWidth: 200
    implicitHeight: 110

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xl
        spacing: Theme.spacing.xs
        Text {
            text: tile.label
            textFormat: Text.PlainText
            // Phase 4d role map: Hero eyebrow = accent.base (gold) — mockup
            // "VISITORS TODAY" label on the maroon hero is var(--gold)
            // (Admin Dashboard.dc.html:76); was brand.onPrimary (cream).
            color: tile.variant === "Hero" ? Theme.accent.base : Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.eyebrow
        }
        Text {
            objectName: "statTileValue"
            text: tile.value
            textFormat: Text.PlainText
            color: tile.variant === "Hero" ? Theme.brand.on : Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.statValue
        }
        Text {
            visible: tile.caption.length > 0
            text: tile.caption
            textFormat: Text.PlainText
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }
    }

    Accessible.role: Accessible.Grouping
}
