#include "apiconfigloader.h"

#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>

#include "apiconfig.h"

namespace ApiConfigLoader {

namespace {

// A rejected value is logged for diagnostics; strip any userinfo so a
// credential typed into the URL never reaches the log.
QString redactedForLog(const QString &value)
{
    if (!value.contains(QLatin1Char('@')))
        return value;
    const QUrl parsed(value, QUrl::TolerantMode);
    if (parsed.isValid() && !parsed.userInfo().isEmpty())
        return parsed.toString(QUrl::RemoveUserInfo)
               + QStringLiteral(" [credentials removed]");
    // Unparseable but contains '@': it may still embed a credential.
    return QStringLiteral("<withheld: value contains '@'>");
}

// Try one source: a blank value means "not configured" (no rejection); a
// present-but-invalid value is recorded and the caller falls through.
bool tryAccept(const QString &raw, Source source, Resolution &out)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return false;
    const QString normalized = ApiConfig::normalizedBaseUrl(trimmed);
    if (normalized.isEmpty()) {
        out.rejected.append(Rejection{source, redactedForLog(trimmed)});
        return false;
    }
    out.url = normalized;
    out.source = source;
    return true;
}

// Reads [Server] BaseURL. A missing file or missing key yields an empty value
// ("not configured"); an EXISTING file QSettings cannot read or parse sets
// `unreadable` so the caller reports it instead of falling back silently.
QString iniValue(const QString &iniFilePath, bool &unreadable)
{
    unreadable = false;
    if (!QFileInfo::exists(iniFilePath))
        return QString();
    QSettings ini(iniFilePath, QSettings::IniFormat);
    if (ini.status() != QSettings::NoError) {
        unreadable = true;
        return QString();
    }
    return ini.value(QStringLiteral("Server/BaseURL")).toString();
}

QString describe(Source source, const QString &iniFilePath)
{
    switch (source) {
    case Source::Environment:
        return QStringLiteral("the WITS_API_BASE_URL environment variable");
    case Source::ConfigIni:
        return QStringLiteral("[Server] BaseURL in %1").arg(iniFilePath);
    case Source::Default:
        break;
    }
    return QStringLiteral("the built-in default");
}

} // namespace

Resolution resolveBaseUrl(const QString &envValue, const QString &iniFilePath)
{
    Resolution r;
    // 1. Environment override (dev/CI).
    if (tryAccept(envValue, Source::Environment, r))
        return r;
    // 2. config.ini beside the exe (only read when the env did not win).
    bool iniUnreadable = false;
    const QString fromIni = iniValue(iniFilePath, iniUnreadable);
    if (iniUnreadable) {
        Rejection rej;
        rej.source = Source::ConfigIni;
        rej.fileUnreadable = true;
        r.rejected.append(rej);
        return r;
    }
    if (tryAccept(fromIni, Source::ConfigIni, r))
        return r;
    // 3. Nothing usable -- caller keeps ApiConfig's localhost default.
    return r;
}

void applyFromRuntime(const QString &appDirPath)
{
    const QString iniPath = appDirPath + QStringLiteral("/config.ini");
    const Resolution r = resolveBaseUrl(qEnvironmentVariable("WITS_API_BASE_URL"), iniPath);

    for (const Rejection &rej : r.rejected) {
        if (rej.fileUnreadable) {
            qWarning().noquote()
                << QStringLiteral("Ignoring backend config %1: the file exists but is "
                                  "unreadable or malformed.")
                       .arg(iniPath);
            continue;
        }
        qWarning().noquote()
            << QStringLiteral("Ignoring invalid backend URL \"%1\" from %2: expected "
                              "http(s)://host[:port]/path with no credentials, query "
                              "or fragment.")
                   .arg(rej.value, describe(rej.source, iniPath));
    }

    if (r.url.isEmpty()) {
        if (!r.rejected.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("No valid backend URL configured; using the default %1")
                       .arg(ApiConfig::baseUrl());
        }
        return;
    }

    ApiConfig::setBaseUrl(r.url);
    qInfo().noquote() << QStringLiteral("Backend base URL: %1 (from %2)")
                             .arg(ApiConfig::baseUrl(), describe(r.source, iniPath));
}

} // namespace ApiConfigLoader
