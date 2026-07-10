#ifndef BRANDTHEME_H
#define BRANDTHEME_H

#include <QJsonObject>
#include <QString>
#include "brandthemedata.h"

class QSettings;

// Logo-derived dynamic branding engine — an additive layer on top of
// theme.h. WitsTheme::Color::* is the fallback palette; extraction maps
// the logo's dominant color onto the Admin role and its secondary color
// onto the Kiosk role (distinct roles, never collapsed).
//
// Deterministic by construction: extraction is pure integer math over the
// decoded image — the same logo always yields the same palette.
namespace BrandTheme {

// Minimum contrast ratio between a primary and its on-color (WCAG 2.1
// non-text UI component minimum). Applies to generated palettes; the
// legacy-faithful fallback palette is exempt (see fallbackPalette()).
inline constexpr double MinContrast = 3.0;

// The legacy palette, verbatim from WitsTheme::Color::* (theme.h).
// kioskOnPrimary is white for fidelity with the existing kiosk styling
// even though the legacy green fails MinContrast — fallback is exempt.
BrandPalette fallbackPalette();

// --- Logo pipeline (Task 2) ---
// Validate a logo file: existence, extension whitelist (svg png jpg jpeg
// bmp webp ico), plugin availability (webp), and a full decode so corrupt
// files are caught. On failure returns false and sets *errorMsg to a
// specific, user-readable reason. Never crashes on bad input.
bool validateLogoFile(const QString &logoPath, QString *errorMsg);

// Extract a deterministic, contrast-enforced palette from a logo. On any
// validation/decode failure returns fallbackPalette() and sets *errorMsg.
// A logo with no usable chromatic pixels (e.g. pure greyscale) returns
// fallbackPalette() with *errorMsg cleared.
BrandPalette extractPalette(const QString &logoPath, QString *errorMsg);

// --- Serialization (Task 1) ---
QJsonObject paletteToJson(const BrandPalette &p);
BrandPalette paletteFromJson(const QJsonObject &o); // missing/invalid fields -> fallback values
QJsonObject configToJson(const BrandingConfig &c);
BrandingConfig configFromJson(const QJsonObject &o);

// --- Local cache (Task 3) --- QSettings is injected so tests use a
// temp-file INI store; the app passes QSettings("MyCompany", "MyApp").
void saveCachedConfig(QSettings &store, const BrandingConfig &config);
BrandingConfig loadCachedConfig(QSettings &store); // never-branded -> mode Auto + fallback palette

// True when `remote` should replace `local` (remote has a valid, strictly
// newer updatedAt; a valid remote always beats a never-branded local).
bool isNewer(const BrandingConfig &remote, const BrandingConfig &local);

// Regenerate config's palette from a logo. THE MANUAL-MODE HOOK: when
// config.mode == ThemeMode::Manual this skips auto-regeneration entirely —
// returns false with *errorMsg cleared, config untouched. In Auto mode it
// re-extracts, updates palette/logoHash/updatedAt, and returns true;
// validation failures return false with *errorMsg set.
bool regenerateFromLogo(BrandingConfig &config, const QString &logoPath,
                        QString *errorMsg);

// Process-wide current palette (defaults to fallbackPalette()). P2 UI
// consumers read current(); Phase 1 only maintains it.
const BrandPalette &current();
void setCurrent(const BrandPalette &palette);

} // namespace BrandTheme

#endif // BRANDTHEME_H
