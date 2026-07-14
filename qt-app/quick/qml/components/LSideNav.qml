import QtQuick
import QtQuick.Layouts
import LOAMS

// Persistent left sidebar (§11). Brand-gradient background reads the brand
// roles directly (§12.5). items: list of {page, label, enabled}.
Rectangle {
    id: nav
    property var items: []
    property string currentPage: ""
    property Item footer: null
    signal pageActivated(var page)

    // Test/logic hook: the single path both a click and a test call take, so
    // the disabled-guard lives in exactly one place.
    function activate(page) {
        for (var i = 0; i < items.length; i++) {
            if (items[i].page === page) {
                if (items[i].enabled === false)
                    return;
                nav.pageActivated(page);
                return;
            }
        }
    }

    color: Theme.brand.admin
    implicitWidth: 240
    implicitHeight: 600

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.lg
        spacing: Theme.spacing.sm

        Repeater {
            model: nav.items
            delegate: Rectangle {
                id: row
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 40
                radius: Theme.radius.sm2
                readonly property bool isActive: row.modelData.page === nav.currentPage
                readonly property bool isEnabled: row.modelData.enabled !== false
                color: isActive ? Qt.alpha(Theme.brand.onPrimary, 0.14) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.md
                    anchors.rightMargin: Theme.spacing.md
                    spacing: Theme.spacing.sm

                    // Gold active-dot.
                    Rectangle {
                        implicitWidth: 6; implicitHeight: 6; radius: 3
                        visible: row.isActive
                        color: Theme.secondary
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.label !== undefined ? row.modelData.label : row.modelData
                        color: row.isEnabled ? Theme.brand.onPrimary
                                             : Qt.alpha(Theme.brand.onPrimary, 0.45)
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        Accessible.role: Accessible.ListItem
                    }
                    // "soon" badge for disabled (Phase 4) items.
                    Text {
                        visible: !row.isEnabled
                        text: qsTr("soon")
                        color: Qt.alpha(Theme.brand.onPrimary, 0.55)
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: row.isEnabled
                    cursorShape: row.isEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: nav.activate(row.modelData.page)
                }
            }
        }

        Item { Layout.fillHeight: true }   // push footer to the bottom

        // Footer slot (e.g. Back-to-Kiosk), reparented into the column.
        Item {
            Layout.fillWidth: true
            implicitHeight: nav.footer ? nav.footer.implicitHeight : 0
            children: nav.footer ? [nav.footer] : []
        }
    }

    Accessible.role: Accessible.List
}
