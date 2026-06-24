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
    // --- admin-page inner reflow ---
    void adminPageHasLayout_data();
    void adminPageHasLayout();
    void generalPageFramesHaveLayouts();
    void chartsPreviewExpands();
    void visitorTableExpands();
    void searchOverlayPreserved();
    void adminCriticalNamesResolve();
    void visualizationWidgetHasLayout();
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

static QWidget *findPage(QWidget *root, const char *name)
{
    return root->findChild<QWidget *>(QString::fromLatin1(name));
}

void TestResponsiveUi::adminPageHasLayout_data()
{
    QTest::addColumn<QString>("pageName");
    QTest::newRow("databasePage") << QStringLiteral("databasePage");
    QTest::newRow("reportingPage") << QStringLiteral("reportingPage");
    QTest::newRow("studentSearchPage") << QStringLiteral("studentSearchPage");
    QTest::newRow("visitorPage") << QStringLiteral("visitorPage");
}

void TestResponsiveUi::adminPageHasLayout()
{
    QFETCH(QString, pageName);
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *page = findPage(w.data(), pageName.toLatin1().constData());
    QVERIFY2(page, qPrintable(QString("%1 not found").arg(pageName)));
    QVERIFY2(page->layout() != nullptr,
             qPrintable(QString("%1 has no top-level layout").arg(pageName)));
}

void TestResponsiveUi::generalPageFramesHaveLayouts()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    for (const char *name : {"adminFrame", "securityFrame", "libraryFrame", "settingsFrame"}) {
        QWidget *f = w->findChild<QWidget *>(name);
        QVERIFY2(f, qPrintable(QString("frame %1 not found").arg(name)));
        QVERIFY2(f->layout() != nullptr,
                 qPrintable(QString("frame %1 has no layout manager").arg(name)));
        // Frames must also GROW to fill the page grid (spec per-frame pattern #6),
        // else their laid-out content never gets the extra space.
        QVERIFY2(f->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding,
                 qPrintable(QString("frame %1 should expand horizontally to fill the page").arg(name)));
    }
}

void TestResponsiveUi::chartsPreviewExpands()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *g = w->findChild<QWidget *>("chartsPreview");
    QVERIFY2(g, "chartsPreview group box not found");
    QCOMPARE(g->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(g->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
}

void TestResponsiveUi::visitorTableExpands()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *t = w->findChild<QWidget *>("visitorTable");
    QVERIFY2(t, "visitorTable not found");
    QCOMPARE(t->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QCOMPARE(t->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
}

void TestResponsiveUi::searchOverlayPreserved()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    QWidget *overlay = w->findChild<QWidget *>("searchOverlay");
    QVERIFY2(overlay, "searchOverlay must still exist after conversion");
    QWidget *page = findPage(w.data(), "studentSearchPage");
    QVERIFY2(page, "studentSearchPage not found");
    // Carve-out: overlay stays a free-floating sibling parented to the page,
    // never managed by the page layout (else it stops covering the page).
    QCOMPARE(overlay->parentWidget(), page);
    if (page->layout())
        QVERIFY2(page->layout()->indexOf(overlay) == -1,
                 "searchOverlay must NOT be added to the studentSearchPage layout");
}

void TestResponsiveUi::adminCriticalNamesResolve()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    // Names bound by adminwindow.cpp slots / per-page stylesheets. Dropping any
    // silently breaks the app. The exhaustive zero-deletions check is the git
    // name-set guard run during each conversion task.
    const char *names[] = {
        "generalPage", "databasePage", "reportingPage", "studentSearchPage", "visitorPage",
        "adminFrame", "securityFrame", "libraryFrame", "settingsFrame",
        "adminNameLineEdit", "adminPositionLineEdit", "schoolName", "address",
        "fontComboBox", "spinBox", "openHourSpinBox", "closeHourSpinBox",
        "lineEdit", "lineEdit_2", "updateBtn", "showOldBtn", "showNewBtn",
        "guestLoginCheckBox", "clearAttendanceCheckBox", "applyChangesBtn",
        "schoolLogoBrowseBtn", "posterBrowseBtn", "bookDropSystemCheckBox",
        "individualRegistrationBox", "bulkRegistrationBox", "bulkTable", "bulkProgressBar",
        "departmentComboBox", "deleteRecordsBtn", "resetCountBtn", "deactivateBtn",
        "filterDepartmentBox", "filterCourseBox", "durationTypeBox", "durationTypeWidget",
        "specificDay", "specificMonth", "semester", "widget_2",
        "dateEdit", "monthComboBox", "yearComboBox", "semesterComboBox",
        "semYearComboBox", "startDateEdit", "endDateEdit",
        "paletteComboBox", "chartTypeBox", "chartsPreview", "visualizatioWidget", "palettePreview",
        "generatePDFBtn", "generateExcelBtn", "statusLabel",
        "searchLineEdit", "searchBtn", "searchDepartmentFilter", "searchCourseFilter",
        "studentSearchTable", "selectAllBtn", "editStudentBtn", "deleteStudentBtn",
        "refreshSearchBtn", "cancelEditBtn", "searchOverlay",
        "visitorTable", "extractVisitorBtn", "visitorTotalLabel",
        "visitorSearchLineEdit", "visitorSearchBtn", "visitorDateFilterBox"
    };
    for (const char *n : names) {
        QVERIFY2(w->findChild<QWidget *>(n),
                 qPrintable(QString("critical objectName '%1' missing").arg(n)));
    }
}

void TestResponsiveUi::visualizationWidgetHasLayout()
{
    QScopedPointer<QWidget> w(loadUi("adminwindow.ui"));
    QVERIFY2(w, "failed to load adminwindow.ui");
    // The Visualization box's inner container held absolutely-positioned controls
    // (palette/chart-type labels + combos) that stayed glued top-left when the box
    // grew. It must carry its own layout so those controls reflow with the page.
    QWidget *vw = w->findChild<QWidget *>("visualizatioWidget");
    QVERIFY2(vw, "visualizatioWidget not found");
    QVERIFY2(vw->layout() != nullptr,
             "visualizatioWidget has no layout — its controls will not reflow");
}

QTEST_MAIN(TestResponsiveUi)
#include "tst_responsive_ui.moc"
