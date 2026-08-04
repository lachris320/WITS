import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import LOAMS

// Database (spec §4.2, increment 4a.2a): filterable, multi-selectable student
// table. Read-only here; register/edit/bulk/delete/dept-ops are 4a.2b. Takes
// `property var vm` (a DatabaseViewModel, or a plain-QML stub in QuickTests).
Rectangle {
    id: screen
    property var vm
    property real pageInT: 0
    color: Theme.appBackground

    readonly property int resultCount: vm && vm.students ? vm.students.count : 0
    readonly property int selectedCount: vm && vm.students ? vm.students.selectedCount : 0
    readonly property bool isLoading: vm ? vm.loading : false
    readonly property bool isError: vm ? vm.errorText.length > 0 : false
    readonly property bool isEmpty: !screen.isLoading && !screen.isError && screen.resultCount === 0

    NumberAnimation {
        id: pageInAnimation; objectName: "pageInAnimation"
        target: screen; property: "pageInT"; to: 1
        duration: Theme.motion.enabled ? Theme.motion.pageIn : 0
        easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing
    }
    Component.onCompleted: pageInAnimation.start()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.lg
        opacity: screen.pageInT
        transform: Translate { y: (1 - screen.pageInT) * 16 }

        // Filter card.
        Rectangle {
            Layout.fillWidth: true
            color: Theme.card; radius: Theme.radius.card
            border.width: 2; border.color: Theme.border
            implicitHeight: filterRow.implicitHeight + Theme.spacing.xl * 2
            RowLayout {
                id: filterRow
                anchors.fill: parent; anchors.margins: Theme.spacing.xl
                spacing: Theme.spacing.md
                LCascadingSelect {
                    id: cascade
                    Layout.fillWidth: true
                    departments: screen.vm ? screen.vm.departments : []
                    courses: screen.vm ? screen.vm.courses : []
                    department: screen.vm ? screen.vm.department : ""
                    course: screen.vm ? screen.vm.course : ""   // reflects the VM's dependent-clear
                    onDepartmentPicked: function(d) { if (screen.vm) screen.vm.setDepartment(d); }
                    onCoursePicked: function(c) { if (screen.vm) screen.vm.setCourse(c); }
                }
            }
        }

        // Count/selection header + row actions (Export/Delete).
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md

            Text {
                objectName: "tableCountHeader"
                text: screen.selectedCount > 0
                      ? qsTr("%1 results · %2 selected").arg(screen.resultCount).arg(screen.selectedCount)
                      : qsTr("%1 results").arg(screen.resultCount)
                color: Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                font.weight: Font.ExtraBold
            }

            Item { Layout.fillWidth: true }   // spacer pushes the actions right

            TextMetrics {
                id: exportMetrics
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                // Worst-case label width so the button never resizes on count change.
                text: qsTr("Export CSV (all %1)").arg(screen.resultCount)
            }
            LButton {
                objectName: "exportButton"
                variant: "Outline"
                compact: true
                text: screen.selectedCount > 0
                      ? qsTr("Export CSV (%1)").arg(screen.selectedCount)
                      : qsTr("Export CSV (all %1)").arg(screen.resultCount)
                // Exportable when M > 0 OR (nothing selected and) N > 0; disabled at N == 0.
                enabled: screen.selectedCount > 0 || screen.resultCount > 0
                tooltipText: qsTr("Exports selected rows, or all filtered rows if none are selected.")
                accessibleName: screen.selectedCount > 0
                                ? qsTr("Export %1 selected rows to CSV").arg(screen.selectedCount)
                                : qsTr("Export all %1 filtered rows to CSV").arg(screen.resultCount)
                Layout.minimumWidth: exportMetrics.width + Theme.spacing.xl * 2
                onClicked: exportDialog.open()
            }

            LButton {
                objectName: "editButton"
                variant: "Primary"
                compact: true
                text: qsTr("Edit")
                enabled: screen.vm ? screen.vm.canEdit : false
                tooltipText: qsTr("Select exactly one student to edit")
                accessibleName: qsTr("Edit the selected student")
                onClicked: if (screen.vm) screen.vm.beginEditSelected()
            }

            TextMetrics {
                id: deleteMetrics
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                text: qsTr("Delete (%1)").arg(screen.resultCount)
            }
            LButton {
                objectName: "deleteButton"
                variant: "Danger"
                compact: true
                text: screen.selectedCount > 0
                      ? qsTr("Delete (%1)").arg(screen.selectedCount)
                      : qsTr("Delete")
                enabled: screen.selectedCount > 0
                accessibleName: qsTr("Delete %1 selected student records").arg(screen.selectedCount)
                Layout.minimumWidth: deleteMetrics.width + Theme.spacing.xl * 2
                onClicked: deleteConfirm.visible = true
            }
        }

        // The table.
        LTable {
            id: studentsTable
            objectName: "studentsTable"
            Layout.fillWidth: true; Layout.fillHeight: true
            selectable: true
            selectionModel: screen.vm ? screen.vm.students : null
            model: screen.vm ? screen.vm.students : null
            emptyStateText: screen.isError
                            ? (screen.vm ? screen.vm.errorText : qsTr("Error"))
                            : qsTr("No students")
            columns: [
                { key: "name",       title: qsTr("Name"),       weight: 3 },
                { key: "schoolId",   title: qsTr("ID"),         weight: 2 },
                { key: "course",     title: qsTr("Course"),     weight: 2 },
                { key: "department", title: qsTr("Department"), weight: 3 },
                { key: "status",     title: qsTr("Status"),     weight: 1 },
                { key: "visits",     title: qsTr("Visits"),     weight: 1 }
            ]
            onRowActivated: function(schoolId) { if (screen.vm) screen.vm.beginEdit(schoolId); }
        }
    }

    FileDialog {
        id: exportDialog
        objectName: "exportDialog"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: ["CSV (*.csv)"]
        onAccepted: if (screen.vm) screen.vm.exportCsv(selectedFile)
    }

    LConfirmDialog {
        id: deleteConfirm
        objectName: "deleteConfirm"
        title: qsTr("Delete students?")
        // Itemized, irreversible-impact message (PlainText per LConfirmDialog).
        message: qsTr("This will permanently delete:\n• %1 student records\n• all associated visit history\n\nThis cannot be undone.")
                    .arg(screen.selectedCount)
        confirmText: qsTr("Delete")
        // M >= 10 requires typing DELETE (the VM owns the threshold).
        requireTypedConfirmation: screen.vm ? screen.vm.requiresTypedConfirmation(screen.selectedCount) : false
        confirmationWord: "DELETE"
        onConfirmed: { deleteConfirm.visible = false; if (screen.vm) screen.vm.deleteSelected(); }
    }

    LToast {
        id: databaseToast
        objectName: "databaseToast"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.xxl
    }

    // NOT a declarative `message: vm.statusMessage` binding — LToast's
    // auto-dismiss Timer imperatively sets message="" (LToast.qml), which
    // would PERMANENTLY destroy such a binding after the first toast, so
    // every later status update would be silently swallowed (same trap
    // documented in KioskScreen.qml). Raise imperatively instead: every time
    // the VM's statusMessage changes to something non-empty, push it in.
    Connections {
        target: screen.vm ? screen.vm : null
        function onStatusMessageChanged() {
            if (screen.vm.statusMessage !== "")
                databaseToast.message = screen.vm.statusMessage;
        }
    }

    StudentEditDialog {
        id: editDialog
        objectName: "editDialog"
        vm: screen.vm
    }

    // beginEdit/beginEditSelected emit editReady only when a record was located;
    // editFinished fires on save success or a no-op. Drive the modal from both.
    Connections {
        target: screen.vm ? screen.vm : null
        function onEditReady() { editDialog.visible = true; }
        function onEditFinished() { editDialog.visible = false; }
    }
}
