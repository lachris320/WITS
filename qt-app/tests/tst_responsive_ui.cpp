#include <QtTest>
#include <QtUiTools/QUiLoader>
#include <QFile>
#include <QWidget>
#include <QLabel>
#include <QLayout>
#include <QSizePolicy>
#include <QString>

#ifndef WITS_UI_DIR
#define WITS_UI_DIR "."
#endif

class TestResponsiveUi : public QObject
{
    Q_OBJECT
private:
    QWidget *loadUi(const QString &fileName);
private slots:
    void mainWindowHasNoMaximumSizeCap();
    void mainWindowCentralWidgetHasLayout();
    void mainWindowSidebarWidthIsBounded();
    void mainWindowContentExpands();
    void mainWindowLoginRowHasLayout();
    void mainWindowHeaderHasColumns();
    void guestDialogHasLayout();
};

QWidget *TestResponsiveUi::loadUi(const QString &fileName)
{
    QUiLoader loader;
    QFile file(QString(WITS_UI_DIR) + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly))
        return nullptr;
    QWidget *w = loader.load(&file);
    file.close();
    return w;
}

void TestResponsiveUi::mainWindowHasNoMaximumSizeCap()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY2(w, "failed to load mainwindow.ui");
    QCOMPARE(w->maximumWidth(), QWIDGETSIZE_MAX);
    QCOMPARE(w->maximumHeight(), QWIDGETSIZE_MAX);
}

void TestResponsiveUi::mainWindowCentralWidgetHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY2(w, "failed to load mainwindow.ui");
    QWidget *central = w->findChild<QWidget *>("centralwidget");
    QVERIFY2(central, "centralwidget not found");
    QVERIFY2(central->layout() != nullptr, "centralwidget has no layout manager");
}

void TestResponsiveUi::mainWindowSidebarWidthIsBounded()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *frame = w->findChild<QWidget *>("frame");
    QVERIFY2(frame, "sidebar frame not found");
    QVERIFY2(frame->maximumWidth() <= 320, "sidebar should be width-bounded (<=320)");
    QVERIFY2(frame->minimumWidth() >= 240, "sidebar should have a sensible minimum width");
}

void TestResponsiveUi::mainWindowContentExpands()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *content = w->findChild<QWidget *>("frame_2");
    QVERIFY2(content, "frame_2 not found");
    QCOMPARE(content->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
}

void TestResponsiveUi::mainWindowLoginRowHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *row = w->findChild<QWidget *>("widget_2");
    QVERIFY2(row, "recent-login row widget_2 not found");
    QVERIFY2(row->layout() != nullptr, "login row has no layout manager");
}

void TestResponsiveUi::mainWindowHeaderHasColumns()
{
    QScopedPointer<QWidget> w(loadUi("mainwindow.ui"));
    QVERIFY(w);
    QWidget *header = w->findChild<QWidget *>("frame_3");
    QVERIFY2(header, "header frame_3 not found");
    QVERIFY2(header->layout() != nullptr, "header frame_3 has no layout manager");
    // The five column captions must live inside the header strip so they align
    // with the data rows and the strip does not collapse to zero height.
    for (const char *name : {"label_10", "label_6", "label_7", "label_8", "label_9"}) {
        QLabel *cap = header->findChild<QLabel *>(name);
        QVERIFY2(cap, qPrintable(QString("caption %1 not under frame_3").arg(name)));
    }
}

void TestResponsiveUi::guestDialogHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("guestwindow.ui"));
    QVERIFY2(w, "failed to load guestwindow.ui");
    QVERIFY2(w->layout() != nullptr, "guest dialog has no top-level layout manager");
    QCOMPARE(w->maximumWidth(), QWIDGETSIZE_MAX);
}

QTEST_MAIN(TestResponsiveUi)
#include "tst_responsive_ui.moc"
