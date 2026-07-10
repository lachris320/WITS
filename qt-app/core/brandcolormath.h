#ifndef BRANDCOLORMATH_H
#define BRANDCOLORMATH_H

#include <QColor>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

// Header-only, pure color math shared by the brand-theme engine and the
// theme tests. Header-only (like theme.h / apiconfig.h) so it links into
// the app and every test target with no extra .cpp.
//
// shade() and mix() reproduce the attached design system's own JS
// derivation functions, so palettes derived here match the HTML mockups:
//   brand-deep = shade(brand, -0.28)   brand-soft = mix(brand, white, 0.90)
namespace BrandColorMath {

// WCAG 2.1 relative luminance of an sRGB color, in [0, 1].
inline double relativeLuminance(const QColor &c)
{
    const auto lin = [](double s) {
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

// WCAG 2.1 contrast ratio between two colors, in [1, 21].
inline double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    const double lighter = std::max(la, lb);
    const double darker  = std::min(la, lb);
    return (lighter + 0.05) / (darker + 0.05);
}

// Per-channel multiply by (1 + amt), clamped; amt in [-1, 1]. Negative
// darkens, positive lightens. Mirrors the design system's shade().
inline QColor shade(const QColor &c, double amt)
{
    const auto f = [amt](int v) {
        return qBound(0, qRound(v * (1.0 + amt)), 255);
    };
    return QColor(f(c.red()), f(c.green()), f(c.blue()), c.alpha());
}

// Linear per-channel interpolation from c toward `toward` by t in [0, 1].
// Mirrors the design system's mix().
inline QColor mix(const QColor &c, const QColor &toward, double t)
{
    const auto ch = [t](int x, int y) { return qRound(x + (y - x) * t); };
    return QColor(ch(c.red(),   toward.red()),
                  ch(c.green(), toward.green()),
                  ch(c.blue(),  toward.blue()),
                  c.alpha());
}

} // namespace BrandColorMath

#endif // BRANDCOLORMATH_H
