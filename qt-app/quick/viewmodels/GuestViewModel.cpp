#include "GuestViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include "apiconfig.h"

GuestViewModel::GuestViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

bool GuestViewModel::validate(const QString &name, const QString &company,
                              const QString &purpose)
{
    return !name.trimmed().isEmpty()
        && !company.trimmed().isEmpty()
        && !purpose.trimmed().isEmpty();
}

void GuestViewModel::submitGuest(const QString &name, const QString &contact,
                                 const QString &company, const QString &purpose)
{
    if (!validate(name, company, purpose)) {
        emit guestFailed(QStringLiteral("Name, Company, and Purpose are required."));
        return;
    }

    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("guest_login.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("name"), name.trimmed());
    form.addQueryItem(QStringLiteral("company"), company.trimmed());
    form.addQueryItem(QStringLiteral("contact"), contact.trimmed());
    form.addQueryItem(QStringLiteral("purpose"), purpose.trimmed());

    QNetworkReply *reply =
        m_nam->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const bool netErr = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (netErr) {
            emit guestFailed(QStringLiteral("Network error. Please try again."));
            return;
        }
        applyGuestResponse(body);
    });
}

void GuestViewModel::applyGuestResponse(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        emit guestFailed(QStringLiteral("Invalid server response."));
        return;
    }
    const QJsonObject obj = doc.object();
    if (obj.value("status").toString() == QLatin1String("success"))
        emit guestSucceeded(obj.value("message").toString());
    else
        emit guestFailed(obj.value("message").toString());
}
