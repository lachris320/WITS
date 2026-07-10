import QtQuick
import QtQuick.Layouts
import LOAMS

// Admin page-header region (§11): title, subtitle, live clock.
Item {
    id: hdr
    property string title: ""
    property string subtitle: ""
    property string clockText: ""
    property Item actions: null

    implicitHeight: 64
    implicitWidth: 480

    ColumnLayout {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacing.xs
        Text {
            text: hdr.title
            color: Theme.text
            font.family: Theme.typography.serif
            font.pixelSize: Theme.typography.pageTitle
            font.weight: Font.Bold
            Accessible.role: Accessible.StaticText
        }
        Text {
            visible: hdr.subtitle.length > 0 || hdr.clockText.length > 0
            text: hdr.clockText.length > 0 ? hdr.clockText : hdr.subtitle
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            Accessible.role: Accessible.StaticText
        }
    }
}
