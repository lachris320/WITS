import QtQuick
import QtQuick.Controls          // ProgressBar
import QtQuick.Layouts
import QtQuick.Dialogs
import LOAMS

// Bulk student import (Phase 4a.3). LDialog-based modal driven by plain
// `visible`. Takes `property var vm` (an ImportViewModel, or a plain-QML stub
// in QuickTests). Every server-provided string renders Text.PlainText.
LDialog {
    id: root
    property var vm
    title: qsTr("Import students")
    maxWidth: 560

    // Phase constants (mirror ImportViewModel::Phase order) so the same
    // comparisons work against the C++ Q_ENUM AND the plain-int QuickTest stub.
    readonly property int phaseIdle: 0
    readonly property int phaseCheckingDuplicates: 1
    readonly property int phaseAwaitingDuplicates: 2
    readonly property int phaseUploading: 3
    readonly property int phaseProcessing: 4
    readonly property int phaseDone: 5
    readonly property int phaseFailed: 6

    readonly property int vmPhase: root.vm ? root.vm.phase : root.phaseIdle
    readonly property bool hasDataFile: root.vm ? root.vm.dataFileName.length > 0 : false

    Connections {
        target: root.vm ? root.vm : null
        function onFinishedOk() {
            // Result is shown briefly; leave the dialog open on the Done phase so
            // the operator sees the counts, then they close it.
        }
    }

    Keys.onEscapePressed: if (root.vm && !root.vm.busy) root.visible = false;

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacing.md

        // Format hint — recognized columns.
        Text {
            objectName: "importFormatHint"
            Layout.fillWidth: true
            text: qsTr("Recognized columns: School ID*, Name*, Course, Department, Year Level, Gender, Status. Extra columns are ignored; only School ID and Name are required.")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.mutedTextCaption
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        // Row 1: data file (required).
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "importChooseDataButton"
                variant: "Outline"
                compact: true
                text: root.hasDataFile ? qsTr("Change data file…") : qsTr("Choose data file…")
                enabled: root.vm ? !root.vm.busy : true
                onClicked: dataDialog.open()
            }
            Text {
                objectName: "importDataLabel"
                Layout.fillWidth: true
                text: root.hasDataFile ? root.vm.dataFileName : qsTr("Excel (.xlsx) or CSV (.csv)")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: root.hasDataFile ? Theme.text : Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }

        // Row 2: photos ZIP (optional).
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "importChooseZipButton"
                variant: "Outline"
                compact: true
                text: (root.vm && root.vm.photosZipName.length > 0)
                      ? qsTr("Change photos ZIP…") : qsTr("Choose photos ZIP…")
                enabled: root.vm ? !root.vm.busy : true
                onClicked: zipDialog.open()
            }
            Text {
                objectName: "importZipLabel"
                Layout.fillWidth: true
                text: (root.vm && root.vm.photosZipName.length > 0)
                      ? root.vm.photosZipName : qsTr("Optional · ZIP of photos named by School ID")
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                color: (root.vm && root.vm.photosZipName.length > 0) ? Theme.text : Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            LButton {
                objectName: "importRemoveZipButton"
                variant: "Ghost"
                compact: true
                text: qsTr("Remove")
                visible: root.vm ? root.vm.photosZipName.length > 0 : false
                onClicked: if (root.vm) root.vm.clearPhotosZip()
            }
        }

        // Inline error (validation / server / auth).
        Text {
            objectName: "importErrorText"
            visible: root.vm ? root.vm.errorText.length > 0 : false
            Layout.fillWidth: true
            text: root.vm ? root.vm.errorText : ""
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        // Duplicate skip-confirm — SOME duplicates (Continue / Cancel).
        ColumnLayout {
            objectName: "importDuplicateSome"
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            visible: root.vmPhase === root.phaseAwaitingDuplicates
                     && root.vm && !root.vm.allDuplicates
            Text {
                Layout.fillWidth: true
                text: root.vm
                      ? qsTr("%1 of %2 rows already exist and will be skipped.")
                          .arg(root.vm.duplicateCount).arg(root.vm.parsedCount)
                      : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.spacing.md
                LButton {
                    objectName: "importDupCancelButton"
                    variant: "Outline"
                    text: qsTr("Cancel")
                    onClicked: if (root.vm) root.vm.cancel()
                }
                LButton {
                    objectName: "importDupContinueButton"
                    text: qsTr("Continue")
                    onClicked: if (root.vm) root.vm.continueAfterDuplicates()
                }
            }
        }

        // Duplicate — ALL duplicates (Close only, nothing to import).
        ColumnLayout {
            objectName: "importDuplicateAll"
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            visible: root.vmPhase === root.phaseAwaitingDuplicates
                     && root.vm && root.vm.allDuplicates
            Text {
                Layout.fillWidth: true
                text: qsTr("Nothing to import — every row already exists.")
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
            LButton {
                objectName: "importDupCloseButton"
                Layout.alignment: Qt.AlignRight
                variant: "Outline"
                text: qsTr("Close")
                onClicked: root.visible = false
            }
        }

        // Progress: byte bar while uploading, indeterminate "Processing…" after.
        ColumnLayout {
            objectName: "importProgress"
            Layout.fillWidth: true
            spacing: Theme.spacing.sm
            visible: root.vmPhase === root.phaseUploading || root.vmPhase === root.phaseProcessing
            ProgressBar {
                objectName: "importProgressBar"
                Layout.fillWidth: true
                indeterminate: root.vmPhase === root.phaseProcessing
                from: 0; to: 100
                value: root.vm ? root.vm.uploadPercent : 0
            }
            Text {
                objectName: "importProcessingLabel"
                visible: root.vmPhase === root.phaseProcessing
                text: qsTr("Processing…")
                textFormat: Text.PlainText
                color: Theme.mutedTextCaption
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
            }
        }

        // Result line — real counts.
        Text {
            objectName: "importResultText"
            visible: root.vmPhase === root.phaseDone
            Layout.fillWidth: true
            text: root.vm ? root.vm.resultText : ""
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.text
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.control
        }

        // Footer: Download Template · Cancel/Close · Import.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            LButton {
                objectName: "importTemplateButton"
                variant: "Ghost"
                compact: true
                text: qsTr("Download template")
                onClicked: templateDialog.open()
            }
            Item { Layout.fillWidth: true }
            LButton {
                objectName: "importCloseButton"
                variant: "Outline"
                text: (root.vmPhase === root.phaseDone) ? qsTr("Close") : qsTr("Cancel")
                enabled: root.vm ? !root.vm.busy : true
                onClicked: root.visible = false
            }
            LButton {
                objectName: "importSubmitButton"
                text: qsTr("Import")
                // Enabled once a data file is chosen and we are idle (not busy/awaiting).
                enabled: root.hasDataFile && root.vm
                         && !root.vm.busy && root.vmPhase === root.phaseIdle
                onClicked: if (root.vm) root.vm.startImport()
            }
        }
    }

    FileDialog {
        id: dataDialog
        objectName: "importDataDialog"
        nameFilters: ["Student data (*.xlsx *.csv)"]
        onAccepted: if (root.vm) root.vm.setDataFile(selectedFile)
    }
    FileDialog {
        id: zipDialog
        objectName: "importZipDialog"
        nameFilters: ["Photos ZIP (*.zip)"]
        onAccepted: if (root.vm) root.vm.setPhotosZip(selectedFile)
    }
    FileDialog {
        id: templateDialog
        objectName: "importTemplateDialog"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: ["CSV (*.csv)"]
        onAccepted: if (root.vm) root.vm.downloadTemplate(selectedFile)
    }
}
