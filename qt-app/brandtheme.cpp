#include "brandtheme.h"

#include <QJsonValue>
#include <QSettings>
#include "brandcolormath.h"
#include "theme.h"

using BrandColorMath::mix;

namespace BrandTheme {

BrandPalette fallbackPalette()
{
    const QColor white(Qt::white);
    BrandPalette p;
    // Brand roles — verbatim theme.h constants (the PRD's fallback rule).
    p.adminPrimary      = QColor(WitsTheme::Color::AdminPrimary);
    p.adminPrimaryHover = QColor(WitsTheme::Color::AdminPrimaryHover);
    p.adminOnPrimary    = white;
    p.adminPrimarySoft  = mix(p.adminPrimary, white, 0.90);
    p.kioskPrimary      = QColor(WitsTheme::Color::KioskPrimary);
    p.kioskPrimaryHover = QColor(WitsTheme::Color::KioskPrimaryHover);
    p.kioskOnPrimary    = white; // legacy-faithful; fallback is exempt from MinContrast
    p.kioskPrimarySoft  = mix(p.kioskPrimary, white, 0.90);
    p.secondary         = QColor(WitsTheme::Color::Secondary);
    // Neutral roles
    p.sidebarBase   = QColor(WitsTheme::Color::SidebarBase);
    p.card          = QColor(WitsTheme::Color::Card);
    p.appBackground = QColor(WitsTheme::Color::AppBackground);
    p.border        = QColor(WitsTheme::Color::Border);
    p.text          = QColor(WitsTheme::Color::Text);
    p.mutedText     = QColor(WitsTheme::Color::MutedText);
    p.success       = QColor(WitsTheme::Color::Success);
    p.error         = QColor(WitsTheme::Color::Error);
    return p;
}

namespace {

// One place defines the JSON field order/names for both directions.
struct FieldMap { const char *key; QColor BrandPalette::*member; };
const FieldMap kPaletteFields[] = {
    {"admin_primary",       &BrandPalette::adminPrimary},
    {"admin_primary_hover", &BrandPalette::adminPrimaryHover},
    {"admin_on_primary",    &BrandPalette::adminOnPrimary},
    {"admin_primary_soft",  &BrandPalette::adminPrimarySoft},
    {"kiosk_primary",       &BrandPalette::kioskPrimary},
    {"kiosk_primary_hover", &BrandPalette::kioskPrimaryHover},
    {"kiosk_on_primary",    &BrandPalette::kioskOnPrimary},
    {"kiosk_primary_soft",  &BrandPalette::kioskPrimarySoft},
    {"secondary",           &BrandPalette::secondary},
    {"sidebar_base",        &BrandPalette::sidebarBase},
    {"card",                &BrandPalette::card},
    {"app_background",      &BrandPalette::appBackground},
    {"border",              &BrandPalette::border},
    {"text",                &BrandPalette::text},
    {"muted_text",          &BrandPalette::mutedText},
    {"success",             &BrandPalette::success},
    {"error",               &BrandPalette::error},
};

const QString kDateTimeFormat = QStringLiteral("yyyy-MM-dd HH:mm:ss");

} // namespace

QJsonObject paletteToJson(const BrandPalette &p)
{
    QJsonObject o;
    for (const FieldMap &f : kPaletteFields)
        o.insert(QLatin1String(f.key), (p.*(f.member)).name());
    return o;
}

BrandPalette paletteFromJson(const QJsonObject &o)
{
    BrandPalette p = fallbackPalette();
    for (const FieldMap &f : kPaletteFields) {
        const QColor c(o.value(QLatin1String(f.key)).toString());
        if (c.isValid())
            p.*(f.member) = c;
    }
    return p;
}

QJsonObject configToJson(const BrandingConfig &c)
{
    QJsonObject o;
    o.insert(QStringLiteral("theme_mode"),
             c.mode == ThemeMode::Manual ? QStringLiteral("manual")
                                         : QStringLiteral("auto"));
    o.insert(QStringLiteral("palette"), paletteToJson(c.palette));
    o.insert(QStringLiteral("logo_hash"), c.logoHash);
    o.insert(QStringLiteral("updated_at"),
             c.updatedAt.isValid() ? c.updatedAt.toString(kDateTimeFormat)
                                   : QString());
    return o;
}

BrandingConfig configFromJson(const QJsonObject &o)
{
    BrandingConfig c;
    c.mode = o.value(QStringLiteral("theme_mode")).toString().trimmed().toLower()
                     == QLatin1String("manual")
                 ? ThemeMode::Manual
                 : ThemeMode::Auto;
    c.palette = paletteFromJson(o.value(QStringLiteral("palette")).toObject());
    c.logoHash = o.value(QStringLiteral("logo_hash")).toString();
    c.updatedAt = QDateTime::fromString(
        o.value(QStringLiteral("updated_at")).toString(), kDateTimeFormat);
    return c;
}

// --- Placeholders replaced by Task 2 (logo pipeline) ---
bool validateLogoFile(const QString &, QString *errorMsg)
{
    if (errorMsg) *errorMsg = QStringLiteral("not implemented");
    return false;
}
BrandPalette extractPalette(const QString &, QString *errorMsg)
{
    if (errorMsg) *errorMsg = QStringLiteral("not implemented");
    return fallbackPalette();
}

// --- Placeholders replaced by Task 3 (cache + hooks) ---
void saveCachedConfig(QSettings &, const BrandingConfig &) {}
BrandingConfig loadCachedConfig(QSettings &)
{
    BrandingConfig c;
    c.palette = fallbackPalette();
    return c;
}
bool isNewer(const BrandingConfig &, const BrandingConfig &) { return false; }
bool regenerateFromLogo(BrandingConfig &, const QString &, QString *errorMsg)
{
    if (errorMsg) *errorMsg = QStringLiteral("not implemented");
    return false;
}

const BrandPalette &current()
{
    static BrandPalette s_current = fallbackPalette();
    return s_current;
}
void setCurrent(const BrandPalette &) {}

} // namespace BrandTheme
