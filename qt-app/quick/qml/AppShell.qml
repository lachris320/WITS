import QtQuick
import QtQuick.Controls
import LOAMS

ApplicationWindow {
    id: appShell
    width: 1280
    height: 800
    visible: true
    title: qsTr("LOAMS 2.0")
    color: Theme.appBackground

    Loader {
        id: surface
        anchors.fill: parent
        sourceComponent: Navigator.currentSurface === Navigator.Admin
                         ? adminSurface : kioskSurface
    }

    Component { id: kioskSurface; KioskScreen {} }
    Component { id: adminSurface; AdminScreen {} }
}
