#include <QtTest>
#include <QUrl>
#include <QUrlQuery>
#include "apiconfig.h"

class TestApiConfig : public QObject
{
    Q_OBJECT
private slots:
    void baseUrlValue();
    void endpointPlainFilename();
    void endpointLeadingSlash();
    void endpointMultiSegmentPath();
    void endpointQueryCanBeAppended();
    void cleanup();
    void setBaseUrlNormalizesMissingSlash();
    void setBaseUrlCollapsesMultipleSlashes();
    void setBaseUrlIgnoresEmpty();
    void endpointReflectsChangedBase();
    void defaultBaseUrlMatchesInitialValue();
    void normalizedBaseUrlAccepts_data();
    void normalizedBaseUrlAccepts();
    void normalizedBaseUrlRejects_data();
    void normalizedBaseUrlRejects();
    void setBaseUrlRejectsInvalidAndKeepsPrevious_data();
    void setBaseUrlRejectsInvalidAndKeepsPrevious();
    void setBaseUrlReturnsTrueAndAppliesNormalized();
    void resetBaseUrlRestoresDefault();
};

void TestApiConfig::baseUrlValue()
{
    QCOMPARE(ApiConfig::baseUrl(), QString("http://localhost/loams_api/"));
}

void TestApiConfig::endpointPlainFilename()
{
    QCOMPARE(ApiConfig::endpoint("get_departments.php").toString(),
             QString("http://localhost/loams_api/get_departments.php"));
}

void TestApiConfig::endpointLeadingSlash()
{
    QCOMPARE(ApiConfig::endpoint("/get_departments.php").toString(),
             QString("http://localhost/loams_api/get_departments.php"));
}

void TestApiConfig::endpointMultiSegmentPath()
{
    QCOMPARE(ApiConfig::endpoint("api.php/reports/data").toString(),
             QString("http://localhost/loams_api/api.php/reports/data"));
}

void TestApiConfig::endpointQueryCanBeAppended()
{
    QUrl url = ApiConfig::endpoint("get_courses.php");
    QUrlQuery query;
    query.addQueryItem("department", "CS");
    url.setQuery(query);
    QCOMPARE(url.toString(),
             QString("http://localhost/loams_api/get_courses.php?department=CS"));
}

void TestApiConfig::cleanup()
{
    // The base URL is a process-global mutable; reset after every case so
    // ordering can't leak state into the hardcoded-default assertions.
    ApiConfig::resetBaseUrl();
    QCOMPARE(ApiConfig::baseUrl(), QString("http://localhost/loams_api/"));
}

void TestApiConfig::setBaseUrlNormalizesMissingSlash()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://192.168.1.100/loams_api"));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://192.168.1.100/loams_api/"));
}

void TestApiConfig::setBaseUrlCollapsesMultipleSlashes()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://host/loams_api///"));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://host/loams_api/"));
}

void TestApiConfig::setBaseUrlIgnoresEmpty()
{
    QVERIFY(ApiConfig::setBaseUrl(QStringLiteral("http://host/loams_api/")));
    QVERIFY(!ApiConfig::setBaseUrl(QStringLiteral("   ")));
    QVERIFY(!ApiConfig::setBaseUrl(QString()));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://host/loams_api/"));
}

void TestApiConfig::endpointReflectsChangedBase()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://192.168.1.100/loams_api"));
    QCOMPARE(ApiConfig::endpoint("student_login.php").toString(),
             QString("http://192.168.1.100/loams_api/student_login.php"));
}

void TestApiConfig::defaultBaseUrlMatchesInitialValue()
{
    QCOMPARE(ApiConfig::defaultBaseUrl(), QString("http://localhost/loams_api/"));
}

void TestApiConfig::normalizedBaseUrlAccepts_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("expected");

    QTest::newRow("http with slash")
        << "http://localhost/loams_api/" << "http://localhost/loams_api/";
    QTest::newRow("http without slash")
        << "http://192.168.1.100/loams_api" << "http://192.168.1.100/loams_api/";
    QTest::newRow("https")
        << "https://kiosk.example.com/loams_api/" << "https://kiosk.example.com/loams_api/";
    QTest::newRow("with port")
        << "http://server.test:8080/loams_api" << "http://server.test:8080/loams_api/";
    QTest::newRow("uppercase scheme normalized")
        << "HTTPS://server.test/loams_api" << "https://server.test/loams_api/";
    QTest::newRow("surrounding whitespace trimmed")
        << "  http://server.test/loams_api  " << "http://server.test/loams_api/";
    QTest::newRow("host only gets root slash")
        << "http://server.test" << "http://server.test/";
    QTest::newRow("multiple trailing slashes collapsed")
        << "http://server.test/loams_api///" << "http://server.test/loams_api/";
    // '@' in the PATH is not userinfo: accepted (the authority has no '@').
    QTest::newRow("http default port stripped")
        << "http://host.test:80/a/" << "http://host.test/a/";
    QTest::newRow("https default port stripped")
        << "https://host.test:443/" << "https://host.test/";
    QTest::newRow("http keeps non-default 443")
        << "http://host.test:443/" << "http://host.test:443/";
    QTest::newRow("at sign in path")
        << "http://host.test/a@b/" << "http://host.test/a@b/";
}

void TestApiConfig::normalizedBaseUrlAccepts()
{
    QFETCH(QString, raw);
    QFETCH(QString, expected);
    QCOMPARE(ApiConfig::normalizedBaseUrl(raw), expected);
}

void TestApiConfig::normalizedBaseUrlRejects_data()
{
    QTest::addColumn<QString>("raw");

    QTest::newRow("empty") << "";
    QTest::newRow("whitespace") << "   ";
    QTest::newRow("ftp scheme") << "ftp://files.example.com/loams_api/";
    QTest::newRow("file scheme") << "file:///C:/loams_api/";
    QTest::newRow("javascript scheme") << "javascript:alert(1)";
    QTest::newRow("mailto scheme") << "mailto:admin@example.com";
    QTest::newRow("scheme-relative") << "//evil.example/loams_api";
    QTest::newRow("no host") << "http://";
    QTest::newRow("no scheme") << "localhost/loams_api";
    QTest::newRow("user and password") << "http://user:pass@host.test/";
    QTest::newRow("user only") << "http://user@host.test/";
    QTest::newRow("empty userinfo") << "http://@host.test/";
    QTest::newRow("empty user and password") << "http://:@host.test/";
    QTest::newRow("empty userinfo with path") << "http://@host.test/loams_api";
    QTest::newRow("query") << "http://host.test/?q=1";
    QTest::newRow("empty query") << "http://host.test/loams_api/?";
    QTest::newRow("fragment") << "http://host.test/#f";
    QTest::newRow("garbage scheme") << "ht!tp://x";
    QTest::newRow("port out of range") << "http://host.test:99999/";
    QTest::newRow("port zero") << "http://host.test:0/";
}

void TestApiConfig::normalizedBaseUrlRejects()
{
    QFETCH(QString, raw);
    QCOMPARE(ApiConfig::normalizedBaseUrl(raw), QString());
}

void TestApiConfig::setBaseUrlRejectsInvalidAndKeepsPrevious_data()
{
    QTest::addColumn<QString>("raw");

    QTest::newRow("ftp scheme") << "ftp://files.example.com/loams_api/";
    QTest::newRow("scheme-relative") << "//evil.example/loams_api";
    QTest::newRow("no scheme") << "localhost/loams_api";
    QTest::newRow("credentials") << "http://user:pass@host.test/";
    QTest::newRow("query") << "http://host.test/?q=1";
    QTest::newRow("garbage") << "ht!tp://x";
}

void TestApiConfig::setBaseUrlRejectsInvalidAndKeepsPrevious()
{
    QFETCH(QString, raw);
    QVERIFY(ApiConfig::setBaseUrl(QStringLiteral("http://previous.test/loams_api/")));
    QVERIFY(!ApiConfig::setBaseUrl(raw));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://previous.test/loams_api/"));
}

void TestApiConfig::setBaseUrlReturnsTrueAndAppliesNormalized()
{
    QVERIFY(ApiConfig::setBaseUrl(QStringLiteral("HTTP://server.test:8080/loams_api")));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://server.test:8080/loams_api/"));
}

void TestApiConfig::resetBaseUrlRestoresDefault()
{
    QVERIFY(ApiConfig::setBaseUrl(QStringLiteral("http://stale.test/loams_api/")));
    ApiConfig::resetBaseUrl();
    QCOMPARE(ApiConfig::baseUrl(), ApiConfig::defaultBaseUrl());
}

QTEST_APPLESS_MAIN(TestApiConfig)
#include "tst_apiconfig.moc"
