#include "brandtheme.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonValue>
#include <QPainter>
#include <QSet>
#include <QSettings>
#include <QSvgRenderer>
#include <QtMath>
#include <algorithm>
#include "brandcolormath.h"
#include "theme.h"

using BrandColorMath::contrastRatio;
using BrandColorMath::mix;
using BrandColorMath::shade;

namespace BrandTheme {

BrandPalette fallbackPalette()
{
    const QColor white(Qt::white);
    BrandPalette p;
    // Brand roles — verbatim theme.h constants (the PRD's fallback rule).
    p.brandBase    = QColor(WitsTheme::Color::AdminPrimary);
    p.brandDeep    = QColor(WitsTheme::Color::AdminPrimaryHover);
    p.brandOn      = white;
    p.brandSoft    = mix(p.brandBase, white, 0.90);
    p.brandOnMuted = QColor("#EFC9A8"); // legacy onBrandMuted literal, now a palette field
    p.brandText    = p.brandBase;       // brand-as-text on light; fallback == base
    p.accentBase   = QColor(WitsTheme::Color::KioskPrimary);
    p.accentDeep   = QColor(WitsTheme::Color::KioskPrimaryHover);
    p.accentOn     = white; // legacy-faithful; fallback is exempt from MinContrast
    p.accentSoft   = mix(p.accentBase, white, 0.90);
    p.accentText   = QColor("#8a6a08"); // legacy accent-as-text; fallback constant
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
    {"brand_base",      &BrandPalette::brandBase},
    {"brand_deep",      &BrandPalette::brandDeep},
    {"brand_on",        &BrandPalette::brandOn},
    {"brand_soft",      &BrandPalette::brandSoft},
    {"brand_on_muted",  &BrandPalette::brandOnMuted},
    {"brand_text",      &BrandPalette::brandText},
    {"accent_base",     &BrandPalette::accentBase},
    {"accent_deep",     &BrandPalette::accentDeep},
    {"accent_on",       &BrandPalette::accentOn},
    {"accent_soft",     &BrandPalette::accentSoft},
    {"accent_text",     &BrandPalette::accentText},
    {"sidebar_base",    &BrandPalette::sidebarBase},
    {"card",            &BrandPalette::card},
    {"app_background",  &BrandPalette::appBackground},
    {"border",          &BrandPalette::border},
    {"text",            &BrandPalette::text},
    {"muted_text",      &BrandPalette::mutedText},
    {"success",         &BrandPalette::success},
    {"error",           &BrandPalette::error},
};
// PERMANENT read-compat: pre-4d configs hold only these old keys. NEVER delete.
// "secondary" is listed BEFORE "kiosk_primary" — both alias accentBase, and
// the alias-apply loop is last-writer-wins; kiosk_primary is the semantically
// correct accent source, so it must be applied (and win) after secondary. A
// pre-4d cached fallback palette held BOTH kiosk_primary (#10B981) and
// secondary (#3B82F6); if secondary were applied last it would silently
// recolor every accent surface blue on migration.
const FieldMap kOldPaletteAliasFields[] = {
    {"secondary",           &BrandPalette::accentBase},
    {"admin_primary",       &BrandPalette::brandBase},
    {"admin_primary_hover", &BrandPalette::brandDeep},
    {"admin_on_primary",    &BrandPalette::brandOn},
    {"admin_primary_soft",  &BrandPalette::brandSoft},
    {"kiosk_primary",       &BrandPalette::accentBase},
    {"kiosk_primary_hover", &BrandPalette::accentDeep},
    {"kiosk_on_primary",    &BrandPalette::accentOn},
    {"kiosk_primary_soft",  &BrandPalette::accentSoft},
};

const QString kDateTimeFormat = QStringLiteral("yyyy-MM-dd HH:mm:ss");

} // namespace

QJsonObject paletteToJson(const BrandPalette &p)
{
    QJsonObject o;
    for (const FieldMap &f : kPaletteFields)
        o.insert(QLatin1String(f.key), (p.*(f.member)).name());
    // Dual-write old keys so an older build reading this config still finds them.
    for (const FieldMap &f : kOldPaletteAliasFields)
        o.insert(QLatin1String(f.key), (p.*(f.member)).name());
    return o;
}

BrandPalette paletteFromJson(const QJsonObject &o)
{
    BrandPalette p = fallbackPalette();
    // Old aliases first...
    for (const FieldMap &f : kOldPaletteAliasFields) {
        const QColor c(o.value(QLatin1String(f.key)).toString());
        if (c.isValid()) p.*(f.member) = c;
    }
    // ...then new keys, which therefore win when both are present.
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

// --- Logo pipeline (Task 2) ---
namespace {

constexpr int kSampleSize = 64;
constexpr int kMinAlpha = 128;
constexpr double kMinSaturation = 0.15;
constexpr double kMinValue = 0.15;
constexpr double kMinHueSeparationDeg = 60.0;
constexpr double kHoverShade = -0.28;
constexpr double kSoftMixToWhite = 0.90;
constexpr double kOnColorDeepShade = -0.60;
// Accent tint is deliberately stronger toward white than kSoftMixToWhite so the
// (typically lighter) accent hue stays legible as a soft fill (§4).
constexpr double kAccentSoftMixToWhite = 0.88;
// Darken the accent seed for use as text; same tunable family as kOnColorDeepShade (§4).
constexpr double kAccentTextShade = -0.40;
// Muted nav label: 25% from brandOn toward brandBase (§4).
constexpr double kBrandOnMutedMix = 0.25;
// WCAG AA text floor — the split-contrast target for every text/label role (§4).
constexpr double kTextContrast = 4.5;
constexpr double kEnforceStep = -0.08;
constexpr int kEnforceMaxIterations = 24;
constexpr float kRaiseValueStep = 0.06f;

// Decode a logo file into a 64x64 ARGB32 image. On any failure returns a
// null QImage and sets *err to a specific, user-readable reason.
QImage renderLogo(const QString &path, QString *err)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (err) *err = QStringLiteral("Logo file not found: %1").arg(path);
        return QImage();
    }

    const QString ext = info.suffix().toLower();
    static const QSet<QString> kAllowedExtensions = {
        QStringLiteral("svg"), QStringLiteral("png"), QStringLiteral("jpg"),
        QStringLiteral("jpeg"), QStringLiteral("bmp"), QStringLiteral("webp"),
        QStringLiteral("ico"),
    };
    if (!kAllowedExtensions.contains(ext)) {
        if (err)
            *err = QStringLiteral("Unsupported logo format \".%1\" — use SVG, PNG, JPG or BMP.")
                       .arg(ext);
        return QImage();
    }

    if (ext == QLatin1String("svg")) {
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) {
            if (err) *err = QStringLiteral("Invalid or corrupt SVG file: %1").arg(path);
            return QImage();
        }
        QImage img(kSampleSize, kSampleSize, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        renderer.render(&painter);
        painter.end();
        if (err) err->clear();
        return img;
    }

    if (ext == QLatin1String("webp")
        && !QImageReader::supportedImageFormats().contains("webp")) {
        if (err)
            *err = QStringLiteral(
                "WEBP logos are not supported on this system — use PNG or SVG instead.");
        return QImage();
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        if (err)
            *err = QStringLiteral("Unable to decode logo (corrupt or unsupported): %1 (%2)")
                       .arg(path, reader.errorString());
        return QImage();
    }

    if (err) err->clear();
    return img.scaled(kSampleSize, kSampleSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_ARGB32);
}

// Bucket key: 4 most-significant bits of each channel packed into 12 bits.
int bucketKey(int r, int g, int b)
{
    return ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
}

// Bucket center color for a given key (reverses bucketKey's quantization).
QColor bucketCenter(int key)
{
    const int r = ((key >> 8) & 0xF) * 16 + 8;
    const int g = ((key >> 4) & 0xF) * 16 + 8;
    const int b = (key & 0xF) * 16 + 8;
    return QColor(r, g, b);
}

// Circular hue distance in degrees, in [0, 180].
double hueDistanceDeg(double h1, double h2)
{
    double d = qAbs(h1 - h2);
    if (d > 180.0) d = 360.0 - d;
    return d;
}

// Highest-count bucket key; ties broken by lowest key (sorted iteration).
int highestCountKey(const QHash<int, int> &histogram)
{
    QList<int> keys = histogram.keys();
    std::sort(keys.begin(), keys.end());
    int bestKey = keys.first();
    int bestCount = histogram.value(bestKey);
    for (int key : keys) {
        const int count = histogram.value(key);
        if (count > bestCount) {
            bestCount = count;
            bestKey = key;
        }
    }
    return bestKey;
}

} // namespace

// Darkens c (via shade) until contrastRatio(c, against) >= target, capped at
// kEnforceMaxIterations. The `against` colour and `target` ratio are parameters,
// so any role can be enforced against any background (§4 split-contrast floors).
QColor enforceContrast(const QColor &c, const QColor &against, double target)
{
    QColor result = c;
    int iterations = 0;
    while (contrastRatio(result, against) < target && iterations < kEnforceMaxIterations) {
        result = shade(result, kEnforceStep);   // darken toward the target
        ++iterations;
    }
    return result;
}

// LIGHTENS c by raising HSV value (preserving hue/saturation) until it meets
// `target` against a DARK `against`. The accent-lighten seam for Task 6.
QColor raiseToContrast(const QColor &c, const QColor &against, double target)
{
    float h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);
    if (h < 0.0f) h = 0.0f;                       // achromatic guard: getHsvF returns hue -1 for greys
    int iterations = 0;
    QColor result = c;
    while (contrastRatio(result, against) < target && iterations < kEnforceMaxIterations) {
        v = qMin(1.0f, v + kRaiseValueStep);      // raise value (lighten), preserve hue/sat
        result = QColor::fromHsvF(h, s, v, a);
        ++iterations;
    }
    return result;
}

bool validateLogoFile(const QString &logoPath, QString *errorMsg)
{
    QString err;
    const QImage img = renderLogo(logoPath, &err);
    if (errorMsg) *errorMsg = err;
    return !img.isNull();
}

// Pure build step: derives a full palette from an already-picked
// primary/secondary seed pair. No I/O, no errorMsg — reads only the two
// seeds plus file-scope helpers/constants, so it's directly unit-testable
// without a logo image.
BrandPalette buildPalette(const QColor &primarySeed, const QColor &secondarySeed)
{
    const QColor white(Qt::white);
    BrandPalette p;

    // Neutrals first — brandText/accentText are enforced against p.card, so the
    // neutral paper colour must be populated before those roles are derived.
    const BrandPalette fb = fallbackPalette();
    p.sidebarBase   = fb.sidebarBase;
    p.card          = fb.card;
    p.appBackground = fb.appBackground;
    p.border        = fb.border;
    p.text          = fb.text;
    p.mutedText     = fb.mutedText;
    p.success       = fb.success;
    p.error         = fb.error;

    // Brand roles (§4).
    p.brandBase    = enforceContrast(primarySeed, white, kTextContrast);
    p.brandDeep    = shade(p.brandBase, kHoverShade);
    p.brandSoft    = mix(p.brandBase, white, kSoftMixToWhite);
    p.brandOn      = white;
    p.brandOnMuted = mix(p.brandOn, p.brandBase, kBrandOnMutedMix);
    if (contrastRatio(p.brandOnMuted, p.brandBase) < kTextContrast)
        p.brandOnMuted = enforceContrast(p.brandOnMuted, p.brandBase, kTextContrast);
    p.brandText    = enforceContrast(p.brandBase, p.card, kTextContrast);

    // Accent roles (§4).
    p.accentBase   = raiseToContrast(secondarySeed, p.brandBase, MinContrast);
    p.accentDeep   = shade(p.accentBase, kHoverShade);
    p.accentSoft   = mix(p.accentBase, white, kAccentSoftMixToWhite);
    if (contrastRatio(p.brandDeep, p.accentBase) >= kTextContrast)
        p.accentOn = p.brandDeep;
    else if (contrastRatio(shade(p.accentBase, kOnColorDeepShade), p.accentBase) >= kTextContrast)
        p.accentOn = shade(p.accentBase, kOnColorDeepShade);
    else
        p.accentOn = (contrastRatio(white, p.accentBase) >= contrastRatio(QColor(Qt::black), p.accentBase))
                     ? white : QColor(Qt::black);
    p.accentText   = enforceContrast(shade(p.accentBase, kAccentTextShade), p.card, kTextContrast);
    if (contrastRatio(p.accentText, p.accentSoft) < kTextContrast)
        p.accentText = enforceContrast(p.accentText, p.accentSoft, kTextContrast);

    return p;
}

BrandPalette extractPalette(const QString &logoPath, QString *errorMsg)
{
    QString err;
    const QImage img = renderLogo(logoPath, &err);
    if (img.isNull()) {
        if (errorMsg) *errorMsg = err;
        return fallbackPalette();
    }

    // Chromatic histogram over quantized RGB buckets.
    QHash<int, int> histogram;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb pixel = img.pixel(x, y);
            if (qAlpha(pixel) < kMinAlpha)
                continue;
            const QColor color(pixel);
            const double s = color.hsvSaturationF();
            const double v = color.valueF();
            if (s < kMinSaturation || v < kMinValue)
                continue;
            const int key = bucketKey(qRed(pixel), qGreen(pixel), qBlue(pixel));
            histogram[key] += 1;
        }
    }

    if (histogram.isEmpty()) {
        if (errorMsg) errorMsg->clear();
        return fallbackPalette();
    }

    const int primaryKey = highestCountKey(histogram);
    const QColor primarySeed = bucketCenter(primaryKey);
    const double primaryHue = primarySeed.hsvHueF() * 360.0;

    // Secondary seed: highest-count bucket whose hue differs from the
    // primary's by >= kMinHueSeparationDeg; else primary rotated +45°.
    QHash<int, int> secondaryCandidates;
    for (auto it = histogram.constBegin(); it != histogram.constEnd(); ++it) {
        if (it.key() == primaryKey)
            continue;
        const QColor candidate = bucketCenter(it.key());
        const double candidateHue = candidate.hsvHueF() * 360.0;
        if (hueDistanceDeg(candidateHue, primaryHue) >= kMinHueSeparationDeg)
            secondaryCandidates.insert(it.key(), it.value());
    }

    QColor secondarySeed;
    if (!secondaryCandidates.isEmpty()) {
        secondarySeed = bucketCenter(highestCountKey(secondaryCandidates));
    } else {
        float h, s, v, a;
        primarySeed.getHsvF(&h, &s, &v, &a);
        secondarySeed = QColor::fromHsvF(fmod(static_cast<double>(h) + 0.125, 1.0), s, v);
    }

    if (errorMsg) errorMsg->clear();
    return buildPalette(primarySeed, secondarySeed);
}

// --- Local cache (Task 3) ---
namespace {
const char *kGroup = "branding";
} // namespace

void saveCachedConfig(QSettings &store, const BrandingConfig &config)
{
    store.beginGroup(QLatin1String(kGroup));
    store.setValue(QStringLiteral("mode"),
                    config.mode == ThemeMode::Manual ? QStringLiteral("manual")
                                                      : QStringLiteral("auto"));
    store.setValue(QStringLiteral("palette"),
                    QString::fromUtf8(QJsonDocument(paletteToJson(config.palette)).toJson(QJsonDocument::Compact)));
    store.setValue(QStringLiteral("logoHash"), config.logoHash);
    store.setValue(QStringLiteral("updatedAt"),
                    config.updatedAt.isValid() ? config.updatedAt.toString(Qt::ISODate)
                                               : QString());
    store.endGroup();
    store.sync();
}

BrandingConfig loadCachedConfig(QSettings &store)
{
    BrandingConfig c;
    store.beginGroup(QLatin1String(kGroup));
    const QString modeStr = store.value(QStringLiteral("mode")).toString().trimmed().toLower();
    c.mode = modeStr == QLatin1String("manual") ? ThemeMode::Manual : ThemeMode::Auto;

    const QString paletteJson = store.value(QStringLiteral("palette")).toString();
    if (paletteJson.isEmpty()) {
        c.palette = fallbackPalette();
    } else {
        const QJsonDocument doc = QJsonDocument::fromJson(paletteJson.toUtf8());
        c.palette = doc.isObject() ? paletteFromJson(doc.object()) : fallbackPalette();
    }

    c.logoHash = store.value(QStringLiteral("logoHash")).toString();
    c.updatedAt = QDateTime::fromString(store.value(QStringLiteral("updatedAt")).toString(),
                                        Qt::ISODate);
    store.endGroup();
    return c;
}

bool isNewer(const BrandingConfig &remote, const BrandingConfig &local)
{
    return remote.updatedAt.isValid()
        && (!local.updatedAt.isValid() || remote.updatedAt > local.updatedAt);
}

bool regenerateFromLogo(BrandingConfig &config, const QString &logoPath, QString *errorMsg)
{
    // Manual-mode hook (PRD condition 8): auto-regeneration is skipped
    // entirely when the config is in Manual mode — config is left untouched.
    if (config.mode == ThemeMode::Manual) {
        if (errorMsg) errorMsg->clear();
        return false;
    }

    QString err;
    const BrandPalette palette = extractPalette(logoPath, &err);
    if (!err.isEmpty()) {
        if (errorMsg) *errorMsg = err;
        return false;
    }

    QFile file(logoPath);
    QString hash;
    if (file.open(QIODevice::ReadOnly)) {
        hash = QString::fromLatin1(
            QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
    }

    config.palette = palette;
    config.logoHash = hash;
    config.updatedAt = QDateTime::currentDateTime();
    if (errorMsg) errorMsg->clear();
    return true;
}

namespace {
BrandPalette &currentStorage()
{
    static BrandPalette s_current = fallbackPalette();
    return s_current;
}
} // namespace

const BrandPalette &current()
{
    return currentStorage();
}

void setCurrent(const BrandPalette &palette)
{
    currentStorage() = palette;
}

} // namespace BrandTheme
