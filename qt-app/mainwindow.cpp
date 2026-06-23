#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "adminwindow.h"
#include "guestwindow.h"
#include <QDateTime>
#include <QTimer>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QPushButton>
#include <QLineEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this ->showFullScreen();

    updateTimeandDate();
    networkManager = new QNetworkAccessManager(this);

    adminWin = new adminWindow(this);

    QPixmap placeholder(":/resources/default_student.png");
    ui->studentPhoto->setPixmap(placeholder.scaled(
        ui->studentPhoto->width(),
        ui->studentPhoto->height(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        ));
    ui->studentPhoto->setScaledContents(false);
    ui->studentPhoto->setAlignment(Qt::AlignCenter);


    connect(adminWin, &adminWindow::schoolInfoUpdated, this, [this](const QString &schoolName, const QString &address, const QFont &font){
        ui->schoolNameLabel->setText(schoolName);
        ui->schoolNameLabel->setFont(font);

        ui->schAddressLabel->setText(address);
        ui->schAddressLabel->setFont(font);
    });
    connect(adminWin, &adminWindow::logoChanged, this,[this](const QString &logoPath) {
        updateLogo(logoPath);
    });
    connect(adminWin, &adminWindow::posterChanged, this, [this](const QString &posterPath) {
        updatePoster(posterPath);
    });
    connect(adminWin, &adminWindow::clearAttendanceRequested, this, [this]() {
        recentLogins.clear();
        refreshRightPanel();
    });
    connect(adminWin, &adminWindow::guestLoginToggled, this, [this](bool enabled) {
        ui->guestLoginBtn->setVisible(enabled);
    });
    connect(ui->guestLoginBtn, &QPushButton::clicked, this, [this]() {
        GuestWindow *guestWin = new GuestWindow(this);
        guestWin->show();
    });

    ui->guestLoginBtn->setVisible(false);
    connect(ui->username, &QLineEdit::returnPressed, this, &MainWindow::handleLogin);

    QSettings settings("MyCompany", "MyApp");
    QString schoolName = settings.value("school/name", "").toString();
    QString address = settings.value("school/address", "").toString();
    QString fontFamily = settings.value("school/fontFamily", "Arial").toString();
    int fontSize = settings.value("school/fontSize", 12).toInt();
    QString logoPath   = settings.value("school/logoPath", "").toString();
    QString posterPath = settings.value("school/posterPath", "").toString();
    updatePoster(posterPath);

    QFont schoolFont(fontFamily, fontSize);
    if (schoolName.isEmpty() && address.isEmpty()) {
        ui->schoolNameLabel->setVisible(false);
        ui->schAddressLabel->setVisible(false);
    } else {
        ui->schoolNameLabel->setText(schoolName);
        ui->schAddressLabel->setText(address);
        ui->schoolNameLabel->setVisible(true);
        ui->schAddressLabel->setVisible(true);
        ui->schoolNameLabel->setText(schoolName);
        ui->schoolNameLabel->setFont(schoolFont);
        ui->schAddressLabel->setText(address);
        ui->schAddressLabel->setFont(schoolFont);
    }
    updateLogo(logoPath);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTimeandDate);
    connect(ui->loginBtn, &QPushButton::clicked, this, &MainWindow::handleLogin);
    timer->start(1000);
    this->setStyleSheet(R"(
        QMainWindow {
            background-color: #BDC3C7;
        }

        /* Left Sidebar Frame */
        QFrame#frame {
            background-color: #2C3E50;
            border: 1px solid #BDC3C7;
            border: none;
        }

        QFrame#frame_3 {
            background-color: #3B4C61;   /* Lighter grey */
            border: 1px solid #BDC3C7;   /* Soft border for texture */
            padding: 10px;
        }


        QLabel#schLogo_Image {
            color: white;
            font-size: 16px;
            font-weight: bold;
            qproperty-alignment: AlignCenter;
            border: 2px dashed #BDC3C7;
            border-radius: 8px;
            background-color: rgba(255,255,255,0.05);
        }

        QLineEdit#username {
            border: 2px solid #BDC3C7;
            border-radius: 10px;
            padding: 8px;
            background: white;
            font-size: 14px;
        }
        QLabel#schoolNameLabel, QLabel#schAddressLabel {
            color: #FFFFFF; /* or another dark color that contrasts with the background */
        }
        QPushButton {
            background-color: #4A90E2;
            color: white;
            border-radius: 8px;
            padding: 6px 12px;
        }

        QPushButton#loginBtn {
            background-color: #1ABC9C;
            color: white;
            font-size: 14px;
            font-weight: bold;
            border-radius: 10px;
            padding: 6px 12px;
        }
        QPushButton#loginBtn:hover {
            background-color: #16A085;
        }
        QPushButton#loginBtn:pressed {
            background-color: #149174;
        }

        QLabel#time_label, QLabel#date_label {
            color: #ECF0F1;
            font-size: 14px;
            font-weight: bold;
            padding-left: 10px;
        }

        QFrame#frame_2 {
            background-color: #FFFFFF;
            border-radius: 10px;
        }

        QScrollArea#stdList {
            background: transparent;
            border: none;
        }

        QLabel#label {
            font-size: 18px;
            font-weight: bold;
            color: #2C3E50;
            padding: 10px;
            border-bottom: 2px solid #BDC3C7;
        }

        QLabel {
            color: #2C3E50;
        }
    )");

    // Center the logo label text
    ui->schLogo_Image->setAlignment(Qt::AlignCenter);

    QGraphicsDropShadowEffect *shadow1 = new QGraphicsDropShadowEffect(this);
    shadow1->setBlurRadius(15);
    shadow1->setOffset(0, 3);
    shadow1->setColor(QColor(0, 0, 0, 50));
    ui->widget->setGraphicsEffect(shadow1);

    QGraphicsDropShadowEffect *shadow3 = new QGraphicsDropShadowEffect(this);
    shadow3->setBlurRadius(20);
    shadow3->setOffset(0, 4);
    shadow3->setColor(QColor(0, 0, 0, 60));
    ui->frame_3->setGraphicsEffect(shadow3);


    connect(ui->username, &QLineEdit::textChanged, this, [this](const QString &text){
        if (text.isEmpty()) {
            // Reset to normal if user clears the field
            ui->username->setEchoMode(QLineEdit::Normal);
            return;
        }

        // Check only the first character
        QChar firstChar = text.at(0);
        ui->username->setEchoMode(firstChar.isLetter() ? QLineEdit::Password : QLineEdit::Normal);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handleLogin() {
    QString input = ui->username->text().trimmed();

    if (input.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter your School ID or Admin Key.");
        return;
    }

    QUrl url;
    QUrlQuery postData;
    QNetworkRequest request;

    bool ok;
    input.toLongLong(&ok); // check if numeric
    if (ok) { // numeric -> assume student ID
        url = QUrl("http://localhost/student_login.php");
        postData.addQueryItem("school_id", input);
    } else { // non-numeric -> assume admin key
        url = QUrl("http://localhost/admin_login.php");
        postData.addQueryItem("admin_key", input);
    }

    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "Network Error", reply->errorString());
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
            if (obj.contains("student")) {
                displayStudent(obj["student"].toObject());
            } else {
                adminWin->show();
            }
        }
    });
}


void MainWindow::displayStudent(const QJsonObject &student) {
    QString photoUrl = student["photo_url"].toString();

    if (!photoUrl.isEmpty()) {
        QNetworkAccessManager *photoManager = new QNetworkAccessManager(this);
        QNetworkRequest request{QUrl(photoUrl)};

        connect(photoManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *photoReply) {
            if (photoReply->error() == QNetworkReply::NoError) {
                QByteArray imgData = photoReply->readAll();
                QPixmap pix;
                if (pix.loadFromData(imgData)) {
                    ui->studentPhoto->setPixmap(pix.scaled(
                        ui->studentPhoto->width(),
                        ui->studentPhoto->height(),
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation
                        ));

                    QGraphicsOpacityEffect *fadeEffect = new QGraphicsOpacityEffect();
                    ui->studentPhoto->setGraphicsEffect(fadeEffect);

                    QPropertyAnimation *fadeAnim = new QPropertyAnimation(fadeEffect, "opacity");
                    fadeAnim->setDuration(700);
                    fadeAnim->setStartValue(0.0);
                    fadeAnim->setEndValue(1.0);
                    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
                }
            }
            photoReply->deleteLater();
        });
        photoManager->get(request);
    }

    recentLogins.prepend(student);
    while (recentLogins.size() > 9)
        recentLogins.removeLast();
    refreshRightPanel();
}

void MainWindow::toggleFullscreen(){
    if (isFullScreen()){
        showNormal();
    }
    else{
        showFullScreen();
    }
}

void MainWindow::updateTimeandDate(){
    QDateTime now = QDateTime::currentDateTime();
    ui->time_label->setText(now.toString("hh:mm:ss AP"));
    ui->date_label->setText(now.toString("dddd, MM/dd/yyyy"));
}

void MainWindow::updateSchoolInfo(const QString &schoolName, const QString &address, const QFont &font){
    ui->schoolNameLabel->setText(schoolName);
    ui->schAddressLabel->setText(address);
    ui->schoolNameLabel->setFont(font);
    ui->schAddressLabel->setFont(font);
}

void MainWindow::refreshRightPanel() {
    QList<QLabel*> nameLabels = { ui->nameLabel, ui->nameLabel_2, ui->nameLabel_3, ui->nameLabel_5,
                                  ui->nameLabel_7, ui->nameLabel_8, ui->nameLabel_9, ui->nameLabel_11, ui->nameLabel_13};
    QList<QLabel*> courseLabels = { ui->courseLabel, ui->courseLabel_2, ui->courseLabel_3, ui->courseLabel_5,
                                    ui->courseLabel_7, ui->courseLabel_8, ui->courseLabel_9, ui->courseLabel_11, ui->courseLabel_13};
    QList<QLabel*> yearLabels = { ui->yrlevel_label, ui->yrlevel_label_2, ui->yrlevel_label_3, ui->yrlevel_label_5,
                                  ui->yrlevel_label_7, ui->yrlevel_label_8, ui->yrlevel_label_9, ui->yrlevel_label_11, ui->yrlevel_label_13};
    QList<QLabel*> depLabels = { ui->depLabel, ui->depLabel_2, ui->depLabel_3, ui->depLabel_5,
                                 ui->depLabel_7, ui->depLabel_8, ui->depLabel_9, ui->depLabel_11, ui->depLabel_13};
    QList<QLabel*> timeLabels = { ui->timeDate_Label, ui->timeDate_Label_2, ui->timeDate_Label_3, ui->timeDate_Label_5,
                                  ui->timeDate_Label_7, ui->timeDate_Label_8, ui->timeDate_Label_9, ui->timeDate_Label_11, ui->timeDate_Label_13};

    for (int i = 0; i < nameLabels.size(); i++) {
        if (i < recentLogins.size()) {
            QJsonObject student = recentLogins[i];
            nameLabels[i]->setText(student["name"].toString());
            courseLabels[i]->setText(student["course"].toString());
            yearLabels[i]->setText(student["year_level"].toString());
            depLabels[i]->setText(student["department"].toString());
            timeLabels[i]->setText(student["time_date"].toString());
        } else {
            nameLabels[i]->clear();
            courseLabels[i]->clear();
            yearLabels[i]->clear();
            depLabels[i]->clear();
            timeLabels[i]->clear();
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

    QSettings settings("MyCompany", "MyApp");
    QString logoPath = settings.value("school/logoPath", "").toString();
    updateLogo(logoPath);
}

void MainWindow::updateLogo(const QString &logoPath) {
    if (!logoPath.isEmpty() && QFile::exists(logoPath)) {
        QPixmap pix(logoPath);
        if (!pix.isNull()) {
            ui->schLogo_Image->setPixmap(pix.scaled(
                ui->schLogo_Image->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation)
            );
            return;
        }
    }
    ui->schLogo_Image->setText("No Logo Selected");
    ui->schLogo_Image->setPixmap(QPixmap()); // clear any old pixmap
    ui->schLogo_Image->setAlignment(Qt::AlignCenter);
}

void MainWindow::updatePoster(const QString &posterPath) {
    if (!posterPath.isEmpty() && QFile::exists(posterPath)) {
        QPixmap pix(posterPath);
        if (!pix.isNull()) {
            ui->poster_Image->setPixmap(
                pix.scaled(
                    ui->poster_Image->size(),
                    Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation
                )
            );
            return;
        }
    }
    // If invalid, show placeholder
    ui->poster_Image->setText("No Poster Selected");
    ui->poster_Image->setPixmap(QPixmap());
    ui->poster_Image->setAlignment(Qt::AlignCenter);
}


