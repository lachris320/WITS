#include <QtTest>
#include <QByteArray>
#include "brandingcontroller.h"
#include "brandtheme.h"

class TestBrandingController : public QObject
{
    Q_OBJECT
private slots:
    void parseSuccessWithConfig();      // full payload -> true, hasConfig, mode/palette/hash/updatedAt decoded
    void parseSuccessNullBranding();    // {"status":"success","branding":null} -> true, hasConfig=false, out untouched
    void parseInvalidJson();            // "not json" -> false, errorMsg non-empty
    void parseStatusError();            // {"status":"error","message":"Query failed"} -> false, errorMsg contains "Query failed"
    void parseMalformedPaletteFallsBack(); // palette:"garbage" (non-object) -> true, hasConfig, palette == fallback
    void parseManualMode();             // theme_mode:"manual" -> ThemeMode::Manual
};

void TestBrandingController::parseSuccessWithConfig()
{
    const QByteArray json = R"({
        "status": "success",
        "branding": {
            "theme_mode": "auto",
            "palette": {
                "admin_primary": "#123456",
                "kiosk_primary": "#654321"
            },
            "logo_hash": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
            "updated_at": "2026-07-06 08:00:00"
        }
    })";

    BrandingConfig config;
    bool hasConfig = false;
    QString errorMsg;
    QVERIFY(BrandingController::parseBrandingResponse(json, &config, &hasConfig, &errorMsg));
    QVERIFY(hasConfig);
    QVERIFY(errorMsg.isEmpty());
    QVERIFY(config.mode == ThemeMode::Auto);
    QCOMPARE(config.palette.adminPrimary, QColor("#123456"));
    QCOMPARE(config.palette.kioskPrimary, QColor("#654321"));
    // Fields absent from the wire palette keep their fallback values.
    QCOMPARE(config.palette.card, BrandTheme::fallbackPalette().card);
    QCOMPARE(config.logoHash,
             QString("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    // Wire format yyyy-MM-dd HH:mm:ss parsed end-to-end via configFromJson.
    QVERIFY(config.updatedAt.isValid());
    QCOMPARE(config.updatedAt,
             QDateTime(QDate(2026, 7, 6), QTime(8, 0, 0)));
}

void TestBrandingController::parseSuccessNullBranding()
{
    const QByteArray json = R"({"status": "success", "branding": null})";

    // Pre-load *out with sentinel values so "untouched" is observable.
    BrandingConfig config;
    config.mode = ThemeMode::Manual;
    config.logoHash = QStringLiteral("sentinel-hash");
    config.updatedAt = QDateTime(QDate(2020, 1, 1), QTime(0, 0, 0));

    bool hasConfig = true;   // must be reset to false
    QString errorMsg;
    QVERIFY(BrandingController::parseBrandingResponse(json, &config, &hasConfig, &errorMsg));
    QVERIFY(!hasConfig);
    QVERIFY(errorMsg.isEmpty());
    QVERIFY(config.mode == ThemeMode::Manual);                       // untouched
    QCOMPARE(config.logoHash, QString("sentinel-hash"));             // untouched
    QCOMPARE(config.updatedAt, QDateTime(QDate(2020, 1, 1), QTime(0, 0, 0)));
}

void TestBrandingController::parseInvalidJson()
{
    BrandingConfig config;
    bool hasConfig = false;
    QString errorMsg;
    QVERIFY(!BrandingController::parseBrandingResponse(QByteArray("not json"),
                                                       &config, &hasConfig, &errorMsg));
    QVERIFY(!hasConfig);
    QVERIFY(!errorMsg.isEmpty());
}

void TestBrandingController::parseStatusError()
{
    const QByteArray json = R"({"status": "error", "message": "Query failed"})";

    BrandingConfig config;
    bool hasConfig = false;
    QString errorMsg;
    QVERIFY(!BrandingController::parseBrandingResponse(json, &config, &hasConfig, &errorMsg));
    QVERIFY(!hasConfig);
    QVERIFY(errorMsg.contains(QStringLiteral("Query failed")));
}

void TestBrandingController::parseMalformedPaletteFallsBack()
{
    const QByteArray json = R"({
        "status": "success",
        "branding": {
            "theme_mode": "auto",
            "palette": "garbage",
            "logo_hash": "",
            "updated_at": "2026-07-06 08:00:00"
        }
    })";

    BrandingConfig config;
    bool hasConfig = false;
    QString errorMsg;
    QVERIFY(BrandingController::parseBrandingResponse(json, &config, &hasConfig, &errorMsg));
    QVERIFY(hasConfig);
    QVERIFY(config.palette == BrandTheme::fallbackPalette());
}

void TestBrandingController::parseManualMode()
{
    const QByteArray json = R"({
        "status": "success",
        "branding": {
            "theme_mode": "manual",
            "palette": {},
            "logo_hash": "",
            "updated_at": "2026-07-06 08:00:00"
        }
    })";

    BrandingConfig config;
    bool hasConfig = false;
    QString errorMsg;
    QVERIFY(BrandingController::parseBrandingResponse(json, &config, &hasConfig, &errorMsg));
    QVERIFY(hasConfig);
    QVERIFY(config.mode == ThemeMode::Manual);
}

QTEST_GUILESS_MAIN(TestBrandingController)
#include "tst_brandingcontroller.moc"
