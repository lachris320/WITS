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
    ApiConfig::setBaseUrl(QStringLiteral("http://localhost/loams_api/"));
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
    ApiConfig::setBaseUrl(QStringLiteral("http://host/loams_api/"));
    ApiConfig::setBaseUrl(QStringLiteral("   "));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://host/loams_api/"));
}

void TestApiConfig::endpointReflectsChangedBase()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://192.168.1.100/loams_api"));
    QCOMPARE(ApiConfig::endpoint("student_login.php").toString(),
             QString("http://192.168.1.100/loams_api/student_login.php"));
}

QTEST_APPLESS_MAIN(TestApiConfig)
#include "tst_apiconfig.moc"
