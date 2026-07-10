import QtQuick
import QtQuick.Controls
import LOAMS

// The only modal-interruption primitive 2.0 keeps (§11).
Dialog {
    id: dlg
    property string message: ""
    modal: true

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius.card
        border.width: 2
        border.color: Theme.border
    }
    contentItem: Text {
        text: dlg.message
        color: Theme.text
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
        wrapMode: Text.WordWrap
    }

    Accessible.role: Accessible.Dialog
}
