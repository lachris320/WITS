#include <QtTest>
#include <QSignalSpy>
#include <QVariantMap>
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/eventbus.h"
#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/mockprovider.h"
#include "accesscontrol/healthmonitor.h"
#include "accesscontrol/accesscontrolservice.h"
#include <QNetworkReply>
#include <QUrl>
#include "accesscontrol/turnstileprovider.h"
#include "sequencednam.h"
#include <QTimeZone>

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
    void polledRecordsCommTimeForActiveProvider();
    void connectedTransitionAndPolledAreBothServiceRecorded();
    void polledFromTornDownProviderIsIgnored();
    void turnstileValidEmptyPollAdvancesFreshness();
    void turnstileInvalidPollDoesNotAdvanceFreshness_data();
    void turnstileInvalidPollDoesNotAdvanceFreshness();
    void turnstileLateResponseAfterDisableIsIgnored();
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

// Factory whose "turnstile" creator builds a REAL TurnstileProvider on the
// injected SequencedNam — multi-poll freshness end to end, no live network.
static AccessProviderFactory turnstileFactory(SequencedNam *nam)
{
    AccessProviderFactory f;
    f.registerProvider(TurnstileProvider::defaultDescriptor(),
        [nam](const ProviderDescriptor &, const QVariantMap &cfg, QObject *parent)
            -> IAccessProvider * {
            return new TurnstileProvider(nam, QUrl(QStringLiteral("http://localhost/loams_api/")),
                                         cfg, parent);
        });
    return f;
}

static QVariantMap fastCfg()
{
    return QVariantMap{{QStringLiteral("pollIntervalMs"), 250},
                       {QStringLiteral("gateId"), QStringLiteral("g1")}};
}

static QByteArray emptyPoll(qint64 latest)
{
    return QStringLiteral("{\"status\":\"success\",\"latest_id\":%1,\"entry\":null}")
        .arg(latest).toUtf8();
}

static QByteArray entryPoll(qint64 latest, qint64 id)
{
    return QStringLiteral(
        "{\"status\":\"success\",\"latest_id\":%1,\"entry\":{\"id\":%2,"
        "\"card\":\"CARD0001\",\"created_at\":\"2026-09-30 08:30:00\",\"reader\":0,"
        "\"student\":null}}").arg(latest).arg(id).toUtf8();
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

void TestAccessControlService::polledRecordsCommTimeForActiveProvider()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);

    const QDateTime at(QDate(2026, 9, 30), QTime(10, 0, 0), QTimeZone::UTC);
    QSignalSpy health(svc.healthMonitor(), &HealthMonitor::healthChanged);
    mock->simulatePolled(at);
    QCOMPARE(svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime, at);
    QVERIFY(health.count() >= 1);
}

void TestAccessControlService::connectedTransitionAndPolledAreBothServiceRecorded()
{
    // Two service-owned recording paths, both kept (see "Two recording paths"
    // above): (1) the Connected transition records the comm time on its own —
    // MockProvider never emits polled, yet lastCommTime is set; (2) a later
    // polled(at) advances it further. The provider touches neither.
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    const QDateTime beforeEnable = QDateTime::currentDateTimeUtc();
    svc.enable(MockProvider::defaultDescriptor(), {});       // Mock: Connecting -> Connected
    const QDateTime onConnected = svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime;
    QVERIFY(onConnected.isValid());                           // path (1), no polled involved
    QVERIFY(onConnected >= beforeEnable);

    const QDateTime later = onConnected.addSecs(30);
    mock->simulatePolled(later);                              // path (2)
    QCOMPARE(svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime, later);
}

void TestAccessControlService::polledFromTornDownProviderIsIgnored()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);
    MockProvider *captured = mock;
    const QDateTime before = svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime;

    svc.disable();                     // severs the provider's signals + deleteLater
    const QDateTime late(QDate(2026, 9, 30), QTime(11, 0, 0), QTimeZone::UTC);
    captured->simulatePolled(late);    // a late callback from the torn-down provider
    QCOMPARE(svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime, before);
}

void TestAccessControlService::turnstileValidEmptyPollAdvancesFreshness()
{
    SequencedNam nam;                  // unqueued requests answer a valid empty poll
    AccessProviderFactory f = turnstileFactory(&nam);
    EventBus bus;
    AccessControlService svc(&bus, &f);
    const QString id = QStringLiteral("turnstile");
    svc.enable(TurnstileProvider::defaultDescriptor(), fastCfg());
    QTRY_COMPARE_WITH_TIMEOUT(svc.connectionState(), ConnectionState::Connected, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(svc.healthMonitor()->snapshot(id).lastCommTime.isValid(), 3000);
    const QDateTime first = svc.healthMonitor()->snapshot(id).lastCommTime;
    // Steady-state empty polls never re-enter Connected, so ONLY the per-poll
    // polled -> recordCommTime wiring can move lastCommTime forward.
    QTRY_VERIFY_WITH_TIMEOUT(svc.healthMonitor()->snapshot(id).lastCommTime > first, 3000);
    svc.disable();
}

void TestAccessControlService::turnstileInvalidPollDoesNotAdvanceFreshness_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("error");   // QNetworkReply::NetworkError
    QTest::newRow("transport") << QByteArray() << int(QNetworkReply::HostNotFoundError);
    QTest::newRow("non-2xx")
        << QByteArrayLiteral("{\"status\":\"error\",\"message\":\"Internal server error\"}")
        << int(QNetworkReply::InternalServerError);
    QTest::newRow("malformed") << QByteArrayLiteral("not json") << int(QNetworkReply::NoError);
    QTest::newRow("non-advancing") << entryPoll(3, 3) << int(QNetworkReply::NoError);
}

void TestAccessControlService::turnstileInvalidPollDoesNotAdvanceFreshness()
{
    QFETCH(QByteArray, body);
    QFETCH(int, error);
    SequencedNam nam;
    nam.enqueue(emptyPoll(3));                                    // baseline: valid
    nam.enqueue(body, static_cast<QNetworkReply::NetworkError>(error));   // steady poll: invalid
    AccessProviderFactory f = turnstileFactory(&nam);
    EventBus bus;
    AccessControlService svc(&bus, &f);
    svc.setReconnectBaseMs(30000);    // no reconnect inside the observation window
    const QString id = QStringLiteral("turnstile");
    svc.enable(TurnstileProvider::defaultDescriptor(), fastCfg());

    // Capture freshness right after the valid baseline (the invalid steady poll
    // only fires one 250 ms interval later) ...
    QTRY_COMPARE_WITH_TIMEOUT(svc.connectionState(), ConnectionState::Connected, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(svc.healthMonitor()->snapshot(id).lastCommTime.isValid(), 3000);
    const QDateTime afterBaseline = svc.healthMonitor()->snapshot(id).lastCommTime;

    // ... then the invalid poll must degrade WITHOUT bumping it.
    QTRY_COMPARE_WITH_TIMEOUT(svc.connectionState(), ConnectionState::Degraded, 3000);
    QCOMPARE(svc.healthMonitor()->snapshot(id).lastCommTime, afterBaseline);
    svc.disable();
}

void TestAccessControlService::turnstileLateResponseAfterDisableIsIgnored()
{
    SequencedNam nam;
    nam.enqueue(emptyPoll(3));        // baseline: valid
    nam.enqueueStall();               // first steady poll: stays in flight
    AccessProviderFactory f = turnstileFactory(&nam);
    EventBus bus;
    AccessControlService svc(&bus, &f);
    const QString id = QStringLiteral("turnstile");
    svc.enable(TurnstileProvider::defaultDescriptor(), fastCfg());
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);     // stalled poll in flight
    const QDateTime before = svc.healthMonitor()->snapshot(id).lastCommTime;
    QVERIFY(before.isValid());

    svc.disable();                    // stop(): generation bump + abort -> late completion
    QTest::qWait(100);
    QCOMPARE(svc.healthMonitor()->snapshot(id).lastCommTime, before);
}

QTEST_MAIN(TestAccessControlService)
#include "tst_accesscontrolservice.moc"
