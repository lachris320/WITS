#ifndef QUICKTESTSETUP_H
#define QUICKTESTSETUP_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include "AccessControlHub.h"

// Shared QUICK_TEST_MAIN_WITH_SETUP setup object for every tst_qml_* target
// (registered via wits_add_qmltest in quick/CMakeLists.txt; each target runs
// exactly one tst_qml_*.qml).
class QuickTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The statically-linked witsquickmodule embeds its qmldir under
        // qrc:/qt/qml; make it importable from the .qml test files. The literal
        // "import LOAMS" lives only in the QUICK_TEST_MAIN .qml data file,
        // which qmlimportscanner never sees, so the automatic static-plugin
        // import never fires.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // AccessControlScreen/KioskScreen resolve the AccessControl singleton.
        // Live-but-disabled hub (never initialize() -> no polling), robust
        // against an inherited WITS_ACCESS_CONTROL.
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

#endif // QUICKTESTSETUP_H
