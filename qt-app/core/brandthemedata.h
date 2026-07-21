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
    // Brand roles (logo-derived in Auto mode). Renamed to role names in 4d;
    // both surfaces draw from both families by element role.
    QColor brandBase;      // was adminPrimary
    QColor brandDeep;      // was adminPrimaryHover
    QColor brandOn;        // was adminOnPrimary
    QColor brandSoft;      // was adminPrimarySoft
    QColor brandOnMuted;   // NEW — muted text on the brand sidebar
    QColor brandText;      // NEW — brand colour used AS text on a light card
    QColor accentBase;     // was kioskPrimary
    QColor accentDeep;     // was kioskPrimaryHover
    QColor accentOn;       // was kioskOnPrimary
    QColor accentSoft;     // was kioskPrimarySoft
    QColor accentText;     // NEW — accent colour used AS text on a light card

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
        return brandBase == o.brandBase
            && brandDeep == o.brandDeep
            && brandOn == o.brandOn
            && brandSoft == o.brandSoft
            && brandOnMuted == o.brandOnMuted
            && brandText == o.brandText
            && accentBase == o.accentBase
            && accentDeep == o.accentDeep
            && accentOn == o.accentOn
            && accentSoft == o.accentSoft
            && accentText == o.accentText
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
    bool         didFallBack = false; // true when the last regen produced the fallback palette
};

#endif // BRANDTHEMEDATA_H
