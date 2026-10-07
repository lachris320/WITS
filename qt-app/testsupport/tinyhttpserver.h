#ifndef TINYHTTPSERVER_H
#define TINYHTTPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QTcpServer>
#include <QUrl>

class QTcpSocket;

// Test-only, loopback-only HTTP/1.1 responder: listens on 127.0.0.1 with an
// ephemeral port, so nothing leaves the machine and the real backend is never
// touched. One request per connection ("Connection: close"); exact-path
// routes; any other path answers 404. Records every request verbatim so a test
// can prove two managers put byte-identical requests on the wire.
class TinyHttpServer : public QObject
{
    Q_OBJECT
public:
    struct Response {
        int status = 200;
        QByteArray reason = "OK";
        QList<QPair<QByteArray, QByteArray>> headers;
        QByteArray body;
    };
    struct Request {
        QByteArray method;
        QByteArray target;        // request-target as sent (path + optional query)
        QByteArray headerBlock;   // header lines after the request line, verbatim
        QByteArray body;
    };

    explicit TinyHttpServer(QObject *parent = nullptr);

    bool listen();
    quint16 port() const;
    QUrl url(const QString &path) const;            // http://127.0.0.1:<port><path>
    void route(const QByteArray &path, const Response &response);
    const QList<Request> &requests() const { return m_requests; }
    int countFor(const QByteArray &path) const;     // requests whose path (sans query) == path
    void clear() { m_requests.clear(); }

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void respond(QTcpSocket *socket, const Request &request);

    QTcpServer m_server;
    QHash<QByteArray, Response> m_routes;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<Request> m_requests;
};

#endif // TINYHTTPSERVER_H
