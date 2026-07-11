import QtQuick
import LOAMS

// The one bordered-container primitive (§11).
Rectangle {
    id: card
    property int padding: Theme.spacing.xl
    property bool filled: false
    property bool elevated: false
    default property alias content: body.data

    color: filled ? Theme.brand.admin : Theme.card
    radius: Theme.radius.card
    border.width: filled ? 0 : 2
    border.color: Theme.border
    implicitWidth: 160
    implicitHeight: 96

    Item {
        id: body
        anchors.fill: parent
        anchors.margins: card.padding
    }

    Accessible.role: Accessible.Grouping
}
