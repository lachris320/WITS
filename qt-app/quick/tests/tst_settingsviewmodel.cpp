#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QImage>
#include <QFile>
#include <QUrl>
#include "SettingsViewModel.h"
#include "AdminSession.h"

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
    void adminInfoSuccessEmitsSaved();
    void adminInfoErrorEmitsFailedWithMessage();
    void adminInfoAuthFailureEmitsAuthFailed();
    void keyChangeSuccessRefreshesSession();
    void keyChangeWrongOldKeyEmitsFailedAndKeepsSession();
    void departmentsParseFillsList();
    void departmentsErrorLeavesListEmpty();
    void serializeCsvQuotesSpecialCells();
    void resetVisitsEmptyDepartmentIsGuarded();
    void resetVisitsSuccessEmitsReset();
    void resetVisitsAuthFailureEmitsAuthFailed();
    void writeResetManifestContainsMetadataNotBackup();
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

void TestSettingsViewModel::adminInfoSuccessEmitsSaved()
{
    SettingsViewModel vm;
    QSignalSpy ok(&vm, &SettingsViewModel::adminInfoSaved);
    vm.applyAdminInfoResponse(R"({"status":"success","message":"Admin info updated successfully."})");
    QCOMPARE(ok.count(), 1);
}

void TestSettingsViewModel::adminInfoErrorEmitsFailedWithMessage()
{
    SettingsViewModel vm;
    QSignalSpy bad(&vm, &SettingsViewModel::adminInfoFailed);
    vm.applyAdminInfoResponse(R"({"status":"error","message":"Failed to update admin info."})");
    QCOMPARE(bad.count(), 1);
    QCOMPARE(bad.at(0).at(0).toString(), QStringLiteral("Failed to update admin info."));
}

void TestSettingsViewModel::adminInfoAuthFailureEmitsAuthFailed()
{
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyAdminInfoResponse(R"({"status":"error","message":"Invalid admin key"})");
    QCOMPARE(auth.count(), 1);
}

void TestSettingsViewModel::keyChangeSuccessRefreshesSession()
{
    AdminSession::instance().setKey(QStringLiteral("OLDKEY"));
    SettingsViewModel vm;
    QSignalSpy ok(&vm, &SettingsViewModel::keyChanged);
    vm.applyKeyChangeResponse(R"({"status":"success","message":"Password updated successfully."})",
                              QStringLiteral("NEWKEY"));
    QCOMPARE(ok.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("NEWKEY"));
    AdminSession::instance().clear();   // isolate: don't leak into sibling tests
}

void TestSettingsViewModel::keyChangeWrongOldKeyEmitsFailedAndKeepsSession()
{
    AdminSession::instance().setKey(QStringLiteral("OLDKEY"));
    SettingsViewModel vm;
    QSignalSpy bad(&vm, &SettingsViewModel::keyChangeFailed);
    vm.applyKeyChangeResponse(R"({"status":"error","message":"Old password is incorrect."})",
                              QStringLiteral("NEWKEY"));
    QCOMPARE(bad.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("OLDKEY"));   // unchanged
    AdminSession::instance().clear();   // isolate: don't leak into sibling tests
}

void TestSettingsViewModel::departmentsParseFillsList()
{
    SettingsViewModel vm;
    QSignalSpy spy(&vm, &SettingsViewModel::departmentsChanged);
    vm.applyDepartmentsResponse(R"({"status":"success","departments":["CE","IT","BA"]})");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(vm.departments(), (QStringList{"CE","IT","BA"}));
}

void TestSettingsViewModel::departmentsErrorLeavesListEmpty()
{
    SettingsViewModel vm;
    vm.applyDepartmentsResponse(R"({"status":"error","message":"No departments found"})");
    QVERIFY(vm.departments().isEmpty());
}

void TestSettingsViewModel::serializeCsvQuotesSpecialCells()
{
    const QStringList headers{ "Name", "Note" };
    const QList<QStringList> rows{
        { "Ana Cruz",       "ok" },
        { "O'Hara, Liam",   "has \"quote\" and, comma" },
    };
    const QString csv = SettingsViewModel::serializeCsv(headers, rows);
    const QString expected =
        "Name,Note\r\n"
        "Ana Cruz,ok\r\n"
        "\"O'Hara, Liam\",\"has \"\"quote\"\" and, comma\"\r\n";
    QCOMPARE(csv, expected);
}

void TestSettingsViewModel::resetVisitsEmptyDepartmentIsGuarded()
{
    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    QSignalSpy failed(&vm, &SettingsViewModel::resetFailed);
    vm.resetVisits(QString(), QStringLiteral("key"));   // empty dept -> no POST
    QCOMPARE(reset.count(), 0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(!vm.busy());                                // never went busy
}

void TestSettingsViewModel::resetVisitsSuccessEmitsReset()
{
    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    vm.applyResetVisitsResponse(R"({"status":"success","message":"Visit counts reset and visit history cleared for department: CE"})");
    QCOMPARE(reset.count(), 1);
}

void TestSettingsViewModel::resetVisitsAuthFailureEmitsAuthFailed()
{
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyResetVisitsResponse(R"({"status":"error","message":"Invalid admin key"})");
    QCOMPARE(auth.count(), 1);
}

void TestSettingsViewModel::writeResetManifestContainsMetadataNotBackup()
{
    SettingsViewModel vm;
    vm.load();
    vm.setProperty("adminName", QStringLiteral("Jane Librarian"));   // Operator
    const QString path = m_tmp.path() + QStringLiteral("/manifest.csv");
    QVERIFY(vm.writeResetManifest(QStringLiteral("CE"), QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csv = QString::fromUtf8(f.readAll());
    QVERIFY(csv.contains(QStringLiteral("Department")));
    QVERIFY(csv.contains(QStringLiteral("CE")));               // the reset department
    QVERIFY(csv.contains(QStringLiteral("Jane Librarian")));   // Operator
    QVERIFY(csv.contains(QStringLiteral("Reset requested")));
    QVERIFY(!csv.contains(QStringLiteral("Backup")));          // never labeled a backup
}

QTEST_MAIN(TestSettingsViewModel)
#include "tst_settingsviewmodel.moc"
