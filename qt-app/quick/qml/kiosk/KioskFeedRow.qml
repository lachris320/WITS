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
    color: rowFresh ? Theme.accent.soft : Theme.card
    // Left edge highlight for the freshest row.
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: 4
        color: row.rowFresh ? Theme.accent.base : "transparent"
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
            // Phase 4d role map: feed avatar = maroon brand, NOT gold (Kiosk:179-180),
            // matching the admin search avatar. Fresh: brand.base bg + brand.on cream
            // initials; non-fresh: brand.soft bg + brand.base maroon initials.
            color: row.rowFresh ? Theme.brand.base : Theme.brand.soft
            Text {
                anchors.centerIn: parent
                text: row.rowInitials
                color: row.rowFresh ? Theme.brand.on : Theme.brand.base
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
            // Phase 4d role map: fresh login-time = brand.text maroon on light
            // (Kiosk:181), NOT gold; non-fresh stays mutedText.
            color: row.rowFresh ? Theme.brand.text : Theme.mutedText
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
