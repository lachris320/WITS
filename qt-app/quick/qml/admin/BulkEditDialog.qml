import QtQuick
import QtQuick.Layouts
import LOAMS

// Bulk-edit form (Phase 4a.2b-iii). An LDialog-based modal driven by plain
// `visible`. Takes `property var vm` (a DatabaseViewModel, or a plain-QML stub
// in QuickTests). Each field is a tri-state row: an LCheckbox "change this"
// toggle + a value control disabled until the toggle is on. Only toggled fields
// are applied. School ID / Name are never bulk-editable. Department and Course
// are COUPLED (move together). Emits applyRequested() — the screen opens the
// change-preview confirm; the dialog stays open until vm.bulkEditFinished.
LDialog {
    id: root
    property var vm
    signal applyRequested()
    title: qsTr("Bulk edit students")

    // Same combo severance/prefill trap as StudentEditDialog: LComboBox
    // .selectValue() emits selected() and onActivated severs bindings. Guard
    // the on-open reset so it only sets displayed values, never re-enters the
    // vm setters; and re-sync the Course combo when the vm clears bulkCourse.
    property bool prefilling: false
    Connections {
        target: root.vm ? root.vm : null
        function onBulkCourseChanged() { bulkCourseCombo.selectValue(root.vm.bulkCourse); }
    }
    onVisibleChanged: if (visible && root.vm) {
        root.prefilling = true;
        // Checkboxes: set imperatively (setting `checked` does NOT emit
        // toggled — only a MouseArea click does — so no re-entry guard needed
        // for these, and re-assigning on each open HEALS the binding-severance
        // a prior click caused, so a reopened dialog never shows stale toggles.
        // Clickable checks carry NO declarative `checked:` binding (it would be
        // severed on first click anyway); only the driven Course check binds.
        deptCheck.checked   = root.vm.changeDepartment;
        yearCheck.checked   = root.vm.changeYearLevel;
        genderCheck.checked = root.vm.changeGender;
        statusCheck.checked = root.vm.changeStatus;
        // Combos: selectValue() DOES emit selected(), hence the prefilling guard.
        deptCombo.selectValue(root.vm.bulkDepartment);
        bulkCourseCombo.selectValue(root.vm.bulkCourse);
        yearField.text = root.vm.bulkYearLevel;
        genderCombo.selectValue(root.vm.bulkGender);
        statusCombo.selectValue(root.vm.bulkStatus);
        root.prefilling = false;
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        // Department (drives Course). No declarative `checked:` binding — a
        // MouseArea click imperatively writes checked (severing any binding),
        // so state is synced from the vm on open (onVisibleChanged) instead.
        LCheckbox {
            id: deptCheck
            objectName: "bulkDeptCheck"
            label: qsTr("Change Department")
            onToggled: function(on) { if (!root.prefilling && root.vm) root.vm.setChangeDepartment(on); }
        }
        LComboBox {
            id: deptCombo
            objectName: "bulkDeptCombo"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeDepartment : false
            model: root.vm ? root.vm.departments : []
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkDepartment(v); }
        }

        // Course (coupled to Department).
        LCheckbox {
            objectName: "bulkCourseCheck"
            label: qsTr("Change Course")
            // Driven by Department — checked iff Department is on.
            checked: root.vm ? root.vm.changeDepartment : false
            enabled: false                       // not independently toggleable
        }
        LComboBox {
            id: bulkCourseCombo
            objectName: "bulkCourseCombo"
            Layout.fillWidth: true
            enabled: root.vm ? (root.vm.changeDepartment && root.vm.bulkDepartment.length > 0) : false
            model: root.vm ? root.vm.bulkCourses : []
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkCourse(v); }
        }

        // Year Level.
        LCheckbox {
            id: yearCheck
            objectName: "bulkYearCheck"
            label: qsTr("Change Year Level")
            onToggled: function(on) { if (!root.prefilling && root.vm) root.vm.setChangeYearLevel(on); }
        }
        LTextField {
            id: yearField
            objectName: "bulkYearField"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeYearLevel : false
            label: qsTr("Year Level")
            onTextChanged: if (!root.prefilling && root.vm) root.vm.setBulkYearLevel(text)
        }

        // Gender.
        LCheckbox {
            id: genderCheck
            objectName: "bulkGenderCheck"
            label: qsTr("Change Gender")
            onToggled: function(on) { if (!root.prefilling && root.vm) root.vm.setChangeGender(on); }
        }
        LComboBox {
            id: genderCombo
            objectName: "bulkGenderCombo"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeGender : false
            model: ["Male", "Female"]
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkGender(v); }
        }

        // Status.
        LCheckbox {
            id: statusCheck
            objectName: "bulkStatusCheck"
            label: qsTr("Change Status")
            onToggled: function(on) { if (!root.prefilling && root.vm) root.vm.setChangeStatus(on); }
        }
        LComboBox {
            id: statusCombo
            objectName: "bulkStatusCombo"
            Layout.fillWidth: true
            enabled: root.vm ? root.vm.changeStatus : false
            model: ["Active", "Inactive"]
            onSelected: function(v) { if (!root.prefilling && root.vm) root.vm.setBulkStatus(v); }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "bulkCancelButton"
                variant: "Outline"
                text: qsTr("Cancel")
                onClicked: root.visible = false
            }
            LButton {
                objectName: "bulkApplyButton"
                text: qsTr("Apply")
                // Disabled until at least one valid change AND not mid-flight.
                enabled: root.vm ? (root.vm.canApplyBulk && !root.vm.bulkBusy) : false
                onClicked: root.applyRequested()
            }
        }
    }
}
