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

        // Hero + stat tiles.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LCard {
                id: hero
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.preferredHeight: 120
                filled: true
                ColumnLayout {
                    anchors.fill: parent
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
                Layout.preferredHeight: 120
                label: qsTr("VISITORS TODAY")
                value: mainArea.vm ? String(mainArea.vm.visitorsToday) : "0"
            }
            LStatTile {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                label: qsTr("THIS HOUR")
                value: mainArea.vm ? String(mainArea.vm.visitorsThisHour) : "0"
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
