import QtQuick
import LOAMS

// Circular student avatar: a photo (cropped via LCircleImage) or an initials
// chip fallback. Presentational: absolute-or-empty `source` + a PRECOMPUTED
// `initials` string (Initials::of() is C++-only, not QML-accessible — callers
// pass model.initials / vm.currentInitials). Falls back to initials when the
// source is empty, its path ends with default.jpg (suffix check, not filename
// equality), or the image fails to load (Image.Error).
Item {
    id: avatar

    property string source: ""
    property string initials: ""
    property int size: 40
    property color fallbackBackground: Theme.brand.soft
    property color fallbackForeground: Theme.brand.base

    // "No usable photo" — blank LCircleImage's source in these cases so it never
    // tries to load the default.jpg sentinel (a valid URL that would otherwise
    // paint), and shows the initials placeholder instead.
    readonly property bool _emptyOrSentinel:
        !source || source.length === 0 || avatar._endsWithDefault(source)
    // True whenever the initials chip should be the visible content. The
    // Image.Error term is kept SEPARATE from _emptyOrSentinel (and we do NOT
    // blank source on error) so a broken-but-set url actually reaches
    // Image.Error and the binding doesn't oscillate.
    readonly property bool showInitials: _emptyOrSentinel || circle.imageStatus === Image.Error
    readonly property bool showingImage: circle.showingImage
    readonly property int imageStatus: circle.imageStatus

    implicitWidth: size
    implicitHeight: size

    function _endsWithDefault(s) {
        var path = ("" + s).split("?")[0].split("#")[0];  // strip query/hash
        var needle = "default.jpg";
        return path.length >= needle.length
            && path.slice(-needle.length).toLowerCase() === needle;
    }

    LCircleImage {
        id: circle
        anchors.fill: parent
        size: avatar.size
        // Empty/sentinel -> blank (no load, show placeholder). A real url -> load;
        // if it errors, LCircleImage keeps the canvas hidden and shows the
        // placeholder, and imageStatus stays Error (source unchanged).
        source: avatar._emptyOrSentinel ? "" : avatar.source

        // Placeholder slot: the initials chip. Always mounted; LCircleImage
        // shows it whenever the canvas isn't painting a photo.
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: avatar.fallbackBackground
            Text {
                objectName: "avatarInitials"
                anchors.centerIn: parent
                text: avatar.initials
                color: avatar.fallbackForeground
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.control
                font.weight: Font.ExtraBold
            }
        }
    }
}
