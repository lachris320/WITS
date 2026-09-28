#include <QtTest>
#include <QJsonObject>
#include <QMetaType>
#include "accesscontrol/accesstypes.h"

using namespace AccessControl;

// A tiny sender so the test can exercise an actual QUEUED signal carrying an
// AccessEvent — the property the seam truly needs (an AccessEvent must survive
// a cross-thread/queued hop). A plain isValid() check would be tautological:
// Q_DECLARE_METATYPE alone makes QMetaType::fromType<T>() valid.
class TypeEmitter : public QObject
{
    Q_OBJECT
signals:
    void fire(const AccessControl::AccessEvent &event);
};

class TestAccessTypes : public QObject
{
    Q_OBJECT
private slots:
    void credentialHoldsRawAndKind();
    void decisionDefaultsToError();
    void entryObservedIsDistinctFromGranted();
    void providerDescriptorCarriesConfigSchema();
    void accessEventSurvivesQueuedConnection();
};

void TestAccessTypes::credentialHoldsRawAndKind()
{
    Credential c;
    c.kind = CredentialKind::Qr;
    c.raw = QStringLiteral("TOKEN-123");
    c.gateId = QStringLiteral("gate-a");
    QCOMPARE(c.kind, CredentialKind::Qr);
    QCOMPARE(c.raw, QStringLiteral("TOKEN-123"));
    QCOMPARE(c.gateId, QStringLiteral("gate-a"));
}

void TestAccessTypes::decisionDefaultsToError()
{
    // A default-constructed decision must NOT read as Granted or Denied —
    // absence of a real answer is an Error, never an accidental allow.
    AccessDecision d;
    QCOMPARE(d.result, AccessDecision::Result::Error);
}

void TestAccessTypes::entryObservedIsDistinctFromGranted()
{
    QVERIFY(AccessEvent::Type::EntryObserved != AccessEvent::Type::AccessGranted);
}

void TestAccessTypes::providerDescriptorCarriesConfigSchema()
{
    ProviderDescriptor pd;
    pd.providerId = QStringLiteral("mock");
    pd.displayName = QStringLiteral("Mock Provider");
    pd.configSchema.append(ConfigFieldDescriptor{
        QStringLiteral("baseUrl"), QStringLiteral("Base URL"),
        QStringLiteral("string"), true });
    QCOMPARE(pd.configSchema.size(), 1);
    QCOMPARE(pd.configSchema.first().key, QStringLiteral("baseUrl"));
    QVERIFY(pd.configSchema.first().required);
}

void TestAccessTypes::accessEventSurvivesQueuedConnection()
{
    // registerMetaTypes() runs automatically at library load (Q_CONSTRUCTOR_FUNCTION
    // in accesstypes.cpp); call it again to prove idempotency.
    registerMetaTypes();
    registerMetaTypes();

    TypeEmitter em;
    AccessEvent received;
    bool got = false;
    // Qt::QueuedConnection forces the payload through the metatype system: if
    // AccessEvent were not a usable queued metatype this delivery would be
    // dropped and `got` would stay false.
    connect(&em, &TypeEmitter::fire, this,
            [&](const AccessEvent &e) { received = e; got = true; },
            Qt::QueuedConnection);

    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;
    e.gateId = QStringLiteral("gate-a");
    e.correlationId = QStringLiteral("corr-q");
    emit em.fire(e);

    QVERIFY(QTest::qWaitFor([&]() { return got; }, 1000));
    QCOMPARE(received.type, AccessEvent::Type::EntryObserved);
    QCOMPARE(received.gateId, QStringLiteral("gate-a"));
    QCOMPARE(received.correlationId, QStringLiteral("corr-q"));
}

QTEST_MAIN(TestAccessTypes)
#include "tst_accesstypes.moc"
