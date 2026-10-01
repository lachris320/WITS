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

// Validates the EXPLICIT port in the raw input's authority ourselves, before
// trusting QUrl: Qt 6.11 StrictMode wraps an overflowing port (e.g.
// ":4294967376" == 2^32+80 parses as 80). Authority = text between "://" and
// the next '/', '?' or '#'; an IPv6 literal's port follows the closing ']'.
// No ':' delimiter -> no explicit port -> true. Otherwise the port must be
// 1-5 ASCII digits (no sign, space or percent-encoding; empty is rejected)
// with a value of 1..65535. Inputs without "://" are left to the URL checks.
inline bool hasValidExplicitPort(const QString &raw)
{
    const qsizetype schemeEnd = raw.indexOf(QLatin1String("://"));
    if (schemeEnd < 0)
        return true;
    const qsizetype start = schemeEnd + 3;
    qsizetype end = raw.size();
    for (qsizetype i = start; i < raw.size(); ++i) {
        const QChar c = raw.at(i);
        if (c == QLatin1Char('/') || c == QLatin1Char('?') || c == QLatin1Char('#')) {
            end = i;
            break;
        }
    }
    QStringView hostPort = QStringView(raw).mid(start, end - start);
    const qsizetype at = hostPort.lastIndexOf(QLatin1Char('@'));
    if (at >= 0)
        hostPort = hostPort.mid(at + 1);   // userinfo is rejected separately

    qsizetype colon = -1;
    if (hostPort.startsWith(QLatin1Char('['))) {
        const qsizetype close = hostPort.indexOf(QLatin1Char(']'));
        if (close < 0)
            return false;
        if (close + 1 == hostPort.size())
            return true;                     // "[v6]" with no port
        if (hostPort.at(close + 1) != QLatin1Char(':'))
            return false;
        colon = close + 1;
    } else {
        colon = hostPort.indexOf(QLatin1Char(':'));
        if (colon < 0)
            return true;
    }

    const QStringView port = hostPort.mid(colon + 1);
    if (port.isEmpty() || port.size() > 5)
        return false;
    int value = 0;
    for (const QChar c : port) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
        value = value * 10 + (c.unicode() - u'0');
    }
    return value >= 1 && value <= 65535;
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
    if (!detail::hasValidExplicitPort(trimmed))
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

    // Port 0 is never a usable server port. The scheme's default port is
    // stripped so "http://srv:80/" and "http://srv/" are the same origin
    // (LoginParser's same-origin photo check compares QUrl::port()).
    if (url.port() == 0)
        return QString();
    const int defaultPort = scheme == QLatin1String("https") ? 443 : 80;
    if (url.port() == defaultPort)
        url.setPort(-1);

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
