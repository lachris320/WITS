#include <QtTest>
#include <QColor>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
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
    QCOMPARE(vm.adminPrimary(), BrandTheme::current().brandBase);
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
    QCOMPARE(vm.adminPrimary(), QColor(0x12, 0x34, 0x56));
}

void TestThemeViewModel::regenerateFromLogoRethemesAndNotifies()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;
    QSignalSpy spy(&vm, &ThemeViewModel::changed);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString logo = writeSolidPng(dir.filePath("logo.png"), QColor(0x2E, 0x86, 0xC1));

    QVERIFY(vm.regenerateFromImportedLogo(logo));   // Auto mode -> re-extracts
    QCOMPARE(spy.count(), 1);
    // A chromatic logo yields a branded admin role distinct from the fallback.
    QVERIFY(vm.adminPrimary() != BrandTheme::fallbackPalette().brandBase);
}

void TestThemeViewModel::getterIsLiveNotCached()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;

    BrandPalette custom = BrandTheme::fallbackPalette();
    custom.brandBase = QColor(0x0A, 0x0B, 0x0C);
    BrandTheme::setCurrent(custom);   // change the engine, do NOT call vm.refresh()

    // Single source of truth: the getter reflects the engine immediately.
    QCOMPARE(vm.adminPrimary(), QColor(0x0A, 0x0B, 0x0C));
}

QTEST_MAIN(TestThemeViewModel)
#include "tst_themeviewmodel.moc"
