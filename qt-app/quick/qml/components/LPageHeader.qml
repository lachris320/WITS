import QtQuick
import QtQuick.Layouts
import LOAMS

// Admin page-header region (§11): title + subtitle on the left, live
// date + clock on the right — all shown together. Reference (Admin
// Dashboard.dc.html ~L68): "{{ clockDate }} · {{ clockTime }} {{ clockMeridiem }}",
// date and time as two differently-styled runs (muted date, brand-colored
// time) rather than one merged string — hence a separate dateText property
// instead of folding the date into clockText.
Item {
    id: hdr
    property string title: ""
    property string subtitle: ""
    property string dateText: ""
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

        RowLayout {
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            spacing: Theme.spacing.xs

            Text {
                objectName: "headerDateText"
                visible: hdr.dateText.length > 0
                text: hdr.dateText
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                Accessible.role: Accessible.StaticText
            }
            Text {
                visible: hdr.dateText.length > 0 && hdr.clockText.length > 0
                text: "·"
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            Text {
                objectName: "headerClockText"
                visible: hdr.clockText.length > 0
                text: hdr.clockText
                color: Theme.brand.admin
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.cardTitle
                font.weight: Font.Bold
                Accessible.role: Accessible.StaticText
            }
        }
    }
}
