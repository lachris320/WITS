import QtQuick
import LOAMS

// Inline, non-blocking status surface (§11). Assertive live-region semantics.
Rectangle {
    id: toast
    property string message: ""
    property string severity: "Info"   // Info | Success | Warning | Error
    property int autoDismissMs: Theme.motion.toastHold
    signal dismissed()

    visible: message.length > 0
    color: severity === "Error" ? Theme.errorSoft : Theme.card
    radius: Theme.radius.md
    border.width: 2
    border.color: severity === "Error" ? Theme.errorBorder : Theme.border
    implicitHeight: 40
    implicitWidth: contentText.implicitWidth + Theme.spacing.xxl

    // Auto-dismiss: (re)start whenever a non-empty message is shown.
    onMessageChanged: if (message.length > 0 && autoDismissMs > 0) dismissTimer.restart()
    Timer {
        id: dismissTimer
        interval: toast.autoDismissMs
        repeat: false
        onTriggered: { toast.message = ""; toast.dismissed() }
    }

    Text {
        id: contentText
        objectName: "toastText"
        anchors.centerIn: parent
        text: toast.message
        // toast.message is consumer-supplied and, via the Database delete
        // flow, can carry a server error `message` straight from the
        // backend (DatabaseViewModel::onDeleteFinished's generic-error
        // branch) over cleartext HTTP. Text defaults to AutoText, which
        // auto-detects and RENDERS rich text (including remote <img>
        // fetches). Pinned plain here so no consumer has to remember
        // (mirrors LDialog.qml's dialogTitleText/dialogMessageText guard).
        textFormat: Text.PlainText
        color: toast.severity === "Success" ? Theme.success
             : toast.severity === "Error" ? Theme.error : Theme.text
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
    }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: toast.message
}
