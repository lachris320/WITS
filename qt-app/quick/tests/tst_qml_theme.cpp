#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <QQmlContext>

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
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_theme, Setup)
#include "tst_qml_theme.moc"
