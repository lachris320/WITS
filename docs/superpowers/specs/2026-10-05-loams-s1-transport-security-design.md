# LOAMS Security Uplift S1 — Transport Security (HTTPS) — Design Spec

**Date:** 2026-10-05
**Status:** Approved (brainstorming)
**Scope:** LOAMS 2.0 client (`WITSQuick`, `qt-app/`), PHP backend (`deliverables/loams_api/`) and its XAMPP/Apache host on the gate PC, PKI tooling and runbooks, release packaging and production cutover. The legacy Widgets `WITS.exe` is in scope only as a constrained compatibility exception.

## Background

The security uplift is the hard gate before Access Control go-live. It is decomposed into four sub-projects:

| Sub-project | Covers |
|---|---|
| **S1 — Transport / HTTPS** | This spec. |
| **S2 — Device identity** | Per-device tokens for kiosks and the gate; covers the open attendance endpoints. Device enrolment is interim-authorised by the existing admin key over HTTPS until S3 lands. |
| **S3 — Staff identity & sessions** | Replaces the shared admin key. |
| **S4 — Roles / RBAC & audit** | Authorisation model and audit trail. |

Current state:

- All client↔server traffic is plain HTTP.
- Admin operations use one shared admin key (bcrypt hash in the `admin` table), checked by `requireAdminAuth` (`deliverables/loams_api/auth_helper.php:39`).
- Attendance writers `student_login.php`, `rfid_login.php` and `guest_login.php` are open (unauthenticated).
- The Cloud+ turnstile controller posts plain HTTP to `turnstile.php`, protected only by an IP allowlist.
- The deployed legacy Widgets `WITS.exe` on the gate PC cannot be rebuilt; it uses `http://localhost` and the shared admin key.
- A PowerShell bridge (`deliverables/loams_api/bridge/loams-turnstile-bridge.ps1`) calls `turnstile_pull.php` over `http://127.0.0.1`.
- The server (XAMPP) runs on the gate PC.
- Each Qt controller/ViewModel creates its own `QNetworkAccessManager`; there is no shared client.
- Backend URL precedence today: `WITS_API_BASE_URL` env > `config.ini` `[Server] BaseURL` > `http://localhost/loams_api/` (`qt-app/core/apiconfigloader.cpp`; env read at `qt-app/core/apiconfigloader.cpp:165`).
- The deployment is LAN-only with no public domain, so a public ACME CA (Let's Encrypt) is not available.
- The XAMPP bundle found on the dev box is Apache 2.4.56, OpenSSL 1.1.1t (end-of-life), PHP 8.2.4.
- The Qt kit ships `qopensslbackend`, `qschannelbackend` and `qcertonlybackend`.
- AVG on the dev box intercepts TLS.

## Legacy compatibility rule

The owner's binding rule:

> "No security migration should break the deployed Legacy system, but backward compatibility must never become a permanent authentication bypass."

Every legacy accommodation MUST be:

- **Explicit** — a named path, never a silent fallback.
- **Narrow** — scoped to the legacy client's endpoints and host.
- **Observable** — logged.
- **Removable** — guarded by a server-side kill switch, with planned removal tied to legacy retirement.

No new client, kiosk or device MAY use a legacy path.

## 1. Purpose, scope, invariants

### Purpose

"Every LOAMS 2.0 client↔server exchange uses encrypted HTTPS with verified server identity. Client and user authentication are handled separately by S2 and S3. No LOAMS 2.0 release client may use insecure transport."

### Scope

| In scope | Out of scope |
|---|---|
| Client HTTPS enforcement | S2–S4 |
| Server TLS + transport guard | Revocation infrastructure |
| PKI tooling and runbooks | Reverse-proxy deployments |
| Server stack baseline | Widgets app migration |
| Packaging and cutover | |

### Invariants

This spec owns these invariants. Slice plans MUST NOT redefine them.

1. **No plaintext from 2.0 clients.** Release builds cannot send, follow or be redirected to `http://`, and cannot be reconfigured to do so.
2. **Exclusive, issuer-independent trust.** Clients trust exactly the configured CA(s), through an identical code path whether the issuer is the LOAMS private CA or an institutional CA. No system roots, no bypass, no downgrade, no "proceed anyway".
3. **Fail closed everywhere.** Missing/invalid config, wrong TLS backend, manifest mismatch or certificate failure → no request is sent, and an actionable error is shown.
4. **Legacy compatibility rule.** Exactly two exceptions exist: `LegacyLoopbackHttp` and `ControllerHttp`. No new client may use them; both are removed at legacy retirement.
5. **Administrator-controlled configuration only in production.** No user-level environment variable can affect the server address, trust, TLS library, plugins (including the platform plugin) or QML import paths; the packaged `qt.conf` is authoritative for plugin and import paths.
6. **Keys stay where born.** The CA key is offline and encrypted; the server key never leaves the server (CSR flow).
7. **Pinned, verified runtime.** Server and client TLS libraries match the compatibility manifest, verified from what is actually loaded.
8. **No production deployment before S1f.**

## 2. Architecture & topology

### Topologies

Single-PC is a special case of networked. The server runs on the gate PC.

**Mandatory migration rule:** the server MUST NOT move off the gate PC until LOAMS 2.0 has replaced legacy `WITS.exe` there. This is a stated precondition in the server-migration runbook.

| Topology | BaseURL | Server certificate SANs |
|---|---|---|
| Single PC | `https://localhost/loams_api/` | DNS `localhost`, IP `127.0.0.1` |
| Networked | `https://<server-name-or-IP>/loams_api/` | Server LAN DNS name and/or static IP; plus `localhost`/`127.0.0.1` only if a 2.0 client also runs on the gate PC (explicit `-IncludeLoopback`, never automatic) |

### Listeners

Only these listeners exist:

| Listener | Serves | Named exception |
|---|---|---|
| `:443` (loopback + LAN) | Entire API, HTTPS-only, for all 2.0 clients — except `turnstile_display.php`, loopback / server's own addresses only (below) | — |
| `127.0.0.1:80` and `[::1]:80` | Exact endpoint + method allowlist for legacy `WITS.exe` and the bridge | `LegacyLoopbackHttp` |
| `<server-LAN-IP>:80` | `turnstile.php` only, from `<controller-IP>` only; also enforced by Windows Firewall | `ControllerHttp` |

Port 80 from any other LAN source MUST be blocked by the firewall and refused by Apache. Forbidden HTTP is refused with 403 and MUST NOT be redirected to HTTPS.

**Gate-PC-only endpoint — `turnstile_display.php`.** Today it is loopback-only (`deliverables/loams_api/turnstile_display.php:30-32`) and is polled by `AccessControlHub`. It returns student identity data and S1 does not authenticate clients, so **it is NOT opened to the LAN in S1** (owner-level decision, recorded here):

- On `:443` it is served only to loopback / the server's own addresses; any other LAN source → 403. During the bridge era its existing loopback restriction also stays in force.
- The gate PC's 2.0 client therefore uses BaseURL `https://localhost/loams_api/`; the server certificate MUST include `localhost` via `-IncludeLoopback` on that deployment.
- Networked turnstile-display polling from other PCs is deferred to S2 device identity.

### Controller as untrusted input

The Cloud+ controller is treated as unauthenticated network input:

- Apache `Require ip <controller-IP>`.
- Windows Firewall rule: inbound TCP 80 only from `<controller-IP>`.
- VLAN / switch-port isolation is recommended and documented in the deployment guide; it is the institution's network, so LOAMS cannot enforce it.
- **Single source of the controller IP.** `turnstile.php` today compiles in an `ALLOWED_CONTROLLER_IP` constant and fails closed when it is empty (`deliverables/loams_api/turnstile.php:43-53`, `219-231`). S1d replaces the compiled constant with a value read from the admin-only server config (same ProgramData location as `compat.ini`, e.g. `[Controller] AllowedIp`). That one value is authoritative: the deployment tooling renders Apache's `Require ip` and the Windows Firewall rule from it, and `Test-LoamsDeployment.ps1` verifies all three agree. Empty or missing → fail closed, as today. `BENCH_MODE` semantics are unchanged unless the S1d plan decides otherwise, and it MUST remain false in production.

### Client: one security policy, not one physical manager

- `TransportPolicy` is immutable, shared as `std::shared_ptr<const TransportPolicy>`, and thread-safe.
- `HttpClient` owns the controller-facing manager.
- A `QQmlNetworkAccessManagerFactory` creates a **new** QML-owned manager on each `create()` call (which may be called from multiple threads), using the same `TransportPolicy`.
- Both manager kinds are `PolicyEnforcingNam` instances.
- The QML URL interceptor (`QQmlAbstractUrlInterceptor::intercept()` may be called from multiple threads) reads an immutable policy snapshot obtained by atomic load of the `shared_ptr` (C++17 `std::atomic_load` / `std::atomic_store` on `shared_ptr`); a trust-epoch change swaps the snapshot atomically.

### Server request path

```
Apache listener
  → php_admin_value auto_prepend_file = transport_guard.php
      → TLS?                                    → pass → endpoint
      → else named exception AND switch on?     → log "allowed"  → endpoint
      → else                                    → log "rejected" → 403
```

The decision uses only connection facts: `HTTPS` (from mod_ssl), `SERVER_ADDR`, `SERVER_PORT`, `REMOTE_ADDR`, the canonical decoded request path (script + `PATH_INFO`) and method. `X-Forwarded-*` headers are ignored. No reverse proxy is supported.

### Trust flow

1. The offline CA signs the server's CSR.
2. The server receives only its signed certificate and the public CA certificate.
3. The CA certificate is placed in each client's install directory and referenced by `config.ini` `CaCertificate`.
4. Its fingerprint is verified out-of-band against the CA tool's printout.

### Listener verification

The S1d deployment script verifies against the **live** Apache, not config files:

- HTTPS answers on the intended interfaces.
- HTTP answers only on loopback and the controller-facing interface.
- No wildcard HTTP listener exists.
- IPv4 and IPv6 behave identically.
- Apache and PHP each reject independently.

## 3. Client transport (S1a + S1e)

### S1a seam — behaviour-neutral Passthrough mode

- S1a introduces `HttpClient`, `PolicyEnforcingNam` and the QML factory in a **Passthrough** policy mode that preserves today's behaviour: the current `http` default is allowed and no TLS enforcement is applied. S1a can therefore merge before S1e without breaking anything.
- Passthrough exists only until S1e, which deletes it. S1f packaging (the only release boundary) requires S1e, so Passthrough can never reach production.
- **Injection boundary:** `HttpClient` accepts an injected `QNetworkAccessManager` / manager factory, so the existing `CapturingNam` / `SequencedNam` tests stay realistic (`qt-app/testsupport/capturingnam.h`, `qt-app/testsupport/sequencednam.h`).
- Core controllers already accept injected managers. ViewModels and hubs that construct managers internally today (e.g. `qt-app/quick/viewmodels/DatabaseViewModel.cpp:11-24`, `qt-app/quick/viewmodels/GuestViewModel.cpp:11`, `qt-app/quick/viewmodels/KioskViewModel.cpp:20`, `qt-app/quick/AccessControlHub.cpp:20-29`) MUST obtain them from the seam instead.

### PolicyEnforcingNam

A `QNetworkAccessManager` subclass overriding `createRequest()`. It is the single choke point for every request in the process; a rejected request returns an error reply without touching the network.

- **Remote URLs:** `https` with the exact BaseURL origin (scheme, host, port) only. Student photo and logo URLs MUST be same-origin (this closes the photo scheme/origin check deferred from PR #60).
- **Local URLs:** only `qrc:` is allowed at the NAM level; `file:`, `data:` and other schemes are rejected.
- **URL interceptor:** QML loads local files and `image://` without the NAM, so a `QQmlAbstractUrlInterceptor` additionally allows `qrc:`, the registered `image://` providers, the HTTPS origin and the two `file:` locations below, and rejects any other scheme and any other `file:` URL. A local-resource allowance is never permission for another network protocol. The interceptor reads its policy via an atomic snapshot (see Section 2).
- **Allowed `file:` locations:** only (a) the install directory and (b) the imported-asset directory. Imported logos/posters are copied into `QStandardPaths::AppDataLocation` and shown as `file:` URLs (`qt-app/core/settingscontroller.cpp:75-89`, `qt-app/quick/viewmodels/SchoolInfoUtil.cpp:7-11`, `qt-app/quick/qml/components/LLogoCircle.qml:25`). The allowance for (b) is narrow:
  - the app-data asset directory is canonicalised once at startup;
  - image extensions only;
  - each requested path is compared by canonical path after resolving symlinks, junctions and reparse points; if the canonical target leaves the directory, it is rejected;
  - no other `file:` location is allowed.
- **Resource inventory:** S1a inventories every resource actually loaded (logos, photos, fallback avatars, bundled assets); each becomes a test.
- **Per-request `QSslConfiguration`** from the policy:
  - CA list = exactly the configured CA(s) via `setCaCertificates` (replace, never append system roots);
  - `VerifyPeer`;
  - TLS 1.2 minimum;
  - hostname/SAN verification, valid chain, supported algorithms, valid dates.
- **Redirects:** all redirects are rejected in both controller and QML paths. `createRequest()` copies the request and forces `RedirectPolicyAttribute = ManualRedirectPolicy`, overriding any caller value. 301/302/303/307/308, or any 3xx with `Location`, yields `RedirectRejected`; 304 is a normal status. Tests MUST prove neither controllers nor QML can override this.
- **SSL errors** are never ignored. `ignoreSslErrors` MUST NOT appear anywhere in client source; a source grep check enforces this.
- **TLS session resumption** is disabled (`SslOptionDisableSessionTickets`, `SslOptionDisableSessionSharing`).

### Startup — TransportBootstrap

Three phases, in this order, so that nothing TLS-related loads before it is verified. The ordering MUST be verified on Qt 6.11.1 and on a clean install, including platform-plugin loading.

| Phase | When | Steps |
|---|---|---|
| **A — pre-Qt** | Before `QApplication` is constructed | 1. Neutralise the environment (all builds; list below). 2. `SetDefaultDllDirectories` restricted to the app directory + System32. 3. A packaged, trusted `qt.conf` fixes the plugin and QML import paths to the install directory, so the platform plugin path is controlled before `QApplication` exists. 4. Hash the packaged `libssl`, `libcrypto` and `qopensslbackend.dll` on disk against the client manifest. 5. Preload the verified OpenSSL DLLs by absolute path (`LoadLibraryExW`) so Qt's later by-name load resolves to the verified modules. |
| **B — Qt runtime** | After `QApplication`, before any TLS class/object is used and before any network object exists | 1. `QSslSocket::setActiveBackend("openssl")`; if unavailable, fail closed — never fall back to Schannel. 2. Post-load verification: the actually loaded module paths (`GetModuleFileName`) + SHA-256 for `libssl`, `libcrypto` and the TLS plugin match the manifest. |
| **C — transport** | After A and B pass | Read `config.ini` `[Server] BaseURL` and `CaCertificate` (+ rotation keys), build the `TransportPolicy`, and enable networking. |

- Any failure in any phase → the app starts in a **transport setup error** state: no network objects are created, and a setup screen explains the problem.
- An admin-only-writable install directory closes the hash-to-load gap between the Phase A hash and the actual load.
- **Current code order:** both entry points construct `QApplication` first (`qt-app/quick/main.cpp:15`, `qt-app/main.cpp:10`), and `ApiConfigLoader::applyFromRuntime` runs after it. S1e restructures `qt-app/quick/main.cpp` to the order above.

**Environment neutralised in Phase A (all builds):** `OPENSSL_CONF`, `OPENSSL_MODULES`, `OPENSSL_ENGINES`, `SSL_CERT_FILE`, `SSL_CERT_DIR`, `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `QML_IMPORT_PATH`, `QML2_IMPORT_PATH`.

- The packaged `qt.conf` is authoritative: the environment MUST NOT be able to redirect plugin or QML import paths.
- `QT_QUICK_BACKEND=software` stays allowed: it only selects the built-in software renderer (see `qt-app/quick/main.cpp`).

### Configuration

- In release builds, `BaseURL` and `CaCertificate` come from `config.ini` only. The CA path is never read from the environment.
- **Release builds fail closed on invalid config** (invariant 3). Today a present-but-invalid `BaseURL` falls back to the default (`qt-app/core/apiconfigloader.cpp:143-181`); in release this changes:

| Key | Absent | Present but invalid |
|---|---|---|
| `BaseURL` | Built-in default `https://localhost/loams_api/` | Setup error, no fallback (bad URL, `http` scheme, query/fragment, etc.) |
| `CaCertificate` | Setup error (no CA → no requests) | Setup error (unreadable or invalid certificate) |

- Dev builds MAY keep today's warn-and-fall-through behaviour.
- Rotation keys:

| Key | Meaning |
|---|---|
| `CaCertificate` | Active / new CA |
| `CaRetiring` | Old CA during rotation |
| `CaRotationUntil` | Instant at which `CaRetiring` stops being trusted |

- `CaRotationUntil` grammar: ISO 8601 UTC with mandatory `Z`, exactly `YYYY-MM-DDTHH:MM:SSZ`. Anything else (offsets, missing `Z`, date-only, fractional seconds) is a setup error.
- More than one CA requires `CaRotationUntil`; if it is missing, that is a setup error.
- Each configured CA MUST be `CA:TRUE`, have `keyCertSign`, and be currently valid.
- Diagnostics list every trusted CA fingerprint.

### CA retirement (automatic)

- During rotation both CAs are trusted. The retiring CA is automatically untrusted at and after `CaRotationUntil` (`now >= deadline`).
- Certificates from the retired CA → `CertUntrusted`, with an admin message naming the retired fingerprint, plus an urgent admin banner until the `CaRetiring` lines are removed.
- The emergency runbook removes `CaRetiring` immediately, with no grace period.
- **Trust epoch:** the policy carries an epoch, advanced at the deadline.
  - `HttpClient` clears its connection cache and aborts in-flight replies started under the old epoch (`TransportError::TrustRetired`).
  - Each QML `PolicyEnforcingNam` tracks its replies and subscribes to the `TrustEpoch` signal via a queued connection, so the slot runs on its owning thread: abort old-epoch replies, `clearConnectionCache`, apply the new policy.
- **Deadline enforcement:**
  - Primary: a precise single-shot deadline timer, re-armed in chunks because `QTimer` intervals are limited to ~24.8 days.
  - Also checked on every request and on Windows `WM_TIMECHANGE` and resume-from-sleep power events (native event filter).
  - A 60 s timer runs only as a recovery check, never as the primary mechanism.
- Tests (injected clock): a keep-alive connection across the deadline; an unfinished QML image download across the deadline; an in-flight reply aborted at the exact deadline (within timer tolerance); malformed `CaRotationUntil` values → setup error.

### Errors

Typed `TransportError`:

`NotConfigured`, `BackendUnavailable`, `ManifestMismatch`, `InsecureScheme`, `CrossOrigin`, `RedirectRejected`, `CertUntrusted`, `HostnameMismatch`, `CertExpired`, `CertNotYetValid`, `TlsHandshakeFailed`, `TrustRetired`, `Network`, `Timeout`, `Http(status)`.

| Audience | Presentation |
|---|---|
| Kiosk screens | Generic calm message + short code, e.g. "Secure connection to the LOAMS server failed — contact the librarian. Code: TLS-HOST". Never certificate details. |
| Admin screens | Full reason, server + CA SHA-256 fingerprints, what to check. |

### Expiry monitoring

After the handshake, the client reads the peer certificate's expiry and the configured CA's expiry and raises admin banners (never kiosk banners):

| Item | Warning | Urgent |
|---|---|---|
| Server certificate | 60 d, 30 d | 14 d |
| CA certificate | 6 months: "plan replacement" | — |

### Dev-only overrides

Compile definition `LOAMS_DEV_TRANSPORT_OVERRIDES` (never set in release; release packaging verifies it is off) enables:

- the `WITS_API_BASE_URL` environment override;
- plain-HTTP origins.

Both are compiled out of release. The pure resolver in `ApiConfigLoader` keeps taking the env value as a parameter; only the environment read (`qt-app/core/apiconfigloader.cpp:165`) is gated.

### TLS backend

- The OpenSSL backend with pinned OpenSSL 3.x DLLs and `qopensslbackend` ships with LOAMS.
- `qschannelbackend` (and `qcertonlybackend`) are excluded from the release package as defence in depth, while backend selection is still enforced at runtime.
- Exclusive trust MUST be **tested, not assumed**, including on the actual Windows 11 deployment environment.
- Candidate verified DLL source: the Qt Maintenance Tool OpenSSL 3 toolkit. The manifest records the chosen source.

### Legacy Widgets source

- The target sits behind CMake option `LOAMS_BUILD_LEGACY_WIDGETS=OFF` by default; release packaging explicitly rejects the legacy target.
- The source is marked deprecated / reference-only: no new features, security migrations or routine maintenance. It is never deployable or security-supported.
- Shared `core/` changes are not constrained by keeping it buildable; resulting incompatibilities are documented.
- The currently deployed binary and its required configuration are preserved securely, with documented rollback procedures and restrictions.
- Retirement criteria: the Widgets source is deleted and the server HTTP compatibility paths removed only after LOAMS 2.0 passes field validation (kiosk attendance, admin operations, turnstile integration).
- Freezing the source does not freeze the deployed system's security: the existing `WITS.exe` is a temporary security exception whose restricted HTTP access, monitoring and removal plan stay actively maintained until retirement.

## 4. Server side (S1b + S1d)

### Compatibility manifest

`deploy/stack/loams-stack-manifest.json`, versioned:

- **Server section** = "LOAMS Server Stack v1.0": an exact Apache 2.4.x built on OpenSSL 3.x, a supported PHP 8.x, and required extensions.
- **Client section** = the client runtime (OpenSSL DLLs, `qopensslbackend`).
- Each component records: version, source, archive SHA-256, expected loaded module names + SHA-256.
- Schema-checked in the gate. Single source of truth for `Test-LoamsServerHost.ps1` now and Installer 2.0 later.

**Pinning ≠ freezing.** New bundle versions go through security-patch review and a repeatable approval process: staging validation of the full integration list, checksums, a manifest bump (v1.1, …) and a tested rollback.

Stack requirements:

- Production MUST NOT be upgraded directly without staging validation.
- Back up Apache config, PHP config, API files, databases, certificates and compatibility settings.
- Validate auth, attendance endpoints, RFID login, turnstile events, reporting, file uploads, and the deployed legacy app's actual behaviour.
- Pinned versions with verified authenticity/checksums.
- No arbitrary mixing of Apache/PHP/OpenSSL binaries without compatibility testing.
- A failed upgrade MUST NOT leave attendance unavailable or the database partially migrated.

### Test-LoamsServerHost.ps1

- Modes: `-Report` (default, read-only) and `-Converge`.
- Runtime verification enumerates the DLLs actually loaded by the running `httpd` process (file versions + hashes) against the manifest, including PHP's linked OpenSSL — never just `openssl.exe version`. No new diagnostic HTTP endpoint is added.
- Deployment situations it handles:

| Situation | Behaviour |
|---|---|
| Fresh PC that we install | Converge |
| Existing XAMPP launched from Control Panel | Converge: back up, register the service (`httpd -k install -n Apache2.4`), switch account, apply ACLs, verify; documented rollback |
| Existing service under another name or LocalSystem | Converge: back up service config + ACLs, re-point to the virtual account, verify; documented rollback |
| PHP as FastCGI instead of Apache module | Set `auto_prepend_file` in `php.ini` and verify it is active |
| Missing mod_ssl, unsupported version, non-XAMPP layout | Stop and report |

- The first run on any gate PC is report-only.
- Convergence is **recoverable, not atomic**:
  1. complete preflight before any change;
  2. verified backups + restore prerequisites to a timestamped admin-only folder, with a generated restore script;
  3. staged changes with named checkpoints;
  4. post-change validation;
  5. automatic rollback where technically safe;
  6. if rollback fails, an explicit **RECOVERY REQUIRED** state naming the failed checkpoint and manual restore steps. Success is never reported after partial restoration.

### Service identity & least privilege

Apache runs as `NT SERVICE\Apache2.4` (virtual account), after verifying the actual Windows service name and XAMPP compatibility.

| Resource | Apache/PHP access |
|---|---|
| API code, Apache/PHP config, compat allowlist | read/execute |
| `C:\ProgramData\LOAMS\server\compat.ini` and the controller-IP config in the same folder | read |
| Server cert, CA cert, server key | read (key: Apache + Administrators only) |
| Compatibility log | append-only (`FILE_APPEND_DATA`) |
| Apache's own logs; PHP temp/upload/session dirs | modify (exact list from the S1b inventory) |
| Certificate scripts, backups, everything else | none |

- Append-only MUST be tested with PHP's real file-open behaviour; if it cannot hold, the fallback is a separate privileged channel (e.g. Windows Event Log), never broad write access.
- Apache/PHP MUST NOT be able to modify compatibility switches, application code, or certificate provisioning scripts.
- Safe migration: back up service config and ACLs, test, and document rollback before switching accounts.
- Register the `LOAMS-Transport` event source during privileged setup and verify the Apache account can write to it.

**Recorded caution:** the Apache identity cannot fully protect the TLS private key from compromised PHP — PHP runs in Apache's process and there is no practical separate identity on Windows. This reduces damage; it does not eliminate it.

### Apache layer — default-deny before PHP

Static files never execute the PHP guard, so Apache denies first:

- Each HTTP vhost starts with `<Location "/"> Require all denied`, then opens only exact `<LocationMatch>` paths, with methods enforced via `Require method` (not `<Limit>`).
- **Authorization composition.** Sibling `Require` directives are implicitly OR-ed (`RequireAny`), so each exception `<LocationMatch>` MUST use `AuthMerging Off` and a single `<RequireAll>` containing both `Require method ...` **and** the source restriction (`Require local` / loopback, or `Require ip <controller-IP>`). A method-only or source-only grant is a defect.
- `Options -Indexes`, `AllowEncodedSlashes Off`, `TraceEnable Off`; no `Alias`; no rewrite rules.
- **Path-info routes.** The client calls `POST api.php/reports/data` (`qt-app/core/reportcontroller.cpp:428`), routed by `deliverables/loams_api/api.php`, which parses the path from `REQUEST_URI` (~lines 19-32). Therefore there is no blanket `AcceptPathInfo Off`:
  - path-info is accepted **only** on `api.php`, and only for exact allowlisted routes (exact route + method);
  - every other script has `AcceptPathInfo Off`;
  - HTTPS vhost: all approved `api.php` routes used by 2.0;
  - HTTP loopback vhost: only routes evidenced as used by the deployed `WITS.exe` (e.g. `reports/data` if the access log shows it).
- **The one static exception:** legacy `WITS.exe` fetches student photos as static files — `rfid_login.php` builds `photo_url` as `<base>` + the stored relative photo path, falling back to `<base>uploads/default.jpg` (`deliverables/loams_api/rfid_login.php:139-145`). Allowed: GET/HEAD only, under `/loams_api/uploads/`, image extensions only, loopback listener only. Exact depth and extensions come from access-log evidence.
- **Endpoint allowlist:** an exact list of endpoints + HTTP methods, no wildcard routes, built from evidence — the legacy Widgets source, the bridge script, and the gate PC's real Apache access log (the deployed binary may differ from source) — before it is locked.
- **HTTPS vhost is default-deny too:** "entire API" means the approved endpoint scripts (and approved `api.php` routes) and `uploads/` images only. Non-endpoint content under `loams_api/` (`bridge/`, `tests/`, `sql/`, `*.ps1`, `*.sql`, `*.log`, backups) MUST be refused over HTTPS as well, and the S1f server bundle MUST NOT deploy `tests/` or dev SQL into the web root. `turnstile_display.php` is restricted to loopback / the server's own addresses (Section 2).
- **Bypass tests:** URL encoding, double encoding, case variants, `PATH_INFO` on non-`api.php` scripts, trailing dots/slashes, alternate methods, `.php` under `uploads/`, backup/config files; `api.php` route variants (encoded, trailing slash, case, extra segments, unlisted routes); wrong method from an allowed source; allowed method from a wrong source.

### PHP layer — transport_guard.php

- Installed via `php_admin_value auto_prepend_file` (cannot be overridden by `.htaccess` or `ini_set`). A test enumerates every `*.php` and asserts HTTP rejection.
- TLS → pass.
- **Request path:** the decision model uses the canonical, decoded request path (script + `PATH_INFO`), not just the script path. Encoded slashes and dot-segments are rejected before matching.
- Otherwise the request MUST match a named exception in `compat_allowlist.php` (code, versioned in the repo, read-only to Apache) **and** that exception's switch MUST be on in `compat.ini`. Each allowlist entry is exact: script, path-info route (or none), methods, listener (`SERVER_ADDR`/`SERVER_PORT`).
- Everything else → 403, no redirect, logged.
- **TLS loopback requests:** endpoints with a locality restriction (`turnstile_display.php`) MUST treat a loopback request over `:443` (`HTTPS` on, `REMOTE_ADDR` loopback) correctly — allowed — while refusing every non-local source.
- **Controller IP:** `turnstile.php` reads its allowed controller IP from the admin-only server config (Section 2), not from a compiled constant; empty/missing → fail closed.

### Kill switches

- `LegacyLoopbackHttp` and `ControllerHttp` live in `compat.ini`, outside the web root.
- Default OFF in new deployments. The upgrade runbook for the existing gate PC explicitly enables both as a named step.
- Read and validated on **every** compatibility request (no mtime caching).
- `Set-LoamsCompatSwitch.ps1` writes a temp file and atomically replaces `compat.ini`.
- Missing, unreadable, incomplete or malformed → both off.
- Only administrators can modify the file. No Apache restart is needed.
- Tests prove that turning a switch off affects the next request, including under concurrent load.

### Logging

Two coordinated layers with consistent field names and UTC timestamps:

| Layer | Records |
|---|---|
| Apache dedicated `CustomLog` | HTTP-vhost authorization denials |
| PHP guard JSON-lines log | Every allowed and rejected non-TLS request the guard evaluates |

- Fields: timestamp, source IP, listener, script path, path-info route, method, exception name, outcome.
- MUST NOT log query string, body, headers, admin key, card numbers/RFID credentials, or other sensitive payloads.
- Aggregation never assumes a rejection reached PHP.
- If the guard log write fails → Windows Event Log error (`LOAMS-Transport`). If **both** channels fail → compatibility requests fail closed (403); TLS requests are unaffected.
- Rotation + integrity: a privileged scheduled task rotates logs and writes the SHA-256 of each closed file to an admin-only manifest.

## 5. PKI tooling & runbooks (S1c)

### Requirements

- CA creation and signing happen **only** on the offline machine.
- The server receives only its signed certificate and the public CA certificate.
- The server key and CSR are generated on the server and never transferred.
- The CA passphrase is entered interactively (secure entry only) and piped to OpenSSL's stdin. It is never a command argument and never stored in scripts, config or logs.
- All scripts are PowerShell and use the manifest-pinned OpenSSL.

### Scripts

| Script | Runs on | Behaviour |
|---|---|---|
| `New-LoamsCa.ps1` | Offline CA | 5-year root; encrypted key; `CA:TRUE, pathlen:0`; `keyCertSign` + `cRLSign`; refuses overwrite; prints SHA-256 fingerprint for out-of-band distribution. |
| `New-LoamsServerKey.ps1` | Server | ECDSA P-256 key (RSA-3072 if the manifest says so) + CSR with validated SANs (`-DnsName`, `-IpAddress`, optional `-IncludeLoopback`); key ACL Apache + Administrators; refuses overwrite; fresh key every renewal; versioned filenames (e.g. `loams-server-2026-10.key`). |
| `Approve-LoamsCsr.ps1` | Offline CA | Uses `openssl ca` with `copy_extensions = none` and a script-built extension section. Signs only SANs present in the deployment's approved server identity inventory (maintained on the offline CA machine, **not** in the repo). Operator typed confirmation. Fixed profile: `CA:FALSE`, `digitalSignature` (+ `keyEncipherment` for RSA), EKU `serverAuth`, ~397 days, random serial. Refuses if certificate expiry would exceed CA expiry; refuses overwrite; writes a local issuance ledger. |
| `Test-LoamsServerCert.ps1` | Both | Chain to the given CA, `-purpose sslserver`, `-verify_hostname` / `-verify_ip`, validity window, key usage / EKU, certificate↔key pairing (server side); clear pass/fail. |
| `Install-LoamsServerCert.ps1` | Server | Stage → Validate → Activate (`httpd -t`, graceful restart) → Live HTTPS probe (a real TLS connection to each configured hostname/IP, verified against the configured CA only — an open port is not enough) → Commit or Rollback; RECOVERY REQUIRED if rollback fails. Also provides the institutional-CA import path: any issuer's certificate + full chain, with full-chain and key-pairing validation, without the LOAMS CA scripts and without weakening client exclusive trust. |

### SAN validation

- DNS names: RFC 1123, no wildcards, no IP literals in DNS entries.
- IPs MUST parse as IPv4/IPv6.
- Loopback only with `-IncludeLoopback`, and shown in the confirmation.

### CA loss protection

- At least two encrypted offline CA-key backups on separate media/locations, never on the server or clients.
- Separately protected copies of the issuance ledger.
- Documented passphrase custody and recovery by named roles.
- Tested offline CA restoration.

### Lifetime & revocation

- Server certificates: ~13 months (≈397 d), renewed annually without waiting for expiry. CA: 5 years.
- Qt does not check revocation for the custom trust anchor, so the compromise response is CA rotation.
- Warnings at 60/30 d, urgent at 14 d. CA replacement planning starts at least 6 months before CA expiry.
- Suspected server-key compromise → immediate key replacement, CA rotation, client trust-anchor replacement, and removal of the old CA from every client.
- New CA fingerprints are verified through a trusted administrative channel, never via the possibly-compromised server alone.
- Expired, untrusted or invalid certificates never trigger HTTP fallback or a bypass option.
- **Institutional CA:** revocation is NOT assumed (only if a spike proves the actual Qt TLS path checks it). The same emergency trust-anchor replacement applies; because LOAMS cannot rotate the university CA, the emergency path switches clients to an emergency LOAMS CA.
- Emergency CA replacement is a coordinated deployment: an inventory of all client PCs; a managed risk window while clients still trust the old CA (staged cutover, old CA removed last); a possible maintenance outage; tested before production.

### Runbooks (`docs/security/runbooks/`)

| # | Runbook |
|---|---|
| 1 | Initial PKI setup |
| 2 | Annual renewal (60/30/14) |
| 3 | CA replacement (≥ 6 months before expiry) |
| 4 | Emergency compromise (new CA, no grace, client inventory, staged cutover, old CA removed, verified) |
| 5 | Server migration (incl. the mandatory legacy-retirement precondition) |
| 6 | Client recovery (broken trust anchor / config) |
| 7 | Institutional-CA import + emergency switch to a LOAMS CA |
| 8 | Kill-switch operation + legacy retirement |
| 9 | Offline CA restoration |

Runbooks 2, 4, 6 and 9 MUST have a recorded staging dry run before go-live. Together the runbooks cover the owner's original requirement: CA replacement, certificate expiry, server migration and client recovery.

### Release-signing key

An offline Ed25519 key (see S1f), separate from the CA key, under the same backup and custody rules.

## 6. Testing & acceptance

### Gate

There is no hosted CI yet. "CI" means the pre-PR gate script `Invoke-LoamsSecurityGate.ps1`, which runs the same commands a future CI job would. It fails:

- on any failed test;
- on any test reported Not Run (including dependents skipped because a CTest fixture setup failed);
- if the security-labelled test count is below its committed expected minimum.

Security tests use `QFAIL`, never `QSKIP`, when prerequisites are missing; missing OpenSSL, certificate-generation failure or unsupported config are explicit failures.

### Test PKI

- Generated at test time by a CTest `FIXTURES_SETUP` / `FIXTURES_REQUIRED` / `FIXTURES_CLEANUP` fixture (`Generate-TestPki`) using the manifest-pinned OpenSSL, into a unique per-run build-directory folder; never written to the source tree.
- Real OpenSSL certificates + `QSslServer` (listening on port 0); validation is not mocked.
- Expired / not-yet-valid certificates via `openssl ca -startdate/-enddate`, never by changing the Windows clock. Parallel runs are isolated.
- "Windows-trusted but unconfigured CA" in CI = a second generated CA injected into `QSslConfiguration::defaultConfiguration()` (where system roots would appear). The client MUST still reject it, and its effective CA list MUST equal exactly the configured CA. The real Windows-store / AVG case is Layer 8.
- Environment/DLL neutralisation runs in every build so CI exercises it.

### Layers

| # | Layer | Covers |
|---|---|---|
| 1 | Qt unit (CTest) | Policy decisions (scheme, origin, redirect, `qrc:` / `image://` / `file:`, URL interceptor incl. the app-data asset allowance: image extensions only, `..` traversal and symlink/junction/reparse-point escapes rejected); config parsing incl. rotation keys, `CaRotationUntil` grammar and release fail-closed on present-but-invalid keys; manifest parsing / hash checks against generated dummy files; `TransportError` mapping; expiry thresholds + trust epoch with injected clock. |
| 2 | Qt TLS integration (CTest + PKI fixture + `QSslServer`) | Valid, expired, not-yet-valid, wrong host, wrong IP, unknown CA, injected "system" CA, broken chain, client-auth-only EKU, `CA:TRUE` leaf, invalid key usage; 30x→`http` and 30x→same-origin `https` both rejected; caller cannot override redirect policy; 304 passes; keep-alive and in-flight across the rotation deadline; in-flight reply aborted at the exact deadline; no session resumption; QML `Image` via factory managers (allowed origin loads; cross-origin, `http:`, stray `file:` rejected; unfinished image download across the deadline); imported logo and poster loading from the app-data asset directory, plus traversal/junction escapes rejected; no `ignoreSslErrors` (grep). |
| 3 | Bootstrap (child process, CTest) | Hostile values for each neutralised variable — `OPENSSL_CONF`, `OPENSSL_MODULES`, `OPENSSL_ENGINES`, `SSL_CERT_FILE`, `SSL_CERT_DIR`, `QT_PLUGIN_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `QML_IMPORT_PATH`, `QML2_IMPORT_PATH` — have no effect, including a hostile QML import path and a hostile platform plugin path; packaged `qt.conf` wins; decoy `libssl` in CWD and on `PATH` not loaded; tampered DLL/plugin hash → setup error before load; missing OpenSSL backend → setup error, no Schannel fallback; clean Qt 6.11.1 deployment works incl. platform-plugin loading. |
| 4 | Release configuration | Second build with `LOAMS_DEV_TRANSPORT_OVERRIDES=OFF` reruns 1–3. Behavioural: setting `WITS_API_BASE_URL` has no effect and `http://` origins are refused. The environment-read function itself is not compiled, verified by a compile-time test / build check — not by searching the binary for literal strings (diagnostic strings may legitimately remain). Passthrough mode is absent. |
| 5 | PHP guard (PHP CLI + `php -S`, logic only) | Pure decision function vs `$_SERVER` cases; every `*.php` rejects HTTP with `auto_prepend_file` active; malformed / missing / partial `compat.ini` → all off; log-write failure → Event Log alert; both-channel failure → fail closed. |
| 6 | PowerShell tooling (Pester 5) | SAN validation; refuse overwrite; hostile CSR requesting `CA:TRUE` still gets `CA:FALSE`; inventory enforcement; lifetime ≤ CA; passphrase never appears in child-process command lines (captured via process-creation records), PowerShell transcripts, `-Verbose`/`-Debug` output, exception messages or logs; manifest schema. |
| 7 | Staging deployment (`Test-LoamsDeployment.ps1` against a **live** staging Apache) | Listeners IPv4/IPv6; no wildcard HTTP; default-deny bypass corpus (incl. `api.php` route variants and method/source composition); `turnstile_display.php` refused from non-local sources over HTTPS and allowed over loopback TLS; controller IP agrees across server config, Apache `Require ip` and the firewall rule; each layer rejects alone (the other disabled in staging); `auto_prepend_file` actually active (guard marker observed from every endpoint); every PHP endpoint executes the guard; static photo exception serves images only (other uploads and `.php` under `uploads/` refused); guard effective under the actual PHP execution mode; kill switch effective on the next request under load; both log layers populated; append-only ACL holds; event source writable by the Apache account; loaded DLLs match the manifest; service-account effective permissions. |
| 8 | Deployment acceptance (S1f; scripted manual checklist on a representative real Windows 11 machine) | AVG interception rejected; a CA present in the Windows root store but not configured is rejected; release package has only `qopensslbackend`; DLL loading restrictions; clean install; legacy `WITS.exe` end-to-end (login, RFID, photos, the admin operations it performs, turnstile + bridge); time sync; recorded dry runs of runbooks 2, 4, 6, 9. |
| 9 | Functional regression | Kiosk student / RFID / guest attendance; admin login + guarded operations; student database operations and imports; reporting and exports (incl. the `api.php/reports/data` route); turnstile polling on the gate PC, reconnection and cursor preservation (`AccessControlHub`, `qt-app/quick/AccessControlHub.h`); remote photos, imported logos/posters, fallback avatars, bundled QML assets; all existing `CapturingNam` / `SequencedNam` suites (`qt-app/testsupport/`) adapted to inject through `HttpClient`. |

S1a MUST NOT change behaviour (Passthrough mode, Section 3); Layer 9 proves it. Passing security tests MUST NOT conceal broken functionality.

**Distinction:** the automated gate verifies certificate validation, exclusive trust, redirects and QML image requests through controlled test infrastructure. Deployment acceptance separately verifies real Windows 11 behaviour (AVG, system-root distrust, DLL loading, packaged release restrictions).

## 7. Slicing, packaging, cutover, rollback (S1f)

### Slices

One spec (this one) and one implementation plan per slice, written as each slice starts. The spec is the single source of truth; slice plans describe implementation and MUST NOT redefine security rules.

| Slice | Content | Depends on |
|---|---|---|
| **S1a** Client networking seam | All controllers/ViewModels/hubs + QML engine networking onto `HttpClient` / `PolicyEnforcingNam` with injectable managers (Section 3 injection boundary); behaviour-neutral via the temporary **Passthrough** policy mode; plus the `LOAMS_BUILD_LEGACY_WIDGETS=OFF` freeze. | — |
| **S1b** Server stack & host | Manifest (server + client sections), `Test-LoamsServerHost.ps1`, service identity, ACLs, event source, staging upgrade runbook. | — |
| **S1c** Certificate tooling + runbooks | Section 5. | S1b (manifest OpenSSL) |
| **S1d** Server transport guard | Apache layout (incl. exact `api.php` path-info routes and `AuthMerging Off` + `<RequireAll>` composition), `auto_prepend_file` guard with canonical-path matching, evidence-based allowlist, kill switches, two-layer logging, controller IP moved from the compiled constant to admin-only server config, `turnstile_display.php` kept gate-PC-only with loopback TLS requests (`REMOTE_ADDR` loopback over `:443`) handled correctly, `Test-LoamsDeployment.ps1`. | S1b, S1c |
| **S1e** Client TLS enforcement | Restructured `qt-app/quick/main.cpp` bootstrap order (Phases A–C), OpenSSL backend + manifest checks, env/DLL/`qt.conf` neutralisation, exclusive CA from `config.ini` with release fail-closed parsing, release compile-outs, deletion of Passthrough mode, test PKI fixtures + negative suites, rotation / trust epoch with precise deadline timer, expiry warnings, error UI, URL interceptor incl. the app-data asset allowance. | S1a, S1c, S1b's client manifest |
| **S1f** Packaging, acceptance & cutover | Packaging, signed release records, Layer 8, production cutover. | All; requires successful end-to-end integration of S1d + S1e incl. QML image loading and legacy compatibility |

Conditions:

- S1a–S1e MAY merge independently after review, but incomplete security changes MUST NOT reach production.
- Only S1f authorises production cutover. No release package MAY be built from `master` for production use until S1f's gate passes (release checklist; enforced by S1f packaging).
- Each slice needs passing tests, security review where applicable, and documented rollback considerations before approval. A passing unit test alone is not sufficient.

**What cutover means:** S1 production cutover is **server-side**. The site still runs legacy `WITS.exe`. 2.0 client PCs are deployed later from an authorised package during the 2.0 rollout, repeating machine-specific acceptance.

### Client package — New-LoamsReleasePackage.ps1

The only release boundary. Packaging invokes the gate itself; a developer-run script alone is not an enforceable boundary.

- Clean checkout of a tagged commit; refuse a dirty tree; verify the tag resolves to the recorded commit (`git rev-parse <tag>^{commit}`).
- Release build with `LOAMS_DEV_TRANSPORT_OVERRIDES=OFF` and `LOAMS_BUILD_LEGACY_WIDGETS=OFF`.
- File-set allowlist: only `qopensslbackend` under `plugins/tls`, only manifest-pinned OpenSSL DLLs, no `WITS.exe`; any unexpected DLL/plugin → fail.
- Run gate layers 1–4 and 9 against the packaged binaries.
- Produce a release record.

### Server bundle — New-LoamsServerBundle.ps1

- Contents: the approved PHP API + guard, Apache config + compat allowlist, deployment scripts, stack manifest + verified dependencies.
- Same tagged commit and manifest version as the client package.
- The bundle is verified against its release record by the independent verifier (see Release records) before any of its scripts run or the gate PC is modified.

### Release records

Release records (artifact SHA-256s, commit, tag, manifest version, gate results) are signed with the offline Ed25519 release-signing key (OpenSSL). Authenticity never depends solely on a checksum stored beside the package. Authenticode is deferred.

Verification MUST NOT be circular:

- It MUST run **before** any script or executable from the package/bundle is executed.
- It uses a verifier and Ed25519 public key provisioned independently of the package — e.g. a standalone `Test-LoamsRelease.ps1` + public key installed once out-of-band (admin media / the offline CA machine), using an already-trusted OpenSSL. The public key's fingerprint is confirmed out-of-band.
- A package's own scripts never vouch for themselves; installers and deployment scripts run only after the independent verifier has passed.

### Production cutover runbook

Gate PC, in a maintenance window outside library hours.

| Step | Action |
|---|---|
| 0 | **Preconditions:** S1a–S1e merged; layers 1–9 passed with signed-off evidence, including Layer 8 on a representative Windows 11 machine; runbook dry runs recorded; controller-outage evidence collected (below); type=100 rule satisfied (below); maximum window length, measurable success criteria, rollback deadline (= window end − measured restore time) and rollback owner fixed in advance. |
| 1 | `Test-LoamsServerHost.ps1 -Report` matches the staging-validated profile. |
| 2 | Converge with backups (consistent DB snapshot) + stack upgrade to Server Stack v1.0 if needed. |
| 3 | Service identity, ACLs, event source. |
| 4 | Server key + CSR → offline signing → `Install-LoamsServerCert.ps1` live probe. |
| 5 | Apache layout, guard, allowlist, firewall rule; explicitly enable `LegacyLoopbackHttp` and `ControllerHttp`. |
| 6 | `Test-LoamsDeployment.ps1` + production rechecks: real legacy `WITS.exe` end-to-end (RFID, photos, admin), real controller swipe + bridge display, firewall, TLS endpoint. |
| 7 | Commit, or approved rollback. |

Any failed or not-run mandatory check is a failure. There is no informal override; on failure, stop and initiate the approved rollback.

### Rollback

- Restore from the consistent snapshot.
- Idempotent reconciliation of post-snapshot attendance — `library_visits`, `turnstile_events` **and** dependent state, including `students.visits`, constraints and any other state affected by attendance writes — so that nothing is duplicated or lost.
- Verify restored service, DB consistency, attendance behaviour, firewall rules and legacy compatibility.
- If rollback fails → RECOVERY REQUIRED + operational contingency (manual attendance).
- Rolling back to the old stack restores known vulnerabilities: this is a documented, time-limited exposure with restricted access and a mandatory dated remediation plan.

### Controller outage evidence (S1f precondition, critical)

A controlled server-disconnection test, in staging or an approved window, documents separately:

| Question | Records |
|---|---|
| **Access** | Do cards open the gate without the server? |
| **Recording** | Are swipes buffered? |
| **Recovery** | Are they replayed after reconnection, without duplicates? As type=100? With what fields? |

The runbook states only what was observed. It MUST NOT promise lossless attendance without evidence.

**Finding:** `turnstile.php` currently acknowledges type=100 historical swipe records by echoing `IndexEvent` **without** recording attendance (`deliverables/loams_api/turnstile.php:244-250`). Replayed swipes would therefore be silently lost.

### type=100 recording fix

The fix is a separate task and is **not** part of S1. S1f rule:

- If testing confirms the controller buffers and replays via type=100, the fix MUST be completed and validated before S1f production cutover.
- If the controller cannot buffer/replay, document the limitation and require approved manual attendance + reconciliation procedures.

Fix completion criteria:

- Replay behaviour verified with captured protocol examples.
- Idempotent recording using verified device/event identifiers, accounting for index resets/reuse.
- Original event timestamp preserved.
- Acknowledge only after durable recording.
- Tests for duplicates, replay ordering, DB failures and reconnection.

### Retirement (later, after field-validation criteria)

1. Switch both kill switches off.
2. Observe both log layers for a defined period with no legitimate legacy traffic.
3. Remove the HTTP vhosts, allowlist and firewall rule.
4. Delete the Widgets source.
5. Archive the preserved legacy binary.

## 8. Residual-risk register & non-goals

### Residual risks

| # | Risk | S1 mitigation | Closes when |
|---|---|---|---|
| R1 | Unauthenticated attendance writes (`student_login.php`, `rfid_login.php`, `guest_login.php`): HTTPS protects transport but does not authenticate attendance-producing clients; unauthorised LAN hosts may submit fabricated attendance. | Temporary network restrictions, monitoring, rate limiting where operationally feasible — these do not replace authentication. | S2 |
| R2 | Controller IP spoofing: ARP/IP spoofing can forge swipes or inflate attendance. | Apache `Require ip`, firewall, recommended VLAN, compatibility logging. | Only when the controller-to-server path has verified authentication or an equivalent enforceable protection. S2 device tokens alone do not authenticate the Cloud+ if its firmware cannot send authenticated requests; otherwise the risk stays documented. |
| R3 | The loopback compatibility path authenticates locality, not `WITS.exe`; any compromised local process could use it. | Exact endpoint/method allowlist, two-layer logs, kill switch. | Legacy retirement |
| R4 | Loopback HTTP is a temporary plaintext-credential exposure: legacy `WITS.exe` sends the shared admin key over loopback HTTP. Local malware, compromised applications or processes with sufficient access may intercept credentials or exploit compatibility endpoints. | Endpoint restrictions, host security, monitoring. | Legacy retirement |
| R5 | The shared admin key remains the 2.0 admin credential (now encrypted in transit). | — | S3 |
| R6 | The TLS private key is readable by compromised PHP. | Dedicated service identity, compromise runbook. | Not addressed in S1 |
| R7 | No automatic revocation checking (LOAMS or institutional CA). | Annual keys, emergency CA rotation runbook. | Future, only if Qt-verified revocation is proven |
| R8 | An institutional root accepts any certificate it issues for the LOAMS hostname. | Documented; prefer a dedicated LOAMS CA or institutional sub-CA. | Accepted |
| R9 | Client clock rollback extends trust in a retiring CA. | Admin-controlled clocks, time-sync acceptance check. | Accepted |
| R10 | No Authenticode signing. | Signed release records, admin-only install directory. | Future code-signing certificate |
| R11 | Rollback temporarily restores a vulnerable stack. | Time-limited, restricted, dated remediation plan. | Per occurrence |
| R12 | Swipes during server downtime may be lost (type=100 ACKed but not recorded). | Evidence test, manual reconciliation. | Separate type=100 fix (criteria in Section 7) |
| R13 | Turnstile display is limited to the gate PC: `turnstile_display.php` returns student identity data and S1 does not authenticate clients, so other PCs cannot poll it. | Served only to loopback / the server's own addresses; gate PC 2.0 client uses `https://localhost/loams_api/`. | S2 device identity |

### Non-goals

- S2–S4 scope.
- Mutual TLS.
- CRL/OCSP infrastructure.
- Reverse-proxy deployments.
- Public CAs, ACME, automated renewal.
- Non-Windows servers.
- Widgets app migration.
- Authenticode.
- The type=100 recording fix (tracked separately).
- Networked turnstile-display polling from PCs other than the gate PC (S2).
