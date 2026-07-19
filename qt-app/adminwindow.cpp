#include "adminwindow.h"
#include "ui_adminwindow.h"
#include "apiconfig.h"
#include "busyindicator.h"
#include "theme.h"
#include "attachfilesdialog.h"
#include "brandtheme.h"
#include <QGraphicsDropShadowEffect>
#include <QGraphicsLayout>
#include <QStyle>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDate>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QMessageBox>
#include <QStringList>
#include <QGridLayout>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QApplication>
#include <QLabel>
#include <QJsonArray>
#include <QDebug>
#include <QPdfWriter>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QColor>
#include <QChart>
#include <QChartView>
#include <QCloseEvent>
#include <QPageSize>
#include <QPageLayout>
#include <QTableWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QIcon>

#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "xlsxcellrange.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace {

// Mirrors the shared contract documented on HttpForm::isServerAnswer()
// (qt-app/quick/HttpForm.h). Deliberately duplicated here rather than linked:
// this legacy Widgets app is the rollback path for the Qt Quick UI, so it must
// not take a link dependency on the very module it rolls back from.
//
// The useful question is not "did reply->error() get set" but "did the server
// answer at all". requireAdminAuth() answers a bad admin key with HTTP 401 plus
// {"status":"error","message":"..."} — Qt reports that as ContentAccessDenied,
// yet the body is exactly what the admin needs to see. Returns false only for a
// genuine transport failure (no status line, or a status with an empty body).
bool isServerAnswer(bool replyHadError, int httpStatus, const QByteArray &body)
{
    if (!replyHadError)
        return true;
    return httpStatus > 0 && !body.isEmpty();
}

} // namespace

void adminWindow::setActiveSidebar(QPushButton* activeBtn){
    QList<QPushButton*> buttons = {
        ui->generalBtn,
        ui->databaseBtn,
        ui->reportingBtn,
        ui->studentSearchBtn,
        ui->visitorBtn
    };
    for (auto btn : buttons){
        btn->setProperty("active", (btn == activeBtn));
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
    }
    if (m_headerTitle && activeBtn)
        m_headerTitle->setText(activeBtn->text().trimmed());
}

void adminWindow::buildHeaderBar()
{
    QWidget *central = ui->stackedWidget->parentWidget();           // centralwidget
    auto *mainRow = qobject_cast<QHBoxLayout *>(central->layout()); // horizontalLayout_main
    if (!mainRow) return;                                           // unexpected: bail safely
    const int stackIdx = mainRow->indexOf(ui->stackedWidget);
    if (stackIdx < 0) return;
    const int stackStretch = mainRow->stretch(stackIdx);

    // --- the header bar itself ---
    m_headerBar = new QFrame;
    m_headerBar->setObjectName("adminHeaderBar");
    m_headerBar->setFixedHeight(56);
    auto *row = new QHBoxLayout(m_headerBar);
    row->setContentsMargins(20, 0, 20, 0);

    m_headerTitle = new QLabel("General", m_headerBar);
    m_headerTitle->setObjectName("adminHeaderTitle");
    row->addWidget(m_headerTitle);
    row->addStretch(1);

    auto *avatar = new QLabel("A", m_headerBar);
    avatar->setObjectName("adminHeaderAvatar");
    avatar->setFixedSize(32, 32);
    avatar->setAlignment(Qt::AlignCenter);
    auto *who = new QLabel("Administrator", m_headerBar);
    who->setObjectName("adminHeaderWho");
    row->addWidget(avatar);
    row->addSpacing(8);
    row->addWidget(who);

    // --- vertical wrapper: header on top, the existing stack below ---
    auto *contentCol = new QWidget(central);
    contentCol->setObjectName("adminContentColumn");
    auto *col = new QVBoxLayout(contentCol);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    mainRow->removeWidget(ui->stackedWidget);   // detach from the HBox
    col->addWidget(m_headerBar);
    col->addWidget(ui->stackedWidget, 1);       // addWidget reparents the stack into contentCol
    mainRow->insertWidget(stackIdx, contentCol, stackStretch);
}

adminWindow::adminWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::adminWindow)
    , m_settingsController(new SettingsController(this))
{
    ui->setupUi(this);
    buildHeaderBar();

    ui->generalBtn->setIcon(QIcon(":/resources/icons/settings.svg"));
    ui->databaseBtn->setIcon(QIcon(":/resources/icons/database.svg"));
    ui->reportingBtn->setIcon(QIcon(":/resources/icons/bar-chart.svg"));
    ui->studentSearchBtn->setIcon(QIcon(":/resources/icons/search.svg"));
    ui->visitorBtn->setIcon(QIcon(":/resources/icons/map-pin.svg"));
    const QSize navIcon(20, 20);
    for (QPushButton *b : {ui->generalBtn, ui->databaseBtn, ui->reportingBtn,
                           ui->studentSearchBtn, ui->visitorBtn})
        b->setIconSize(navIcon);

    networkManager = new QNetworkAccessManager(this);
    m_visitorController = new VisitorController(networkManager, this);
    connect(m_visitorController, &VisitorController::visitorsLoaded,
            this, &adminWindow::populateVisitorTable);
    connect(m_visitorController, &VisitorController::fetchError,
            this, &adminWindow::onVisitorFetchError);
    m_importController = new ImportController(networkManager, this);
    connect(m_importController, &ImportController::duplicatesResolved,
            this, &adminWindow::onImportDuplicatesResolved);
    connect(m_importController, &ImportController::importError,
            this, &adminWindow::onImportError);
    connect(m_importController, &ImportController::uploadStarted,
            this, &adminWindow::onUploadStarted);
    connect(m_importController, &ImportController::uploadProgress,
            this, &adminWindow::onUploadProgress);
    connect(m_importController, &ImportController::uploadFinished,
            this, &adminWindow::onUploadFinished);
    connect(m_importController, &ImportController::uploadFailed,
            this, &adminWindow::onUploadFailed);
    m_studentController = new StudentController(networkManager, this);
    connect(m_studentController, &StudentController::searchFinished,
            this, &adminWindow::onSearchFinished);
    connect(m_studentController, &StudentController::searchFailed,
            this, &adminWindow::onSearchFailed);
    connect(m_studentController, &StudentController::bulkUpdateFinished,
            this, &adminWindow::onBulkUpdateFinished);
    connect(m_studentController, &StudentController::bulkUpdateFailed,
            this, &adminWindow::onBulkUpdateFailed);
    connect(m_studentController, &StudentController::deleteFinished,
            this, &adminWindow::onDeleteFinished);
    connect(m_studentController, &StudentController::deleteFailed,
            this, &adminWindow::onDeleteFailed);
    connect(m_studentController, &StudentController::departmentsLoaded,
            this, &adminWindow::onDepartmentsLoaded);
    connect(m_studentController, &StudentController::coursesLoaded,
            this, &adminWindow::onCoursesLoaded);

    m_reportController = new ReportController(networkManager, this);

    connect(m_reportController, &ReportController::departmentsLoaded,
            this, &adminWindow::onReportDepartmentsLoaded);
    connect(m_reportController, &ReportController::yearsLoaded,
            this, &adminWindow::onReportYearsLoaded);
    connect(m_reportController, &ReportController::coursesLoaded,
            this, &adminWindow::onReportCoursesLoaded);
    connect(m_reportController, &ReportController::reportDataReady,
            this, &adminWindow::onReportDataReady);
    connect(m_reportController, &ReportController::reportError,
            this, &adminWindow::onReportError);
    connect(m_reportController, &ReportController::previewDataReady,
            this, &adminWindow::onPreviewDataReady);
    connect(m_reportController, &ReportController::loadError,
            this, &adminWindow::onReportLoadError);

    chartsPreviewBoxLayout = ui->chartsPreviewBox;

    m_reportController->loadDepartments();

    ui->filterCourseBox->clear();
    ui->filterCourseBox->addItem("All Courses");
    ui->filterCourseBox->setCurrentIndex(0);


    connect(ui->durationTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &adminWindow::onDurationChanged);
    m_reportController->loadYears();
    loadDepartments();
    populateFilters();

    connect(this, &adminWindow::logoChanged, this, [=](const QString &path) {
        if (ui->schoolLabelLogo) {
            QPixmap pix(path);
            ui->schoolLabelLogo->setPixmap(
                pix.scaled(
                    ui->schoolLabelLogo->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                    )
                );
        }
    });

    updateChartsPreview(QJsonArray());  // Initialize with empty charts
    connectFilterSignals();

    connect(ui->applyChangesBtn, &QPushButton::clicked, this, &adminWindow::onApplyChangesBtnClicked);

    connect(ui->schoolName, &QLineEdit::textChanged, this, [=](const QString &) {
        changesMade = true;
    });
    connect(ui->address, &QLineEdit::textChanged, this, [=](const QString &) {
        changesMade = true;
    });
    connect(ui->adminNameLineEdit, &QLineEdit::textChanged, this, [=](const QString &) {
        changesMade = true;
    });
    connect(ui->adminPositionLineEdit, &QLineEdit::textChanged, this, [=](const QString &) {
        changesMade = true;
    });
    connect(ui->paletteComboBox, &QComboBox::currentTextChanged,
            this, &adminWindow::updatePalettePreview);
    updatePalettePreview(ui->paletteComboBox->currentText());


    // --- Duration widget connectors ---
    connect(ui->dateEdit, &QDateEdit::dateChanged, this, [=](const QDate &date){
        qDebug() << "Specific Day selected:" << date.toString("yyyy-MM-dd");
    });

    connect(ui->monthComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index){
                if (index >= 0) {
                    qDebug() << "Month selected:" << ui->monthComboBox->currentText();
                }
            });

    connect(ui->yearComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index){
                qDebug() << "Year (for month) changed:" << index;
            });

    connect(ui->semesterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index){
                if (index >= 0) {
                    qDebug() << "Semester selected:" << ui->semesterComboBox->currentText();
                }
            });

    connect(ui->semYearComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index){
                qDebug() << "Year (for semester) changed:" << index;
            });

    connect(ui->startDateEdit, &QDateEdit::dateChanged, this, [=](const QDate &date){
        qDebug() << "Custom Start Date:" << date.toString("yyyy-MM-dd");
    });

    connect(ui->endDateEdit, &QDateEdit::dateChanged, this, [=](const QDate &date){
        qDebug() << "Custom End Date:" << date.toString("yyyy-MM-dd");
    });


    connect(ui->attachFileBtn, &QPushButton::clicked, this, &adminWindow::onAttachFileBtnClicked);

    // --- Sidebar navigation ---
    connect(ui->generalBtn, &QPushButton::clicked, this, [=](){
        ui->stackedWidget->setCurrentWidget(ui->generalPage);
        setActiveSidebar(ui->generalBtn);
    });
    connect(ui->databaseBtn, &QPushButton::clicked, this, [=](){
        ui->stackedWidget->setCurrentWidget(ui->databasePage);
        setActiveSidebar(ui->databaseBtn);
    });
    connect(ui->reportingBtn, &QPushButton::clicked, this, [=](){
        ui->stackedWidget->setCurrentWidget(ui->reportingPage);
        setActiveSidebar(ui->reportingBtn);
    });
    connect(ui->studentSearchBtn, &QPushButton::clicked, this, [=](){
        ui->stackedWidget->setCurrentWidget(ui->studentSearchPage);
        setActiveSidebar(ui->studentSearchBtn);
    });
    connect(ui->visitorBtn, &QPushButton::clicked, this, [=](){
        ui->stackedWidget->setCurrentWidget(ui->visitorPage);
        setActiveSidebar(ui->visitorBtn);
    });
    connect(ui->fontComboBox, &QFontComboBox::currentFontChanged, this, [=](const QFont &){
        updatePreviewLabel();
    });
    connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int){
        updatePreviewLabel();
    });
    connect(ui->schoolName, &QLineEdit::textChanged, this, [=](const QString &){
        updatePreviewLabel();
    });
    connect(ui->address, &QLineEdit::textChanged, this, [=](const QString &){
        updatePreviewLabel();
    });
    connect(ui->schoolLogoBrowseBtn, &QPushButton::clicked,
            this, &adminWindow::onSchoolLogoBrowseBtnClicked);
    connect(ui->browsePhotoBtn, &QPushButton::clicked, this, &adminWindow::onBrowsePhotoBtnClicked);
    connect(ui->editStudentBtn, &QPushButton::clicked, this, &adminWindow::onEditStudentBtnClicked);
    connect(ui->deleteStudentBtn, &QPushButton::clicked, this, &adminWindow::onDeleteStudentBtnClicked);
    connect(ui->updateDatabaseBtn, &QPushButton::clicked, this, &adminWindow::onUpdateDatabaseBtnClicked);
    connect(ui->posterBrowseBtn, &QPushButton::clicked, this, &adminWindow::onPosterBrowseBtnClicked);
    connect(ui->generatePDFBtn, &QPushButton::clicked, this, &adminWindow::onGeneratePDFBtnClicked);
    connect(ui->generateExcelBtn, &QPushButton::clicked, this, &adminWindow::onGenerateExcelBtnClicked);
    connect(ui->printReportBtn, &QPushButton::clicked, this, &adminWindow::onPrintReportBtnClicked);


    // --- Layout spacing ---
    ui->verticalLayout->setSpacing(15);
    ui->verticalLayout->setContentsMargins(10,10,10,10);

    // --- Add shadows to frames ---
    auto addShadow = [](QWidget* w){
        auto *shadow = new QGraphicsDropShadowEffect;
        shadow->setBlurRadius(20);
        shadow->setXOffset(0);
        shadow->setYOffset(4);
        shadow->setColor(QColor(0,0,0,60));
        w->setGraphicsEffect(shadow);
    };

    // --- Setting UI Changes to Duration Widget ---
    ui->durationTypeBox->setCurrentIndex(-1);
    ui->monthComboBox->setCurrentIndex(-1);
    ui->semesterComboBox->setCurrentIndex(0);
    ui->genderComboBox->setCurrentIndex(-1);
    ui->statusComboBox->setCurrentIndex(-1);

    ui->bulkProgressBar->setVisible(false);
    ui->departmentComboBox->clear();
    ui->departmentComboBox->addItem("-- Select Department --");
    ui->departmentComboBox->setCurrentIndex(0); // make sure placeholder is selected at startup

    ui->semYearComboBox->setCurrentIndex(0);
    ui->monthComboBox->setCurrentIndex(-1);
    ui->yearComboBox->setCurrentIndex(-1);
    // Disable action buttons until a department is chosen
    ui->deactivateBtn->setEnabled(false);
    ui->resetCountBtn->setEnabled(false);
    ui->deleteRecordsBtn->setEnabled(false);

    // Enable buttons only when a valid department is selected
    connect(ui->departmentComboBox, &QComboBox::currentIndexChanged, this, [=](int index) {
        bool valid = (index > 0); // index 0 is the placeholder
        ui->deactivateBtn->setEnabled(valid);
        ui->resetCountBtn->setEnabled(valid);
        ui->deleteRecordsBtn->setEnabled(valid);
    });

    //connectors for visitorPage
    ui->visitorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->visitorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(ui->extractVisitorBtn, &QPushButton::clicked, this, &adminWindow::on_extractVisitorBtn_clicked);
    QStringList months = {"January", "February", "March", "April", "May", "June",
                          "July", "August", "September", "October", "November", "December"};
    ui->visitorMonthCombo->addItems(months);

    ui->visitorYearSpin->setRange(2020, QDate::currentDate().year());
    ui->visitorYearSpin->setValue(QDate::currentDate().year());

    // Default visibility
    ui->visitorMonthCombo->setVisible(false);
    ui->visitorYearSpin->setVisible(false);
    ui->visitorStartDate->setVisible(false);
    ui->visitorEndDate->setVisible(false);
    ui->visitorDateEdit->setVisible(false);
    ui->visitorDateFilterBox->setCurrentText("All");

    connect(ui->visitorDateFilterBox, &QComboBox::currentTextChanged, this, [=](const QString &text) {
        // Hide all first (avoid overlapping visibility issues)
        ui->visitorDateEdit->setVisible(false);
        ui->visitorMonthCombo->setVisible(false);
        ui->visitorYearSpin->setVisible(false);
        ui->visitorStartDate->setVisible(false);
        ui->visitorEndDate->setVisible(false);

        if (text == "Specific Day") {
            ui->visitorDateEdit->setVisible(true);
        }
        else if (text == "Month") {
            ui->visitorMonthCombo->setVisible(true);
            ui->visitorYearSpin->setVisible(true);
        }
        else if (text == "Date Range") {
            ui->visitorStartDate->setVisible(true);
            ui->visitorEndDate->setVisible(true);
        }
    });

    connect(ui->visitorSearchBtn, &QPushButton::clicked, this, [=]() {
        m_visitorController->fetchVisitors(collectVisitorFilter());
    });

    connect(ui->visitorBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->visitorPage);
        setActiveSidebar(ui->visitorBtn);
        m_visitorController->fetchVisitors(VisitorFilter{}); // Auto-load all logs on open
    });

    // Connectors for studentSearchPage
    ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->searchLoadingBar->setVisible(false);
    ui->searchOverlay->setVisible(false);
    ui->selectAllBtn->setVisible(false);
    ui->deleteStudentBtn->setVisible(false);
    ui->cancelEditBtn->setVisible(false);
    ui->studentSearchTable->setColumnCount(9);
    ui->studentSearchTable->setHorizontalHeaderLabels(
        {"Select", "Code", "School ID", "Name", "Course", "Department", "Year Level", "Gender", "Status"});
    connect(ui->searchLineEdit, &QLineEdit::returnPressed, this, &adminWindow::onSearchBtnClicked);
    connect(ui->searchBtn, &QPushButton::clicked, this, &adminWindow::onSearchBtnClicked);
    connect(ui->studentSearchTable, &QTableWidget::itemChanged, this, &adminWindow::onStudentTableItemChanged);
    connect(ui->searchDepartmentFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &adminWindow::onDepartmentFilterChanged);
    connect(ui->selectAllBtn, &QPushButton::clicked, this, &adminWindow::on_selectAllBtn_clicked);
    connect(ui->cancelEditBtn, &QPushButton::clicked, this, &adminWindow::onCancelEditBtnClicked);

    // ✅ Create a URL with query parameter include_all=true
    QUrl deptUrl = ApiConfig::endpoint("get_departments.php");
    QUrlQuery query;
    query.addQueryItem("include_all", "true");
    deptUrl.setQuery(query);

    QNetworkRequest deptRequest(deptUrl);
    QNetworkReply *deptReply = networkManager->get(deptRequest);

    connect(deptReply, &QNetworkReply::finished, this, [=]() {
        if (deptReply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(deptReply->readAll());
            QJsonObject obj = doc.object();

            if (obj["status"].toString() == "success") {
                ui->searchDepartmentFilter->clear();
                ui->searchDepartmentFilter->addItem("Select Department");

                for (auto d : obj["departments"].toArray()) {
                    ui->searchDepartmentFilter->addItem(d.toString());
                }
            }
        } else {
            QMessageBox::warning(this, "Error", deptReply->errorString());
        }
        deptReply->deleteLater();
    });

    connect(ui->studentSearchTable, &QTableWidget::itemChanged, this, [=](QTableWidgetItem *item) {
        if (item->column() == 0) {
            bool allChecked = true;
            bool noneChecked = true;

            for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
                QTableWidgetItem *rowItem = ui->studentSearchTable->item(row, 0);
                if (rowItem) {
                    if (rowItem->checkState() == Qt::Unchecked)
                        allChecked = false;
                    else
                        noneChecked = false;
                }
            }
            // You can later update your "Select All" checkbox state here
        }
    });



    // --- Setting up Password Field ---
    connect(ui->lineEdit, &QLineEdit::textChanged, this, [=](const QString &text){
        if (!text.isEmpty()) {
            ui->lineEdit->setEchoMode(QLineEdit::Password);
        } else {
            ui->lineEdit->setEchoMode(QLineEdit::Normal);
        }
    });

    connect(ui->lineEdit_2, &QLineEdit::textChanged, this, [=](const QString &text){
        if (!text.isEmpty()) {
            ui->lineEdit_2->setEchoMode(QLineEdit::Password);
        } else {
            ui->lineEdit_2->setEchoMode(QLineEdit::Normal);
        }
    });
    ui->showNewBtn->setText("👁");
    ui->showOldBtn->setText("👁");
    connect(ui->showOldBtn, &QPushButton::clicked, this, [=]() {
        static bool show = false;
        show = !show;
        ui->lineEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });

    connect(ui->filterDepartmentBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &adminWindow::onFilterDepartmentBoxCurrentIndexChanged);

    connect(ui->showNewBtn, &QPushButton::clicked, this, [=]() {
        static bool show = false;
        show = !show;
        ui->lineEdit_2->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });
    // Button Connectors for Starting New Semester
    connect(ui->deactivateBtn, &QPushButton::clicked, this, [=]() {
        QString dept = ui->departmentComboBox->currentText();
        if (dept.isEmpty() || dept == "-- Select Department --") {
            QMessageBox::warning(this, "Error", "Please select a department.");
            return;
        }

        // ✅ Confirmation dialog
        QMessageBox::StandardButton confirm = QMessageBox::warning(
            this,
            "Confirm Deactivation",
            QString("Are you sure you want to deactivate ALL students in the '%1' department?\n\nThey will not be able to login until reactivated.")
                .arg(dept),
            QMessageBox::Yes | QMessageBox::No
            );

        if (confirm != QMessageBox::Yes) {
            return; // Cancel if admin chooses No
        }

        QUrl url = ApiConfig::endpoint("deactivate_department.php");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery postData;
        postData.addQueryItem("department", dept);

        QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Error", reply->errorString());
                reply->deleteLater();
                return;
            }

            QByteArray resp = reply->readAll();
            reply->deleteLater();

            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isObject() && doc["status"].toString() == "success") {
                QMessageBox::information(this, "Success", doc["message"].toString());
            } else {
                QMessageBox::warning(this, "Failed", doc["message"].toString());
            }
        });
    });

    connect(ui->resetCountBtn, &QPushButton::clicked, this, [=]() {
        QString dept = ui->departmentComboBox->currentText();
        if (dept.isEmpty() || dept == "-- Select Department --") {
            QMessageBox::warning(this, "Error", "Please select a department.");
            return;
        }

        // ✅ Confirmation dialog
        QMessageBox::StandardButton confirm = QMessageBox::warning(
            this,
            "Confirm Reset",
            QString("Are you sure you want to reset the visit count and delete all visit history for the '%1' department?\n\nThis action cannot be undone.")
                .arg(dept),
            QMessageBox::Yes | QMessageBox::No
            );

        if (confirm != QMessageBox::Yes) {
            return; // Cancel if admin chooses No
        }

        QUrl url = ApiConfig::endpoint("reset_visits.php");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery postData;
        postData.addQueryItem("department", dept);
        // reset_visits.php is guarded by requireAdminAuth; without the key it 401s.
        postData.addQueryItem("admin_key", m_adminKey);

        QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

        connect(reply, &QNetworkReply::finished, this, [=]() {
            const QByteArray resp = reply->readAll();
            const bool replyHadError = reply->error() != QNetworkReply::NoError;
            const int httpStatus = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString transportError = reply->errorString();
            reply->deleteLater();

            // A 401 from requireAdminAuth is the server answering, not a
            // transport failure: show its message, not "Content Access Denied".
            if (!isServerAnswer(replyHadError, httpStatus, resp)) {
                QMessageBox::critical(this, "Error", transportError);
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isObject() && doc["status"].toString() == "success") {
                QMessageBox::information(this, "Success", doc["message"].toString());
            } else {
                const QString message = doc.isObject() ? doc["message"].toString() : QString();
                QMessageBox::warning(this, "Failed",
                                     message.isEmpty()
                                         ? QStringLiteral("The server rejected the request.")
                                         : message);
            }
        });
    });

    connect(ui->deleteRecordsBtn, &QPushButton::clicked, this, [=]() {
        QString dept = ui->departmentComboBox->currentText();
        if (dept.isEmpty() || dept == "-- Select Department --") {
            QMessageBox::warning(this, "Error", "Please select a department.");
            return;
        }

        // ⚠️ Strong confirmation (double check)
        QMessageBox::StandardButton confirm = QMessageBox::critical(
            this,
            "Confirm Delete",
            QString("⚠️ WARNING: This will permanently DELETE ALL student records in the '%1' department.\n\n"
                    "This action cannot be undone.\n\nDo you want to continue?")
                .arg(dept),
            QMessageBox::Yes | QMessageBox::No
            );

        if (confirm != QMessageBox::Yes) {
            return; // Cancel if admin says No
        }

        QUrl url = ApiConfig::endpoint("delete_department.php");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery postData;
        postData.addQueryItem("department", dept);

        QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Error", reply->errorString());
                reply->deleteLater();
                return;
            }

            QByteArray resp = reply->readAll();
            reply->deleteLater();

            QJsonDocument doc = QJsonDocument::fromJson(resp);
            if (doc.isObject() && doc["status"].toString() == "success") {
                QMessageBox::information(this, "Success", doc["message"].toString());
            } else {
                QMessageBox::warning(this, "Failed", doc["message"].toString());
            }
        });
    });


    addShadow(ui->securityFrame);
    addShadow(ui->adminFrame);
    addShadow(ui->libraryFrame);
    addShadow(ui->settingsFrame);
    addShadow(ui->individualRegistrationBox);
    addShadow(ui->bulkRegistrationBox);
    addShadow(ui->chartsPreview);

    // Enable alternating row colors for bulkTable (the other two are already enabled)
    ui->bulkTable->setAlternatingRowColors(true);

    connect(ui->registerBtn, &QPushButton::clicked, this, [=]() {
        // Create multipart form data
        QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        // Lambda to add text fields
        auto addPart = [&](const QString &name, const QString &value) {
            QHttpPart part;
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"" + name + "\""));
            part.setBody(value.toUtf8());
            multiPart->append(part);
        };

        // Add all form fields
        addPart("code", ui->codeLineEdit->text());
        addPart("name", ui->nameLineEdit->text());
        addPart("school_id", ui->idLineEdit->text());
        addPart("year_level", ui->yrlvlLineEdit->text());
        addPart("course", ui->courseLineEdit->text());
        addPart("department", ui->departmentLineEdit->text());
        addPart("gender", ui->genderComboBox->currentText());
        addPart("status", ui->statusComboBox->currentText());

        // Add image file if selected
        if (!selectedPhotoPath.isEmpty()) {
            QHttpPart filePart;
            filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                               QVariant("form-data; name=\"photo\"; filename=\"" +
                                        QFileInfo(selectedPhotoPath).fileName() + "\""));

            QFile *file = new QFile(selectedPhotoPath);
            if (!file->open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, "Error", "Could not open photo file.");
                delete file;
                delete multiPart;
                return;
            }

            filePart.setBodyDevice(file);
            file->setParent(multiPart);  // Auto-delete file with multiPart
            multiPart->append(filePart);
        }

        // Setup network request
        QUrl url = ApiConfig::endpoint("register_student.php");
        QNetworkRequest request(url);
        // NOTE: Don't set ContentTypeHeader manually - multipart sets it automatically

        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        QNetworkReply *reply = manager->post(request, multiPart);
        multiPart->setParent(reply);  // Auto-delete multiPart with reply

        // Handle response
        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Error", reply->errorString());
                reply->deleteLater();
                return;
            }

            QByteArray response = reply->readAll();
            qDebug() << "Server response:" << response;
            reply->deleteLater();

            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            if (!jsonDoc.isObject()) {
                QMessageBox::warning(this, "Error", "Invalid server response.");
                return;
            }

            QJsonObject obj = jsonDoc.object();
            if (obj["status"].toString() == "success") {
                QMessageBox::information(this, "Success", obj["message"].toString());

                // Clear all fields
                ui->codeLineEdit->clear();
                ui->nameLineEdit->clear();
                ui->idLineEdit->clear();
                ui->yrlvlLineEdit->clear();
                ui->courseLineEdit->clear();
                ui->departmentLineEdit->clear();
                ui->genderComboBox->setCurrentIndex(-1);
                ui->statusComboBox->setCurrentIndex(-1);

                // Clear photo preview and path
                selectedPhotoPath.clear();
                //ui->studentPhotoLabel->clear();
                //ui->studentPhotoLabel->setText("No Photo");

            } else {
                QMessageBox::warning(this, "Failed", obj["message"].toString());
            }
        });
    });

    connect(ui->updateBtn, &QPushButton::clicked, this, [=]() {
        QUrl url = ApiConfig::endpoint("update_admin_key.php");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QUrlQuery postData;
        const QString newKey = ui->lineEdit_2->text();
        postData.addQueryItem("old_key", ui->lineEdit->text());
        postData.addQueryItem("new_key", newKey);

        QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

        connect(reply, &QNetworkReply::finished, this, [=]() {
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Error", reply->errorString());
                reply->deleteLater();
                return;
            }

            QByteArray response = reply->readAll();
            reply->deleteLater();

            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            if (!jsonDoc.isObject()) {
                QMessageBox::warning(this, "Error", "Invalid server response.");
                return;
            }

            QJsonObject obj = jsonDoc.object();
            if (obj["status"].toString() == "success") {
                QMessageBox::information(this, "Success", obj["message"].toString());
                // The held key just became stale server-side; refresh it so the
                // guarded POSTs above keep authenticating (Phase 4c).
                m_adminKey = newKey;
                ui->lineEdit->clear();
                ui->lineEdit_2->clear();
            } else {
                QMessageBox::warning(this, "Failed", obj["message"].toString());
            }
        });
    });

    bindSettingsSignals();
    m_currentSettings = m_settingsController->load();
    populateSettingsForm(m_currentSettings);
    changesMade = false;   // populateSettingsForm fires textChanged; reset the dirty flag
}

// --- CSV Bulk Registration ---

void adminWindow::onAttachFileBtnClicked()
{
    AttachFilesDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        selectedExcelPath = dialog.getExcelPath();
        selectedZipPath = dialog.getZipPath();

        if (selectedExcelPath.isEmpty()) {
            QMessageBox::warning(this, "Missing Data File", "You must select a student data file (Excel or CSV).");
            return;
        }

        // Preview file contents in your table
        if (selectedExcelPath.endsWith(".csv", Qt::CaseInsensitive)) {
            loadCSVtoTable(selectedExcelPath);
        } else if (selectedExcelPath.endsWith(".xlsx", Qt::CaseInsensitive)) {
            loadExcelToTable(selectedExcelPath);
        } else {
            QMessageBox::warning(this, "Invalid File", "Please select a CSV or Excel file.");
            return;
        }

        // Optionally show attached filenames in your UI
        QString summary = QString("Data: %1 | Photos: %2")
                              .arg(QFileInfo(selectedExcelPath).fileName())
                              .arg(selectedZipPath.isEmpty() ? "None" : QFileInfo(selectedZipPath).fileName());

        //if (ui->attachedFilesLabel)
        //    ui->attachedFilesLabel->setText(summary);
    }
}

void adminWindow::onUpdateDatabaseBtnClicked()
{
    if (selectedExcelPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Data File", "Please attach the student Excel/CSV file first.");
        return;
    }

    if (selectedZipPath.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "No Photo ZIP",
            "No ZIP file was selected. Continue uploading without photos?",
            QMessageBox::Yes | QMessageBox::No
            );
        if (reply == QMessageBox::No) return;
    }

    int rowCount = ui->bulkTable->rowCount();
    if (rowCount == 0) {
        QMessageBox::warning(this, "Error", "No data to upload.");
        return;
    }

    // helper lambda to extract cell safely — reads the LIVE table so any
    // hand-edit the user made after the preview load is picked up (bulkTable
    // is editable, unlike visitorTable — this is why school IDs are NOT
    // cached from the ParsedTable, only bulkHeaderIndex is).
    auto getCell = [&](int row, const QString &key, int fallbackIndex = -1) -> QString {
        int col = -1;
        if (bulkHeaderIndex.contains(key)) col = bulkHeaderIndex[key];
        else if (fallbackIndex >= 0 && fallbackIndex < ui->bulkTable->columnCount()) col = fallbackIndex;
        if (col < 0) return QString();
        QTableWidgetItem *it = ui->bulkTable->item(row, col);
        return it ? it->text().trimmed() : QString();
    };

    QStringList schoolIds;
    for (int i = 0; i < rowCount; i++) {
        QString sid = getCell(i, "school_id", 1);
        if (!sid.isEmpty())
            schoolIds << sid;
    }

    m_importController->checkDuplicates(schoolIds);
}

void adminWindow::onCancelUploadBtnClicked()
{
    cancelUpload = true;
    QMessageBox::information(this, "Cancelled", "Bulk upload has been cancelled.");
    ui->bulkTable->clearContents();
}

void adminWindow::onImportDuplicatesResolved(const QStringList &duplicates)
{
    if (duplicates.isEmpty()) {
        m_importController->uploadStudents(selectedExcelPath, selectedZipPath, QStringList{});
        return;
    }

    // Show only first 3 in the main message
    QString previewList = duplicates.mid(0, 3).join("\n");
    QString msg = "The following School IDs already exist:\n\n" + previewList;

    if (duplicates.size() > 3) {
        msg += "\n...and more.\n\nDo you want to skip them and continue?";
    } else {
        msg += "\n\nDo you want to skip them and continue?";
    }

    QMessageBox box(QMessageBox::Question,
                    "Duplicates Found",
                    msg,
                    QMessageBox::Yes | QMessageBox::No,
                    this);

    // Add "Show More" button if there are more than 3
    QPushButton *showMoreBtn = nullptr;
    if (duplicates.size() > 3) {
        showMoreBtn = box.addButton("Show More", QMessageBox::ActionRole);
    }

    box.exec();

    if (box.clickedButton() == showMoreBtn) {
        // Show full list in a new dialog
        QString fullMsg = "Full list of duplicate School IDs:\n\n" + duplicates.join("\n");
        QMessageBox::information(this, "All Duplicates", fullMsg);

        // Re-ask the original question after showing full list
        QMessageBox::StandardButton choice =
            QMessageBox::question(this, "Proceed?",
                                  "Do you want to skip duplicates and continue?",
                                  QMessageBox::Yes | QMessageBox::No);
        if (choice == QMessageBox::No) {
            QMessageBox::information(this, "Cancelled", "Bulk upload cancelled.");
            return;
        }
    } else if (box.standardButton(box.clickedButton()) == QMessageBox::No) {
        QMessageBox::information(this, "Cancelled", "Bulk upload cancelled.");
        return;
    }

    // Yes (either directly or via the Show More re-ask) — the entire
    // duplicates list becomes skipIds, matching legacy line 1300's guard
    // which always sends the full list.
    m_importController->uploadStudents(selectedExcelPath, selectedZipPath, duplicates);
}

void adminWindow::onImportError(const QString &title, const QString &message, ImportSeverity severity)
{
    if (severity == ImportSeverity::Critical)
        QMessageBox::critical(this, title, message);
    else
        QMessageBox::warning(this, title, message);
}

void adminWindow::onUploadStarted()
{
    cancelUpload = false;
    ui->cancelUploadBtn->setEnabled(true);
    ui->updateDatabaseBtn->setEnabled(false);

    ui->bulkProgressBar->setVisible(true);
    ui->bulkProgressBar->setMinimum(0);
    ui->bulkProgressBar->setMaximum(0); // Indeterminate progress during upload
    ui->bulkProgressBar->setValue(0);
}

void adminWindow::onUploadProgress(int percent)
{
    // Every emission re-asserts determinate mode, matching the legacy lambda
    // which called setMaximum(100) unconditionally inside uploadProgress
    // (adminwindow.cpp:1362) — cheap, idempotent, preserved exactly.
    ui->bulkProgressBar->setMaximum(100);
    ui->bulkProgressBar->setValue(percent);
}

void adminWindow::onUploadFinished(const UploadResult &result)
{
    ui->bulkProgressBar->setVisible(false);
    ui->updateDatabaseBtn->setEnabled(true);
    ui->cancelUploadBtn->setEnabled(false);

    if (result.plainText) {
        QMessageBox::information(this, "Upload Complete", result.rawText);
        return;
    }

    if (result.ok) {
        QMessageBox::information(this, "Upload Complete",
                                 QString("Bulk upload finished!\n\n"
                                         "Successfully inserted: %1\n"
                                         "Errors/Skipped: %2\n\n%3")
                                     .arg(result.successCount)
                                     .arg(result.errorCount)
                                     .arg(result.message));

        // Clear the table after successful upload — the ONLY outcome that
        // clears bulkTable/selectedExcelPath/selectedZipPath.
        ui->bulkTable->setRowCount(0);
        selectedExcelPath.clear();
        selectedZipPath.clear();
    } else {
        QMessageBox::warning(this, "Upload Issue", result.message);
    }
}

void adminWindow::onUploadFailed(const QString &message)
{
    ui->bulkProgressBar->setVisible(false);
    ui->updateDatabaseBtn->setEnabled(true);
    ui->cancelUploadBtn->setEnabled(false);

    QMessageBox::critical(this, "Upload Failed", message);
}

void adminWindow::loadExcelToTable(const QString &filePath)
{
    ExcelParseError err = ExcelParseError::None;
    const ParsedTable table = m_importController->parseExcel(filePath, &err);

    if (err == ExcelParseError::OpenFailed) {
        QMessageBox::warning(this, "Error", "Failed to open Excel file.");
        return;   // table left untouched, matching adminwindow.cpp:1387-1390
    }
    if (err == ExcelParseError::NoSheets) {
        QMessageBox::warning(this, "Error", "This workbook has no sheets.");
        return;   // table left untouched, matching adminwindow.cpp:1393-1396
    }
    if (err == ExcelParseError::EmptySheet) {
        QMessageBox::information(this, "Excel", "Sheet is empty.");
        ui->bulkTable->clear();
        ui->bulkTable->setRowCount(0);
        ui->bulkTable->setColumnCount(0);
        return;   // matches adminwindow.cpp:1401-1406
    }

    bulkHeaderIndex = table.headerIndex;

    // Excel population — port of loadExcelToTable lines 1449-1462 verbatim.
    ui->bulkTable->clear();
    ui->bulkTable->setRowCount(table.rows.size());
    ui->bulkTable->setColumnCount(table.headers.size());
    ui->bulkTable->setHorizontalHeaderLabels(table.headers);

    for (int r = 0; r < table.rows.size(); ++r) {
        const QStringList &row = table.rows.at(r);
        for (int c = 0; c < row.size(); ++c)
            ui->bulkTable->setItem(r, c, new QTableWidgetItem(row.at(c)));
    }

    ui->bulkTable->resizeColumnsToContents();
}

void adminWindow::loadCSVtoTable(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + filePath);
        return;
    }

    QTextStream in(&file);
    const QString rawText = in.readAll();
    file.close();

    // CSV clears the table up front — before any failure dialog — unlike
    // Excel's case-specific clear. This is why a failed CSV load clears the
    // table but a failed Excel OpenFailed/NoSheets does not.
    ui->bulkTable->clear();
    ui->bulkTable->setRowCount(0);

    if (rawText.isEmpty()) {
        QMessageBox::information(this, "CSV Preview", "File is empty.");
        return;
    }

    const ParsedTable table = m_importController->parseCsv(rawText);

    if (table.headers.isEmpty()) {
        QMessageBox::warning(this, "CSV Error", "CSV file has no headers.");
        return;
    }

    bulkHeaderIndex = table.headerIndex;

    // CSV population — port of loadCSVtoTable lines 1523-1541 verbatim,
    // including the "fill against stale column count, widen afterward" quirk.
    for (const QStringList &rowData : table.rows) {
        const int row = ui->bulkTable->rowCount();
        ui->bulkTable->insertRow(row);

        for (int j = 0; j < rowData.size() && j < ui->bulkTable->columnCount(); j++) {
            ui->bulkTable->setItem(row, j, new QTableWidgetItem(rowData.at(j)));
        }
    }

    ui->bulkTable->setColumnCount(qMax(ui->bulkTable->columnCount(), table.headers.size()));
    ui->bulkTable->setHorizontalHeaderLabels(table.headers);
    ui->bulkTable->resizeColumnsToContents();
}
void adminWindow::onDurationChanged(int index)
{
    ui->durationTypeWidget->setCurrentIndex(index);

    switch (index) {
    case 0: { // Specific Day
        ui->dateEdit->setDate(QDate::currentDate());
        break;
    }
    case 1: { // Month
        ui->monthComboBox->setCurrentIndex(-1);
        ui->yearComboBox->setCurrentText(QString::number(QDate::currentDate().year()));
        break;
    }
    case 2: { // Semester
        ui->semesterComboBox->setCurrentIndex(-1);
        int idx = ui->semYearComboBox->findText(QString::number(QDate::currentDate().year()));
        if (idx >= 0) ui->semYearComboBox->setCurrentIndex(idx);
        break;
    }
    case 3: { // Custom Period
        ui->startDateEdit->setDate(QDate::currentDate());
        ui->endDateEdit->setDate(QDate::currentDate());
        break;
    }
    }
}


void adminWindow::updatePreviewLabel() {
    // The adminWindow root stylesheet has a `QWidget { font-family; font-size; }`
    // rule that cascades to every child widget. In Qt a stylesheet font rule
    // outranks QWidget::setFont(), so setFont() on the preview label is silently
    // ignored. Setting the font via the label's OWN stylesheet outranks the
    // inherited QWidget rule, so the preview actually updates.
    const QFont previewFont = ui->fontComboBox->currentFont();
    ui->schoolText_previewLabel->setStyleSheet(
        QStringLiteral("font-family: '%1'; font-size: %2pt;")
            .arg(previewFont.family())
            .arg(ui->spinBox->value()));
    ui->schoolText_previewLabel->setText(ui->schoolName->text() + "\n" + ui->address->text());
}

adminWindow::~adminWindow()
{
    delete ui;
}

void adminWindow::bindSettingsSignals()
{
    connect(m_settingsController, &SettingsController::settingsSaved,
            this,                 &adminWindow::onSettingsSaved);
    connect(m_settingsController, &SettingsController::logoChanged,
            this,                 &adminWindow::onLogoChanged);
    // Branding engine: a newly imported logo regenerates the palette in
    // Auto mode (Manual mode skips regeneration — the v1 code hook).
    connect(m_settingsController, &SettingsController::logoChanged, this,
            [](const QString &destinationPath) {
        QSettings store(QLatin1String("MyCompany"), QLatin1String("MyApp"));
        BrandingConfig config = BrandTheme::loadCachedConfig(store);
        QString errorMsg;
        if (BrandTheme::regenerateFromLogo(config, destinationPath, &errorMsg)) {
            BrandTheme::setCurrent(config.palette);
            BrandTheme::saveCachedConfig(store, config);
        } else if (!errorMsg.isEmpty()) {
            qWarning() << "Branding regeneration skipped:" << errorMsg;
        }
    });
    connect(m_settingsController, &SettingsController::posterChanged,
            this,                 &adminWindow::onPosterChanged);
    connect(m_settingsController, &SettingsController::importError,
            this,                 &adminWindow::onSettingsImportError);
}

void adminWindow::populateSettingsForm(const SettingsData &d)
{
    ui->schoolName->setText(d.schoolName);
    ui->address->setText(d.schoolAddress);

    if (!d.fontFamily.isEmpty())
        ui->fontComboBox->setCurrentFont(QFont(d.fontFamily, d.fontSize));
    ui->spinBox->setValue(d.fontSize);

    if (!d.adminName.isEmpty())
        ui->adminNameLineEdit->setText(d.adminName);
    if (!d.adminPosition.isEmpty())
        ui->adminPositionLineEdit->setText(d.adminPosition);

    // QSettings stores 24 h; spinboxes display 12 h (1–12 AM / PM)
    const int openH12  = (d.libraryOpenHour  == 0)  ? 12 : d.libraryOpenHour;
    const int closeH12 = (d.libraryCloseHour == 12) ? 12 : d.libraryCloseHour - 12;
    ui->openHourSpinBox->setValue(openH12);
    ui->closeHourSpinBox->setValue(closeH12);

    ui->guestLoginCheckBox->setChecked(d.guestLoginEnabled);

    if (!d.logoPath.isEmpty() && QFile::exists(d.logoPath)) {
        QPixmap pix(d.logoPath);
        if (!pix.isNull())
            ui->schoolLabelLogo->setPixmap(
                pix.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

SettingsData adminWindow::collectSettingsForm()
{
    SettingsData d;
    d.schoolName    = ui->schoolName->text();
    d.schoolAddress = ui->address->text();
    d.fontFamily    = ui->fontComboBox->currentFont().family();
    d.fontSize      = ui->spinBox->value();
    d.adminName     = ui->adminNameLineEdit->text().trimmed();
    d.adminPosition = ui->adminPositionLineEdit->text().trimmed();

    // Spinboxes are 12 h (1–12 AM / PM) → convert to 24 h for storage
    const int openH12  = ui->openHourSpinBox->value();
    const int closeH12 = ui->closeHourSpinBox->value();
    d.libraryOpenHour  = (openH12  == 12) ? 0  : openH12;
    d.libraryCloseHour = (closeH12 == 12) ? 12 : closeH12 + 12;

    d.guestLoginEnabled = ui->guestLoginCheckBox->isChecked();

    // logoPath and posterPath are maintained by onLogoChanged/onPosterChanged
    d.logoPath   = m_currentSettings.logoPath;
    d.posterPath = m_currentSettings.posterPath;
    return d;
}

// --- Apply button handler ---
void adminWindow::onApplyChangesBtnClicked()
{
    m_currentSettings = collectSettingsForm();
    if (!m_settingsController->save(m_currentSettings)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not save settings. Please try again."));
        return;
    }

    updatePreviewLabel();
    changesMade = false;

    QString msg = tr("Changes applied successfully.");
    if (ui->clearAttendanceCheckBox->isChecked()) {
        emit clearAttendanceRequested();
        msg += tr("\n\nAttendance panel has been cleared.");
        ui->clearAttendanceCheckBox->setChecked(false);
    }

    QMessageBox::information(this, tr("Saved"), msg);
    this->close();
}

void adminWindow::onSettingsSaved(const SettingsData &data)
{
    QFont selectedFont(data.fontFamily, data.fontSize);
    emit schoolInfoUpdated(data.schoolName, data.schoolAddress, selectedFont);
    if (!data.logoPath.isEmpty() && QFile::exists(data.logoPath))
        emit logoChanged(data.logoPath);
    if (!data.posterPath.isEmpty() && QFile::exists(data.posterPath))
        emit posterChanged(data.posterPath);
    emit guestLoginToggled(data.guestLoginEnabled);
}



// --- closeEvent handler ---
void adminWindow::closeEvent(QCloseEvent *event) {
    if (changesMade) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(
            this,
            "Unsaved Changes",
            "You have unsaved changes.\nDo you want to save them before exiting?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
            );

        if (reply == QMessageBox::Save) {
            onApplyChangesBtnClicked();
            event->accept();
        } else if (reply == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}



void adminWindow::onSchoolLogoBrowseBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Logo"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;
    const QString dest = m_settingsController->importLogo(path);
    if (!dest.isEmpty())
        QMessageBox::information(this, tr("Changes Applied"),
                                 tr("School logo has been uploaded successfully!"));
}

void adminWindow::onLogoChanged(const QString &path)
{
    m_currentSettings.logoPath = path;
    ui->schLogo_previewLabel->setPixmap(
        QPixmap(path).scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    changesMade = true;
}

void adminWindow::onPosterBrowseBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Poster"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) return;
    const QString dest = m_settingsController->importPoster(path);
    if (!dest.isEmpty())
        QMessageBox::information(this, tr("Changes Applied"),
                                 tr("Poster has been updated successfully!"));
}

void adminWindow::onPosterChanged(const QString &path)
{
    m_currentSettings.posterPath = path;
    ui->posterPreviewLabel->setPixmap(
        QPixmap(path).scaled(
            ui->posterPreviewLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    changesMade = true;
}

void adminWindow::onSettingsImportError(const QString &message)
{
    QMessageBox::warning(this, tr("Import Error"), message);
}

void adminWindow::onClearAttendanceCheckBoxStateChanged(int state) {
    if (state == Qt::Checked) {
        QMessageBox::information(this, "Pending Action", "Attendance panel will be cleared once you press 'Apply Changes'.");
    }
}

void adminWindow::loadDepartments() {
    QUrl url = ApiConfig::endpoint("get_departments.php");
    QNetworkRequest request(url);

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray response = reply->readAll();
        reply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        if (!jsonDoc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid server response.");
            return;
        }

        QJsonObject obj = jsonDoc.object();
        if (obj["status"].toString() == "success") {
            ui->departmentComboBox->clear();
            ui->departmentComboBox->addItem("-- Select Department --");
            QJsonArray deptArray = obj["departments"].toArray();
            for (auto v : deptArray) {
                ui->departmentComboBox->addItem(v.toString());
            }
        } else {
            QMessageBox::warning(this, "Error", "Failed to load departments.");
        }
    });
}

ReportHeaderInfo adminWindow::collectHeaderInfo() const {
    QSettings settings("MyCompany", "MyApp");
    ReportHeaderInfo info;
    info.schoolName = settings.value("school/name", "Your School Name").toString();
    info.address    = settings.value("school/address", "Your Address").toString();
    info.logoPath   = settings.value("school/logoPath", "").toString();
    info.librarian  = settings.value("admin/name", "").toString();
    info.position   = settings.value("admin/position", "").toString();
    info.openHour   = settings.value("library/openHour", 7).toInt();
    info.closeHour  = settings.value("library/closeHour", 21).toInt();
    return info;
}

// Combo population (departments) — reproduces cpp:1842-1848.
void adminWindow::onReportDepartmentsLoaded(const QStringList &departments) {
    ui->filterDepartmentBox->clear();
    ui->filterDepartmentBox->addItem("-- Select Department --");
    for (const QString &d : departments)
        ui->filterDepartmentBox->addItem(d);
}

// Years -> both combos (reproduces cpp:1876-1883).
void adminWindow::onReportYearsLoaded(const QStringList &years) {
    ui->yearComboBox->clear();
    ui->semYearComboBox->clear();
    for (const QString &y : years) {
        ui->yearComboBox->addItem(y);
        ui->semYearComboBox->addItem(y);
    }
}

// Courses (reproduces cpp:1927-1932).
void adminWindow::onReportCoursesLoaded(const QStringList &courses) {
    ui->filterCourseBox->clear();
    for (const QString &c : courses)
        ui->filterCourseBox->addItem(c);
}

// Combo-load dialogs (rows 4/4a/5/6/7/8/9/10).
void adminWindow::onReportLoadError(const QString &title, const QString &message, bool critical) {
    if (critical)
        QMessageBox::critical(this, title, message);
    else
        QMessageBox::warning(this, title, message);
}

// Report-data error (rows 1/2/3), title "Error".
void adminWindow::onReportError(const QString &message, bool critical) {
    if (critical)
        QMessageBox::critical(this, "Error", message);
    else
        QMessageBox::warning(this, "Error", message);
    m_pendingReportAction = ReportAction::None;
}

// Preview refresh (reproduces fetchPreviewData success path cpp:2747).
void adminWindow::onPreviewDataReady(const QJsonArray &data) {
    updateChartsPreview(data);
}

// Export/print dispatch — replaces the three postReportData callbacks.
void adminWindow::onReportDataReady(const QJsonArray &data) {
    const QJsonObject filters = m_pendingReportFilters;
    const ReportAction action = m_pendingReportAction;
    m_pendingReportAction = ReportAction::None;

    switch (action) {
    case ReportAction::Pdf:   exportReportToPDF(data, filters);   break;
    case ReportAction::Excel: exportReportToExcel(data, filters); break;
    case ReportAction::Print: printReport(data, filters);         break;
    case ReportAction::None:  break;
    }
}

void adminWindow::onGeneratePDFBtnClicked() {
    QJsonObject filters = collectReportFilters(true);
    if (filters.isEmpty()) return;
    emit reportFiltersReady(filters);
    qDebug() << "Generate PDF filters:" << filters;

    m_pendingReportAction  = ReportAction::Pdf;
    m_pendingReportFilters = filters;
    m_reportController->fetchReportRows(filters);
}

void adminWindow::onGenerateExcelBtnClicked() {
    QJsonObject filters = collectReportFilters(true);
    if (filters.isEmpty()) return;
    emit reportFiltersReady(filters);
    qDebug() << "Generate Excel filters:" << filters;

    m_pendingReportAction  = ReportAction::Excel;
    m_pendingReportFilters = filters;
    m_reportController->fetchReportRows(filters);
}


QJsonObject adminWindow::collectReportFilters(bool validateFilters)
{
    qDebug() << "collectReportFilters() called";

    QJsonObject filters;

    // Department validation
    if (ui->filterDepartmentBox->currentIndex() <= 0) {
        if (validateFilters)
            QMessageBox::warning(this, "Invalid Input", "Please select a department.");
        return {};
    }
    filters["department"] = ui->filterDepartmentBox->currentText();

    // Course validation
    if (ui->filterCourseBox->count() == 0) {
        if (validateFilters)
            QMessageBox::warning(this, "Invalid Input", "No courses available for the selected department.");
        return {};
    }
    if (ui->filterCourseBox->currentIndex() == 0) {
        filters["course"] = "All";
        filters["includeAllCourses"] = true;
    } else {
        filters["course"] = ui->filterCourseBox->currentText();
        filters["includeAllCourses"] = false;
    }

    filters["palette"] = ui->paletteComboBox->currentText();

    // --- Duration switch (date math delegated to ReportController::computeDateRange) ---
    const int durationType = ui->durationTypeBox->currentIndex();
    switch (durationType) {
    case 0: { // Day
        DateRange r = ReportController::computeDateRange(
            0, ui->dateEdit->date(), 0, 0, QString(), 0, QDate(), QDate());
        if (!r.valid) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a valid date.");
            return {};
        }
        filters["start"] = r.start;
        filters["end"]   = r.end;
        filters["durationType"] = "day";
        break;
    }
    case 1: { // Month
        int monthIndex = ui->monthComboBox->currentIndex();
        int yearIndex  = ui->yearComboBox->currentIndex();
        if (monthIndex < 0 || yearIndex < 0) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a month and year.");
            return {};
        }
        int month = monthIndex + 1;
        int year  = ui->yearComboBox->currentText().toInt();
        DateRange r = ReportController::computeDateRange(
            1, QDate(), month, year, QString(), 0, QDate(), QDate());
        filters["start"] = r.start;
        filters["end"]   = r.end;
        filters["year"]  = year;
        filters["durationType"] = "month";
        break;
    }
    case 2: { // Semester
        int semIndex  = ui->semesterComboBox->currentIndex();
        int yearIndex = ui->semYearComboBox->currentIndex();
        if (semIndex < 0 || yearIndex < 0) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a semester and year.");
            return {};
        }
        int year = ui->semYearComboBox->currentText().toInt();
        QString sem = ui->semesterComboBox->currentText();
        DateRange r = ReportController::computeDateRange(
            2, QDate(), 0, 0, sem, year, QDate(), QDate());
        filters["start"]    = r.start;
        filters["end"]      = r.end;
        filters["semester"] = sem;
        filters["year"]     = year;
        filters["durationType"] = "semester";
        break;
    }
    case 3: { // Custom
        DateRange r = ReportController::computeDateRange(
            3, QDate(), 0, 0, QString(), 0,
            ui->startDateEdit->date(), ui->endDateEdit->date());
        if (!r.valid) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a valid date range.");
            return {};
        }
        filters["start"] = r.start;
        filters["end"]   = r.end;
        filters["durationType"] = "custom";
        break;
    }
    default:
        if (validateFilters)
            QMessageBox::warning(this, "Invalid Input", "Please select a duration type.");
        return {};
    }

    filters["chartType"] = ui->chartTypeBox->currentText();
    filters["schoolYear"] = ui->filterSchoolYearEdit->text().trimmed();

    return filters;
}

QJsonObject adminWindow::collectReportFiltersForPreview()
{
    // Call the existing function with validateFilters = false
    return collectReportFilters(false);
}



void adminWindow::onFilterDepartmentBoxCurrentIndexChanged(int index) {
    if (index <= 0) {
        ui->filterCourseBox->clear();
        ui->filterCourseBox->addItem("All Courses");
        return;
    }
    m_reportController->loadCourses(ui->filterDepartmentBox->currentText());
}

void adminWindow::exportReportToPDF(const QJsonArray &data, const QJsonObject &filters)
{
    qDebug() << "=== PDF EXPORT DEBUG ===";
    qDebug() << "Data array size:" << data.size();
    qDebug() << "Data contents:" << data;
    qDebug() << "Filters received:" << filters;

    if (data.isEmpty()) {
        QMessageBox::information(this, "No Data",
                                 "No data available for the selected filters. Please check your date range and department selection.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Save Report", "", "PDF Files (*.pdf)");
    if (filePath.isEmpty()) {
        QMessageBox::information(this, "Export Canceled", "No file selected.");
        return;
    }

    QPdfWriter pdf(filePath);
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(150);
    pdf.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    ReportHeaderInfo info = collectHeaderInfo();
    ReportPalette palette = ReportController::getPalette(filters["palette"].toString());
    if (!m_reportRenderer.paintReport(&pdf, 150, data, filters, palette, info)) {
        QMessageBox::critical(this, "Error", "Failed to open PDF for writing.");
        return;
    }

    QMessageBox::information(this, "Success", "Report exported to PDF successfully.");
}

void adminWindow::onPrintReportBtnClicked() {
    QJsonObject filters = collectReportFilters(true);
    if (filters.isEmpty()) return;
    emit reportFiltersReady(filters);
    qDebug() << "Print Report filters:" << filters;

    m_pendingReportAction  = ReportAction::Print;
    m_pendingReportFilters = filters;
    m_reportController->fetchReportRows(filters);
}

void adminWindow::printReport(const QJsonArray &data, const QJsonObject &filters) {
    if (data.isEmpty()) {
        QMessageBox::information(this, "No Data",
                                 "No data available for the selected filters. Please check your date range and department selection.");
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ReportHeaderInfo info = collectHeaderInfo();
    ReportPalette palette = ReportController::getPalette(filters["palette"].toString());
    if (!m_reportRenderer.paintReport(&printer, printer.resolution(), data, filters, palette, info)) {
        QMessageBox::critical(this, "Error", "Failed to start painting to printer.");
    }
}

void adminWindow::exportReportToExcel(const QJsonArray &rows, const QJsonObject &filters) {
    if (rows.isEmpty()) {
        QMessageBox::information(this, "No Data",
                                 "No data available for the selected filters.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Save Excel Report", "", "Excel Files (*.xlsx)");
    if (filePath.isEmpty())
        return;

    QXlsx::Document xlsx;
    ReportHeaderInfo info = collectHeaderInfo();
    m_reportRenderer.writeReportToXlsx(xlsx, rows, filters, info);

    if (!xlsx.saveAs(filePath)) {
        QMessageBox::critical(this, "Error", "Failed to save Excel file.");
    } else {
        QMessageBox::information(this, "Success", "Report exported to Excel successfully.");
    }
}



void adminWindow::updatePalettePreview(const QString &choice) {
    ReportPalette palette = ReportController::getPalette(choice);

    QPixmap pix(200, 50);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);

    // Draw header section
    painter.fillRect(QRect(0, 0, 200, 20), palette.headerBg);
    painter.setPen(palette.headerText);
    painter.drawText(10, 15, "School ID   |   Name   |   Course   |   Visits");

    // Draw row section
    painter.fillRect(QRect(0, 20, 200, 30), palette.rowEvenBg);
    painter.setPen(palette.rowText);
    painter.drawText(10, 40, "20251234   |   Juan Dela Cruz   |   BSIT   |   12");
    painter.end();
    ui->palettePreview->setPixmap(pix);
}

void adminWindow::updateChartsPreview(const QJsonArray &data)
{
    QGridLayout *layout = ui->chartsPreviewBox;

    if (!layout) {
        qDebug() << "updateChartsPreview: no grid layout found";
        return;
    }

    // Clear existing widgets
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (QWidget *w = child->widget()) {
            layout->removeWidget(w);
            w->deleteLater();
        }
        delete child;
    }

    // Remove layout margins for tighter fit
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5); // Small gap between charts

    // Get palette
    ReportPalette pal = ReportController::getPalette(ui->paletteComboBox->currentText());
    if (pal.chartColors.isEmpty()) {
        pal.chartColors = { QColor("#3498DB"), QColor("#E74C3C"), QColor("#2ECC71"),
                           QColor("#F39C12"), QColor("#9B59B6"), QColor("#1ABC9C") };
    }

    // Prepare data
    QMap<QString,int> courseCounts;
    QMap<int,int> hourCounts;
    bool hasRealData = !data.isEmpty();

    if (hasRealData) {
        for (const QJsonValue &v : data) {
            QJsonObject obj = v.toObject();
            QString course = obj["course"].toString();
            int visits = obj["visits"].toInt();
            courseCounts[course] += visits;

            QTime t = QTime::fromString(obj["login_time"].toString(), "HH:mm:ss");
            if (t.isValid()) hourCounts[t.hour()] += 1;
        }
    } else {
        courseCounts["Grade 7"] = 0;
        courseCounts["Grade 8"] = 0;
        courseCounts["Grade 9"] = 0;
        for (int h = 8; h <= 17; h++) hourCounts[h] = 0;
    }

    // === BAR CHART ===
    QBarSeries *barSeries = new QBarSeries();
    QStringList categories;
    int colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QBarSet *set = new QBarSet(it.key());
        *set << it.value();
        set->setBrush(hasRealData ? pal.chartColors[colorIndex % pal.chartColors.size()]
                                  : QColor("#dfe6e9"));
        set->setLabelColor(Qt::black);
        barSeries->append(set);
        categories << it.key();
        ++colorIndex;
    }

    QChart *barChart = new QChart();
    barChart->addSeries(barSeries);
    barChart->setTitle(hasRealData ? tr("Visits by Course") : tr("Visits by Course (No data)"));
    barChart->setTitleFont(QFont("Arial", 9, QFont::Bold));

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsFont(QFont("Arial", 7));
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText(tr("Visits"));
    axisY->setLabelsFont(QFont("Arial", 7));
    barChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    barChart->setAnimationOptions(hasRealData ? QChart::SeriesAnimations : QChart::NoAnimation);
    barChart->legend()->setVisible(hasRealData);

    optimizeChartForPreview(barChart); // Apply optimization

    QChartView *barView = new QChartView(barChart);
    barView->setRenderHint(QPainter::Antialiasing);
    barView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    barView->setContentsMargins(0, 0, 0, 0); // No view margins

    // === PIE CHART ===
    QPieSeries *pieSeries = new QPieSeries();
    colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QPieSlice *slice = pieSeries->append(it.key(), it.value());
        slice->setBrush(hasRealData ? pal.chartColors[colorIndex % pal.chartColors.size()]
                                    : QColor("#dfe6e9"));
        if (hasRealData && it.value() > 0) {
            slice->setLabelVisible(true);
            slice->setLabel(QString("%1: %2%").arg(it.key())
                                .arg(QString::number(slice->percentage() * 100, 'f', 1)));
            slice->setLabelFont(QFont("Arial", 7));
        }
        ++colorIndex;
    }

    QChart *pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle(hasRealData ? tr("Share by Course") : tr("Share by Course (No data)"));
    pieChart->setTitleFont(QFont("Arial", 9, QFont::Bold));
    pieChart->legend()->setVisible(hasRealData);
    pieChart->legend()->setAlignment(Qt::AlignRight);

    optimizeChartForPreview(pieChart); // Apply optimization

    QChartView *pieView = new QChartView(pieChart);
    pieView->setRenderHint(QPainter::Antialiasing);
    pieView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pieView->setContentsMargins(0, 0, 0, 0);

    // === LINE CHART ===
    QLineSeries *lineSeries = new QLineSeries();
    lineSeries->setName(tr("Hourly Visits"));
    for (auto it = hourCounts.begin(); it != hourCounts.end(); ++it) {
        lineSeries->append(it.key(), it.value());
    }

    QPen linePen(hasRealData ? pal.chartColors[0] : QColor("#b2bec3"));
    linePen.setWidth(3);
    if (!hasRealData) linePen.setStyle(Qt::DashLine);
    lineSeries->setPen(linePen);
    lineSeries->setPointsVisible(true);

    QChart *lineChart = new QChart();
    lineChart->addSeries(lineSeries);
    lineChart->setTitle(hasRealData ? tr("Visits by Hour") : tr("Visits by Hour (No data)"));
    lineChart->setTitleFont(QFont("Arial", 9, QFont::Bold));
    lineChart->createDefaultAxes();

    QList<QAbstractAxis*> axes = lineChart->axes();
    if (axes.size() >= 2) {
        axes[0]->setTitleText(tr("Hour"));
        axes[1]->setTitleText(tr("Visits"));
        axes[0]->setLabelsFont(QFont("Arial", 7));
        axes[1]->setLabelsFont(QFont("Arial", 7));
    }

    lineChart->setAnimationOptions(hasRealData ? QChart::SeriesAnimations : QChart::NoAnimation);
    lineChart->legend()->setVisible(hasRealData);

    optimizeChartForPreview(lineChart); // Apply optimization

    QChartView *lineView = new QChartView(lineChart);
    lineView->setRenderHint(QPainter::Antialiasing);
    lineView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    lineView->setContentsMargins(0, 0, 0, 0);

    // === Add charts to layout ===
    layout->addWidget(barView, 0, 0);
    layout->addWidget(pieView, 0, 1);
    layout->addWidget(lineView, 1, 0, 1, 2);

    layout->setRowStretch(0, 1);
    layout->setRowStretch(1, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    qDebug() << "Charts updated with" << (hasRealData ? "real" : "placeholder") << "data";
}

void adminWindow::optimizeChartForPreview(QChart *chart)
{
    if (!chart) return;

    // Remove all chart margins
    chart->setMargins(QMargins(0, 0, 0, 0));
    if (chart->layout()) {
        chart->layout()->setContentsMargins(5, 5, 5, 5); // Minimal padding
    }
    chart->setBackgroundRoundness(0);

    // Make legend more compact
    if (chart->legend() && chart->legend()->isVisible()) {
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->setFont(QFont("Arial", 7));
        chart->legend()->setMaximumHeight(30); // Limit legend height
    }
}

void adminWindow::connectFilterSignals()
{
    // --- Department ---
    connect(ui->filterDepartmentBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index <= 0 || ui->durationTypeBox->currentIndex() < 0) {
                    updateChartsPreview(QJsonArray());
                    return;
                }
                QJsonObject filters = collectReportFiltersForPreview();
                if (!filters.isEmpty()) {
                    m_reportController->fetchPreviewData(filters);
                } else {
                    updateChartsPreview(QJsonArray());
                }
            });

    // --- Course ---
    connect(ui->filterCourseBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (ui->filterDepartmentBox->currentIndex() <= 0 || ui->durationTypeBox->currentIndex() < 0) {
                    updateChartsPreview(QJsonArray());
                    return;
                }
                QJsonObject filters = collectReportFiltersForPreview();
                if (!filters.isEmpty()) {
                    m_reportController->fetchPreviewData(filters);
                } else {
                    updateChartsPreview(QJsonArray());
                }
            });

    // --- Duration ---
    connect(ui->durationTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (ui->filterDepartmentBox->currentIndex() <= 0 || index < 0) {
                    updateChartsPreview(QJsonArray());
                    return;
                }
                QJsonObject filters = collectReportFiltersForPreview();
                if (!filters.isEmpty()) {
                    m_reportController->fetchPreviewData(filters);
                } else {
                    updateChartsPreview(QJsonArray());
                }
            });

    // --- Palette ---
    connect(ui->paletteComboBox, &QComboBox::currentTextChanged,
            this, [this](const QString &) {
                updatePalettePreview(ui->paletteComboBox->currentText());
                if (ui->filterDepartmentBox->currentIndex() > 0 && ui->durationTypeBox->currentIndex() >= 0) {
                    QJsonObject filters = collectReportFiltersForPreview();
                    if (!filters.isEmpty()) {
                        m_reportController->fetchPreviewData(filters);
                    } else {
                        updateChartsPreview(QJsonArray());
                    }
                }
            });
}


void adminWindow::onEditStudentBtnClicked()
{
    if (ui->editStudentBtn->text() == "Edit") {
        // ✅ Check if there are any students in the table first
        if (ui->studentSearchTable->rowCount() == 0) {
            showTemporaryOverlay(ui->studentSearchPage,
                                 "⚠️ No students to edit\nPlease search for students first");
            return;
        }

        // --- Enter Edit Mode ---
        ui->editStudentBtn->setText("Update");
        ui->cancelEditBtn->setVisible(true);
        ui->selectAllBtn->setVisible(true);
        ui->deleteStudentBtn->setVisible(true);
        ui->selectAllBtn->setText("Select All");
        ui->studentSearchTable->setEditTriggers(QAbstractItemView::DoubleClicked);

        // ✅ Prevent false triggers while setting up
        ui->studentSearchTable->blockSignals(true);
        isSettingUpEditMode = true;

        // Remove existing widgets (cleanup)
        for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row)
            ui->studentSearchTable->removeCellWidget(row, 0);

        // --- Add checkboxes + make columns editable ---
        for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
            // Create checkbox widget
            QCheckBox *checkbox = new QCheckBox();
            QWidget *widget = new QWidget();
            QHBoxLayout *layout = new QHBoxLayout(widget);
            layout->addWidget(checkbox);
            layout->setAlignment(Qt::AlignCenter);
            layout->setContentsMargins(0, 0, 0, 0);
            ui->studentSearchTable->setCellWidget(row, 0, widget);

            // --- Connect red highlight when checkbox checked ---
            connect(checkbox, &QCheckBox::checkStateChanged, this, [=](int state) {
                QColor color = (state == Qt::Checked)
                ? QColor("#FFCCCB")  // light red when checked
                : QColor(Qt::white); // white when unchecked
                for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
                    QTableWidgetItem *cell = ui->studentSearchTable->item(row, col);
                    if (cell)
                        cell->setBackground(color);
                }
            });

            // --- Unlock columns except School ID (col 2) ---
            for (int col = 1; col < ui->studentSearchTable->columnCount(); ++col) {
                QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
                if (!item) continue;
                if (col == 2)
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                else
                    item->setFlags(item->flags() | Qt::ItemIsEditable);
            }
        }

        // ✅ Finish setup
        isSettingUpEditMode = false;
        ui->studentSearchTable->blockSignals(false);

        // --- Highlight yellow when admin edits a cell & auto-check the row ---
        disconnect(ui->studentSearchTable, &QTableWidget::itemChanged, nullptr, nullptr);
        connect(ui->studentSearchTable, &QTableWidget::itemChanged, this, [=](QTableWidgetItem *item) {
            if (isSettingUpEditMode || !item) return;

            int row = item->row();
            QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
            QCheckBox *checkbox = widget ? widget->findChild<QCheckBox*>() : nullptr;

            // ✅ Auto-check the checkbox when a cell is edited
            if (checkbox && !checkbox->isChecked())
                checkbox->setChecked(true);

            // ✅ Apply yellow highlight for edits
            QColor editColor("#FFF8DC"); // light yellow
            for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
                QTableWidgetItem *cell = ui->studentSearchTable->item(row, col);
                if (cell)
                    cell->setBackground(editColor);
            }
        });

        // ✅ Show info overlay
        showTemporaryOverlay(ui->studentSearchPage,
                             "📝 Edit Mode Active\nDouble-click cells to edit (yellow highlight).\nChecked rows are selected for update (red highlight).");

    } else {
        // --- Update Mode ---
        ui->selectAllBtn->setVisible(false);
        ui->deleteStudentBtn->setVisible(false);

        QList<StudentRecord> updates;

        for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
            QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
            QCheckBox *checkbox = widget ? widget->findChild<QCheckBox*>() : nullptr;
            if (!checkbox || !checkbox->isChecked()) continue;

            auto getText = [&](int col) -> QString {
                QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
                return item ? item->text().trimmed() : QString();
            };

            QString schoolId = getText(2);
            if (schoolId.isEmpty()) continue;

            StudentRecord rec;
            rec.schoolId   = schoolId;
            rec.code       = getText(1);
            rec.name       = getText(3);
            rec.department = getText(5);
            rec.course     = getText(4);
            rec.yearLevel  = getText(6);
            rec.gender     = getText(7);
            rec.status     = getText(8);
            updates.append(rec);
        }

        // Reset colors after update
        for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row)
            for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
                QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
                if (item) item->setBackground(Qt::white);
            }

        if (updates.isEmpty()) {
            showTemporaryOverlay(ui->studentSearchPage,
                                 "⚠️ No students selected or edited.");
            ui->editStudentBtn->setText("Edit");
            ui->cancelEditBtn->setVisible(false);
            ui->selectAllBtn->setVisible(false);
            ui->deleteStudentBtn->setVisible(false);
            ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            clearCheckboxes();
            return;
        }

        // --- Send updates ---
        bulkUpdateStudents(updates);
    }
}

void adminWindow::clearCheckboxes()
{
    // Just remove the checkbox widgets, don't remove the column
    for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
        ui->studentSearchTable->removeCellWidget(row, 0);
    }
}

void adminWindow::bulkUpdateStudents(const QList<StudentRecord> &updates)
{
    m_studentController->bulkUpdateStudents(updates);
}

// Restores the Student Search toolbar to its non-edit (view) state:
// relabels/enables the Edit button, hides the edit-only controls, disables
// table editing, and clears the row checkboxes. Shared by every edit-mode
// exit path (cancel, bulk-update completion, delete completion).
void adminWindow::exitStudentEditMode()
{
    ui->editStudentBtn->setText("Edit");
    ui->editStudentBtn->setEnabled(true);
    ui->cancelEditBtn->setVisible(false);
    ui->selectAllBtn->setVisible(false);
    ui->deleteStudentBtn->setVisible(false);
    ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clearCheckboxes();
}

void adminWindow::onBulkUpdateFailed(const QString &errorString)
{
    exitStudentEditMode();

    showTemporaryOverlay(ui->studentSearchPage,
                         QString("❌ Network Error\n%1").arg(errorString));   // row 4
}

void adminWindow::onBulkUpdateFinished(const BulkUpdateResult &result)
{
    exitStudentEditMode();

    if (result.ok) {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("✅ %1 student%2 updated successfully")
                                 .arg(result.updatedCount)
                                 .arg(result.updatedCount == 1 ? "" : "s"));   // row 5

        if (!result.errors.isEmpty()) {
            QString errorList;
            for (const QString &err : result.errors)
                errorList += err + "\n";
            QTimer::singleShot(2500, this, [this, errorList]() {
                QMessageBox::warning(this, "Some Updates Failed", errorList);   // row 6
            });
        }

        performStudentSearch(false);   // silent refresh (flag=false via thin helper)
    } else {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("⚠️ Update Failed\n%1").arg(result.message));   // row 7
    }
}

void adminWindow::onDeleteStudentBtnClicked()
{
    QStringList selectedIds;
    for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
        QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
        if (!widget) continue;
        QCheckBox *cb = widget->findChild<QCheckBox*>();
        if (cb && cb->isChecked()) {
            QTableWidgetItem *idItem = ui->studentSearchTable->item(row, 2); // col 2 = School ID
            if (idItem)
                selectedIds << idItem->text();
        }
    }

    if (selectedIds.isEmpty()) {
        showTemporaryOverlay(ui->studentSearchPage,
                             "⚠️ Please select at least one student to delete");
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this, "Confirm Delete",
        QString("Are you sure you want to delete %1 student(s)?\n\nThis action cannot be undone.")
            .arg(selectedIds.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (confirm == QMessageBox::Yes)
        m_studentController->deleteStudents(selectedIds);
}

void adminWindow::onDeleteFinished(bool ok, int requestedCount, const QString &message)
{
    exitStudentEditMode();

    if (ok) {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("✅ %1 student%2 deleted successfully")
                                 .arg(requestedCount)
                                 .arg(requestedCount == 1 ? "" : "s"));
        performStudentSearch(false);   // silent refresh
    } else {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("⚠️ %1").arg(message));
    }
}

void adminWindow::onDeleteFailed(const QString &errorString)
{
    exitStudentEditMode();

    showTemporaryOverlay(ui->studentSearchPage,
                         QString("❌ Error: %1").arg(errorString));
}

// Add cancel button handler
void adminWindow::onCancelEditBtnClicked()
{
    exitStudentEditMode();

    // Lock all cells again
    for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
        // Reset row colors
        for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
            QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
            if (!item) continue;

            item->setBackground(Qt::white);  // ✅ Reset colors
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);  // ✅ Lock editing
        }
    }

    // ✅ Show cancellation overlay
    showTemporaryOverlay(ui->studentSearchPage,
                         "❌ Edit mode cancelled\nNo changes were saved");

    performStudentSearch(false); // Reload original data
}

void adminWindow::on_selectAllBtn_clicked()
{
    bool selectAll = (ui->selectAllBtn->text() == "Select All");
    int selectedCount = 0;

    // ✅ Disable updates during batch operation
    ui->studentSearchTable->setUpdatesEnabled(false);

    for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
        QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
        if (!widget) continue;

        QCheckBox *checkbox = widget->findChild<QCheckBox*>();
        if (checkbox) {
            checkbox->blockSignals(true);
            checkbox->setChecked(selectAll);
            checkbox->blockSignals(false);

            if (selectAll) selectedCount++;
        }

        // Apply colors
        QColor targetColor = selectAll ? QColor("#ADD8E6") : QColor(Qt::white);
        for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
            QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
            if (item) {
                item->setBackground(targetColor);
            }
        }
    }

    // ✅ Re-enable updates and force full repaint
    ui->studentSearchTable->setUpdatesEnabled(true);
    ui->studentSearchTable->viewport()->update();

    ui->selectAllBtn->setText(selectAll ? "Deselect All" : "Select All");

    // Show feedback overlay
    if (selectAll && selectedCount > 0) {
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("✅ Selected all %1 student%2")
                                 .arg(selectedCount)
                                 .arg(selectedCount == 1 ? "" : "s"));
    } else if (!selectAll) {
        ui->studentSearchTable->blockSignals(true);
        for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
            QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
            if (!widget) continue;

            // Uncheck checkbox (if any)
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox) {
                checkbox->blockSignals(true);
                checkbox->setChecked(false);
                checkbox->blockSignals(false);
            }

            // Reset row color
            for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
                QTableWidgetItem *item = ui->studentSearchTable->item(row, col);
                if (!item) continue;
                item->setBackground(Qt::white);
            }
        }

        // ✅ Re-enable signals after cleanup
        ui->studentSearchTable->blockSignals(false);

        // ✅ Update button text
        ui->selectAllBtn->setText("Select All");
        showTemporaryOverlay(ui->studentSearchPage, "✔ All students deselected");
    }
}

void adminWindow::performStudentSearch(bool showOverlay)
{
    ui->searchLoadingBar->setVisible(true);
    ui->searchLoadingBar->setValue(0);
    m_studentSearchShowOverlay = showOverlay;

    m_studentController->searchStudents(
        ui->searchLineEdit->text().trimmed(),
        ui->searchDepartmentFilter->currentText(),
        ui->searchCourseFilter->currentText());
}

void adminWindow::displaySearchResults(const QList<StudentRecord> &students,
                                       const QString &highlightTerm)
{
    Q_UNUSED(highlightTerm);   // legacy never used it — no highlighting exists; preserved for parity
    ui->studentSearchTable->blockSignals(true);
    ui->studentSearchTable->clearContents();
    ui->studentSearchTable->setRowCount(students.size());

    for (int row = 0; row < students.size(); ++row) {
        const StudentRecord &s = students.at(row);
        ui->studentSearchTable->setItem(row, 0, new QTableWidgetItem(""));
        ui->studentSearchTable->setItem(row, 1, new QTableWidgetItem(s.code));
        ui->studentSearchTable->setItem(row, 2, new QTableWidgetItem(s.schoolId));
        ui->studentSearchTable->setItem(row, 3, new QTableWidgetItem(s.name));
        ui->studentSearchTable->setItem(row, 4, new QTableWidgetItem(s.course));
        ui->studentSearchTable->setItem(row, 5, new QTableWidgetItem(s.department));
        ui->studentSearchTable->setItem(row, 6, new QTableWidgetItem(s.yearLevel));
        ui->studentSearchTable->setItem(row, 7, new QTableWidgetItem(s.gender));
        ui->studentSearchTable->setItem(row, 8, new QTableWidgetItem(s.status));
    }

    ui->studentSearchTable->blockSignals(false);
    ui->selectAllBtn->setText("Select All");
}

void adminWindow::onSearchFailed(const QString &errorString)
{
    ui->searchLoadingBar->setVisible(false);
    if (m_studentSearchShowOverlay)
        showTemporaryOverlay(ui->studentSearchPage,
                             QString("❌ Network Error\n%1").arg(errorString));   // row 11
}

void adminWindow::onSearchFinished(SearchOutcome outcome,
                                   const QList<StudentRecord> &records,
                                   const QString &message,
                                   const QString &searchTerm)
{
    ui->searchLoadingBar->setVisible(false);

    switch (outcome) {
    case SearchOutcome::InvalidResponse:
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage, "⚠️ Invalid server response");  // row 12
        break;
    case SearchOutcome::NotSuccess:
        ui->studentSearchTable->clearContents();          // cleared regardless of flag
        ui->studentSearchTable->setRowCount(0);
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage, QString("🔍 %1").arg(message));  // row 13
        break;
    case SearchOutcome::Empty:
        ui->studentSearchTable->clearContents();          // cleared regardless of flag
        ui->studentSearchTable->setRowCount(0);
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage,
                                 "🔍 No students found\nTry adjusting your search filters");  // row 14
        break;
    case SearchOutcome::Results:
        // Legacy guard was `showOverlay && students.size() > 0` (adminwindow.cpp:3236);
        // the size check is subsumed here because parseSearchResponse only returns
        // Results when the array is non-empty (Empty is split out upstream).
        if (m_studentSearchShowOverlay)
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("✅ Found %1 student%2")
                                     .arg(records.size())
                                     .arg(records.size() == 1 ? "" : "s"));   // row 15
        displaySearchResults(records, searchTerm);
        break;
    case SearchOutcome::NetworkError:
        break;   // never emitted via searchFinished; handled by onSearchFailed
    }
}

void adminWindow::onSearchBtnClicked()
{
    performStudentSearch(true); // That's it - performStudentSearch handles everything
}

void adminWindow::populateFilters()
{
    m_studentController->loadDepartments();
}

void adminWindow::onDepartmentFilterChanged(int)
{
    ui->searchCourseFilter->clear();
    ui->searchCourseFilter->addItem("Select Course");   // preserves the index-0 end state

    const QString dept = ui->searchDepartmentFilter->currentText();
    if (dept.isEmpty() || dept == "Select Department")
        return;                                          // placeholder: combo already = ["Select Course"]

    m_studentController->loadCourses(dept);              // single GET (was two)
}

void adminWindow::onDepartmentsLoaded(const QStringList &departments)
{
    ui->searchDepartmentFilter->clear();
    ui->searchDepartmentFilter->addItem("Select Department");
    ui->searchDepartmentFilter->addItems(departments);
}

void adminWindow::onCoursesLoaded(const QStringList &courses)
{
    ui->searchCourseFilter->clear();
    ui->searchCourseFilter->addItem("Select Course");
    ui->searchCourseFilter->addItems(courses);
}

VisitorFilter adminWindow::collectVisitorFilter() const
{
    VisitorFilter f;
    f.search = ui->visitorSearchLineEdit->text().trimmed();

    const QString dateType = ui->visitorDateFilterBox->currentText();
    if (dateType == "Specific Day") {
        f.dateType  = DateFilterType::SpecificDay;
        f.startDate = ui->visitorDateEdit->date().toString("yyyy-MM-dd");
        f.endDate   = f.startDate;
    }
    else if (dateType == "Month") {
        f.dateType = DateFilterType::Month;
        const QPair<QString, QString> range =
            VisitorController::monthRange(ui->visitorMonthCombo->currentIndex() + 1,
                                          ui->visitorYearSpin->value());
        f.startDate = range.first;
        f.endDate   = range.second;
    }
    else if (dateType == "Date Range") {
        f.dateType  = DateFilterType::DateRange;
        f.startDate = ui->visitorStartDate->date().toString("yyyy-MM-dd");
        f.endDate   = ui->visitorEndDate->date().toString("yyyy-MM-dd");
    }
    else {
        // Default fallback: show all — dates stay empty
        f.dateType = DateFilterType::All;
    }
    return f;
}

void adminWindow::populateVisitorTable(const QList<VisitorRecord> &records, int totalCount)
{
    m_visitorRecords = records;

    ui->visitorTable->setRowCount(records.size());
    ui->visitorTable->setColumnCount(5);
    QStringList headers = {"Name", "Company", "Purpose", "Date", "Time"};
    ui->visitorTable->setHorizontalHeaderLabels(headers);

    for (int i = 0; i < records.size(); ++i) {
        const VisitorRecord &rec = records.at(i);
        ui->visitorTable->setItem(i, 0, new QTableWidgetItem(rec.name));
        ui->visitorTable->setItem(i, 1, new QTableWidgetItem(rec.company));
        ui->visitorTable->setItem(i, 2, new QTableWidgetItem(rec.purpose));
        ui->visitorTable->setItem(i, 3, new QTableWidgetItem(rec.date));
        ui->visitorTable->setItem(i, 4, new QTableWidgetItem(rec.time));
    }

    // Per-load table re-setup — redundant on repeat loads but kept exactly as
    // today to stay behavior-preserving.
    ui->visitorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->visitorTable->setAlternatingRowColors(true);
    ui->visitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->visitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->visitorTable->setSelectionMode(QAbstractItemView::SingleSelection);

    if (ui->visitorTotalLabel) {
        ui->visitorTotalLabel->setText(QString("📊 Total Visitors: %1").arg(totalCount));
    }

    if (records.isEmpty()) {
        ui->visitorTable->setRowCount(0);
        showTemporaryOverlay(ui->visitorTable, "No visitors found for the selected filters.");
        if (ui->visitorTotalLabel)
            ui->visitorTotalLabel->setText("📊 Total Visitors: 0");
        return;
    }
}

void adminWindow::onVisitorFetchError(const QString &title, const QString &message)
{
    // Reproduces today's three cases exactly (old adminwindow.cpp:3694, 3704,
    // 3710): network error → critical, everything else → warning.
    if (title == QLatin1String("Network Error"))
        QMessageBox::critical(this, title, message);
    else
        QMessageBox::warning(this, title, message);
}

void adminWindow::on_extractVisitorBtn_clicked() {
    // Guard moves from visitorTable->rowCount() == 0 to the cache —
    // equivalent, since the table always mirrors the cache.
    if (m_visitorRecords.isEmpty()) {
        QMessageBox::information(this, "No Data", "There are no visitor logs to export.");
        return;
    }

    // Read at export time, matching today's behavior of deriving the
    // filename/filter rows from the CURRENT widget state even if the user
    // changed filters without re-searching.
    const VisitorFilter filter = collectVisitorFilter();

    // --- Ask user where to save ---
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Visitor Logs",
        QDir::homePath() + "/" + VisitorController::defaultExportFileName(filter),
        "Excel Files (*.xlsx)"
        );
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".xlsx"))
        filePath += ".xlsx";

    if (m_visitorController->exportToExcel(m_visitorRecords, filter,
                                           ui->schoolName->text(), filePath)) {
        QMessageBox::information(this, "Export Successful",
                                 QString("Visitor logs exported successfully!\n\nSaved as:\n%1").arg(filePath));
    } else {
        QMessageBox::critical(this, "Error", "Failed to save the Excel file.");
    }
}

void adminWindow::showTemporaryOverlay(QWidget *parent, const QString &message)
{
    QWidget *page = ui->stackedWidget->currentWidget();
    if (!page) page = parent;

    // --- Full overlay container ---
    QWidget *overlay = new QWidget(page);
    overlay->setStyleSheet("background-color: rgba(0, 0, 0, 120);");
    overlay->resize(page->size());
    overlay->raise();

    // --- Message box ---
    QFrame *box = new QFrame(overlay);
    box->setStyleSheet(
        "QFrame {"
        "    background-color: rgba(255, 255, 255, 230);"
        "    border-radius: 12px;"
        "    border: 1px solid rgba(150, 150, 150, 100);"
        "}"
        );

    QLabel *label = new QLabel(message, box);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setStyleSheet(
        "color: #2C3E50;"
        "font-size: 14px;"
        "font-weight: 600;"
        "background: transparent;"
        "border: none;"
        );

    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->addWidget(label);
    layout->setContentsMargins(40, 30, 40, 30);

    box->adjustSize();
    box->move(
        (overlay->width() - box->width()) / 2,
        (overlay->height() - box->height()) / 2
        );

    // ✅ Fade in effect
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(effect);

    QPropertyAnimation *fadeIn = new QPropertyAnimation(effect, "opacity");
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QPropertyAnimation::DeleteWhenStopped);

    overlay->show();

    // ✅ Auto-hide after 2 seconds with fade out
    QTimer::singleShot(2000, overlay, [overlay, effect]() {
        QPropertyAnimation *fadeOut = new QPropertyAnimation(effect, "opacity");
        fadeOut->setDuration(400);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);

        QObject::connect(fadeOut, &QPropertyAnimation::finished, overlay, [overlay]() {
            overlay->deleteLater();
        });

        fadeOut->start(QPropertyAnimation::DeleteWhenStopped);
    });
}

void adminWindow::onBrowsePhotoBtnClicked() {
    QString filePath = QFileDialog::getOpenFileName(
        this, "Select Student Photo", "", "Images (*.png *.jpg *.jpeg *.bmp)"
        );

    if (!filePath.isEmpty()) {
        selectedPhotoPath = filePath;
        ui->photoPreviewLabel->setPixmap(
            QPixmap(filePath).scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
    }
}

void adminWindow::onStudentTableItemChanged(QTableWidgetItem *item)
{
    if (!item) return;
    qDebug() << "itemChanged triggered:" << item->row() << item->column();

    int row = item->row();

    // Highlight row background color to show it was modified
    for (int col = 0; col < ui->studentSearchTable->columnCount(); ++col) {
        QTableWidgetItem *cell = ui->studentSearchTable->item(row, col);
        if (cell) cell->setBackground(QColor("#FFF8DC")); // soft yellow
    }

    // Track which rows have been edited
    editedRows.insert(row);
}


