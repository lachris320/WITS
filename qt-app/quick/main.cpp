#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Deployment-hardware fallback (proposal §19/spec §10 Risk 4): the library
    // PC may lack a working OpenGL/RHI path. "--software" or
    // QT_QUICK_BACKEND=software forces the software rasterizer scene-graph
    // backend. Must precede the first QQuickWindow creation.
    const QStringList args = QCoreApplication::arguments();
    if (args.contains(QStringLiteral("--software"))
        || qEnvironmentVariable("QT_QUICK_BACKEND") == QLatin1String("software")) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("LOAMS", "AppShell");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
