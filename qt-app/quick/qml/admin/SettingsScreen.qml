import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LOAMS

// Settings screen (spec §4.1) — School identity, Administrator (+ key change),
// Library hours / guest login, and the tier-2 Reset Visits flow.
//
// `property var vm` so QuickTests can inject a plain-QML stub. Every VM access
// is guarded (`vm ? ... : ...` / `if (vm)`) because the stub is assigned after
// construction and because the screen must render standalone with no vm at all.
Rectangle {
    id: screen
    property var vm
    color: Theme.appBackground

    // NB: no Component.onCompleted network call here — load()/loadDepartments()
    // are gated by AdminScreen's Loader.onLoaded (the established pattern), so
    // QuickTests instantiating this screen with a stub vm stay network-free.

    // FileDialog hands back a QUrl ("file:///C:/pics/logo.png"), but
    // SettingsViewModel::importLogo() takes a filesystem PATH and forwards it to
    // SettingsController::importLogo(), which does QFile::exists()/QFile::copy()
    // — both of which fail on a "file://" string. QML has no urlToLocalFile, so
    // convert here. Handles the Windows ("file:///C:/x" -> "C:/x") and POSIX
    // ("file:///home/x" -> "/home/x") forms, and percent-decodes.
    function toLocalPath(fileUrl) {
        var s = decodeURIComponent(fileUrl.toString());
        if (s.indexOf("file:///") !== 0)
            return s;
        return s.charAt(9) === ":" ? s.substring(8) : s.substring(7);
    }

    function openLogoDialog() { logoDialog.open() }

    // Pre-fill the save name imperatively (not as a binding): the dialog writes
    // selectedFile back when the user picks a file, and a live binding would
    // clobber that choice on the next departments/selection change.
    function openManifestDialog() {
        manifestSaveDialog.selectedFile =
            "Reset_Manifest_" + deptPicker.currentValue + "_"
            + Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss") + ".csv";
        manifestSaveDialog.open();
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true

        ColumnLayout {
            id: content
            objectName: "settingsContent"
            width: flick.width
            spacing: Theme.spacing.xl

            // --- School identity ---
            LCard {
                id: schoolCard
                Layout.fillWidth: true
                padding: Theme.spacing.lg
                // LCard's own implicitHeight is a fixed 96 and its default
                // `content` slot parents children into a padded Item, so a
                // content-sized card in a Flickable must drive its height from
                // the inner column (nothing here stretches it via fillHeight).
                implicitHeight: schoolColumn.implicitHeight + padding * 2

                ColumnLayout {
                    id: schoolColumn
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("School")
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                    }
                    LTextField {
                        objectName: "schoolNameField"
                        Layout.fillWidth: true
                        label: qsTr("Name")
                        text: vm ? vm.schoolName : ""
                        onTextChanged: if (vm) vm.schoolName = text
                    }
                    LTextField {
                        objectName: "schoolAddressField"
                        Layout.fillWidth: true
                        label: qsTr("Address")
                        text: vm ? vm.schoolAddress : ""
                        onTextChanged: if (vm) vm.schoolAddress = text
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.md
                        Image {
                            objectName: "logoPreview"
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            fillMode: Image.PreserveAspectFit
                            source: vm && vm.hasLogo ? vm.logoUrl : ""
                            visible: vm ? vm.hasLogo : false
                        }
                        LButton {
                            objectName: "importLogoButton"
                            variant: "Outline"
                            text: qsTr("Import Logo…")
                            onClicked: screen.openLogoDialog()
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // --- Administrator ---
            LCard {
                id: adminCard
                Layout.fillWidth: true
                padding: Theme.spacing.lg
                implicitHeight: adminColumn.implicitHeight + padding * 2

                ColumnLayout {
                    id: adminColumn
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("Administrator")
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                    }
                    LTextField {
                        objectName: "adminNameField"
                        Layout.fillWidth: true
                        label: qsTr("Name")
                        text: vm ? vm.adminName : ""
                        onTextChanged: if (vm) vm.adminName = text
                    }
                    LTextField {
                        objectName: "adminPositionField"
                        Layout.fillWidth: true
                        label: qsTr("Position")
                        text: vm ? vm.adminPosition : ""
                        onTextChanged: if (vm) vm.adminPosition = text
                    }
                    LButton {
                        objectName: "saveAdminInfoButton"
                        variant: "Primary"
                        text: qsTr("Save Admin Info")
                        enabled: vm ? !vm.busy : false
                        onClicked: if (vm) vm.saveAdminInfo()
                    }

                    // Change-key subform. The three fields are deliberately NOT
                    // bound to the VM — the key never round-trips through a
                    // Q_PROPERTY, it is passed straight to changeAdminKey().
                    Text {
                        text: qsTr("Change Admin Key")
                        color: Theme.mutedText
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.control
                    }
                    LTextField {
                        id: oldKeyField
                        objectName: "oldKeyField"
                        Layout.fillWidth: true
                        label: qsTr("Current Key")
                        isPassword: true
                    }
                    LTextField {
                        id: newKeyField
                        objectName: "newKeyField"
                        Layout.fillWidth: true
                        label: qsTr("New Key")
                        isPassword: true
                    }
                    LTextField {
                        id: confirmKeyField
                        objectName: "confirmNewKeyField"
                        Layout.fillWidth: true
                        label: qsTr("Confirm New Key")
                        isPassword: true
                    }
                    LButton {
                        objectName: "changeKeyButton"
                        variant: "Danger"
                        text: qsTr("Change Key")
                        enabled: vm ? (!vm.busy
                                   && newKeyField.text.length > 0
                                   && newKeyField.text === confirmKeyField.text) : false
                        onClicked: if (vm) vm.changeAdminKey(oldKeyField.text, newKeyField.text)
                    }
                }
            }

            // --- Library hours + guest toggle ---
            LCard {
                id: hoursCard
                Layout.fillWidth: true
                padding: Theme.spacing.lg
                implicitHeight: hoursColumn.implicitHeight + padding * 2

                ColumnLayout {
                    id: hoursColumn
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("Library Hours")
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.md
                        SpinBox {
                            objectName: "openHourSpin"
                            from: 0; to: 23
                            value: vm ? vm.openHour : 8
                            onValueModified: if (vm) vm.openHour = value
                        }
                        Text {
                            text: qsTr("to")
                            color: Theme.mutedText
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
                        SpinBox {
                            objectName: "closeHourSpin"
                            from: 0; to: 23
                            value: vm ? vm.closeHour : 17
                            onValueModified: if (vm) vm.closeHour = value
                        }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.md
                        Text {
                            text: qsTr("Guest Login")
                            color: Theme.text
                            font.family: Theme.typography.sans
                            font.pixelSize: Theme.typography.body
                        }
                        Switch {
                            objectName: "guestToggle"
                            checked: vm ? vm.guestEnabled : false
                            onToggled: if (vm) vm.guestEnabled = checked
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // --- Reset visits (tier-2 destructive) ---
            LCard {
                id: resetCard
                Layout.fillWidth: true
                padding: Theme.spacing.lg
                implicitHeight: resetColumn.implicitHeight + padding * 2

                ColumnLayout {
                    id: resetColumn
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("Reset Visits")
                        color: Theme.error
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                    }
                    Text {
                        objectName: "resetWarningText"
                        Layout.fillWidth: true
                        text: qsTr("Permanently deletes the selected department's visit history and cannot be undone. Download the Reset Manifest first — it records what was reset (department, time, operator). The full pre-reset visit-row export arrives in Phase 4a when the visit-fetch path exists.")
                        color: Theme.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                    }
                    LComboBox {
                        id: deptPicker
                        objectName: "resetDeptPicker"
                        Layout.fillWidth: true
                        placeholder: qsTr("Select Department")
                        model: vm ? vm.departments : []
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.md
                        LButton {
                            objectName: "manifestButton"
                            variant: "Outline"
                            text: qsTr("Download Reset Manifest")
                            enabled: deptPicker.currentValue.length > 0
                            onClicked: screen.openManifestDialog()
                        }
                        LButton {
                            objectName: "resetVisitsButton"
                            variant: "Danger"
                            text: qsTr("Reset Visits…")
                            enabled: deptPicker.currentValue.length > 0
                            onClicked: resetConfirm.visible = true
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // --- Footer save ---
            LButton {
                objectName: "saveButton"
                Layout.alignment: Qt.AlignRight
                variant: "Primary"
                text: qsTr("Save")
                enabled: vm ? (vm.dirty && !vm.busy) : false
                onClicked: if (vm) vm.save()
            }
        }
    }

    // Tier-2 reset-visits confirm. onConfirmed(key) forwards the FRESHLY
    // re-typed key (spec §3.3) — never the held AdminSession key.
    LConfirmDialog {
        id: resetConfirm
        objectName: "resetConfirmDialog"
        tier: 2
        title: qsTr("Reset Visits")
        message: qsTr("This permanently deletes the department's visit history and cannot be undone.")
        confirmText: qsTr("Reset")
        busy: vm ? vm.busy : false
        onConfirmed: function(key) { if (vm) vm.resetVisits(deptPicker.currentValue, key) }
    }
    // Hide the dialog once the VM reports the reset finished. (Cancel already
    // hides itself inside LConfirmDialog before emitting cancelled().)
    Connections {
        target: vm
        ignoreUnknownSignals: true
        function onVisitsReset() { resetConfirm.visible = false }
    }

    FileDialog {
        id: logoDialog
        objectName: "logoDialog"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.gif)"]
        onAccepted: {
            if (!vm)
                return;
            vm.importLogo(screen.toLocalPath(selectedFile));
            // Live re-theme must run on the SAME ThemeViewModel instance
            // Theme.qml binds its tokens to (Theme._vm). Calling any other
            // instance — e.g. one owned by the VM — recomputes a palette
            // nothing is bound to and is a silent no-op in the running UI.
            if (vm.hasLogo)
                Theme._vm.regenerateFromImportedLogo(vm.logoPath);
        }
    }
    // Reset Manifest — NOT a visit backup/export: it records what was reset
    // (department, time, operator). The VM writes it via serializeCsv().
    FileDialog {
        id: manifestSaveDialog
        objectName: "manifestSaveDialog"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: ["CSV files (*.csv)"]
        onAccepted: if (vm) vm.writeResetManifest(deptPicker.currentValue, selectedFile)
    }
}
