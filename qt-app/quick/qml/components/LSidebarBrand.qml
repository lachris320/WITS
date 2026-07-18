import QtQuick
import QtQuick.Layouts
import LOAMS

// Sidebar brand block (reference: Admin Dashboard.dc.html ~L33-40) — a
// circular school logo beside a two-line title block ("Library Admin" over
// the school name). Presentational only: takes primitive props, not a vm —
// mirrors LPageHeader's pattern (title/subtitle/clockText, not a vm) so this
// stays independently testable with literal fixture values and reusable
// without a SchoolInfoViewModel in scope.
//
// Circular photo crop: QtQuick has no built-in rounded-image clip (Rectangle
// clip:true clips to the bounding box, not the rounded shape), and this repo
// links no shader-effects module (no QtQuick.Effects / Qt5Compat.GraphicalEffects
// dependency exists yet). Rather than add one for a single decorative crop,
// this draws the loaded Image onto a Canvas with a circular clip path — both
// types are part of the core QtQuick module already in use everywhere else,
// so this needs no new CMake link and no GPU/shader support, which keeps it
// safe under the OFFSCREEN QuickTest platform.
RowLayout {
    id: brand
    property string schoolName: ""
    property url logoUrl: ""
    property bool hasLogo: false

    spacing: Theme.spacing.md

    readonly property int logoSize: 52

    Item {
        id: logoFrame
        Layout.preferredWidth: brand.logoSize
        Layout.preferredHeight: brand.logoSize

        // Loads off-scene; Image loading is independent of `visible`, so
        // this still decodes even though nothing ever draws it directly.
        Image {
            id: logoImage
            objectName: "logoImage"
            anchors.fill: parent
            source: brand.hasLogo ? brand.logoUrl : ""
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
            visible: brand.hasLogo && logoImage.status === Image.Ready
            onVisibleChanged: if (visible) requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                if (logoImage.status !== Image.Ready)
                    return;
                ctx.save();
                ctx.beginPath();
                ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2, true);
                ctx.closePath();
                ctx.clip();
                ctx.drawImage(logoImage, 0, 0, width, height);
                ctx.restore();
            }
        }

        // Fallback placeholder: no logo configured, path rotted (VM already
        // filters that out via hasLogo), or the file hasn't finished/failed
        // loading yet. Same visual language as the kiosk's BrandPanel circle.
        Rectangle {
            id: placeholder
            objectName: "logoPlaceholder"
            anchors.fill: parent
            radius: width / 2
            visible: !logoCanvas.visible
            color: Theme.card
            border.width: 2
            border.color: Theme.secondary
            Text {
                anchors.centerIn: parent
                text: qsTr("LOGO")
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.eyebrow
            }
        }

        // Gold ring over the real photo (reference: 2px gold border on the
        // <img> itself) — the placeholder above already carries its own.
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.secondary
            visible: logoCanvas.visible
        }
    }

    ColumnLayout {
        spacing: 2
        Text {
            objectName: "brandTitleText"
            text: qsTr("Library Admin")
            color: Theme.brand.onPrimary
            font.family: Theme.typography.serif
            font.weight: Font.Bold
            // No Theme.typography token sits at the reference's literal 19px
            // for this one label (scale has cardTitle:17 / heroName:27); 17
            // is the nearest existing token and this is the only consumer,
            // so cardTitle is reused rather than adding a token for one spot.
            font.pixelSize: Theme.typography.cardTitle
        }
        Text {
            objectName: "brandSchoolNameText"
            text: brand.schoolName
            visible: brand.schoolName.length > 0
            color: Theme.onBrandMuted
            elide: Text.ElideRight
            Layout.maximumWidth: 150
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.eyebrow
        }
    }
}
