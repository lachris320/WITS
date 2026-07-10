import QtQuick
import LOAMS

// Inline, non-blocking status surface (§11). Assertive live-region semantics.
Rectangle {
    id: toast
    property string message: ""
    property string severity: "Info"   // Info | Success | Warning | Error
    property int autoDismissMs: Theme.motion.toastIn

    visible: message.length > 0
    color: severity === "Error" ? Theme.errorSoft : Theme.card
    radius: Theme.radius.md
    border.width: 2
    border.color: severity === "Error" ? Theme.errorBorder : Theme.border
    implicitHeight: 40
    implicitWidth: contentText.implicitWidth + Theme.spacing.xxl

    Text {
        id: contentText
        anchors.centerIn: parent
        text: toast.message
        color: toast.severity === "Success" ? Theme.success
             : toast.severity === "Error" ? Theme.error : Theme.text
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
    }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: toast.message
}
