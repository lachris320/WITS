#ifndef CAPTURINGNAM_H
#define CAPTURINGNAM_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QUrl>

// Test-only QNetworkAccessManager that records the next request's operation,
// URL, Content-Type, and full outgoing body, then returns a reply that finishes
// immediately with a canned payload. Lets request-assembly be asserted with no
// live network (project house rule: no live network in unit tests).
class CapturingNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit CapturingNam(const QByteArray &cannedResponse =
                              QByteArrayLiteral("{\"status\":\"success\"}"),
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
};

#endif // CAPTURINGNAM_H
