import QtQuick
import LOAMS

// Kiosk brand fill gradient (BrandPanel + hero card). Assign directly to a
// `gradient:` property: `gradient: LKioskGradient {}`.
Gradient {
    GradientStop { position: 0.0; color: Theme.brand.kiosk }
    GradientStop { position: 1.0; color: Theme.brand.kioskHover }
}
