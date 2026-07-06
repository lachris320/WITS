#include <QtTest>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include "brandcolormath.h"
#include "brandtheme.h"
#include "theme.h"

class TestBrandTheme : public QObject
{
    Q_OBJECT
private slots:
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
};

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

QTEST_GUILESS_MAIN(TestBrandTheme)
#include "tst_brandtheme.moc"
