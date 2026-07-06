#include <QtTest>
#include <QPalette>
#include <QColor>
#include "brandcolormath.h"
#include "theme.h"

#ifndef WITS_QSS_PATH
#define WITS_QSS_PATH "resources/wits.qss"
#endif

// Behavioral theme assertions: the palette must satisfy the properties the
// fixed light theme exists to guarantee (dark legible text on light
// surfaces), without pinning any specific hex value — so a rebrand cannot
// break this suite unless it actually breaks legibility.
class TestTheme : public QObject
{
    Q_OBJECT
private slots:
    void baseSurfacesAreLight();
    void textIsLegibleOnItsSurfaces();      // contrast >= 4.5 (WCAG AA text)
    void highlightIsLegible();              // contrast >= 3.0 (UI component)
    void placeholderIsMutedButVisible();
    void disabledTextIsMutedButVisible();
    void namedColorsAreValid();
    void adminAndKioskRolesStayDistinct();
    void hoverVariantsAreDarker();
    void loadStyleSheetReturnsNonEmpty();
    void loadStyleSheetMissingFileIsEmpty();
};

void TestTheme::baseSurfacesAreLight()
{
    const QPalette p = WitsTheme::lightPalette();
    QVERIFY(p.color(QPalette::Base).lightness() > 128);
    QVERIFY(p.color(QPalette::Window).lightness() > 128);
    QVERIFY(p.color(QPalette::Button).lightness() > 128);
}

void TestTheme::textIsLegibleOnItsSurfaces()
{
    const QPalette p = WitsTheme::lightPalette();
    using BrandColorMath::contrastRatio;
    QVERIFY(contrastRatio(p.color(QPalette::Text), p.color(QPalette::Base)) >= 4.5);
    QVERIFY(contrastRatio(p.color(QPalette::WindowText), p.color(QPalette::Window)) >= 4.5);
    QVERIFY(contrastRatio(p.color(QPalette::ButtonText), p.color(QPalette::Button)) >= 4.5);
    QVERIFY(contrastRatio(p.color(QPalette::ToolTipText), p.color(QPalette::ToolTipBase)) >= 4.5);
}

void TestTheme::highlightIsLegible()
{
    const QPalette p = WitsTheme::lightPalette();
    QVERIFY(BrandColorMath::contrastRatio(p.color(QPalette::HighlightedText),
                                          p.color(QPalette::Highlight)) >= 3.0);
}

void TestTheme::placeholderIsMutedButVisible()
{
    const QPalette p = WitsTheme::lightPalette();
    const QColor placeholder = p.color(QPalette::PlaceholderText);
    const QColor text = p.color(QPalette::Text);
    const QColor base = p.color(QPalette::Base);
    QVERIFY(placeholder != text);                       // visually distinct from real input
    QVERIFY(placeholder.lightness() > text.lightness()); // muted, not emphasized
    QVERIFY(BrandColorMath::contrastRatio(placeholder, base) >= 1.5); // still visible
}

void TestTheme::disabledTextIsMutedButVisible()
{
    const QPalette p = WitsTheme::lightPalette();
    const QColor disabled = p.color(QPalette::Disabled, QPalette::Text);
    const QColor normal = p.color(QPalette::Text);
    QVERIFY(disabled.lightness() > normal.lightness());
    QVERIFY(BrandColorMath::contrastRatio(disabled, p.color(QPalette::Base)) >= 1.5);
}

void TestTheme::namedColorsAreValid()
{
    const QStringList all = {
        WitsTheme::Color::SidebarBase, WitsTheme::Color::Card,
        WitsTheme::Color::AppBackground, WitsTheme::Color::Border,
        WitsTheme::Color::Text, WitsTheme::Color::MutedText,
        WitsTheme::Color::KioskPrimary, WitsTheme::Color::KioskPrimaryHover,
        WitsTheme::Color::AdminPrimary, WitsTheme::Color::AdminPrimaryHover,
        WitsTheme::Color::Secondary, WitsTheme::Color::Success,
        WitsTheme::Color::Error,
    };
    for (const QString &name : all)
        QVERIFY2(QColor(name).isValid(), qPrintable(name));
}

void TestTheme::adminAndKioskRolesStayDistinct()
{
    QVERIFY(QColor(WitsTheme::Color::AdminPrimary) != QColor(WitsTheme::Color::KioskPrimary));
    QVERIFY(QColor(WitsTheme::Color::AdminPrimaryHover) != QColor(WitsTheme::Color::KioskPrimaryHover));
}

void TestTheme::hoverVariantsAreDarker()
{
    using BrandColorMath::relativeLuminance;
    QVERIFY(relativeLuminance(QColor(WitsTheme::Color::AdminPrimaryHover))
            < relativeLuminance(QColor(WitsTheme::Color::AdminPrimary)));
    QVERIFY(relativeLuminance(QColor(WitsTheme::Color::KioskPrimaryHover))
            < relativeLuminance(QColor(WitsTheme::Color::KioskPrimary)));
}

void TestTheme::loadStyleSheetReturnsNonEmpty()
{
    const QString qss = WitsTheme::loadStyleSheet(QStringLiteral(WITS_QSS_PATH));
    QVERIFY2(!qss.isEmpty(), "wits.qss should load with content");
    QVERIFY2(qss.contains("QPushButton"), "wits.qss should contain button rules");
}

void TestTheme::loadStyleSheetMissingFileIsEmpty()
{
    QCOMPARE(WitsTheme::loadStyleSheet(QStringLiteral("/no/such/file.qss")), QString());
}

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
