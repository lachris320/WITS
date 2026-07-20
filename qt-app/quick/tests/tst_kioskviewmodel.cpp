#include <QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include "KioskViewModel.h"
#include "RecentLoginsModel.h"
#include "AdminSession.h"

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
};

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

QTEST_MAIN(TestKioskViewModel)
#include "tst_kioskviewmodel.moc"
