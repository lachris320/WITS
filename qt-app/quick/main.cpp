#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSGRendererInterface>
#include "brandtheme.h"

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

    // Use the "Basic" Controls style (not the native Windows style) so the
    // LButton/TextField `background`/`contentItem` customizations in the
    // component library actually apply — the native style ignores them and
    // emits a QWARN per customized control otherwise. Must precede the first
    // Controls type instantiation.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Cache-first branding (spec §6): apply the cached palette before the first
    // frame so there is no unbranded flash. Background re-sync lands in Phase 4.
    {
        QSettings brandingStore(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
        BrandTheme::setCurrent(BrandTheme::loadCachedConfig(brandingStore).palette);
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("LOAMS", "AppShell");
    if (engine.rootObjects().isEmpty())
        return -1;

    // Go-live parity (parity proof item 3.6): the legacy WITS.exe always calls
    // showFullScreen() unconditionally. Here it's opt-in via "--fullscreen" or
    // WITS_FULLSCREEN so the dev loop stays windowed (window controls, alt-tab)
    // by default; kiosk-hardware fullscreen becomes a launch-flag decision
    // instead of a code change before go-live.
    if (args.contains(QStringLiteral("--fullscreen"))
        || qEnvironmentVariableIsSet("WITS_FULLSCREEN")) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
            window->showFullScreen();
    }

    return app.exec();
}
