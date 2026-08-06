#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "apiconfig.h"
#include "apiconfigloader.h"

class TestApiConfigLoader : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void envWinsOverIni();
    void iniUsedWhenEnvEmpty();
    void defaultWhenNeitherPresent();
    void malformedIniFallsThrough();
    void fullChainSetsEndpoint();

private:
    // Write an ini with the given BaseURL line and return its path.
    QString writeIni(QTemporaryDir &dir, const QString &baseUrlLine)
    {
        const QString path = dir.path() + QStringLiteral("/config.ini");
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream(&f) << "[Server]\n" << baseUrlLine << "\n";
        f.close();
        return path;
    }
};

void TestApiConfigLoader::cleanup()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://localhost/loams_api/"));
}

void TestApiConfigLoader::envWinsOverIni()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://from-ini/loams_api"));
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QStringLiteral("http://from-env/loams_api"), ini),
             QString("http://from-env/loams_api"));
}

void TestApiConfigLoader::iniUsedWhenEnvEmpty()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://from-ini/loams_api"));
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QString(), ini),
             QString("http://from-ini/loams_api"));
}

void TestApiConfigLoader::defaultWhenNeitherPresent()
{
    // No env, and a path to a file that does not exist -> empty (caller keeps default).
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QString(),
                 QStringLiteral("C:/no/such/config.ini")),
             QString());
}

void TestApiConfigLoader::malformedIniFallsThrough()
{
    QTemporaryDir dir;
    // Right group, empty value -> treated as not configured.
    const QString ini = writeIni(dir, QStringLiteral("BaseURL="));
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QString(), ini), QString());
}

void TestApiConfigLoader::fullChainSetsEndpoint()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://192.168.1.100/loams_api"));
    ApiConfig::setBaseUrl(ApiConfigLoader::resolveBaseUrl(QString(), ini));
    QCOMPARE(ApiConfig::endpoint("student_login.php").toString(),
             QString("http://192.168.1.100/loams_api/student_login.php"));
}

QTEST_MAIN(TestApiConfigLoader)
#include "tst_apiconfigloader.moc"
