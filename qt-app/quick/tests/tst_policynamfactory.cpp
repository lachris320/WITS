#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QThread>
#include <atomic>
#include <memory>
#include <vector>

#include "PolicyNamFactory.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

class TestPolicyNamFactory : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void createReturnsNewParentedPolicyNam();
    void createUsesPolicySnapshotAtCallTime();
    void createIsSafeFromManyThreads();
    void engineManagerComesFromFactory();
    void installOnFreshEngineSucceeds();
    void installRefusedAfterEngineCreatedAManager();
    void installRefusedAfterQmlLoaded();
    void installRefusedWhenAnotherFactoryInstalled();
};

void TestPolicyNamFactory::cleanup()
{
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
}

void TestPolicyNamFactory::createReturnsNewParentedPolicyNam()
{
    PolicyNamFactory factory;
    QObject owner;
    QNetworkAccessManager *a = factory.create(&owner);
    QNetworkAccessManager *b = factory.create(&owner);
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(a));
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(b));
    QVERIFY(a != b);                                   // a NEW manager per call (spec §2)
    QCOMPARE(a->parent(), &owner);                     // QML-owned via the given parent
    QCOMPARE(b->parent(), &owner);
    QCOMPARE(factory.createdCount(), 2);
}

void TestPolicyNamFactory::createUsesPolicySnapshotAtCallTime()
{
    PolicyNamFactory factory;
    QObject owner;
    const auto p1 = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(p1);
    auto *n1 = qobject_cast<PolicyEnforcingNam *>(factory.create(&owner));
    const auto p2 = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(p2);
    auto *n2 = qobject_cast<PolicyEnforcingNam *>(factory.create(&owner));
    QVERIFY(n1 && n2);
    QCOMPARE(n1->policy().get(), p1.get());
    QCOMPARE(n2->policy().get(), p2.get());
}

void TestPolicyNamFactory::createIsSafeFromManyThreads()
{
    // QQmlNetworkAccessManagerFactory::create() is called from the engine's
    // pixmap-reader / loader threads (spec §2).
    PolicyNamFactory factory;
    const auto policy = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(policy);
    constexpr int kThreads = 8;
    std::atomic<int> ok{0};
    std::vector<std::unique_ptr<QThread>> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back(QThread::create([&factory, &ok, policy] {
            QObject owner;   // lives on this worker; deletes the manager on this thread
            auto *nam = qobject_cast<PolicyEnforcingNam *>(factory.create(&owner));
            if (nam && nam->parent() == &owner && nam->thread() == QThread::currentThread()
                && nam->policy().get() == policy.get())
                ++ok;
        }));
    }
    for (auto &t : threads)
        t->start();
    for (auto &t : threads)
        QVERIFY(t->wait(10000));
    QCOMPARE(ok.load(), kThreads);
    QCOMPARE(factory.createdCount(), kThreads);
}

void TestPolicyNamFactory::engineManagerComesFromFactory()
{
    PolicyNamFactory factory;                          // declared first: outlives the engine
    QQmlEngine engine;
    QVERIFY(PolicyNamFactory::installOn(engine, factory));
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(engine.networkAccessManager()));
    QVERIFY(factory.createdCount() >= 1);
}

// --- Ordering rule (spec §2/§3 + S1a approval condition): the factory must be
// installed before any QML is loaded and before any manager/request exists. ---

void TestPolicyNamFactory::installOnFreshEngineSucceeds()
{
    PolicyNamFactory factory;
    QQmlEngine engine;
    QVERIFY(PolicyNamFactory::installOn(engine, factory));
    QCOMPARE(engine.networkAccessManagerFactory(),
             static_cast<QQmlNetworkAccessManagerFactory *>(&factory));
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(engine.networkAccessManager()));
    QCOMPARE(factory.createdCount(), 1);               // the eager common manager
}

void TestPolicyNamFactory::installRefusedAfterEngineCreatedAManager()
{
    PolicyNamFactory factory;
    QQmlEngine engine;
    QNetworkAccessManager *early = engine.networkAccessManager();   // default, unprotected
    QVERIFY(early);
    QVERIFY(!qobject_cast<PolicyEnforcingNam *>(early));
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("PolicyNamFactory::installOn")));
    QVERIFY(!PolicyNamFactory::installOn(engine, factory));
    QCOMPARE(factory.createdCount(), 0);
    // Qt keeps the plain cached manager: failure is not self-healing, so the
    // caller must abort initialization.
    QCOMPARE(engine.networkAccessManager(), early);
}

void TestPolicyNamFactory::installRefusedAfterQmlLoaded()
{
    PolicyNamFactory factory;
    QQmlApplicationEngine engine;
    engine.loadData(QByteArrayLiteral("import QtQml\nQtObject {}"));
    QCOMPARE(engine.rootObjects().size(), 1);
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("PolicyNamFactory::installOn")));
    QVERIFY(!PolicyNamFactory::installOn(engine, factory));
    QVERIFY(engine.networkAccessManagerFactory() == nullptr);
}

void TestPolicyNamFactory::installRefusedWhenAnotherFactoryInstalled()
{
    PolicyNamFactory first;
    PolicyNamFactory second;
    QQmlEngine engine;
    QVERIFY(PolicyNamFactory::installOn(engine, first));
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("PolicyNamFactory::installOn")));
    QVERIFY(!PolicyNamFactory::installOn(engine, second));
    QCOMPARE(engine.networkAccessManagerFactory(),
             static_cast<QQmlNetworkAccessManagerFactory *>(&first));
}

QTEST_MAIN(TestPolicyNamFactory)
#include "tst_policynamfactory.moc"
