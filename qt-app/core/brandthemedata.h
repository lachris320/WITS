#ifndef BRANDTHEMEDATA_H
#define BRANDTHEMEDATA_H

#include <QColor>
#include <QDateTime>
#include <QString>

// Value types for the brand-theme engine. Plain structs (like settingsdata.h
// / visitordata.h) so they link into the app and test targets with no .cpp.

// Manual mode is a code hook only in v1: regeneration is skipped when a
// config says Manual; there is NO UI that sets it.
enum class ThemeMode { Auto, Manual };

// Role palette the branding engine produces. Brand roles derive from the
// logo in Auto mode; neutral roles always keep the WitsTheme fallback
// values in v1. Admin and Kiosk stay distinct roles by design — the logo's
// primary maps onto Admin, its secondary/accent onto Kiosk.
struct BrandPalette
{
    // Brand roles (logo-derived in Auto mode)
    QColor adminPrimary;
    QColor adminPrimaryHover;
    QColor adminOnPrimary;    // text/icon color on adminPrimary fills
    QColor adminPrimarySoft;  // tinted background (design: brand-soft)
    QColor kioskPrimary;
    QColor kioskPrimaryHover;
    QColor kioskOnPrimary;
    QColor kioskPrimarySoft;
    QColor secondary;

    // Neutral roles (fallback values in v1)
    QColor sidebarBase;
    QColor card;
    QColor appBackground;
    QColor border;
    QColor text;
    QColor mutedText;
    QColor success;
    QColor error;

    bool operator==(const BrandPalette &o) const
    {
        return adminPrimary == o.adminPrimary
            && adminPrimaryHover == o.adminPrimaryHover
            && adminOnPrimary == o.adminOnPrimary
            && adminPrimarySoft == o.adminPrimarySoft
            && kioskPrimary == o.kioskPrimary
            && kioskPrimaryHover == o.kioskPrimaryHover
            && kioskOnPrimary == o.kioskOnPrimary
            && kioskPrimarySoft == o.kioskPrimarySoft
            && secondary == o.secondary
            && sidebarBase == o.sidebarBase
            && card == o.card
            && appBackground == o.appBackground
            && border == o.border
            && text == o.text
            && mutedText == o.mutedText
            && success == o.success
            && error == o.error;
    }
    bool operator!=(const BrandPalette &o) const { return !(*this == o); }
};

// One persisted branding state — what the cache holds locally and the
// branding_config table holds remotely.
struct BrandingConfig
{
    ThemeMode    mode = ThemeMode::Auto;
    BrandPalette palette;   // callers default this to BrandTheme::fallbackPalette()
    QString      logoHash;  // SHA-256 hex of the source logo file bytes
    QDateTime    updatedAt; // invalid == never branded
};

#endif // BRANDTHEMEDATA_H
