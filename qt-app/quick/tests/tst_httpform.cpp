#include <QtTest>
#include <QNetworkRequest>
#include <QUrl>
#include "HttpForm.h"

// Unit test for the PURE, network-free helpers in namespace HttpForm. The
// network-wiring shell (HttpForm::submit) is exercised indirectly by the
// existing tst_kioskviewmodel / tst_guestviewmodel behavior tests — no live
// network is stood up here (workflow rule: no localhost/real server in units).
class TestHttpForm : public QObject
{
    Q_OBJECT
private slots:
    void encodeEmptyListIsEmptyBody();
    void encodeSingleField();
    void encodeMultipleFieldsPreserveOrder();
    void encodeEmptyValueKeepsKeyAndEquals();
    void encodeSpecialCharsAreFullyEncoded();
    void formRequestSetsContentType();
    void formRequestUrlRoundTrips();
    void noReplyErrorIsAServerAnswer();
    void httpErrorStatusWithBodyIsAServerAnswer();
    void transportFailureWithNoStatusIsNotAServerAnswer();
    void httpErrorStatusWithEmptyBodyIsNotAServerAnswer();
    void httpErrorStatusWithNonJsonBodyIsAServerAnswer();
};

void TestHttpForm::encodeEmptyListIsEmptyBody()
{
    QCOMPARE(HttpForm::encodeForm({}), QByteArray());
}

void TestHttpForm::encodeSingleField()
{
    const QByteArray body =
        HttpForm::encodeForm({{QStringLiteral("school_id"), QStringLiteral("2021-0001")}});
    QCOMPARE(body, QByteArray("school_id=2021-0001"));
}

void TestHttpForm::encodeMultipleFieldsPreserveOrder()
{
    const QByteArray body = HttpForm::encodeForm({
        {QStringLiteral("name"), QStringLiteral("Ana")},
        {QStringLiteral("company"), QStringLiteral("Acme")},
        {QStringLiteral("contact"), QStringLiteral("0917")},
        {QStringLiteral("purpose"), QStringLiteral("Meeting")},
    });
    QCOMPARE(body, QByteArray("name=Ana&company=Acme&contact=0917&purpose=Meeting"));
}

void TestHttpForm::encodeEmptyValueKeepsKeyAndEquals()
{
    // An empty value must still emit "key=" (name, '=', no value) — this is a
    // live path: the Guest form's optional 'contact' field can be blank, so
    // encodeForm is called with an empty value in production. The value is an
    // empty-but-non-null QString (what QLineEdit::text() returns for a blank
    // field); a *null* QString would drop the '=', so this must be non-null.
    QCOMPARE(HttpForm::encodeForm({{QStringLiteral("contact"), QStringLiteral("")}}),
             QByteArray("contact="));

    // Mixed with a populated field, the empty value keeps its bare "a=" slot.
    const QByteArray mixed = HttpForm::encodeForm({
        {QStringLiteral("a"), QStringLiteral("")},
        {QStringLiteral("b"), QStringLiteral("x")},
    });
    QCOMPARE(mixed, QByteArray("a=&b=x"));
}

void TestHttpForm::encodeSpecialCharsAreFullyEncoded()
{
    // Spaces, '&', '=', and non-ASCII must be percent-encoded (FullyEncoded),
    // matching the legacy query.toString(QUrl::FullyEncoded) behavior.
    const QByteArray body = HttpForm::encodeForm({
        {QStringLiteral("q"), QStringLiteral("a b&c=d")},
        {QStringLiteral("u"), QString::fromUtf8("caf\xC3\xA9")},   // "café"
    });
    QCOMPARE(body, QByteArray("q=a%20b%26c%3Dd&u=caf%C3%A9"));
}

void TestHttpForm::formRequestSetsContentType()
{
    const QNetworkRequest req = HttpForm::formRequest(QUrl(QStringLiteral("http://example.com/x.php")));
    QCOMPARE(req.header(QNetworkRequest::ContentTypeHeader).toString(),
             QStringLiteral("application/x-www-form-urlencoded"));
}

void TestHttpForm::formRequestUrlRoundTrips()
{
    const QUrl url(QStringLiteral("http://example.com/endpoint.php"));
    const QNetworkRequest req = HttpForm::formRequest(url);
    QCOMPARE(req.url(), url);
}

// --- isServerAnswer: the pure response-classification seam -------------------
// The distinction is "the server answered, and its answer was an error" vs
// "we never got a usable answer". Only the latter is a network error.

void TestHttpForm::noReplyErrorIsAServerAnswer()
{
    // The ordinary 200 path: no reply error at all.
    QVERIFY(HttpForm::isServerAnswer(false, 200,
        QByteArray(R"({"status":"success","message":"ok"})")));
    // Defensive: even if the status attribute were somehow missing, a reply
    // with no error has always been treated as a success dispatch.
    QVERIFY(HttpForm::isServerAnswer(false, 0, QByteArray("{}")));
}

void TestHttpForm::httpErrorStatusWithBodyIsAServerAnswer()
{
    // requireAdminAuth() (deliverables/loams_api/auth_helper.php) answers a bad
    // or missing admin key with HTTP 401 AND a real JSON body. Qt surfaces the
    // 401 as QNetworkReply::ContentAccessDenied — a reply error — but the body
    // is the whole point, so this must reach the decode seam.
    QVERIFY(HttpForm::isServerAnswer(true, 401,
        QByteArray(R"({"status":"error","message":"Invalid admin key"})")));
    QVERIFY(HttpForm::isServerAnswer(true, 401,
        QByteArray(R"({"status":"error","message":"Admin authentication required"})")));
    // Same for the helper's 500 branches, which also carry a JSON body.
    QVERIFY(HttpForm::isServerAnswer(true, 500,
        QByteArray(R"({"status":"error","message":"Authentication check failed"})")));
}

void TestHttpForm::transportFailureWithNoStatusIsNotAServerAnswer()
{
    // DNS failure / connection refused / timeout: no response line was ever
    // received, so there is no status code and no body. Still a network error.
    QVERIFY(!HttpForm::isServerAnswer(true, 0, QByteArray()));
}

void TestHttpForm::httpErrorStatusWithEmptyBodyIsNotAServerAnswer()
{
    // A status code with nothing behind it (e.g. a proxy 502 with an empty
    // body) gives the decode seam nothing to classify, so it would degrade to
    // a meaningless generic failure. Report it as what it is: no usable answer.
    QVERIFY(!HttpForm::isServerAnswer(true, 502, QByteArray()));
    QVERIFY(!HttpForm::isServerAnswer(true, 401, QByteArray()));
}

void TestHttpForm::httpErrorStatusWithNonJsonBodyIsAServerAnswer()
{
    // A PHP fatal error emits HTML with a 500. That IS the server answering,
    // so it must not be swallowed as a network error: it reaches the decode
    // seam, which yields an empty JSON object and therefore the ViewModel's
    // generic "Failed to ..." message rather than "Network error".
    QVERIFY(HttpForm::isServerAnswer(true, 500,
        QByteArray("<br /><b>Fatal error</b>: call to undefined function")));
}

QTEST_MAIN(TestHttpForm)
#include "tst_httpform.moc"
