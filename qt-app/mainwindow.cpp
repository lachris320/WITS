#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "apiconfig.h"
#include "adminwindow.h"
#include "guestwindow.h"
#include <QDateTime>
#include <QTimer>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QPushButton>
#include <QLineEdit>
#include <QFontComboBox>
#include <QSpinBox>
#include <QIcon>
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
#include <QApplication>
#include <QElapsedTimer>
#include "rfidkeyboardfilter.h"
#include "theme.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this ->showFullScreen();

    ui->loginBtn->setIcon(QIcon(":/resources/icons/log-in.svg"));
    ui->loginBtn->setIconSize(QSize(20, 20));

    updateTimeandDate();
    networkManager = new QNetworkAccessManager(this);

    adminWin = new adminWindow(this);

    QIcon placeholderIcon(":/resources/default_student.svg");
    ui->studentPhoto->setPixmap(placeholderIcon.pixmap(
        ui->studentPhoto->size()));
    ui->studentPhoto->setScaledContents(false);
    ui->studentPhoto->setAlignment(Qt::AlignCenter);

    m_idleHint = new QLabel("Scan your ID or type your ID number.", ui->widget);
    m_idleHint->setObjectName("emptyState");
    ui->detailInfoLayout->addWidget(m_idleHint);
    m_idleHint->hide();
    refreshRightPanel();

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

    rfidFilter = new RfidKeyboardFilter(this, this);
    qApp->installEventFilter(rfidFilter);
    connect(rfidFilter, &RfidKeyboardFilter::rfidScanned, this, &MainWindow::handleRfidLogin);
    m_rfidDebounceClock.start();

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
    // Center the logo label text
    ui->schLogo_Image->setAlignment(Qt::AlignCenter);

    QGraphicsDropShadowEffect *shadow1 = new QGraphicsDropShadowEffect(this);
    shadow1->setBlurRadius(20);
    shadow1->setOffset(0, 3);
    shadow1->setColor(QColor(0, 0, 0, 40));
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
        url = ApiConfig::endpoint("student_login.php");
        postData.addQueryItem("school_id", input);
    } else { // non-numeric -> assume admin key
        url = ApiConfig::endpoint("admin_login.php");
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
        connect(photoManager, &QNetworkAccessManager::finished, photoManager, &QObject::deleteLater);
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

void MainWindow::showKioskStatus(const QString &message, bool error) {
    if (!m_statusLabel) {
        m_statusLabel = new QLabel(this);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->hide();
    }
    m_statusLabel->setStyleSheet(QString(
        "background:%1; color:white; padding:14px 22px; border-radius:22px;"
        "font-size:18px; font-weight:600;")
        .arg(error ? WitsTheme::Color::Error : WitsTheme::Color::Success));
    m_statusLabel->setText(message);
    m_statusLabel->adjustSize();
    m_statusLabel->move((width() - m_statusLabel->width()) / 2,
                        height() - m_statusLabel->height() - 60);
    m_statusLabel->raise();
    m_statusLabel->show();
    QTimer::singleShot(2500, m_statusLabel, [this]() {
        if (m_statusLabel) m_statusLabel->hide();
    });
}

void MainWindow::handleRfidLogin(const QString &code) {
    // Debounce: ignore the same card re-scanned within 2.5s (one tap = one visit).
    const qint64 now = m_rfidDebounceClock.elapsed();
    if (code == m_lastRfidCode && (now - m_lastRfidMs) < 2500)
        return;
    m_lastRfidCode = code;
    m_lastRfidMs = now;

    ui->username->clear(); // remove the brief flash of the scanned code

    QUrl url = ApiConfig::endpoint("rfid_login.php");
    QUrlQuery postData;
    postData.addQueryItem("rfid_id", code);

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {

        if (reply->error() != QNetworkReply::NoError) {
            showKioskStatus("Network error. Please try again.", true);
            reply->deleteLater();
            return;
        }

        QByteArray response = reply->readAll();
        reply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        if (!jsonDoc.isObject()) {
            showKioskStatus("Invalid server response.", true);
            return;
        }

        QJsonObject obj = jsonDoc.object();
        if (obj["status"].toString() == "success" && obj.contains("student")) {
            displayStudent(obj["student"].toObject());
        } else {
            showKioskStatus("Card not registered. Please see the librarian.", true);
        }
    });
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
            if (i == 0 && m_idleHint) m_idleHint->hide();
        } else if (i == 0) {
            // Spotlight idle empty-state: friendly placeholder when no student is active
            nameLabels[0]->setText("Waiting for log in…");
            courseLabels[0]->setText("—");
            yearLabels[0]->setText("—");
            depLabels[0]->setText("—");
            timeLabels[0]->setText("—");
            if (m_idleHint) m_idleHint->show();
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
    syncPosterBg();          // keep the poster layer covering frame_2 on resize
}

// Render src into a circular pixmap of the given diameter (transparent corners).
static QPixmap makeCircularPixmap(const QPixmap &src, int diameter)
{
    QPixmap out(diameter, diameter);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addEllipse(0, 0, diameter, diameter);
    p.setClipPath(clip);
    const QPixmap scaled = src.scaled(diameter, diameter,
        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    p.drawPixmap((diameter - scaled.width()) / 2,
                 (diameter - scaled.height()) / 2, scaled);
    return out;
}

void MainWindow::updateLogo(const QString &logoPath) {
    const int d = ui->schLogo_Image->width();
    ui->schLogo_Image->setFixedHeight(d); // keep the logo area square
    // Round the label's own background to a circle (matches the masked pixmap).
    ui->schLogo_Image->setStyleSheet(
        QString("background-color:#FFFFFF; color:#1E293B; border-radius:%1px;").arg(d / 2));

    if (!logoPath.isEmpty() && QFile::exists(logoPath) && d > 0) {
        QPixmap pix(logoPath);
        if (!pix.isNull()) {
            ui->schLogo_Image->setPixmap(makeCircularPixmap(pix, d));
            return;
        }
    }
    ui->schLogo_Image->setText("No Logo");
    ui->schLogo_Image->setPixmap(QPixmap());
    ui->schLogo_Image->setAlignment(Qt::AlignCenter);
}

void MainWindow::updatePoster(const QString &posterPath) {
    m_posterPath = posterPath;
    if (!m_posterBg) {
        m_posterBg = new QLabel(ui->frame_2);
        m_posterBg->setObjectName("posterBg");
        m_posterBg->setScaledContents(false);
        m_posterBg->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    syncPosterBg();
    m_posterBg->lower(); // behind the scroll content
}

void MainWindow::syncPosterBg() {
    if (!m_posterBg) return;
    m_posterBg->setGeometry(ui->frame_2->rect());
    const QSize sz = ui->frame_2->size();
    if (m_posterPath.isEmpty() || !QFile::exists(m_posterPath) || sz.isEmpty()) {
        m_posterBg->clear();
        m_posterBg->hide();   // fall back to frame_2's solid QSS background
        return;
    }
    QPixmap pix(m_posterPath);
    if (pix.isNull()) { m_posterBg->clear(); m_posterBg->hide(); return; }

    // Scale to cover, center-crop, apply a rounded mask (match frame_2's 16px) + dark scrim.
    QPixmap canvas(sz);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(sz)), 16, 16);
    p.setClipPath(clip);
    const QPixmap scaled = pix.scaled(sz, Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
    p.drawPixmap((sz.width() - scaled.width()) / 2,
                 (sz.height() - scaled.height()) / 2, scaled);
    p.fillRect(canvas.rect(), QColor(15, 23, 42, 115)); // ~0.45 dark scrim
    p.end();

    m_posterBg->setPixmap(canvas);
    m_posterBg->show();
}


