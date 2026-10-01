#ifndef APICONFIGLOADER_H
#define APICONFIGLOADER_H

#include <QList>
#include <QString>

// Resolves the backend base URL from the environment / a config.ini and applies
// it to ApiConfig at startup. All environment + filesystem I/O lives here so
// apiconfig.h can stay header-only and dependency-free (and unit-testable).
namespace ApiConfigLoader {

// Where a base-URL value came from.
enum class Source {
    Default,       // nothing usable configured: ApiConfig keeps its localhost default
    Environment,   // WITS_API_BASE_URL (dev/CI override)
    ConfigIni,     // [Server] BaseURL in <appDir>/config.ini (deployment config)
};

// A present-but-invalid source that resolution skipped. `value` is for
// diagnostics only and has any embedded credentials (userinfo) stripped.
struct Rejection {
    Source source = Source::Default;
    QString value;
};

struct Resolution {
    QString url;                  // validated + normalized; empty => keep default
    Source source = Source::Default;
    QList<Rejection> rejected;    // in precedence order
};

// Pure precedence resolver -- no globals, no real env/filesystem beyond the
// supplied ini path. Precedence:
//   1. envValue                                (WITS_API_BASE_URL: dev/CI override)
//   2. [Server] BaseURL from iniFilePath       (deployment config)
//   3. nothing -> url empty, caller keeps ApiConfig's localhost default
// A blank source counts as "not configured". A present-but-INVALID source (see
// ApiConfig::normalizedBaseUrl) is recorded in `rejected` and resolution FALLS
// THROUGH to the next source; it never redirects traffic to a malformed URL.
Resolution resolveBaseUrl(const QString &envValue, const QString &iniFilePath);

// Reads the real WITS_API_BASE_URL and <appDirPath>/config.ini, warns (qWarning)
// once per rejected source, and applies the resolved URL via
// ApiConfig::setBaseUrl when there is one. Call once at startup, after the
// application object exists and BEFORE anything captures ApiConfig::baseUrl()
// (e.g. AccessControlHub::initialize()).
void applyFromRuntime(const QString &appDirPath);

} // namespace ApiConfigLoader

#endif // APICONFIGLOADER_H
