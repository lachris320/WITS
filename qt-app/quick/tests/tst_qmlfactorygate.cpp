#include <QtTest>
#include <QBuffer>
#include <QImage>
#include <QNetworkProxy>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>

#include "RecordingPolicyNam.h"
#include "tinyhttpserver.h"

// S1a GATE G2: proves on this Qt build that QML image fetches (Image and
// Canvas.loadImage) go through a manager created by the engine's installed
// factory. RecordingPolicyNamFactory's managers log every request that passes
// through them; the in-process TinyHttpServer (127.0.0.1, ephemeral port)
// serves the image — never XAMPP.
namespace {
QByteArray pngBytes()
{
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}
} // namespace

class TestQmlFactoryGate : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void imageLoadUsesInstalledFactoryManager();
    void canvasLoadImageUsesInstalledFactoryManager();

private:
    QQuickItem *load(QQuickView &view, const QByteArray &qml);
    TinyHttpServer m_server;
};

void TestQmlFactoryGate::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    QVERIFY(m_server.listen());                         // 127.0.0.1, port 0
    TinyHttpServer::Response png;
    png.headers = {{"Content-Type", "image/png"}};
    png.body = pngBytes();
    m_server.route("/loams_api/uploads/students/gate-image.png", png);
    m_server.route("/loams_api/uploads/students/gate-canvas.png", png);
}

QQuickItem *TestQmlFactoryGate::load(QQuickView &view, const QByteArray &qml)
{
    QQmlComponent component(view.engine());
    component.setData(qml, QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return nullptr;
    }
    auto *item = qobject_cast<QQuickItem *>(component.create());
    if (!item)
        return nullptr;
    item->setParent(view.contentItem());
    item->setParentItem(view.contentItem());
    return item;
}

void TestQmlFactoryGate::imageLoadUsesInstalledFactoryManager()
{
    RequestLog log;
    RecordingPolicyNamFactory factory(&log);            // declared before the view: outlives it
    QQuickView view;
    QVERIFY(PolicyNamFactory::installOn(*view.engine(), factory));   // before any QML/request
    view.resize(100, 100);
    view.show();

    const QUrl url = m_server.url(QStringLiteral("/loams_api/uploads/students/gate-image.png"));
    QQuickItem *image = load(view, QByteArrayLiteral("import QtQuick\nImage { cache: false; asynchronous: true; source: \"")
                                   + url.toString().toUtf8() + "\" }");
    QVERIFY(image);
    QTRY_COMPARE_WITH_TIMEOUT(image->property("status").toInt(), 1 /* Image.Ready */, 5000);
    QVERIFY(m_server.countFor("/loams_api/uploads/students/gate-image.png") >= 1);

    QThread *issuer = log.threadFor(url);
    QVERIFY2(issuer, "GATE G2 FAILED: the Image request did not pass through a factory-made manager");
    qInfo("GATE G2: image request ran on the %s thread",
          issuer == QThread::currentThread() ? "GUI" : "non-GUI (pixmap reader)");
}

void TestQmlFactoryGate::canvasLoadImageUsesInstalledFactoryManager()
{
    RequestLog log;
    RecordingPolicyNamFactory factory(&log);
    QQuickView view;
    QVERIFY(PolicyNamFactory::installOn(*view.engine(), factory));
    view.resize(100, 100);
    view.show();

    const QUrl url = m_server.url(QStringLiteral("/loams_api/uploads/students/gate-canvas.png"));
    QQuickItem *canvas = load(view, QByteArrayLiteral(
        "import QtQuick\nCanvas { width: 10; height: 10; property bool done: false\n"
        "  Component.onCompleted: loadImage(\"") + url.toString().toUtf8() + QByteArrayLiteral("\")\n"
        "  onImageLoaded: done = true }"));
    QVERIFY(canvas);
    QTRY_VERIFY_WITH_TIMEOUT(canvas->property("done").toBool(), 5000);

    QVERIFY2(log.contains(url),
             "GATE G2 FAILED: the Canvas.loadImage request did not pass through a factory-made manager");
}

QTEST_MAIN(TestQmlFactoryGate)
#include "tst_qmlfactorygate.moc"
