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

    color: variant === "Hero" ? Theme.brand.admin : Theme.card
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
            color: tile.variant === "Hero" ? Theme.brand.onPrimary : Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.eyebrow
        }
        Text {
            text: tile.value
            color: tile.variant === "Hero" ? Theme.brand.onPrimary : Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.statValue
        }
        Text {
            visible: tile.caption.length > 0
            text: tile.caption
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }
    }

    Accessible.role: Accessible.Grouping
}
