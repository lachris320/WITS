#include <QtTest>
#include <QColor>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QStyleHints>
#include "ThemeViewModel.h"
#include "brandtheme.h"
#include "brandthemedata.h"

class TestThemeViewModel : public QObject
{
    Q_OBJECT
private slots:
    void mapsCurrentBrandRole();
    void refreshEmitsChangedAfterExternalSetCurrent();
    void regenerateFromLogoRethemesAndNotifies();
    void getterIsLiveNotCached();
    void roleAccessorsReadEngine();
    void init();   // reset persisted mode to a known default before each test
    void modePersistsAcrossInstances();
    void resolvedDarkTruthTable();
    void systemModeFollowsColorScheme();
    void setModeEmitsSignals();

private:
    QString writeSolidPng(const QString &path, const QColor &fill);
};

QString TestThemeViewModel::writeSolidPng(const QString &path, const QColor &fill)
{
    QImage img(48, 48, QImage::Format_ARGB32);
    img.fill(fill);
    img.save(path, "PNG");
    return path;
}

void TestThemeViewModel::mapsCurrentBrandRole()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;
    QCOMPARE(vm.brandBase(), BrandTheme::current().brandBase);
}

void TestThemeViewModel::refreshEmitsChangedAfterExternalSetCurrent()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;
    QSignalSpy spy(&vm, &ThemeViewModel::changed);

    BrandPalette custom = BrandTheme::fallbackPalette();
    custom.brandBase = QColor(0x12, 0x34, 0x56);
    BrandTheme::setCurrent(custom);

    vm.refresh();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(vm.brandBase(), QColor(0x12, 0x34, 0x56));
}

void TestThemeViewModel::regenerateFromLogoRethemesAndNotifies()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;
    QSignalSpy spy(&vm, &ThemeViewModel::changed);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Solid maroon #7E1A15 — a gate-surviving logo (a steel-blue solid would
    // fail the quality gate and fall back, which is a different code path).
    const QString logo = writeSolidPng(dir.filePath("logo.png"), QColor(0x7E, 0x1A, 0x15));

    QCOMPARE(vm.regenerateFromImportedLogo(logo),
             ThemeViewModel::RegenResult::Ok);   // Auto mode -> re-extracts a usable palette
    QCOMPARE(spy.count(), 1);
    // A chromatic logo yields a branded base role distinct from the fallback.
    QVERIFY(vm.brandBase() != BrandTheme::fallbackPalette().brandBase);
}

void TestThemeViewModel::getterIsLiveNotCached()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;

    BrandPalette custom = BrandTheme::fallbackPalette();
    custom.brandBase = QColor(0x0A, 0x0B, 0x0C);
    BrandTheme::setCurrent(custom);   // change the engine, do NOT call vm.refresh()

    // Single source of truth: the getter reflects the engine immediately.
    QCOMPARE(vm.brandBase(), QColor(0x0A, 0x0B, 0x0C));
}

void TestThemeViewModel::roleAccessorsReadEngine()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;

    // The role accessors are the one source of truth, read straight from the
    // engine's current palette.
    QCOMPARE(vm.brandBase(), BrandTheme::current().brandBase);
    QCOMPARE(vm.accentBase(), BrandTheme::current().accentBase);

    // Metaobject check: proves the Q_PROPERTY (what QML sees), not just the
    // C++ method, is registered under the new role name — catches a typo'd
    // READ name that would otherwise only surface in Task 3's QML.
    QVERIFY(vm.property("brandBase").isValid());
    QCOMPARE(vm.property("brandBase").value<QColor>(), vm.brandBase());
}

void TestThemeViewModel::init()
{
    // AppSettings is process-isolated in tests, but shared across test
    // functions in this process; reset to the default so each test is
    // independent of persisted leftovers from another.
    ThemeViewModel v;
    v.setMode("System");
}

void TestThemeViewModel::modePersistsAcrossInstances()
{
    { ThemeViewModel vm; vm.setMode("Dark"); }
    ThemeViewModel vm2;
    QCOMPARE(vm2.mode(), QStringLiteral("Dark"));
}

void TestThemeViewModel::resolvedDarkTruthTable()
{
    ThemeViewModel vm;
    vm.setMode("Light");
    vm.applySystemColorScheme(Qt::ColorScheme::Dark);
    QVERIFY(!vm.resolvedDark());                       // Light overrides system

    vm.setMode("Dark");
    vm.applySystemColorScheme(Qt::ColorScheme::Light);
    QVERIFY(vm.resolvedDark());                        // Dark overrides system

    vm.setMode("System");
    vm.applySystemColorScheme(Qt::ColorScheme::Dark);
    QVERIFY(vm.resolvedDark());                        // System follows dark OS
    vm.applySystemColorScheme(Qt::ColorScheme::Light);
    QVERIFY(!vm.resolvedDark());                       // System follows light OS
    vm.applySystemColorScheme(Qt::ColorScheme::Unknown);
    QVERIFY(!vm.resolvedDark());                       // Unknown treated as light
}

void TestThemeViewModel::systemModeFollowsColorScheme()
{
    ThemeViewModel vm;
    vm.setMode("System");
    vm.applySystemColorScheme(Qt::ColorScheme::Light);
    QSignalSpy spy(&vm, &ThemeViewModel::resolvedDarkChanged);
    vm.applySystemColorScheme(Qt::ColorScheme::Dark);
    QCOMPARE(spy.count(), 1);
    QVERIFY(vm.resolvedDark());
}

void TestThemeViewModel::setModeEmitsSignals()
{
    ThemeViewModel vm;
    vm.setMode("Light");
    QSignalSpy modeSpy(&vm, &ThemeViewModel::modeChanged);
    QSignalSpy darkSpy(&vm, &ThemeViewModel::resolvedDarkChanged);
    vm.setMode("Dark");
    QCOMPARE(modeSpy.count(), 1);   // the picker binding re-evaluates
    QCOMPARE(darkSpy.count(), 1);   // isDark re-evaluates; drives the token flip
}

QTEST_MAIN(TestThemeViewModel)
#include "tst_themeviewmodel.moc"
