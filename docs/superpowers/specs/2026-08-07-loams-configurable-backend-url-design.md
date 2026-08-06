# Configurable Backend URL — Design Spec

**Date:** 2026-08-07
**Status:** Approved (brainstorming)
**Scope:** LOAMS client (both the legacy Widgets `WITS` app and the LOAMS 2.0 `WITSQuick` app)

## Problem

Every backend request in the client is built through `ApiConfig::endpoint(...)`
(`qt-app/core/apiconfig.h`), and `ApiConfig::baseUrl()` is **hardcoded** to
`http://localhost/loams_api/`. That is correct for the single-PC / all-in-one
deployment (backend on the same machine) and for development, but it makes a
**server + multiple kiosks** deployment impossible: a kiosk needs to reach
`http://<server-ip>/loams_api/`, and the compiled client can only ever talk to
`localhost`.

Because **admin login itself hits the backend** (`admin_login.php`), the URL
cannot live *only* in an in-app admin setting — if it were wrong or unset, the
admin screen (the place to fix it) would be unreachable. The backend URL must be
resolvable **before the first network call**, ideally written at install time.

## Goals

- Let a deployment point the client at any backend base URL without recompiling.
- Resolve the URL **before any backend request**, from a source set at/after
  install (a file beside the exe), with a developer/CI override and a safe dev
  default.
- Keep `ApiConfig` the single, centralized source every endpoint already uses.
- Preserve `ApiConfig`'s dependency-free, header-only testability.
- Apply identically to both the legacy `WITS` app and the `WITSQuick` app.

## Non-goals (explicit YAGNI)

- **No `ApiClient` network wrapper.** Callers keep constructing their own
  `QNetworkRequest`/`QNetworkAccessManager` as they do today. Centralizing the
  *network layer* is a separate, much larger refactor and is not required to make
  the URL configurable.
- **No in-app admin "Server URL" field.** The bootstrap problem above rules it
  out as a primary source; a convenience field can be a later, separate feature.
- **No installer changes in this spec.** The client-only installer that *writes*
  `config.ini` from a "server address" prompt is a follow-up (see below). This
  spec makes the client *able* to read that file.

## Design

### 1. `ApiConfig` becomes settable (still header-only, still dependency-free)

`qt-app/core/apiconfig.h` gains a settable base URL backed by an
inline-function-local static (C++17 guarantees a single shared instance across
translation units). The default is unchanged, so absent any configuration the
behavior is identical to today.

```cpp
namespace ApiConfig {

namespace detail {
    // Single shared instance across all TUs (inline function-local static).
    inline QString &mutableBaseUrl() {
        static QString value = QStringLiteral("http://localhost/loams_api/");
        return value;
    }
}

// Getter — unchanged signature, now returns the configured value.
inline QString baseUrl() { return detail::mutableBaseUrl(); }

// Setter — normalizes to a single trailing slash so it joins cleanly with a
// relative endpoint path. Empty/whitespace input is ignored (keeps the current
// value) so a blank config line can't blank out the base.
inline void setBaseUrl(const QString &url) {
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return;
    QString v = trimmed;
    while (v.endsWith(QLatin1Char('/')))
        v.chop(1);
    detail::mutableBaseUrl() = v + QLatin1Char('/');
}

} // namespace ApiConfig
```

`endpoint(const QString&)` is **unchanged** — it already builds on `baseUrl()`.

Properties preserved: no `QSettings`, no network, no `QApplication` — it still
links into the `QTEST_APPLESS_MAIN` unit test.

### 2. Resolver in `core/` — the only place that touches env/filesystem

New unit in `witscore`: `qt-app/core/apiconfigloader.h` / `.cpp`. It owns the
I/O and the precedence, kept out of the dependency-free header.

**Pure, testable core** — inputs in, URL out, no globals touched:

```cpp
namespace ApiConfigLoader {
// Returns the base URL to use, applying precedence. Does NOT read the real
// environment or filesystem — caller supplies both, so this is unit-testable
// with a synthetic env string and a temp ini path.
//   1. envValue (WITS_API_BASE_URL) if non-empty      -> dev/CI override
//   2. [Server] BaseURL in iniFilePath if present/valid -> deployment config
//   3. QString()  -> caller/ApiConfig keeps its localhost default
QString resolveBaseUrl(const QString &envValue, const QString &iniFilePath);
}
```

- `resolveBaseUrl` reads the ini with `QSettings(iniFilePath, QSettings::IniFormat)`
  and key `Server/BaseURL`. A missing file, missing key, `QSettings::status() !=
  NoError` (malformed), or empty value all fall through to the next tier.
- Returning an empty string when nothing is configured lets `ApiConfig` keep its
  own default — the default lives in exactly one place (the header).

**Thin runtime wrapper** — wires the real sources and applies the result:

```cpp
namespace ApiConfigLoader {
// Reads WITS_API_BASE_URL and <appDirPath>/config.ini, then calls
// ApiConfig::setBaseUrl(...) when a non-empty URL is resolved.
void applyFromRuntime(const QString &appDirPath);
}
```

`applyFromRuntime` uses `qEnvironmentVariable("WITS_API_BASE_URL")` and
`appDirPath + "/config.ini"`, calls `resolveBaseUrl(...)`, and if the result is
non-empty calls `ApiConfig::setBaseUrl(result)`. (`qEnvironmentVariable` is
QtCore-only — no widget/QSettings dependency leaks into ApiConfig.)

### 3. `config.ini` format

A plain INI beside the executable, written by the installer (follow-up) or by
hand:

```ini
[Server]
BaseURL=http://192.168.1.100/loams_api
```

- The value is the **full base URL** including `/loams_api`. Normalization in
  `setBaseUrl` handles the trailing slash, so both `.../loams_api` and
  `.../loams_api/` work.
- `https://…` is accepted verbatim (no scheme rewriting).

### 4. Startup wiring

Both entry points call the wrapper once, immediately after the application object
exists (needed for `applicationDirPath()`) and before any window/controller can
issue a request:

- `qt-app/main.cpp` (legacy `WITS`): after `QApplication app(...)`.
- `qt-app/quick/main.cpp` (`WITSQuick`): after the `QGuiApplication`/engine setup,
  before the root component loads.

```cpp
ApiConfigLoader::applyFromRuntime(QCoreApplication::applicationDirPath());
```

### 5. Centralization verification (proves non-goal #1 is safe)

As part of the work, audit that **no endpoint bypasses `ApiConfig`**: grep the
non-vendored client sources for `http://`, `https://`, `localhost`, and `.php`
outside `apiconfig.h`, its loader, and tests. Expected result: every backend URL
is built via `ApiConfig::endpoint(...)`. If a stray hardcoded URL exists, route
it through `ApiConfig` in this change. (Current grep shows all callers already
use `ApiConfig::endpoint`; this step guards against regressions and documents the
invariant.)

## Data flow

```
startup
  main.cpp / quick/main.cpp
    -> ApiConfigLoader::applyFromRuntime(appDir)
         env WITS_API_BASE_URL ─┐
         appDir/config.ini ─────┤ resolveBaseUrl(env, iniPath)
                                 └─> non-empty? -> ApiConfig::setBaseUrl(url)
                                     empty?     -> ApiConfig keeps localhost default
  ... later, any controller/window ...
    ApiConfig::endpoint("student_login.php")
      -> baseUrl() (now the configured value) + path
```

## Error handling

| Situation | Behavior |
|---|---|
| `WITS_API_BASE_URL` set | Used verbatim (normalized); wins over ini. |
| No env, `config.ini` present & valid | `[Server] BaseURL` used (normalized). |
| No env, no `config.ini` | Falls back to `http://localhost/loams_api/`. |
| `config.ini` present but malformed / key missing / value empty | Treated as "not configured" → falls through to default; app still starts. |
| Value has trailing slash / no trailing slash | Normalized to exactly one trailing slash. |

The app **never fails to start** because of configuration; a bad/absent config
degrades to the localhost default rather than crashing.

## Testing (TDD)

**`tst_apiconfig` (extend existing, still `QTEST_APPLESS_MAIN`):**
- Existing assertions stay green (default is still `http://localhost/loams_api/`).
- Add `cleanup()` that resets `ApiConfig::setBaseUrl("http://localhost/loams_api/")`
  so the mutable global can't leak between cases.
- `setBaseUrl` normalizes: no trailing slash, one trailing slash, multiple
  trailing slashes all yield exactly one.
- `setBaseUrl("")` / whitespace is ignored (previous value retained).
- `endpoint(...)` reflects a changed base after `setBaseUrl`.

**`tst_apiconfigloader` (new target via `wits_add_qttest`):**
- env non-empty → env wins even when ini present.
- env empty, ini valid → ini value returned.
- env empty, ini file absent → empty string (→ default).
- ini present but malformed / key missing / empty value → empty string.
- trailing-slash normalization is applied by the consumer (`setBaseUrl`), so the
  loader test asserts the raw resolved value and an integration-style case asserts
  `setBaseUrl(resolveBaseUrl(...))` then `endpoint(...)`.
- Uses `QTemporaryDir` + a written ini file; no real env mutation required
  (`resolveBaseUrl` takes the env value as a parameter).

Both run headless under ctest (`APPLESS`/offscreen as appropriate).

## Follow-ups (separate specs/tasks, not this change)

1. **Client-only installer** (`packaging/wits-client.iss`) that prompts for the
   server address and writes `config.ini` beside the exe — the deployment path
   for server + many kiosks. Depends on this feature.
2. Optional: all-in-one installer could write a `config.ini` with the localhost
   value for explicitness (not required — localhost is the default).

## Security considerations

- `config.ini` holds only a URL — **no secrets** (consistent with
  `security-hygiene`: the admin key and DB creds are never in the client).
- Prefer `https://` for networked deployments; the design accepts it as-is. Plain
  `http://` on a LAN kiosk network is the current backend posture (see
  `deliverables/docs/DEPLOYMENT_GUIDE.md`), unchanged by this work.
- No PII and no credentials are read, logged, or written by the loader.
