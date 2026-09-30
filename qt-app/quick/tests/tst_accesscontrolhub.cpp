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

QTEST_MAIN(TestAccessControlHub)
#include "tst_accesscontrolhub.moc"
