import QtQuick
import LOAMS

// Placeholder for track 4b (Reporting). Navigation must work before that
// track fills it in. No `vm` — AdminScreen's Loader.onLoaded guards on
// `item.vm && item.vm.refresh`, so a vm-less screen issues no network.
Rectangle {
    id: screen
    color: Theme.appBackground
    Text {
        anchors.centerIn: parent
        objectName: "reportingPlaceholder"
        text: qsTr("Reporting — coming soon")
        color: Theme.mutedText
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.cardTitle
    }
}
