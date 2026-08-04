import QtQuick
import QtQuick.Layouts
import LOAMS

// Single-student edit form (Phase 4a.2b-ii). An LDialog-based modal driven by
// plain `visible`. Takes `property var vm` (a DatabaseViewModel, or a plain-QML
// stub in QuickTests). Edits exactly the six server-editable fields; School ID
// is shown read-only (it is the immutable identity / WHERE key).
LDialog {
    id: root
    property var vm

    // Guard against the programmatic prefill firing the vm setters.
    // LComboBox.selectValue(v) EMITS `selected(v)` (LComboBox.qml:30-35), so
    // pushing vm state into a combo on open would re-enter its onSelected and
    // call the vm setter. For Department that is destructive: setEditDepartment
    // CLEARS editCourse, which would blank the just-prefilled Course before
    // courseCombo.selectValue(editCourse) runs. While `prefilling` is true, the
    // onSelected/onTextChanged handlers skip the vm push — so the open-time
    // sync only sets each control's displayed value, never mutates the vm.
    property bool prefilling: false
    title: qsTr("Edit student")

    // LComboBox.onActivated imperatively assigns its own currentValue, which
    // severs any declarative `currentValue: vm.editX` binding after the first
    // pick. So push vm state INTO the combos via selectValue(...) on open, and
    // — critically — re-sync the Course combo whenever the vm clears/changes
    // editCourse (a real department change sets it to ""), or the dependent-clear
    // would not visibly reset the combo. This re-sync's own selectValue is
    // prefilling-safe: a genuine clear happens with prefilling=false, and its
    // re-entrant setEditCourse("") is an idempotent no-op in the vm.
    Connections {
        target: root.vm ? root.vm : null
        function onEditCourseChanged() { courseCombo.selectValue(root.vm.editCourse); }
    }
    onVisibleChanged: if (visible && root.vm) {
        root.prefilling = true;
        nameField.text = root.vm.editName;
        yearField.text = root.vm.editYearLevel;
        deptCombo.selectValue(root.vm.editDepartment);
        courseCombo.selectValue(root.vm.editCourse);
        genderCombo.selectValue(root.vm.editGender);
        statusCombo.selectValue(root.vm.editStatus);
        root.prefilling = false;
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        Text {
            objectName: "editSchoolIdText"
            text: qsTr("School ID: %1").arg(root.vm ? root.vm.editSchoolId : "")
            // Read-only identity; server-supplied — pin plain (anti-injection).
            textFormat: Text.PlainText
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        LTextField {
            id: nameField
            objectName: "editNameField"
            Layout.fillWidth: true
            label: qsTr("Name")
            // No `text:` binding — see the combo-severance note: bind-then-type
            // would sever it. Set imperatively on open (above, under prefilling),
            // push edits back to the vm here so Save-enable (vm.editName) stays
            // reactive. `!root.prefilling` skips the push during open-time sync.
            onTextChanged: if (!root.prefilling && root.vm) root.vm.setEditName(text)
        }

        LComboBox {
            id: deptCombo
            objectName: "editDeptCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.departments : []
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditDepartment(value); }
        }

        LComboBox {
            id: courseCombo
            objectName: "editCourseCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.editCourses : []
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditCourse(value); }
        }

        LTextField {
            id: yearField
            objectName: "editYearField"
            Layout.fillWidth: true
            label: qsTr("Year Level")
            onTextChanged: if (!root.prefilling && root.vm) root.vm.setEditYearLevel(text)
        }

        LComboBox {
            id: genderCombo
            objectName: "editGenderCombo"
            Layout.fillWidth: true
            model: ["Male", "Female"]
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditGender(value); }
        }

        LComboBox {
            id: statusCombo
            objectName: "editStatusCombo"
            Layout.fillWidth: true
            model: ["Active", "Inactive"]
            onSelected: function(value) { if (!root.prefilling && root.vm) root.vm.setEditStatus(value); }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "editCancelButton"
                variant: "Outline"
                text: qsTr("Cancel")
                onClicked: root.visible = false
            }
            LButton {
                objectName: "editSaveButton"
                text: qsTr("Save")
                // Required Name — disabled while blank (inline field precondition).
                enabled: root.vm && root.vm.editName.trim().length > 0
                onClicked: if (root.vm) root.vm.saveEdit()
            }
        }
    }
}
