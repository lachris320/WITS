#include "GuestViewModel.h"

#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include "apiconfig.h"
#include "HttpForm.h"
#include "transport/httpclient.h"

GuestViewModel::GuestViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
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

    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("name"), name.trimmed()},
        {QStringLiteral("company"), company.trimmed()},
        {QStringLiteral("contact"), contact.trimmed()},
        {QStringLiteral("purpose"), purpose.trimmed()},
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("guest_login.php")),
                     fields, this,
        [this](const QByteArray &body) { applyGuestResponse(body); },
        [this]() {
            emit guestFailed(QStringLiteral("Network error. Please try again."));
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
