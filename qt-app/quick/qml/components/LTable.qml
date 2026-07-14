import QtQuick
import QtQuick.Layouts
import LOAMS

// Row-grid primitive (§11). columns: list of {key, title, weight?}; model:
// a QAbstractListModel whose roles match the column keys.
Rectangle {
    id: table
    property var columns: []
    property var model: null
    property bool selectable: false
    property string emptyStateText: qsTr("No records")

    // Introspection for tests + the empty state.
    readonly property int rowCount: list.count
    readonly property bool emptyVisible: list.count === 0

    color: Theme.card
    radius: Theme.radius.card
    border.width: 2
    border.color: Theme.border
    implicitWidth: 320
    implicitHeight: 200
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 36
            color: Theme.tableHeaderBg
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing.lg
                anchors.rightMargin: Theme.spacing.lg
                spacing: Theme.spacing.md
                Repeater {
                    model: table.columns
                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: modelData.weight !== undefined ? modelData.weight : 1
                        text: modelData.title !== undefined ? modelData.title : ""
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Body.
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: table.model
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: rowDelegate
                required property var model
                required property int index
                width: ListView.view ? ListView.view.width : 0
                implicitHeight: 40
                color: hover.hovered ? Qt.alpha(Theme.brand.admin, 0.06)
                                     : (index % 2 === 0 ? Theme.card : Theme.rowHairline)

                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.lg
                    anchors.rightMargin: Theme.spacing.lg
                    spacing: Theme.spacing.md
                    Repeater {
                        model: table.columns
                        delegate: Text {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: modelData.weight !== undefined ? modelData.weight : 1
                            text: {
                                var v = rowDelegate.model[modelData.key];
                                return v !== undefined && v !== null ? v : "";
                            }
                            color: Theme.text
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // Empty state.
    Text {
        anchors.centerIn: parent
        visible: table.emptyVisible
        text: table.emptyStateText
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
    }

    Accessible.role: Accessible.Table
}
