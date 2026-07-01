#include "adminwindow.h"
#include "ui_adminwindow.h"
#include "apiconfig.h"
#include "busyindicator.h"
#include "theme.h"
#include "attachfilesdialog.h"
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
#include "reportpreviewdialog.h"

#include <functional>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

// Utility: expand chart to fill most of the available space
void adminWindow::expandChartPlotArea(QChart *chart, const QSize &targetSize)
{
    if (!chart) return;

    // Kill chart margins and padding
    chart->setMargins(QMargins(0, 0, 0, 0));
    if (chart->layout()) {
        chart->layout()->setContentsMargins(0, 0, 0, 0);
    }
    chart->setBackgroundRoundness(0);

    // Optional: keep legend but make it smaller
    if (chart->legend()) {
        chart->legend()->setAlignment(Qt::AlignBottom);
        chart->legend()->setFont(QFont("Arial", 8));
    }

    // Force the plot area to fill most of the available chart size
    QRectF rect(10, 10, targetSize.width() - 20, targetSize.height() - 40);
    chart->setPlotArea(rect);

    qDebug() << "[expandChartPlotArea] Forced plot area to:" << rect
             << " targetSize:" << targetSize;
}

// Renders a QChart into a QImage (software-backed) — safe for PDF use.
QImage adminWindow::renderChartToImage(QChart *chart, const QSize &targetSize)
{
    if (!chart) {
        qDebug() << "Error: Null chart passed to renderChartToImage";
        return QImage();
    }

    QChartView chartView(chart);
    chartView.setRenderHint(QPainter::Antialiasing);
    chartView.resize(targetSize);

    QImage img(targetSize, QImage::Format_ARGB32);
    img.fill(Qt::white);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    chartView.render(&painter, QRectF(QPointF(0, 0), QSizeF(targetSize)));
    painter.end();

    qDebug() << "Chart rendered to image, size:" << img.size();

    return img;
}



// --- Bar Chart ---
QImage adminWindow::makeBarChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette) {
    // Aggregate visits by course
    QMap<QString, int> courseCounts;
    for (const QJsonValue &v : data) {
        QJsonObject obj = v.toObject();
        QString course = obj["course"].toString();
        int visits = obj["visits"].toInt();
        courseCounts[course] += visits;
    }

    //color set
    QBarSeries *series = new QBarSeries();
    QStringList categories;
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };

    int colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QBarSet *set = new QBarSet(it.key());
        *set << it.value();
        set->setBrush(palette.chartColors[colorIndex % palette.chartColors.size()]);
        series->append(set);
        categories << it.key();
        colorIndex++;
    }

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Library Visits by Course");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsFont(QFont("Arial", 12));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Visits");
    axisY->setLabelsFont(QFont("Arial", 12));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(true);
    chart->legend()->setFont(QFont("Arial", 12));

    // ✅ Remove margins and force chart to fill
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    // Render to image
    QImage chartImage(size, QImage::Format_ARGB32);
    chartImage.fill(Qt::white);
    QPainter painter(&chartImage);

    QChartView view(chart);
    view.setRenderHint(QPainter::Antialiasing);
    view.resize(size);
    view.show();
    view.chart()->resize(size);
    view.render(&painter);

    return chartImage;
}


// --- Pie Chart ---
QImage adminWindow::makePieChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette) {
    // Aggregate visits by course
    QMap<QString, int> courseCounts;
    for (const QJsonValue &v : data) {
        QJsonObject obj = v.toObject();
        QString course = obj["course"].toString();
        int visits = obj["visits"].toInt();
        courseCounts[course] += visits;
    }

    // Create pie series
    QPieSeries *series = new QPieSeries();
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };
    int colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QPieSlice *slice = series->append(it.key(), it.value());
        slice->setLabel(QString("%1: %2").arg(it.key()).arg(it.value()));
        slice->setLabelVisible(true);
        slice->setBrush(palette.chartColors[colorIndex % palette.chartColors.size()]);
        slice->setLabelFont(QFont("Arial", 14, QFont::Bold)); // ✅ bigger font
        colorIndex++;
    }

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Library Visits by Course");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));
    chart->legend()->setVisible(true);
    chart->legend()->setFont(QFont("Arial", 12));
    chart->legend()->setAlignment(Qt::AlignRight);

    // ✅ Remove extra margins so chart fills the image
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    // Render chart into image
    QImage chartImage(size, QImage::Format_ARGB32);
    chartImage.fill(Qt::white);
    QPainter painter(&chartImage);

    QChartView view(chart);
    view.setRenderHint(QPainter::Antialiasing);
    view.resize(size);
    view.show();                 // ensures layout calculates
    view.chart()->resize(size);  // force resize of chart
    view.render(&painter);

    return chartImage;
}



// --- Line Chart ---
QImage adminWindow::makeLineChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette) {
    // ✅ Load library hours from QSettings
    QSettings settings("MyCompany", "MyApp");
    int openHour  = settings.value("library/openHour", 7).toInt();   // default 7 AM
    int closeHour = settings.value("library/closeHour", 21).toInt(); // default 9 PM

    // Aggregate visits per course per hour
    QMap<QString, QMap<int, int>> courseTimeCounts;
    int globalMax = 0;

    for (const QJsonValue &v : data) {
        QJsonObject obj = v.toObject();
        QString course = obj["course"].toString();
        QString loginTime = obj["login_time"].toString();
        QTime time = QTime::fromString(loginTime, "HH:mm:ss");
        if (!time.isValid()) {
            qDebug() << "Invalid login_time format:" << loginTime;
            continue;
        }
        int hour = time.hour();

        // ✅ Skip hours outside library hours
        if (hour < openHour || hour > closeHour)
            continue;

        courseTimeCounts[course][hour] += 1;

        if (courseTimeCounts[course][hour] > globalMax)
            globalMax = courseTimeCounts[course][hour];
    }

    QChart *chart = new QChart();
    chart->setTitle("Library Peak Hours by Course");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));

    // One line series per course
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };
    int colorIndex = 0;
    for (auto it = courseTimeCounts.begin(); it != courseTimeCounts.end(); ++it) {
        QLineSeries *series = new QLineSeries();
        series->setName(it.key());

        // ✅ Only loop within open–close hours
        for (int h = openHour; h <= closeHour; ++h) {
            int count = it.value().value(h, 0);
            series->append(h, count);
        }
        series->setColor(palette.chartColors[colorIndex % palette.chartColors.size()]);
        chart->addSeries(series);
        colorIndex++;
    }

    // X axis = only library hours
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Hour of Day");
    axisX->setRange(openHour, closeHour);  // ✅ restrict to library hours
    axisX->setTickCount(closeHour - openHour + 1);
    axisX->setLabelFormat("%d:00");   // shows "7:00", "8:00", etc.
    axisX->setLabelsFont(QFont("Arial", 10));
    chart->addAxis(axisX, Qt::AlignBottom);

    // Y axis = number of students (auto-scale)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Students");
    axisY->setLabelsFont(QFont("Arial", 10));
    axisY->setRange(0, globalMax + 1);
    axisY->setTickCount(globalMax + 2);
    chart->addAxis(axisY, Qt::AlignLeft);

    // Attach axes
    for (QAbstractSeries *s : chart->series()) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    chart->legend()->setVisible(true);
    chart->legend()->setFont(QFont("Arial", 12));

    // ✅ Keep your centered & unclipped layout
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    // Render to QImage (your working approach)
    QImage chartImage(size, QImage::Format_ARGB32);
    chartImage.fill(Qt::white);
    QPainter painter(&chartImage);

    QChartView view(chart);
    view.setRenderHint(QPainter::Antialiasing);
    view.resize(size);
    view.show();
    view.chart()->resize(size);
    view.render(&painter);

    return chartImage;
}

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
    chartsPreviewBoxLayout = ui->chartsPreviewBox;
    searchSpinner = nullptr;
    overlayEffect = nullptr;

    loadFilterDepartments();

    ui->filterCourseBox->clear();
    ui->filterCourseBox->addItem("All Courses");
    ui->filterCourseBox->setCurrentIndex(0);


    connect(ui->durationTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &adminWindow::onDurationChanged);
    loadAvailableYears();
    loadDepartments();
    populateFilters();

    connect(ui->searchDepartmentFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index) {
                ui->searchCourseFilter->clear();
                ui->searchCourseFilter->addItem("Select Course");

                if (index <= 0) return;

                QString dept = ui->searchDepartmentFilter->currentText();

                QUrl url = ApiConfig::endpoint("get_courses.php");
                QUrlQuery query;
                query.addQueryItem("department", dept);
                url.setQuery(query);

                QNetworkRequest courseRequest(url);
                QNetworkReply *courseReply = networkManager->get(courseRequest);

                connect(courseReply, &QNetworkReply::finished, this, [=]() {
                    QByteArray resp = courseReply->readAll();
                    courseReply->deleteLater();

                    QJsonDocument doc = QJsonDocument::fromJson(resp);
                    QJsonObject obj = doc.object();

                    ui->searchCourseFilter->clear();
                    ui->searchCourseFilter->addItem("Select Course");

                    if (obj["status"].toString() == "success") {
                        QJsonArray arr = obj["courses"].toArray();
                        for (const QJsonValue &val : arr) {
                            ui->searchCourseFilter->addItem(val.toString());
                        }
                    }
                });
            });

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
        QString search = ui->visitorSearchLineEdit->text().trimmed();
        QString dateType = ui->visitorDateFilterBox->currentText();
        QString startDate, endDate;

        if (dateType == "Specific Day") {
            startDate = ui->visitorDateEdit->date().toString("yyyy-MM-dd");
            endDate = startDate;
        }
        else if (dateType == "Month") {
            int month = ui->visitorMonthCombo->currentIndex() + 1; // assuming 0-based
            int year = ui->visitorYearSpin->value();
            QDate firstDay(year, month, 1);
            QDate lastDay(year, month, firstDay.daysInMonth());
            startDate = firstDay.toString("yyyy-MM-dd");
            endDate = lastDay.toString("yyyy-MM-dd");
        }
        else if (dateType == "Date Range") {
            startDate = ui->visitorStartDate->date().toString("yyyy-MM-dd");
            endDate = ui->visitorEndDate->date().toString("yyyy-MM-dd");
        }
        else {
            // Default fallback: show all
            dateType = "all";
            startDate = "";
            endDate = "";
        }

        loadVisitorLogs(search, dateType, startDate, endDate);
    });

    connect(ui->visitorBtn, &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentWidget(ui->visitorPage);
        setActiveSidebar(ui->visitorBtn);
        loadVisitorLogs("", "all", "", ""); // Auto-load all logs on open
    });

    // Connectors for studentSearchPage
    ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->searchLoadingBar->setVisible(false);
    ui->searchOverlay->setVisible(false);
    ui->selectAllBtn->setVisible(false);
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
        postData.addQueryItem("old_key", ui->lineEdit->text());
        postData.addQueryItem("new_key", ui->lineEdit_2->text());

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

    // helper lambda to extract cell safely
    auto getCell = [&](int row, const QString &key, int fallbackIndex = -1) -> QString {
        int col = -1;
        if (bulkHeaderIndex.contains(key)) col = bulkHeaderIndex[key];
        else if (fallbackIndex >= 0 && fallbackIndex < ui->bulkTable->columnCount()) col = fallbackIndex;
        if (col < 0) return QString();
        QTableWidgetItem *it = ui->bulkTable->item(row, col);
        return it ? it->text().trimmed() : QString();
    };

    // Step 1: Collect all school IDs
    QStringList schoolIds;
    for (int i = 0; i < rowCount; i++) {
        QString sid = getCell(i, "school_id", 1);
        if (!sid.isEmpty())
            schoolIds << sid;
    }

    // Step 2: Call check_duplicates.php
    QUrl url = ApiConfig::endpoint("check_duplicates.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonArray idsArray;
    for (const QString &id : schoolIds)
        idsArray.append(id);

    QJsonObject jsonReq;
    jsonReq["school_ids"] = idsArray;

    QNetworkReply *dupReply = networkManager->post(request, QJsonDocument(jsonReq).toJson());

    connect(dupReply, &QNetworkReply::finished, this, [=]() {
        if (dupReply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", dupReply->errorString());
            dupReply->deleteLater();
            return;
        }

        QByteArray resp = dupReply->readAll();
        dupReply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(resp);
        if (!jsonDoc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid duplicate check response.");
            return;
        }

        QJsonObject obj = jsonDoc.object();
        if (obj["status"].toString() != "success") {
            QMessageBox::warning(this, "Error", "Duplicate check failed.");
            return;
        }

        // Step 3: Handle duplicates before upload
        QJsonArray dupArray = obj["duplicates"].toArray();
        QStringList duplicates;
        for (auto v : dupArray) duplicates << v.toString();

        if (!duplicates.isEmpty()) {
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
        }

        // Step 4: Prepare bulk upload with Excel and ZIP files
        cancelUpload = false;
        ui->cancelUploadBtn->setEnabled(true);
        ui->updateDatabaseBtn->setEnabled(false);

        ui->bulkProgressBar->setVisible(true);
        ui->bulkProgressBar->setMinimum(0);
        ui->bulkProgressBar->setMaximum(0); // Indeterminate progress during upload
        ui->bulkProgressBar->setValue(0);

        // Create multipart form data
        QNetworkRequest uploadRequest(ApiConfig::endpoint("upload_students_zip.php"));
        QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        // Excel file part
        QHttpPart excelPart;
        excelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QVariant("form-data; name=\"excel\"; filename=\"" +
                                     QFileInfo(selectedExcelPath).fileName() + "\""));
        QFile *excelFile = new QFile(selectedExcelPath);
        if (!excelFile->open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Cannot open Excel file.");
            delete excelFile;
            delete multiPart;
            ui->bulkProgressBar->setVisible(false);
            ui->updateDatabaseBtn->setEnabled(true);
            ui->cancelUploadBtn->setEnabled(false);
            return;
        }
        excelPart.setBodyDevice(excelFile);
        excelFile->setParent(multiPart);
        multiPart->append(excelPart);

        // ZIP file part (optional)
        if (!selectedZipPath.isEmpty()) {
            QHttpPart zipPart;
            zipPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                              QVariant("form-data; name=\"photos_zip\"; filename=\"" +
                                       QFileInfo(selectedZipPath).fileName() + "\""));
            QFile *zipFile = new QFile(selectedZipPath);
            if (!zipFile->open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, "Warning", "Cannot open ZIP file. Proceeding without photos.");
            } else {
                zipPart.setBodyDevice(zipFile);
                zipFile->setParent(multiPart);
                multiPart->append(zipPart);
            }
        }

        // Add duplicates list to skip them server-side
        if (!duplicates.isEmpty()) {
            QHttpPart dupPart;
            dupPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                              QVariant("form-data; name=\"skip_ids\""));
            dupPart.setBody(duplicates.join(",").toUtf8());
            multiPart->append(dupPart);
        }

        // Send the upload request
        QNetworkReply *uploadReply = networkManager->post(uploadRequest, multiPart);
        multiPart->setParent(uploadReply);

        connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply]() {
            ui->bulkProgressBar->setVisible(false);
            ui->updateDatabaseBtn->setEnabled(true);
            ui->cancelUploadBtn->setEnabled(false);

            if (uploadReply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Upload Failed", uploadReply->errorString());
                uploadReply->deleteLater();
                return;
            }

            QByteArray response = uploadReply->readAll();
            uploadReply->deleteLater();

            // Parse JSON response
            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            if (jsonDoc.isObject()) {
                QJsonObject obj = jsonDoc.object();
                QString status = obj["status"].toString();
                QString message = obj["message"].toString();
                int successCount = obj["success_count"].toInt();
                int errorCount = obj["error_count"].toInt();

                if (status == "success") {
                    QMessageBox::information(this, "Upload Complete",
                                             QString("Bulk upload finished!\n\n"
                                                     "Successfully inserted: %1\n"
                                                     "Errors/Skipped: %2\n\n%3")
                                                 .arg(successCount)
                                                 .arg(errorCount)
                                                 .arg(message));

                    // Clear the table after successful upload
                    ui->bulkTable->setRowCount(0);
                    selectedExcelPath.clear();
                    selectedZipPath.clear();
                } else {
                    QMessageBox::warning(this, "Upload Issue", message);
                }
            } else {
                // Plain text response fallback
                QMessageBox::information(this, "Upload Complete", QString::fromUtf8(response));
            }
        });

        // Handle upload progress (optional)
        connect(uploadReply, &QNetworkReply::uploadProgress, this,
                [this](qint64 bytesSent, qint64 bytesTotal) {
                    if (bytesTotal > 0) {
                        int progress = (bytesSent * 100) / bytesTotal;
                        ui->bulkProgressBar->setMaximum(100);
                        ui->bulkProgressBar->setValue(progress);
                    }
                });
    });
}

void adminWindow::onCancelUploadBtnClicked()
{
    cancelUpload = true;
    QMessageBox::information(this, "Cancelled", "Bulk upload has been cancelled.");
    ui->bulkTable->clearContents();
}

static QString normalizeHeader(const QString &h) {
    QString s = h.trimmed().toLower();
    s.remove(' ');
    s.remove('_');
    s.remove('-');
    return s;
}

void adminWindow::loadExcelToTable(const QString &filePath)
{
    QXlsx::Document xlsx(filePath);
    if (!xlsx.isLoadPackage()) {
        QMessageBox::warning(this, "Error", "Failed to open Excel file.");
        return;
    }

    const QStringList sheets = xlsx.sheetNames();
    if (sheets.isEmpty()) {
        QMessageBox::warning(this, "Error", "This workbook has no sheets.");
        return;
    }
    xlsx.selectSheet(sheets.first());

    QXlsx::CellRange rng = xlsx.dimension();
    if (!rng.isValid()) {
        QMessageBox::information(this, "Excel", "Sheet is empty.");
        ui->bulkTable->clear();
        ui->bulkTable->setRowCount(0);
        ui->bulkTable->setColumnCount(0);
        return;
    }

    const int firstRow = rng.firstRow();
    const int lastRow  = rng.lastRow();
    const int firstCol = rng.firstColumn();
    const int lastCol  = rng.lastColumn();

    // Read header row (firstRow)
    QStringList headers;
    bulkHeaderIndex.clear();
    for (int c = firstCol; c <= lastCol; ++c) {
        QVariant hv = xlsx.read(firstRow, c);
        QString hdr = hv.toString();
        headers << hdr;

        QString n = normalizeHeader(hdr);
        int tableCol = c - firstCol; // zero-based column index for ui->bulkTable

        if (n.contains("schoolid") || n.contains("studentid") || (n == "id"))
            bulkHeaderIndex["school_id"] = tableCol;
        else if (n.contains("name") || n.contains("fullname") || n.contains("full"))
            bulkHeaderIndex["name"] = tableCol;
        else if (n.contains("code") || n.contains("studentcode"))
            bulkHeaderIndex["code"] = tableCol;
        else if (n.contains("course"))
            bulkHeaderIndex["course"] = tableCol;
        else if (n.contains("year"))
            bulkHeaderIndex["year_level"] = tableCol;
        else if (n.contains("department") || n.contains("dept"))
            bulkHeaderIndex["department"] = tableCol;
        else if (n.contains("gender"))
            bulkHeaderIndex["gender"] = tableCol;
        else if (n.contains("status"))
            bulkHeaderIndex["status"] = tableCol;
        else if (n.contains("visit"))
            bulkHeaderIndex["visits"] = tableCol;
        else
            bulkHeaderIndex[QString("col_%1").arg(tableCol)] = tableCol;
    }

    const int rows = lastRow - firstRow; // exclude header row
    const int cols = lastCol - firstCol + 1;

    ui->bulkTable->clear();
    ui->bulkTable->setRowCount(rows);
    ui->bulkTable->setColumnCount(cols);
    ui->bulkTable->setHorizontalHeaderLabels(headers);

    for (int r = firstRow + 1; r <= lastRow; ++r) {
        int rowIndex = r - firstRow - 1;
        for (int c = firstCol; c <= lastCol; ++c) {
            QVariant val = xlsx.read(r, c);
            ui->bulkTable->setItem(rowIndex, c - firstCol, new QTableWidgetItem(val.toString()));
        }
    }

    ui->bulkTable->resizeColumnsToContents();
}

void adminWindow::loadCSVtoTable(const QString &filePath){
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + filePath);
        return;
    }

    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    ui->bulkTable->clear();
    ui->bulkTable->setRowCount(0);

    if (lines.isEmpty()) {
        QMessageBox::information(this, "CSV Preview", "File is empty.");
        return;
    }

    // Header line
    QStringList headers = lines.first().split(",", Qt::SkipEmptyParts);
    for (int i = 0; i < headers.size(); ++i)
        headers[i] = headers[i].trimmed();

    if (headers.isEmpty()) {
        QMessageBox::warning(this, "CSV Error", "CSV file has no headers.");
        return;
    }

    bulkHeaderIndex.clear();
    for (int c = 0; c < headers.size(); ++c) {
        QString n = normalizeHeader(headers[c]);
        if (n.contains("schoolid") || n.contains("studentid") || (n == "id"))
            bulkHeaderIndex["school_id"] = c;
        else if (n.contains("name") || n.contains("fullname") || n.contains("full"))
            bulkHeaderIndex["name"] = c;
        else if (n.contains("code") || n.contains("studentcode"))
            bulkHeaderIndex["code"] = c;
        else if (n.contains("course"))
            bulkHeaderIndex["course"] = c;
        else if (n.contains("year"))
            bulkHeaderIndex["year_level"] = c;
        else if (n.contains("department") || n.contains("dept"))
            bulkHeaderIndex["department"] = c;
        else if (n.contains("gender"))
            bulkHeaderIndex["gender"] = c;
        else if (n.contains("status"))
            bulkHeaderIndex["status"] = c;
        else if (n.contains("visit"))
            bulkHeaderIndex["visits"] = c;
        else
            bulkHeaderIndex[QString("col_%1").arg(c)] = c;
    }

    // Add rows (skip header)
    for (int i = 1; i < lines.size(); ++i) {
        QStringList rowData = lines[i].split(",");

        // ADD this check to prevent crashes on empty lines:
        if (rowData.isEmpty() || (rowData.size() == 1 && rowData.first().trimmed().isEmpty())) {
            continue; // Skip empty lines
        }

        int row = ui->bulkTable->rowCount();
        ui->bulkTable->insertRow(row);

        for (int j = 0; j < rowData.size() && j < ui->bulkTable->columnCount(); j++) {
            ui->bulkTable->setItem(row, j, new QTableWidgetItem(rowData[j].trimmed()));
        }
    }

    ui->bulkTable->setColumnCount(qMax(ui->bulkTable->columnCount(), headers.size()));
    ui->bulkTable->setHorizontalHeaderLabels(headers);
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
    QFont previewFont = ui->fontComboBox->currentFont();
    previewFont.setPointSize(ui->spinBox->value());
    ui->schoolText_previewLabel->setFont(previewFont);
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
    if (!m_settingsController->save(m_currentSettings))
        return;

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

void adminWindow::postReportData(const QJsonObject &filters,
                                 std::function<void(const QJsonArray &)> onData)
{
    QUrl url = ApiConfig::endpoint("get_report_data.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(filters).toJson());
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", reply->errorString());
            reply->deleteLater();
            return;
        }
        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid response from server.");
            return;
        }
        QJsonObject obj = doc.object();
        if (obj["status"].toString() != "success") {
            QMessageBox::warning(this, "Error", obj["message"].toString());
            return;
        }
        onData(obj["data"].toArray());
    });
}

void adminWindow::onGeneratePDFBtnClicked() {
    QJsonObject filters = collectReportFilters(true); // show warnings if invalid
    if (filters.isEmpty()) return; // stop if validation failed
    emit reportFiltersReady(filters);
    qDebug() << "Generate PDF filters:" << filters;

    postReportData(filters, [=](const QJsonArray &data) {
        exportReportToPDF(data, filters);
    });
}

void adminWindow::onGenerateExcelBtnClicked() {
    QJsonObject filters = collectReportFilters(true); // show warnings if invalid
    if (filters.isEmpty()) return; // stop if validation failed

    emit reportFiltersReady(filters);
    qDebug() << "Generate Excel filters:" << filters;

    postReportData(filters, [=](const QJsonArray &rows) {
        exportReportToExcel(rows, filters);
    });
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

    // --- Duration switch ---
    switch (ui->durationTypeBox->currentIndex()) {
    case 0: { // Day
        QDate date = ui->dateEdit->date();
        if (!date.isValid()) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a valid date.");
            return {};
        }
        filters["start"] = date.toString("yyyy-MM-dd");
        filters["end"]   = date.toString("yyyy-MM-dd");
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
        QDate start(year, month, 1);
        QDate end = start.addMonths(1).addDays(-1);
        filters["start"] = start.toString("yyyy-MM-dd");
        filters["end"]   = end.toString("yyyy-MM-dd");
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
        if (sem.contains("1")) {
            filters["start"] = QString("%1-01-01").arg(year);
            filters["end"]   = QString("%1-06-30").arg(year);
        } else {
            filters["start"] = QString("%1-07-01").arg(year);
            filters["end"]   = QString("%1-12-31").arg(year);
        }
        filters["semester"] = sem;
        filters["year"]     = year;
        filters["durationType"] = "semester";
        break;
    }
    case 3: { // Custom
        QDate start = ui->startDateEdit->date();
        QDate end   = ui->endDateEdit->date();
        if (!start.isValid() || !end.isValid() || start > end) {
            if (validateFilters)
                QMessageBox::warning(this, "Invalid Input", "Please select a valid date range.");
            return {};
        }
        filters["start"] = start.toString("yyyy-MM-dd");
        filters["end"]   = end.toString("yyyy-MM-dd");
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



void adminWindow::loadFilterDepartments() {
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

        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid server response.");
            return;
        }

        QJsonObject obj = doc.object();
        if (obj["status"].toString() == "success") {
            ui->filterDepartmentBox->clear();
            ui->filterDepartmentBox->addItem("-- Select Department --");

            QJsonArray deptArray = obj["departments"].toArray();
            for (auto v : deptArray) {
                ui->filterDepartmentBox->addItem(v.toString());
            }
        } else {
            QMessageBox::warning(this, "Error", "Failed to load departments.");
        }
    });
}

void adminWindow::loadAvailableYears() {
    QUrl url = ApiConfig::endpoint("get_years.php");
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

        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (!doc.isObject()) return;

        QJsonObject obj = doc.object();
        if (obj["status"].toString() == "success") {
            ui->yearComboBox->clear();
            ui->semYearComboBox->clear();
            QJsonArray years = obj["years"].toArray();
            for (auto v : years) {
                QString yearStr = QString::number(v.toInt());
                ui->yearComboBox->addItem(yearStr);
                ui->semYearComboBox->addItem(yearStr);
            }
        } else {
            QMessageBox::warning(this, "Error", obj["message"].toString());
        }
    });
}

void adminWindow::onFilterDepartmentBoxCurrentIndexChanged(int index)
{
    if (index <= 0) {
        ui->filterCourseBox->clear();
        ui->filterCourseBox->addItem("All Courses");
        return;
    }

    QString department = ui->filterDepartmentBox->currentText();

    QUrl url = ApiConfig::endpoint("get_courses.php");
    QUrlQuery query;
    query.addQueryItem("department", department);
    query.addQueryItem("include_all", "true"); // ✅ Request "All" option
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid response from server.");
            return;
        }

        QJsonObject obj = doc.object();
        if (obj["status"].toString() == "success") {
            ui->filterCourseBox->clear();

            QJsonArray courses = obj["courses"].toArray();
            for (auto v : courses) {
                ui->filterCourseBox->addItem(v.toString());
            }
        } else {
            QMessageBox::warning(this, "Error", obj["message"].toString());
        }
    });
}

void adminWindow::fetchReportData(const QJsonObject &filters) {
    QUrl url = ApiConfig::endpoint("get_report_data.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(filters).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid server response.");
            return;
        }

        QJsonObject obj = doc.object();
        if (obj["status"].toString() != "success") {
            QMessageBox::warning(this, "Error", obj["message"].toString());
            return;
        }

        // ✅ Build a preview string (like HTML table)
        QJsonArray rows = obj["data"].toArray();
        updateChartsPreview(rows);
        QString html = "<h2>Report Preview</h2>";

        if (rows.isEmpty()) {
            html += "<p><i>No data found for the selected filters.</i></p>";
        } else {
            // Table headers
            QJsonObject firstRow = rows.first().toObject();
            QStringList headers = { "school_id", "name", "gender", "status",
                                   "course", "department", "year_level", "visits" };

            html += "<table border='1' cellspacing='0' cellpadding='4'>";
            html += "<tr>";
            for (const QString &h : headers) {
                // Pretty headers
                QString pretty = h;
                if (h == "school_id") pretty = "School ID";
                else if (h == "name") pretty = "Name";
                else if (h == "gender") pretty = "Gender";
                else if (h == "status") pretty = "Status";
                else if (h == "course") pretty = "Course";
                else if (h == "department") pretty = "Department";
                else if (h == "year_level") pretty = "Year Level";
                else if (h == "visits") pretty = "Total Visits";

                html += "<th>" + pretty + "</th>";
            }
            html += "</tr>";

            // Table rows
            for (const auto &val : rows) {
                QJsonObject row = val.toObject();
                html += "<tr>";
                for (const QString &h : headers) {
                    html += "<td>" + row[h].toVariant().toString() + "</td>";
                }
                html += "</tr>";
            }
            html += "</table>";
        }

        // ✅ Show in popup preview dialog
        ReportPreviewDialog *preview = new ReportPreviewDialog(this);
        preview->setHtml(html);
        preview->exec();
    });
}

bool adminWindow::paintReport(QPagedPaintDevice *device, int resolution,
                              const QJsonArray &data, const QJsonObject &filters)
{
    QPainter painter;
    if (!painter.begin(device)) {
        return false;
    }

    auto finalize = qScopeGuard([&]() {
        if (painter.isActive()) {
            painter.end();
            qDebug() << "Report paint finalized successfully.";
        }
    });

    auto safeText = [](const QString &s) -> QString {
        QString clean = s;
        clean.replace(QChar(0xFFFD), "?");
        return clean;
    };

    QRectF pageRect = device->pageLayout().paintRectPixels(resolution);
    int pageWidth  = pageRect.width();
    int pageHeight = pageRect.height();
    int margin     = pageWidth * 0.03;
    int usableWidth  = pageWidth - 2*margin;
    int usableHeight = pageHeight - 2*margin;
    int y = margin;
    int currentPage = 1;
    QSettings settings("MyCompany", "MyApp");
    QString librarian  = settings.value("admin/name", "").toString();
    QString position   = settings.value("admin/position", "").toString();

    auto drawFooter = [&](int pageNum) {
        QFont footerFont("Arial", 8);
        footerFont.setItalic(true);
        painter.setFont(footerFont);
        painter.setPen(Qt::black);

        // Left side: system-generated text
        painter.drawText(QRect(margin, pageHeight - margin - 20,
                               usableWidth, 20),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         "This is a system generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");

        // Right side: page number
        QString footerText = QString("Page %1").arg(pageNum);
        painter.drawText(QRect(margin, pageHeight - margin - 20,
                               usableWidth, 20),
                         Qt::AlignRight | Qt::AlignVCenter, footerText);
    };


    qDebug() << "Paint Width:" << pageWidth << "Height:" << pageHeight;
    qDebug() << "Calculated margin:" << margin;
    qDebug() << "Paint Resolution:" << resolution;

    // ===== HEADER =====
    auto drawHeader = [&](int &y) {
        QSettings settings("MyCompany", "MyApp");
        QString schoolName = settings.value("school/name", "Your School Name").toString();
        QString address    = settings.value("school/address", "Your Address").toString();
        QString logoPath   = settings.value("school/logoPath", "").toString();

        int logoSize = pageWidth * 0.08;
        if (!logoPath.isEmpty()) {
            QPixmap logo(logoPath);
            if (!logo.isNull()) {
                QPixmap scaledLogo = logo.scaled(logoSize, logoSize,
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);
                painter.drawPixmap(QRect(margin, y, logoSize, logoSize),
                                   scaledLogo, scaledLogo.rect());
            }
        }

        int textLeft = margin + logoSize + 15;
        int textWidth = usableWidth - logoSize - 15;

        painter.setFont(QFont("Times New Roman", 16, QFont::Bold));
        painter.drawText(QRect(textLeft, y, textWidth, 30),
                         Qt::AlignLeft | Qt::AlignVCenter, safeText(schoolName));

        painter.setFont(QFont("Times New Roman", 11));
        painter.drawText(QRect(textLeft, y + 25, textWidth, 30),
                         Qt::AlignLeft | Qt::AlignVCenter, safeText(address));

        QString dateStr = QDate::currentDate().toString("dddd, MMMM d, yyyy");
        QString timeStr = QTime::currentTime().toString("hh:mm:ss AP");
        painter.setFont(QFont("Arial", 9));
        painter.drawText(QRect(margin, y, usableWidth, 20), Qt::AlignRight, dateStr);
        painter.drawText(QRect(margin, y + 15, usableWidth, 20), Qt::AlignRight, timeStr);

        // Line under header
        y += logoSize + 20;
        painter.setPen(Qt::black);
        painter.drawLine(margin, y, pageWidth - margin, y);
        y += 30;  // spacing after header
    };
    drawHeader(y);


    // ===== FILTERS =====
    painter.setFont(QFont("Arial", 10));
    QString filtersLine = QString("Department: %1 | Course: %2 | Period: %3 - %4 | School Year: %5")
                              .arg(safeText(filters["department"].toString()))
                              .arg(safeText(filters["course"].toString()))
                              .arg(safeText(filters["start"].toString()))
                              .arg(safeText(filters["end"].toString()))
                              .arg(safeText(filters["schoolYear"].toString()));
    painter.drawText(QRect(margin, y, usableWidth, 30), Qt::AlignLeft, filtersLine);
    y += 40;

    // ===== TABLE =====
    ReportPalette palette = getPalette(filters["palette"].toString());

    // --- Define column widths (8 columns total) ---
    int col1 = margin;                                   // School ID
    int col2 = margin + (usableWidth * 0.12);            // Name
    int col3 = margin + (usableWidth * 0.32);            // Gender
    int col4 = margin + (usableWidth * 0.42);            // Status
    int col5 = margin + (usableWidth * 0.55);            // Course
    int col6 = margin + (usableWidth * 0.70);            // Department
    int col7 = margin + (usableWidth * 0.85);            // Year Level
    int col8 = margin + (usableWidth * 0.95);            // Visits

    // --- Draw header row ---
    painter.fillRect(QRect(margin, y - 15, usableWidth, 20), palette.headerBg);
    painter.setPen(palette.headerText);

    painter.drawText(col1, y, "School ID");
    painter.drawText(col2, y, "Name");
    painter.drawText(col3, y, "Gender");
    painter.drawText(col4, y, "Status");
    painter.drawText(col5, y, "Course");
    painter.drawText(col6, y, "Department");
    painter.drawText(col7, y, "Year Level");
    painter.drawText(col8, y, "Visits");

    y += 25;
    painter.setPen(Qt::black);
    painter.drawLine(margin, y, pageWidth - margin, y);
    y += 20;

    qDebug() << "Starting to draw" << data.size() << "table rows";

    int rowIndex = 0;
    for (auto v : data) {
        QJsonObject row = v.toObject();
        if (rowIndex < 3) qDebug() << "Drawing row" << rowIndex << ":" << row;

        QRect rowRect(margin, y - 15, usableWidth, 20);
        painter.fillRect(rowRect, (rowIndex % 2 == 0) ? palette.rowEvenBg : palette.rowOddBg);

        painter.setPen(palette.rowText);
        QFontMetrics fm = painter.fontMetrics();

        QString schoolId   = fm.elidedText(safeText(row["school_id"].toString()), Qt::ElideRight, col2 - col1 - 5);
        QString name       = fm.elidedText(safeText(row["name"].toString()), Qt::ElideRight, col3 - col2 - 5);
        QString gender     = fm.elidedText(safeText(row["gender"].toString()), Qt::ElideRight, col4 - col3 - 5);
        QString status     = fm.elidedText(safeText(row["status"].toString()), Qt::ElideRight, col5 - col4 - 5);
        QString course     = fm.elidedText(safeText(row["course"].toString()), Qt::ElideRight, col6 - col5 - 5);
        QString department = fm.elidedText(safeText(row["department"].toString()), Qt::ElideRight, col7 - col6 - 5);
        QString yearLevel  = fm.elidedText(safeText(row["year_level"].toString()), Qt::ElideRight, col8 - col7 - 5);

        painter.drawText(col1, y, schoolId);
        painter.drawText(col2, y, name);
        painter.drawText(col3, y, gender);
        painter.drawText(col4, y, status);
        painter.drawText(col5, y, course);
        painter.drawText(col6, y, department);
        painter.drawText(col7, y, yearLevel);
        painter.drawText(col8, y, QString::number(row["visits"].toInt()));

        y += 20;
        rowIndex++;

        if (y > usableHeight - 200) {
            drawFooter(currentPage);
            device->newPage();
            currentPage++;
            y = margin;
            drawHeader(y);
        }
    }

    // ===== CHARTS: each chart placed on its own page and scaled to fill almost whole page =====
    auto drawFullscreenChart = [&](const QString &label, const QImage &img) {
        if (img.isNull()) {
            qDebug() << label << "is null, skipping.";
            return;
        }

        drawFooter(currentPage);
        device->newPage();
        currentPage++;
        y = margin;        // reset Y for the new page
        drawHeader(y);
        qDebug() << "New page created for chart (" << label << "), page:" << currentPage;

        // Compute area for chart (leave space for footer)
        const int bottomReserve = 60;
        QRect targetArea(margin, y, usableWidth, pageHeight - y - margin - bottomReserve);

        // Scale preserving aspect ratio
        QSize scaledSize = img.size().scaled(targetArea.size(), Qt::KeepAspectRatio);

        // Center in target area
        int x = targetArea.left() + (targetArea.width() - scaledSize.width()) / 2;
        int y = targetArea.top() + (targetArea.height() - scaledSize.height()) / 2;
        QRect drawRect(x, y, scaledSize.width(), scaledSize.height());

        // Draw the image at the calculated rect
        painter.drawImage(drawRect, img);

        qDebug() << "Chart" << label << "drawn at rect:" << drawRect
                 << "from image size:" << img.size();
    };

    QString chartChoice = filters["chartType"].toString();

    // request chart images sized to the full usable area (so the chart is rendered at that resolution)
    // Before calling chart generation functions, calculate appropriate sizes

    if (chartChoice.contains("All", Qt::CaseInsensitive)) {
        QSize barSize(usableWidth, 600);  // Wide rectangle for bar charts
        QSize pieSize(700, 700);          // Square for pie charts
        QSize lineSize(usableWidth, 600); // Wide rectangle for line charts

        drawFullscreenChart("Bar Chart",  makeBarChartImage(data, barSize, palette));
        drawFullscreenChart("Pie Chart",  makePieChartImage(data, pieSize, palette));
        drawFullscreenChart("Line Chart", makeLineChartImage(data, lineSize, palette));
    } else if (chartChoice.contains("Pie", Qt::CaseInsensitive)) {
        QSize pieSize(700, 700);  // Square dimensions
        drawFullscreenChart("Pie Chart", makePieChartImage(data, pieSize, palette));
    } else {
        // For bar and line charts, use rectangular size
        QSize rectSize(usableWidth, 600);
        if (chartChoice.contains("Bar", Qt::CaseInsensitive)) {
            drawFullscreenChart("Bar Chart", makeBarChartImage(data, rectSize, palette));
        } else if (chartChoice.contains("Line", Qt::CaseInsensitive)) {
            drawFullscreenChart("Line Chart", makeLineChartImage(data, rectSize, palette));
        }
    }

    // Footer on the last page with current page number
    drawFooter(currentPage);

    // ===== PREPARED BY =====
    device->newPage();
    currentPage++;

    y = margin + 100;
    drawHeader(y);

    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(QRect(margin, y, pageWidth - 2*margin, 25),
                     Qt::AlignCenter, QString("Prepared by: %1").arg(safeText(librarian)));
    y += 25;

    painter.setFont(QFont("Arial", 10));
    painter.drawText(QRect(margin, y, pageWidth - 2*margin, 20),
                     Qt::AlignCenter, safeText(position));
    y += 40;

    int sigWidth = 240;
    int sigX = (pageWidth - sigWidth) / 2;
    painter.drawLine(sigX, y, sigX + sigWidth, y);
    painter.drawText(QRect(sigX, y + 5, sigWidth, 20), Qt::AlignCenter, "(Signature)");
    drawFooter(currentPage);

    return true;
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
    // Increase resolution for crisper charts in the PDF
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(150); // -> good balance quality/filesize; increase to 300 if you want very high DPI
    pdf.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    if (!paintReport(&pdf, 150, data, filters)) {
        QMessageBox::critical(this, "Error", "Failed to open PDF for writing.");
        return;
    }

    QMessageBox::information(this, "Success", "Report exported to PDF successfully.");
}

void adminWindow::onPrintReportBtnClicked()
{
    QJsonObject filters = collectReportFilters(true); // show warnings if invalid
    if (filters.isEmpty()) return; // stop if validation failed
    emit reportFiltersReady(filters);
    qDebug() << "Print Report filters:" << filters;

    postReportData(filters, [=](const QJsonArray &data) {
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

        if (!paintReport(&printer, printer.resolution(), data, filters)) {
            QMessageBox::critical(this, "Error", "Failed to start painting to printer.");
        }
    });
}

void adminWindow::exportReportToExcel(const QJsonArray &rows, const QJsonObject &filters)
{
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

    // ===== HEADER =====
    QSettings settings("MyCompany", "MyApp");
    QString schoolName = settings.value("school/name", "Your School Name").toString();
    QString address    = settings.value("school/address", "Your Address").toString();
    QString librarian  = settings.value("admin/name", "").toString();
    QString position   = settings.value("admin/position", "").toString();

    QXlsx::Format titleFmt;
    titleFmt.setFontBold(true);
    titleFmt.setFontSize(16);
    titleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format subTitleFmt;
    subTitleFmt.setFontSize(11);
    subTitleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    int row = 1;
    int colCount = 8; // ID, Name, Gender, Course, Year Level, Department, Status, Visits

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), titleFmt);
    xlsx.write(row++, 1, schoolName, titleFmt);

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, address, subTitleFmt);

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, QString("Library Report - %1 to %2")
                             .arg(filters["start"].toString(), filters["end"].toString()), subTitleFmt);
    row += 1;

    // ===== FILTERS =====
    xlsx.write(row++, 1, QString("Department: %1 | Course: %2 | School Year: %3")
                             .arg(filters["department"].toString(),
                                  filters["course"].toString(),
                                  filters["schoolYear"].toString()));

    row += 1;

    // ===== TABLE HEADERS =====
    QStringList headers = {"School ID", "Name", "Gender", "Course",
                           "Year Level", "Department", "Status", "Visits"};

    QXlsx::Format hdrFmt;
    hdrFmt.setFontBold(true);
    hdrFmt.setPatternBackgroundColor(QColor("#D6EAF8"));
    hdrFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    for (int c = 0; c < headers.size(); ++c) {
        xlsx.write(row, c + 1, headers[c], hdrFmt);
    }

    // Freeze the header row
    //xlsx.currentWorksheet()->freezePane(QXlsx::CellRange(row + 1, 1, row + 1, 1));
    row++;

    // ===== TABLE ROWS =====
    QXlsx::Format evenFmt, oddFmt;
    evenFmt.setPatternBackgroundColor(QColor("#F9F9F9"));
    oddFmt.setPatternBackgroundColor(QColor("#FFFFFF"));

    for (const auto &val : rows) {
        QJsonObject obj = val.toObject();
        QStringList rowData = {
            obj["school_id"].toString(),
            obj["name"].toString(),
            obj["gender"].toString(),
            obj["course"].toString(),
            obj["year_level"].toString(),
            obj["department"].toString(),
            obj["status"].toString(),
            QString::number(obj["visits"].toInt())
        };

        for (int c = 0; c < rowData.size(); ++c) {
            xlsx.write(row, c + 1, rowData[c], (row % 2 == 0) ? evenFmt : oddFmt);
        }
        row++;
    }

    // Auto-fit columns (simulate by setting width based on text length)
    for (int c = 0; c < headers.size(); ++c) {
        xlsx.setColumnWidth(c + 1, headers[c].length() + 5);
    }

    // ===== FOOTER =====
    row += 2;
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount));
    xlsx.write(row++, 1,
               "This is a system-generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");

    row += 2;
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount));
    xlsx.write(row++, 1, QString("Prepared by: %1").arg(librarian));

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount));
    xlsx.write(row++, 1, position);

    // ===== SAVE FILE =====
    if (!xlsx.saveAs(filePath)) {
        QMessageBox::critical(this, "Error", "Failed to save Excel file.");
    } else {
        QMessageBox::information(this, "Success", "Report exported to Excel successfully.");
    }
}



ReportPalette adminWindow::getPalette(const QString &choice) {
    if (choice == "Blue") {
        return {
            QColor("#2E86C1"), Qt::white, QColor("#EBF5FB"), Qt::white, Qt::black,
            { QColor("#0D47A1"), QColor("#1565C0"), QColor("#1976D2"),
             QColor("#1E88E5"), QColor("#42A5F5"), QColor("#64B5F6"),
             QColor("#90CAF9"), QColor("#BBDEFB") }
        };
    } else if (choice == "Green") {
        return {
            QColor("#27AE60"), Qt::white, QColor("#E9F7EF"), Qt::white, Qt::black,
            { QColor("#1B5E20"), QColor("#2E7D32"), QColor("#388E3C"),
             QColor("#43A047"), QColor("#66BB6A"), QColor("#81C784"),
             QColor("#A5D6A7"), QColor("#C8E6C9") }
        };
    } else if (choice == "Red") {
        return {
            QColor("#C0392B"), Qt::white, QColor("#FDEDEC"), Qt::white, Qt::black,
            { QColor("#7B241C"), QColor("#922B21"), QColor("#A93226"),
             QColor("#C0392B"), QColor("#CD6155"), QColor("#E6B0AA"),
             QColor("#F5B7B1"), QColor("#FADBD8") }
        };
    } else { // Default (multi-color)
        return {
            QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
            { QColor("#1f77b4"), QColor("#ff7f0e"), QColor("#2ca02c"),
             QColor("#d62728"), QColor("#9467bd"), QColor("#8c564b"),
             QColor("#e377c2"), QColor("#7f7f7f"), QColor("#bcbd22"), QColor("#17becf") }
        };
    }
}


void adminWindow::updatePalettePreview(const QString &choice) {
    ReportPalette palette = getPalette(choice);

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
    ReportPalette pal = getPalette(ui->paletteComboBox->currentText());
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

// Fetch preview data from API
void adminWindow::fetchPreviewData(const QJsonObject &filters)
{
    QUrl url = ApiConfig::endpoint("api.php/reports/data");  // ✅ Using API router
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(filters).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Preview fetch error:" << reply->errorString();
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (doc.isObject() && doc.object()["status"].toString() == "success") {
            QJsonArray data = doc.object()["data"].toArray();
            updateChartsPreview(data);
        }
    });
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
                    fetchPreviewData(filters);
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
                    fetchPreviewData(filters);
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
                    fetchPreviewData(filters);
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
                        fetchPreviewData(filters);
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

        QJsonArray updates;

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

            QJsonObject student;
            student["school_id"]  = schoolId;
            student["code"]       = getText(1);
            student["name"]       = getText(3);
            student["department"] = getText(5);
            student["course"]     = getText(4);
            student["year_level"] = getText(6);
            student["gender"]     = getText(7);
            student["status"]     = getText(8);

            updates.append(student);
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

void adminWindow::bulkUpdateStudents(const QJsonArray &updates)
{
    QUrl url = ApiConfig::endpoint("bulk_update_students.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject data;
    data["students"] = updates;

    QNetworkReply *reply = networkManager->post(request,
                                                QJsonDocument(data).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        // Re-enable button regardless of outcome
        ui->editStudentBtn->setText("Edit");
        ui->editStudentBtn->setEnabled(true);
        ui->cancelEditBtn->setVisible(false);
        ui->selectAllBtn->setVisible(false);
        ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        clearCheckboxes();

        if (reply->error() != QNetworkReply::NoError) {
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("❌ Network Error\n%1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QJsonObject obj = doc.object();

        if (obj["status"].toString() == "success") {
            int updatedCount = obj["updated"].toInt();

            // ✅ Success overlay
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("✅ %1 student%2 updated successfully")
                                     .arg(updatedCount)
                                     .arg(updatedCount == 1 ? "" : "s"));

            // Show errors if any
            if (obj.contains("errors") && !obj["errors"].toArray().isEmpty()) {
                QJsonArray errors = obj["errors"].toArray();
                QString errorList;
                for (const auto &err : errors) {
                    errorList += err.toString() + "\n";
                }

                QTimer::singleShot(2500, this, [=]() {
                    QMessageBox::warning(this, "Some Updates Failed", errorList);
                });
            }

            performStudentSearch(false); // ✅ Refresh table silently (no overlay)
        } else {
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("⚠️ Update Failed\n%1").arg(obj["message"].toString()));
        }
    });
}

// Add cancel button handler
void adminWindow::onCancelEditBtnClicked()
{
    ui->editStudentBtn->setText("Edit");
    ui->editStudentBtn->setEnabled(true);
    ui->cancelEditBtn->setVisible(false);
    ui->selectAllBtn->setVisible(false);
    ui->studentSearchTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clearCheckboxes();

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
    // Show loading indicator
    ui->searchLoadingBar->setVisible(true);
    ui->searchLoadingBar->setValue(0);

    // Collect filters
    QString searchText = ui->searchLineEdit->text().trimmed();
    QString department = ui->searchDepartmentFilter->currentText();
    QString course = ui->searchCourseFilter->currentText();

    // Prepare request
    QUrl url = ApiConfig::endpoint("search_students.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject filters;
    filters["search"] = searchText;
    filters["department"] = (department == "All" || department == "All Departments" || department == "Select Department") ? "" : department;
    filters["course"] = (course == "All" || course == "All Courses" || course == "Select Course") ? "" : course;

    // Send POST request
    QNetworkReply *reply = networkManager->post(request, QJsonDocument(filters).toJson());

    // Handle server response
    connect(reply, &QNetworkReply::finished, this, [=]() {
        ui->searchLoadingBar->setVisible(false);

        if (reply->error() != QNetworkReply::NoError) {
            // ✅ Show error overlay only if showOverlay is true
            if (showOverlay) {
                showTemporaryOverlay(ui->studentSearchPage,
                                     QString("❌ Network Error\n%1").arg(reply->errorString()));
            }
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            if (showOverlay) {
                showTemporaryOverlay(ui->studentSearchPage,
                                     "⚠️ Invalid server response");
            }
            return;
        }

        QJsonObject obj = doc.object();

        // If not successful, show message with overlay
        if (obj["status"].toString() != "success") {
            QString msg = obj.contains("message") ? obj["message"].toString() : "No students found.";

            ui->studentSearchTable->clearContents();
            ui->studentSearchTable->setRowCount(0);

            // ✅ Show overlay only if showOverlay is true
            if (showOverlay) {
                showTemporaryOverlay(ui->studentSearchPage,
                                     QString("🔍 %1").arg(msg));
            }
            return;
        }

        // Display results
        QJsonArray students = obj["students"].toArray();

        // ✅ Check if students array is empty (no results found)
        if (students.isEmpty()) {
            ui->studentSearchTable->clearContents();
            ui->studentSearchTable->setRowCount(0);

            if (showOverlay) {
                showTemporaryOverlay(ui->studentSearchPage,
                                     "🔍 No students found\nTry adjusting your search filters");
            }
            return;
        }

        // ✅ Show success overlay with count only if showOverlay is true
        if (showOverlay && students.size() > 0) {
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("✅ Found %1 student%2")
                                     .arg(students.size())
                                     .arg(students.size() == 1 ? "" : "s"));
        }
        displaySearchResults(students, obj["searchTerm"].toString());
    });
}

void adminWindow::displaySearchResults(const QJsonArray &students, const QString &searchTerm)
{
    // Block all itemChanged() signals while filling table
    ui->studentSearchTable->blockSignals(true);

    ui->studentSearchTable->clearContents();
    ui->studentSearchTable->setRowCount(students.size());

    for (int row = 0; row < students.size(); ++row) {
        QJsonObject student = students[row].toObject();

        // Leave Select column (index 0) empty for now
        ui->studentSearchTable->setItem(row, 0, new QTableWidgetItem(""));

        ui->studentSearchTable->setItem(row, 1, new QTableWidgetItem(student["code"].toString()));
        ui->studentSearchTable->setItem(row, 2, new QTableWidgetItem(student["school_id"].toString()));
        ui->studentSearchTable->setItem(row, 3, new QTableWidgetItem(student["name"].toString()));
        ui->studentSearchTable->setItem(row, 4, new QTableWidgetItem(student["course"].toString()));
        ui->studentSearchTable->setItem(row, 5, new QTableWidgetItem(student["department"].toString()));
        ui->studentSearchTable->setItem(row, 6, new QTableWidgetItem(student["year_level"].toString()));
        ui->studentSearchTable->setItem(row, 7, new QTableWidgetItem(student["gender"].toString()));
        ui->studentSearchTable->setItem(row, 8, new QTableWidgetItem(student["status"].toString()));
    }

    ui->studentSearchTable->blockSignals(false); // ✅ Re-enable signals after fill

    // --- Sync header checkbox state with row selections
    ui->selectAllBtn->setText("Select All");
}


void adminWindow::onSearchBtnClicked()
{
    performStudentSearch(true); // That's it - performStudentSearch handles everything
}

void adminWindow::deleteStudents(const QStringList &schoolIds)
{
    QUrl url = ApiConfig::endpoint("delete_students.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject data;
    QJsonArray ids;
    for (const QString &id : schoolIds) {
        ids.append(id);
    }
    data["school_ids"] = ids;

    QNetworkReply *reply = networkManager->post(request,
                                                QJsonDocument(data).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("❌ Error: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QJsonObject obj = doc.object();

        if (obj["status"].toString() == "success") {
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("✅ %1 student%2 deleted successfully")
                                     .arg(schoolIds.size())
                                     .arg(schoolIds.size() == 1 ? "" : "s"));

            performStudentSearch(false); // ✅ Refresh silently
        } else {
            showTemporaryOverlay(ui->studentSearchPage,
                                 QString("⚠️ %1").arg(obj["message"].toString()));
        }
    });
}

void adminWindow::onDeleteStudentBtnClicked()
{
    QStringList selectedIds;

    for (int row = 0; row < ui->studentSearchTable->rowCount(); ++row) {
        QWidget *widget = ui->studentSearchTable->cellWidget(row, 0);
        if (!widget) continue;

        QCheckBox *cb = widget->findChild<QCheckBox*>();
        if (cb && cb->isChecked()) {
            QString id = ui->studentSearchTable->item(row, 2)->text(); // Column 2 = School ID
            selectedIds << id;
        }
    }

    if (selectedIds.isEmpty()) {
        showTemporaryOverlay(ui->studentSearchPage,
                             "⚠️ Please select at least one student to delete");
        return;
    }

    QMessageBox::StandardButton confirm = QMessageBox::warning(
        this, "Confirm Delete",
        QString("Are you sure you want to delete %1 student(s)?\n\nThis action cannot be undone.")
            .arg(selectedIds.size()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (confirm == QMessageBox::Yes) {
        deleteStudents(selectedIds);
    }
}

void adminWindow::populateFilters()
{
    // --- Populate Departments ---
    QNetworkRequest deptRequest(ApiConfig::endpoint("get_departments.php"));
    QNetworkReply *deptReply = networkManager->get(deptRequest);

    connect(deptReply, &QNetworkReply::finished, this, [=]() {
        QByteArray resp = deptReply->readAll();
        deptReply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QJsonObject obj = doc.object();

        ui->searchDepartmentFilter->clear();
        ui->searchDepartmentFilter->addItem("Select Department");

        if (obj["status"].toString() == "success") {
            QJsonArray arr = obj["departments"].toArray();
            for (const QJsonValue &val : arr) {
                QString dept = val.toString();
                if (dept.toLower() != "all") {  // ✅ Skip "All" since we don't request it
                    ui->searchDepartmentFilter->addItem(dept);
                }
            }
        }
    });
}

void adminWindow::showSearchOverlay()
{
    if (!ui->searchOverlay) return;

    ui->searchOverlay->raise();
    ui->searchOverlay->setVisible(true);

    // Create spinner if not created yet
    if (!searchSpinner) {
        searchSpinner = new BusyIndicator(ui->searchOverlay);
        searchSpinner->setFixedSize(48, 48);
        searchSpinner->setColor(QColor(WitsTheme::Color::Secondary));
    }

    // Center spinner
    int sw = searchSpinner->width();
    int sh = searchSpinner->height();
    int ow = ui->searchOverlay->width();
    int oh = ui->searchOverlay->height();
    searchSpinner->move((ow - sw) / 2, (oh - sh) / 2 - 12);
    searchSpinner->start();

    // Fade in effect
    if (!overlayEffect) {
        overlayEffect = new QGraphicsOpacityEffect(ui->searchOverlay);
        ui->searchOverlay->setGraphicsEffect(overlayEffect);
    }

    QPropertyAnimation *fadeIn = new QPropertyAnimation(overlayEffect, "opacity", this);
    fadeIn->setDuration(250);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QPropertyAnimation::DeleteWhenStopped);
}

void adminWindow::hideSearchOverlay()
{
    if (!ui->searchOverlay) return;

    if (!overlayEffect) {
        overlayEffect = new QGraphicsOpacityEffect(ui->searchOverlay);
        ui->searchOverlay->setGraphicsEffect(overlayEffect);
        overlayEffect->setOpacity(1.0);
    }

    QPropertyAnimation *fadeOut = new QPropertyAnimation(overlayEffect, "opacity", this);
    fadeOut->setDuration(250);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
        if (searchSpinner) searchSpinner->stop();
        ui->searchOverlay->setVisible(false);
    });

    fadeOut->start(QPropertyAnimation::DeleteWhenStopped);
}

void adminWindow::loadAllStudents()
{
    QUrl url = ApiConfig::endpoint("search_students.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject filters;
    filters["search"] = "";  // Empty search = all students
    filters["department"] = "All";
    filters["course"] = "All";

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(filters).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Error", "Failed to load students: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QJsonObject obj = doc.object();

        if (obj["status"].toString() == "success") {
            QJsonArray students = obj["students"].toArray();
            displaySearchResults(students, "" ); // No highlighting
        }
    });
}

void adminWindow::onDepartmentFilterChanged(int)
{
    QString department = ui->searchDepartmentFilter->currentText();
    if (department.isEmpty() || department == "Select Department") return;

    QUrl url = ApiConfig::endpoint("get_courses.php");
    QUrlQuery query;
    query.addQueryItem("department", department);
    // ✅ Don't include "All" for search filter
    // query.addQueryItem("include_all", "false"); // Optional, false by default
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            if (obj["status"].toString() == "success") {
                ui->searchCourseFilter->clear();
                ui->searchCourseFilter->addItem("Select Course"); // ✅ Placeholder instead of "All"

                for (auto c : obj["courses"].toArray()) {
                    ui->searchCourseFilter->addItem(c.toString());
                }
            }
        }
        reply->deleteLater();
    });
}

void adminWindow::loadVisitorLogs(const QString &search, const QString &dateType,
                                  const QString &startDate, const QString &endDate)
{
    QUrl url = ApiConfig::endpoint("get_visitors.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["search"] = search;
    payload["date_type"] = dateType;
    payload["start_date"] = startDate;
    payload["end_date"] = endDate;

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Network Error", reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray resp = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            QMessageBox::warning(this, "Error", "Invalid server response.");
            return;
        }

        QJsonObject obj = doc.object();
        if (obj["status"].toString() != "success") {
            QMessageBox::warning(this, "Error", obj["message"].toString());
            return;
        }

        QJsonArray logs = obj["visitors"].toArray();
        int totalCount = obj["count"].toInt();

        // --- Populate visitor table ---
        ui->visitorTable->setRowCount(logs.size());
        ui->visitorTable->setColumnCount(5);  // ✅ Changed from 4 to 5
        QStringList headers = {"Name", "Company", "Purpose", "Date", "Time"};  // ✅ Added Date column
        ui->visitorTable->setHorizontalHeaderLabels(headers);

        for (int i = 0; i < logs.size(); ++i) {
            QJsonObject log = logs[i].toObject();

            // Parse the datetime from server
            QString timeIn = log["time_in"].toString();  // e.g., "2025-01-15 14:30:00"
            QDateTime dt = QDateTime::fromString(timeIn, "yyyy-MM-dd HH:mm:ss");

            QString dateStr = dt.isValid() ? dt.toString("yyyy-MM-dd") : "N/A";
            QString timeStr = dt.isValid() ? dt.toString("hh:mm AP") : "N/A";

            ui->visitorTable->setItem(i, 0, new QTableWidgetItem(log["name"].toString()));
            ui->visitorTable->setItem(i, 1, new QTableWidgetItem(log["company"].toString()));
            ui->visitorTable->setItem(i, 2, new QTableWidgetItem(log["purpose"].toString()));
            ui->visitorTable->setItem(i, 3, new QTableWidgetItem(dateStr));  // ✅ Date column
            ui->visitorTable->setItem(i, 4, new QTableWidgetItem(timeStr));  // ✅ Time column
        }

        ui->visitorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        ui->visitorTable->setAlternatingRowColors(true);
        ui->visitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->visitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->visitorTable->setSelectionMode(QAbstractItemView::SingleSelection);

        // ✅ Display total count
        if (ui->visitorTotalLabel) {
            ui->visitorTotalLabel->setText(QString("📊 Total Visitors: %1").arg(totalCount));
        }

        if (logs.isEmpty()) {
            ui->visitorTable->setRowCount(0);
            showTemporaryOverlay(ui->visitorTable, "No visitors found for the selected filters.");
            if (ui->visitorTotalLabel)
                ui->visitorTotalLabel->setText("📊 Total Visitors: 0");
            return;
        }
    });
}

void adminWindow::on_extractVisitorBtn_clicked() {
    if (ui->visitorTable->rowCount() == 0) {
        QMessageBox::information(this, "No Data", "There are no visitor logs to export.");
        return;
    }

    // --- Build default filename based on filters ---
    QString baseName = "VisitorLogs";
    QString dateType = ui->visitorDateFilterBox->currentText();
    QString datePart;

    if (dateType == "Specific Day") {
        datePart = ui->visitorDateEdit->date().toString("yyyy-MM-dd");
    }
    else if (dateType == "Month") {
        QString month = ui->visitorMonthCombo->currentText();
        QString year = QString::number(ui->visitorYearSpin->value());
        datePart = QString("%1_%2").arg(month, year);
    }
    else if (dateType == "Date Range") {
        QString start = ui->visitorStartDate->date().toString("yyyy-MM-dd");
        QString end = ui->visitorEndDate->date().toString("yyyy-MM-dd");
        datePart = QString("Range_%1_to_%2").arg(start, end);
    }
    else {
        datePart = "All";
    }

    QString searchTerm = ui->visitorSearchLineEdit->text().trimmed();
    if (!searchTerm.isEmpty()) {
        searchTerm.replace(" ", "_");
        baseName += "_" + searchTerm;
    }

    QString defaultFileName = QString("%1_%2.xlsx").arg(baseName, datePart);

    // --- Ask user where to save ---
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Visitor Logs",
        QDir::homePath() + "/" + defaultFileName,
        "Excel Files (*.xlsx)"
        );
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".xlsx"))
        filePath += ".xlsx";

    // --- Initialize Excel document ---
    QXlsx::Document xlsx;

    QXlsx::Format boldCenter;
    boldCenter.setFontBold(true);
    boldCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setPatternBackgroundColor(Qt::lightGray);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    QXlsx::Format normalCenter;
    normalCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    normalCenter.setBorderStyle(QXlsx::Format::BorderThin);

    // --- Header info ---
    QString schoolName = ui->schoolName->text().isEmpty() ? "School Name" : ui->schoolName->text();
    QString reportTitle = "Library Visitor Logs Report";
    QString generatedDate = QDateTime::currentDateTime().toString("MMMM dd, yyyy hh:mm AP");

    // Merge & write school name and title
    xlsx.mergeCells("A1:E1");
    xlsx.mergeCells("A2:E2");
    xlsx.write("A1", schoolName, boldCenter);
    xlsx.write("A2", reportTitle, boldCenter);

    // --- Write filter info ---
    xlsx.write("A4", "Generated On:", boldCenter);
    xlsx.write("B4", generatedDate, normalCenter);

    if (datePart != "All")
        xlsx.write("A5", "Filter Applied:", boldCenter),
            xlsx.write("B5", datePart, normalCenter);

    if (!searchTerm.isEmpty())
        xlsx.write("A6", "Search Keyword:", boldCenter),
            xlsx.write("B6", searchTerm.replace("_", " "), normalCenter);

    int startRow = 8;

    // --- Write table headers ---
    for (int col = 0; col < ui->visitorTable->columnCount(); ++col) {
        QString headerText = ui->visitorTable->horizontalHeaderItem(col)->text();
        xlsx.write(startRow, col + 1, headerText, headerFormat);
    }

    // --- Write table data ---
    for (int row = 0; row < ui->visitorTable->rowCount(); ++row) {
        for (int col = 0; col < ui->visitorTable->columnCount(); ++col) {
            QTableWidgetItem *item = ui->visitorTable->item(row, col);
            if (item)
                xlsx.write(startRow + row + 1, col + 1, item->text(), normalCenter);
        }
    }

    // Auto-size columns
    for (int col = 1; col <= ui->visitorTable->columnCount(); ++col)
        xlsx.setColumnWidth(col, 25);

    int lastRow = startRow + ui->visitorTable->rowCount() + 2;
    xlsx.write(lastRow, 1, "Total Visitors:", boldCenter);
    xlsx.write(lastRow, 2, ui->visitorTable->rowCount(), normalCenter);

    if (xlsx.saveAs(filePath)) {
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


