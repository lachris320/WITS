import QtQuick
import LOAMS

// Small pulsing status dot (kiosk live/scan indicators). `color` is
// Rectangle's own property; `pulseDuration` has no default — callers must
// state it explicitly since existing kiosk usages disagree (900 vs 800).
Rectangle {
    id: dot
    required property int pulseDuration

    width: 7; height: 7; radius: 3.5

    SequentialAnimation on opacity {
        running: Theme.motion.enabled; loops: Animation.Infinite
        NumberAnimation { to: 1.0; duration: dot.pulseDuration }
        NumberAnimation { to: 0.45; duration: dot.pulseDuration }
    }
}
