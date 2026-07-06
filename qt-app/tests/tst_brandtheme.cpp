#include <QtTest>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
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
    void fallbackKeepsAdminAndKioskDistinct();

    // JSON round-trip
    void paletteJsonRoundTrip();
    void configJsonRoundTrip();
    void paletteFromJsonMissingFieldsFallsBack();

    // Logo validation + extraction (Task 2)
    void validPngProducesBrandedPalette();
    void validSvgProducesBrandedPalette();
    void corruptFileRejectedWithSpecificError();
    void unsupportedExtensionRejected();
    void missingFileRejected();
    void webpNeverCrashes();
    void sameLogoTwiceIsDeterministic();
    void differentLogosDifferentPalettes();
    void twoToneLogoMapsPrimaryAndSecondary();
    void generatedPalettesMeetMinContrast();
    void greyscaleLogoFallsBack();

    // Cache, freshness, Manual-mode hook, current palette (Task 3)
    void cacheRoundTrip();
    void cacheLoadNeverBranded();
    void isNewerComparisons();
    void manualModeSkipsRegeneration();
    void autoModeRegenerates();
    void currentDefaultsToFallbackAndSets();

private:
    QString writePng(const QString &name, const QColor &fill);
    QString writeTwoTonePng(const QString &name, const QColor &left, const QColor &right);
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

QString TestBrandTheme::writeTwoTonePng(const QString &name, const QColor &left, const QColor &right)
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(left);
    QPainter p(&img);
    p.fillRect(32, 0, 32, 64, right);
    p.end();
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
    QCOMPARE(p.adminPrimary,      QColor(WitsTheme::Color::AdminPrimary));
    QCOMPARE(p.adminPrimaryHover, QColor(WitsTheme::Color::AdminPrimaryHover));
    QCOMPARE(p.kioskPrimary,      QColor(WitsTheme::Color::KioskPrimary));
    QCOMPARE(p.kioskPrimaryHover, QColor(WitsTheme::Color::KioskPrimaryHover));
    QCOMPARE(p.secondary,         QColor(WitsTheme::Color::Secondary));
    QCOMPARE(p.sidebarBase,       QColor(WitsTheme::Color::SidebarBase));
    QCOMPARE(p.card,              QColor(WitsTheme::Color::Card));
    QCOMPARE(p.appBackground,     QColor(WitsTheme::Color::AppBackground));
    QCOMPARE(p.border,            QColor(WitsTheme::Color::Border));
    QCOMPARE(p.text,              QColor(WitsTheme::Color::Text));
    QCOMPARE(p.mutedText,         QColor(WitsTheme::Color::MutedText));
    QCOMPARE(p.success,           QColor(WitsTheme::Color::Success));
    QCOMPARE(p.error,             QColor(WitsTheme::Color::Error));
    QCOMPARE(p.adminOnPrimary,    QColor(Qt::white));
    QCOMPARE(p.kioskOnPrimary,    QColor(Qt::white));
    QCOMPARE(p.adminPrimarySoft,
             BrandColorMath::mix(p.adminPrimary, QColor(Qt::white), 0.90));
    QCOMPARE(p.kioskPrimarySoft,
             BrandColorMath::mix(p.kioskPrimary, QColor(Qt::white), 0.90));
}

void TestBrandTheme::fallbackKeepsAdminAndKioskDistinct()
{
    const BrandPalette p = BrandTheme::fallbackPalette();
    QVERIFY(p.adminPrimary != p.kioskPrimary);
    QVERIFY(p.adminPrimaryHover != p.kioskPrimaryHover);
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
    QCOMPARE(p.adminPrimary, QColor("#7E1A15"));
    QCOMPARE(p.kioskPrimary, fb.kioskPrimary); // invalid -> fallback
    QCOMPARE(p.text, fb.text);                 // missing -> fallback
}

void TestBrandTheme::validPngProducesBrandedPalette()
{
    const QString path = writePng(QStringLiteral("red.png"), QColor(Qt::red));
    QString err;
    const BrandPalette p = BrandTheme::extractPalette(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY(p.adminPrimary != fb.adminPrimary);
    QVERIFY(p.adminPrimary.isValid());
}

void TestBrandTheme::validSvgProducesBrandedPalette()
{
    const QString path = writeSvg(QStringLiteral("logo.svg"), QStringLiteral("#7E1A15"));
    QString err;
    const BrandPalette p = BrandTheme::extractPalette(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QVERIFY(p.adminPrimary != fb.adminPrimary);
    QVERIFY(p.adminPrimary.isValid());
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
    QVERIFY(pRed.adminPrimary != pBlue.adminPrimary);
}

void TestBrandTheme::twoToneLogoMapsPrimaryAndSecondary()
{
    const QColor maroon(QStringLiteral("#7E1A15"));
    const QColor gold(QStringLiteral("#D4A017"));
    const QString path = writeTwoTonePng(QStringLiteral("twotone.png"), maroon, gold);
    QString err;
    const BrandPalette p = BrandTheme::extractPalette(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));

    const int maroonHue = maroon.hsvHue();
    const int goldHue = gold.hsvHue();
    QVERIFY2(qAbs(p.adminPrimary.hsvHue() - maroonHue) <= 30,
             qPrintable(QStringLiteral("adminPrimary hue %1 vs maroon hue %2")
                            .arg(p.adminPrimary.hsvHue())
                            .arg(maroonHue)));
    QVERIFY2(qAbs(p.kioskPrimary.hsvHue() - goldHue) <= 30,
             qPrintable(QStringLiteral("kioskPrimary hue %1 vs gold hue %2")
                            .arg(p.kioskPrimary.hsvHue())
                            .arg(goldHue)));
    QVERIFY(p.adminPrimary != p.kioskPrimary);
}

void TestBrandTheme::generatedPalettesMeetMinContrast()
{
    const QVector<QPair<QString, QColor>> cases = {
        { QStringLiteral("contrast-red.png"), QColor(Qt::red) },
        { QStringLiteral("contrast-blue.png"), QColor(Qt::blue) },
        { QStringLiteral("contrast-paleyellow.png"), QColor(QStringLiteral("#F0E68C")) },
        { QStringLiteral("contrast-navy.png"), QColor(QStringLiteral("#101840")) },
    };
    for (const auto &c : cases) {
        const QString path = writePng(c.first, c.second);
        QString err;
        const BrandPalette p = BrandTheme::extractPalette(path, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        const double adminContrast = BrandColorMath::contrastRatio(p.adminPrimary, p.adminOnPrimary);
        const double kioskContrast = BrandColorMath::contrastRatio(p.kioskPrimary, p.kioskOnPrimary);
        QVERIFY2(adminContrast >= BrandTheme::MinContrast,
                 qPrintable(QStringLiteral("%1: admin contrast %2").arg(c.first).arg(adminContrast)));
        QVERIFY2(kioskContrast >= BrandTheme::MinContrast,
                 qPrintable(QStringLiteral("%1: kiosk contrast %2").arg(c.first).arg(kioskContrast)));
    }
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
