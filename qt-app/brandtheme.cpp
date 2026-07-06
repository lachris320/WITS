#include "brandtheme.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
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
constexpr double kEnforceStep = -0.08;
constexpr int kEnforceMaxIterations = 24;

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

// Darken c until it meets MinContrast against white, capped iterations.
QColor enforceOnWhite(const QColor &c)
{
    QColor result = c;
    const QColor white(Qt::white);
    int iterations = 0;
    while (contrastRatio(result, white) < MinContrast && iterations < kEnforceMaxIterations) {
        result = shade(result, kEnforceStep);
        ++iterations;
    }
    return result;
}

} // namespace

bool validateLogoFile(const QString &logoPath, QString *errorMsg)
{
    QString err;
    const QImage img = renderLogo(logoPath, &err);
    if (errorMsg) *errorMsg = err;
    return !img.isNull();
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

    // Build the palette.
    const QColor white(Qt::white);
    BrandPalette p;

    p.adminPrimary = enforceOnWhite(primarySeed);
    p.adminOnPrimary = white;

    QColor kioskPrimary;
    QColor kioskOnPrimary;
    if (contrastRatio(secondarySeed, white) >= MinContrast) {
        kioskPrimary = secondarySeed;
        kioskOnPrimary = white;
    } else if (contrastRatio(secondarySeed, shade(secondarySeed, kOnColorDeepShade)) >= MinContrast) {
        kioskPrimary = secondarySeed;
        kioskOnPrimary = shade(secondarySeed, kOnColorDeepShade);
    } else {
        kioskPrimary = enforceOnWhite(secondarySeed);
        kioskOnPrimary = white;
    }
    p.kioskPrimary = kioskPrimary;
    p.kioskOnPrimary = kioskOnPrimary;

    p.adminPrimaryHover = shade(p.adminPrimary, kHoverShade);
    p.kioskPrimaryHover = shade(p.kioskPrimary, kHoverShade);
    p.adminPrimarySoft = mix(p.adminPrimary, white, kSoftMixToWhite);
    p.kioskPrimarySoft = mix(p.kioskPrimary, white, kSoftMixToWhite);
    p.secondary = p.kioskPrimary;

    const BrandPalette fb = fallbackPalette();
    p.sidebarBase   = fb.sidebarBase;
    p.card          = fb.card;
    p.appBackground = fb.appBackground;
    p.border        = fb.border;
    p.text          = fb.text;
    p.mutedText     = fb.mutedText;
    p.success       = fb.success;
    p.error         = fb.error;

    if (errorMsg) errorMsg->clear();
    return p;
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
