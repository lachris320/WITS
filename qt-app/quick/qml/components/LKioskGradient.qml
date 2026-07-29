import QtQuick
import LOAMS

// Kiosk brand fill gradient (BrandPanel + hero card). Assign directly to a
// `gradient:` property: `gradient: LKioskGradient {}`.
// Phase 4d role map: kiosk STRUCTURE = brand.* — the gradient flips gold->maroon
// (brand.base->brand.deep) to match the mockup kiosk sidebar + signed-in hero,
// drawn as --brand->--brand-deep (Library Kiosk v2.dc.html:30 sidebar, :76 hero).
// This is the root of the kiosk cascade: every onKiosk text inside these panels
// moves to brand.on in the same commit, or dark-maroon text goes invisible.
Gradient {
    GradientStop { position: 0.0; color: Theme.brand.base }
    GradientStop { position: 1.0; color: Theme.brand.deep }
}
