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

namespace detail {
// Single shared instance across all translation units (inline function-local
// static, guaranteed unique in C++17). This is the one place the default lives.
inline QString &mutableBaseUrl()
{
    static QString value = QStringLiteral("http://localhost/loams_api/");
    return value;
}
} // namespace detail

// The single source of truth for the backend base URL. Includes the trailing
// slash so it joins cleanly with a relative endpoint path.
inline QString baseUrl()
{
    return detail::mutableBaseUrl();
}

// Override the base URL at runtime (resolved from config.ini / env at startup;
// see core/apiconfigloader). Normalizes to exactly one trailing slash. Empty or
// whitespace-only input is ignored so a blank config line can't blank the base.
inline void setBaseUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return;
    QString v = trimmed;
    while (v.endsWith(QLatin1Char('/')))
        v.chop(1);
    detail::mutableBaseUrl() = v + QLatin1Char('/');
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
