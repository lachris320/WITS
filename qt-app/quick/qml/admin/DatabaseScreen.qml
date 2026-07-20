import QtQuick
import LOAMS

// Placeholder for track 4a (Database + Import). Navigation must work before
// that track fills it in. No `vm` — AdminScreen's Loader.onLoaded guards on
// `item.vm && item.vm.refresh`, so a vm-less screen issues no network.
Rectangle {
    id: screen
    color: Theme.appBackground
    Text {
        anchors.centerIn: parent
        objectName: "databasePlaceholder"
        text: qsTr("Database — coming soon")
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.cardTitle
    }
}
