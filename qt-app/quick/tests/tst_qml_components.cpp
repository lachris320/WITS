#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The statically-linked witsquick module embeds its qmldir under
        // qrc:/qt/qml; make it importable from the .qml test files. Same
        // escape hatch as tst_qml_theme (Task 5): the literal "import LOAMS"
        // lives only in this test's .qml data file, which qmlimportscanner
        // never sees, so the automatic static-plugin import never fires.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_components, Setup)
#include "tst_qml_components.moc"
