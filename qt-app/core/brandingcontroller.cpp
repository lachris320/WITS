#include "brandingcontroller.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include "apiconfig.h"
#include "brandtheme.h"

BrandingController::BrandingController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

void BrandingController::fetchRemoteConfig()
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("get_branding.php")));
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(reply->errorString());
            return;
        }
        BrandingConfig config;
        bool hasConfig = false;
        QString errorMsg;
        if (!parseBrandingResponse(reply->readAll(), &config, &hasConfig, &errorMsg)) {
            emit fetchError(errorMsg);
            return;
        }
        if (hasConfig)
            emit remoteConfigLoaded(config);
        // branding: null -> server not branded yet; nothing to sync.
    });
}

void BrandingController::saveBranding(const BrandingConfig &config, const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("save_branding.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("admin_key"), adminKey);
    postData.addQueryItem(QStringLiteral("theme_mode"),
                          config.mode == ThemeMode::Manual ? QStringLiteral("manual")
                                                           : QStringLiteral("auto"));
    postData.addQueryItem(QStringLiteral("palette"),
                          QString::fromUtf8(QJsonDocument(BrandTheme::paletteToJson(config.palette))
                                                .toJson(QJsonDocument::Compact)));
    postData.addQueryItem(QStringLiteral("logo_hash"), config.logoHash);

    QNetworkReply *reply =
        m_nam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit saveFinished(false, reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit saveFinished(false, QStringLiteral("Invalid server response."));
            return;
        }
        const QJsonObject obj = doc.object();
        const bool ok = obj.value(QStringLiteral("status")).toString()
                        == QLatin1String("success");
        emit saveFinished(ok, obj.value(QStringLiteral("message")).toString());
    });
}

bool BrandingController::parseBrandingResponse(const QByteArray &json,
                                               BrandingConfig *out,
                                               bool *hasConfig,
                                               QString *errorMsg)
{
    if (hasConfig) *hasConfig = false;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("Invalid branding response: not a JSON object.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        if (errorMsg) {
            *errorMsg = obj.value(QStringLiteral("message")).toString();
            if (errorMsg->isEmpty())
                *errorMsg = QStringLiteral("Branding request failed.");
        }
        return false;
    }
    const QJsonValue branding = obj.value(QStringLiteral("branding"));
    if (branding.isNull() || branding.isUndefined())
        return true; // server not branded yet
    if (out)
        *out = BrandTheme::configFromJson(branding.toObject());
    if (hasConfig) *hasConfig = true;
    return true;
}
