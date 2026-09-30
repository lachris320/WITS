#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include "AccessControlHub.h"

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // Every QuickTest target runs every tst_*.qml in QUICK_TEST_SOURCE_DIR,
        // and AccessControlScreen/KioskScreen resolve the AccessControl
        // singleton. Live-but-disabled hub (never initialize() -> no polling).
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_admin, Setup)
#include "tst_qml_admin.moc"
