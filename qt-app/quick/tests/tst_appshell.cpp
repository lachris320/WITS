#include <QtTest>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtGlobal>
#include <vector>

#include "AccessControlHub.h"
#include "PolicyNamFactory.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

// Capture QML warnings so a load that "succeeds" but logs binding/type
// warnings still fails the test (premium-shell discipline: zero warnings).
static std::vector<QString> g_messages;
static void captureHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
        g_messages.push_back(msg);
}

class TestAppShell : public QObject
{
    Q_OBJECT
private slots:
    void loadsWithZeroWarnings();
};

void TestAppShell::loadsWithZeroWarnings()
{
    g_messages.clear();
    QtMessageHandler prev = qInstallMessageHandler(captureHandler);

    // Same composition order as quick/main.cpp (S1a transport seam).
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
    PolicyNamFactory namFactory;          // declared BEFORE engine -> outlives it

    AccessControlHub hub;                 // default settings, no env -> disabled
    AccessControlHub::setInstance(&hub);

    QQmlApplicationEngine engine;         // declared AFTER hub -> engine dies first
    const bool installed = PolicyNamFactory::installOn(engine, namFactory);   // before the load
    engine.loadFromModule("LOAMS", "AppShell");

    qInstallMessageHandler(prev);

    QVERIFY(installed);
    QCOMPARE(engine.networkAccessManagerFactory(),
             static_cast<QQmlNetworkAccessManagerFactory *>(&namFactory));
    QCOMPARE(engine.rootObjects().size(), 1);
    if (!g_messages.empty())
        qWarning() << "Unexpected QML messages:" << g_messages;
    QVERIFY(g_messages.empty());
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(engine.networkAccessManager()));
    QVERIFY(namFactory.createdCount() >= 1);
}

QTEST_MAIN(TestAppShell)
#include "tst_appshell.moc"
