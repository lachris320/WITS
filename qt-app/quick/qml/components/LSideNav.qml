import QtQuick
import QtQuick.Layouts
import LOAMS

// Persistent left sidebar (§11). Brand-gradient background reads the brand
// roles directly (§12.5), not Theme.sidebarBase.
Rectangle {
    id: nav
    property var items: []            // bound to Navigator's registered pages (later)
    property string currentPage: ""
    property Item footer: null

    color: Theme.brand.admin
    implicitWidth: 240
    implicitHeight: 600

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.lg
        spacing: Theme.spacing.sm
        Repeater {
            model: nav.items
            delegate: Text {
                text: modelData.label !== undefined ? modelData.label : modelData
                color: Theme.brand.onPrimary
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                Accessible.role: Accessible.ListItem
            }
        }
    }

    Accessible.role: Accessible.List
}
