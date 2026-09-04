import QtQuick
import LOAMS

// Circular image with a "cover" crop, drawn on a Canvas (core QtQuick only — this
// repo links no shader-effects module, and a Rectangle clip clips to the bounding
// box, not the circle). Extracted from LLogoCircle so LAvatar and LLogoCircle
// share one tested crop. Presentational: primitive props, no vm.
//
// The Canvas image cache is imperative; the placeholder slot shows whenever no
// image is drawn (empty source / not yet Ready / failed).
Item {
    id: frame

    property url source: ""
    property int size: 52
    property int ringWidth: 0
    property color ringColor: Theme.accent.base
    // Shown when the canvas isn't drawing an image. Consumers fill this slot.
    default property alias placeholderContent: placeholderHost.data
    // Exposed so consumers can gate on load state (LAvatar reads imageStatus for
    // the Image.Error fallback; showingImage says the photo is actually painted).
    readonly property int imageStatus: circleImage.status
    readonly property bool showingImage: circleCanvas.visible

    implicitWidth: size
    implicitHeight: size

    onSourceChanged: circleCanvas.reloadSource()

    // Loads off-scene. Nothing draws this element — the Canvas draws from the
    // url — but it reports load *status* (gates the placeholder swap) and the
    // *natural* pixel dimensions (sourceSize) the cover-crop maths needs.
    Image {
        id: circleImage
        objectName: "logoImage"        // PRESERVED: LLogoCircle regression tests
        anchors.fill: parent
        source: frame.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        visible: false
        onStatusChanged: circleCanvas.requestPaint()
    }

    Canvas {
        id: circleCanvas
        objectName: "logoCanvas"       // PRESERVED
        anchors.fill: parent
        renderTarget: Canvas.Image

        property url loadedUrl: ""
        property bool canvasImageReady: false

        function reloadSource() {
            if (loadedUrl != "")
                unloadImage(loadedUrl);
            canvasImageReady = false;
            loadedUrl = frame.source;
            if (loadedUrl != "") {
                loadImage(loadedUrl);
                if (isImageLoaded(loadedUrl))
                    canvasImageReady = true;
            }
            requestPaint();
        }

        Component.onCompleted: reloadSource()
        onImageLoaded: { canvasImageReady = true; requestPaint(); }

        visible: frame.source != ""
                 && circleImage.status === Image.Ready
                 && canvasImageReady
        onVisibleChanged: if (visible) requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            if (loadedUrl == "" || !isImageLoaded(loadedUrl))
                return;
            var natW = circleImage.sourceSize.width;
            var natH = circleImage.sourceSize.height;
            if (natW <= 0 || natH <= 0)
                return;
            var side = Math.min(natW, natH);
            var sx = (natW - side) / 2;
            var sy = (natH - side) / 2;
            ctx.save();
            ctx.beginPath();
            ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2, true);
            ctx.closePath();
            ctx.clip();
            ctx.drawImage(loadedUrl, sx, sy, side, side, 0, 0, width, height);
            ctx.restore();
        }
    }

    // Placeholder slot host: visible whenever the canvas isn't. Consumers put
    // their fallback (LOGO circle, initials chip) here. The host's own
    // visibility is a convenience default; a consumer that needs its
    // placeholder content itself reported as visible/hidden (as LLogoCircle's
    // regression tests do via findChild) should bind that content's own
    // `visible` to `!<thisComponent>.showingImage` instead of relying on the
    // host — a QQuickItem's own `visible` property doesn't flip just because
    // an ancestor's does.
    Item {
        id: placeholderHost
        anchors.fill: parent
        visible: !circleCanvas.visible
    }

    // Optional ring over the drawn photo (LLogoCircle's gold ring).
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: frame.ringWidth
        border.color: frame.ringColor
        visible: circleCanvas.visible && frame.ringWidth > 0
    }
}
