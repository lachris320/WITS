import QtQuick
import QtQuick.Layouts
import LOAMS

// Right-hand main area: greeting, hero card + stat tiles, live feed.
Item {
    id: mainArea
    property var vm
    readonly property int feedCount: feed.count
    readonly property bool heroShowsStudent: vm ? vm.hasStudent : false
    readonly property string visitorsTodayShown: todayTile.value

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // Greeting headline + LIVE FEED badge.
        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                font.family: Theme.typography.serif
                font.pixelSize: Theme.typography.heroName
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
                text: (mainArea.vm && mainArea.vm.hasStudent)
                      ? (mainArea.vm.greeting + " 👋  " + mainArea.vm.currentName + qsTr(" just logged in"))
                      : ((mainArea.vm ? mainArea.vm.greeting : "") + qsTr(" — waiting for log in…"))
                color: (mainArea.vm && mainArea.vm.hasStudent)
                       ? Theme.text : Theme.mutedTextCaption
            }
            RowLayout {
                spacing: Theme.spacing.sm
                Rectangle {
                    width: 7; height: 7; radius: 3.5; color: Theme.brand.kiosk
                    SequentialAnimation on opacity {
                        running: Theme.motion.enabled; loops: Animation.Infinite
                        NumberAnimation { to: 1.0; duration: 800 }
                        NumberAnimation { to: 0.45; duration: 800 }
                    }
                }
                Text {
                    text: qsTr("LIVE FEED"); color: Theme.brand.kiosk
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                    font.weight: Font.ExtraBold; font.letterSpacing: 1.4
                }
            }
        }

        // Hero (Student Info Card) + Visitors Today.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md

            // Signed-in hero: NOT an LCard { filled: true } — that hard-codes
            // Theme.brand.admin (maroon meant for the admin brand), which left
            // this kiosk-brand-themed text (onKiosk/onBrandMuted/secondary)
            // low-contrast against the wrong fill. Built as its own themed
            // Rectangle filled with the kiosk brand gradient instead, mirroring
            // the reference's signed-in card (Library Kiosk v2.dc.html ~L76).
            Rectangle {
                id: hero
                Layout.fillWidth: true
                // Ratio verified by live render, not just reading the code: a
                // bare `Layout.preferredWidth: 2` is a literal 2px baseline,
                // not a stretch weight, so with only one sibling left in the
                // row (THIS HOUR removed) the hero collapsed to a near-invisible
                // sliver and Visitors Today claimed almost the whole row.
                // horizontalStretchFactor (Qt 6.5+) is the correct mechanism
                // for a fill-width ratio; give both items the same preferred
                // baseline as LStatTile's implicitWidth (200) and let the
                // 2:1 stretch weight do the emphasis.
                Layout.preferredWidth: 200
                Layout.horizontalStretchFactor: 2
                Layout.preferredHeight: 120
                radius: Theme.radius.card
                clip: true
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.brand.kiosk }
                    GradientStop { position: 1.0; color: Theme.brand.kioskHover }
                }
                Accessible.role: Accessible.Grouping

                // Decorative corner ring, matching the reference's accent ring.
                Rectangle {
                    width: 150; height: 150
                    radius: width / 2
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: -50
                    anchors.rightMargin: -50
                    color: "transparent"
                    border.width: 24
                    border.color: Qt.alpha(Theme.secondary, 0.15)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.xl
                    spacing: Theme.spacing.xs
                    Text {
                        text: qsTr("NOW SIGNED IN")
                        color: Theme.secondary
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                        font.weight: Font.ExtraBold; font.letterSpacing: 1.4
                    }
                    Text {
                        visible: mainArea.vm ? mainArea.vm.hasStudent : false
                        text: mainArea.vm ? mainArea.vm.currentFullName : ""
                        color: Theme.brand.onKiosk
                        font.family: Theme.typography.serif
                        font.pixelSize: Theme.typography.heroName
                        font.weight: Font.Bold
                    }
                    Text {
                        visible: mainArea.vm ? mainArea.vm.hasStudent : false
                        text: mainArea.vm
                              ? (mainArea.vm.currentCourse + " · " + mainArea.vm.currentYear
                                 + " · " + mainArea.vm.currentDept + " · " + mainArea.vm.currentTime)
                              : ""
                        color: Theme.onBrandMuted
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                    }
                    Text {
                        visible: mainArea.vm ? !mainArea.vm.hasStudent : true
                        text: qsTr("Scan your ID to begin")
                        color: Theme.onBrandMuted
                        font.family: Theme.typography.serif
                        font.pixelSize: Theme.typography.cardTitle
                    }
                }
            }
            LStatTile {
                id: todayTile
                Layout.fillWidth: true
                // Explicit factor, not left implicit: mixing an explicit
                // horizontalStretchFactor on one RowLayout sibling with an
                // implicit (preferredWidth-derived) one on another produces a
                // wildly lopsided split in practice — verified by live render,
                // where the hero swallowed ~100% of the row width and this
                // tile collapsed to nothing. Setting 1 here (vs hero's 2)
                // keeps both on the same explicit scale for the ~2:1 emphasis.
                Layout.horizontalStretchFactor: 1
                Layout.preferredHeight: 120
                label: qsTr("VISITORS TODAY")
                value: mainArea.vm ? String(mainArea.vm.visitorsToday) : "0"
            }
        }

        // Attendance feed.
        LCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0
            ListView {
                id: feed
                anchors.fill: parent
                clip: true
                model: mainArea.vm ? mainArea.vm.recentLogins : null
                delegate: KioskFeedRow {
                    width: ListView.view ? ListView.view.width : 0
                    rowName: model.name
                    rowCourse: model.course
                    rowYearShort: model.yearShort
                    rowDept: model.dept
                    rowTime: model.time
                    rowInitials: model.initials
                    rowFresh: model.fresh
                }
            }
        }
    }
}
