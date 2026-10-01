#ifndef APICONFIG_H
#define APICONFIG_H

#include <QString>
#include <QUrl>

// Centralized API base-URL configuration.
//
// The PHP backend is deployed under the "loams_api/" subfolder, so every
// backend request must be built relative to that base. Routing all endpoints
// through ApiConfig::endpoint() keeps the base path defined in exactly one
// place. Header-only and dependency-free (no QSettings/network) so it links
// into both the app and the unit-test target and stays trivially testable.
namespace ApiConfig {

// The compiled-in default, used when no valid runtime value is configured.
// This is the one place the default lives; tests also use it to restore the
// process-global after overriding it.
inline QString defaultBaseUrl()
{
    return QStringLiteral("http://localhost/loams_api/");
}

namespace detail {
// Single shared instance across all translation units (inline function-local
// static, guaranteed unique in C++17).
inline QString &mutableBaseUrl()
{
    static QString value = defaultBaseUrl();
    return value;
}
} // namespace detail

// The single source of truth for the backend base URL. Includes the trailing
// slash so it joins cleanly with a relative endpoint path.
inline QString baseUrl()
{
    return detail::mutableBaseUrl();
}

// Validate + normalize a candidate base URL. Every API request -- including the
// ones carrying the admin key -- is built on this base, so only an absolute
// http(s) URL with a host is accepted: no other scheme, no userinfo
// (credentials), no query, no fragment. Port and path are allowed. Returns the
// URL with a lowercase scheme and exactly one trailing slash, or an EMPTY
// string when the input is blank or invalid.
inline QString normalizedBaseUrl(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return QString();

    QUrl url(trimmed, QUrl::StrictMode);
    if (!url.isValid() || url.isRelative())
        return QString();

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return QString();
    if (url.host().isEmpty())
        return QString();
    if (url.hasQuery() || url.hasFragment())
        return QString();
    // Reject ANY userinfo, including an empty one: StrictMode accepts
    // "http://@host/" with userInfo() empty, so check the authority for the
    // '@' delimiter itself. An '@' in the path is not part of the authority.
    if (!url.userInfo().isEmpty()
        || url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@')))
        return QString();

    url.setScheme(scheme);
    QString normalized = url.toString(QUrl::FullyEncoded);
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    return normalized + QLatin1Char('/');
}

// Override the base URL at runtime (resolved from config.ini / env at startup;
// see core/apiconfigloader). Applies only a value that passes
// normalizedBaseUrl(); anything else (blank, malformed, wrong scheme,
// credentials, query, fragment) leaves the current base UNCHANGED and returns
// false, so bad configuration can never redirect API traffic.
inline bool setBaseUrl(const QString &url)
{
    const QString normalized = normalizedBaseUrl(url);
    if (normalized.isEmpty())
        return false;
    detail::mutableBaseUrl() = normalized;
    return true;
}

// Restore the built-in default (through the single writer above). Used when
// runtime resolution yields nothing valid, so a re-resolution never keeps a
// previously applied URL.
inline void resetBaseUrl()
{
    const bool applied = setBaseUrl(defaultBaseUrl());
    Q_ASSERT(applied);   // the compiled-in default always validates
    Q_UNUSED(applied);
}

// Build a full endpoint URL from a relative path (e.g. "get_departments.php"
// or "api.php/reports/data"). A single leading slash on the path is stripped
// so both "x.php" and "/x.php" resolve identically; multi-segment paths are
// preserved verbatim.
inline QUrl endpoint(const QString &path)
{
    QString relativePath = path;
    if (relativePath.startsWith(QLatin1Char('/'))) {
        relativePath.remove(0, 1);
    }
    return QUrl(baseUrl() + relativePath);
}

} // namespace ApiConfig

#endif // APICONFIG_H
