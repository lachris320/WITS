#include <QtTest>
#include <QPalette>
#include <QColor>
#include "theme.h"

#ifndef WITS_QSS_PATH
#define WITS_QSS_PATH "resources/wits.qss"
#endif

class TestTheme : public QObject
{
    Q_OBJECT
private slots:
    void inputTextColorIsDark();
    void inputBackgroundIsLight();
    void windowTextIsDark();
    void textIsDarkerThanBase();
    void highlightColors();
    void loadStyleSheetReturnsNonEmpty();
    void loadStyleSheetMissingFileIsEmpty();
};

void TestTheme::inputTextColorIsDark()
{
    QCOMPARE(WitsTheme::lightPalette().color(QPalette::Text), QColor("#2C3E50"));
}

void TestTheme::inputBackgroundIsLight()
{
    QCOMPARE(WitsTheme::lightPalette().color(QPalette::Base), QColor("#FFFFFF"));
}

void TestTheme::windowTextIsDark()
{
    QCOMPARE(WitsTheme::lightPalette().color(QPalette::WindowText), QColor("#2C3E50"));
}

void TestTheme::textIsDarkerThanBase()
{
    const QPalette p = WitsTheme::lightPalette();
    const QColor text = p.color(QPalette::Text);
    const QColor base = p.color(QPalette::Base);
    QCOMPARE(text.lightness() < base.lightness(), true);
}

void TestTheme::highlightColors()
{
    const QPalette p = WitsTheme::lightPalette();
    QCOMPARE(p.color(QPalette::Highlight), QColor("#4A90E2"));
    QCOMPARE(p.color(QPalette::HighlightedText), QColor("#FFFFFF"));
}

void TestTheme::loadStyleSheetReturnsNonEmpty()
{
    // WITS_QSS_PATH is injected by CMake to the on-disk wits.qss.
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
