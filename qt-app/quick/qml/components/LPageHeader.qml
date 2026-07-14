import QtQuick
import QtQuick.Layouts
import LOAMS

// Admin page-header region (§11): title + subtitle on the left, live clock on
// the right — all shown together.
Item {
    id: hdr
    property string title: ""
    property string subtitle: ""
    property string clockText: ""
    property Item actions: null

    implicitHeight: 64
    implicitWidth: 480

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing.lg

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
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
                visible: hdr.subtitle.length > 0
                text: hdr.subtitle
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                Accessible.role: Accessible.StaticText
            }
        }

        Text {
            visible: hdr.clockText.length > 0
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            text: hdr.clockText
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.cardTitle
            Accessible.role: Accessible.StaticText
        }
    }
}
