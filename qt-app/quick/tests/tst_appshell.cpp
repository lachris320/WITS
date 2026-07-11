#include <QtTest>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtGlobal>
#include <vector>

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

    QQmlApplicationEngine engine;
    engine.loadFromModule("LOAMS", "AppShell");

    qInstallMessageHandler(prev);

    QCOMPARE(engine.rootObjects().size(), 1);
    if (!g_messages.empty())
        qWarning() << "Unexpected QML messages:" << g_messages;
    QVERIFY(g_messages.empty());
}

QTEST_MAIN(TestAppShell)
#include "tst_appshell.moc"
