import QtQuick
import LOAMS

Rectangle {
    id: kiosk
    color: Theme.appBackground
    Text {
        anchors.centerIn: parent
        text: qsTr("Kiosk")
        color: Theme.text
        font.family: Theme.typography.serif
        font.pixelSize: Theme.typography.pageTitle
    }
}
