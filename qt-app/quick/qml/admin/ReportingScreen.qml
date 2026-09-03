import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import LOAMS

// Reporting (spec 4b-i): Dept->Course + duration filters and a native-QML
// preview (stat tiles + visits-by-course bar chart + per-student table). Run
// by an explicit Generate button. Takes `property var vm` (a ReportingViewModel
// or a plain-QML stub in QuickTests). No Component.onCompleted fetch — the
// initial bootstrap is issued by AdminScreen's Loader.onLoaded gate.
// Also hosts the 4b-ii export bar (palette + chart-type pickers, PDF/Excel/Print
// actions) alongside the preview.
Rectangle {
    id: screen
    property var vm
    color: Theme.appBackground

    readonly property bool isLoading: vm ? vm.loading : false
    readonly property bool isError: vm ? vm.errorText.length > 0 : false
    readonly property bool showPreview: vm ? (vm.hasResult && !screen.isError) : false
    readonly property bool canExport: vm ? vm.canExport : false
    readonly property bool hasRows: vm && vm.rows && vm.rows.count > 0
    property bool showRoster: false

    Flickable {
        id: reportFlick
        objectName: "reportScroll"
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        contentWidth: width
        contentHeight: reportContent.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: reportContent
            width: reportFlick.width
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
                        // Clickable unless a fetch is in flight — with
                        // incomplete filters, clicking surfaces a validation
                        // message (vm.generateReport()) instead of silently
                        // doing nothing behind a disabled-looking button.
                        enabled: screen.vm ? !screen.vm.loading : false
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
                // Retry re-issues the same fetch, so it only makes sense
                // once filters were complete enough to have fetched in the
                // first place (a real network/server error). A validation
                // prompt means the filters themselves are incomplete —
                // "retrying" the same fetch would just repeat the message.
                visible: screen.vm ? screen.vm.filtersComplete : false
                onClicked: if (screen.vm) screen.vm.retry()
            }
        }

        // --- Preview (stat tiles + chart + table) ---
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.lg
            visible: screen.showPreview
            opacity: screen.isLoading ? 0.4 : 1.0

            // KPI band — four tiles when there are rows...
            RowLayout {
                objectName: "kpiBand"
                Layout.fillWidth: true
                spacing: Theme.spacing.lg
                visible: screen.hasRows
                LStatTile {
                    objectName: "totalVisitsTile"
                    Layout.fillWidth: true
                    label: qsTr("TOTAL VISITS")
                    value: screen.vm ? String(screen.vm.totalVisits) : "0"
                }
                LStatTile {
                    objectName: "uniqueVisitorsTile"
                    Layout.fillWidth: true
                    label: qsTr("UNIQUE VISITORS")
                    value: screen.vm ? String(screen.vm.uniqueVisitors) : "0"
                }
                LStatTile {
                    objectName: "avgVisitsTile"
                    Layout.fillWidth: true
                    label: qsTr("AVG. VISITS / VISITOR")
                    value: screen.vm ? screen.vm.avgVisitsPerVisitor.toFixed(1) : "0"
                }
                LStatTile {
                    objectName: "topDepartmentTile"
                    Layout.fillWidth: true
                    label: qsTr("TOP DEPARTMENT")
                    value: screen.vm ? screen.vm.topDepartment : "—"
                    caption: screen.vm ? (String(screen.vm.topDepartmentVisits) + qsTr(" visits")) : ""
                }
            }
            // ...and a single "No report data" state when the result is empty.
            LCard {
                objectName: "kpiEmptyState"
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                visible: !screen.hasRows
                Text {
                    anchors.centerIn: parent
                    text: qsTr("No report data")
                    textFormat: Text.PlainText
                    color: Theme.mutedTextCaption
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.body
                }
            }

            LCard {
                objectName: "reportChartCard"
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                visible: screen.hasRows
                LBarChart {
                    objectName: "reportBarChart"
                    anchors.fill: parent
                    orientation: "Horizontal"
                    model: screen.vm ? screen.vm.courseBars : null
                    maxValue: (screen.vm && screen.vm.courseBars) ? screen.vm.courseBars.maxValue : 100
                }
            }

            // Top-10 rankings — Students / Courses / Departments.
            RowLayout {
                objectName: "rankingsRow"
                Layout.fillWidth: true
                spacing: Theme.spacing.lg
                visible: screen.hasRows

                ColumnLayout {
                    objectName: "topStudentsTable"
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spacing.xs
                    Text {
                        text: qsTr("Top 10 Students"); textFormat: Text.PlainText
                        color: Theme.text; font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle; font.bold: true
                    }
                    Text {
                        visible: !screen.vm || !screen.vm.topStudents || screen.vm.topStudents.count === 0
                        text: qsTr("No data available."); textFormat: Text.PlainText
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                    }
                    Repeater {
                        model: screen.vm ? screen.vm.topStudents : null
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing.sm
                            Text { text: rank + "."; textFormat: Text.PlainText; color: Theme.text }
                            Text { text: label; textFormat: Text.PlainText; color: Theme.text; Layout.fillWidth: true }
                            Text { text: sublabel; textFormat: Text.PlainText; color: Theme.mutedTextCaption }
                            Text { text: String(visits); textFormat: Text.PlainText; color: Theme.text }
                        }
                    }
                }

                ColumnLayout {
                    objectName: "topCoursesTable"
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spacing.xs
                    Text {
                        text: qsTr("Top 10 Courses"); textFormat: Text.PlainText
                        color: Theme.text; font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle; font.bold: true
                    }
                    Text {
                        visible: !screen.vm || !screen.vm.topCourses || screen.vm.topCourses.count === 0
                        text: qsTr("No data available."); textFormat: Text.PlainText
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                    }
                    Repeater {
                        model: screen.vm ? screen.vm.topCourses : null
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing.sm
                            Text { text: rank + "."; textFormat: Text.PlainText; color: Theme.text }
                            Text { text: label; textFormat: Text.PlainText; color: Theme.text; Layout.fillWidth: true }
                            Text { text: String(visits); textFormat: Text.PlainText; color: Theme.text }
                            Text { text: percent.toFixed(0) + "%"; textFormat: Text.PlainText; color: Theme.mutedTextCaption }
                        }
                    }
                }

                ColumnLayout {
                    objectName: "topDepartmentsTable"
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spacing.xs
                    Text {
                        text: qsTr("Top 10 Departments"); textFormat: Text.PlainText
                        color: Theme.text; font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle; font.bold: true
                    }
                    Text {
                        visible: !screen.vm || !screen.vm.topDepartments || screen.vm.topDepartments.count === 0
                        text: qsTr("No data available."); textFormat: Text.PlainText
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                    }
                    Repeater {
                        model: screen.vm ? screen.vm.topDepartments : null
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing.sm
                            Text { text: rank + "."; textFormat: Text.PlainText; color: Theme.text }
                            Text { text: label; textFormat: Text.PlainText; color: Theme.text; Layout.fillWidth: true }
                            Text { text: String(visits); textFormat: Text.PlainText; color: Theme.text }
                            Text { text: percent.toFixed(0) + "%"; textFormat: Text.PlainText; color: Theme.mutedTextCaption }
                        }
                    }
                }
            }

            // --- "When do students visit?" (spec 4b-iv-a §6) — below KPIs/
            // rankings/course chart, gated with the rest of the analytics
            // scaffolding on hasRows. Its OWN state (spinner / data / empty /
            // inline error) reflects only the time request's outcome. ---
            LCard {
                objectName: "whenSection"
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                visible: screen.hasRows

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("When do students visit?")
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                        font.bold: true
                    }

                    // Loading: the section's OWN spinner, bound to timeLoading —
                    // NOT the operation flag and NOT the rows loading/opacity, so
                    // the main report renders at full opacity as soon as rows
                    // settle while only this section keeps spinning (spec §6).
                    Item {
                        objectName: "whenLoading"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: screen.vm ? screen.vm.timeLoading : false
                        BusyIndicator { anchors.centerIn: parent; running: parent.visible }
                    }

                    // Inline time-error (localized — the rest of the report above
                    // still renders, spec §5.2).
                    Text {
                        objectName: "whenError"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: screen.vm ? (!screen.vm.timeLoading && screen.vm.timeError.length > 0) : false
                        text: qsTr("Couldn't load visit times")
                        textFormat: Text.PlainText
                        color: Theme.error
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        verticalAlignment: Text.AlignVCenter
                    }

                    // Success + all-zero: empty state.
                    Text {
                        objectName: "whenEmpty"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: screen.vm ? (!screen.vm.timeLoading
                                              && screen.vm.timeError.length === 0
                                              && !screen.vm.hasTimeData) : false
                        text: qsTr("No visit activity in this range")
                        textFormat: Text.PlainText
                        color: Theme.mutedTextCaption
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                        verticalAlignment: Text.AlignVCenter
                    }

                    // Success + data: both charts + captions. Captions live INSIDE
                    // this subtree (not merely hidden by opacity), so the "Busiest:"
                    // strings never render in the loading/empty/error states where
                    // the peak indices are meaningless (spec §6).
                    ColumnLayout {
                        objectName: "whenData"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.spacing.sm
                        visible: screen.vm ? (!screen.vm.timeLoading
                                              && screen.vm.timeError.length === 0
                                              && screen.vm.hasTimeData) : false

                        Text {
                            text: qsTr("Peak hours"); textFormat: Text.PlainText
                            color: Theme.mutedTextCaption; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.eyebrow
                        }
                        LBarChart {
                            objectName: "hourlyChart"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90
                            orientation: "Vertical"
                            model: screen.vm ? screen.vm.hourlyBars : null
                            maxValue: (screen.vm && screen.vm.hourlyBars) ? screen.vm.hourlyBars.maxValue : 100
                        }
                        Text {
                            objectName: "busiestHourCaption"
                            text: qsTr("Busiest: %1").arg(screen.vm ? screen.vm.busiestHourLabel : "")
                            visible: screen.vm ? screen.vm.busiestHourLabel.length > 0 : false
                            textFormat: Text.PlainText
                            color: Theme.text; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }

                        Text {
                            text: qsTr("Busiest days"); textFormat: Text.PlainText
                            color: Theme.mutedTextCaption; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.eyebrow
                        }
                        LBarChart {
                            objectName: "weekdayChart"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90
                            orientation: "Vertical"
                            model: screen.vm ? screen.vm.weekdayBars : null
                            maxValue: (screen.vm && screen.vm.weekdayBars) ? screen.vm.weekdayBars.maxValue : 100
                        }
                        Text {
                            objectName: "busiestDayCaption"
                            text: qsTr("Busiest: %1").arg(screen.vm ? screen.vm.busiestDayLabel : "")
                            textFormat: Text.PlainText
                            color: Theme.text; font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
                    }
                }
            }

            LButton {
                objectName: "viewRosterToggle"
                text: qsTr("View full roster")
                variant: "Outline"
                visible: screen.hasRows
                onClicked: screen.showRoster = !screen.showRoster
            }

            LTable {
                objectName: "reportTable"
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                visible: screen.showRoster
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

        // --- Export bar (visible only when there is a non-empty result) ---
        Rectangle {
            Layout.fillWidth: true
            visible: screen.vm ? (screen.vm.hasResult && screen.vm.rows && screen.vm.rows.count > 0) : false
            color: Theme.card; radius: Theme.radius.card
            border.width: 2; border.color: Theme.border
            implicitHeight: exportCol.implicitHeight + Theme.spacing.xl * 2

            ColumnLayout {
                id: exportCol
                anchors.fill: parent
                anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md

                LCheckbox {
                    objectName: "includeRosterCheck"
                    label: qsTr("Include detailed roster in export")
                    checked: screen.vm ? screen.vm.includeRosterInExport : false
                    onToggled: function(c) { if (screen.vm) screen.vm.setIncludeRosterInExport(c); }
                }

                RowLayout {
                    id: exportRow
                    Layout.fillWidth: true
                    spacing: Theme.spacing.md

                    LComboBox {
                        id: paletteCombo
                        objectName: "paletteCombo"
                        accessibleName: qsTr("Report palette")
                        Layout.preferredWidth: 150
                        model: screen.vm ? screen.vm.palettes : []
                        placeholder: qsTr("Palette")
                        currentValue: screen.vm ? screen.vm.palette : "Default"
                        onSelected: function(v) { if (screen.vm) screen.vm.setPalette(v); }
                    }
                    LComboBox {
                        id: chartTypeCombo
                        objectName: "chartTypeCombo"
                        accessibleName: qsTr("Chart type")
                        Layout.preferredWidth: 150
                        model: screen.vm ? screen.vm.chartTypes : []
                        placeholder: qsTr("Chart")
                        currentValue: screen.vm ? screen.vm.chartType : "Bar"
                        onSelected: function(v) { if (screen.vm) screen.vm.setChartType(v); }
                    }

                    Item { Layout.fillWidth: true }   // spacer

                    // Design OS §4.5: disabled controls expose WHY via Accessible.description.
                    LButton {
                        objectName: "exportPdfButton"
                        text: qsTr("Export PDF")
                        accessibleName: qsTr("Export PDF")
                        enabled: screen.canExport
                        Accessible.description: screen.canExport ? "" : qsTr("Generate a report with results to enable export")
                        onClicked: exportPdfDialog.open()
                    }
                    LButton {
                        objectName: "exportExcelButton"
                        text: qsTr("Export Excel")
                        accessibleName: qsTr("Export Excel")
                        variant: "Outline"
                        enabled: screen.canExport
                        Accessible.description: screen.canExport ? "" : qsTr("Generate a report with results to enable export")
                        onClicked: exportExcelDialog.open()
                    }
                    LButton {
                        objectName: "printButton"
                        text: qsTr("Print")
                        accessibleName: qsTr("Print report")
                        variant: "Outline"
                        enabled: screen.canExport
                        Accessible.description: screen.canExport ? "" : qsTr("Generate a report with results to enable export")
                        onClicked: if (screen.vm) screen.vm.printReport()
                    }
                }
            }
        }

        // Empty-state affordance (Design OS #4): a report was generated but has no rows.
        Text {
            objectName: "exportEmptyState"
            Layout.fillWidth: true
            visible: screen.vm ? (screen.vm.hasResult && (!screen.vm.rows || screen.vm.rows.count === 0)) : false
            text: qsTr("No data to export. Adjust the filters and generate a report with results.")
            textFormat: Text.PlainText
            color: Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            wrapMode: Text.WordWrap
        }

        // Export feedback: transient success / persistent error (Design OS #5).
        Text {
            objectName: "exportFeedback"
            Layout.fillWidth: true
            visible: text.length > 0
            text: screen.vm ? (screen.vm.exportError.length > 0 ? screen.vm.exportError : screen.vm.exportStatus) : ""
            textFormat: Text.PlainText
            color: (screen.vm && screen.vm.exportError.length > 0) ? Theme.error : Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
            wrapMode: Text.WordWrap
        }

        FileDialog {
            id: exportPdfDialog
            fileMode: FileDialog.SaveFile
            nameFilters: [qsTr("PDF document (*.pdf)")]
            defaultSuffix: "pdf"
            onAccepted: if (screen.vm) screen.vm.exportPdf(selectedFile)
        }
        FileDialog {
            id: exportExcelDialog
            fileMode: FileDialog.SaveFile
            nameFilters: [qsTr("Excel workbook (*.xlsx)")]
            defaultSuffix: "xlsx"
            onAccepted: if (screen.vm) screen.vm.exportExcel(selectedFile)
        }
        }
    }

    // Export busy overlay — blocks input; announces progress (Design OS #7/a11y).
    Rectangle {
        objectName: "exportBusyOverlay"
        anchors.fill: parent
        visible: screen.vm ? screen.vm.exporting : false
        color: Qt.alpha(Theme.appBackground, 0.7)
        z: 100
        Accessible.role: Accessible.Indicator
        Accessible.name: qsTr("Exporting report")
        MouseArea { anchors.fill: parent }   // swallow clicks
        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacing.md
            BusyIndicator { running: parent.visible; Layout.alignment: Qt.AlignHCenter }
            Text {
                text: qsTr("Exporting…")
                textFormat: Text.PlainText
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
            }
        }
    }
}
