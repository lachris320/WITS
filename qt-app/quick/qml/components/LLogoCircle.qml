import QtQuick
import LOAMS

// Circular school-logo badge: the real logo cropped to a circle when one is
// configured and loaded, otherwise a "LOGO" placeholder circle. Shared by the
// admin sidebar (LSidebarBrand, 52px / 2px ring) and the kiosk brand panel
// (BrandPanel, 96px / 3px ring) so both surfaces render the school's logo
// identically instead of each carrying its own copy of the circle.
//
// Presentational only: takes primitive props, not a vm — mirrors
// LSidebarBrand's / LPageHeader's pattern (title/subtitle/clockText, not a vm)
// so this stays independently testable with literal fixture values and
// reusable without any particular ViewModel in scope.
//
// Circular photo crop: QtQuick has no built-in rounded-image clip (Rectangle
// clip:true clips to the bounding box, not the rounded shape), and this repo
// links no shader-effects module (no QtQuick.Effects / Qt5Compat.GraphicalEffects
// dependency exists yet). Rather than add one for a single decorative crop,
// this draws the loaded Image onto a Canvas with a circular clip path — both
// types are part of the core QtQuick module already in use everywhere else,
// so this needs no new CMake link and no GPU/shader support, which keeps it
// safe under the OFFSCREEN QuickTest platform.
//
// How the pixels get onto the Canvas: via Canvas.loadImage(url) /
// Context2D.drawImage(url, ...), NOT by handing the Image *element* to
// drawImage. Passing an Image item is the unreliable path — it silently
// painted nothing at all in the admin sidebar (gold ring drawn, circle
// interior empty) even with the Image at status Ready. The url-based image
// cache is the documented flow and is what actually renders.
Item {
    id: logoFrame

    property url logoUrl: ""
    property bool hasLogo: false
    // The single source of truth for "which url, if any, should be drawn".
    // hasLogo:false vetoes a set logoUrl (the VM reports hasLogo:false for a
    // configured-but-missing file), so both the Image and the Canvas image
    // cache must key off this rather than logoUrl directly.
    readonly property url effectiveUrl: hasLogo ? logoUrl : ""
    // Diameter in px. Drives the implicit size so plain-anchored consumers get
    // the right box; Layout consumers may still pin Layout.preferredWidth/Height.
    property int size: 52
    // Gold ring thickness: admin sidebar uses 2, the larger kiosk circle uses 3.
    property int ringWidth: 2

    implicitWidth: size
    implicitHeight: size

    // The Canvas image cache is imperative, not bindable, so a url change has
    // to be pushed into it.
    onEffectiveUrlChanged: logoCanvas.reloadSource()

    // Loads off-scene. Nothing ever draws this element — the Canvas draws from
    // the url, not from this item — but it is still the component's source of
    // two things the Canvas image cache cannot report: load *status* (which
    // gates the placeholder swap) and the image's *natural* pixel dimensions
    // (sourceSize), which the cover-crop maths below needs.
    Image {
        id: logoImage
        objectName: "logoImage"
        anchors.fill: parent
        source: logoFrame.effectiveUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        visible: false
        onStatusChanged: logoCanvas.requestPaint()
    }

    Canvas {
        id: logoCanvas
        objectName: "logoCanvas"
        anchors.fill: parent
        renderTarget: Canvas.Image

        // The url currently held in this Canvas's own image cache. Tracked
        // separately from logoFrame.effectiveUrl so a url change can unload
        // the previous entry instead of leaving it cached forever.
        property url loadedUrl: ""
        // True once imageLoaded has fired for loadedUrl. Gates `visible` so the
        // circle is never shown as an empty disc between "Image reached Ready"
        // and "the Canvas image cache actually has the pixels".
        property bool canvasImageReady: false

        function reloadSource() {
            if (loadedUrl != "")
                unloadImage(loadedUrl);
            canvasImageReady = false;
            loadedUrl = logoFrame.effectiveUrl;
            if (loadedUrl != "") {
                loadImage(loadedUrl);
                // A url already sitting in Qt's *global* pixmap cache (another
                // LLogoCircle on screen loaded the same logo, or this one
                // reloaded it) resolves synchronously inside loadImage() and
                // never emits imageLoaded — so imageLoaded alone would leave
                // the second badge stuck on its placeholder forever. Settle the
                // ready flag up-front for that path.
                if (isImageLoaded(loadedUrl))
                    canvasImageReady = true;
            }
            requestPaint();
        }

        Component.onCompleted: reloadSource()
        // The async path: a url not yet in the cache reports back here.
        onImageLoaded: {
            canvasImageReady = true;
            requestPaint();
        }

        visible: logoFrame.hasLogo
                 && logoImage.status === Image.Ready
                 && canvasImageReady
        onVisibleChanged: if (visible) requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            if (loadedUrl == "" || !isImageLoaded(loadedUrl))
                return;

            // Centred "cover" crop: scale the source so it fills the circle
            // with no distortion and no empty margin. The destination is always
            // square, so take the largest centred *square* out of the source and
            // let drawImage scale that 1:1 into the box. A plain
            // drawImage(src, 0, 0, w, h) stretches the whole source into the
            // square instead, which visibly squashes any landscape logo — the
            // Image element's PreserveAspectCrop does NOT apply here, because
            // the Canvas reads source pixels, not the Image's rendered output.
            var natW = logoImage.sourceSize.width;
            var natH = logoImage.sourceSize.height;
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

    // Fallback placeholder: no logo configured, path rotted (the VM already
    // filters that out via hasLogo), or the file hasn't finished/failed
    // loading yet.
    Rectangle {
        id: placeholder
        objectName: "logoPlaceholder"
        anchors.fill: parent
        radius: width / 2
        visible: !logoCanvas.visible
        color: Theme.card
        border.width: logoFrame.ringWidth
        border.color: Theme.accent.base
        Text {
            anchors.centerIn: parent
            text: qsTr("LOGO")
            color: Theme.mutedText
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.eyebrow
        }
    }

    // Gold ring over the real photo (reference: a gold border on the <img>
    // itself) — the placeholder above already carries its own.
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: logoFrame.ringWidth
        border.color: Theme.accent.base
        visible: logoCanvas.visible
    }
}
