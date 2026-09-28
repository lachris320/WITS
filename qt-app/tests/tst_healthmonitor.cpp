#include <QtTest>
#include <QDateTime>
#include <QSignalSpy>
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/healthmonitor.h"

using namespace AccessControl;

class TestHealthMonitor : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void unknownProviderReturnsDefaultSnapshot();
    void recordStateUpdatesSnapshotAndEmits();
    void recordCommTracksLatencyAndTime();
    void recordCommTimeLeavesLatencyUnknown();
    void recordRetryIncrements();
};

void TestHealthMonitor::unknownProviderReturnsDefaultSnapshot()
{
    HealthMonitor m;
    const HealthSnapshot s = m.snapshot(QStringLiteral("nope"));
    QCOMPARE(s.state, ConnectionState::Disconnected);
    QCOMPARE(s.latencyMs, qint64(-1));
    QCOMPARE(s.retryCount, 0);
}

void TestHealthMonitor::recordStateUpdatesSnapshotAndEmits()
{
    HealthMonitor m;
    QSignalSpy spy(&m, &HealthMonitor::healthChanged);
    m.recordState(QStringLiteral("mock"), ConnectionState::Connected);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.snapshot(QStringLiteral("mock")).state, ConnectionState::Connected);
    QCOMPARE(qvariant_cast<HealthSnapshot>(spy.at(0).at(0)).providerId,
             QStringLiteral("mock"));
}

void TestHealthMonitor::recordCommTracksLatencyAndTime()
{
    HealthMonitor m;
    QSignalSpy spy(&m, &HealthMonitor::healthChanged);
    const QDateTime t = QDateTime::currentDateTimeUtc();
    m.recordComm(QStringLiteral("mock"), 42, t);
    QCOMPARE(spy.count(), 1);
    const HealthSnapshot s = m.snapshot(QStringLiteral("mock"));
    QCOMPARE(s.latencyMs, qint64(42));
    QCOMPARE(s.lastCommTime, t);
}

void TestHealthMonitor::recordCommTimeLeavesLatencyUnknown()
{
    HealthMonitor m;
    QSignalSpy spy(&m, &HealthMonitor::healthChanged);
    const QDateTime t = QDateTime::currentDateTimeUtc();
    m.recordCommTime(QStringLiteral("mock"), t);
    QCOMPARE(spy.count(), 1);
    const HealthSnapshot s = m.snapshot(QStringLiteral("mock"));
    QCOMPARE(s.lastCommTime, t);
    QCOMPARE(s.latencyMs, qint64(-1));   // latency stays unknown — never fabricated
}

void TestHealthMonitor::recordRetryIncrements()
{
    HealthMonitor m;
    QSignalSpy spy(&m, &HealthMonitor::healthChanged);
    m.recordRetry(QStringLiteral("mock"));
    m.recordRetry(QStringLiteral("mock"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(m.snapshot(QStringLiteral("mock")).retryCount, 2);
}

QTEST_MAIN(TestHealthMonitor)
#include "tst_healthmonitor.moc"
