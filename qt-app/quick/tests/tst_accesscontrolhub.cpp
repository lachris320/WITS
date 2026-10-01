#include <QtTest>
#include <QSignalSpy>
#include <QTimeZone>
#include "AccessControlHub.h"
#include "appsettings.h"
#include "sequencednam.h"
#include "accesscontrol/accesstypes.h"

using namespace AccessControl;

class TestAccessControlHub : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void init()
    {
        AppSettings::isolateForTesting();       // fresh throwaway INI each test
        qunsetenv("WITS_ACCESS_CONTROL");
        AppSettings s; s.clear(); s.sync();
    }
    void toAccessEntry_knownStudent();
    void toAccessEntry_unknownStudent();
    void disabledByDefault_inert();
    void settingEnables();
    void envZeroDoesNotForceOff_settingWins();
    void envForceEnablesAndEmitsEntry();
    void connectionStateEnumOrderPinned();
    void setAccessEnabled_persistsTogglesAndIsIdempotent();
    void notLockedWhenEnabledBySetting();
    void enableLocked_refusesDisable();
    void failedStartupKeepsIntent();
    void connectionStateRelaysService();
    void lastContactAdvancesPerPoll();
    void stateOnlyHealthChangeDoesNotEmitLastContact();
    void contactAgeText_delegatesToPureFormatter();

private:
    static QByteArray entryPayload(qint64 latest, qint64 id)
    {
        return QStringLiteral(
            "{\"status\":\"success\",\"latest_id\":%1,\"entry\":{\"id\":%2,"
            "\"card\":\"C\",\"created_at\":\"2026-09-29 08:30:00\",\"reader\":0,"
            "\"student\":{\"name\":\"A\",\"photo_path\":\"uploads/a.jpg\"}}}")
            .arg(latest).arg(id).toUtf8();
    }
    static QByteArray emptyPayload(qint64 latest)
    {
        return QStringLiteral("{\"status\":\"success\",\"latest_id\":%1,\"entry\":null}")
            .arg(latest).toUtf8();
    }
};

void TestAccessControlHub::toAccessEntry_knownStudent()
{
    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;
    e.subject = QJsonObject{{"name", "A"}};
    e.correlationId = QStringLiteral("42");
    e.at = QDateTime(QDate(2026, 9, 29), QTime(8, 30), QTimeZone::UTC);
    const QVariantMap m = AccessControlHub::toAccessEntry(e);
    QCOMPARE(m.value("hasStudent").toBool(), true);
    QCOMPARE(m.value("student").toMap().value("name").toString(), QStringLiteral("A"));
    QCOMPARE(m.value("eventId").toString(), QStringLiteral("42"));
    QCOMPARE(m.value("at").toDateTime(), e.at);
}

void TestAccessControlHub::toAccessEntry_unknownStudent()
{
    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;   // empty subject == unresolved
    e.correlationId = QStringLiteral("7");
    const QVariantMap m = AccessControlHub::toAccessEntry(e);
    QCOMPARE(m.value("hasStudent").toBool(), false);
    QVERIFY(m.value("student").toMap().isEmpty());
}

void TestAccessControlHub::disabledByDefault_inert()
{
    SequencedNam nam;
    AccessControlHub hub(&nam);
    QSignalSpy spy(&hub, &AccessControlHub::entryObserved);
    hub.initialize();                       // accessControl/enabled defaults false
    QVERIFY(!hub.isAccessEnabled());
    QTest::qWait(300);
    QCOMPARE(nam.requestCount(), 0);        // no polling
    QCOMPARE(spy.count(), 0);
}

void TestAccessControlHub::settingEnables()
{
    { AppSettings s; s.setValue("accessControl/enabled", true); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());         // enabled by setting, no env
}

void TestAccessControlHub::envZeroDoesNotForceOff_settingWins()
{
    qputenv("WITS_ACCESS_CONTROL", "0");    // 0 is NOT a force-off
    { AppSettings s; s.setValue("accessControl/enabled", true); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());         // falls through to the (true) setting
}

void TestAccessControlHub::envForceEnablesAndEmitsEntry()
{
    qputenv("WITS_ACCESS_CONTROL", "1");
    { AppSettings s; s.setValue("accessControl/pollIntervalMs", 250); s.sync(); }
    SequencedNam nam;
    nam.enqueue(emptyPayload(3));           // baseline: cursor = 3
    nam.enqueue(entryPayload(4, 4));        // poll -> entry 4
    nam.enqueue(emptyPayload(4));           // drain end
    AccessControlHub hub(&nam);
    QSignalSpy spy(&hub, &AccessControlHub::entryObserved);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());
    QVERIFY(spy.wait(3000));
    const QVariantMap m = spy.at(0).at(0).toMap();
    QCOMPARE(m.value("hasStudent").toBool(), true);
    QCOMPARE(m.value("eventId").toString(), QStringLiteral("4"));
}

void TestAccessControlHub::connectionStateEnumOrderPinned()
{
    // QML maps these ints to labels (ConnectionState has no Q_ENUM).
    QCOMPARE(int(ConnectionState::Disconnected), 0);
    QCOMPARE(int(ConnectionState::Connecting), 1);
    QCOMPARE(int(ConnectionState::Connected), 2);
    QCOMPARE(int(ConnectionState::Degraded), 3);
    QCOMPARE(int(ConnectionState::Error), 4);
}

void TestAccessControlHub::setAccessEnabled_persistsTogglesAndIsIdempotent()
{
    SequencedNam nam;                        // unqueued requests: valid empty polls
    AccessControlHub hub(&nam);
    hub.initialize();                        // flag off: descriptor/config still retained
    QVERIFY(!hub.isAccessEnabled());
    QSignalSpy spy(&hub, &AccessControlHub::accessEnabledChanged);

    hub.setAccessEnabled(true);
    QVERIFY(hub.isAccessEnabled());
    QCOMPARE(spy.count(), 1);
    { AppSettings s; QCOMPARE(s.value("accessControl/enabled").toBool(), true); }
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() >= 1, 3000);   // service really started
    QVERIFY(nam.lastUrl.path().endsWith(QStringLiteral("turnstile_display.php")));

    hub.setAccessEnabled(true);              // idempotent: no churn, no re-persist signal
    QCOMPARE(spy.count(), 1);

    hub.setAccessEnabled(false);
    QVERIFY(!hub.isAccessEnabled());
    QCOMPARE(spy.count(), 2);
    { AppSettings s; QCOMPARE(s.value("accessControl/enabled").toBool(), false); }
    QCOMPARE(hub.connectionState(), int(ConnectionState::Disconnected));

    hub.setAccessEnabled(false);             // idempotent
    QCOMPARE(spy.count(), 2);
}

void TestAccessControlHub::notLockedWhenEnabledBySetting()
{
    { AppSettings s; s.setValue("accessControl/enabled", true); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());
    QVERIFY(!hub.isEnableLocked());          // only the env var locks
    hub.setAccessEnabled(false);             // so disabling is allowed
    QVERIFY(!hub.isAccessEnabled());
}

void TestAccessControlHub::enableLocked_refusesDisable()
{
    qputenv("WITS_ACCESS_CONTROL", "1");
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isEnableLocked());
    QVERIFY(hub.isAccessEnabled());
    QSignalSpy spy(&hub, &AccessControlHub::accessEnabledChanged);

    hub.setAccessEnabled(false);             // refused
    QVERIFY(hub.isAccessEnabled());
    QCOMPARE(spy.count(), 0);
    { AppSettings s; QVERIFY(!s.contains("accessControl/enabled")); }   // nothing persisted
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() >= 1, 3000);            // still monitoring
}

void TestAccessControlHub::failedStartupKeepsIntent()
{
    SequencedNam nam;
    for (int i = 0; i < 4; ++i)              // baseline + backoff retries all fail
        nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);
    AccessControlHub hub(&nam);
    hub.initialize();                        // flag off
    hub.setAccessEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Degraded), 3000);
    QVERIFY(hub.isAccessEnabled());          // intent NOT reverted by the failed connect
    { AppSettings s; QCOMPARE(s.value("accessControl/enabled").toBool(), true); }
}

void TestAccessControlHub::connectionStateRelaysService()
{
    { AppSettings s; s.setValue("accessControl/enabled", true);
      s.setValue("accessControl/pollIntervalMs", 250); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    QSignalSpy spy(&hub, &AccessControlHub::connectionStateChanged);
    hub.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Connected), 3000);
    QVERIFY(spy.count() >= 2);               // Connecting, then Connected
}

void TestAccessControlHub::lastContactAdvancesPerPoll()
{
    { AppSettings s; s.setValue("accessControl/enabled", true);
      s.setValue("accessControl/pollIntervalMs", 250); s.sync(); }
    SequencedNam nam;                        // every poll is a valid empty poll
    AccessControlHub hub(&nam);
    hub.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Connected), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(hub.lastContactAt().isValid(), 3000);
    const QDateTime first = hub.lastContactAt();
    QSignalSpy spy(&hub, &AccessControlHub::lastContactChanged);
    QTRY_VERIFY_WITH_TIMEOUT(hub.lastContactAt() > first, 3000);   // steady empty poll
    QVERIFY(spy.count() >= 1);
}

void TestAccessControlHub::stateOnlyHealthChangeDoesNotEmitLastContact()
{
    SequencedNam nam;
    for (int i = 0; i < 4; ++i)
        nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);
    AccessControlHub hub(&nam);
    hub.initialize();
    QSignalSpy contact(&hub, &AccessControlHub::lastContactChanged);
    QSignalSpy state(&hub, &AccessControlHub::connectionStateChanged);
    hub.setAccessEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Degraded), 3000);
    QVERIFY(state.count() >= 1);             // state-only HealthMonitor updates happened...
    QCOMPARE(contact.count(), 0);            // ...but lastCommTime never changed
    QVERIFY(!hub.lastContactAt().isValid());
}

void TestAccessControlHub::contactAgeText_delegatesToPureFormatter()
{
    AccessControlHub hub;                    // never initialized: stateless helper still works
    const QDateTime t(QDate(2026, 9, 30), QTime(8, 0, 0), QTimeZone::UTC);
    QCOMPARE(hub.contactAgeText(true, QVariant(t), QVariant(t.addSecs(5))),
             QStringLiteral("Last contact 5 s ago"));
    QCOMPARE(hub.contactAgeText(true, QVariant(), QVariant(t)),
             QStringLiteral("No contact yet"));
    QCOMPARE(hub.contactAgeText(false, QVariant(t), QVariant(t)),
             QStringLiteral("Monitoring off"));
}

QTEST_MAIN(TestAccessControlHub)
#include "tst_accesscontrolhub.moc"
