#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_admin, Setup)
#include "tst_qml_admin.moc"
