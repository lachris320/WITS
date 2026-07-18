#include <QtTest>
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

QTEST_MAIN(TestSchoolInfoViewModel)
#include "tst_schoolinfoviewmodel.moc"
