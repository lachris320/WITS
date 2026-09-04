import QtQuick
import LOAMS

// Circular school-logo badge (see LCircleImage for the crop mechanism). Real
// logo cropped to a circle when configured+loaded, else a "LOGO" placeholder.
// Shared by LSidebarBrand (52px/2px ring) and BrandPanel (96px/3px ring).
// Presentational: primitive props, not a vm.
Item {
    id: logoFrame

    property url logoUrl: ""
    property bool hasLogo: false
    // hasLogo:false vetoes a set logoUrl (VM reports hasLogo:false for a
    // configured-but-missing file); LCircleImage keys off this blanked url.
    readonly property url effectiveUrl: hasLogo ? logoUrl : ""
    property int size: 52
    property int ringWidth: 2

    implicitWidth: size
    implicitHeight: size

    LCircleImage {
        id: circleImg
        anchors.fill: parent
        source: logoFrame.effectiveUrl
        size: logoFrame.size
        ringWidth: logoFrame.ringWidth   // gold ring over the photo (ringColor default = accent.base)

        // Placeholder: the "LOGO" circle with its own gold ring. objectName
        // PRESERVED here (not on LCircleImage's generic host) so existing
        // regression tests keep finding a Rectangle with a `border` property;
        // `visible` is bound directly to showingImage rather than left to the
        // host, since a QQuickItem's own `visible` doesn't flip just because
        // an ancestor's does.
        Rectangle {
            objectName: "logoPlaceholder"
            anchors.fill: parent
            radius: width / 2
            color: Theme.card
            border.width: logoFrame.ringWidth
            border.color: Theme.accent.base
            visible: !circleImg.showingImage
            Text {
                anchors.centerIn: parent
                text: qsTr("LOGO")
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.eyebrow
            }
        }
    }
}
