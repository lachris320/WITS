#include "sequencednam.h"

#include <QBuffer>
#include <QPointer>
#include <QTimer>
#include <QNetworkRequest>

namespace {
class CannedReply : public QNetworkReply
{
public:
    CannedReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req,
                const QByteArray &body, QNetworkReply::NetworkError error,
                bool stall, SequencedNam *owner)
        : QNetworkReply(owner), m_body(body), m_owner(owner)
    {
        setRequest(req);
        setUrl(req.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        m_buffer.setData(m_body);
        m_buffer.open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,
                     error == QNetworkReply::NoError ? 200 : 500);
        if (!stall) {                          // auto-finish next tick
            QTimer::singleShot(0, this, [this, error]() {
                if (isFinished()) return;      // already aborted — don't double-finish
                if (error != QNetworkReply::NoError) {
                    setError(error, QStringLiteral("canned error"));
                    emit errorOccurred(error);
                }
                if (m_owner) m_owner->noteFinished();
                setFinished(true);
                emit finished();
            });
        }
        // stall: finishes only via abort()
    }
    void abort() override
    {
        if (isFinished()) return;
        if (m_owner) { m_owner->noteAbort(); m_owner->noteFinished(); }
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted"));
        emit errorOccurred(QNetworkReply::OperationCanceledError);
        setFinished(true);
        emit finished();
    }
    qint64 readData(char *data, qint64 maxlen) override { return m_buffer.read(data, maxlen); }
    qint64 bytesAvailable() const override
    { return m_buffer.bytesAvailable() + QNetworkReply::bytesAvailable(); }
private:
    QByteArray m_body;
    QBuffer m_buffer;
    QPointer<SequencedNam> m_owner;
};
} // namespace

SequencedNam::SequencedNam(QObject *parent) : QNetworkAccessManager(parent) {}

void SequencedNam::enqueue(const QByteArray &body, QNetworkReply::NetworkError error)
{ m_queue.enqueue({body, error, false}); }

void SequencedNam::enqueueStall()
{ m_queue.enqueue({QByteArray(), QNetworkReply::NoError, true}); }

QNetworkReply *SequencedNam::createRequest(Operation op, const QNetworkRequest &request,
                                           QIODevice *)
{
    ++m_requestCount;
    noteActive();                       // track peak concurrency (must stay 1)
    lastUrl = request.url();
    Canned c = m_queue.isEmpty()
                   ? Canned{QByteArrayLiteral("{\"status\":\"success\",\"latest_id\":0,\"entry\":null}"),
                            QNetworkReply::NoError, false}
                   : m_queue.dequeue();
    return new CannedReply(op, request, c.body, c.error, c.stall, this);
}
