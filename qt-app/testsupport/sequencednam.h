#ifndef SEQUENCEDNAM_H
#define SEQUENCEDNAM_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QUrl>

// Test-only NAM that answers each request with the NEXT enqueued canned
// response. enqueue() finishes on the next event-loop turn; enqueueStall()
// finishes only when the reply is aborted (drives timeout/stop/generation
// paths). Tracks request + abort counts. No live network.
class SequencedNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit SequencedNam(QObject *parent = nullptr);
    void enqueue(const QByteArray &body,
                 QNetworkReply::NetworkError error = QNetworkReply::NoError);
    void enqueueStall();
    int requestCount() const { return m_requestCount; }
    int abortCount() const { return m_abortCount; }
    int maxActive() const { return m_maxActive; }   // peak concurrent in-flight replies
    void noteAbort() { ++m_abortCount; }
    void noteActive() { if (++m_active > m_maxActive) m_maxActive = m_active; }
    void noteFinished() { if (m_active > 0) --m_active; }
    QUrl lastUrl;
    QList<QUrl> urls;   // every request URL, in order

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    struct Canned { QByteArray body; QNetworkReply::NetworkError error; bool stall; };
    QQueue<Canned> m_queue;
    int m_requestCount = 0;
    int m_abortCount = 0;
    int m_active = 0;
    int m_maxActive = 0;
};

#endif // SEQUENCEDNAM_H
