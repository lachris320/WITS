#include <QtTest>
#include <QSignalSpy>
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/mockprovider.h"

using namespace AccessControl;

class TestMockProvider : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void startsIntoConnectedState();
    void startIsIdempotentWhenConnected();
    void stopReturnsToDisconnected();
    void simulateEmitsEachEventTypeIndependently();
    void simulateHardwareErrorEmitsSignal();
    void descriptorRoundTrips();
    void simulatePolledEmitsPolled();
};

void TestMockProvider::startsIntoConnectedState()
{
    MockProvider p(MockProvider::defaultDescriptor());
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    QCOMPARE(p.state(), ConnectionState::Disconnected);
    p.start();
    QCOMPARE(p.state(), ConnectionState::Connected);
    // Connecting then Connected — two transitions.
    QCOMPARE(states.count(), 2);
    QCOMPARE(qvariant_cast<ConnectionState>(states.at(1).at(0)),
             ConnectionState::Connected);
}

void TestMockProvider::startIsIdempotentWhenConnected()
{
    MockProvider p(MockProvider::defaultDescriptor());
    p.start();
    QCOMPARE(p.state(), ConnectionState::Connected);
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();   // already connected — must be a no-op
    QCOMPARE(states.count(), 0);
    QCOMPARE(p.state(), ConnectionState::Connected);
}

void TestMockProvider::stopReturnsToDisconnected()
{
    MockProvider p(MockProvider::defaultDescriptor());
    p.start();
    p.stop();
    QCOMPARE(p.state(), ConnectionState::Disconnected);
}

void TestMockProvider::simulateEmitsEachEventTypeIndependently()
{
    MockProvider p(MockProvider::defaultDescriptor());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.simulateGranted(QStringLiteral("S-1"), QStringLiteral("gate-a"));
    p.simulateDenied(QStringLiteral("S-2"), QStringLiteral("expired"));
    p.simulateError(QStringLiteral("reader offline"));
    p.simulateEntryObserved(QStringLiteral("gate-a"));
    QCOMPARE(events.count(), 4);
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(0).at(0)).type,
             AccessEvent::Type::AccessGranted);
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(1).at(0)).type,
             AccessEvent::Type::AccessDenied);
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(2).at(0)).type,
             AccessEvent::Type::AccessError);
    // EntryObserved is a DISTINCT type from AccessGranted.
    const auto entry = qvariant_cast<AccessEvent>(events.at(3).at(0));
    QCOMPARE(entry.type, AccessEvent::Type::EntryObserved);
    QCOMPARE(entry.gateId, QStringLiteral("gate-a"));
}

void TestMockProvider::simulateHardwareErrorEmitsSignal()
{
    MockProvider p(MockProvider::defaultDescriptor());
    QSignalSpy hw(&p, &IAccessProvider::hardwareError);
    p.simulateHardwareError(QStringLiteral("USB unplugged"));
    QCOMPARE(hw.count(), 1);
    QCOMPARE(hw.at(0).at(0).toString(), QStringLiteral("USB unplugged"));
}

void TestMockProvider::descriptorRoundTrips()
{
    const ProviderDescriptor d = MockProvider::defaultDescriptor();
    MockProvider p(d);
    QCOMPARE(p.descriptor().providerId, d.providerId);
    QCOMPARE(p.descriptor().providerId, QStringLiteral("mock"));
}

void TestMockProvider::simulatePolledEmitsPolled()
{
    MockProvider p(MockProvider::defaultDescriptor());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    const QDateTime at = QDateTime::currentDateTimeUtc();
    p.simulatePolled(at);
    QCOMPARE(polled.count(), 1);
    QCOMPARE(polled.at(0).at(0).toDateTime(), at);
}

QTEST_MAIN(TestMockProvider)
#include "tst_mockprovider.moc"
