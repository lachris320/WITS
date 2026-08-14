import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Native themed date picker (Phase 4b-i). A field showing the selected ISO
// date that opens a calendar-grid popup. Colors from Theme tokens only.
// selectedDate is "yyyy-MM-dd" ("" = unset). Emits picked(dateString) on a
// day tap. displayYear/displayMonth (1..12) drive the shown grid.
Item {
    id: root
    property string selectedDate: ""
    property string placeholder: qsTr("Pick a date")
    property int displayYear: (new Date()).getFullYear()
    property int displayMonth: (new Date()).getMonth() + 1   // 1..12
    signal picked(string dateString)

    implicitWidth: 280
    implicitHeight: 44

    function open() { popup.open() }
    function close() { popup.close() }
    function showMonth(y, m) { root.displayYear = y; root.displayMonth = m }
    function nextMonth() {
        if (root.displayMonth === 12) { root.displayMonth = 1; root.displayYear++ }
        else root.displayMonth++
    }
    function prevMonth() {
        if (root.displayMonth === 1) { root.displayMonth = 12; root.displayYear-- }
        else root.displayMonth--
    }
    // Popup-independent selection seam: the cell MouseArea AND the QuickTest
    // both call this, so day-selection is testable without driving a click
    // inside the Controls Popup overlay (which QuickTest cannot reliably hit).
    function selectDay(d) {
        var iso = root.displayYear + "-" + root._pad(root.displayMonth) + "-" + root._pad(d);
        root.selectedDate = iso;
        root.picked(iso);
        root.close();
    }

    // Two-digit zero-pad without printf: "8" -> "08".
    function _pad(n) { return (n < 10 ? "0" : "") + n }

    // Leading-blank count + day count for the shown month via JS Date.
    readonly property int _firstDow: (new Date(root.displayYear, root.displayMonth - 1, 1)).getDay() // 0=Sun
    readonly property int _daysInMonth: (new Date(root.displayYear, root.displayMonth, 0)).getDate()
    readonly property var _cells: {
        var out = [];
        for (var b = 0; b < root._firstDow; ++b) out.push(0);        // blanks
        for (var d = 1; d <= root._daysInMonth; ++d) out.push(d);
        return out;
    }
    readonly property var _monthNames: ["January","February","March","April","May","June",
                                        "July","August","September","October","November","December"]

    // The field (acts as a button).
    Rectangle {
        id: field
        anchors.fill: parent
        radius: Theme.radius.sm
        color: Theme.card
        border.width: 2
        border.color: popup.opened ? Theme.brand.base : Theme.border
        Text {
            id: fieldText
            objectName: "datePickerField"
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing.md
            anchors.rightMargin: Theme.spacing.md
            verticalAlignment: Text.AlignVCenter
            text: root.selectedDate.length > 0 ? root.selectedDate : root.placeholder
            textFormat: Text.PlainText
            color: root.selectedDate.length > 0 ? Theme.text : Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            elide: Text.ElideRight
        }
        MouseArea { anchors.fill: parent; onClicked: root.open() }
    }

    Popup {
        id: popup
        y: field.height + Theme.spacing.xs
        width: 300
        padding: Theme.spacing.md
        background: Rectangle {
            radius: Theme.radius.md
            color: Theme.card
            border.width: 1
            border.color: Theme.border
        }
        ColumnLayout {
            spacing: Theme.spacing.sm
            width: parent.width

            // Header: ‹ Month Year ›
            RowLayout {
                Layout.fillWidth: true
                LButton {
                    objectName: "datePrevMonth"
                    variant: "Ghost"; compact: true; text: "‹"
                    onClicked: root.prevMonth()
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root._monthNames[root.displayMonth - 1] + " " + root.displayYear
                    color: Theme.text
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                    font.weight: Font.ExtraBold
                }
                LButton {
                    objectName: "dateNextMonth"
                    variant: "Ghost"; compact: true; text: "›"
                    onClicked: root.nextMonth()
                }
            }

            // Weekday headers.
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                columnSpacing: 0; rowSpacing: 0
                Repeater {
                    model: ["Su","Mo","Tu","We","Th","Fr","Sa"]
                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.eyebrow
                    }
                }
            }

            // Day grid.
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                columnSpacing: 0; rowSpacing: 0
                Repeater {
                    model: root._cells
                    delegate: Item {
                        required property var modelData    // 0 = blank
                        Layout.fillWidth: true
                        implicitHeight: 34
                        Rectangle {
                            visible: modelData > 0
                            objectName: modelData > 0 ? ("dayCell_" + modelData) : ""
                            anchors.centerIn: parent
                            width: 30; height: 30
                            radius: Theme.radius.sm
                            readonly property bool isSelected:
                                root.selectedDate === (root.displayYear + "-" + root._pad(root.displayMonth) + "-" + root._pad(modelData))
                            color: isSelected ? Theme.brand.base
                                              : (cellHover.hovered ? Qt.alpha(Theme.brand.base, 0.10) : "transparent")
                            HoverHandler { id: cellHover }
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                textFormat: Text.PlainText
                                color: parent.isSelected ? Theme.brand.on : Theme.text
                                font.family: Theme.typography.sans
                                font.pixelSize: Theme.typography.body
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.selectDay(modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
