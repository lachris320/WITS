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

// Response classification — pure, network-free, and the single place that
// decides which submit() callback fires.
//
// The useful distinction is NOT "did reply->error() get set" but "did the
// server answer at all". An HTTP response carrying a status code and a body is
// the server answering, even when its answer is an error status: Qt reports a
// 401 as QNetworkReply::ContentAccessDenied, yet the body is exactly what the
// caller needs. requireAdminAuth() in the backend answers a bad or missing
// admin key with HTTP 401 plus {"status":"error","message":"Invalid admin
// key"} — routing that to onNetworkError() would throw the message away and
// tell the user "network error" when they simply mistyped their key.
//
//   replyHadError - reply->error() != QNetworkReply::NoError
//   httpStatus    - QNetworkRequest::HttpStatusCodeAttribute, or 0 when the
//                   attribute is absent (no response line was ever received)
//   body          - reply->readAll()
//
// Returns true when the response should be handed to onSuccess (the decode
// seam), which then classifies auth-vs-generic failure from the JSON itself.
// Returns false only for a genuine transport failure — DNS failure, refused
// connection, timeout — where there is no status code, or a status code with
// an empty body that would give the decode seam nothing to classify.
bool isServerAnswer(bool replyHadError, int httpStatus, const QByteArray &body);

// Posts encodeForm(fields) to url via nam and wires the reply's finished
// signal. context is the connection receiver (lifetime safety): if it is
// destroyed before the reply finishes, the slot never fires. The finished
// handler reads the body and the HTTP status, calls deleteLater() on the
// reply, then dispatches via isServerAnswer() above: onSuccess(body) whenever
// the server actually answered, onNetworkError() only on a transport failure.
void submit(QNetworkAccessManager *nam, const QUrl &url,
            const QList<QPair<QString, QString>> &fields, QObject *context,
            std::function<void(const QByteArray &)> onSuccess,
            std::function<void()> onNetworkError);

// GETs url and wires the reply through the SAME finished-handler and the SAME
// isServerAnswer() classification submit() uses — only the verb and the absent
// body differ. Callers that need a bodiless request (get_departments.php) were
// otherwise forced to hand-roll the reply shell, and a hand-rolled copy is
// exactly what drifts away from the POST paths' error handling.
void get(QNetworkAccessManager *nam, const QUrl &url, QObject *context,
         std::function<void(const QByteArray &)> onSuccess,
         std::function<void()> onNetworkError);

} // namespace HttpForm

#endif // HTTPFORM_H
