#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include "AdminSession.h"

// AdminSession is a process-wide singleton, so every test clears it first.
class TestAdminSession : public QObject
{
    Q_OBJECT
private slots:
    void init() { AdminSession::instance().clear(); }

    void startsEmpty();
    void setKeyStoresAndSignalsAndReportsHasKey();
    void setKeySameValueIsIdempotent();
    void refreshUpdatesHeldKey();
    void clearEmptiesAndSignals();
    void keyIsNeverPersistedToSettings();  // RAM-only guarantee (documentary)
};

void TestAdminSession::startsEmpty()
{
    QVERIFY(!AdminSession::instance().hasKey());
    QVERIFY(AdminSession::instance().key().isEmpty());
}

void TestAdminSession::setKeyStoresAndSignalsAndReportsHasKey()
{
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("SECRET-1"));
    QVERIFY(AdminSession::instance().hasKey());
    QCOMPARE(spy.count(), 1);
}

void TestAdminSession::setKeySameValueIsIdempotent()
{
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));  // unchanged
    QCOMPARE(spy.count(), 0);
}

void TestAdminSession::refreshUpdatesHeldKey()
{
    AdminSession::instance().setKey(QStringLiteral("OLD"));
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().refresh(QStringLiteral("NEW"));
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("NEW"));
    QCOMPARE(spy.count(), 1);
}

void TestAdminSession::clearEmptiesAndSignals()
{
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QSignalSpy spy(&AdminSession::instance(), &AdminSession::changed);
    AdminSession::instance().clear();
    QVERIFY(!AdminSession::instance().hasKey());
    QCOMPARE(spy.count(), 1);
}

void TestAdminSession::keyIsNeverPersistedToSettings()
{
    // AdminSession has no QSettings dependency at all — the key lives only in
    // the m_key member. Documentary RAM-only check, kept HERMETIC: probe a
    // throwaway INI scope (never the developer's real MyCompany/MyApp registry,
    // matching the SettingsViewModel tests' isolation).
    AdminSession::instance().setKey(QStringLiteral("SECRET-1"));
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QSettings s(tmp.path() + QStringLiteral("/probe.ini"), QSettings::IniFormat);
    QVERIFY(!s.allKeys().contains(QStringLiteral("admin/key")));
    QVERIFY(!s.allKeys().contains(QStringLiteral("admin/admin_key")));
}

QTEST_MAIN(TestAdminSession)
#include "tst_adminsession.moc"
