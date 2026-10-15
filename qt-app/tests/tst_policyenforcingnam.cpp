#include <QtTest>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <memory>

#include "tinyhttpserver.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

// Behaviour-neutrality proof for the S1a choke point (spec §3, §6 Layer 9):
// every request is sent once through a plain QNetworkAccessManager and once
// through a Passthrough PolicyEnforcingNam against the same loopback server,
// and what reaches the wire / comes back must be identical.
class TestPolicyEnforcingNam : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void carriesTheGivenPolicySnapshot();
    void nullPolicyFallsBackToCurrent();
    void wireRequestIdenticalToPlainNam_data();
    void wireRequestIdenticalToPlainNam();
    void multipartUploadIdenticalToPlainNam();
    void replyRequestIdenticalToPlainNam();
    void plainHttpOriginIsAllowed();
    void redirectFollowedExactlyLikePlainNam();
    void callerManualRedirectPolicyPreserved();
    void transportErrorIdenticalToPlainNam();

private:
    enum class Verb { Get, PostForm, PostJson };
    static QNetworkReply *send(QNetworkAccessManager &nam, Verb verb, const QUrl &url);
    static bool waitFor(QNetworkReply *reply);

    TinyHttpServer m_server;
};

QNetworkReply *TestPolicyEnforcingNam::send(QNetworkAccessManager &nam, Verb verb, const QUrl &url)
{
    QNetworkRequest req(url);
    switch (verb) {
    case Verb::Get:
        return nam.get(req);
    case Verb::PostForm:
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
        return nam.post(req, QByteArrayLiteral("school_id=TEST-0001&admin_key=s1a-test-key"));
    case Verb::PostJson:
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        return nam.post(req, QByteArrayLiteral("{\"search\":\"Test Student A\"}"));
    }
    return nullptr;
}

bool TestPolicyEnforcingNam::waitFor(QNetworkReply *reply)
{
    if (reply->isFinished())
        return true;
    QSignalSpy spy(reply, &QNetworkReply::finished);
    return spy.wait(5000);
}

void TestPolicyEnforcingNam::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);   // loopback only, both managers
    QVERIFY(m_server.listen());

    TinyHttpServer::Response json;
    json.headers = {{"Content-Type", "application/json"}};
    json.body = "{\"status\":\"success\"}";
    m_server.route("/loams_api/get_departments.php", json);
    m_server.route("/loams_api/turnstile_display.php", json);
    m_server.route("/loams_api/student_login.php", json);
    m_server.route("/loams_api/api.php/reports/data", json);
    m_server.route("/loams_api/upload_students_zip.php", json);

    TinyHttpServer::Response redirect;
    redirect.status = 302;
    redirect.reason = "Found";
    redirect.headers = {{"Location", "/loams_api/final.php"}};
    m_server.route("/loams_api/redirect.php", redirect);

    TinyHttpServer::Response finalPage;
    finalPage.body = "final";
    m_server.route("/loams_api/final.php", finalPage);
}

void TestPolicyEnforcingNam::init()
{
    m_server.clear();
}

void TestPolicyEnforcingNam::carriesTheGivenPolicySnapshot()
{
    const auto policy = TransportPolicy::makePassthrough();
    PolicyEnforcingNam nam(policy);
    QCOMPARE(nam.policy().get(), policy.get());
}

void TestPolicyEnforcingNam::nullPolicyFallsBackToCurrent()
{
    PolicyEnforcingNam nam(nullptr);
    QVERIFY(nam.policy());
    QCOMPARE(nam.policy().get(), TransportPolicy::current().get());
}

void TestPolicyEnforcingNam::wireRequestIdenticalToPlainNam_data()
{
    QTest::addColumn<int>("verb");
    QTest::addColumn<QString>("path");
    QTest::newRow("GET get_departments")
        << int(Verb::Get) << QStringLiteral("/loams_api/get_departments.php");
    QTest::newRow("GET with query")
        << int(Verb::Get) << QStringLiteral("/loams_api/turnstile_display.php?after=3");
    QTest::newRow("POST form")
        << int(Verb::PostForm) << QStringLiteral("/loams_api/student_login.php");
    QTest::newRow("POST json api.php route")
        << int(Verb::PostJson) << QStringLiteral("/loams_api/api.php/reports/data");
}

void TestPolicyEnforcingNam::wireRequestIdenticalToPlainNam()
{
    QFETCH(int, verb);
    QFETCH(QString, path);
    const QUrl url = m_server.url(path);

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(send(plain, Verb(verb), url));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(send(seam, Verb(verb), url));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(m_server.requests().size(), 2);
    const TinyHttpServer::Request viaPlain = m_server.requests().at(0);
    const TinyHttpServer::Request viaSeam = m_server.requests().at(1);
    QCOMPARE(viaSeam.method, viaPlain.method);
    QCOMPARE(viaSeam.target, viaPlain.target);
    QCOMPARE(viaSeam.headerBlock, viaPlain.headerBlock);
    QCOMPARE(viaSeam.body, viaPlain.body);
    QCOMPARE(b->error(), a->error());
    QCOMPARE(b->attribute(QNetworkRequest::HttpStatusCodeAttribute),
             a->attribute(QNetworkRequest::HttpStatusCodeAttribute));
    QCOMPARE(b->readAll(), a->readAll());
}

void TestPolicyEnforcingNam::multipartUploadIdenticalToPlainNam()
{
    // ImportController / StudentController::registerStudent upload multipart.
    auto upload = [this](QNetworkAccessManager &nam) {
        auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        multi->setBoundary(QByteArrayLiteral("s1a-fixed-boundary"));
        QHttpPart field;
        field.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QStringLiteral("form-data; name=\"admin_key\""));
        field.setBody(QByteArrayLiteral("s1a-test-key"));
        multi->append(field);
        QHttpPart file;
        file.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/zip"));
        file.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"zip\"; filename=\"students.zip\""));
        file.setBody(QByteArrayLiteral("PK synthetic zip bytes"));
        multi->append(file);
        QNetworkReply *reply = nam.post(
            QNetworkRequest(m_server.url(QStringLiteral("/loams_api/upload_students_zip.php"))), multi);
        multi->setParent(reply);
        return reply;
    };

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(upload(plain));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(upload(seam));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(m_server.requests().size(), 2);
    QCOMPARE(m_server.requests().at(1).headerBlock, m_server.requests().at(0).headerBlock);
    QCOMPARE(m_server.requests().at(1).body, m_server.requests().at(0).body);
}

void TestPolicyEnforcingNam::replyRequestIdenticalToPlainNam()
{
    // No attribute / header / redirect-policy / SSL change reaches the
    // QNetworkAccessManager machinery.
    QNetworkRequest req(m_server.url(QStringLiteral("/loams_api/get_departments.php")));
    req.setRawHeader("X-Test", "s1a");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(req));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(req));
    QVERIFY(b->request() == a->request());
    QVERIFY(waitFor(a.get()));
    QVERIFY(waitFor(b.get()));
}

void TestPolicyEnforcingNam::plainHttpOriginIsAllowed()
{
    // Passthrough keeps today's http default (spec §3 "S1a seam").
    const QUrl url = m_server.url(QStringLiteral("/loams_api/get_departments.php"));
    QCOMPARE(url.scheme(), QStringLiteral("http"));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> r(seam.get(QNetworkRequest(url)));
    QVERIFY(waitFor(r.get()));
    QCOMPARE(r->error(), QNetworkReply::NoError);
    QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
}

void TestPolicyEnforcingNam::redirectFollowedExactlyLikePlainNam()
{
    // Today's Qt 6 default (NoLessSafeRedirectPolicy) follows the 302; S1a must
    // not change that (redirect rejection is S1e).
    const QUrl url = m_server.url(QStringLiteral("/loams_api/redirect.php"));
    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(QNetworkRequest(url)));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(QNetworkRequest(url)));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(a->url().path(), QStringLiteral("/loams_api/final.php"));   // documents today
    QCOMPARE(b->url(), a->url());
    QCOMPARE(b->attribute(QNetworkRequest::HttpStatusCodeAttribute),
             a->attribute(QNetworkRequest::HttpStatusCodeAttribute));
    QCOMPARE(b->readAll(), a->readAll());
    QCOMPARE(m_server.countFor("/loams_api/final.php"), 2);
}

void TestPolicyEnforcingNam::callerManualRedirectPolicyPreserved()
{
    QNetworkRequest req(m_server.url(QStringLiteral("/loams_api/redirect.php")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(req));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(req));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(a->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 302);
    QCOMPARE(b->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 302);
    QCOMPARE(m_server.countFor("/loams_api/final.php"), 0);
}

void TestPolicyEnforcingNam::transportErrorIdenticalToPlainNam()
{
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 closedPort = probe.serverPort();
    probe.close();
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/loams_api/get_departments.php").arg(closedPort));

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(QNetworkRequest(url)));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(QNetworkRequest(url)));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(a->error(), QNetworkReply::ConnectionRefusedError);
    QCOMPARE(b->error(), a->error());
}

QTEST_MAIN(TestPolicyEnforcingNam)
#include "tst_policyenforcingnam.moc"
