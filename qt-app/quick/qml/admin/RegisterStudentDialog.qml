import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import LOAMS

// New-student registration form (Phase 4a.2b-iv). An LDialog-based modal driven
// by plain `visible`. Takes `property var vm` (a DatabaseViewModel, or a
// plain-QML stub in QuickTests). A FRESH form — no prefill — so it carries no
// prefill-guard machinery, only the narrow dept->course re-sync (LComboBox
// severs its own binding on pick). Deliberately parallel to StudentEditDialog so
// a later shared-base extraction is mechanical.
LDialog {
    id: root
    property var vm
    title: qsTr("Register student")

    // `resetting` guards the on-open control reset so pushing placeholder state
    // into the combos/fields doesn't re-enter the vm setters. (selectValue("")
    // EMITS selected(""), and text="" fires onTextChanged.)
    property bool resetting: false

    // Re-sync the Course combo whenever the vm clears/changes regCourse (a real
    // department change sets it to ""); and on a duplicate result, refocus the
    // School ID field and select its text so the fix is one keystroke away.
    Connections {
        target: root.vm ? root.vm : null
        function onRegCourseChanged() { courseCombo.selectValue(root.vm.regCourse); }
        function onRegDuplicateChanged() {
            if (root.vm.regDuplicate) {
                schoolIdField.forceFieldFocus();
                schoolIdField.selectAllText();
            }
        }
    }

    onVisibleChanged: if (visible && root.vm) {
        // Reset the CONTROLS to match the vm's already-clean state (beginRegister
        // ran before this opened). resetting=true so none of these push to the vm.
        root.resetting = true;
        schoolIdField.text = "";
        nameField.text = "";
        codeField.text = "";
        yearField.text = "";
        deptCombo.selectValue("");
        courseCombo.selectValue("");
        genderCombo.selectValue("");
        statusCombo.selectValue("");
        root.resetting = false;
        schoolIdField.forceFieldFocus();   // autofocus School ID (also the wedge-scan target)
    }

    // Enter-to-submit, guarded so a bare School-ID scan (Name still blank ->
    // canRegister false) can't half-submit.
    function trySubmit() {
        if (root.vm && root.vm.canRegister && !root.vm.regBusy)
            root.vm.registerStudent();
    }
    // Esc-to-cancel (blocked mid-request). The scrim is non-dismissing (LDialog),
    // so an accidental click never discards a filled form.
    Keys.onEscapePressed: if (root.vm && !root.vm.regBusy) root.visible = false;

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        LTextField {
            id: schoolIdField
            objectName: "regSchoolIdField"
            Layout.fillWidth: true
            label: qsTr("School ID *")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegSchoolId(text)
            onAccepted: root.trySubmit()
        }
        Text {
            objectName: "regDuplicateError"
            visible: root.vm ? root.vm.regDuplicate : false
            text: qsTr("This School ID already exists.")
            textFormat: Text.PlainText
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        LTextField {
            id: nameField
            objectName: "regNameField"
            Layout.fillWidth: true
            label: qsTr("Name *")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegName(text)
            onAccepted: root.trySubmit()
        }

        LTextField {
            id: codeField
            objectName: "regCodeField"
            Layout.fillWidth: true
            label: qsTr("Code")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegCode(text)
            onAccepted: root.trySubmit()
        }

        LComboBox {
            id: deptCombo
            objectName: "regDeptCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.departments : []
            placeholder: qsTr("Department")
            onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegDepartment(value); }
        }
        LComboBox {
            id: courseCombo
            objectName: "regCourseCombo"
            Layout.fillWidth: true
            model: root.vm ? root.vm.regCourses : []
            placeholder: qsTr("Course")
            onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegCourse(value); }
        }

        LTextField {
            id: yearField
            objectName: "regYearField"
            Layout.fillWidth: true
            label: qsTr("Year Level")
            placeholder: qsTr("e.g. 1, 2, 3, 4")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegYearLevel(text)
            onAccepted: root.trySubmit()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LComboBox {
                id: genderCombo
                objectName: "regGenderCombo"
                Layout.fillWidth: true
                model: ["Male", "Female"]
                placeholder: qsTr("Gender")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegGender(value); }
            }
            LComboBox {
                id: statusCombo
                objectName: "regStatusCombo"
                Layout.fillWidth: true
                model: ["Active", "Inactive"]
                placeholder: qsTr("Status")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegStatus(value); }
            }
        }

        // Photo (optional) — no preview; only the picked filename + constraints.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "regChoosePhotoButton"
                variant: "Outline"
                compact: true
                text: (root.vm && root.vm.regPhotoName.length > 0)
                      ? qsTr("Change photo…") : qsTr("Choose photo…")
                onClicked: photoDialog.open()
            }
            Text {
                objectName: "regPhotoLabel"
                Layout.fillWidth: true
                text: (root.vm && root.vm.regPhotoName.length > 0)
                      ? root.vm.regPhotoName
                      : qsTr("No photo selected — JPG, PNG, or GIF, up to 5MB")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: (root.vm && root.vm.regPhotoName.length > 0) ? Theme.text : Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            LButton {
                objectName: "regRemovePhotoButton"
                variant: "Ghost"
                compact: true
                text: qsTr("Remove")
                visible: root.vm ? root.vm.regPhotoName.length > 0 : false
                onClicked: if (root.vm) root.vm.clearRegPhoto()
            }
        }

        Text {
            objectName: "regRequiredCaption"
            text: qsTr("* required")
            textFormat: Text.PlainText
            color: Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing.md
            LButton {
                objectName: "regCancelButton"
                variant: "Outline"
                text: qsTr("Cancel")
                enabled: root.vm ? !root.vm.regBusy : true
                onClicked: root.visible = false
            }
            LButton {
                objectName: "regSubmitButton"
                text: (root.vm && root.vm.regBusy) ? qsTr("Registering…") : qsTr("Register")
                enabled: root.vm ? (root.vm.canRegister && !root.vm.regBusy) : false
                onClicked: root.trySubmit()
            }
        }
    }

    FileDialog {
        id: photoDialog
        objectName: "regPhotoDialog"
        nameFilters: ["Images (*.jpg *.jpeg *.png *.gif)"]
        onAccepted: if (root.vm) root.vm.setRegPhoto(selectedFile)
    }
}
