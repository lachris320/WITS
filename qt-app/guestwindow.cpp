#include "guestwindow.h"
#include "ui_guestwindow.h"
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>

GuestWindow::GuestWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::guestwindow),
    networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    // optional: make dialog delete itself when closed
    connect(ui->submitBtn, &QPushButton::clicked, this, &GuestWindow::onSubmitBtnClicked);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &GuestWindow::onCancelBtnClicked);
    setAttribute(Qt::WA_DeleteOnClose);
}

GuestWindow::~GuestWindow()
{
    delete ui;
}

void GuestWindow::onSubmitBtnClicked()
{
    QString name = ui->fullNameLineEdit->text().trimmed();
    QString contact = ui->contactLineEdit->text().trimmed();
    QString company = ui->companyLineEdit->text().trimmed();
    QString purpose = ui->purposeLineEdit->text().trimmed();

    if (name.isEmpty() || company.isEmpty() || purpose.isEmpty()) {
        QMessageBox::warning(this, "Error", "Name, Company, and Purpose are required.");
        return;
    }

    QUrl url("http://localhost/guest_login.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("name", name);
    postData.addQueryItem("company", company);   // ✅ Added this
    postData.addQueryItem("contact", contact);
    postData.addQueryItem("purpose", purpose);

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
            this->close(); // close after success
        } else {
            QMessageBox::warning(this, "Failed", obj["message"].toString());
        }
    });
}

void GuestWindow::onCancelBtnClicked(){
    this->close();
}
