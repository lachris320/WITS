import QtQuick
import QtQuick.Layouts
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

        // Count/selection header.
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
        }
    }
}
