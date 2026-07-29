#include "capturingnam.h"

#include <QBuffer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {
// Minimal QNetworkReply that reports success and hands back canned bytes on
// readAll(), emitting finished() on the next event-loop turn (so consumers that
// connect to finished() after createRequest() returns still receive it).
class CannedReply : public QNetworkReply
{
public:
    CannedReply(const QByteArray &data, QObject *parent) : QNetworkReply(parent)
    {
        m_buf.setData(data);
        m_buf.open(QIODevice::ReadOnly);
        setOpenMode(QIODevice::ReadOnly);
        setError(QNetworkReply::NoError, QString());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        QTimer::singleShot(0, this, [this]() {
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {}
    qint64 bytesAvailable() const override
    {
        return m_buf.bytesAvailable() + QNetworkReply::bytesAvailable();
    }
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char *data, qint64 maxlen) override { return m_buf.read(data, maxlen); }
private:
    QBuffer m_buf;
};
} // namespace

CapturingNam::CapturingNam(const QByteArray &cannedResponse, QObject *parent)
    : QNetworkAccessManager(parent), m_canned(cannedResponse)
{}

QNetworkReply *CapturingNam::createRequest(Operation op, const QNetworkRequest &request,
                                           QIODevice *outgoingData)
{
    lastOp = op;
    lastUrl = request.url();
    lastContentType =
        request.header(QNetworkRequest::ContentTypeHeader).toString();
    lastBody = outgoingData ? outgoingData->readAll() : QByteArray();
    return new CannedReply(m_canned, this);
}
