#ifndef CAPTURINGNAM_H
#define CAPTURINGNAM_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

// Test-only QNetworkAccessManager that records the next request's operation,
// URL, Content-Type, and full outgoing body, then returns a reply that finishes
// immediately with a canned payload. Lets request-assembly be asserted with no
// live network (project house rule: no live network in unit tests).
class CapturingNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    // cannedError/cannedHttpStatus default to the historical behavior (a
    // successful reply with HTTP 200) so every pre-existing caller compiles
    // and behaves unchanged. Pass a non-NoError value (e.g.
    // AuthenticationRequiredError + 401) to simulate a guard rejection that
    // still carries a decodable body, per HttpForm::isServerAnswer semantics.
    explicit CapturingNam(const QByteArray &cannedResponse =
                              QByteArrayLiteral("{\"status\":\"success\"}"),
                          QNetworkReply::NetworkError cannedError = QNetworkReply::NoError,
                          int cannedHttpStatus = 200,
                          QObject *parent = nullptr);

    QNetworkAccessManager::Operation lastOp = QNetworkAccessManager::UnknownOperation;
    QUrl lastUrl;
    QString lastContentType;
    QByteArray lastBody;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    QByteArray m_canned;
    QNetworkReply::NetworkError m_cannedError;
    int m_cannedHttpStatus;
};

#endif // CAPTURINGNAM_H
