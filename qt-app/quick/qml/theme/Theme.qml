pragma Singleton
import QtQuick
import LOAMS

QtObject {
    id: root

    // Brand roles come from the C++ engine via ThemeViewModel; live re-theme
    // works because these are property bindings that re-evaluate on changed().
    readonly property ThemeViewModel _vm: ThemeViewModel {}

    readonly property QtObject brand: QtObject {
        readonly property color admin:        root._vm.adminPrimary
        readonly property color adminHover:   root._vm.adminPrimaryHover
        readonly property color adminSoft:    root._vm.adminPrimarySoft
        readonly property color kiosk:        root._vm.kioskPrimary
        readonly property color kioskHover:   root._vm.kioskPrimaryHover
        readonly property color kioskSoft:    root._vm.kioskPrimarySoft
        readonly property color onPrimary:    root._vm.adminOnPrimary
        readonly property color onKiosk:      root._vm.kioskOnPrimary
    }

    readonly property color secondary:     root._vm.secondary
    readonly property color card:          root._vm.card
    readonly property color appBackground: root._vm.appBackground
    readonly property color border:        root._vm.border
    readonly property color text:          root._vm.text
    readonly property color mutedText:     root._vm.mutedText
    readonly property color success:       root._vm.success
    readonly property color error:         root._vm.error
    readonly property color sidebarBase:   root._vm.sidebarBase

    // Extra design tokens (no BrandPalette field — literals, §12.1).
    readonly property color secondarySoft:    "#FDF3E0"
    readonly property color mutedTextCaption: "#B0A08A"
    readonly property color tableHeaderBg:    "#F7F1E6"
    readonly property color rowHairline:      "#F3ECDD"
    readonly property color errorSoft:        "#FDF4F3"
    readonly property color errorBorder:      "#F3D9D6"
    readonly property color scrim:            Qt.rgba(15/255, 23/255, 42/255, 0.45)
    readonly property color onBrandMuted:     "#EFC9A8"

    // Structural scales (§12.2/12.3).
    readonly property QtObject spacing: QtObject {
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int base: 14
        readonly property int lg: 16
        readonly property int xl: 18
        readonly property int xl2: 22
        readonly property int xxl: 24
        readonly property int xxxl: 28
    }
    readonly property QtObject radius: QtObject {
        readonly property int xs: 6
        readonly property int sm: 8
        readonly property int sm2: 10
        readonly property int md: 12
        readonly property int card: 16
        readonly property int hero: 18
        readonly property int pill: 999
    }
    // Typography (§12.6): family + weight + pixel size per role.
    readonly property QtObject typography: QtObject {
        readonly property string serif: "Source Serif 4"
        readonly property string sans:  "Public Sans"
        readonly property int pageTitle: 30
        readonly property int heroName: 27
        readonly property int cardTitle: 17
        readonly property int statValue: 34
        readonly property int body: 14
        readonly property int control: 13
        readonly property int eyebrow: 11
    }
    // Elevation (§12.7): shadow specs as data (a component maps these to a
    // DropShadow / layer effect). Kept as strings so no widget dep leaks in.
    readonly property QtObject elevation: QtObject {
        readonly property string resting: ""
        readonly property string hover:   "0 12px 26px rgba(94,14,11,0.10)"
        readonly property string heroFill: "0 12px 30px rgba(94,14,11,0.25)"
        readonly property string ctaGold: "0 5px 14px rgba(232,177,14,0.30)"
        readonly property string modal:   "0 24px 48px rgba(0,0,0,0.30)"
    }
    // Motion (§15): one shared easing for one-shot transitions + a reduce switch.
    readonly property QtObject motion: QtObject {
        readonly property bool enabled: true
        readonly property int pageIn: 400
        readonly property int rowIn: 400
        readonly property int cardIn: 450
        readonly property int barGrow: 600
        readonly property int deptBarFill: 800
        readonly property int hoverLift: 180
        readonly property int toastIn: 200
        readonly property int toastOut: 150
        readonly property int toastHold: 2500
        // 6-value BezierSpline form (the 4-value cubic-bezier control points
        // from the reference mock, plus the required (1.0, 1.0) endpoint) so
        // call sites can use it directly as
        // `easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.motion.easing`.
        readonly property var easing: [0.2, 0.8, 0.3, 1.0, 1.0, 1.0]
        // Per-item entrance delay for staggered lists/charts. 25ms here is a
        // DELIBERATE deviation from the reference mock's 35-40ms: this is an
        // all-shift tool used hundreds of times a day, so snappier wins and
        // intra-app consistency across Search/Dashboard/Visit Logs beats
        // fidelity to a mock nobody holds next to the running app.
        readonly property int rowStagger: 25
        readonly property int barStagger: 45
        // Cap on how many items get an extra stagger delay; beyond this,
        // items appear with no additional wait so a large list/chart doesn't
        // take seconds to finish its entrance.
        readonly property int staggerCap: 10

        // Shared clamp-and-multiply stagger arithmetic (Phase 3 gatefix DRY):
        // was hand-rolled identically at LBarChart's bar entrance, LTable's
        // populate/add Transitions, and (after the SearchScreen migration)
        // the results-row entrance too. `i` is clamped to [0, staggerCap]
        // (a delegate's index transiently goes to -1 during a model-reset
        // teardown; without the max(0, ...) that would feed a negative
        // duration to a PauseAnimation) before multiplying by the caller's
        // own per-item step (rowStagger/barStagger).
        function staggerDelay(i, step) {
            return Math.max(0, Math.min(i, staggerCap)) * step;
        }
    }

    // Mode (§13.5) — Phase 1 defaults to light; full light/dark derivation is
    // a Phase-5 item (spec §10 Risk 3).
    readonly property string mode: "Light"
    readonly property bool isDark: false
}
