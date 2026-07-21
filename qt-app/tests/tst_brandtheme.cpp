#include <QtTest>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>
#include "brandcolormath.h"
#include "brandtheme.h"
#include "theme.h"

class TestBrandTheme : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    // Color math
    void contrastBlackWhiteIsMax();
    void contrastSameColorIsOne();
    void shadeDarkensAndClamps();
    void mixReachesEndpoints();

    // Fallback palette
    void fallbackMatchesWitsThemeConstants();

    // JSON round-trip
    void paletteJsonRoundTrip();
    void configJsonRoundTrip();
    void paletteFromJsonMissingFieldsFallsBack();
    void paletteFromJsonReadsOldKeys();
    void paletteFromJsonMixedKeysNewWins();
    void paletteToJsonDualWritesBothKeySets();
    void paletteFromJsonOldKioskPrimaryBeatsSecondary();

    // Logo validation + extraction (Task 2)
    void validPngProducesBrandedPalette();
    void validSvgProducesBrandedPalette();
    void corruptFileRejectedWithSpecificError();
    void unsupportedExtensionRejected();
    void missingFileRejected();
    void webpNeverCrashes();
    void sameLogoTwiceIsDeterministic();
    void differentLogosDifferentPalettes();
    void brandAndAccentAreDistinctHues();
    void rolePaletteMeetsSplitContrastFloors();
    void brandOnMutedRecoversByLightening();
    void greyscaleLogoFallsBack();
    void buildPaletteIsCallableFromSeeds();

    // Parameterised contrast enforcement (Task 5)
    void enforceContrastReachesTarget();
    void raiseToContrastLightensDarkAccent();

    // Cache, freshness, Manual-mode hook, current palette (Task 3)
    void cacheRoundTrip();
    void cacheLoadNeverBranded();
    void isNewerComparisons();
    void manualModeSkipsRegeneration();
    void autoModeRegenerates();
    void currentDefaultsToFallbackAndSets();

private:
    QString writePng(const QString &name, const QColor &fill);
    QString writeSvg(const QString &name, const QString &fillHex);
    QString writeGarbage(const QString &name);

    QTemporaryDir m_dir;
};

void TestBrandTheme::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

QString TestBrandTheme::writePng(const QString &name, const QColor &fill)
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(fill);
    const QString path = m_dir.filePath(name);
    if (!img.save(path, "PNG"))
        qWarning("failed to write test PNG: %s", qPrintable(path));
    return path;
}

QString TestBrandTheme::writeSvg(const QString &name, const QString &fillHex)
{
    const QString path = m_dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << QStringLiteral(
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\">"
            "<rect width=\"64\" height=\"64\" fill=\"%1\"/></svg>")
                   .arg(fillHex);
    } else {
        qWarning("failed to write test SVG: %s", qPrintable(path));
    }
    return path;
}

QString TestBrandTheme::writeGarbage(const QString &name)
{
    const QString path = m_dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write("this is not an image");
    } else {
        qWarning("failed to write garbage file: %s", qPrintable(path));
    }
    return path;
}

void TestBrandTheme::contrastBlackWhiteIsMax()
{
    const double r = BrandColorMath::contrastRatio(QColor(Qt::black), QColor(Qt::white));
    QVERIFY2(r > 20.9 && r <= 21.0, qPrintable(QString::number(r)));
}

void TestBrandTheme::contrastSameColorIsOne()
{
    QCOMPARE(BrandColorMath::contrastRatio(QColor("#7E1A15"), QColor("#7E1A15")), 1.0);
}

void TestBrandTheme::shadeDarkensAndClamps()
{
    const QColor base(100, 200, 50);
    const QColor darker = BrandColorMath::shade(base, -0.28);
    QCOMPARE(darker, QColor(72, 144, 36));
    QCOMPARE(BrandColorMath::shade(QColor(200, 200, 200), 1.0), QColor(255, 255, 255));
    QCOMPARE(BrandColorMath::shade(base, -1.0), QColor(0, 0, 0));
}

void TestBrandTheme::mixReachesEndpoints()
{
    const QColor a(10, 20, 30), b(210, 220, 230);
    QCOMPARE(BrandColorMath::mix(a, b, 0.0), a);
    QCOMPARE(BrandColorMath::mix(a, b, 1.0), b);
    QCOMPARE(BrandColorMath::mix(a, b, 0.5), QColor(110, 120, 130));
}

void TestBrandTheme::fallbackMatchesWitsThemeConstants()
{
    const BrandPalette p = BrandTheme::fallbackPalette();
    QCOMPARE(p.brandBase,         QColor(WitsTheme::Color::AdminPrimary));
    QCOMPARE(p.brandDeep,         QColor(WitsTheme::Color::AdminPrimaryHover));
    QCOMPARE(p.accentBase,        QColor(WitsTheme::Color::KioskPrimary));
    QCOMPARE(p.accentDeep,        QColor(WitsTheme::Color::KioskPrimaryHover));
    QCOMPARE(p.sidebarBase,       QColor(WitsTheme::Color::SidebarBase));
    QCOMPARE(p.card,              QColor(WitsTheme::Color::Card));
    QCOMPARE(p.appBackground,     QColor(WitsTheme::Color::AppBackground));
    QCOMPARE(p.border,            QColor(WitsTheme::Color::Border));
    QCOMPARE(p.text,              QColor(WitsTheme::Color::Text));
    QCOMPARE(p.mutedText,         QColor(WitsTheme::Color::MutedText));
    QCOMPARE(p.success,           QColor(WitsTheme::Color::Success));
    QCOMPARE(p.error,             QColor(WitsTheme::Color::Error));
    QCOMPARE(p.brandOn,           QColor(Qt::white));
    QCOMPARE(p.accentOn,          QColor(Qt::white));
    QCOMPARE(p.brandSoft,
             BrandColorMath::mix(p.brandBase, QColor(Qt::white), 0.90));
    QCOMPARE(p.accentSoft,
             BrandColorMath::mix(p.accentBase, QColor(Qt::white), 0.90));
}

void TestBrandTheme::paletteJsonRoundTrip()
{
    const BrandPalette p = BrandTheme::fallbackPalette();
    const BrandPalette back = BrandTheme::paletteFromJson(BrandTheme::paletteToJson(p));
    QVERIFY(back == p);
}

void TestBrandTheme::configJsonRoundTrip()
{
    BrandingConfig c;
    c.mode = ThemeMode::Manual;
    c.palette = BrandTheme::fallbackPalette();
    c.logoHash = QStringLiteral("ab").repeated(32);
    c.updatedAt = QDateTime::fromString(QStringLiteral("2026-07-06 12:34:56"),
                                        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const BrandingConfig back = BrandTheme::configFromJson(BrandTheme::configToJson(c));
    QCOMPARE(back.mode, ThemeMode::Manual);
    QVERIFY(back.palette == c.palette);
    QCOMPARE(back.logoHash, c.logoHash);
    QCOMPARE(back.updatedAt, c.updatedAt);
}

void TestBrandTheme::paletteFromJsonMissingFieldsFallsBack()
{
    QJsonObject o;
    o.insert(QStringLiteral("admin_primary"), QStringLiteral("#7E1A15"));
    o.insert(QStringLiteral("kiosk_primary"), QStringLiteral("not-a-color"));
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QCOMPARE(p.brandBase, QColor("#7E1A15"));
    QCOMPARE(p.accentBase, fb.accentBase); // invalid -> fallback
    QCOMPARE(p.text, fb.text);              // missing -> fallback
}

void TestBrandTheme::paletteFromJsonReadsOldKeys()
{
    // A config written by ANY pre-4d build holds only the OLD snake_case keys.
    // Read-compat must land them in the new role fields, not silently fall back.
    QJsonObject o;
    o.insert("admin_primary", "#123456");        // -> brandBase
    o.insert("kiosk_primary", "#abcdef");        // -> accentBase
    o.insert("admin_on_primary", "#ffffff");     // -> brandOn
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    QCOMPARE(p.brandBase,  QColor("#123456"));
    QCOMPARE(p.accentBase, QColor("#abcdef"));
    QCOMPARE(p.brandOn,    QColor("#ffffff"));
}

void TestBrandTheme::paletteFromJsonMixedKeysNewWins()
{
    // When both keys for the same field are present, the NEW key is authoritative.
    QJsonObject o;
    o.insert("admin_primary", "#111111");  // old alias for brandBase
    o.insert("brand_base",    "#222222");  // new key for brandBase
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    QCOMPARE(p.brandBase, QColor("#222222"));
}

void TestBrandTheme::paletteToJsonDualWritesBothKeySets()
{
    BrandPalette p = BrandTheme::fallbackPalette();
    p.brandBase = QColor("#123456");
    const QJsonObject o = BrandTheme::paletteToJson(p);
    QCOMPARE(o.value("brand_base").toString(),    QString("#123456")); // new key
    QCOMPARE(o.value("admin_primary").toString(), QString("#123456")); // old key, equal value
}

void TestBrandTheme::paletteFromJsonOldKioskPrimaryBeatsSecondary()
{
    // A pre-4d cached fallback palette wrote BOTH "kiosk_primary" (#10B981,
    // the semantically correct accent source) and "secondary" (#3B82F6, a
    // distinct legacy field collapsed into accentBase in 4d). The alias
    // table must apply "secondary" before "kiosk_primary" so kiosk_primary
    // wins — otherwise every accent surface silently recolors blue on
    // migration from an old cached config.
    QJsonObject o;
    o.insert("secondary",     "#3B82F6");
    o.insert("kiosk_primary", "#10B981");
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    QCOMPARE(p.accentBase, QColor("#10B981"));
}

void TestBrandTheme::validPngProducesBrandedPalette()
{
    const QString path = writePng(QStringLiteral("red.png"), QColor(Qt::red));
    QString err;
    const BrandPalette p = BrandTheme::extractPalette(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY(p.brandBase != fb.brandBase);
    QVERIFY(p.brandBase.isValid());
}

void TestBrandTheme::validSvgProducesBrandedPalette()
{
    const QString path = writeSvg(QStringLiteral("logo.svg"), QStringLiteral("#7E1A15"));
    QString err;
    const BrandPalette p = BrandTheme::extractPalette(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY(p.brandBase != fb.brandBase);
    QVERIFY(p.brandBase.isValid());
}

void TestBrandTheme::corruptFileRejectedWithSpecificError()
{
    const QString path = writeGarbage(QStringLiteral("garbage.png"));
    QString validateErr;
    const bool valid = BrandTheme::validateLogoFile(path, &validateErr);
    QVERIFY(!valid);
    QVERIFY2(validateErr.contains(QStringLiteral("corrupt")), qPrintable(validateErr));

    QString extractErr;
    const BrandPalette p = BrandTheme::extractPalette(path, &extractErr);
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY(p == fb);
    QVERIFY(!extractErr.isEmpty());
}

void TestBrandTheme::unsupportedExtensionRejected()
{
    const QString path = writeGarbage(QStringLiteral("garbage.txt"));
    QString err;
    const bool valid = BrandTheme::validateLogoFile(path, &err);
    QVERIFY(!valid);
    QVERIFY2(err.contains(QStringLiteral("Unsupported logo format")), qPrintable(err));
}

void TestBrandTheme::missingFileRejected()
{
    const QString path = m_dir.filePath(QStringLiteral("does-not-exist.png"));
    QString err;
    const bool valid = BrandTheme::validateLogoFile(path, &err);
    QVERIFY(!valid);
    QVERIFY2(err.contains(QStringLiteral("not found")), qPrintable(err));
}

void TestBrandTheme::webpNeverCrashes()
{
    const QString path = writeGarbage(QStringLiteral("garbage.webp"));
    QString err;
    const bool valid = BrandTheme::validateLogoFile(path, &err);
    QVERIFY(!valid);
    if (!QImageReader::supportedImageFormats().contains("webp")) {
        QVERIFY2(err.contains(QStringLiteral("not supported")), qPrintable(err));
    } else {
        QVERIFY2(err.contains(QStringLiteral("corrupt or unsupported")), qPrintable(err));
    }
}

void TestBrandTheme::sameLogoTwiceIsDeterministic()
{
    const QString path = writePng(QStringLiteral("red-det.png"), QColor(Qt::red));
    QString err1, err2;
    const BrandPalette p1 = BrandTheme::extractPalette(path, &err1);
    const BrandPalette p2 = BrandTheme::extractPalette(path, &err2);
    QVERIFY2(err1.isEmpty(), qPrintable(err1));
    QVERIFY2(err2.isEmpty(), qPrintable(err2));
    QVERIFY(p1 == p2);
}

void TestBrandTheme::differentLogosDifferentPalettes()
{
    const QString redPath = writePng(QStringLiteral("red-diff.png"), QColor(Qt::red));
    const QString bluePath = writePng(QStringLiteral("blue-diff.png"), QColor(Qt::blue));
    QString errRed, errBlue;
    const BrandPalette pRed = BrandTheme::extractPalette(redPath, &errRed);
    const BrandPalette pBlue = BrandTheme::extractPalette(bluePath, &errBlue);
    QVERIFY2(errRed.isEmpty(), qPrintable(errRed));
    QVERIFY2(errBlue.isEmpty(), qPrintable(errBlue));
    QVERIFY(pRed.brandBase != pBlue.brandBase);
}

void TestBrandTheme::rolePaletteMeetsSplitContrastFloors()
{
    const BrandPalette p = BrandTheme::buildPalette(QColor("#7E1A15"), QColor("#E8B10E"));
    using BrandColorMath::contrastRatio;
    QVERIFY(contrastRatio(p.brandOn,     p.brandBase)  >= 4.5); // text on brand fill
    QVERIFY(contrastRatio(p.accentOn,    p.accentBase) >= 4.5); // text on accent fill
    QVERIFY(contrastRatio(p.accentBase,  p.brandBase)  >= 3.0); // graphical highlight on dark
    QVERIFY(contrastRatio(p.accentText,  p.card)       >= 4.5); // accent-as-text on paper
    QVERIFY(contrastRatio(p.accentText,  p.accentSoft) >= 4.5); // accent-as-text on its tint
    QVERIFY(contrastRatio(p.brandText,   p.card)       >= 4.5); // brand-as-text on paper
    QVERIFY(contrastRatio(p.brandOnMuted, p.brandBase) >= 4.5); // muted nav label on brand
}

void TestBrandTheme::brandOnMutedRecoversByLightening()
{
    using BrandColorMath::contrastRatio;
    // Steel-blue primary: brandBase stays #407090 (5.33:1 vs white), and the
    // un-clamped brandOnMuted = mix(white, brandBase, 0.25) is #CFDBE3 at only
    // 3.78:1 vs brandBase — so the recovery clamp MUST fire. The OLD darken clamp
    // drove it to near-black #1C1D1F (3.16:1, still failing); the LIGHTEN fix
    // reaches #E9F6FF (4.85:1). This asserts BOTH the floor and the direction.
    const BrandPalette p = BrandTheme::buildPalette(QColor("#407090"), QColor("#E8B10E"));
    const QColor rawMix = BrandColorMath::mix(QColor(Qt::white), p.brandBase, 0.25);
    QVERIFY(contrastRatio(rawMix, p.brandBase) < 4.5);              // precondition: clamp fires
    QVERIFY(contrastRatio(p.brandOnMuted, p.brandBase) >= 4.5);     // floor met
    QVERIFY(p.brandOnMuted.valueF() > rawMix.valueF());            // it got LIGHTER, not darker
}

void TestBrandTheme::brandAndAccentAreDistinctHues()
{
    const BrandPalette p = BrandTheme::buildPalette(QColor("#7E1A15"), QColor("#E8B10E"));
    const double dh = std::abs(p.brandBase.hsvHueF()*360.0 - p.accentBase.hsvHueF()*360.0);
    QVERIFY(qMin(dh, 360.0 - dh) >= 30.0);
}

void TestBrandTheme::greyscaleLogoFallsBack()
{
    const QString path = writePng(QStringLiteral("grey.png"), QColor(QStringLiteral("#808080")));
    QString err;
    const BrandPalette p = BrandTheme::extractPalette(path, &err);
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(p == fb);
}

void TestBrandTheme::buildPaletteIsCallableFromSeeds()
{
    // Maroon primary, gold secondary — the reference case. buildPalette is
    // the pure build step: callable directly from two seed colors, without
    // a logo image, for deterministic unit testing of the derivation.
    const BrandPalette p = BrandTheme::buildPalette(QColor("#7E1A15"), QColor("#E8B10E"));
    QVERIFY(p.brandBase.isValid());
    QVERIFY(p.accentBase.isValid());
    QVERIFY(p.brandBase != p.accentBase);
}

void TestBrandTheme::enforceContrastReachesTarget()
{
    const QColor white(Qt::white);
    const QColor out = BrandTheme::enforceContrast(QColor("#999999"), white, 4.5);
    QVERIFY(BrandColorMath::contrastRatio(out, white) >= 4.5);
}

void TestBrandTheme::raiseToContrastLightensDarkAccent()
{
    const QColor brand("#4A0E0B");   // dark maroon
    const QColor out = BrandTheme::raiseToContrast(QColor("#1A2340"), brand, 3.0); // dark navy
    QVERIFY(BrandColorMath::contrastRatio(out, brand) >= 3.0);
    QVERIFY(out.valueF() > QColor("#1A2340").valueF()); // it got lighter, not darker
}

void TestBrandTheme::cacheRoundTrip()
{
    QSettings s(m_dir.path() + QStringLiteral("/branding.ini"), QSettings::IniFormat);

    BrandingConfig c;
    c.mode = ThemeMode::Manual;
    // Extracted colors can carry a non-Rgb QColor spec (e.g. built via
    // fromHsvF) that is visually identical but fails QColor::operator==
    // against a hex-reconstructed color. Normalize through one JSON
    // round-trip so `c.palette` reflects the same Rgb/hex precision the
    // cache actually stores, matching what loadCachedConfig will return.
    const BrandPalette extracted =
        BrandTheme::extractPalette(writePng(QStringLiteral("cache-red.png"), QColor(Qt::red)), nullptr);
    c.palette = BrandTheme::paletteFromJson(BrandTheme::paletteToJson(extracted));
    c.logoHash = QStringLiteral("ab").repeated(32);
    // Qt::ISODate (the cache's timestamp format, per spec) has second
    // precision, so seed with a whole-second QDateTime to make the
    // round-trip exact rather than truncating milliseconds away.
    c.updatedAt = QDateTime::fromString(
        QDateTime::currentDateTime().toString(Qt::ISODate), Qt::ISODate);

    BrandTheme::saveCachedConfig(s, c);
    const BrandingConfig back = BrandTheme::loadCachedConfig(s);

    QCOMPARE(back.mode, c.mode);
    QVERIFY(back.palette == c.palette);
    QCOMPARE(back.logoHash, c.logoHash);
    QCOMPARE(back.updatedAt, c.updatedAt);
}

void TestBrandTheme::cacheLoadNeverBranded()
{
    QSettings s(m_dir.path() + QStringLiteral("/branding-empty.ini"), QSettings::IniFormat);
    const BrandingConfig c = BrandTheme::loadCachedConfig(s);
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QCOMPARE(c.mode, ThemeMode::Auto);
    QVERIFY(c.palette == fb);
    QVERIFY(c.logoHash.isEmpty());
    QVERIFY(!c.updatedAt.isValid());
}

void TestBrandTheme::isNewerComparisons()
{
    BrandingConfig remote;
    BrandingConfig local;

    remote.updatedAt = QDateTime::currentDateTime();
    local.updatedAt = remote.updatedAt.addSecs(-60);
    QVERIFY(BrandTheme::isNewer(remote, local));

    local.updatedAt = remote.updatedAt;
    QVERIFY(!BrandTheme::isNewer(remote, local));

    BrandingConfig invalidRemote;
    BrandingConfig validLocal;
    validLocal.updatedAt = QDateTime::currentDateTime();
    QVERIFY(!BrandTheme::isNewer(invalidRemote, validLocal));

    BrandingConfig validRemote;
    validRemote.updatedAt = QDateTime::currentDateTime();
    BrandingConfig invalidLocal;
    QVERIFY(BrandTheme::isNewer(validRemote, invalidLocal));
}

void TestBrandTheme::manualModeSkipsRegeneration()
{
    const QString path = writePng(QStringLiteral("manual-red.png"), QColor(Qt::red));
    BrandingConfig cfg;
    cfg.mode = ThemeMode::Manual;
    cfg.palette = BrandTheme::fallbackPalette();

    QString err;
    const bool result = BrandTheme::regenerateFromLogo(cfg, path, &err);

    QVERIFY(!result);
    QVERIFY(err.isEmpty());
    QVERIFY(cfg.palette == BrandTheme::fallbackPalette());
    QVERIFY(cfg.logoHash.isEmpty());
}

void TestBrandTheme::autoModeRegenerates()
{
    const QString path = writePng(QStringLiteral("auto-red.png"), QColor(Qt::red));
    BrandingConfig cfg;
    cfg.mode = ThemeMode::Auto;
    cfg.palette = BrandTheme::fallbackPalette();

    QString err;
    const bool result = BrandTheme::regenerateFromLogo(cfg, path, &err);

    QVERIFY2(result, qPrintable(err));
    QVERIFY(cfg.palette != BrandTheme::fallbackPalette());
    QCOMPARE(cfg.logoHash.length(), 64);
    QVERIFY(!cfg.logoHash.contains(QRegularExpression(QStringLiteral("[^0-9a-f]"))));
    QVERIFY(cfg.updatedAt.isValid());
}

void TestBrandTheme::currentDefaultsToFallbackAndSets()
{
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY(BrandTheme::current() == fb);

    const QString path = writePng(QStringLiteral("current-red.png"), QColor(Qt::red));
    const BrandPalette extracted = BrandTheme::extractPalette(path, nullptr);
    BrandTheme::setCurrent(extracted);
    QVERIFY(BrandTheme::current() == extracted);

    BrandTheme::setCurrent(fb);
    QVERIFY(BrandTheme::current() == fb);
}

QTEST_GUILESS_MAIN(TestBrandTheme)
#include "tst_brandtheme.moc"
