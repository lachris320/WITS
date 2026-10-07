#ifndef QUICKTESTSETUP_H
#define QUICKTESTSETUP_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include "AccessControlHub.h"
#include "PolicyNamFactory.h"

// Shared QUICK_TEST_MAIN_WITH_SETUP setup object for every generated QuickTest
// main: the tst_qml_* targets (registered via wits_add_qmltest in
// quick/CMakeLists.txt; each runs exactly one tst_qml_*.qml) and
// tst_quicktestmain_missingfile.
class QuickTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // S1a transport seam: QuickTest engines get their managers from the
        // same PolicyNamFactory as WITSQuick (quick/main.cpp), installed before
        // anything loads. Static: outlives every engine in the process. A
        // failed install leaves the engine unusable, so abort (qFatal fires in
        // Release too, unlike an assert).
        static PolicyNamFactory namFactory;
        if (!PolicyNamFactory::installOn(*engine, namFactory))
            qFatal("QuickTestSetup: transport seam could not be installed before QML load");
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
