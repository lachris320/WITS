import QtQuick
import QtQuick.Layouts
import LOAMS

// Reporting (spec 4b-i): Dept->Course + duration filters and a native-QML
// preview (stat tiles + visits-by-course bar chart + per-student table). Run
// by an explicit Generate button. Takes `property var vm` (a ReportingViewModel
// or a plain-QML stub in QuickTests). No Component.onCompleted fetch — the
// initial bootstrap is issued by AdminScreen's Loader.onLoaded gate.
Rectangle {
    id: screen
    property var vm
    color: Theme.appBackground

    readonly property bool isLoading: vm ? vm.loading : false
    readonly property bool isError: vm ? vm.errorText.length > 0 : false
    readonly property bool showPreview: vm ? (vm.hasResult && !screen.isError) : false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg

        // --- Filter card (self-sizing Rectangle: LCard has a fixed
        // implicitHeight of 96 and does NOT grow to fit slotted content, so
        // the two-row filter would clip — mirror DatabaseScreen's filter card). ---
        Rectangle {
            Layout.fillWidth: true
            color: Theme.card; radius: Theme.radius.card
            border.width: 2; border.color: Theme.border
            implicitHeight: filterCol.implicitHeight + Theme.spacing.xl * 2
            ColumnLayout {
                id: filterCol
                anchors.fill: parent
                anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md

                LCascadingSelect {
                    id: cascade
                    Layout.fillWidth: true
                    departments: screen.vm ? screen.vm.departments : []
                    courses: screen.vm ? screen.vm.courses : []
                    department: screen.vm ? screen.vm.department : ""
                    course: screen.vm ? screen.vm.course : ""
                    onDepartmentPicked: function(d) { if (screen.vm) screen.vm.setDepartment(d); }
                    onCoursePicked: function(c) { if (screen.vm) screen.vm.setCourse(c); }
                }

                // Duration selector + mode-specific sub-controls.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.md

                    LComboBox {
                        id: durationCombo
                        objectName: "durationCombo"
                        Layout.preferredWidth: 160
                        model: [qsTr("Day"), qsTr("Month"), qsTr("Semester"), qsTr("Custom")]
                        placeholder: qsTr("Duration")
                        currentValue: model[screen.vm ? screen.vm.durationType : 0]
                        onSelected: function(v) {
                            if (screen.vm) screen.vm.setDurationType(durationCombo.model.indexOf(v));
                        }
                    }

                    // Day mode.
                    LDatePicker {
                        objectName: "dayPicker"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 0
                        Layout.preferredWidth: 220
                        placeholder: qsTr("Pick a day")
                        selectedDate: screen.vm ? screen.vm.day : ""
                        onPicked: function(d) { if (screen.vm) screen.vm.setDay(d); }
                    }

                    // Month mode.
                    LComboBox {
                        objectName: "monthCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 1
                        Layout.preferredWidth: 150
                        model: [qsTr("January"), qsTr("February"), qsTr("March"), qsTr("April"),
                                qsTr("May"), qsTr("June"), qsTr("July"), qsTr("August"),
                                qsTr("September"), qsTr("October"), qsTr("November"), qsTr("December")]
                        placeholder: qsTr("Month")
                        onSelected: function(v) {
                            if (screen.vm) screen.vm.setMonth(model.indexOf(v) + 1);   // 1..12
                        }
                    }
                    LComboBox {
                        objectName: "monthYearCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 1
                        Layout.preferredWidth: 120
                        model: screen.vm ? screen.vm.years : []
                        placeholder: qsTr("Year")
                        onSelected: function(v) { if (screen.vm) screen.vm.setMonthYear(parseInt(v)); }
                    }

                    // Semester mode.
                    LComboBox {
                        objectName: "semesterCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 2
                        Layout.preferredWidth: 180
                        model: [qsTr("First Semester"), qsTr("Second Semester"), qsTr("Summer")]
                        placeholder: qsTr("Semester")
                        onSelected: function(v) { if (screen.vm) screen.vm.setSemester(v); }
                    }
                    LComboBox {
                        objectName: "semYearCombo"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 2
                        Layout.preferredWidth: 120
                        model: screen.vm ? screen.vm.years : []
                        placeholder: qsTr("Year")
                        onSelected: function(v) { if (screen.vm) screen.vm.setSemYear(parseInt(v)); }
                    }

                    // Custom mode.
                    LDatePicker {
                        objectName: "customStartPicker"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 3
                        Layout.preferredWidth: 200
                        placeholder: qsTr("Start date")
                        selectedDate: screen.vm ? screen.vm.customStart : ""
                        onPicked: function(d) { if (screen.vm) screen.vm.setCustomStart(d); }
                    }
                    LDatePicker {
                        objectName: "customEndPicker"
                        visible: (screen.vm ? screen.vm.durationType : 0) === 3
                        Layout.preferredWidth: 200
                        placeholder: qsTr("End date")
                        selectedDate: screen.vm ? screen.vm.customEnd : ""
                        onPicked: function(d) { if (screen.vm) screen.vm.setCustomEnd(d); }
                    }

                    Item { Layout.fillWidth: true }   // spacer

                    LButton {
                        objectName: "generateButton"
                        text: qsTr("Generate Report")
                        enabled: screen.vm ? screen.vm.canGenerate : false
                        onClicked: if (screen.vm) screen.vm.generateReport()
                    }
                }
            }
        }

        // --- Error + retry block ---
        RowLayout {
            Layout.fillWidth: true
            visible: screen.isError
            spacing: Theme.spacing.md
            Text {
                objectName: "reportErrorText"
                Layout.fillWidth: true
                text: screen.vm ? screen.vm.errorText : ""
                textFormat: Text.PlainText     // server-echoed over cleartext HTTP
                color: Theme.error
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                wrapMode: Text.WordWrap
            }
            LButton {
                objectName: "reportRetryButton"
                variant: "Outline"; compact: true
                text: qsTr("Retry")
                onClicked: if (screen.vm) screen.vm.retry()
            }
        }

        // --- Preview (stat tiles + chart + table) ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.lg
            visible: screen.showPreview
            opacity: screen.isLoading ? 0.4 : 1.0

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.lg
                LStatTile {
                    objectName: "totalVisitsTile"
                    Layout.fillWidth: true
                    label: qsTr("TOTAL VISITS")
                    value: screen.vm ? String(screen.vm.totalVisits) : "0"
                }
                LStatTile {
                    objectName: "studentsShownTile"
                    Layout.fillWidth: true
                    label: qsTr("STUDENTS")
                    value: screen.vm ? String(screen.vm.studentsShown) : "0"
                }
                LStatTile {
                    objectName: "topCourseTile"
                    Layout.fillWidth: true
                    label: qsTr("TOP COURSE")
                    value: screen.vm ? screen.vm.topCourse : "—"
                }
            }

            LCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                LBarChart {
                    objectName: "reportBarChart"
                    anchors.fill: parent
                    orientation: "Horizontal"
                    model: screen.vm ? screen.vm.courseBars : null
                    maxValue: (screen.vm && screen.vm.courseBars) ? screen.vm.courseBars.maxValue : 100
                }
            }

            LTable {
                objectName: "reportTable"
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: [
                    { key: "name",   title: qsTr("Name"),   weight: 3 },
                    { key: "course", title: qsTr("Course"), weight: 2 },
                    { key: "year",   title: qsTr("Year"),   weight: 1 },
                    { key: "visits", title: qsTr("Visits"), weight: 1 }
                ]
                model: screen.vm ? screen.vm.rows : null
                emptyStateText: qsTr("No visits in this range")
            }
        }
    }
}
