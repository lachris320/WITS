#ifndef APICONFIGLOADER_H
#define APICONFIGLOADER_H

#include <QString>

// Resolves the backend base URL from the environment / a config.ini and applies
// it to ApiConfig at startup. All environment + filesystem I/O lives here so
// apiconfig.h can stay header-only and dependency-free (and unit-testable).
namespace ApiConfigLoader {

// Pure precedence resolver — no globals, no real env/filesystem beyond the
// supplied ini path. Returns:
//   1. envValue, if non-empty                 (WITS_API_BASE_URL: dev/CI override)
//   2. [Server] BaseURL from iniFilePath,      (deployment config)
//      if the file exists, parses, and the value is non-empty
//   3. QString()                               (nothing configured -> caller keeps default)
QString resolveBaseUrl(const QString &envValue, const QString &iniFilePath);

// Reads the real WITS_API_BASE_URL and <appDirPath>/config.ini, then calls
// ApiConfig::setBaseUrl(...) when resolveBaseUrl returns a non-empty URL.
void applyFromRuntime(const QString &appDirPath);

} // namespace ApiConfigLoader

#endif // APICONFIGLOADER_H
