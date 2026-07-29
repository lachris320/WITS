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
// The circular logo badge itself lives in LLogoCircle (shared with the kiosk's
// BrandPanel) — see that file for why the circular crop is drawn on a Canvas.
RowLayout {
    id: brand
    property string schoolName: ""
    property url logoUrl: ""
    property bool hasLogo: false

    spacing: Theme.spacing.md

    readonly property int logoSize: 52

    LLogoCircle {
        logoUrl: brand.logoUrl
        hasLogo: brand.hasLogo
        size: brand.logoSize
        ringWidth: 2                 // reference: 2px gold border on the <img>
        Layout.preferredWidth: brand.logoSize
        Layout.preferredHeight: brand.logoSize
    }

    ColumnLayout {
        spacing: 2
        Text {
            objectName: "brandTitleText"
            text: qsTr("Library Admin")
            color: Theme.brand.on
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
            color: Theme.brand.onMuted
            elide: Text.ElideRight
            Layout.maximumWidth: 150
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.eyebrow
        }
    }
}
