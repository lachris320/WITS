#include <QtTest>
#include <QSignalSpy>
#include "Navigator.h"

class TestNavigator : public QObject
{
    Q_OBJECT
private slots:
    void defaultsToKiosk();
    void showAdminSwitchesAndSignals();
    void showKioskSwitchesBack();
    void defaultsToDashboardPage();
    void showAdminPageSwitchesAndSignals();
    void showAdminResetsPageToDashboard();
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

QTEST_MAIN(TestNavigator)
#include "tst_navigator.moc"
