#include <QtTest>
#include <QSignalSpy>
#include <QVariantMap>
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/eventbus.h"
#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/mockprovider.h"
#include "accesscontrol/healthmonitor.h"
#include "accesscontrol/accesscontrolservice.h"

using namespace AccessControl;

class TestAccessControlService : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void defaultsToDisabled();
    void enableStartsProviderAndPublishesConnected();
    void providerEventIsRepublishedOnBus();
    void disableStopsRepublishing();
    void degradedSchedulesReconnect();
    void reconnectTimerCancelledWhenConnected();
    void unexpectedDisconnectPublishesAndReconnects();
};

// Builds a factory whose "mock" creator stores the created instance in *out so
// the test can drive it after enable().
static AccessProviderFactory factoryCapturing(MockProvider **out)
{
    AccessProviderFactory f;
    f.registerProvider(MockProvider::defaultDescriptor(),
        [out](const ProviderDescriptor &d, const QVariantMap &, QObject *parent) -> IAccessProvider * {
            auto *p = new MockProvider(d, parent);
            *out = p;
            return p;
        });
    return f;
}

void TestAccessControlService::defaultsToDisabled()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    QSignalSpy busSpy(&bus, &EventBus::eventPublished);
    AccessControlService svc(&bus, &f);

    // Flag OFF by default means MORE than a bool: nothing is built and nothing
    // is emitted. Prove all three so "zero behaviour change" is actually tested.
    QVERIFY(!svc.isEnabled());
    QVERIFY(mock == nullptr);                 // factory creator was never called
    QCOMPARE(busSpy.count(), 0);              // no traffic on the bus
    QCOMPARE(svc.connectionState(), ConnectionState::Disconnected);
}

void TestAccessControlService::enableStartsProviderAndPublishesConnected()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    QSignalSpy enabledSpy(&svc, &AccessControlService::enabledChanged);
    QSignalSpy busSpy(&bus, &EventBus::eventPublished);

    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(svc.isEnabled());
    QCOMPARE(enabledSpy.count(), 1);
    QCOMPARE(svc.connectionState(), ConnectionState::Connected);
    // Reaching Connected republishes a ControllerConnected event on the bus.
    bool sawConnected = false;
    for (const auto &call : busSpy)
        if (qvariant_cast<AccessEvent>(call.at(0)).type == AccessEvent::Type::ControllerConnected)
            sawConnected = true;
    QVERIFY(sawConnected);
}

void TestAccessControlService::providerEventIsRepublishedOnBus()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);

    QSignalSpy busSpy(&bus, &EventBus::eventPublished);
    mock->simulateGranted(QStringLiteral("S-1"), QStringLiteral("gate-a"));
    QCOMPARE(busSpy.count(), 1);
    const auto ev = qvariant_cast<AccessEvent>(busSpy.at(0).at(0));
    QCOMPARE(ev.type, AccessEvent::Type::AccessGranted);
    // The service stamps the emitting provider's id even though the mock left it
    // empty — bus consumers always know the provenance.
    QCOMPARE(ev.providerId, QStringLiteral("mock"));
}

void TestAccessControlService::disableStopsRepublishing()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);
    MockProvider *captured = mock;   // keep a handle; service will drop its own

    svc.disable();
    QVERIFY(!svc.isEnabled());

    // The provider is torn down; nothing further reaches the bus.
    QSignalSpy busSpy(&bus, &EventBus::eventPublished);
    captured->simulateGranted(QStringLiteral("S-2"), QStringLiteral("gate-a"));
    QCOMPARE(busSpy.count(), 0);
}

void TestAccessControlService::degradedSchedulesReconnect()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.setReconnectBaseMs(5);   // tiny backoff so the test is fast
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);

    QSignalSpy stateSpy(&svc, &AccessControlService::connectionStateChanged);
    mock->simulateDisconnect();   // -> Degraded, which schedules a reconnect
    QCOMPARE(svc.connectionState(), ConnectionState::Degraded);
    // Backoff timer fires provider->start(), returning to Connected.
    QVERIFY(QTest::qWaitFor([&]() {
        return svc.connectionState() == ConnectionState::Connected;
    }, 1000));
    QVERIFY(svc.healthMonitor()->snapshot(QStringLiteral("mock")).retryCount >= 1);
}

void TestAccessControlService::reconnectTimerCancelledWhenConnected()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.setReconnectBaseMs(30000);   // == cap; far longer than the test, so the timer will NOT fire
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);

    mock->simulateDisconnect();               // -> Degraded, arms the reconnect timer
    QCOMPARE(svc.connectionState(), ConnectionState::Degraded);
    QVERIFY(svc.isReconnectPending());        // timer is armed (won't fire for 60s)

    mock->start();                            // provider recovers on its own -> Connected
    QCOMPARE(svc.connectionState(), ConnectionState::Connected);
    QVERIFY(!svc.isReconnectPending());       // reaching Connected CANCELLED the timer
}

void TestAccessControlService::unexpectedDisconnectPublishesAndReconnects()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.setReconnectBaseMs(5);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);

    QSignalSpy busSpy(&bus, &EventBus::eventPublished);
    mock->stop();   // an UNEXPECTED drop while enabled (the service did not ask for it)
    QCOMPARE(svc.connectionState(), ConnectionState::Disconnected);
    bool sawDisconnected = false;
    for (const auto &call : busSpy)
        if (qvariant_cast<AccessEvent>(call.at(0)).type == AccessEvent::Type::ControllerDisconnected)
            sawDisconnected = true;
    QVERIFY(sawDisconnected);
    // ...and the scheduled reconnect brings it back to Connected.
    QVERIFY(QTest::qWaitFor([&]() {
        return svc.connectionState() == ConnectionState::Connected;
    }, 1000));
}

QTEST_MAIN(TestAccessControlService)
#include "tst_accesscontrolservice.moc"
