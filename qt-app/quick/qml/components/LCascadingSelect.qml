import QtQuick
import QtQuick.Layouts
import LOAMS

// Reusable cascading filter (spec §3.4, Dept->Course; Year deferred to 4b —
// see the plan's Global Constraints). Presentational: primitive list props,
// no vm. "All" = no filter (emits ""). Picking a department clears the course
// and asks the consumer to re-scope `courses` (departmentPicked); if the new
// `courses` no longer contains the current course, it self-clears.
RowLayout {
    id: root
    property var departments: []
    property var courses: []
    property string department: ""
    property string course: ""
    signal departmentPicked(string department)
    signal coursePicked(string course)
    spacing: Theme.spacing.md

    readonly property var deptModel: ["All"].concat(root.departments)
    readonly property var courseModel: ["All"].concat(root.courses)

    onCoursesChanged: {
        if (root.course !== "" && root.courses.indexOf(root.course) === -1)
            root.course = "";
    }

    LComboBox {
        objectName: "cascDept"
        Layout.fillWidth: true
        model: root.deptModel
        placeholder: qsTr("All Departments")
        currentValue: root.department === "" ? "" : root.department
        onSelected: function(value) {
            var picked = (value === "All") ? "" : value;
            root.department = picked;
            root.course = "";                 // dependent-clear
            root.departmentPicked(picked);
        }
    }
    LComboBox {
        objectName: "cascCourse"
        Layout.fillWidth: true
        model: root.courseModel
        placeholder: qsTr("All Courses")
        currentValue: root.course === "" ? "" : root.course
        onSelected: function(value) {
            var picked = (value === "All") ? "" : value;
            root.course = picked;
            root.coursePicked(picked);
        }
    }
}
