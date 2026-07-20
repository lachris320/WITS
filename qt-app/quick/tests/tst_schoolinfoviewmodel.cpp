#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "SchoolInfoViewModel.h"

// Every test here uses the QSettings& constructor against an ini file under
// a QTemporaryDir — never the real "MyCompany"/"MyApp" registry scope. See
// SchoolInfoViewModel.h's constructor doc and tst_brandtheme.cpp for the
// established pattern this mirrors.
class TestSchoolInfoViewModel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void loadsNameAndAddressFromSettings();
    void realLogoFileExposesLoadableFileUrl();
    void missingLogoFileDegradesToNoLogo();
    void emptyLogoPathDegradesToNoLogo();
    void unsetKeysDegradeToEmptyNotNull();
    void reloadPicksUpARenamedSchoolAndNotifies();
    void reloadPicksUpANewlyImportedLogoAndNotifies();
    void reloadWithNoChangeEmitsNothing();

private:
    QTemporaryDir m_dir;
    QString iniPath(const QString &name) const { return m_dir.path() + QLatin1Char('/') + name; }
};

void TestSchoolInfoViewModel::init()
{
    QVERIFY(m_dir.isValid());
}

void TestSchoolInfoViewModel::cleanup()
{
}

void TestSchoolInfoViewModel::loadsNameAndAddressFromSettings()
{
    QSettings s(iniPath(QStringLiteral("name-address.ini")), QSettings::IniFormat);
    s.setValue(QStringLiteral("school/name"), QStringLiteral("Northwind Community Library"));
    s.setValue(QStringLiteral("school/address"), QStringLiteral("100 Fixture Lane"));
    s.sync();

    SchoolInfoViewModel vm(s);
    QCOMPARE(vm.schoolName(), QStringLiteral("Northwind Community Library"));
    QCOMPARE(vm.schoolAddress(), QStringLiteral("100 Fixture Lane"));
}

void TestSchoolInfoViewModel::realLogoFileExposesLoadableFileUrl()
{
    const QString logoPath = iniPath(QStringLiteral("synthetic-logo.jpg"));
    QFile f(logoPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not a real jpeg, just needs to exist on disk");
    f.close();

    QSettings s(iniPath(QStringLiteral("with-logo.ini")), QSettings::IniFormat);
    s.setValue(QStringLiteral("school/logoPath"), logoPath);
    s.sync();

    SchoolInfoViewModel vm(s);
    QVERIFY(vm.hasLogo());
    QCOMPARE(vm.logoUrl(), QUrl::fromLocalFile(logoPath));
    QVERIFY(vm.logoUrl().isLocalFile());
}

void TestSchoolInfoViewModel::missingLogoFileDegradesToNoLogo()
{
    QSettings s(iniPath(QStringLiteral("rotted-path.ini")), QSettings::IniFormat);
    s.setValue(QStringLiteral("school/logoPath"),
               iniPath(QStringLiteral("this-file-does-not-exist.jpg")));
    s.sync();

    SchoolInfoViewModel vm(s);
    QVERIFY(!vm.hasLogo());
    QVERIFY(vm.logoUrl().isEmpty());
}

void TestSchoolInfoViewModel::emptyLogoPathDegradesToNoLogo()
{
    QSettings s(iniPath(QStringLiteral("empty-path.ini")), QSettings::IniFormat);
    s.setValue(QStringLiteral("school/logoPath"), QStringLiteral(""));
    s.sync();

    SchoolInfoViewModel vm(s);
    QVERIFY(!vm.hasLogo());
    QVERIFY(vm.logoUrl().isEmpty());
}

void TestSchoolInfoViewModel::unsetKeysDegradeToEmptyNotNull()
{
    // Never-configured store (spec parallel to tst_brandtheme's
    // cacheLoadNeverBranded): no school/* keys written at all.
    QSettings s(iniPath(QStringLiteral("never-configured.ini")), QSettings::IniFormat);

    SchoolInfoViewModel vm(s);
    QVERIFY(vm.schoolName().isEmpty());
    QVERIFY(vm.schoolAddress().isEmpty());
    QVERIFY(!vm.hasLogo());
    QVERIFY(vm.logoUrl().isEmpty());
}

// Phase 4c is the first code that WRITES school/name and school/logoPath, so
// the sidebar brand (bound to this VM in AdminScreen.qml) has to be able to
// re-read them after a Settings save. Without reload() + a NOTIFY signal the
// palette re-colored live while the name/logo stayed stale until restart.
void TestSchoolInfoViewModel::reloadPicksUpARenamedSchoolAndNotifies()
{
    QSettings s(iniPath(QStringLiteral("rename.ini")), QSettings::IniFormat);
    s.setValue(QStringLiteral("school/name"), QStringLiteral("Old Name"));
    s.sync();

    SchoolInfoViewModel vm(s);
    QCOMPARE(vm.schoolName(), QStringLiteral("Old Name"));

    QSignalSpy spy(&vm, &SchoolInfoViewModel::schoolInfoChanged);
    s.setValue(QStringLiteral("school/name"), QStringLiteral("New Name"));
    s.setValue(QStringLiteral("school/address"), QStringLiteral("2 New Road"));
    s.sync();

    vm.reload();

    QCOMPARE(vm.schoolName(), QStringLiteral("New Name"));
    QCOMPARE(vm.schoolAddress(), QStringLiteral("2 New Road"));
    QCOMPARE(spy.count(), 1);
}

void TestSchoolInfoViewModel::reloadPicksUpANewlyImportedLogoAndNotifies()
{
    QSettings s(iniPath(QStringLiteral("logo-later.ini")), QSettings::IniFormat);
    s.sync();

    SchoolInfoViewModel vm(s);
    QVERIFY(!vm.hasLogo());

    const QString logoPath = iniPath(QStringLiteral("imported-logo.jpg"));
    QFile f(logoPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not a real jpeg, just needs to exist on disk");
    f.close();
    s.setValue(QStringLiteral("school/logoPath"), logoPath);
    s.sync();

    QSignalSpy spy(&vm, &SchoolInfoViewModel::schoolInfoChanged);
    vm.reload();

    QVERIFY(vm.hasLogo());
    QCOMPARE(vm.logoUrl(), QUrl::fromLocalFile(logoPath));
    QCOMPARE(spy.count(), 1);
}

// A save that changed nothing this VM reads must not churn the sidebar's
// bindings — and a signal-per-reload would be a needless re-layout on every
// unrelated Settings save (hours, guest toggle, admin info).
void TestSchoolInfoViewModel::reloadWithNoChangeEmitsNothing()
{
    QSettings s(iniPath(QStringLiteral("no-change.ini")), QSettings::IniFormat);
    s.setValue(QStringLiteral("school/name"), QStringLiteral("Same Name"));
    s.sync();

    SchoolInfoViewModel vm(s);
    QSignalSpy spy(&vm, &SchoolInfoViewModel::schoolInfoChanged);

    vm.reload();

    QCOMPARE(vm.schoolName(), QStringLiteral("Same Name"));
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestSchoolInfoViewModel)
#include "tst_schoolinfoviewmodel.moc"
