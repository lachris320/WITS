#ifndef BRANDINGCONTROLLER_H
#define BRANDINGCONTROLLER_H
#include <QByteArray>
#include <QObject>
#include <QString>
#include "brandthemedata.h"

class QNetworkAccessManager;

// Thin network controller for the branding backend (get_branding.php /
// save_branding.php), following the VisitorController pattern: injected
// QNetworkAccessManager, async signals, and a pure static parser that the
// unit test feeds synthetic payloads (no live network in tests).
class BrandingController : public QObject
{
    Q_OBJECT
public:
    explicit BrandingController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Async — result arrives via remoteConfigLoaded / fetchError. A server
    // with no branding row (branding: null) emits neither: there is simply
    // nothing to sync yet.
    void fetchRemoteConfig();

    // Async admin-auth write (code hook in v1 — no UI calls this yet; the
    // admin key is a parameter because the client does not retain it).
    void saveBranding(const BrandingConfig &config, const QString &adminKey);

    // Pure, unit-testable. Returns false + *errorMsg on malformed input or
    // status != success. On success *hasConfig says whether branding was
    // non-null and *out holds the decoded config (untouched when null).
    static bool parseBrandingResponse(const QByteArray &json,
                                      BrandingConfig *out,
                                      bool *hasConfig,
                                      QString *errorMsg);

signals:
    void remoteConfigLoaded(const BrandingConfig &config);
    void fetchError(const QString &message);
    void saveFinished(bool ok, const QString &message);

private:
    QNetworkAccessManager *m_nam; // injected, not owned
};

#endif // BRANDINGCONTROLLER_H
