#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include "apiconfig.h"
#include "apiconfigloader.h"

using ApiConfigLoader::Reason;
using ApiConfigLoader::Source;

class TestApiConfigLoader : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void envWinsOverIni();
    void iniUsedWhenEnvEmpty();
    void defaultWhenNeitherPresent();
    void missingIniUsesEnv();
    void iniKeyPresentButBlankIsRejected();
    void iniKeyPresentButEmptyIsRejected();
    void envSetButBlankIsRejectedAndFallsThroughToIni();
    void envSetButEmptyIsRejectedAndFallsToDefault();
    void envUnsetIsSilent();
    void iniCommaValueIsNotSilentlyDropped();
    void applyFromRuntimeResetsToDefaultWhenNothingConfigured();
    void applyFromRuntimeResetsToDefaultWhenAllRejected();
    void validEnvDoesNotConsultInvalidIni();
    void invalidEnvFallsThroughToIni();
    void invalidEnvAndInvalidIniGivesEmpty();
    void invalidEnvAndMissingIniGivesEmpty();
    void iniWithoutBaseUrlKeyIsSilent();
    void malformedIniFileIsReported();
    void unreadableIniFileIsReported();
    void applyFromRuntimeWarnsOnUnreadableIni();
    void rejectedCredentialsAreRedacted();
    void rejectedQueryAndFragmentAreRedacted_data();
    void rejectedQueryAndFragmentAreRedacted();
    void rejectedValueHasNoControlCharacters_data();
    void rejectedValueHasNoControlCharacters();
    void fullChainSetsEndpoint();
    void applyFromRuntimeWarnsOnInvalidEnvAndUsesIni();
    void applyFromRuntimeKeepsDefaultWhenAllInvalid();

private:
    // Write an ini with the given BaseURL line and return its path.
    QString writeIni(QTemporaryDir &dir, const QString &baseUrlLine)
    {
        const QString path = dir.path() + QStringLiteral("/config.ini");
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return QString();
        QTextStream(&f) << "[Server]\n" << baseUrlLine << "\n";
        f.close();
        return path;
    }

    static QString missingIni() { return QStringLiteral("C:/no/such/config.ini"); }
};

void TestApiConfigLoader::init()
{
    qunsetenv("WITS_API_BASE_URL");
}

void TestApiConfigLoader::cleanup()
{
    // Process-global state: restore the localhost default and the env so no
    // later case (or target) observes this case's configuration.
    qunsetenv("WITS_API_BASE_URL");
    QVERIFY(ApiConfig::setBaseUrl(ApiConfig::defaultBaseUrl()));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://localhost/loams_api/"));
}

void TestApiConfigLoader::envWinsOverIni()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://ini.test/loams_api"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("http://env.test/loams_api"), ini);
    QCOMPARE(r.url, QString("http://env.test/loams_api/"));
    QVERIFY(r.source == Source::Environment);
    QVERIFY(r.rejected.isEmpty());
}

void TestApiConfigLoader::iniUsedWhenEnvEmpty()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://ini.test/loams_api"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, ini);
    QCOMPARE(r.url, QString("http://ini.test/loams_api/"));
    QVERIFY(r.source == Source::ConfigIni);
    QVERIFY(r.rejected.isEmpty());
}

void TestApiConfigLoader::defaultWhenNeitherPresent()
{
    // No env, and a path to a file that does not exist -> empty (caller keeps default).
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, missingIni());
    QCOMPARE(r.url, QString());
    QVERIFY(r.source == Source::Default);
    QVERIFY(r.rejected.isEmpty());
}

void TestApiConfigLoader::missingIniUsesEnv()
{
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("https://env.test/loams_api/"),
                                                   missingIni());
    QCOMPARE(r.url, QString("https://env.test/loams_api/"));
    QVERIFY(r.source == Source::Environment);
}

void TestApiConfigLoader::iniKeyPresentButBlankIsRejected()
{
    // The key is present but blank: invalid config, NOT "not configured".
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=   "));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, ini);
    QCOMPARE(r.url, QString());
    QVERIFY(r.source == Source::Default);
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::ConfigIni);
    QVERIFY(r.rejected.at(0).reason == Reason::Blank);
}

void TestApiConfigLoader::iniKeyPresentButEmptyIsRejected()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL="));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, ini);
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).reason == Reason::Blank);
}

void TestApiConfigLoader::envSetButBlankIsRejectedAndFallsThroughToIni()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://ini.test/loams_api"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("   "), ini);
    QCOMPARE(r.url, QString("http://ini.test/loams_api/"));
    QVERIFY(r.source == Source::ConfigIni);
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::Environment);
    QVERIFY(r.rejected.at(0).reason == Reason::Blank);
}

void TestApiConfigLoader::envSetButEmptyIsRejectedAndFallsToDefault()
{
    const auto r = ApiConfigLoader::resolveBaseUrl(QString(""), missingIni());
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::Environment);
    QVERIFY(r.rejected.at(0).reason == Reason::Blank);
}

void TestApiConfigLoader::envUnsetIsSilent()
{
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, missingIni());
    QCOMPARE(r.url, QString());
    QVERIFY(r.rejected.isEmpty());
}

void TestApiConfigLoader::iniCommaValueIsNotSilentlyDropped()
{
    // QSettings splits an unquoted comma value into a QStringList; it must be
    // read back as the literal text, not as an empty "not configured" value.
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://ini.test/a,b"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, ini);
    QCOMPARE(r.url, QString("http://ini.test/a,b/"));
    QVERIFY(r.rejected.isEmpty());
}

void TestApiConfigLoader::applyFromRuntimeResetsToDefaultWhenNothingConfigured()
{
    // Legacy WITS re-runs applyFromRuntime on an in-process restart; a URL
    // applied earlier must not survive a resolution that yields nothing.
    QTemporaryDir configured;
    QVERIFY(!writeIni(configured, QStringLiteral("BaseURL=http://ini.test/loams_api")).isEmpty());
    ApiConfigLoader::applyFromRuntime(configured.path());
    QCOMPARE(ApiConfig::baseUrl(), QString("http://ini.test/loams_api/"));

    QTemporaryDir empty;   // no config.ini, env unset
    ApiConfigLoader::applyFromRuntime(empty.path());
    QCOMPARE(ApiConfig::baseUrl(), ApiConfig::defaultBaseUrl());
}

void TestApiConfigLoader::applyFromRuntimeResetsToDefaultWhenAllRejected()
{
    QVERIFY(ApiConfig::setBaseUrl(QStringLiteral("http://stale.test/loams_api/")));
    QTemporaryDir empty;
    qputenv("WITS_API_BASE_URL", "   ");   // set but blank

    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("WITS_API_BASE_URL.*blank")));
    // The fallback warning must print the REAL default, not the stale value.
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("default http://localhost/loams_api/")));
    ApiConfigLoader::applyFromRuntime(empty.path());
    QCOMPARE(ApiConfig::baseUrl(), ApiConfig::defaultBaseUrl());
}

void TestApiConfigLoader::validEnvDoesNotConsultInvalidIni()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=ftp://ini.test/loams_api/"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("http://env.test/loams_api/"), ini);
    QCOMPARE(r.url, QString("http://env.test/loams_api/"));
    QVERIFY(r.rejected.isEmpty());   // lower-precedence source never reached
}

void TestApiConfigLoader::invalidEnvFallsThroughToIni()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://ini.test/loams_api"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("ftp://env.test/loams_api/"), ini);
    QCOMPARE(r.url, QString("http://ini.test/loams_api/"));
    QVERIFY(r.source == Source::ConfigIni);
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::Environment);
    QCOMPARE(r.rejected.at(0).value, QString("ftp://env.test/loams_api/"));
}

void TestApiConfigLoader::invalidEnvAndInvalidIniGivesEmpty()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=localhost/loams_api"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("//evil.example/loams_api"), ini);
    QCOMPARE(r.url, QString());
    QVERIFY(r.source == Source::Default);
    QCOMPARE(r.rejected.size(), 2);
    QVERIFY(r.rejected.at(0).source == Source::Environment);
    QCOMPARE(r.rejected.at(0).value, QString("//evil.example/loams_api"));
    QVERIFY(r.rejected.at(1).source == Source::ConfigIni);
    QCOMPARE(r.rejected.at(1).value, QString("localhost/loams_api"));
}

void TestApiConfigLoader::iniWithoutBaseUrlKeyIsSilent()
{
    // A readable ini that simply doesn't configure the URL is "not
    // configured", not "invalid": no rejection.
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("Other=1"));
    QVERIFY(!ini.isEmpty());
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, ini);
    QCOMPARE(r.url, QString());
    QVERIFY(r.rejected.isEmpty());
}

void TestApiConfigLoader::malformedIniFileIsReported()
{
    // Unterminated section header: QSettings reports FormatError.
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/config.ini");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream(&f) << "[Server\nBaseURL=http://ini.test/loams_api/\n";
    }
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, path);
    QCOMPARE(r.url, QString());
    QVERIFY(r.source == Source::Default);
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::ConfigIni);
    QVERIFY(r.rejected.at(0).reason == Reason::UnreadableFile);
}

void TestApiConfigLoader::unreadableIniFileIsReported()
{
    // A config.ini that exists but cannot be opened as a file (here: a
    // directory of that name) -> QSettings AccessError -> reported.
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/config.ini");
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("config.ini")));
    const auto r = ApiConfigLoader::resolveBaseUrl(std::nullopt, path);
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::ConfigIni);
    QVERIFY(r.rejected.at(0).reason == Reason::UnreadableFile);
}

void TestApiConfigLoader::applyFromRuntimeWarnsOnUnreadableIni()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("config.ini")));

    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("config\\.ini.*unreadable or malformed")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("default")));
    ApiConfigLoader::applyFromRuntime(dir.path());
    QCOMPARE(ApiConfig::baseUrl(), QString("http://localhost/loams_api/"));
}

void TestApiConfigLoader::invalidEnvAndMissingIniGivesEmpty()
{
    const auto r = ApiConfigLoader::resolveBaseUrl(QStringLiteral("javascript:alert(1)"),
                                                   missingIni());
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(r.rejected.at(0).source == Source::Environment);
}

void TestApiConfigLoader::rejectedCredentialsAreRedacted()
{
    // A rejected value is reported for diagnostics, but embedded credentials
    // must never reach the log.
    const auto r = ApiConfigLoader::resolveBaseUrl(
        QStringLiteral("http://user:s3cret@env.test/loams_api/"), missingIni());
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY(!r.rejected.at(0).value.contains(QStringLiteral("s3cret")));
    QVERIFY(!r.rejected.at(0).value.contains(QStringLiteral("user")));
    QVERIFY(r.rejected.at(0).value.contains(QStringLiteral("env.test")));
    QVERIFY(r.rejected.at(0).value.contains(QStringLiteral("credentials removed")));
}

void TestApiConfigLoader::rejectedQueryAndFragmentAreRedacted_data()
{
    QTest::addColumn<QString>("raw");
    QTest::newRow("query secret") << "http://srv.test/loams_api/x.php?admin_key=SECRET";
    QTest::newRow("fragment secret") << "http://srv.test/loams_api/#SECRET";
    QTest::newRow("unparseable with query") << "ht!tp://srv.test/?admin_key=SECRET";
    QTest::newRow("unparseable with fragment") << "ht!tp://srv.test/#SECRET";
    QTest::newRow("scheme-relative with query") << "//srv.test/x?admin_key=SECRET";
}

void TestApiConfigLoader::rejectedQueryAndFragmentAreRedacted()
{
    QFETCH(QString, raw);
    const auto r = ApiConfigLoader::resolveBaseUrl(raw, missingIni());
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    QVERIFY2(!r.rejected.at(0).value.contains(QStringLiteral("SECRET")),
             qPrintable(r.rejected.at(0).value));
}

void TestApiConfigLoader::rejectedValueHasNoControlCharacters_data()
{
    QTest::addColumn<QString>("raw");
    QTest::newRow("CRLF in parseable URL")
        << QStringLiteral("http://srv.test/a\r\nWARNING: forged line");
    QTest::newRow("LF in unparseable value")
        << QStringLiteral("not a url\nWARNING: forged line");
    QTest::newRow("tab and bell") << QStringLiteral("ftp://srv.test/\t\a");
}

void TestApiConfigLoader::rejectedValueHasNoControlCharacters()
{
    QFETCH(QString, raw);
    const auto r = ApiConfigLoader::resolveBaseUrl(raw, missingIni());
    QCOMPARE(r.url, QString());
    QCOMPARE(r.rejected.size(), 1);
    const QString logged = r.rejected.at(0).value;
    for (const QChar c : logged)
        QVERIFY2(c.category() != QChar::Other_Control, qPrintable(logged));
    QVERIFY(!logged.contains(QLatin1Char('\n')));
    QVERIFY(!logged.contains(QLatin1Char('\r')));
}

void TestApiConfigLoader::fullChainSetsEndpoint()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://192.168.1.100/loams_api"));
    QVERIFY(!ini.isEmpty());
    QVERIFY(ApiConfig::setBaseUrl(ApiConfigLoader::resolveBaseUrl(std::nullopt, ini).url));
    QCOMPARE(ApiConfig::endpoint("student_login.php").toString(),
             QString("http://192.168.1.100/loams_api/student_login.php"));
}

void TestApiConfigLoader::applyFromRuntimeWarnsOnInvalidEnvAndUsesIni()
{
    QTemporaryDir dir;
    QVERIFY(!writeIni(dir, QStringLiteral("BaseURL=http://ini.test/loams_api")).isEmpty());
    qputenv("WITS_API_BASE_URL", "ftp://env.test/loams_api/");

    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("ftp://env\\.test.*WITS_API_BASE_URL")));
    ApiConfigLoader::applyFromRuntime(dir.path());
    QCOMPARE(ApiConfig::baseUrl(), QString("http://ini.test/loams_api/"));
}

void TestApiConfigLoader::applyFromRuntimeKeepsDefaultWhenAllInvalid()
{
    QTemporaryDir dir;
    QVERIFY(!writeIni(dir, QStringLiteral("BaseURL=file:///C:/loams_api/")).isEmpty());
    qputenv("WITS_API_BASE_URL", "http://host.test/?q=1");

    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("WITS_API_BASE_URL")));
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("config\\.ini")));
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("default")));
    ApiConfigLoader::applyFromRuntime(dir.path());
    QCOMPARE(ApiConfig::baseUrl(), QString("http://localhost/loams_api/"));
}

QTEST_MAIN(TestApiConfigLoader)
#include "tst_apiconfigloader.moc"
