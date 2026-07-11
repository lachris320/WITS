import QtQuick
import LOAMS

Rectangle {
    id: admin
    color: Theme.appBackground
    Text {
        anchors.centerIn: parent
        text: qsTr("Admin (coming in Phase 3)")
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.cardTitle
    }
}
