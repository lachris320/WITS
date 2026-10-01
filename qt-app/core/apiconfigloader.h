#ifndef APICONFIGLOADER_H
#define APICONFIGLOADER_H

#include <QList>
#include <QString>
#include <optional>

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

// Why a present source was rejected. Only an UNSET env var, a MISSING
// config.ini, or a readable config.ini WITHOUT the [Server] BaseURL key count as
// "not configured" (silent); everything below is reported.
enum class Reason {
    InvalidUrl,      // value present but fails ApiConfig::normalizedBaseUrl()
    Blank,           // env var set / ini key present, but empty or whitespace
    UnreadableFile,  // config.ini exists but QSettings could not read/parse it
};

// A present-but-invalid source that resolution skipped. `value` is for
// diagnostics only and has any embedded credentials (userinfo) stripped; it is
// empty for Blank and UnreadableFile.
struct Rejection {
    Source source = Source::Default;
    Reason reason = Reason::InvalidUrl;
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
//   3. nothing -> url empty, caller uses ApiConfig's localhost default
// envValue is std::nullopt when the variable is UNSET; an engaged empty or
// whitespace value means "set but blank" and is rejected. A present-but-
// INVALID source (see Reason) is recorded in `rejected` and resolution FALLS
// THROUGH to the next source; it never redirects traffic to a malformed URL.
Resolution resolveBaseUrl(const std::optional<QString> &envValue,
                          const QString &iniFilePath);

// Reads the real WITS_API_BASE_URL and <appDirPath>/config.ini, warns (qWarning)
// once per rejected source, and applies the resolved URL -- or RESETS to the
// built-in default when nothing valid resolves, so a re-run (legacy WITS's
// in-process restart) never keeps a stale URL. Call at startup, after the
// application object exists and BEFORE anything captures ApiConfig::baseUrl()
// (e.g. AccessControlHub::initialize()).
void applyFromRuntime(const QString &appDirPath);

} // namespace ApiConfigLoader

#endif // APICONFIGLOADER_H
