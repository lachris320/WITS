#include <QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include "KioskViewModel.h"
#include "RecentLoginsModel.h"
#include "AdminSession.h"
#include "appsettings.h"

static QJsonObject student(const QString &name, const QString &course,
                           const QString &year, const QString &dept,
                           const QString &time)
{
    QJsonObject o;
    o.insert("name", name);
    o.insert("course", course);
    o.insert("year_level", year);
    o.insert("department", dept);
    o.insert("time_date", time);
    return o;
}

class TestKioskViewModel : public QObject
{
    Q_OBJECT
private slots:
    void applyStudentSetsCurrentAndBumpsStats();
    void applyStudentPrependsRowFresh();
    void modelCapsAtForty();
    void invalidRfidCodeSetsErrorStatusNoCrash();
    void requestGuestEmitsSignal();
    void adminLoginResponseCapturesHeldKey();
    void studentLoginResponseDoesNotCaptureKey();

    // Kiosk BrandPanel logo — same contract SchoolInfoViewModel already
    // implements for the admin sidebar (see tst_schoolinfoviewmodel.cpp).
    void logoAbsentWhenUnset();
    void logoExposedWhenFileExists();
    void logoAbsentWhenPathRotted();

    // reload(): the kiosk equivalent of SchoolInfoViewModel::reload(). The
    // admin Settings screen writes school/* mid-session, so the kiosk must be
    // able to re-read them without being reconstructed.
    void reloadPicksUpChangedSchoolInfo();
    void reloadIsQuietWhenNothingChanged();
    void reloadClearsRemovedLogo();

private:
    // AppSettings is process-isolated onto a throwaway INI by
    // settingsisolation.cpp (linked into every wits_add_qttest target), but
    // that store is shared by every test in this binary — so each school-info
    // test clears the keys it owns rather than inheriting a neighbour's value.
    void clearSchoolInfo();
    // Creates a real (non-image) file on disk so QFileInfo::exists() passes —
    // the VM only stats the path, it never decodes the bytes.
    QString writeLogoFile(const QString &fileName);
    QTemporaryDir m_dir;
};

void TestKioskViewModel::clearSchoolInfo()
{
    QVERIFY(AppSettings::isIsolatedForTesting());
    AppSettings s;
    s.remove(QStringLiteral("school/logoPath"));
    s.remove(QStringLiteral("school/name"));
    s.sync();
}

QString TestKioskViewModel::writeLogoFile(const QString &fileName)
{
    const QString path = m_dir.path() + QLatin1Char('/') + fileName;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write("not a real jpeg, just needs to exist on disk");
    f.close();
    return path;
}

void TestKioskViewModel::applyStudentSetsCurrentAndBumpsStats()
{
    KioskViewModel vm;
    QSignalSpy cur(&vm, &KioskViewModel::currentChanged);
    QSignalSpy stat(&vm, &KioskViewModel::statsChanged);

    vm.applyStudentLogin(student("Maria Santos", "BSCE", "3rd Year",
                                 "Civil Engineering", "8:04 AM"));

    QVERIFY(vm.hasStudent());
    QCOMPARE(vm.currentFullName(), QStringLiteral("Maria Santos"));
    QCOMPARE(vm.currentName(), QStringLiteral("Maria"));       // first name only
    QCOMPARE(vm.visitorsToday(), 1);
    QCOMPARE(vm.visitorsThisHour(), 1);
    QVERIFY(cur.count() >= 1);
    QVERIFY(stat.count() >= 1);
}

void TestKioskViewModel::applyStudentPrependsRowFresh()
{
    KioskViewModel vm;
    vm.applyStudentLogin(student("Maria Santos", "BSCE", "3rd Year", "CE", "8:04 AM"));
    vm.applyStudentLogin(student("Jose Ramirez", "BSEE", "2nd Year", "EE", "8:06 AM"));

    RecentLoginsModel *m = vm.recentLogins();
    QCOMPARE(m->rowCount(), 2);
    // Newest is row 0 and is the only "fresh" row.
    QCOMPARE(m->data(m->index(0), RecentLoginsModel::NameRole).toString(),
             QStringLiteral("Jose Ramirez"));
    QCOMPARE(m->data(m->index(0), RecentLoginsModel::FreshRole).toBool(), true);
    QCOMPARE(m->data(m->index(1), RecentLoginsModel::FreshRole).toBool(), false);
    // Initials derived.
    QCOMPARE(m->data(m->index(0), RecentLoginsModel::InitialsRole).toString(),
             QStringLiteral("JR"));
}

void TestKioskViewModel::modelCapsAtForty()
{
    KioskViewModel vm;
    for (int i = 0; i < 45; ++i)
        vm.applyStudentLogin(student(QStringLiteral("S%1 Name").arg(i),
                                     "BSIT", "1st Year", "IT", "9:00 AM"));
    QCOMPARE(vm.recentLogins()->rowCount(), 40);
}

void TestKioskViewModel::invalidRfidCodeSetsErrorStatusNoCrash()
{
    KioskViewModel vm;
    QSignalSpy st(&vm, &KioskViewModel::statusChanged);
    vm.handleRfidScan(QStringLiteral("A;"));   // fails isValidRfidCode -> no POST
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(vm.statusSeverity(), QStringLiteral("Error"));
    QVERIFY(st.count() >= 1);
    QCOMPARE(vm.recentLogins()->rowCount(), 0);  // nothing logged
}

void TestKioskViewModel::requestGuestEmitsSignal()
{
    KioskViewModel vm;
    QSignalSpy g(&vm, &KioskViewModel::guestRequested);
    vm.requestGuest();
    QCOMPARE(g.count(), 1);
}

void TestKioskViewModel::adminLoginResponseCapturesHeldKey()
{
    AdminSession::instance().clear();
    KioskViewModel vm;
    QSignalSpy admin(&vm, &KioskViewModel::adminRequested);
    // Network-free seam: a parsed admin-success payload (status=success, NO
    // "student" object -> isAdmin), plus the key that was posted.
    vm.applyLoginResponse(R"({"status":"success"})", QStringLiteral("SUPERSECRET"));
    QCOMPARE(admin.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("SUPERSECRET"));
}

void TestKioskViewModel::studentLoginResponseDoesNotCaptureKey()
{
    AdminSession::instance().clear();
    KioskViewModel vm;
    vm.applyLoginResponse(R"({"status":"success","student":{"name":"Ana","course":"BSCE"}})",
                          QStringLiteral("ignored"));
    QVERIFY(!AdminSession::instance().hasKey());   // student branch never captures
}

void TestKioskViewModel::logoAbsentWhenUnset()
{
    clearSchoolInfo();

    KioskViewModel vm;
    QVERIFY(!vm.hasLogo());
    QVERIFY(vm.logoUrl().isEmpty());
}

void TestKioskViewModel::logoExposedWhenFileExists()
{
    QVERIFY(m_dir.isValid());
    const QString logoPath = writeLogoFile(QStringLiteral("synthetic-logo.jpg"));
    QVERIFY(!logoPath.isEmpty());

    clearSchoolInfo();
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/logoPath"), logoPath);
        s.sync();
    }

    KioskViewModel vm;
    QVERIFY(vm.hasLogo());
    QCOMPARE(vm.logoUrl(), QUrl::fromLocalFile(logoPath));
    QVERIFY(vm.logoUrl().isLocalFile());
}

// The graceful-degradation case: a configured path whose file is gone must
// never reach an Image element, so hasLogo is false AND logoUrl stays empty.
void TestKioskViewModel::logoAbsentWhenPathRotted()
{
    QVERIFY(m_dir.isValid());
    clearSchoolInfo();
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/logoPath"),
                   m_dir.path() + QStringLiteral("/this-file-does-not-exist.jpg"));
        s.sync();
    }

    KioskViewModel vm;
    QVERIFY(!vm.hasLogo());
    QVERIFY(vm.logoUrl().isEmpty());
}

// The core regression: an admin rename + logo import lands in QSettings while
// an already-constructed kiosk VM is alive. Before reload() existed the kiosk
// kept its constructor-time snapshot for the rest of the session.
void TestKioskViewModel::reloadPicksUpChangedSchoolInfo()
{
    QVERIFY(m_dir.isValid());
    clearSchoolInfo();

    KioskViewModel vm;
    QCOMPARE(vm.schoolName(), QString());
    QVERIFY(!vm.hasLogo());

    const QString logoPath = writeLogoFile(QStringLiteral("reloaded-logo.jpg"));
    QVERIFY(!logoPath.isEmpty());
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/name"), QStringLiteral("Reloaded High"));
        s.setValue(QStringLiteral("school/logoPath"), logoPath);
        s.sync();
    }

    QSignalSpy info(&vm, &KioskViewModel::schoolInfoChanged);
    vm.reload();

    QCOMPARE(info.count(), 1);
    QCOMPARE(vm.schoolName(), QStringLiteral("Reloaded High"));
    QVERIFY(vm.hasLogo());
    QCOMPARE(vm.logoUrl(), QUrl::fromLocalFile(logoPath));
}

// Signal-quiet contract (mirrors SchoolInfoViewModel::reload): the refresh hook
// fires on every return to the kiosk surface, so a no-op reload must not churn
// QML bindings.
void TestKioskViewModel::reloadIsQuietWhenNothingChanged()
{
    clearSchoolInfo();
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/name"), QStringLiteral("Steady State Academy"));
        s.sync();
    }

    KioskViewModel vm;
    QCOMPARE(vm.schoolName(), QStringLiteral("Steady State Academy"));

    QSignalSpy info(&vm, &KioskViewModel::schoolInfoChanged);
    vm.reload();
    vm.reload();
    QCOMPARE(info.count(), 0);          // nothing moved -> nothing emitted

    {
        AppSettings s;
        s.setValue(QStringLiteral("school/name"), QStringLiteral("Renamed Academy"));
        s.sync();
    }
    vm.reload();
    QCOMPARE(info.count(), 1);          // exactly one emission for one change
    QCOMPARE(vm.schoolName(), QStringLiteral("Renamed Academy"));
}

// Graceful degradation across a reload: a logo whose file is deleted (or whose
// path is cleared) must flip hasLogo back to false and empty logoUrl, so a
// rotted path can never reach an Image element after the refresh either.
void TestKioskViewModel::reloadClearsRemovedLogo()
{
    QVERIFY(m_dir.isValid());
    const QString logoPath = writeLogoFile(QStringLiteral("removable-logo.jpg"));
    QVERIFY(!logoPath.isEmpty());

    clearSchoolInfo();
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/logoPath"), logoPath);
        s.sync();
    }

    KioskViewModel vm;
    QVERIFY(vm.hasLogo());

    QSignalSpy info(&vm, &KioskViewModel::schoolInfoChanged);
    QVERIFY(QFile::remove(logoPath));   // file gone, setting still points at it
    vm.reload();

    QCOMPARE(info.count(), 1);
    QVERIFY(!vm.hasLogo());
    QVERIFY(vm.logoUrl().isEmpty());
}

QTEST_MAIN(TestKioskViewModel)
#include "tst_kioskviewmodel.moc"
