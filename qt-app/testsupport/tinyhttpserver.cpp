#include "tinyhttpserver.h"

#include <QHostAddress>
#include <QTcpSocket>

namespace {
QByteArray pathOf(const QByteArray &target)
{
    const qsizetype q = target.indexOf('?');
    return q < 0 ? target : target.left(q);
}
} // namespace

TinyHttpServer::TinyHttpServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &TinyHttpServer::onNewConnection);
}

bool TinyHttpServer::listen()
{
    return m_server.listen(QHostAddress::LocalHost, 0);
}

quint16 TinyHttpServer::port() const
{
    return m_server.serverPort();
}

QUrl TinyHttpServer::url(const QString &path) const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port()).arg(path));
}

void TinyHttpServer::route(const QByteArray &path, const Response &response)
{
    m_routes.insert(path, response);
}

int TinyHttpServer::countFor(const QByteArray &path) const
{
    int n = 0;
    for (const Request &r : m_requests) {
        if (pathOf(r.target) == path)
            ++n;
    }
    return n;
}

void TinyHttpServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QObject::destroyed, this, [this, socket]() { m_buffers.remove(socket); });
    }
}

void TinyHttpServer::onReadyRead(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;                                         // wait for the full header

    const QByteArray head = buffer.left(headerEnd);
    const qsizetype lineEnd = head.indexOf("\r\n");
    const QByteArray requestLine = lineEnd < 0 ? head : head.left(lineEnd);
    const QByteArray headerBlock = lineEnd < 0 ? QByteArray() : head.mid(lineEnd + 2);

    qsizetype contentLength = 0;
    for (const QByteArray &line : headerBlock.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("content-length:"))
            contentLength = trimmed.mid(15).trimmed().toLongLong();
    }
    const qsizetype bodyStart = headerEnd + 4;
    if (buffer.size() - bodyStart < contentLength)
        return;                                         // wait for the full body

    Request request;
    const QList<QByteArray> parts = requestLine.split(' ');
    request.method = parts.value(0);
    request.target = parts.value(1);
    request.headerBlock = headerBlock;
    request.body = buffer.mid(bodyStart, contentLength);
    m_buffers.remove(socket);                           // `buffer` is dangling from here on
    m_requests.append(request);
    respond(socket, request);
}

void TinyHttpServer::respond(QTcpSocket *socket, const Request &request)
{
    Response response;
    const QByteArray path = pathOf(request.target);
    if (m_routes.contains(path)) {
        response = m_routes.value(path);
    } else {
        response.status = 404;
        response.reason = "Not Found";
    }
    QByteArray out = "HTTP/1.1 " + QByteArray::number(response.status) + ' '
                     + response.reason + "\r\n";
    for (const auto &header : response.headers)
        out += header.first + ": " + header.second + "\r\n";
    out += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    if (request.method != "HEAD")
        out += response.body;
    socket->write(out);
    socket->disconnectFromHost();
}
