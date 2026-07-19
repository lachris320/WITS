#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QImage>
#include "SettingsViewModel.h"

class TestSettingsViewModel : public QObject
{
    Q_OBJECT
    QTemporaryDir m_tmp;
private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmp.path());
    }
    void init()
    {
        // Seed a known baseline in the redirected QSettings before each test.
        QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
        s.clear();
        s.setValue(QStringLiteral("school/name"), QStringLiteral("Acme Library"));
        s.setValue(QStringLiteral("school/address"), QStringLiteral("123 Main St"));
        s.setValue(QStringLiteral("admin/name"), QStringLiteral("J. Rizal"));
        s.setValue(QStringLiteral("admin/position"), QStringLiteral("Head Librarian"));
        s.setValue(QStringLiteral("library/openHour"), 8);
        s.setValue(QStringLiteral("library/closeHour"), 17);
        s.setValue(QStringLiteral("features/guestLogin"), true);
        s.sync();
    }

    void loadPopulatesPropertiesFromSettings();
    void loadStartsClean();
    void editingASettingMarksDirtyAndSignals();
    void resettingToSavedValueClearsDirty();
    void saveePersistsAllFieldsAndEmitsSaved();
    void saveMirrorsGuestFlagOntoKioskKey();
    void saveClearsDirty();
    void importLogoUpdatesPathAndEmitsLogoChanged();
    void importBadPathLeavesLogoUnchanged();
};

void TestSettingsViewModel::loadPopulatesPropertiesFromSettings()
{
    SettingsViewModel vm;
    vm.load();
    QCOMPARE(vm.schoolName(), QStringLiteral("Acme Library"));
    QCOMPARE(vm.schoolAddress(), QStringLiteral("123 Main St"));
    QCOMPARE(vm.adminName(), QStringLiteral("J. Rizal"));
    QCOMPARE(vm.adminPosition(), QStringLiteral("Head Librarian"));
    QCOMPARE(vm.openHour(), 8);
    QCOMPARE(vm.closeHour(), 17);
    QCOMPARE(vm.guestEnabled(), true);
}

void TestSettingsViewModel::loadStartsClean()
{
    SettingsViewModel vm;
    vm.load();
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::editingASettingMarksDirtyAndSignals()
{
    SettingsViewModel vm;
    vm.load();
    QSignalSpy dirtySpy(&vm, &SettingsViewModel::dirtyChanged);
    vm.setSchoolName(QStringLiteral("New Name"));
    QVERIFY(vm.dirty());
    QVERIFY(dirtySpy.count() >= 1);
}

void TestSettingsViewModel::resettingToSavedValueClearsDirty()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("New Name"));
    QVERIFY(vm.dirty());
    vm.setSchoolName(QStringLiteral("Acme Library"));   // back to the loaded value
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::saveePersistsAllFieldsAndEmitsSaved()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("Renamed Library"));
    vm.setOpenHour(7);
    QSignalSpy saved(&vm, &SettingsViewModel::saved);
    vm.save();
    QCOMPARE(saved.count(), 1);
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s.value(QStringLiteral("school/name")).toString(), QStringLiteral("Renamed Library"));
    QCOMPARE(s.value(QStringLiteral("library/openHour")).toInt(), 7);
}

void TestSettingsViewModel::saveMirrorsGuestFlagOntoKioskKey()
{
    SettingsViewModel vm;
    vm.load();
    vm.setGuestEnabled(false);
    vm.save();
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    // Both the controller's key AND the key the kiosk reads must be set.
    QCOMPARE(s.value(QStringLiteral("features/guestLogin")).toBool(), false);
    QCOMPARE(s.value(QStringLiteral("kiosk/guestEnabled")).toBool(), false);

    vm.setGuestEnabled(true);
    vm.save();
    QSettings s2(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s2.value(QStringLiteral("features/guestLogin")).toBool(), true);
    QCOMPARE(s2.value(QStringLiteral("kiosk/guestEnabled")).toBool(), true);
}

void TestSettingsViewModel::saveClearsDirty()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("X"));
    QVERIFY(vm.dirty());
    vm.save();
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::importLogoUpdatesPathAndEmitsLogoChanged()
{
    // Write a genuine 2x2 PNG into the temp dir so importImageFile accepts it.
    const QString src = m_tmp.path() + QStringLiteral("/src_logo.png");
    QImage img(2, 2, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(src, "PNG"));

    SettingsViewModel vm;
    vm.load();
    QSignalSpy logo(&vm, &SettingsViewModel::logoChanged);
    vm.importLogo(src);

    QVERIFY(logo.count() >= 1);
    QVERIFY(!vm.logoPath().isEmpty());
    QVERIFY(vm.hasLogo());
    QVERIFY(vm.logoUrl().isValid());
    QVERIFY(vm.dirty());   // an import is an unsaved change
}

void TestSettingsViewModel::importBadPathLeavesLogoUnchanged()
{
    SettingsViewModel vm;
    vm.load();
    const QString before = vm.logoPath();
    vm.importLogo(m_tmp.path() + QStringLiteral("/does_not_exist.png"));
    QCOMPARE(vm.logoPath(), before);   // unchanged
}

QTEST_MAIN(TestSettingsViewModel)
#include "tst_settingsviewmodel.moc"
