#include <QtTest>
#include <QSignalSpy>
#include <QSet>
#include "AdminSession.h"
#include "Navigator.h"

class TestNavigator : public QObject
{
    Q_OBJECT
private slots:
    void init() { AdminSession::instance().clear(); }
    void cleanup() { AdminSession::instance().clear(); }

    void defaultsToKiosk();
    void showAdminSwitchesAndSignals();
    void showKioskSwitchesBack();
    void defaultsToDashboardPage();
    void showAdminPageSwitchesAndSignals();
    void showAdminResetsPageToDashboard();
    void showAdminPageRoutesToSettings();
    void newEnumValuesAreDistinct();
    void showKioskClearsTheHeldAdminKey();
    void showKioskClearsEvenWhenAlreadyOnKiosk();
    void enteringAdminDoesNotClearTheHeldKey();
};

void TestNavigator::defaultsToKiosk()
{
    Navigator nav;
    QCOMPARE(nav.currentSurface(), Navigator::Kiosk);
}

void TestNavigator::showAdminSwitchesAndSignals()
{
    Navigator nav;
    QSignalSpy spy(&nav, &Navigator::currentSurfaceChanged);
    nav.showAdmin();
    QCOMPARE(nav.currentSurface(), Navigator::Admin);
    QCOMPARE(spy.count(), 1);
    nav.showAdmin();                 // idempotent — no redundant signal
    QCOMPARE(spy.count(), 1);
}

void TestNavigator::showKioskSwitchesBack()
{
    Navigator nav;
    nav.showAdmin();
    nav.showKiosk();
    QCOMPARE(nav.currentSurface(), Navigator::Kiosk);
}

void TestNavigator::defaultsToDashboardPage()
{
    Navigator nav;
    QCOMPARE(nav.adminPage(), Navigator::Dashboard);
}

void TestNavigator::showAdminPageSwitchesAndSignals()
{
    Navigator nav;
    QSignalSpy spy(&nav, &Navigator::adminPageChanged);
    nav.showAdminPage(Navigator::VisitLogs);
    QCOMPARE(nav.adminPage(), Navigator::VisitLogs);
    QCOMPARE(spy.count(), 1);
    nav.showAdminPage(Navigator::VisitLogs);   // idempotent — no redundant signal
    QCOMPARE(spy.count(), 1);
}

// Re-entering admin must always land on Dashboard, even if a prior admin
// session left a different sub-page selected — the user wants a predictable
// Dashboard-first landing rather than resuming wherever they left off.
void TestNavigator::showAdminResetsPageToDashboard()
{
    Navigator nav;
    nav.showAdmin();
    nav.showAdminPage(Navigator::VisitLogs);
    nav.showKiosk();

    nav.showAdmin();

    QCOMPARE(nav.adminPage(), Navigator::Dashboard);
}

void TestNavigator::showAdminPageRoutesToSettings()
{
    Navigator nav;
    QSignalSpy spy(&nav, &Navigator::adminPageChanged);
    nav.showAdminPage(Navigator::Settings);
    QCOMPARE(nav.adminPage(), Navigator::Settings);
    QCOMPARE(spy.count(), 1);
    nav.showAdminPage(Navigator::Settings);   // idempotent — no redundant signal
    QCOMPARE(spy.count(), 1);
}

void TestNavigator::newEnumValuesAreDistinct()
{
    // The three new pages must be distinct values, and distinct from the
    // three originals, or the pageLoader switch (AdminScreen.qml) would alias
    // two sidebar items onto one screen.
    QSet<int> seen{ Navigator::Dashboard, Navigator::Search, Navigator::VisitLogs,
                    Navigator::Database, Navigator::Reporting, Navigator::Settings };
    QCOMPARE(seen.size(), 6);
}

// Leaving the admin surface ends the admin session. Without this the plaintext
// admin key sits in the process image for the rest of an unattended kiosk's
// uptime (crash dumps, swap, debugger attach) — exactly what AdminSession's
// "RAM ONLY" contract exists to bound. Re-entering admin re-posts the key to
// admin_login.php, so nothing legitimate depends on it surviving.
void TestNavigator::showKioskClearsTheHeldAdminKey()
{
    Navigator nav;
    nav.showAdmin();
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));

    nav.showKiosk();

    QVERIFY(!AdminSession::instance().hasKey());
    QVERIFY(AdminSession::instance().key().isEmpty());
}

// setSurface() is deliberately idempotent, but the clear must NOT be: a
// redundant "back to kiosk" is still an explicit request to end the session,
// and hanging the wipe off the surface-changed edge would silently skip it.
void TestNavigator::showKioskClearsEvenWhenAlreadyOnKiosk()
{
    Navigator nav;                                   // starts on Kiosk
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));

    nav.showKiosk();

    QVERIFY(!AdminSession::instance().hasKey());
}

void TestNavigator::enteringAdminDoesNotClearTheHeldKey()
{
    Navigator nav;
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));

    nav.showAdmin();
    nav.showAdminPage(Navigator::Settings);

    QCOMPARE(AdminSession::instance().key(), QStringLiteral("SECRET-1"));
}

QTEST_MAIN(TestNavigator)
#include "tst_navigator.moc"
