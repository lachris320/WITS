import QtQuick
import QtQuick.Layouts
import LOAMS

// One attendance-feed row. `model*` come from RecentLoginsModel roles.
Rectangle {
    id: row
    property string rowName
    property string rowCourse
    property string rowYearShort
    property string rowDept
    property string rowTime
    property string rowInitials
    property bool rowFresh: false

    implicitHeight: 60
    color: rowFresh ? Theme.secondarySoft : Theme.card
    // Left edge highlight for the freshest row.
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: 4
        color: row.rowFresh ? Theme.secondary : "transparent"
    }
    Rectangle {   // bottom hairline
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 1; color: Theme.rowHairline
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing.xxl
        anchors.rightMargin: Theme.spacing.xxl
        spacing: Theme.spacing.md

        Rectangle {   // avatar chip
            Layout.preferredWidth: 34; Layout.preferredHeight: 34
            radius: width / 2
            color: row.rowFresh ? Theme.brand.kiosk : Theme.brand.kioskSoft
            Text {
                anchors.centerIn: parent
                text: row.rowInitials
                color: row.rowFresh ? Theme.brand.onKiosk : Theme.brand.kiosk
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                font.weight: Font.ExtraBold
            }
        }
        Text {
            Layout.preferredWidth: 200
            text: row.rowName
            color: Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            font.weight: Font.ExtraBold
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            text: row.rowCourse + " · " + row.rowYearShort
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }
        Text {
            Layout.preferredWidth: 160
            text: row.rowDept
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
            elide: Text.ElideRight
        }
        Text {
            text: row.rowTime
            color: row.rowFresh ? Theme.brand.kiosk : Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
            font.weight: Font.ExtraBold
        }
    }

    // rowIn: slide-up + fade on insertion.
    opacity: 0
    Component.onCompleted: rowIn.start()
    ParallelAnimation {
        id: rowIn
        running: false
        NumberAnimation { target: row; property: "opacity"; from: 0; to: 1; duration: Theme.motion.rowIn }
        NumberAnimation { target: row; property: "y"; from: row.y + 14; to: row.y; duration: Theme.motion.rowIn; easing.type: Easing.OutCubic }
    }
}
