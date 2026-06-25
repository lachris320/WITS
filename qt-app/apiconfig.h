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

// The single source of truth for the backend base URL. Includes the trailing
// slash so it joins cleanly with a relative endpoint path.
inline QString baseUrl()
{
    return QStringLiteral("http://localhost/loams_api/");
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
