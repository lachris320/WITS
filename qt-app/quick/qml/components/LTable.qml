import QtQuick
import QtQuick.Controls
import LOAMS

// Row-grid primitive (§11). Phase 1 skeleton: header tint + hairline rows;
// full column/model wiring lands with Database/Visit Logs screens.
Rectangle {
    id: table
    property var columns: []
    property var model: null
    property bool selectable: false
    property string emptyStateText: qsTr("No records")

    color: Theme.card
    radius: Theme.radius.card
    border.width: 2
    border.color: Theme.border
    implicitWidth: 320
    implicitHeight: 200

    Rectangle {
        id: header
        width: parent.width
        height: 36
        color: Theme.tableHeaderBg
        radius: Theme.radius.card
    }

    Accessible.role: Accessible.Table
}
