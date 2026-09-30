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
        // Live-but-disabled singleton (never initialize() -> no polling), robust
        // against an inherited WITS_ACCESS_CONTROL.
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_kiosk, Setup)
#include "tst_qml_kiosk.moc"
