import QtQuick
import LOAMS

// Circular "loading" ring: a translucent full ring with a gold arc segment at
// the top, spinning once per `period`. Matches the kiosk reference's idle
// spinner (border-top-color gold, ringSpin 1.6s linear). Canvas-based (no
// shader-effects module in this repo). Respects reduced motion: when
// `spinning` is false it renders as a static ring (no animation).
Item {
    id: spinner

    property int size: 52
    property real thickness: 3
    property color ringColor: Qt.alpha(Theme.brand.on, 0.25)   // translucent cream
    property color accentColor: Theme.accent.base              // gold
    property int period: 1600                                  // ms per rotation
    property bool spinning: Theme.motion.enabled

    implicitWidth: size
    implicitHeight: size

    Canvas {
        id: canvas
        objectName: "spinnerCanvas"
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            var cx = width / 2, cy = height / 2;
            var r = (Math.min(width, height) - spinner.thickness) / 2;
            ctx.lineWidth = spinner.thickness;
            // full translucent ring
            ctx.strokeStyle = spinner.ringColor;
            ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
            // gold arc segment at the top (~90 degrees, centered on -90deg)
            ctx.strokeStyle = spinner.accentColor;
            ctx.beginPath();
            ctx.arc(cx, cy, r, -Math.PI / 2 - Math.PI / 4, -Math.PI / 2 + Math.PI / 4);
            ctx.stroke();
        }
        // Rotating the Canvas item rotates the whole drawing (incl. the gold
        // segment) -> the segment appears to spin, like the reference.
        RotationAnimator on rotation {
            from: 0; to: 360; duration: spinner.period
            loops: Animation.Infinite
            running: spinner.spinning
        }
    }

    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
