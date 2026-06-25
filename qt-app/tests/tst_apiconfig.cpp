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

QTEST_APPLESS_MAIN(TestApiConfig)
#include "tst_apiconfig.moc"
