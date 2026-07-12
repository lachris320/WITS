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

    // Entry animation (fade + slide-up) is driven by the ListView's own
    // `add`/`displaced` transitions (see KioskMain.qml) so that rows
    // displaced by a prepend also animate, not just the newly-inserted row.
    // Note: `add:` does not fire for the initial population of the view (only
    // `populate:` would), so the first batch of rows on load renders instantly
    // with no animation — intentional, not an oversight; only later inserts
    // (a real login) get the fade/slide.
}
