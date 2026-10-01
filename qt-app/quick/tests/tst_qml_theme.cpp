#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <QQmlContext>
#include "AccessControlHub.h"

class Setup : public QObject
{
    Q_OBJECT
public:
    Setup() = default;

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The statically-linked witsquick module embeds its qmldir under
        // qrc:/qt/qml; make it importable from the .qml test files.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // Every QuickTest target runs every tst_*.qml in QUICK_TEST_SOURCE_DIR,
        // so the AccessControl singleton must resolve here too. Live-but-
        // disabled hub (never initialize() -> no polling).
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_theme, Setup)
#include "tst_qml_theme.moc"
