# Configurable Backend URL Implementation Plan

> **Superseded in part (2026-10-01):** the final signatures and validation, warning and reset behaviour are in the "Addendum" and "Review follow-ups" sections of `docs/superpowers/specs/2026-08-07-loams-configurable-backend-url-design.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the LOAMS client resolve its backend base URL at runtime from a `config.ini` beside the exe (or a `WITS_API_BASE_URL` env override), falling back to `http://localhost/loams_api/`, so one build can serve both the all-in-one PC and a server + multi-kiosk deployment.

**Architecture:** `ApiConfig` (header-only, dependency-free) gains a settable base URL backed by an inline-function-local static; it keeps being the single source every endpoint already calls through `ApiConfig::endpoint(...)`. A new `core/apiconfigloader` unit owns all env/filesystem I/O as a *pure* `resolveBaseUrl(env, iniPath)` function plus a thin `applyFromRuntime(appDir)` wrapper that calls `ApiConfig::setBaseUrl(...)`. Both entry points (`main.cpp`, `quick/main.cpp`) invoke the wrapper once at startup.

**Tech Stack:** Qt 6.11.1 (MinGW 64-bit), C++17, CMake + Ninja, Qt Test + ctest. Spec: `docs/superpowers/specs/2026-08-07-loams-configurable-backend-url-design.md`.

## Global Constraints

- **Qt / language:** Qt 6.11.1 MinGW kit; C++17; header stays dependency-free (no `QSettings`/network/`QApplication` in `apiconfig.h`).
- **Core file naming:** `core/` C++ files are **lowercase** (`apiconfig.h`, `settingscontroller.cpp`) — new files are `apiconfigloader.h` / `apiconfigloader.cpp`. Namespace is `ApiConfigLoader`; functions `resolveBaseUrl`, `applyFromRuntime`.
- **Default base URL (verbatim):** `http://localhost/loams_api/` — defined in exactly one place (`apiconfig.h`).
- **config.ini key (verbatim):** group `[Server]`, key `BaseURL`; QSettings key path `Server/BaseURL`.
- **Env var (verbatim):** `WITS_API_BASE_URL`.
- **Precedence:** env (if non-empty) → ini `Server/BaseURL` (if present & non-empty) → empty string (caller keeps the localhost default).
- **Normalization:** `setBaseUrl` trims, ignores empty, collapses trailing slashes to exactly one.
- **Build environment (Qt tools are NOT on PATH).** Every PowerShell command block below begins with this line; shell state does not persist between calls, so include it each time:
  ```powershell
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  ```
- **Short build dir (verbatim):** `C:/b/loams-dbg` — avoids the Windows MAX_PATH overflow the deep QML-module object paths hit under the OneDrive repo path. Configure once (Task 1, Step 2).
- **Commit discipline:** each task's final commit is made via the project `commit` skill (Conventional Commits, why-focused body). The commands shown are the intended grouping/message.

---

### Task 1: Make `ApiConfig` settable (header) + extend its unit test

**Files:**
- Modify: `qt-app/core/apiconfig.h` (add `detail::mutableBaseUrl()`, change `baseUrl()`, add `setBaseUrl()`)
- Test: `qt-app/tests/tst_apiconfig.cpp` (add `cleanup()` reset + new cases)

**Interfaces:**
- Consumes: nothing (leaf).
- Produces: `QString ApiConfig::baseUrl()` (unchanged signature), `void ApiConfig::setBaseUrl(const QString&)`, `QUrl ApiConfig::endpoint(const QString&)` (unchanged). `setBaseUrl` normalizes to exactly one trailing slash and ignores empty/whitespace input.

- [ ] **Step 1: Write the failing tests**

Edit `qt-app/tests/tst_apiconfig.cpp`. Add these five private slots to the class declaration (after `endpointQueryCanBeAppended();`):

```cpp
    void cleanup();
    void setBaseUrlNormalizesMissingSlash();
    void setBaseUrlCollapsesMultipleSlashes();
    void setBaseUrlIgnoresEmpty();
    void endpointReflectsChangedBase();
```

Add the implementations (before the `QTEST_APPLESS_MAIN` line):

```cpp
void TestApiConfig::cleanup()
{
    // The base URL is a process-global mutable; reset after every case so
    // ordering can't leak state into the hardcoded-default assertions.
    ApiConfig::setBaseUrl(QStringLiteral("http://localhost/loams_api/"));
}

void TestApiConfig::setBaseUrlNormalizesMissingSlash()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://192.168.1.100/loams_api"));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://192.168.1.100/loams_api/"));
}

void TestApiConfig::setBaseUrlCollapsesMultipleSlashes()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://host/loams_api///"));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://host/loams_api/"));
}

void TestApiConfig::setBaseUrlIgnoresEmpty()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://host/loams_api/"));
    ApiConfig::setBaseUrl(QStringLiteral("   "));
    QCOMPARE(ApiConfig::baseUrl(), QString("http://host/loams_api/"));
}

void TestApiConfig::endpointReflectsChangedBase()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://192.168.1.100/loams_api"));
    QCOMPARE(ApiConfig::endpoint("student_login.php").toString(),
             QString("http://192.168.1.100/loams_api/student_login.php"));
}
```

- [ ] **Step 2: Configure the build (once) and run the test to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B C:/b/loams-dbg -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-dbg --target tst_apiconfig
```
Expected: **compile FAILS** with `'setBaseUrl' is not a member of 'ApiConfig'`.

- [ ] **Step 3: Implement the settable base URL**

Edit `qt-app/core/apiconfig.h`. Replace the current `baseUrl()` definition (the `inline QString baseUrl()` block) with:

```cpp
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
```

Leave `endpoint(const QString&)` exactly as-is (it already builds on `baseUrl()`).

- [ ] **Step 4: Build and run the test to verify it passes**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build C:/b/loams-dbg --target tst_apiconfig
ctest --test-dir C:/b/loams-dbg -R tst_apiconfig --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1` (all original + 4 new cases green).

- [ ] **Step 5: Commit (via the `commit` skill)**

Intended grouping/message:
```bash
git add qt-app/core/apiconfig.h qt-app/tests/tst_apiconfig.cpp
git commit -m "feat(core): make ApiConfig base URL settable at runtime

- Replace the hardcoded baseUrl() with a settable inline-static so a
  deployment can point the client at a remote backend; keep the header
  dependency-free so the APPLESS unit test still links
- Normalize to exactly one trailing slash and ignore empty input so a
  blank config value can't blank the base
- Reset the process-global base in test cleanup() to keep the default
  assertions order-independent"
```

---

### Task 2: `ApiConfigLoader` resolver + new `tst_apiconfigloader` + CMake wiring

**Files:**
- Create: `qt-app/core/apiconfigloader.h`
- Create: `qt-app/core/apiconfigloader.cpp`
- Create: `qt-app/tests/tst_apiconfigloader.cpp`
- Modify: `qt-app/core/CMakeLists.txt:20` (add the two files to the `witscore` source list)
- Modify: `qt-app/tests/CMakeLists.txt` (append a `wits_add_qttest` registration)

**Interfaces:**
- Consumes: `ApiConfig::setBaseUrl(const QString&)`, `ApiConfig::baseUrl()`, `ApiConfig::endpoint(const QString&)` (Task 1).
- Produces:
  - `QString ApiConfigLoader::resolveBaseUrl(const QString &envValue, const QString &iniFilePath)` — pure; precedence env → ini `Server/BaseURL` → `QString()`. No globals touched.
  - `void ApiConfigLoader::applyFromRuntime(const QString &appDirPath)` — reads `WITS_API_BASE_URL` + `appDirPath + "/config.ini"`, and on a non-empty result calls `ApiConfig::setBaseUrl(...)`.

- [ ] **Step 1: Write the failing loader test**

Create `qt-app/tests/tst_apiconfigloader.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "apiconfig.h"
#include "apiconfigloader.h"

class TestApiConfigLoader : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void envWinsOverIni();
    void iniUsedWhenEnvEmpty();
    void defaultWhenNeitherPresent();
    void malformedIniFallsThrough();
    void fullChainSetsEndpoint();

private:
    // Write an ini with the given BaseURL line and return its path.
    QString writeIni(QTemporaryDir &dir, const QString &baseUrlLine)
    {
        const QString path = dir.path() + QStringLiteral("/config.ini");
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream(&f) << "[Server]\n" << baseUrlLine << "\n";
        f.close();
        return path;
    }
};

void TestApiConfigLoader::cleanup()
{
    ApiConfig::setBaseUrl(QStringLiteral("http://localhost/loams_api/"));
}

void TestApiConfigLoader::envWinsOverIni()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://from-ini/loams_api"));
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QStringLiteral("http://from-env/loams_api"), ini),
             QString("http://from-env/loams_api"));
}

void TestApiConfigLoader::iniUsedWhenEnvEmpty()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://from-ini/loams_api"));
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QString(), ini),
             QString("http://from-ini/loams_api"));
}

void TestApiConfigLoader::defaultWhenNeitherPresent()
{
    // No env, and a path to a file that does not exist -> empty (caller keeps default).
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QString(),
                 QStringLiteral("C:/no/such/config.ini")),
             QString());
}

void TestApiConfigLoader::malformedIniFallsThrough()
{
    QTemporaryDir dir;
    // Right group, empty value -> treated as not configured.
    const QString ini = writeIni(dir, QStringLiteral("BaseURL="));
    QCOMPARE(ApiConfigLoader::resolveBaseUrl(QString(), ini), QString());
}

void TestApiConfigLoader::fullChainSetsEndpoint()
{
    QTemporaryDir dir;
    const QString ini = writeIni(dir, QStringLiteral("BaseURL=http://192.168.1.100/loams_api"));
    ApiConfig::setBaseUrl(ApiConfigLoader::resolveBaseUrl(QString(), ini));
    QCOMPARE(ApiConfig::endpoint("student_login.php").toString(),
             QString("http://192.168.1.100/loams_api/student_login.php"));
}

QTEST_MAIN(TestApiConfigLoader)
#include "tst_apiconfigloader.moc"
```

- [ ] **Step 2: Create the header + a stub implementation (compiles/links, tests red)**

Create `qt-app/core/apiconfigloader.h`:

```cpp
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
```

Create `qt-app/core/apiconfigloader.cpp` as a **stub** (real logic lands in Step 5). Returning `QString()` unconditionally makes the precedence tests fail at runtime — a true red, not a build error:

```cpp
#include "apiconfigloader.h"

namespace ApiConfigLoader {

QString resolveBaseUrl(const QString & /*envValue*/, const QString & /*iniFilePath*/)
{
    return QString(); // stub — replaced in Step 5
}

void applyFromRuntime(const QString & /*appDirPath*/)
{
    // stub — replaced in Step 5
}

} // namespace ApiConfigLoader
```

- [ ] **Step 3: Add the loader to `witscore` and register the test**

Edit `qt-app/core/CMakeLists.txt`. In the `add_library(witscore STATIC ...)` list (starts line 19), add the loader right after the `apiconfig.h` line so it reads:

```cmake
    apiconfig.h
    apiconfigloader.h apiconfigloader.cpp
```

Append to `qt-app/tests/CMakeLists.txt`:

```cmake
# --- backend base-URL resolver (pure core, no offscreen). Compiles the loader
# .cpp directly (same pattern as the other core tests). ---
wits_add_qttest(tst_apiconfigloader
    SOURCES
        tst_apiconfigloader.cpp
        ${CMAKE_SOURCE_DIR}/core/apiconfigloader.cpp
        ${CMAKE_SOURCE_DIR}/core/apiconfigloader.h
    INCLUDES ${CMAKE_SOURCE_DIR}/core)
```

- [ ] **Step 4: Configure, build, and run the test to verify it FAILS**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B C:/b/loams-dbg -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-dbg --target tst_apiconfigloader
ctest --test-dir C:/b/loams-dbg -R tst_apiconfigloader --output-on-failure
```
Expected: **build succeeds, test FAILS** — e.g. `envWinsOverIni` gets `""` but expects `"http://from-env/loams_api"` (the stub returns empty).

- [ ] **Step 5: Implement the resolver (replace the stub body)**

Replace the entire contents of `qt-app/core/apiconfigloader.cpp` with:

```cpp
#include "apiconfigloader.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

#include "apiconfig.h"

namespace ApiConfigLoader {

QString resolveBaseUrl(const QString &envValue, const QString &iniFilePath)
{
    // 1. Environment override (dev/CI).
    const QString env = envValue.trimmed();
    if (!env.isEmpty())
        return env;

    // 2. config.ini beside the exe.
    if (QFileInfo::exists(iniFilePath)) {
        QSettings ini(iniFilePath, QSettings::IniFormat);
        if (ini.status() == QSettings::NoError) {
            const QString fromIni =
                ini.value(QStringLiteral("Server/BaseURL")).toString().trimmed();
            if (!fromIni.isEmpty())
                return fromIni;
        }
    }

    // 3. Nothing configured — caller keeps ApiConfig's localhost default.
    return QString();
}

void applyFromRuntime(const QString &appDirPath)
{
    const QString resolved = resolveBaseUrl(
        qEnvironmentVariable("WITS_API_BASE_URL"),
        appDirPath + QStringLiteral("/config.ini"));
    if (!resolved.isEmpty())
        ApiConfig::setBaseUrl(resolved);
}

} // namespace ApiConfigLoader
```

- [ ] **Step 6: Build and run the test to verify it passes**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B C:/b/loams-dbg -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-dbg --target tst_apiconfigloader
ctest --test-dir C:/b/loams-dbg -R tst_apiconfigloader --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1` (all 5 cases green).

- [ ] **Step 7: Commit (via the `commit` skill)**

Intended grouping/message:
```bash
git add qt-app/core/apiconfigloader.h qt-app/core/apiconfigloader.cpp \
        qt-app/tests/tst_apiconfigloader.cpp qt-app/core/CMakeLists.txt \
        qt-app/tests/CMakeLists.txt
git commit -m "feat(core): add ApiConfigLoader backend-URL resolver

- Resolve the base URL with precedence env WITS_API_BASE_URL ->
  config.ini [Server] BaseURL -> empty (ApiConfig keeps localhost),
  isolating all QSettings/env I/O out of the dependency-free header
- Keep resolveBaseUrl pure (env + ini path in, URL out) so the full
  precedence matrix is unit-tested with a QTemporaryDir, no real env
- Add the unit to witscore so both apps can wire it at startup"
```

---

### Task 3: Wire startup in both entry points + centralization audit

**Files:**
- Modify: `qt-app/main.cpp` (legacy `WITS`)
- Modify: `qt-app/quick/main.cpp` (`WITSQuick`)

**Interfaces:**
- Consumes: `ApiConfigLoader::applyFromRuntime(const QString&)` (Task 2), `QCoreApplication::applicationDirPath()`.
- Produces: nothing (entry-point wiring).

- [ ] **Step 1: Audit that no endpoint bypasses `ApiConfig` (records the invariant)**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
Select-String -Path (Get-ChildItem qt-app -Recurse -Include *.cpp,*.h,*.qml |
    Where-Object FullName -notmatch 'libs\\QXlsx|\\build\\|\\tests\\|apiconfig') `
    -Pattern 'https?://|localhost|\.php' | Select-Object Path,LineNumber,Line
```
Expected: **no matches that construct a backend URL directly.** Every backend call goes through `ApiConfig::endpoint(...)`. If a stray hardcoded URL appears, route it through `ApiConfig::endpoint(...)` before proceeding, and note it in the commit body.

- [ ] **Step 2: Wire the legacy Widgets app**

Edit `qt-app/main.cpp`. Add the include after `#include <QApplication>` (line 3):

```cpp
#include <QApplication>
#include "apiconfigloader.h"
```

Insert the resolver call immediately after `QApplication a(argc, argv);` (line 9), before `a.setPalette(...)`:

```cpp
        QApplication a(argc, argv);
        // Resolve the backend base URL (config.ini beside the exe / env) before
        // any window can issue a request. No config -> localhost default.
        ApiConfigLoader::applyFromRuntime(QCoreApplication::applicationDirPath());
```

- [ ] **Step 3: Wire the Quick app**

Edit `qt-app/quick/main.cpp`. Add the include after `#include "brandtheme.h"` (line 9):

```cpp
#include "brandtheme.h"
#include "apiconfigloader.h"
```

Insert the resolver call immediately after `QGuiApplication app(argc, argv);` (line 13):

```cpp
    QGuiApplication app(argc, argv);

    // Resolve the backend base URL (config.ini beside the exe / env) before the
    // engine loads any screen that can issue a request. No config -> localhost.
    ApiConfigLoader::applyFromRuntime(QCoreApplication::applicationDirPath());
```

- [ ] **Step 4: Full build + full suite to verify no regression**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B C:/b/loams-dbg -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-dbg
ctest --test-dir C:/b/loams-dbg --output-on-failure
```
Expected: build links `WITS.exe` and `WITSQuick.exe` with no new warnings; **all tests pass** (the original suite + `tst_apiconfig` with its 4 new cases + `tst_apiconfigloader`).

- [ ] **Step 5: Manual smoke (recommended — GUI app, not automatable here)**

Confirm the wiring changes the live base URL:
```powershell
"[Server]`nBaseURL=http://127.0.0.1:9/loams_api" | Set-Content -Encoding ascii "C:/b/loams-dbg/config.ini"
```
Copy that `config.ini` next to the built `WITS.exe` (same folder), launch it, attempt a login, and confirm it tries to reach `127.0.0.1:9` (connection-refused / timeout to that host) rather than `localhost`. Delete the throwaway `config.ini` afterward. (Absent a `config.ini`, the app must still behave exactly as today — localhost.)

- [ ] **Step 6: Commit (via the `commit` skill)**

Intended grouping/message:
```bash
git add qt-app/main.cpp qt-app/quick/main.cpp
git commit -m "feat(app): resolve backend URL at startup in both entry points

- Call ApiConfigLoader::applyFromRuntime() right after the application
  object is built in WITS and WITSQuick, so config.ini/env is applied
  before any window or screen can issue a backend request
- Audited that every backend call still flows through ApiConfig::endpoint
  (no hardcoded URLs bypass the single configurable source)"
```

---

## Post-plan verification

After Task 3, run the project review gate before finishing the branch:
- `/claude-review` on the branch diff (per `.claude/rules/workflow.md` §3), fix Critical/Important findings, resubmit until APPROVE.
- Then `create-pr` (dispatches `dry-checker`, `security-reviewer`, `general-code-reviewer`) → open the PR targeting `master`.

## Follow-ups (out of scope — separate work)

1. `packaging/wits-client.iss` — a client-only installer that prompts for the server address and writes `config.ini` beside the exe (the deployment path this feature unlocks).
2. Optional: have the all-in-one installer write an explicit localhost `config.ini` (not required — localhost is the default).
