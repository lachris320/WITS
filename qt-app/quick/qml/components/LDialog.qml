import QtQuick
import QtQuick.Layouts
import LOAMS

// The only modal-interruption primitive 2.0 keeps (§11). A self-contained
// overlay (scrim + centered card) — NOT a Controls.Dialog — so screens drop
// arbitrary content into it and drive it with plain `visible`.
Item {
    id: dlg
    property string title: ""
    property string message: ""
    default property alias content: body.data

    anchors.fill: parent
    visible: false

    Rectangle {                                   // scrim: dim + swallow clicks
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing.xxxl * 2, 460)
        implicitHeight: card.implicitHeight + Theme.spacing.xxl * 2
        color: Theme.card
        radius: Theme.radius.card
        border.width: 2
        border.color: Theme.border

        ColumnLayout {
            id: card
            anchors.fill: parent
            anchors.margins: Theme.spacing.xxl
            spacing: Theme.spacing.lg
            // title/message are consumer-supplied strings and Text defaults to
            // AutoText, which auto-detects and RENDERS rich text (including
            // remote <img> fetches). A consumer that ever pipes a server
            // message in here — the backend is reached over cleartext HTTP —
            // must not turn this dialog into a markup renderer. Pinned plain
            // in the primitive so no consumer has to remember.
            Text {
                objectName: "dialogTitleText"
                visible: dlg.title.length > 0
                text: dlg.title
                textFormat: Text.PlainText
                color: Theme.text
                font.family: Theme.typography.serif
                font.pixelSize: Theme.typography.cardTitle
                font.weight: Font.Bold
            }
            Text {
                objectName: "dialogMessageText"
                visible: dlg.message.length > 0
                text: dlg.message
                textFormat: Text.PlainText
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Item {                                 // consumer content lands here
                id: body
                // Explicit width binding (not just Layout.fillWidth) so the
                // slotted content itself has something concrete to bind to:
                // Layout.fillWidth only sizes `body` within `card` — it does
                // NOT propagate to body's own children (the default `content`
                // alias just parents them into body's data list, outside the
                // Layout machinery). Consumers bind their root layout to
                // `width: parent.width` to stretch into this.
                width: card.width
                implicitHeight: childrenRect.height
            }
        }
    }

    Accessible.role: Accessible.Dialog
    Accessible.name: dlg.title
}
