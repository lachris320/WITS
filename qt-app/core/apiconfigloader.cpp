#include "apiconfigloader.h"

#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QVariant>

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

// Try one PRESENT source (env var set / ini key present). A blank value is a
// rejection (Reason::Blank), not "not configured"; an invalid one is recorded
// with its redacted value. Either way the caller falls through.
bool tryAccept(const QString &raw, Source source, Resolution &out)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        out.rejected.append(Rejection{source, Reason::Blank, QString()});
        return false;
    }
    const QString normalized = ApiConfig::normalizedBaseUrl(trimmed);
    if (normalized.isEmpty()) {
        out.rejected.append(Rejection{source, Reason::InvalidUrl, redactedForLog(trimmed)});
        return false;
    }
    out.url = normalized;
    out.source = source;
    return true;
}

struct IniRead {
    enum class State { NotConfigured, Unreadable, Present };
    State state = State::NotConfigured;
    QString value;
};

// Reads [Server] BaseURL. A missing file or a readable file without the key is
// NotConfigured (silent); an EXISTING file QSettings cannot read or parse is
// Unreadable (reported); otherwise the key is Present (possibly blank).
IniRead readIni(const QString &iniFilePath)
{
    IniRead result;
    if (!QFileInfo::exists(iniFilePath))
        return result;
    QSettings ini(iniFilePath, QSettings::IniFormat);
    if (ini.status() != QSettings::NoError) {
        result.state = IniRead::State::Unreadable;
        return result;
    }
    const QString key = QStringLiteral("Server/BaseURL");
    if (!ini.contains(key))
        return result;
    result.state = IniRead::State::Present;
    const QVariant v = ini.value(key);
    // QSettings splits an unquoted comma-separated value into a list; read it
    // back as the literal text instead of silently treating it as empty.
    result.value = v.metaType().id() == QMetaType::QStringList
                       ? v.toStringList().join(QLatin1Char(','))
                       : v.toString();
    return result;
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

QString warningFor(const Rejection &rej, const QString &iniFilePath)
{
    switch (rej.reason) {
    case Reason::UnreadableFile:
        return QStringLiteral("Ignoring backend config %1: the file exists but is "
                              "unreadable or malformed.")
            .arg(iniFilePath);
    case Reason::Blank:
        return QStringLiteral("Ignoring backend URL from %1: it is set but blank.")
            .arg(describe(rej.source, iniFilePath));
    case Reason::InvalidUrl:
        break;
    }
    return QStringLiteral("Ignoring invalid backend URL \"%1\" from %2: expected "
                          "http(s)://host[:port]/path with no credentials, query "
                          "or fragment.")
        .arg(rej.value, describe(rej.source, iniFilePath));
}

} // namespace

Resolution resolveBaseUrl(const std::optional<QString> &envValue,
                          const QString &iniFilePath)
{
    Resolution r;
    // 1. Environment override (dev/CI) -- only when the variable is set.
    if (envValue.has_value() && tryAccept(*envValue, Source::Environment, r))
        return r;
    // 2. config.ini beside the exe (only read when the env did not win).
    const IniRead ini = readIni(iniFilePath);
    if (ini.state == IniRead::State::Unreadable) {
        r.rejected.append(Rejection{Source::ConfigIni, Reason::UnreadableFile, QString()});
        return r;
    }
    if (ini.state == IniRead::State::Present && tryAccept(ini.value, Source::ConfigIni, r))
        return r;
    // 3. Nothing usable -- caller applies ApiConfig's built-in default.
    return r;
}

void applyFromRuntime(const QString &appDirPath)
{
    const QString iniPath = appDirPath + QStringLiteral("/config.ini");
    const char *envName = "WITS_API_BASE_URL";
    const std::optional<QString> env = qEnvironmentVariableIsSet(envName)
                                           ? std::optional<QString>(qEnvironmentVariable(envName))
                                           : std::nullopt;
    const Resolution r = resolveBaseUrl(env, iniPath);

    for (const Rejection &rej : r.rejected)
        qWarning().noquote() << warningFor(rej, iniPath);

    if (r.url.isEmpty()) {
        // Explicit reset: a re-run (legacy in-process restart) must not keep a
        // previously applied URL.
        ApiConfig::resetBaseUrl();
        if (!r.rejected.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("No valid backend URL configured; using the default %1")
                       .arg(ApiConfig::defaultBaseUrl());
        } else {
            qInfo().noquote() << QStringLiteral("Backend base URL: %1 (the built-in default)")
                                     .arg(ApiConfig::defaultBaseUrl());
        }
        return;
    }

    ApiConfig::setBaseUrl(r.url);
    qInfo().noquote() << QStringLiteral("Backend base URL: %1 (from %2)")
                             .arg(ApiConfig::baseUrl(), describe(r.source, iniPath));
}

} // namespace ApiConfigLoader
