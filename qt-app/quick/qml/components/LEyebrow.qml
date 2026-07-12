import QtQuick
import LOAMS

// Small-caps eyebrow label (kiosk section labels). `text`/`color` are Text's
// own properties, set per call site. letterSpacing defaults to 1.4 (matches
// 2 of 3 existing call sites); BrandPanel overrides it to 1.6.
Text {
    font.family: Theme.typography.sans
    font.pixelSize: Theme.typography.eyebrow
    font.weight: Font.ExtraBold
    font.letterSpacing: 1.4
}
