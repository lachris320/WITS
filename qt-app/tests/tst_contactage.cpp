#include <QtTest>
#include <QTimeZone>
#include "accesscontrol/contactage.h"

using AccessControl::formatContactAge;

class TestContactAge : public QObject
{
    Q_OBJECT
private slots:
    void monitoringOffWinsOverAnyTimestamp();
    void noContactYetWhenOnAndNoTimestamp();
    void secondsUnderAMinute();
    void minutesFromSixtySeconds();
    void ageIncreasesWithNowForSameContact();
    void clockSkewClampsToZero();

private:
    static QDateTime t0() { return QDateTime(QDate(2026, 9, 30), QTime(8, 0, 0), QTimeZone::UTC); }
};

void TestContactAge::monitoringOffWinsOverAnyTimestamp()
{
    QCOMPARE(formatContactAge(false, t0(), t0().addSecs(5)), QStringLiteral("Monitoring off"));
    QCOMPARE(formatContactAge(false, QDateTime(), t0()), QStringLiteral("Monitoring off"));
}

void TestContactAge::noContactYetWhenOnAndNoTimestamp()
{
    QCOMPARE(formatContactAge(true, QDateTime(), t0()), QStringLiteral("No contact yet"));
}

void TestContactAge::secondsUnderAMinute()
{
    QCOMPARE(formatContactAge(true, t0(), t0()), QStringLiteral("Last contact 0 s ago"));
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(3)), QStringLiteral("Last contact 3 s ago"));
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(59)), QStringLiteral("Last contact 59 s ago"));
}

void TestContactAge::minutesFromSixtySeconds()
{
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(60)), QStringLiteral("Last contact 1 min ago"));
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(150)), QStringLiteral("Last contact 2 min ago"));
}

void TestContactAge::ageIncreasesWithNowForSameContact()
{
    // Same lastContactAt, advancing "now" (no new events, e.g. an outage):
    // the age must keep climbing.
    const QDateTime last = t0();
    QCOMPARE(formatContactAge(true, last, last.addSecs(5)), QStringLiteral("Last contact 5 s ago"));
    QCOMPARE(formatContactAge(true, last, last.addSecs(20)), QStringLiteral("Last contact 20 s ago"));
    QCOMPARE(formatContactAge(true, last, last.addSecs(125)), QStringLiteral("Last contact 2 min ago"));
}

void TestContactAge::clockSkewClampsToZero()
{
    QCOMPARE(formatContactAge(true, t0().addSecs(10), t0()), QStringLiteral("Last contact 0 s ago"));
}

QTEST_APPLESS_MAIN(TestContactAge)
#include "tst_contactage.moc"
