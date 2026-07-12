#ifndef HTTPFORM_H
#define HTTPFORM_H

#include <QByteArray>
#include <QList>
#include <QNetworkRequest>
#include <QPair>
#include <QString>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QObject;

// Shared form-POST transport shell. The ViewModels (KioskViewModel,
// GuestViewModel) POST application/x-www-form-urlencoded bodies with an
// identical request/QUrlQuery/finished-lambda boilerplate; the only thing that
// differs is the response decoder. This namespace factors out exactly the
// shared transport so that boilerplate lives in one place. It is an internal
// util — NOT a QObject, NOT a QML_ELEMENT.
namespace HttpForm {

// Builds an application/x-www-form-urlencoded body from ordered key/value
// fields, exactly as the legacy code did: a QUrlQuery, addQueryItem in order,
// query.toString(QUrl::FullyEncoded).toUtf8(). Pure + network-free.
QByteArray encodeForm(const QList<QPair<QString, QString>> &fields);

// A QNetworkRequest for url with the form-urlencoded content type set.
// Pure + network-free.
QNetworkRequest formRequest(const QUrl &url);

// Posts encodeForm(fields) to url via nam and wires the reply's finished
// signal. context is the connection receiver (lifetime safety): if it is
// destroyed before the reply finishes, the slot never fires. The finished
// handler reads the body, captures whether there was a network error, calls
// deleteLater() on the reply, then dispatches to onNetworkError() on error or
// onSuccess(body) otherwise — matching the legacy ordering exactly.
void submit(QNetworkAccessManager *nam, const QUrl &url,
            const QList<QPair<QString, QString>> &fields, QObject *context,
            std::function<void(const QByteArray &)> onSuccess,
            std::function<void()> onNetworkError);

} // namespace HttpForm

#endif // HTTPFORM_H
