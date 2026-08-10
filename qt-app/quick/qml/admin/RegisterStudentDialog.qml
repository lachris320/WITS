import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import LOAMS

// New-student registration form (Phase 4a.2b-iv). LDialog-based modal driven by
// plain `visible`. Takes `property var vm` (a DatabaseViewModel, or a plain-QML
// stub in QuickTests). A FRESH form — no prefill machinery, only the narrow
// dept->course re-sync. Two-column grid so short fields pair up and every combo
// gets a guaranteed half-width (GUI-smoke: paired combos were collapsing).
LDialog {
    id: root
    property var vm
    title: qsTr("Register student")
    maxWidth: 560

    // Guards the on-open control reset so pushing placeholder state into the
    // combos/fields doesn't re-enter the vm setters.
    property bool resetting: false

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

    function trySubmit() {
        if (root.vm && root.vm.canRegister && !root.vm.regBusy)
            root.vm.registerStudent();
    }
    Keys.onEscapePressed: if (root.vm && !root.vm.regBusy) root.visible = false;

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        // Row 1: School ID (+ inline duplicate error under it) | Code
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignTop
                spacing: Theme.spacing.xs
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
                    Layout.fillWidth: true
                    text: qsTr("This School ID already exists.")
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: Theme.error
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.control
                }
            }
            LTextField {
                id: codeField
                objectName: "regCodeField"
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignTop
                label: qsTr("Code")
                onTextChanged: if (!root.resetting && root.vm) root.vm.setRegCode(text)
                onAccepted: root.trySubmit()
            }
        }

        // Row 2: Name (full width)
        LTextField {
            id: nameField
            objectName: "regNameField"
            Layout.fillWidth: true
            label: qsTr("Name *")
            onTextChanged: if (!root.resetting && root.vm) root.vm.setRegName(text)
            onAccepted: root.trySubmit()
        }

        // Row 3: Department | Course
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LComboBox {
                id: deptCombo
                objectName: "regDeptCombo"
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.minimumWidth: 140
                model: root.vm ? root.vm.departments : []
                placeholder: qsTr("Department")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegDepartment(value); }
            }
            LComboBox {
                id: courseCombo
                objectName: "regCourseCombo"
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.minimumWidth: 140
                model: root.vm ? root.vm.regCourses : []
                placeholder: qsTr("Course")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegCourse(value); }
            }
        }

        // Row 4: Gender | Status
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LComboBox {
                id: genderCombo
                objectName: "regGenderCombo"
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.minimumWidth: 140
                model: ["Male", "Female"]
                placeholder: qsTr("Gender")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegGender(value); }
            }
            LComboBox {
                id: statusCombo
                objectName: "regStatusCombo"
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.minimumWidth: 140
                model: ["Active", "Inactive"]
                placeholder: qsTr("Status")
                onSelected: function(value) { if (!root.resetting && root.vm) root.vm.setRegStatus(value); }
            }
        }

        // Row 5: Year Level | spacer (keeps Year at half width)
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LTextField {
                id: yearField
                objectName: "regYearField"
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                label: qsTr("Year Level")
                placeholder: qsTr("e.g. 1, 2, 3, 4")
                onTextChanged: if (!root.resetting && root.vm) root.vm.setRegYearLevel(text)
                onAccepted: root.trySubmit()
            }
            Item { Layout.fillWidth: true; Layout.preferredWidth: 100 }
        }

        // Photo (optional) — full width; no preview, only filename + constraints.
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
                      : qsTr("JPG, PNG, or GIF · up to 5MB")
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
