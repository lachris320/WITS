#include "HttpForm.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QVariant>

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

bool isServerAnswer(bool replyHadError, int httpStatus, const QByteArray &body)
{
    if (!replyHadError)
        return true;
    // An error status is still the server answering — provided it sent
    // something for the decode seam to read.
    return httpStatus > 0 && !body.isEmpty();
}

namespace {

// The finished-signal shell shared by submit() (POST) and get() (GET): read the
// body + status, release the reply, then dispatch through isServerAnswer() so
// both verbs classify a response identically.
void wireReply(QNetworkReply *reply, QObject *context,
               std::function<void(const QByteArray &)> onSuccess,
               std::function<void()> onNetworkError)
{
    QObject::connect(reply, &QNetworkReply::finished, context,
                     [reply, onSuccess = std::move(onSuccess),
                      onNetworkError = std::move(onNetworkError)]() {
        const QByteArray body = reply->readAll();
        const bool replyHadError = reply->error() != QNetworkReply::NoError;
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (isServerAnswer(replyHadError, httpStatus, body))
            onSuccess(body);
        else
            onNetworkError();
    });
}

} // namespace

void submit(QNetworkAccessManager *nam, const QUrl &url,
            const QList<QPair<QString, QString>> &fields, QObject *context,
            std::function<void(const QByteArray &)> onSuccess,
            std::function<void()> onNetworkError)
{
    wireReply(nam->post(formRequest(url), encodeForm(fields)), context,
              std::move(onSuccess), std::move(onNetworkError));
}

void get(QNetworkAccessManager *nam, const QUrl &url, QObject *context,
         std::function<void(const QByteArray &)> onSuccess,
         std::function<void()> onNetworkError)
{
    wireReply(nam->get(QNetworkRequest(url)), context,
              std::move(onSuccess), std::move(onNetworkError));
}

} // namespace HttpForm
