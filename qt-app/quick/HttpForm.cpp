#include "HttpForm.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

namespace HttpForm {

QByteArray encodeForm(const QList<QPair<QString, QString>> &fields)
{
    QUrlQuery form;
    for (const auto &field : fields)
        form.addQueryItem(field.first, field.second);
    return form.toString(QUrl::FullyEncoded).toUtf8();
}

QNetworkRequest formRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    return request;
}

void submit(QNetworkAccessManager *nam, const QUrl &url,
            const QList<QPair<QString, QString>> &fields, QObject *context,
            std::function<void(const QByteArray &)> onSuccess,
            std::function<void()> onNetworkError)
{
    QNetworkReply *reply = nam->post(formRequest(url), encodeForm(fields));

    QObject::connect(reply, &QNetworkReply::finished, context,
                     [reply, onSuccess = std::move(onSuccess),
                      onNetworkError = std::move(onNetworkError)]() {
        const QByteArray body = reply->readAll();
        const bool netErr = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();

        if (netErr)
            onNetworkError();
        else
            onSuccess(body);
    });
}

} // namespace HttpForm
