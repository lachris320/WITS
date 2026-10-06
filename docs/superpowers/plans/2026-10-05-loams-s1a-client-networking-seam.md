# LOAMS S1a — Client Networking Seam & Legacy Widgets Freeze — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route every `WITSQuick` network request — controllers, ViewModels, the Access Control hub and the QML engine — through one `TransportPolicy` / `PolicyEnforcingNam` / `HttpClient` seam running in a behaviour-neutral **Passthrough** mode, and freeze the legacy Widgets target behind `LOAMS_BUILD_LEGACY_WIDGETS=OFF`.

**Architecture:** A new `qt-app/core/transport/` module (compiled into `witscore`) holds an immutable `TransportPolicy` (shared as `std::shared_ptr<const TransportPolicy>`, published through a process-wide atomic snapshot), a `PolicyEnforcingNam` (`QNetworkAccessManager` subclass whose `createRequest()` is the single choke point; Passthrough forwards unchanged), and an `HttpClient` that owns one controller-facing `PolicyEnforcingNam` or uses an injected manager. Every owner that built a `QNetworkAccessManager` today instead builds (or is injected with) an `HttpClient`, keeping today's one-manager-per-owner topology; a `PolicyNamFactory` (`QQmlNetworkAccessManagerFactory`) installed on the QML engine creates a new `PolicyEnforcingNam` per `create()` call, from any thread. S1e later swaps the policy's internals without touching any call site.

**Tech Stack:** Qt 6.11.1 (MinGW 64-bit) / C++17, CMake + Ninja, QtNetwork, QtQml / Qt Quick, QtTest (`wits_add_qttest`), Qt Quick Test (`wits_add_qmltest`), CTest.

## Global Constraints

- **Spec is the single source of truth:** `docs/superpowers/specs/2026-10-05-loams-s1-transport-security-design.md`. "Slice plans describe implementation and MUST NOT redefine security rules." Nothing here adds, weakens or re-words an S1 security rule.
- **Behaviour-neutral:** "S1a MUST NOT change behaviour (Passthrough mode, Section 3); Layer 9 proves it." Same URLs, same headers/bodies, same redirect handling, same TLS settings, same error paths.
- **Passthrough semantics:** "preserves today's behaviour: the current `http` default is allowed and no TLS enforcement is applied." No redirect-policy forcing, no `QSslConfiguration` changes, no scheme/origin checks in S1a — those are S1e.
- **Passthrough is temporary:** "Passthrough exists only until S1e, which deletes it." "Passthrough can never reach production."
- **Policy object:** "`TransportPolicy` is immutable, shared as `std::shared_ptr<const TransportPolicy>`, and thread-safe"; snapshots are read/written with C++17 `std::atomic_load` / `std::atomic_store` on the `shared_ptr`.
- **Managers:** "`HttpClient` owns the controller-facing manager." "A `QQmlNetworkAccessManagerFactory` creates a **new** QML-owned manager on each `create()` call (which may be called from multiple threads), using the same `TransportPolicy`." "Both manager kinds are `PolicyEnforcingNam` instances."
- **Injection boundary:** "`HttpClient` accepts an injected `QNetworkAccessManager` / manager factory, so the existing `CapturingNam` / `SequencedNam` tests stay realistic." ViewModels/hubs that construct managers internally "MUST obtain them from the seam instead."
- **Owner invariant (S1a plan approval condition):** every production network request passes through the authoritative transport policy. In LOAMS 2.0 production every core network class (`StudentController`, `VisitorController`, `ReportController`, `ImportController`, `BrandingController`, `AccessControl::AccessDecisionService`, `AccessControl::TurnstileProvider`) receives the manager supplied by its owner's `HttpClient::manager()` (a `PolicyEnforcingNam`). Raw `QNetworkAccessManager*` injection on these classes remains **only** as a test seam; no production constructor may create an unprotected manager, and a null manager is a contract violation (`Q_ASSERT_X`, Task 4b). Frozen Widgets compatibility never weakens this rule — a resulting Widgets incompatibility is documented, not worked around.
- **Mandatory gates (S1a plan approval condition):** GATE G1 (Task 2: `std::atomic_load`/`std::atomic_store` on `shared_ptr` compiles warning-free under `-Wall -Wextra -Werror`) and GATE G2 (Task 5b: Qt 6.11.1's image/Canvas loading uses the manager from the installed factory) run before any task that depends on them. **If a gate fails: STOP — do not continue dependent tasks and do not substitute a workaround; escalate to the owner for a design review.**
- **Factory ordering:** the QML network factory is installed on the engine before any QML is loaded and before any request is initiated. Enforcement is two-part and its limits are explicit: (1) `quick/main.cpp` installs on a freshly constructed `QQmlApplicationEngine` immediately before `loadFromModule`, pinned by source-order guard `quickMainInstallsTransportBeforeLoad` (Task 7); (2) at runtime `PolicyNamFactory::installOn()` (Task 5) refuses — and `main.cpp` fails closed — in exactly three detectable cases: the engine already has a factory, the engine has already created its own manager, or a `QQmlApplicationEngine` already has root objects. It does **not** detect a plain `QQmlEngine`/`QQuickView` that compiled components without creating a manager; that case is covered only by the fresh-engine + source-order rule.
- **SSL errors:** "`ignoreSslErrors` MUST NOT appear anywhere in client source; a source grep check enforces this." (Enforced from S1a onward by `tst_transportseamguard`.)
- **Legacy Widgets:** "The target sits behind CMake option `LOAMS_BUILD_LEGACY_WIDGETS=OFF` by default; release packaging explicitly rejects the legacy target." Source is "deprecated / reference-only: no new features, security migrations or routine maintenance." Widgets app migration is a spec non-goal: `mainwindow.cpp`, `adminwindow.cpp`, `guestwindow.cpp` are **not** migrated.
- **Release boundary:** "No production deployment before S1f." "S1a–S1e MAY merge independently after review, but incomplete security changes MUST NOT reach production." No release-packaging logic in S1a.
- **Slice approval:** "Each slice needs passing tests, security review where applicable, and documented rollback considerations before approval. A passing unit test alone is not sufficient."
- **No secrets / PII:** synthetic fixtures only (`Test Student A`, `TEST-0001`, `21-1-0001`, `20260001`, key `s1a-test-key`). No real admin key, backend URL credential or student data in code, tests, commits or screenshots.
- **No test may reach a backend (Codex R1 condition):** every seam owner (ViewModel / `AccessControlHub`) a test constructs receives an injected fake through `HttpClient` — `CapturingNam`, `SequencedNam`, or the never-answering `OfflineHttp` — except request-free `defaultConstruction*` / `defaultHub*` ownership tests; enforced by `tst_transportseamguard::ownersInTestsUseFakeManagers` (Task 6b) and proven empirically by an unchanged dev-Apache access log across the full `ctest` run (Task 14 Step 1).
- **No external network in tests:** request assembly via `CapturingNam` / `SequencedNam`; wire-level parity and QML image loads via the in-process `TinyHttpServer`, which listens **only on `127.0.0.1` with an ephemeral port** (`QTcpServer::listen(QHostAddress::LocalHost, 0)`) and **never touches XAMPP** or any other real backend.
- **MVVM:** ViewModels (`quick/viewmodels/`) remain the only QML-facing C++; `HttpClient` / `PolicyNamFactory` are not QML types. No QML visual change in S1a; any QML touched keeps **zero raw hex outside `Theme.qml`** (`Qt.alpha(Theme.<token>, a)` for opacity).
- **Naming:** `core/` files lowercase (`httpclient.h`), `quick/` C++ files PascalCase (`PolicyNamFactory.h`), members `m_camelCase`, function-pointer `connect`.
- **Tests:** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add `OFFSCREEN` for any GUI/Quick/painting test; QML tests via `wits_add_qmltest()`. Security/guard tests fail (`QVERIFY`/`QFAIL`), never `QSKIP`.
- **Commits:** only via the project `commit` skill (`.claude/skills/commit/SKILL.md`), Conventional Commits, **no Claude/Anthropic co-author trailer** (standing owner rule). Never `--no-verify`.
- **Build hygiene:** build only into `C:/b/s1a` (and `C:/b/s1a-legacy` for the ON check) — never `qt-app/build`. Never launch many concurrent copies of the **same** test exe (AV lockup); `ctest -j` across different tests is fine.

### Build & test commands (Git Bash; prepend the PATH line in the same invocation)

```bash
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a                                   # full build: can exceed 10 minutes
cmake --build C:/b/s1a --target <target>                 # one target
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a --output-on-failure -j 8     # whole suite
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^<name>$' --output-on-failure   # one test
```

Every command block below assumes the `export PATH=...` line ran in the same invocation and the working directory is the repo root (the worktree root when using one). Run long builds with a generous timeout or in the background.

**Line numbers** in this plan refer to each file as it is on `master` **before** the task's own edits (an include inserted earlier in the same step shifts later lines by one). Always apply an edit by matching the quoted original text, not by line number alone.

### Owner-approved design decisions (2026-10-05)

1. **One `HttpClient` per owner** (ViewModel / hub), each owning one `PolicyEnforcingNam` — "one security policy, not one physical manager" — preserving today's per-owner lifetimes. An owner with several controllers (`DatabaseViewModel`) still has exactly one client; all its controllers share its manager.
2. **Constructor injection, one locked order for every owner** — ViewModels **and** `AccessControlHub`: `(QObject *parent = nullptr, HttpClient *http = nullptr)`; `HttpClient(QObject *parent = nullptr, QNetworkAccessManager *injected = nullptr)`.
3. **Core network classes — require a non-null manager (chosen approach).** Verified on `master`: none of the seven core network classes creates a manager when given `nullptr` (each just stores the pointer, `studentcontroller.cpp:18-21`, `visitorcontroller.cpp:18-21`, `reportcontroller.cpp:15-18`, `importcontroller.cpp:20-23`, `brandingcontroller.cpp:13-16`, `accessdecisionservice.cpp:16-19`, `turnstileprovider.cpp:20-23`), so there is no silent-default path to remove. S1a makes the contract explicit and enforced: each constructor gets `Q_ASSERT_X(nam, ...)` + a header contract ("non-null; in LOAMS 2.0 production it is `HttpClient::manager()`"), plus a read-only `networkManager()` accessor used by runtime identity tests. **Why not route a null default through the seam:** a hidden per-controller `HttpClient` would create an extra, ownerless manager the owner cannot see or (in S1e) epoch-manage, and would hide wiring bugs; requiring the owner's manager keeps exactly one policy-checked manager per owner and makes a miswired controller fail loudly. Applied identically to all seven classes (Task 4b). The five existing tests that passed `nullptr` for network-free paths now pass an unused test manager.
4. **`LOAMS_BUILD_LEGACY_WIDGETS`** replaces `BUILD_LEGACY_WIDGETS` (default OFF; the old variable is ignored with a warning).
5. **`ignoreSslErrors` ban starts in S1a** (`tst_transportseamguard`).
6. **In-process loopback test server** (`127.0.0.1`, ephemeral port 0; never XAMPP).
7. **Unverified Qt behaviour is gated early** (G1, G2 above).

### Before you start (baseline)

- [ ] Create an isolated worktree/branch with `superpowers:using-git-worktrees` (branch name `feat/s1a-client-networking-seam`, base `master`).
- [ ] Configure + full build into `C:/b/s1a` (commands above) and run the whole suite. **Expected baseline: 62 tests, all passed** (enumerated in Task 14). Record the count; if it is not 62/62 green, stop and report — S1a must start from green.

---

## File Structure

| File | Create / Modify | Single responsibility | Task |
|---|---|---|---|
| `qt-app/CMakeLists.txt` | Modify (lines 12-14, 38-39, end of file) | `LOAMS_BUILD_LEGACY_WIDGETS` option (default OFF), obsolete-option warning, WITS gate, gate self-check test | 1 |
| `qt-app/cmake/CheckLegacyWidgetsGate.cmake` | Create | `cmake -P` assertion: legacy targets exist iff the option is ON | 1 |
| `qt-app/tests/CMakeLists.txt` | Modify (lines 14-29, 54-68; append transport tests) | Gate the two Widgets-only tests; register core transport tests | 1, 2, 3, 4 |
| `qt-app/LEGACY-WIDGETS.md` | Create | Short freeze notice for the Widgets source | 1 |
| `CLAUDE.md` | Modify ("Two executables" bullet) | Document that `WITS` is frozen/OFF by default | 1 |
| `qt-app/core/transport/transportpolicy.h/.cpp` | Create | Immutable policy (Passthrough), atomic process-wide snapshot | 2 |
| `qt-app/core/transport/policyenforcingnam.h/.cpp` | Create | `QNetworkAccessManager` subclass; `createRequest()` choke point | 3 |
| `qt-app/core/transport/httpclient.h/.cpp` | Create | Owns one controller-facing `PolicyEnforcingNam` or wraps an injected manager | 4 |
| `qt-app/core/CMakeLists.txt` | Modify (witscore source list, after line 56) | Compile the transport module into `witscore` | 2, 3, 4 |
| `qt-app/testsupport/tinyhttpserver.h/.cpp` | Create | Test-only loopback HTTP/1.1 responder recording raw requests | 3 |
| `qt-app/tests/tst_transportpolicy.cpp` | Create | Policy immutability, Passthrough identity, atomic snapshot | 2 |
| `qt-app/tests/tst_policyenforcingnam.cpp` | Create | Wire-level parity with a plain `QNetworkAccessManager` | 3 |
| `qt-app/tests/tst_httpclient.cpp` | Create | Ownership / injection / snapshot semantics | 4 |
| `qt-app/core/{studentcontroller,visitorcontroller,reportcontroller,importcontroller,brandingcontroller}.h/.cpp`, `qt-app/core/accesscontrol/{accessdecisionservice,turnstileprovider}.h/.cpp` | Modify (ctor + one accessor) | Non-null manager contract (`Q_ASSERT_X`) + `networkManager()` test accessor | 4b |
| `qt-app/tests/tst_{studentcontroller,visitorcontroller,reportcontroller,importcontroller,brandingcontroller,accessdecisionservice,turnstileprovider}.cpp` | Modify | Accessor-identity test; 5 `nullptr` constructions → unused test manager | 4b |
| `qt-app/quick/PolicyNamFactory.h/.cpp` | Create | QML engine factory: new `PolicyEnforcingNam` per `create()`; `installOn()` refuses a late install | 5 |
| `qt-app/quick/tests/tst_qmlfactorygate.cpp` | Create | GATE G2: Qt image + Canvas loads use the installed factory's manager | 5b |
| `qt-app/quick/tests/RecordingPolicyNam.h` | Create | Test-only recording `PolicyEnforcingNam` + `PolicyNamFactory` subclass (request log = proof of seam use) | 5b, 13 |
| `qt-app/testsupport/sequencednam.h/.cpp` | Modify | `setStallWhenEmpty(bool)` (default off: existing suites unchanged) | 6b |
| `qt-app/testsupport/offlinehttp.h` | Create | Test-only `HttpClient` over a never-answering `SequencedNam` | 6b |
| `qt-app/tests/tst_httpclient.cpp` + its CMake entry | Modify | `offlineHttpNeverAnswers` | 6b |
| `qt-app/quick/CMakeLists.txt` | Modify (module SOURCES line 82; quick test registrations) | Compile factory into `witsquickmodule`; `WITS_TEST_NAM_SOURCES`; register/adjust quick tests | 5, 5b, 6, 6b, 13 |
| `qt-app/quick/tests/tst_policynamfactory.cpp` | Create | Factory per-call/threaded creation, engine integration | 5 |
| `qt-app/quick/tests/tst_transportseamguard.cpp` | Create | Source guard: no raw manager outside the seam; no SSL-error bypass; main.cpp wiring | 6, 7 |
| `qt-app/quick/main.cpp` | Modify (lines 1-58) | Publish Passthrough policy; install `PolicyNamFactory` before first QML load | 7 |
| `qt-app/quick/tests/QuickTestSetup.h` | Modify | Install the factory on every QuickTest engine | 7 |
| `qt-app/quick/tests/tst_appshell.cpp` | Modify | AppShell loads through the seam with zero warnings | 7 |
| `qt-app/quick/AccessControlHub.h/.cpp` | Modify (h:14,34-35,70-78; cpp:1-30) | Hub obtains its manager from an `HttpClient` | 8 |
| `qt-app/quick/tests/tst_accesscontrolhub.cpp` | Modify | `SequencedNam` injected through `HttpClient` | 8 |
| `qt-app/quick/viewmodels/{Dashboard,VisitLogs,AccessControl}ViewModel.h/.cpp` | Modify (ctor + `m_nam` member) | Obtain manager from `HttpClient` | 9 |
| `qt-app/quick/tests/tst_{dashboard,visitlogs,accesscontrol}viewmodel.cpp` | Modify | Inject through `HttpClient`; seam-ownership test | 9 |
| `qt-app/quick/viewmodels/{Kiosk,Guest}ViewModel.h/.cpp` | Modify | Obtain manager from `HttpClient` | 10 |
| `qt-app/quick/tests/tst_{kiosk,guest}viewmodel.cpp` | Modify | Injection + seam-ownership tests | 10 |
| `qt-app/quick/viewmodels/{Search,Database}ViewModel.h/.cpp` | Modify | Obtain manager(s) from `HttpClient` | 11 |
| `qt-app/quick/tests/tst_{search,database}viewmodel.cpp` | Modify | Injection + seam-ownership tests | 11 |
| `qt-app/quick/viewmodels/{Import,Reporting,Settings}ViewModel.h/.cpp` | Modify | Obtain manager from `HttpClient` | 12 |
| `qt-app/quick/tests/tst_{import,reporting,settings}viewmodel.cpp` | Modify | Injection + seam-ownership tests | 12 |
| `qt-app/quick/tests/tst_qmlresourceinventory.cpp` | Create | Every inventoried resource loads through the QML factory | 13 |
| `docs/superpowers/proofs/2026-10-05-loams-s1a-proof.md` | Create | Layer 9 evidence: suite results, parity evidence, manual smoke record | 14 |

### S1a resource inventory (spec §3 "Resource inventory")

Every resource the 2.0 client actually loads, how it is loaded today, and the S1a test that proves it still loads through the seam.

| # | Resource | Built by | Loaded by | URL kind | Network path | S1a test |
|---|---|---|---|---|---|---|
| R1 | Bundled QML module (all `qml/**/*.qml`, incl. `LAvatar`/`LLogoCircle`/`LCircleImage`) | `qt_add_qml_module(witsquickmodule)` (`quick/CMakeLists.txt:33-111`) | QML engine (`loadFromModule`) | `qrc:/qt/qml/LOAMS/...` | none (resource system) | `tst_qmlresourceinventory::everyBundledQmlFileCompilesFromQrc` (each of the 43 `qml/**/*.qml` files loaded from `qrc:/qt/qml/LOAMS/qml/...` and compiled by a seam-installed engine), `bundledModuleQmlLoadsFromQrc` (module lookup), `tst_appshell::loadsWithZeroWarnings` (live AppShell instantiation) |
| R2 | Kiosk photo from a turnstile entry | `LoginParser::parseEntryEvent` → `sameOriginPhotoUrl` (`core/loginparser.cpp:23-46,158-166`) → `KioskViewModel::applyStudentLogin` (`KioskViewModel.cpp:123`) | `LAvatar` → `LCircleImage` `Image` + `Canvas.loadImage` (`LCircleImage.qml:36-70`) | remote `http(s)` same-origin | QML engine NAM (pixmap-reader thread) → `PolicyNamFactory` | `turnstilePhotoLoadsThroughFactory` |
| R3 | Kiosk photo from student/RFID login | backend `photo_url` passed through `LoginParser::parseRfidResponse` / `parseLoginResponse` (`KioskViewModel.cpp:123`) | same as R2 (`KioskMain.qml:104-122`) | remote `http(s)` | same as R2 | `loginPhotoUrlLoadsThroughFactory` |
| R4 | Search result avatar | `SearchResultsModel::PhotoRole` = `ApiConfig::endpoint(photo)` (`models/SearchResultsModel.cpp:30-35`) | `LAvatar` (`SearchScreen.qml:500-507`) | remote `http(s)` | same as R2 | `searchAvatarLoadsThroughFactory` |
| R5 | Fallback avatar: empty photo / `default.jpg` sentinel | `LAvatar._emptyOrSentinel` (`LAvatar.qml:33-61`) | initials chip — **no load** | — | none (never requested) | `defaultSentinelAndEmptyPhotoNeverRequested` |
| R6 | Fallback avatar: missing/broken remote photo | backend URL that 404s | `LAvatar` `Image.Error` → initials (`LAvatar.qml:39`) | remote `http(s)` | same as R2 | `missingRemotePhotoFallsBackToInitials` |
| R7 | Imported school logo (kiosk `BrandPanel`, admin `LSidebarBrand`) | `SettingsController::importImageFile` copies into `AppDataLocation` (`core/settingscontroller.cpp:70-90`); `SchoolInfoUtil::resolveLogoUrl` (`viewmodels/SchoolInfoUtil.cpp:7-11`) | `LLogoCircle` (`LLogoCircle.qml:22-25`) | `file:` (app-data asset dir) | none (local file read by `QQuickPixmap`) | `importedLogoFileUrlLoads` |
| R8 | Settings logo preview | `SettingsViewModel::logoUrl()` (`SettingsViewModel.cpp:41-66`, `file:` + `?v=` token) | `Image` (`SettingsScreen.qml:192-198`) | `file:` | none | `settingsLogoPreviewLoads` |
| R9 | Logo placeholder ("LOGO") | `LLogoCircle` `hasLogo:false` | no load | — | none | existing `tst_qml_components` (`test_noLogoShowsPlaceholder` family) |
| R10 | Imported poster | `SettingsController::importPoster` (`settingscontroller.cpp:59-61`) stores `school/posterPath` | **not displayed by any 2.0 QML** | — | none | `posterIsNotDisplayedByAnyQml` (fails if a screen starts showing one, forcing a load test) |
| R11 | C++ local image reads: BrandTheme logo palette (`core/brandtheme.cpp:240-286`), report export logo (`ReportingViewModel.cpp:661`), import validation `QImage` (`settingscontroller.cpp:70`) | C++ | `QImageReader` / `QSvgRenderer` / `QImage` from a path | filesystem path | none (never a URL, never a NAM) | existing `tst_brandtheme`, `tst_reportrenderer`, `tst_settingsviewmodel` (unchanged) |
| R12 | Test-only `data:` URIs | QuickTest fixtures (`tst_qml_components.qml:24-38,250-290`) | `Image` | `data:` | none | not a production resource; listed so S1e (URL interceptor rejects `data:`) migrates these fixtures |

No `FontLoader`, `XMLHttpRequest`, `BorderImage`/`AnimatedImage` or `image://` provider exists in `qt-app/quick/qml` today.

### Manager-owner inventory (spec §3 "Injection boundary")

`grep -rn "QNetworkAccessManager" qt-app --include=*.cpp --include=*.h` (excluding `libs/`, build dirs) — every place that **constructs** a manager:

| Owner | Today | After S1a | Task |
|---|---|---|---|
| `quick/AccessControlHub.cpp:20-29` | `std::make_unique<QNetworkAccessManager>()` unless injected | owned `std::unique_ptr<HttpClient>` unless an `HttpClient*` is injected | 8 |
| `quick/viewmodels/DashboardViewModel.cpp:13` | `nam ? nam : new QNetworkAccessManager(this)` | `http ? http : new HttpClient(this)` | 9 |
| `quick/viewmodels/VisitLogsViewModel.cpp:19` | same | same | 9 |
| `quick/viewmodels/AccessControlViewModel.cpp:16` | same | same | 9 |
| `quick/viewmodels/KioskViewModel.cpp:20` | `new QNetworkAccessManager(this)` | `http ? http : new HttpClient(this)` | 10 |
| `quick/viewmodels/GuestViewModel.cpp:11` | same | same | 10 |
| `quick/viewmodels/SearchViewModel.cpp:9` | same | same | 11 |
| `quick/viewmodels/DatabaseViewModel.cpp:13,23` | two managers (`m_nam`, `m_editNam`) | **one** `HttpClient` (owned or injected) whose manager both `StudentController`s use; `m_editNam` removed (no code depends on separate managers — Task 11) | 11 |
| `quick/viewmodels/ImportViewModel.cpp:13` | `new QNetworkAccessManager(this)` | `http ? http : new HttpClient(this)` | 12 |
| `quick/viewmodels/ReportingViewModel.cpp:22` | same | same | 12 |
| `quick/viewmodels/SettingsViewModel.cpp:28` | same | same | 12 |
| QML engine (`quick/main.cpp`) | Qt default factory | `PolicyNamFactory` installed before first load | 7 |

Core network classes already take an injected pointer and **never** create a manager themselves: `StudentController`, `VisitorController`, `ReportController`, `ImportController`, `BrandingController` (`core/*controller.*`), `AccessControl::AccessDecisionService`, `AccessControl::TurnstileProvider` (`core/accesscontrol/*`); `HttpForm::submit/get` (`quick/HttpForm.*`) is a free-function helper over the caller's pointer. Task 4b makes their non-null contract explicit (`Q_ASSERT_X`) and adds a `networkManager()` accessor; in 2.0 production their manager is always the owning ViewModel's / hub's `HttpClient::manager()`, proved at runtime by identity tests in Tasks 8, 11 and 12 (`qobject_cast<PolicyEnforcingNam *>(controller->networkManager())` and `controller->networkManager() == owner's HttpClient::manager()`). Their `CapturingNam`/`SequencedNam` suites (`tst_studentcontroller`, `tst_importcontroller`, `tst_reportcontroller`, `tst_brandingcontroller`, `tst_visitorcontroller`, `tst_accessdecisionservice`, `tst_turnstileprovider`, `tst_accesscontrolservice`) keep injecting directly — the raw-pointer constructor is their test seam only. Production owners per class: `StudentController` ← `SearchViewModel`, `DatabaseViewModel` (two controllers sharing its one client); `ImportController` ← `ImportViewModel`; `ReportController` ← `ReportingViewModel`; `TurnstileProvider` ← `AccessControlHub` (via its provider factory lambda, `AccessControlHub.cpp:126-132`); `VisitorController`, `BrandingController`, `AccessDecisionService` have **no 2.0 production owner** today (legacy Widgets / tests only) — any future 2.0 owner must pass `HttpClient::manager()`. Frozen legacy owners (`mainwindow.cpp:52,246`, `adminwindow.cpp:201,745`, `guestwindow.cpp:14`) are **not** migrated. Test doubles (`testsupport/capturingnam.*`, `testsupport/sequencednam.*`, `tests/tst_accessdecisionservice.cpp:33`) stay plain `QNetworkAccessManager` subclasses.

---

### Task 1: Freeze the legacy Widgets target behind `LOAMS_BUILD_LEGACY_WIDGETS` (default OFF)

**Files:**
- Create: `qt-app/cmake/CheckLegacyWidgetsGate.cmake`
- Create: `qt-app/LEGACY-WIDGETS.md`
- Modify: `qt-app/CMakeLists.txt` (lines 12-14 option, line 38-39 `if`, append after line 103)
- Modify: `qt-app/tests/CMakeLists.txt` (wrap lines 14-29 `tst_rfidkeyboardfilter` and lines 54-68 `tst_responsive_ui`)
- Modify: `CLAUDE.md` ("Two executables" bullet under "Build & run")
- Test: CTest `tst_legacywidgetsgate` (a `cmake -P` script test)

**Interfaces:**
- Consumes: existing `WITS`, `tst_rfidkeyboardfilter`, `tst_responsive_ui` targets.
- Produces: cache option `LOAMS_BUILD_LEGACY_WIDGETS:BOOL` (default `OFF`); CTest `tst_legacywidgetsgate`. The obsolete `BUILD_LEGACY_WIDGETS` is ignored with a warning.

- [ ] **Step 1: Write the failing gate check.** Create `qt-app/cmake/CheckLegacyWidgetsGate.cmake`:

```cmake
# Legacy Widgets freeze self-check (S1 spec §3 "Legacy Widgets source").
#
#   cmake -DOPTION=<LOAMS_BUILD_LEGACY_WIDGETS value>
#         -DHAS_WITS=<0|1> -DHAS_RFIDKBD=<0|1> -DHAS_RESPONSIVE=<0|1>
#         -P CheckLegacyWidgetsGate.cmake
#
# The HAS_* values are $<TARGET_EXISTS:...> generator expressions evaluated by
# the real generated build system, so this asserts what was actually
# configured: the frozen WITS target and its two Widgets-only test targets
# exist exactly when the option is ON (default OFF).
if(OPTION)
    set(_expect 1)
else()
    set(_expect 0)
endif()
set(_bad "")
foreach(_name HAS_WITS HAS_RFIDKBD HAS_RESPONSIVE)
    if(NOT "${${_name}}" STREQUAL "${_expect}")
        list(APPEND _bad "${_name}=${${_name}}")
    endif()
endforeach()
if(_bad)
    message(FATAL_ERROR
        "LOAMS_BUILD_LEGACY_WIDGETS='${OPTION}' expects ${_expect} for every "
        "legacy target, got: ${_bad}")
endif()
message(STATUS "legacy widgets gate OK (LOAMS_BUILD_LEGACY_WIDGETS='${OPTION}')")
```

Append to the end of `qt-app/CMakeLists.txt` (after `add_subdirectory(tests)`, line 103):

```cmake

# Legacy Widgets freeze self-check: WITS and its two Widgets-only test targets
# exist exactly when LOAMS_BUILD_LEGACY_WIDGETS is ON (default OFF). A plain
# add_test (no executable of its own), like a cmake -P guard.
add_test(NAME tst_legacywidgetsgate
    COMMAND ${CMAKE_COMMAND}
        -DOPTION=${LOAMS_BUILD_LEGACY_WIDGETS}
        -DHAS_WITS=$<TARGET_EXISTS:WITS>
        -DHAS_RFIDKBD=$<TARGET_EXISTS:tst_rfidkeyboardfilter>
        -DHAS_RESPONSIVE=$<TARGET_EXISTS:tst_responsive_ui>
        -P ${CMAKE_SOURCE_DIR}/cmake/CheckLegacyWidgetsGate.cmake)
```

- [ ] **Step 2: Run it — expected FAIL** (the option does not exist yet, so `OPTION` is empty → expects 0, but the old `BUILD_LEGACY_WIDGETS` default ON still builds all three targets):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
ctest --test-dir C:/b/s1a -R '^tst_legacywidgetsgate$' --output-on-failure
```

Expected: `1 tests failed`, output contains `LOAMS_BUILD_LEGACY_WIDGETS='' expects 0 for every legacy target, got: HAS_WITS=1;HAS_RFIDKBD=1;HAS_RESPONSIVE=1`.

- [ ] **Step 3: Implement the option and gates.** In `qt-app/CMakeLists.txt` replace lines 12-14:

```cmake
# Rollback gate (proposal §7.6 / spec §2 Strategy A): keeps the legacy Widgets
# app building until Phase 6 declares parity. Default ON for all of Phases 1–5.
option(BUILD_LEGACY_WIDGETS "Build the legacy WITS Widgets executable" ON)
```

with:

```cmake
# Legacy Widgets freeze (S1 transport-security spec §3 "Legacy Widgets source"):
# the WITS target is deprecated / reference-only and is NOT built by default.
# Developers may still build it with -DLOAMS_BUILD_LEGACY_WIDGETS=ON. It is never
# deployable or security-supported; S1f release packaging rejects it.
# See LEGACY-WIDGETS.md.
option(LOAMS_BUILD_LEGACY_WIDGETS
    "Build the frozen legacy WITS Widgets executable (reference only)" OFF)
if(DEFINED BUILD_LEGACY_WIDGETS)
    message(WARNING
        "BUILD_LEGACY_WIDGETS is obsolete and ignored; use "
        "-DLOAMS_BUILD_LEGACY_WIDGETS=ON (default OFF). Remove the stale cache "
        "entry with: cmake -U BUILD_LEGACY_WIDGETS -B <build dir>")
endif()
```

Replace lines 38-39:

```cmake
# --- Legacy Widgets app (rollback until Phase 6) ---
if(BUILD_LEGACY_WIDGETS)
```

with:

```cmake
# --- Legacy Widgets app (FROZEN, reference only — LEGACY-WIDGETS.md) ---
if(LOAMS_BUILD_LEGACY_WIDGETS)
```

In `qt-app/tests/CMakeLists.txt`, gate the two blocks that compile/load only frozen Widgets sources (`rfidkeyboardfilter.cpp`, the `*.ui` forms). Do not retype the blocks — only insert lines around them:

- Immediately **before** line 14 (`qt_add_executable(tst_rfidkeyboardfilter`) insert:

```cmake
# Widgets-only tests: they exercise frozen legacy sources (root
# rfidkeyboardfilter.cpp / the *.ui forms), so they build only with the legacy
# target (LOAMS_BUILD_LEGACY_WIDGETS=ON). See LEGACY-WIDGETS.md.
if(LOAMS_BUILD_LEGACY_WIDGETS)
```

- Immediately **after** line 29 (the `)` closing `set_tests_properties(tst_rfidkeyboardfilter ...`) insert `endif()`.
- Immediately **before** line 54 (`qt_add_executable(tst_responsive_ui`) insert `if(LOAMS_BUILD_LEGACY_WIDGETS)   # Widgets-only (*.ui forms); see above`.
- Immediately **after** line 68 (the `)` closing `set_tests_properties(tst_responsive_ui ...`) insert `endif()`.

Create `qt-app/LEGACY-WIDGETS.md`:

```markdown
# Legacy Widgets source — FROZEN (reference only)

The Qt Widgets `WITS` app in this directory — `main.cpp`, `mainwindow.*`,
`adminwindow.*`, `guestwindow.*`, `attachfilesdialog.*`, `busyindicator.*`,
`rfidkeyboardfilter.*`, the `*.ui` forms and `resources.qrc` — is deprecated
(S1 transport-security spec §3 "Legacy Widgets source").

- Not built by default: `LOAMS_BUILD_LEGACY_WIDGETS=OFF`. Developers may
  configure with `-DLOAMS_BUILD_LEGACY_WIDGETS=ON` (this also builds
  `tst_rfidkeyboardfilter` and `tst_responsive_ui`).
- No new features, security migrations or routine maintenance. Never deployable
  or security-supported; release packaging (S1f) rejects the target.
- Shared `core/` changes are not constrained by keeping it buildable; record any
  resulting incompatibility below.
- The binary deployed on the gate PC is a temporary security exception managed
  by the S1 server-side compatibility paths until LOAMS 2.0 passes field
  validation; then this source is deleted (spec §7 "Retirement").

## Known incompatibilities

- None yet.
```

In `CLAUDE.md`, replace the bullet that starts `- **Two executables:**` with:

```markdown
- **Two executables:** `WITSQuick` = the LOAMS 2.0 Qt Quick app, where all work lands (built by default). `WITS` = the legacy Qt Widgets app — **frozen, reference-only** (`qt-app/LEGACY-WIDGETS.md`), built only with `-DLOAMS_BUILD_LEGACY_WIDGETS=ON` (default OFF); the binary deployed at the client is preserved separately and is never rebuilt from this tree. Smoke-test `WITSQuick`.
```

- [ ] **Step 4: Run — expected PASS** (fresh configure, no `-D`):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake -LA -N C:/b/s1a | grep LOAMS_BUILD_LEGACY_WIDGETS     # LOAMS_BUILD_LEGACY_WIDGETS:BOOL=OFF
ctest --test-dir C:/b/s1a -R '^tst_legacywidgetsgate$' --output-on-failure
ctest --test-dir C:/b/s1a -N | tail -1                       # Total Tests: 61 (62 - 2 gated + 1 gate)
```

Expected: gate test passed; `Total Tests: 61`. Then prove ON still configures and builds for developers (separate dir, only the legacy targets):

```bash
cmake -S qt-app -B C:/b/s1a-legacy -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -DLOAMS_BUILD_LEGACY_WIDGETS=ON
cmake --build C:/b/s1a-legacy --target WITS tst_rfidkeyboardfilter tst_responsive_ui
QT_QPA_PLATFORM=offscreen ctest --test-dir C:/b/s1a-legacy -R '^(tst_legacywidgetsgate|tst_rfidkeyboardfilter|tst_responsive_ui)$' --output-on-failure
```

Expected: all three pass.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `build(cmake): freeze legacy Widgets target behind LOAMS_BUILD_LEGACY_WIDGETS (default OFF)`.

---

### Task 2: `TransportPolicy` — immutable policy with Passthrough mode and atomic snapshot

**Files:**
- Create: `qt-app/core/transport/transportpolicy.h`, `qt-app/core/transport/transportpolicy.cpp`
- Modify: `qt-app/core/CMakeLists.txt` (witscore list, insert after line 56 `accesscontrol/contactage.h ...`)
- Modify: `qt-app/tests/CMakeLists.txt` (append)
- Test: `qt-app/tests/tst_transportpolicy.cpp`

**Interfaces:**
- Consumes: Qt `QNetworkRequest`.
- Produces:
  - `class TransportPolicy` (non-copyable, non-assignable, private ctor)
  - `enum class TransportPolicy::Mode { Passthrough };`
  - `static std::shared_ptr<const TransportPolicy> TransportPolicy::makePassthrough();`
  - `TransportPolicy::Mode TransportPolicy::mode() const;`
  - `QNetworkRequest TransportPolicy::prepare(const QNetworkRequest &request) const;` (Passthrough: returns `request` unchanged)
  - `static std::shared_ptr<const TransportPolicy> TransportPolicy::current();` (atomic load; never null; defaults to Passthrough)
  - `static void TransportPolicy::setCurrent(std::shared_ptr<const TransportPolicy> policy);` (atomic store; null ignored with warning `TransportPolicy::setCurrent: null policy ignored`)

- [ ] **Step 1: Write the failing test.** Create `qt-app/tests/tst_transportpolicy.cpp`:

```cpp
#include <QtTest>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QThread>
#include <atomic>
#include <memory>
#include <type_traits>
#include <vector>

#include "transport/transportpolicy.h"

// Compile-time shape of the spec's policy object (§2): shared read-only, immutable.
static_assert(std::is_const_v<std::remove_reference_t<decltype(*TransportPolicy::current())>>,
              "policies are shared as std::shared_ptr<const TransportPolicy>");
static_assert(!std::is_copy_assignable_v<TransportPolicy>, "TransportPolicy is immutable");
static_assert(!std::is_copy_constructible_v<TransportPolicy>, "TransportPolicy is shared, not copied");

class TestTransportPolicy : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void passthroughModeReported();
    void prepareIsIdentityInPassthrough();
    void defaultCurrentIsPassthroughAndNeverNull();
    void setCurrentSwapsSnapshotHoldersKeepTheirs();
    void setCurrentNullIsIgnored();
    void concurrentReadersAlwaysSeeACompleteSnapshot();
};

void TestTransportPolicy::cleanup()
{
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
}

void TestTransportPolicy::passthroughModeReported()
{
    QVERIFY(TransportPolicy::makePassthrough()->mode() == TransportPolicy::Mode::Passthrough);
}

void TestTransportPolicy::prepareIsIdentityInPassthrough()
{
    const auto policy = TransportPolicy::makePassthrough();

    QNetworkRequest req(QUrl(QStringLiteral("http://localhost/loams_api/student_login.php")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("X-Test", "s1a");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(1234);
    QSslConfiguration ssl;
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    req.setSslConfiguration(ssl);

    const QNetworkRequest out = policy->prepare(req);
    QVERIFY(out == req);                                  // nothing added, removed or changed
    QCOMPARE(out.url(), req.url());                       // http default allowed (no upgrade)
    QCOMPARE(out.rawHeaderList(), req.rawHeaderList());
    QCOMPARE(out.attribute(QNetworkRequest::RedirectPolicyAttribute),
             req.attribute(QNetworkRequest::RedirectPolicyAttribute));
    QVERIFY(out.sslConfiguration() == ssl);               // no TLS change in S1a

    // A request with NO redirect policy keeps none: S1a must not force
    // ManualRedirectPolicy (that enforcement belongs to S1e).
    const QNetworkRequest bare(QUrl(QStringLiteral("http://localhost/loams_api/get_departments.php")));
    QVERIFY(!policy->prepare(bare).attribute(QNetworkRequest::RedirectPolicyAttribute).isValid());
}

void TestTransportPolicy::defaultCurrentIsPassthroughAndNeverNull()
{
    const auto p = TransportPolicy::current();
    QVERIFY(p);
    QVERIFY(p->mode() == TransportPolicy::Mode::Passthrough);
}

void TestTransportPolicy::setCurrentSwapsSnapshotHoldersKeepTheirs()
{
    const auto original = TransportPolicy::current();
    const auto replacement = TransportPolicy::makePassthrough();
    QVERIFY(original.get() != replacement.get());

    TransportPolicy::setCurrent(replacement);
    QCOMPARE(TransportPolicy::current().get(), replacement.get());
    QVERIFY(original);                                    // holder's snapshot stays alive
    QVERIFY(original->mode() == TransportPolicy::Mode::Passthrough);   // and unchanged
}

void TestTransportPolicy::setCurrentNullIsIgnored()
{
    const auto before = TransportPolicy::current();
    QTest::ignoreMessage(QtWarningMsg, "TransportPolicy::setCurrent: null policy ignored");
    TransportPolicy::setCurrent(nullptr);
    QCOMPARE(TransportPolicy::current().get(), before.get());
}

void TestTransportPolicy::concurrentReadersAlwaysSeeACompleteSnapshot()
{
    // QQmlAbstractUrlInterceptor / the QML factory may read the policy from
    // other threads (spec §2): readers racing a writer must always get a
    // complete, non-null snapshot.
    std::atomic<bool> stop{false};
    std::atomic<int> badSeen{0};
    std::atomic<long long> reads{0};
    std::vector<std::unique_ptr<QThread>> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back(QThread::create([&stop, &badSeen, &reads] {
            while (!stop.load()) {
                const auto p = TransportPolicy::current();
                if (!p || p->mode() != TransportPolicy::Mode::Passthrough)
                    ++badSeen;
                ++reads;
            }
        }));
        readers.back()->start();
    }
    QTRY_VERIFY_WITH_TIMEOUT(reads.load() > 100, 5000);
    for (int i = 0; i < 2000; ++i)
        TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
    stop = true;
    for (auto &t : readers)
        QVERIFY(t->wait(5000));
    QCOMPARE(badSeen.load(), 0);
}

QTEST_MAIN(TestTransportPolicy)
#include "tst_transportpolicy.moc"
```

Append to `qt-app/tests/CMakeLists.txt`:

```cmake

# --- S1a transport seam: immutable policy + atomic snapshot (Network; no offscreen) ---
wits_add_qttest(tst_transportpolicy
    SOURCES
        tst_transportpolicy.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.h
    LIBS Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/core)
# S1a GATE G1: std::atomic_load / std::atomic_store on shared_ptr must compile
# warning-free (incl. no -Wdeprecated-declarations) on this MinGW GCC. Scoped to
# the one source that uses them (every test target in this directory that
# compiles it inherits it); Qt's imported include dirs are -isystem, so only
# our code is held to -Werror.
set_source_files_properties(${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.cpp
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
```

- [ ] **Step 2: Run — expected FAIL** (configure/compile error: `core/transport/transportpolicy.cpp` / `transport/transportpolicy.h` do not exist):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_transportpolicy
```

Expected: CMake error `Cannot find source file: .../core/transport/transportpolicy.cpp` (or a compile error for the missing header).

- [ ] **Step 3: Implement.** Create `qt-app/core/transport/transportpolicy.h`:

```cpp
#ifndef TRANSPORTPOLICY_H
#define TRANSPORTPOLICY_H

#include <QNetworkRequest>
#include <memory>

// The one client transport policy (S1 spec §2 "one security policy, not one
// physical manager"). Immutable and shared as std::shared_ptr<const
// TransportPolicy>; the process-wide snapshot is read/written with C++17
// std::atomic_load / std::atomic_store, so the QML factory and (S1e) the URL
// interceptor can read it from any thread.
//
// S1a ships ONLY Mode::Passthrough: prepare() returns the request unchanged —
// the current http default is allowed and no TLS/redirect enforcement is
// applied (spec §3 "S1a seam"). S1e adds the enforcing mode behind this same
// interface and deletes Passthrough; call sites (HttpClient, PolicyEnforcingNam,
// PolicyNamFactory) do not change.
class TransportPolicy
{
public:
    enum class Mode { Passthrough };

    static std::shared_ptr<const TransportPolicy> makePassthrough();

    Mode mode() const { return m_mode; }

    // The per-request hook PolicyEnforcingNam::createRequest() applies to every
    // request in the process. Passthrough: identity.
    QNetworkRequest prepare(const QNetworkRequest &request) const;

    // Process-wide snapshot. Never null: defaults to a Passthrough policy until
    // setCurrent() publishes another one.
    static std::shared_ptr<const TransportPolicy> current();
    // Atomically publishes `policy`. A null policy is ignored (warning), so
    // current() can never become null.
    static void setCurrent(std::shared_ptr<const TransportPolicy> policy);

    TransportPolicy(const TransportPolicy &) = delete;
    TransportPolicy &operator=(const TransportPolicy &) = delete;

private:
    explicit TransportPolicy(Mode mode) : m_mode(mode) {}

    const Mode m_mode;
};

#endif // TRANSPORTPOLICY_H
```

Create `qt-app/core/transport/transportpolicy.cpp`:

```cpp
#include "transport/transportpolicy.h"

#include <QtGlobal>
#include <atomic>
#include <utility>

namespace {
// Function-local static: thread-safe initialisation (C++11 magic statics).
// Only ever accessed through std::atomic_load / std::atomic_store below.
std::shared_ptr<const TransportPolicy> &currentSlot()
{
    static std::shared_ptr<const TransportPolicy> slot = TransportPolicy::makePassthrough();
    return slot;
}
} // namespace

std::shared_ptr<const TransportPolicy> TransportPolicy::makePassthrough()
{
    return std::shared_ptr<const TransportPolicy>(new TransportPolicy(Mode::Passthrough));
}

QNetworkRequest TransportPolicy::prepare(const QNetworkRequest &request) const
{
    switch (m_mode) {
    case Mode::Passthrough:
        return request;
    }
    return request;
}

std::shared_ptr<const TransportPolicy> TransportPolicy::current()
{
    return std::atomic_load(&currentSlot());
}

void TransportPolicy::setCurrent(std::shared_ptr<const TransportPolicy> policy)
{
    if (!policy) {
        qWarning("TransportPolicy::setCurrent: null policy ignored");
        return;
    }
    std::atomic_store(&currentSlot(), std::move(policy));
}
```

In `qt-app/core/CMakeLists.txt`, insert after line 56 (`accesscontrol/contactage.h accesscontrol/contactage.cpp`):

```cmake
    transport/transportpolicy.h transport/transportpolicy.cpp
```

and append after the `set_target_properties(witscore ...)` block (after line 65):

```cmake

# S1a GATE G1 (production copy): the transport policy's atomic shared_ptr
# snapshot compiles warning-free in witscore too. Source-scoped so the rest of
# witscore keeps its current warning level.
set_source_files_properties(transport/transportpolicy.cpp PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
```

- [ ] **Step 4: GATE G1 (MANDATORY) — run, expected PASS with zero warnings:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_transportpolicy witscore 2>&1 | tee C:/b/s1a-gate-g1.log
grep -nE "transportpolicy\.(cpp|h).*(warning|error)" C:/b/s1a-gate-g1.log || echo "G1: no warnings in transportpolicy"
ctest --test-dir C:/b/s1a -R '^tst_transportpolicy$' --output-on-failure
```

Expected: both targets build (any warning in `transportpolicy.cpp` — in either `witscore` or `tst_transportpolicy` — is a hard error under `-Werror`), the `grep` prints `G1: no warnings in transportpolicy`, and `100% tests passed` (7 test functions incl. init/cleanup).

**If GATE G1 fails** (a `-Wdeprecated-declarations` or any other diagnostic on `std::atomic_load` / `std::atomic_store`, a compile error, or a failing `concurrentReadersAlwaysSeeACompleteSnapshot`): **STOP.** Do not start Task 3 or any later task. Do not substitute a workaround (no `QMutex`-guarded pointer, no `std::atomic<std::shared_ptr>`, no warning suppression, no dropping `-Werror`). Record the exact compiler output and escalate to the owner for a design review of spec §2's snapshot mechanism.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `feat(transport): add immutable TransportPolicy with Passthrough mode`.

---

### Task 3: `PolicyEnforcingNam` — the single request choke point (Passthrough, wire-identical)

**Files:**
- Create: `qt-app/core/transport/policyenforcingnam.h`, `qt-app/core/transport/policyenforcingnam.cpp`
- Create: `qt-app/testsupport/tinyhttpserver.h`, `qt-app/testsupport/tinyhttpserver.cpp`
- Modify: `qt-app/core/CMakeLists.txt` (after the Task 2 line)
- Modify: `qt-app/tests/CMakeLists.txt` (append)
- Test: `qt-app/tests/tst_policyenforcingnam.cpp`

**Interfaces:**
- Consumes: `TransportPolicy::prepare`, `TransportPolicy::current` (Task 2).
- Produces:
  - `class PolicyEnforcingNam : public QNetworkAccessManager` (`Q_OBJECT`)
  - `explicit PolicyEnforcingNam(std::shared_ptr<const TransportPolicy> policy, QObject *parent = nullptr);` (null policy → `TransportPolicy::current()`)
  - `std::shared_ptr<const TransportPolicy> PolicyEnforcingNam::policy() const;`
  - `protected: QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override;`
  - Test support `class TinyHttpServer : public QObject` with `struct Response { int status; QByteArray reason; QList<QPair<QByteArray,QByteArray>> headers; QByteArray body; }`, `struct Request { QByteArray method, target, headerBlock, body; }`, `bool listen()`, `quint16 port() const`, `QUrl url(const QString &path) const`, `void route(const QByteArray &path, const Response &)`, `const QList<Request> &requests() const`, `int countFor(const QByteArray &path) const`, `void clear()`.

- [ ] **Step 1: Write the test support server and the failing test.** Create `qt-app/testsupport/tinyhttpserver.h`:

```cpp
#ifndef TINYHTTPSERVER_H
#define TINYHTTPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QTcpServer>
#include <QUrl>

class QTcpSocket;

// Test-only, loopback-only HTTP/1.1 responder: listens on 127.0.0.1 with an
// ephemeral port, so nothing leaves the machine and the real backend is never
// touched. One request per connection ("Connection: close"); exact-path
// routes; any other path answers 404. Records every request verbatim so a test
// can prove two managers put byte-identical requests on the wire.
class TinyHttpServer : public QObject
{
    Q_OBJECT
public:
    struct Response {
        int status = 200;
        QByteArray reason = "OK";
        QList<QPair<QByteArray, QByteArray>> headers;
        QByteArray body;
    };
    struct Request {
        QByteArray method;
        QByteArray target;        // request-target as sent (path + optional query)
        QByteArray headerBlock;   // header lines after the request line, verbatim
        QByteArray body;
    };

    explicit TinyHttpServer(QObject *parent = nullptr);

    bool listen();
    quint16 port() const;
    QUrl url(const QString &path) const;            // http://127.0.0.1:<port><path>
    void route(const QByteArray &path, const Response &response);
    const QList<Request> &requests() const { return m_requests; }
    int countFor(const QByteArray &path) const;     // requests whose path (sans query) == path
    void clear() { m_requests.clear(); }

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket *socket);
    void respond(QTcpSocket *socket, const Request &request);

    QTcpServer m_server;
    QHash<QByteArray, Response> m_routes;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<Request> m_requests;
};

#endif // TINYHTTPSERVER_H
```

Create `qt-app/testsupport/tinyhttpserver.cpp`:

```cpp
#include "tinyhttpserver.h"

#include <QHostAddress>
#include <QTcpSocket>

namespace {
QByteArray pathOf(const QByteArray &target)
{
    const qsizetype q = target.indexOf('?');
    return q < 0 ? target : target.left(q);
}
} // namespace

TinyHttpServer::TinyHttpServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &TinyHttpServer::onNewConnection);
}

bool TinyHttpServer::listen()
{
    return m_server.listen(QHostAddress::LocalHost, 0);
}

quint16 TinyHttpServer::port() const
{
    return m_server.serverPort();
}

QUrl TinyHttpServer::url(const QString &path) const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port()).arg(path));
}

void TinyHttpServer::route(const QByteArray &path, const Response &response)
{
    m_routes.insert(path, response);
}

int TinyHttpServer::countFor(const QByteArray &path) const
{
    int n = 0;
    for (const Request &r : m_requests) {
        if (pathOf(r.target) == path)
            ++n;
    }
    return n;
}

void TinyHttpServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QObject::destroyed, this, [this, socket]() { m_buffers.remove(socket); });
    }
}

void TinyHttpServer::onReadyRead(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;                                         // wait for the full header

    const QByteArray head = buffer.left(headerEnd);
    const qsizetype lineEnd = head.indexOf("\r\n");
    const QByteArray requestLine = lineEnd < 0 ? head : head.left(lineEnd);
    const QByteArray headerBlock = lineEnd < 0 ? QByteArray() : head.mid(lineEnd + 2);

    qsizetype contentLength = 0;
    for (const QByteArray &line : headerBlock.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("content-length:"))
            contentLength = trimmed.mid(15).trimmed().toLongLong();
    }
    const qsizetype bodyStart = headerEnd + 4;
    if (buffer.size() - bodyStart < contentLength)
        return;                                         // wait for the full body

    Request request;
    const QList<QByteArray> parts = requestLine.split(' ');
    request.method = parts.value(0);
    request.target = parts.value(1);
    request.headerBlock = headerBlock;
    request.body = buffer.mid(bodyStart, contentLength);
    m_buffers.remove(socket);                           // `buffer` is dangling from here on
    m_requests.append(request);
    respond(socket, request);
}

void TinyHttpServer::respond(QTcpSocket *socket, const Request &request)
{
    Response response;
    const QByteArray path = pathOf(request.target);
    if (m_routes.contains(path)) {
        response = m_routes.value(path);
    } else {
        response.status = 404;
        response.reason = "Not Found";
    }
    QByteArray out = "HTTP/1.1 " + QByteArray::number(response.status) + ' '
                     + response.reason + "\r\n";
    for (const auto &header : response.headers)
        out += header.first + ": " + header.second + "\r\n";
    out += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    if (request.method != "HEAD")
        out += response.body;
    socket->write(out);
    socket->disconnectFromHost();
}
```

Create `qt-app/tests/tst_policyenforcingnam.cpp`:

```cpp
#include <QtTest>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <memory>

#include "tinyhttpserver.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

// Behaviour-neutrality proof for the S1a choke point (spec §3, §6 Layer 9):
// every request is sent once through a plain QNetworkAccessManager and once
// through a Passthrough PolicyEnforcingNam against the same loopback server,
// and what reaches the wire / comes back must be identical.
class TestPolicyEnforcingNam : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void carriesTheGivenPolicySnapshot();
    void nullPolicyFallsBackToCurrent();
    void wireRequestIdenticalToPlainNam_data();
    void wireRequestIdenticalToPlainNam();
    void multipartUploadIdenticalToPlainNam();
    void replyRequestIdenticalToPlainNam();
    void plainHttpOriginIsAllowed();
    void redirectFollowedExactlyLikePlainNam();
    void callerManualRedirectPolicyPreserved();
    void transportErrorIdenticalToPlainNam();

private:
    enum class Verb { Get, PostForm, PostJson };
    static QNetworkReply *send(QNetworkAccessManager &nam, Verb verb, const QUrl &url);
    static bool waitFor(QNetworkReply *reply);

    TinyHttpServer m_server;
};

QNetworkReply *TestPolicyEnforcingNam::send(QNetworkAccessManager &nam, Verb verb, const QUrl &url)
{
    QNetworkRequest req(url);
    switch (verb) {
    case Verb::Get:
        return nam.get(req);
    case Verb::PostForm:
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
        return nam.post(req, QByteArrayLiteral("school_id=TEST-0001&admin_key=s1a-test-key"));
    case Verb::PostJson:
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        return nam.post(req, QByteArrayLiteral("{\"search\":\"Test Student A\"}"));
    }
    return nullptr;
}

bool TestPolicyEnforcingNam::waitFor(QNetworkReply *reply)
{
    if (reply->isFinished())
        return true;
    QSignalSpy spy(reply, &QNetworkReply::finished);
    return spy.wait(5000);
}

void TestPolicyEnforcingNam::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);   // loopback only, both managers
    QVERIFY(m_server.listen());

    TinyHttpServer::Response json;
    json.headers = {{"Content-Type", "application/json"}};
    json.body = "{\"status\":\"success\"}";
    m_server.route("/loams_api/get_departments.php", json);
    m_server.route("/loams_api/turnstile_display.php", json);
    m_server.route("/loams_api/student_login.php", json);
    m_server.route("/loams_api/api.php/reports/data", json);
    m_server.route("/loams_api/upload_students_zip.php", json);

    TinyHttpServer::Response redirect;
    redirect.status = 302;
    redirect.reason = "Found";
    redirect.headers = {{"Location", "/loams_api/final.php"}};
    m_server.route("/loams_api/redirect.php", redirect);

    TinyHttpServer::Response finalPage;
    finalPage.body = "final";
    m_server.route("/loams_api/final.php", finalPage);
}

void TestPolicyEnforcingNam::init()
{
    m_server.clear();
}

void TestPolicyEnforcingNam::carriesTheGivenPolicySnapshot()
{
    const auto policy = TransportPolicy::makePassthrough();
    PolicyEnforcingNam nam(policy);
    QCOMPARE(nam.policy().get(), policy.get());
}

void TestPolicyEnforcingNam::nullPolicyFallsBackToCurrent()
{
    PolicyEnforcingNam nam(nullptr);
    QVERIFY(nam.policy());
    QCOMPARE(nam.policy().get(), TransportPolicy::current().get());
}

void TestPolicyEnforcingNam::wireRequestIdenticalToPlainNam_data()
{
    QTest::addColumn<int>("verb");
    QTest::addColumn<QString>("path");
    QTest::newRow("GET get_departments")
        << int(Verb::Get) << QStringLiteral("/loams_api/get_departments.php");
    QTest::newRow("GET with query")
        << int(Verb::Get) << QStringLiteral("/loams_api/turnstile_display.php?after=3");
    QTest::newRow("POST form")
        << int(Verb::PostForm) << QStringLiteral("/loams_api/student_login.php");
    QTest::newRow("POST json api.php route")
        << int(Verb::PostJson) << QStringLiteral("/loams_api/api.php/reports/data");
}

void TestPolicyEnforcingNam::wireRequestIdenticalToPlainNam()
{
    QFETCH(int, verb);
    QFETCH(QString, path);
    const QUrl url = m_server.url(path);

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(send(plain, Verb(verb), url));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(send(seam, Verb(verb), url));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(m_server.requests().size(), 2);
    const TinyHttpServer::Request viaPlain = m_server.requests().at(0);
    const TinyHttpServer::Request viaSeam = m_server.requests().at(1);
    QCOMPARE(viaSeam.method, viaPlain.method);
    QCOMPARE(viaSeam.target, viaPlain.target);
    QCOMPARE(viaSeam.headerBlock, viaPlain.headerBlock);
    QCOMPARE(viaSeam.body, viaPlain.body);
    QCOMPARE(b->error(), a->error());
    QCOMPARE(b->attribute(QNetworkRequest::HttpStatusCodeAttribute),
             a->attribute(QNetworkRequest::HttpStatusCodeAttribute));
    QCOMPARE(b->readAll(), a->readAll());
}

void TestPolicyEnforcingNam::multipartUploadIdenticalToPlainNam()
{
    // ImportController / StudentController::registerStudent upload multipart.
    auto upload = [this](QNetworkAccessManager &nam) {
        auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        multi->setBoundary(QByteArrayLiteral("s1a-fixed-boundary"));
        QHttpPart field;
        field.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QStringLiteral("form-data; name=\"admin_key\""));
        field.setBody(QByteArrayLiteral("s1a-test-key"));
        multi->append(field);
        QHttpPart file;
        file.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/zip"));
        file.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"zip\"; filename=\"students.zip\""));
        file.setBody(QByteArrayLiteral("PK synthetic zip bytes"));
        multi->append(file);
        QNetworkReply *reply = nam.post(
            QNetworkRequest(m_server.url(QStringLiteral("/loams_api/upload_students_zip.php"))), multi);
        multi->setParent(reply);
        return reply;
    };

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(upload(plain));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(upload(seam));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(m_server.requests().size(), 2);
    QCOMPARE(m_server.requests().at(1).headerBlock, m_server.requests().at(0).headerBlock);
    QCOMPARE(m_server.requests().at(1).body, m_server.requests().at(0).body);
}

void TestPolicyEnforcingNam::replyRequestIdenticalToPlainNam()
{
    // No attribute / header / redirect-policy / SSL change reaches the
    // QNetworkAccessManager machinery.
    QNetworkRequest req(m_server.url(QStringLiteral("/loams_api/get_departments.php")));
    req.setRawHeader("X-Test", "s1a");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(req));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(req));
    QVERIFY(b->request() == a->request());
    QVERIFY(waitFor(a.get()));
    QVERIFY(waitFor(b.get()));
}

void TestPolicyEnforcingNam::plainHttpOriginIsAllowed()
{
    // Passthrough keeps today's http default (spec §3 "S1a seam").
    const QUrl url = m_server.url(QStringLiteral("/loams_api/get_departments.php"));
    QCOMPARE(url.scheme(), QStringLiteral("http"));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> r(seam.get(QNetworkRequest(url)));
    QVERIFY(waitFor(r.get()));
    QCOMPARE(r->error(), QNetworkReply::NoError);
    QCOMPARE(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
}

void TestPolicyEnforcingNam::redirectFollowedExactlyLikePlainNam()
{
    // Today's Qt 6 default (NoLessSafeRedirectPolicy) follows the 302; S1a must
    // not change that (redirect rejection is S1e).
    const QUrl url = m_server.url(QStringLiteral("/loams_api/redirect.php"));
    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(QNetworkRequest(url)));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(QNetworkRequest(url)));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(a->url().path(), QStringLiteral("/loams_api/final.php"));   // documents today
    QCOMPARE(b->url(), a->url());
    QCOMPARE(b->attribute(QNetworkRequest::HttpStatusCodeAttribute),
             a->attribute(QNetworkRequest::HttpStatusCodeAttribute));
    QCOMPARE(b->readAll(), a->readAll());
    QCOMPARE(m_server.countFor("/loams_api/final.php"), 2);
}

void TestPolicyEnforcingNam::callerManualRedirectPolicyPreserved()
{
    QNetworkRequest req(m_server.url(QStringLiteral("/loams_api/redirect.php")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(req));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(req));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(a->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 302);
    QCOMPARE(b->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 302);
    QCOMPARE(m_server.countFor("/loams_api/final.php"), 0);
}

void TestPolicyEnforcingNam::transportErrorIdenticalToPlainNam()
{
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 closedPort = probe.serverPort();
    probe.close();
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/loams_api/get_departments.php").arg(closedPort));

    QNetworkAccessManager plain;
    std::unique_ptr<QNetworkReply> a(plain.get(QNetworkRequest(url)));
    QVERIFY(waitFor(a.get()));
    PolicyEnforcingNam seam(TransportPolicy::makePassthrough());
    std::unique_ptr<QNetworkReply> b(seam.get(QNetworkRequest(url)));
    QVERIFY(waitFor(b.get()));

    QCOMPARE(a->error(), QNetworkReply::ConnectionRefusedError);
    QCOMPARE(b->error(), a->error());
}

QTEST_MAIN(TestPolicyEnforcingNam)
#include "tst_policyenforcingnam.moc"
```

Append to `qt-app/tests/CMakeLists.txt`:

```cmake

# --- S1a transport seam: PolicyEnforcingNam wire-level parity vs a plain
# QNetworkAccessManager against the in-process loopback TinyHttpServer
# (Network; no offscreen; no external network) ---
wits_add_qttest(tst_policyenforcingnam
    SOURCES
        tst_policyenforcingnam.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/policyenforcingnam.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/policyenforcingnam.h
        ${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.h
        ${CMAKE_SOURCE_DIR}/testsupport/tinyhttpserver.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/tinyhttpserver.h
    LIBS Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
```

- [ ] **Step 2: Run — expected FAIL** (missing `core/transport/policyenforcingnam.*`):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_policyenforcingnam
```

Expected: `Cannot find source file: .../core/transport/policyenforcingnam.cpp`.

- [ ] **Step 3: Implement.** Create `qt-app/core/transport/policyenforcingnam.h`:

```cpp
#ifndef POLICYENFORCINGNAM_H
#define POLICYENFORCINGNAM_H

#include <QNetworkAccessManager>
#include <memory>

#include "transport/transportpolicy.h"

// The single choke point for every request in the process (S1 spec §3
// "PolicyEnforcingNam"). Both manager kinds — HttpClient's controller-facing
// manager and every QML-engine manager from PolicyNamFactory — are instances
// of this class. createRequest() runs each request through the policy before
// Qt's own pipeline.
//
// S1a: the policy is Passthrough, so the request is forwarded unchanged and
// behaviour is identical to a plain QNetworkAccessManager (proved on the wire
// by tst_policyenforcingnam). S1e adds rejection (error reply without touching
// the network), forced ManualRedirectPolicy and per-request QSslConfiguration
// here, without changing any caller.
class PolicyEnforcingNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    // A null policy falls back to TransportPolicy::current(), so a manager is
    // never policy-less.
    explicit PolicyEnforcingNam(std::shared_ptr<const TransportPolicy> policy,
                                QObject *parent = nullptr);

    std::shared_ptr<const TransportPolicy> policy() const { return m_policy; }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    const std::shared_ptr<const TransportPolicy> m_policy;
};

#endif // POLICYENFORCINGNAM_H
```

Create `qt-app/core/transport/policyenforcingnam.cpp`:

```cpp
#include "transport/policyenforcingnam.h"

#include <utility>

PolicyEnforcingNam::PolicyEnforcingNam(std::shared_ptr<const TransportPolicy> policy,
                                       QObject *parent)
    : QNetworkAccessManager(parent)
    , m_policy(policy ? std::move(policy) : TransportPolicy::current())
{
}

QNetworkReply *PolicyEnforcingNam::createRequest(Operation op, const QNetworkRequest &request,
                                                 QIODevice *outgoingData)
{
    return QNetworkAccessManager::createRequest(op, m_policy->prepare(request), outgoingData);
}
```

In `qt-app/core/CMakeLists.txt`, after the Task 2 line add:

```cmake
    transport/policyenforcingnam.h transport/policyenforcingnam.cpp
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_policyenforcingnam witscore
ctest --test-dir C:/b/s1a -R '^tst_policyenforcingnam$' --output-on-failure
```

Expected: `100% tests passed`. If `headerBlock` differs, do **not** loosen the comparison — the seam is altering the request; fix the seam.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `feat(transport): add PolicyEnforcingNam request choke point (Passthrough)`.

---

### Task 4: `HttpClient` — owns the controller-facing manager, accepts an injected one

**Files:**
- Create: `qt-app/core/transport/httpclient.h`, `qt-app/core/transport/httpclient.cpp`
- Modify: `qt-app/core/CMakeLists.txt` (after the Task 3 line)
- Modify: `qt-app/tests/CMakeLists.txt` (append)
- Test: `qt-app/tests/tst_httpclient.cpp`

**Interfaces:**
- Consumes: `PolicyEnforcingNam(std::shared_ptr<const TransportPolicy>, QObject*)`, `TransportPolicy::current()`; test double `CapturingNam` (`qt-app/testsupport/capturingnam.h`).
- Produces:
  - `class HttpClient : public QObject` (`Q_OBJECT`)
  - `explicit HttpClient(QObject *parent = nullptr, QNetworkAccessManager *injected = nullptr);` — same argument order as the existing ViewModel injection idiom `(QObject *parent, X *injected)`
  - `QNetworkAccessManager *HttpClient::manager() const;`
  - `std::shared_ptr<const TransportPolicy> HttpClient::policy() const;` (snapshot taken at construction)
  - `bool HttpClient::ownsManager() const;`

- [ ] **Step 1: Write the failing test.** Create `qt-app/tests/tst_httpclient.cpp`:

```cpp
#include <QtTest>
#include <QNetworkReply>
#include <QPointer>
#include <memory>

#include "capturingnam.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

class TestHttpClient : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void defaultOwnsAPolicyEnforcingNam();
    void defaultManagerCarriesCurrentPolicy();
    void eachDefaultClientHasItsOwnManager();
    void ownedManagerDiesWithClient();
    void injectedManagerIsUsedNotOwned();
    void requestsThroughInjectedManagerReachIt();
    void policySnapshotTakenAtConstruction();
};

void TestHttpClient::cleanup()
{
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
}

void TestHttpClient::defaultOwnsAPolicyEnforcingNam()
{
    HttpClient http;
    auto *nam = qobject_cast<PolicyEnforcingNam *>(http.manager());
    QVERIFY(nam);
    QCOMPARE(nam->parent(), static_cast<QObject *>(&http));
    QVERIFY(http.ownsManager());
}

void TestHttpClient::defaultManagerCarriesCurrentPolicy()
{
    const auto policy = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(policy);
    HttpClient http;
    QCOMPARE(http.policy().get(), policy.get());
    auto *nam = qobject_cast<PolicyEnforcingNam *>(http.manager());
    QVERIFY(nam);
    QCOMPARE(nam->policy().get(), policy.get());
}

void TestHttpClient::eachDefaultClientHasItsOwnManager()
{
    // One security policy, not one physical manager (spec §2): owners keep
    // today's one-manager-per-owner topology.
    HttpClient a;
    HttpClient b;
    QVERIFY(a.manager() != b.manager());
    QCOMPARE(a.policy().get(), b.policy().get());
}

void TestHttpClient::ownedManagerDiesWithClient()
{
    QPointer<QNetworkAccessManager> nam;
    {
        HttpClient http;
        nam = http.manager();
        QVERIFY(nam);
    }
    QVERIFY(nam.isNull());
}

void TestHttpClient::injectedManagerIsUsedNotOwned()
{
    CapturingNam capturing;
    QPointer<QNetworkAccessManager> guard(&capturing);
    {
        HttpClient http(nullptr, &capturing);
        QCOMPARE(http.manager(), static_cast<QNetworkAccessManager *>(&capturing));
        QVERIFY(!http.ownsManager());
        QVERIFY(capturing.parent() == nullptr);                       // not reparented
        QVERIFY(http.findChildren<PolicyEnforcingNam *>().isEmpty());  // no stray owned manager
    }
    QVERIFY(!guard.isNull());                                         // client left it alone
}

void TestHttpClient::requestsThroughInjectedManagerReachIt()
{
    CapturingNam capturing;
    HttpClient http(nullptr, &capturing);
    const QNetworkRequest req(QUrl(QStringLiteral("http://localhost/loams_api/get_departments.php")));
    std::unique_ptr<QNetworkReply> reply(http.manager()->get(req));
    QCOMPARE(capturing.lastOp, QNetworkAccessManager::GetOperation);
    QCOMPARE(capturing.lastUrl, req.url());
}

void TestHttpClient::policySnapshotTakenAtConstruction()
{
    const auto first = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(first);
    HttpClient early;
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
    HttpClient late;
    QCOMPARE(early.policy().get(), first.get());
    QVERIFY(late.policy().get() != first.get());
}

QTEST_MAIN(TestHttpClient)
#include "tst_httpclient.moc"
```

Append to `qt-app/tests/CMakeLists.txt`:

```cmake

# --- S1a transport seam: HttpClient ownership / injection (Network; no offscreen) ---
wits_add_qttest(tst_httpclient
    SOURCES
        tst_httpclient.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/httpclient.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/httpclient.h
        ${CMAKE_SOURCE_DIR}/core/transport/policyenforcingnam.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/policyenforcingnam.h
        ${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.cpp
        ${CMAKE_SOURCE_DIR}/core/transport/transportpolicy.h
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
    LIBS Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
```

- [ ] **Step 2: Run — expected FAIL** (missing `core/transport/httpclient.*`):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_httpclient
```

Expected: `Cannot find source file: .../core/transport/httpclient.cpp`.

- [ ] **Step 3: Implement.** Create `qt-app/core/transport/httpclient.h`:

```cpp
#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <memory>

#include "transport/transportpolicy.h"

class QNetworkAccessManager;

// The controller-facing side of the S1 transport seam (spec §2, §3 "Injection
// boundary"). Every C++ owner that used to build its own QNetworkAccessManager
// (ViewModels, AccessControlHub) now builds — or is injected with — an
// HttpClient and hands manager() to its controllers / HttpForm calls.
//
// Production (injected == nullptr): owns one PolicyEnforcingNam as a QObject
// child, carrying the TransportPolicy snapshot current at construction.
// Tests: pass a CapturingNam / SequencedNam as `injected`; it is used as-is,
// never reparented or deleted (the test owns it and must outlive the client).
// Argument order mirrors the ViewModel idiom (parent first): HttpClient(&nam)
// would make `nam` the PARENT, not the injected manager.
class HttpClient : public QObject
{
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr, QNetworkAccessManager *injected = nullptr);

    QNetworkAccessManager *manager() const { return m_manager; }
    std::shared_ptr<const TransportPolicy> policy() const { return m_policy; }
    bool ownsManager() const { return m_ownsManager; }

private:
    const std::shared_ptr<const TransportPolicy> m_policy;
    QNetworkAccessManager *m_manager = nullptr;   // owned child, or injected (not owned)
    bool m_ownsManager = false;
};

#endif // HTTPCLIENT_H
```

Create `qt-app/core/transport/httpclient.cpp`:

```cpp
#include "transport/httpclient.h"

#include "transport/policyenforcingnam.h"

HttpClient::HttpClient(QObject *parent, QNetworkAccessManager *injected)
    : QObject(parent)
    , m_policy(TransportPolicy::current())
{
    if (injected) {
        m_manager = injected;
        m_ownsManager = false;
    } else {
        m_manager = new PolicyEnforcingNam(m_policy, this);
        m_ownsManager = true;
    }
}
```

In `qt-app/core/CMakeLists.txt`, after the Task 3 line add:

```cmake
    transport/httpclient.h transport/httpclient.cpp
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_httpclient witscore
ctest --test-dir C:/b/s1a -R '^tst_httpclient$' --output-on-failure
```

Expected: `100% tests passed`.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `feat(transport): add HttpClient owning the controller-facing manager`.

---

### Task 4b: Core network classes — explicit non-null manager contract + `networkManager()` accessor

**Files:**
- Modify (header: contract comment above the ctor + one inline accessor; .cpp: one `Q_ASSERT_X` in the ctor body):
  - `qt-app/core/studentcontroller.h` (line 17) / `.cpp` (lines 18-21)
  - `qt-app/core/visitorcontroller.h` (line 16) / `.cpp` (lines 18-21)
  - `qt-app/core/reportcontroller.h` (line 21) / `.cpp` (lines 15-18)
  - `qt-app/core/importcontroller.h` (line 16) / `.cpp` (lines 20-23)
  - `qt-app/core/brandingcontroller.h` (line 18) / `.cpp` (lines 13-16)
  - `qt-app/core/accesscontrol/accessdecisionservice.h` (line 29) / `.cpp` (lines 16-19)
  - `qt-app/core/accesscontrol/turnstileprovider.h` (lines 23-24) / `.cpp` (lines 20-27, ctor body)
- Modify tests: `qt-app/tests/tst_studentcontroller.cpp`, `tst_visitorcontroller.cpp` (+ lines 259, 300), `tst_reportcontroller.cpp`, `tst_importcontroller.cpp` (+ lines 486, 513, 524), `tst_brandingcontroller.cpp`, `tst_accessdecisionservice.cpp`, `tst_turnstileprovider.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces (identical on all seven classes): constructor precondition `nam != nullptr` (`Q_ASSERT_X`); `QNetworkAccessManager *networkManager() const;` returning the injected pointer unchanged. Constructor signatures are **unchanged**, so the frozen Widgets sources (which always pass their non-null `networkManager`) keep compiling — no Widgets incompatibility is introduced.

- [ ] **Step 1: Write the failing tests.**

Make the five network-free tests honour the new contract (they pass `nullptr` today):

```bash
sed -i 's/^\(\s*\)ImportController controller(nullptr);\(.*\)$/\1QNetworkAccessManager unusedNam;   \/\/ contract: non-null manager (this path never uses it)\n\1ImportController controller(\&unusedNam);/' qt-app/tests/tst_importcontroller.cpp
sed -i 's/^\(\s*\)VisitorController controller(nullptr);\(.*\)$/\1QNetworkAccessManager unusedNam;   \/\/ contract: non-null manager (this path never uses it)\n\1VisitorController controller(\&unusedNam);/' qt-app/tests/tst_visitorcontroller.cpp
grep -c "controller(&unusedNam);" qt-app/tests/tst_importcontroller.cpp qt-app/tests/tst_visitorcontroller.cpp   # expect 3 and 2
grep -rnE "(Controller|Service|Provider)\s+\w+\(nullptr" qt-app/tests qt-app/quick/tests   # expect: no output
```

In `qt-app/tests/tst_visitorcontroller.cpp` add after line 5 (before `#include "visitorcontroller.h"`): `#include <QNetworkAccessManager>`. In `qt-app/tests/tst_brandingcontroller.cpp` add after line 2: `#include <QNetworkAccessManager>`. In `qt-app/tests/tst_turnstileprovider.cpp` add after line 3: `#include <QUrl>`.

Add one slot `void networkManagerIsTheInjectedOne();` to each test class's `private slots:` list and the matching function before its `QTEST_*MAIN` line:

`tst_studentcontroller.cpp`:

```cpp
void TestStudentController::networkManagerIsTheInjectedOne()
{
    CapturingNam nam;
    StudentController controller(&nam);
    QCOMPARE(controller.networkManager(), static_cast<QNetworkAccessManager *>(&nam));
}
```

`tst_visitorcontroller.cpp`:

```cpp
void TestVisitorController::networkManagerIsTheInjectedOne()
{
    QNetworkAccessManager nam;
    VisitorController controller(&nam);
    QCOMPARE(controller.networkManager(), &nam);
}
```

`tst_reportcontroller.cpp`:

```cpp
void TstReportController::networkManagerIsTheInjectedOne()
{
    CapturingNam nam;
    ReportController controller(&nam);
    QCOMPARE(controller.networkManager(), static_cast<QNetworkAccessManager *>(&nam));
}
```

`tst_importcontroller.cpp`:

```cpp
void TestImportController::networkManagerIsTheInjectedOne()
{
    CapturingNam nam;
    ImportController controller(&nam);
    QCOMPARE(controller.networkManager(), static_cast<QNetworkAccessManager *>(&nam));
}
```

`tst_brandingcontroller.cpp`:

```cpp
void TestBrandingController::networkManagerIsTheInjectedOne()
{
    QNetworkAccessManager nam;
    BrandingController controller(&nam);
    QCOMPARE(controller.networkManager(), &nam);
}
```

`tst_accessdecisionservice.cpp`:

```cpp
void TestAccessDecisionService::networkManagerIsTheInjectedOne()
{
    CapturingNam nam;
    AccessControl::AccessDecisionService service(&nam);
    QCOMPARE(service.networkManager(), static_cast<QNetworkAccessManager *>(&nam));
}
```

`tst_turnstileprovider.cpp`:

```cpp
void TestTurnstileProvider::networkManagerIsTheInjectedOne()
{
    SequencedNam nam;
    AccessControl::TurnstileProvider provider(&nam, QUrl(QStringLiteral("http://localhost/loams_api/")),
                                              QVariantMap{});
    QCOMPARE(provider.networkManager(), static_cast<QNetworkAccessManager *>(&nam));
}
```

(If a test file already has `using namespace AccessControl;`, the `AccessControl::` qualifier is still valid.)

- [ ] **Step 2: Run — expected FAIL** (compile: `'class StudentController' has no member named 'networkManager'`, and likewise for the other six):

```bash
cmake --build C:/b/s1a --target tst_studentcontroller tst_visitorcontroller tst_reportcontroller tst_importcontroller tst_brandingcontroller tst_accessdecisionservice tst_turnstileprovider
```

- [ ] **Step 3: Implement the contract identically on all seven classes.** In each header, replace the constructor declaration with the same declaration preceded by the contract comment, and add the accessor directly below it:

`qt-app/core/studentcontroller.h` line 17 →

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owning ViewModel's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam (CapturingNam / SequencedNam). This class
    // never creates a manager itself.
    explicit StudentController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

`qt-app/core/visitorcontroller.h` line 16, `qt-app/core/importcontroller.h` line 16, `qt-app/core/reportcontroller.h` line 21, `qt-app/core/brandingcontroller.h` line 18 and `qt-app/core/accesscontrol/accessdecisionservice.h` line 29 → the same two blocks with the class name substituted:

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owner's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    explicit VisitorController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owner's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    explicit ImportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owner's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    explicit ReportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owner's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    explicit BrandingController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is the owner's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    explicit AccessDecisionService(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

`qt-app/core/accesscontrol/turnstileprovider.h` lines 23-24 →

```cpp
    // Manager contract (S1 transport seam): `nam` must be non-null and must
    // outlive this object. In LOAMS 2.0 production it is AccessControlHub's
    // HttpClient::manager() (a PolicyEnforcingNam); a raw QNetworkAccessManager
    // is accepted only as a test seam. This class never creates a manager itself.
    TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                      const QVariantMap &config, QObject *parent = nullptr);

    // The injected manager, unchanged — lets tests prove production wiring.
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

In each `.cpp`, replace the empty constructor body `{}` (or, for `TurnstileProvider`, insert as the **first** statement of the existing body) with the assertion; e.g. `qt-app/core/studentcontroller.cpp` lines 18-21 become:

```cpp
StudentController::StudentController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{
    Q_ASSERT_X(nam, "StudentController", "null QNetworkAccessManager: pass the owner's HttpClient::manager()");
}
```

and identically (class name substituted in both the signature context and the first `Q_ASSERT_X` argument) for `VisitorController` (`visitorcontroller.cpp:18-21`), `ReportController` (`reportcontroller.cpp:15-18`), `ImportController` (`importcontroller.cpp:20-23`), `BrandingController` (`brandingcontroller.cpp:13-16`) and `AccessDecisionService` (`accessdecisionservice.cpp:16-19`). The statement to insert in each is exactly:

```cpp
    Q_ASSERT_X(nam, "<ClassName>", "null QNetworkAccessManager: pass the owner's HttpClient::manager()");
```

with `<ClassName>` ∈ {`VisitorController`, `ReportController`, `ImportController`, `BrandingController`, `AccessDecisionService`, `TurnstileProvider`}. For `TurnstileProvider` (`turnstileprovider.cpp`, body starts after the initializer list ending at `m_pollTimer(new QTimer(this))`), insert the `Q_ASSERT_X(nam, "TurnstileProvider", ...)` line as the first line inside its `{`.

- [ ] **Step 4: Run — expected PASS** (core suites and everything that constructs these classes):

```bash
cmake --build C:/b/s1a --target witscore tst_studentcontroller tst_visitorcontroller tst_reportcontroller tst_importcontroller tst_brandingcontroller tst_accessdecisionservice tst_turnstileprovider tst_accesscontrolservice tst_accesscontrolhub tst_databaseviewmodel tst_searchviewmodel tst_importviewmodel tst_reportingviewmodel
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_studentcontroller|tst_visitorcontroller|tst_reportcontroller|tst_importcontroller|tst_brandingcontroller|tst_accessdecisionservice|tst_turnstileprovider|tst_accesscontrolservice|tst_accesscontrolhub|tst_databaseviewmodel|tst_searchviewmodel|tst_importviewmodel|tst_reportingviewmodel)$' --output-on-failure -j 8
```

Expected: all pass (no test anywhere constructs these classes with `nullptr` any more, so no assertion fires). Legacy check (developer config, since `adminwindow.cpp`/`mainwindow.cpp` construct these classes): `cmake --build C:/b/s1a-legacy --target WITS` still builds — signatures are unchanged.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `refactor(core): require a non-null injected manager in network classes`.

---

### Task 5: `PolicyNamFactory` — new `PolicyEnforcingNam` per QML `create()` call, any thread

**Files:**
- Create: `qt-app/quick/PolicyNamFactory.h`, `qt-app/quick/PolicyNamFactory.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (module `SOURCES`, after line 82 `HttpForm.h HttpForm.cpp`; add a test registration after the `tst_themeviewmodel` block, line 184)
- Test: `qt-app/quick/tests/tst_policynamfactory.cpp`

**Interfaces:**
- Consumes: `PolicyEnforcingNam`, `TransportPolicy::current()`.
- Produces:
  - `class PolicyNamFactory : public QQmlNetworkAccessManagerFactory`
  - `QNetworkAccessManager *PolicyNamFactory::create(QObject *parent) override;` — a **new** `PolicyEnforcingNam(TransportPolicy::current(), parent)` per call; thread-safe
  - `int PolicyNamFactory::createdCount() const;` (atomic diagnostic counter)
  - `static bool PolicyNamFactory::installOn(QQmlEngine &engine, PolicyNamFactory &factory);` — installs `factory` only if the engine has no factory yet, has not created its own manager yet, and (for a `QQmlApplicationEngine`) has no root objects; otherwise logs `qCritical` and returns `false` without installing. Guarantee is limited to those three detectable states; it cannot see components compiled on a plain `QQmlEngine` without a manager — the production ordering rule relies on the fresh engine + `quickMainInstallsTransportBeforeLoad` (Task 7) for that

- [ ] **Step 1: Write the failing test.** Create `qt-app/quick/tests/tst_policynamfactory.cpp`:

```cpp
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QThread>
#include <atomic>
#include <memory>
#include <vector>

#include "PolicyNamFactory.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

class TestPolicyNamFactory : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void createReturnsNewParentedPolicyNam();
    void createUsesPolicySnapshotAtCallTime();
    void createIsSafeFromManyThreads();
    void engineManagerComesFromFactory();
    void installOnFreshEngineSucceeds();
    void installRefusedAfterEngineCreatedAManager();
    void installRefusedAfterQmlLoaded();
    void installRefusedWhenAnotherFactoryInstalled();
};

void TestPolicyNamFactory::cleanup()
{
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
}

void TestPolicyNamFactory::createReturnsNewParentedPolicyNam()
{
    PolicyNamFactory factory;
    QObject owner;
    QNetworkAccessManager *a = factory.create(&owner);
    QNetworkAccessManager *b = factory.create(&owner);
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(a));
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(b));
    QVERIFY(a != b);                                   // a NEW manager per call (spec §2)
    QCOMPARE(a->parent(), &owner);                     // QML-owned via the given parent
    QCOMPARE(b->parent(), &owner);
    QCOMPARE(factory.createdCount(), 2);
}

void TestPolicyNamFactory::createUsesPolicySnapshotAtCallTime()
{
    PolicyNamFactory factory;
    QObject owner;
    const auto p1 = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(p1);
    auto *n1 = qobject_cast<PolicyEnforcingNam *>(factory.create(&owner));
    const auto p2 = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(p2);
    auto *n2 = qobject_cast<PolicyEnforcingNam *>(factory.create(&owner));
    QVERIFY(n1 && n2);
    QCOMPARE(n1->policy().get(), p1.get());
    QCOMPARE(n2->policy().get(), p2.get());
}

void TestPolicyNamFactory::createIsSafeFromManyThreads()
{
    // QQmlNetworkAccessManagerFactory::create() is called from the engine's
    // pixmap-reader / loader threads (spec §2).
    PolicyNamFactory factory;
    const auto policy = TransportPolicy::makePassthrough();
    TransportPolicy::setCurrent(policy);
    constexpr int kThreads = 8;
    std::atomic<int> ok{0};
    std::vector<std::unique_ptr<QThread>> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back(QThread::create([&factory, &ok, policy] {
            QObject owner;   // lives on this worker; deletes the manager on this thread
            auto *nam = qobject_cast<PolicyEnforcingNam *>(factory.create(&owner));
            if (nam && nam->parent() == &owner && nam->thread() == QThread::currentThread()
                && nam->policy().get() == policy.get())
                ++ok;
        }));
    }
    for (auto &t : threads)
        t->start();
    for (auto &t : threads)
        QVERIFY(t->wait(10000));
    QCOMPARE(ok.load(), kThreads);
    QCOMPARE(factory.createdCount(), kThreads);
}

void TestPolicyNamFactory::engineManagerComesFromFactory()
{
    PolicyNamFactory factory;                          // declared first: outlives the engine
    QQmlEngine engine;
    QVERIFY(PolicyNamFactory::installOn(engine, factory));
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(engine.networkAccessManager()));
    QVERIFY(factory.createdCount() >= 1);
}

// --- Ordering rule (spec §2/§3 + S1a approval condition): the factory must be
// installed before any QML is loaded and before any manager/request exists. ---

void TestPolicyNamFactory::installOnFreshEngineSucceeds()
{
    PolicyNamFactory factory;
    QQmlEngine engine;
    QVERIFY(PolicyNamFactory::installOn(engine, factory));
    QCOMPARE(engine.networkAccessManagerFactory(),
             static_cast<QQmlNetworkAccessManagerFactory *>(&factory));
}

void TestPolicyNamFactory::installRefusedAfterEngineCreatedAManager()
{
    PolicyNamFactory factory;
    QQmlEngine engine;
    QNetworkAccessManager *early = engine.networkAccessManager();   // default, unprotected
    QVERIFY(early);
    QVERIFY(!qobject_cast<PolicyEnforcingNam *>(early));
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("PolicyNamFactory::installOn")));
    QVERIFY(!PolicyNamFactory::installOn(engine, factory));
    QVERIFY(engine.networkAccessManagerFactory() == nullptr);
    QCOMPARE(factory.createdCount(), 0);
}

void TestPolicyNamFactory::installRefusedAfterQmlLoaded()
{
    PolicyNamFactory factory;
    QQmlApplicationEngine engine;
    engine.loadData(QByteArrayLiteral("import QtQml\nQtObject {}"));
    QCOMPARE(engine.rootObjects().size(), 1);
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("PolicyNamFactory::installOn")));
    QVERIFY(!PolicyNamFactory::installOn(engine, factory));
    QVERIFY(engine.networkAccessManagerFactory() == nullptr);
}

void TestPolicyNamFactory::installRefusedWhenAnotherFactoryInstalled()
{
    PolicyNamFactory first;
    PolicyNamFactory second;
    QQmlEngine engine;
    QVERIFY(PolicyNamFactory::installOn(engine, first));
    QTest::ignoreMessage(QtCriticalMsg, QRegularExpression(QStringLiteral("PolicyNamFactory::installOn")));
    QVERIFY(!PolicyNamFactory::installOn(engine, second));
    QCOMPARE(engine.networkAccessManagerFactory(),
             static_cast<QQmlNetworkAccessManagerFactory *>(&first));
}

QTEST_MAIN(TestPolicyNamFactory)
#include "tst_policynamfactory.moc"
```

In `qt-app/quick/CMakeLists.txt`, after the `tst_themeviewmodel` registration (ends line 184) add:

```cmake

# --- S1a transport seam: QML engine manager factory (C++ QtTest, offscreen:
# witsquickmodule PUBLIC-propagates Qt::Gui) ---
wits_add_qttest(tst_policynamfactory
    SOURCES tests/tst_policynamfactory.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Network
    OFFSCREEN)
```

- [ ] **Step 2: Run — expected FAIL** (`PolicyNamFactory.h: No such file or directory`):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_policynamfactory
```

- [ ] **Step 3: Implement.** Create `qt-app/quick/PolicyNamFactory.h`:

```cpp
#ifndef POLICYNAMFACTORY_H
#define POLICYNAMFACTORY_H

#include <QQmlNetworkAccessManagerFactory>
#include <atomic>

class QQmlEngine;

// QML-engine side of the S1 transport seam (spec §2). Installed on the engine
// before the first QML load (quick/main.cpp, QuickTestSetup.h). Qt calls
// create() for every manager the engine needs — including from its
// image-reader / loader threads — and each call returns a NEW
// PolicyEnforcingNam owned by `parent`, built with the TransportPolicy
// snapshot current at that instant (atomic load). Not a QML type.
// The factory must outlive every engine it is installed on.
class PolicyNamFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;

    // Diagnostic: how many managers this factory has created (thread-safe).
    int createdCount() const { return m_created.load(); }

    // The only sanctioned way to install the factory. Refuses (qCritical +
    // false, nothing installed) in exactly three detectable "too late" states:
    // the engine already has a factory; it has already created its own
    // manager (QQmlEngine parents that manager to itself); or it is a
    // QQmlApplicationEngine that already has root objects. It cannot detect
    // components compiled on a plain QQmlEngine/QQuickView that never created
    // a manager — callers must install on a freshly constructed engine before
    // any load (quick/main.cpp is pinned by a source-order guard test).
    // Callers treat false as a fatal startup error.
    static bool installOn(QQmlEngine &engine, PolicyNamFactory &factory);

private:
    std::atomic<int> m_created{0};
};

#endif // POLICYNAMFACTORY_H
```

Create `qt-app/quick/PolicyNamFactory.cpp`:

```cpp
#include "PolicyNamFactory.h"

#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlEngine>

#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

QNetworkAccessManager *PolicyNamFactory::create(QObject *parent)
{
    m_created.fetch_add(1);
    return new PolicyEnforcingNam(TransportPolicy::current(), parent);
}

bool PolicyNamFactory::installOn(QQmlEngine &engine, PolicyNamFactory &factory)
{
    if (engine.networkAccessManagerFactory()) {
        qCritical("PolicyNamFactory::installOn: engine already has a network factory");
        return false;
    }
    if (!engine.findChildren<QNetworkAccessManager *>(Qt::FindDirectChildrenOnly).isEmpty()) {
        qCritical("PolicyNamFactory::installOn: engine already created a network manager");
        return false;
    }
    if (auto *app = qobject_cast<QQmlApplicationEngine *>(&engine);
        app && !app->rootObjects().isEmpty()) {
        qCritical("PolicyNamFactory::installOn: QML already loaded");
        return false;
    }
    engine.setNetworkAccessManagerFactory(&factory);
    return true;
}
```

In `qt-app/quick/CMakeLists.txt` module `SOURCES`, after line 82 (`HttpForm.h HttpForm.cpp`) add:

```cmake
        PolicyNamFactory.h PolicyNamFactory.cpp
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_policynamfactory
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^tst_policynamfactory$' --output-on-failure
```

Expected: `100% tests passed` (9 functions). `installRefusedAfterEngineCreatedAManager` is part of the mandatory ordering gate: it relies on Qt 6.11.1 parenting the engine's own manager to the engine. **If it fails, STOP** — do not continue to Task 5b or later and do not weaken `installOn()`; escalate to the owner for a design review of how the factory-before-any-request rule is enforced.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `feat(quick): add PolicyNamFactory for QML engine networking`.

---

### Task 5b: GATE G2 (MANDATORY) — Qt image and Canvas loads use the installed factory's manager

This gate verifies, on Qt 6.11.1, the Qt behaviour the whole QML half of the seam relies on: an `Image` (and `Canvas.loadImage`, used by `LCircleImage`) fetching an `http(s)` URL does so through a manager obtained from the engine's installed `QQmlNetworkAccessManagerFactory` (spec §2: "`create()` … may be called from multiple threads"). Tasks 6-14 depend on it.

**Files:**
- Create: `qt-app/quick/tests/RecordingPolicyNam.h` (header-only test support, reused by Task 13)
- Create: `qt-app/quick/tests/tst_qmlfactorygate.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (register after the `tst_policynamfactory` block added in Task 5)
- Test: `tst_qmlfactorygate`

**Interfaces:**
- Consumes: `PolicyEnforcingNam` (its protected `createRequest` override point), `PolicyNamFactory` (virtual `create`, `installOn`), `TinyHttpServer` (Task 3).
- Produces: test-only `struct RequestLog { bool contains(const QUrl &) const; QThread *threadFor(const QUrl &) const; }`, `class RecordingPolicyNam : public PolicyEnforcingNam`, `class RecordingPolicyNamFactory : public PolicyNamFactory` (`explicit RecordingPolicyNamFactory(RequestLog *log)`); CTest `tst_qmlfactorygate`; a recorded answer (in the test log) to whether the image request ran on a non-GUI thread.

- [ ] **Step 1: Write the gate test.** Create `qt-app/quick/tests/RecordingPolicyNam.h`:

```cpp
#ifndef RECORDINGPOLICYNAM_H
#define RECORDINGPOLICYNAM_H

#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QThread>
#include <QUrl>

#include "PolicyNamFactory.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

// Test-only proof that a request really went through a manager made by the
// installed factory. RecordingPolicyNamFactory is a PolicyNamFactory whose
// managers are PolicyEnforcingNams that log each request (URL + issuing
// thread) and then hand it to the real PolicyEnforcingNam pipeline. Counting
// create() calls is NOT proof (an engine may reuse a manager), so tests assert
// on this log instead. Thread-safe: managers may live on loader threads.
struct RequestLog
{
    mutable QMutex mutex;
    QList<QUrl> urls;
    QList<QThread *> threads;

    bool contains(const QUrl &url) const
    {
        QMutexLocker lock(&mutex);
        return urls.contains(url);
    }
    // Thread that issued the first request for `url`, or nullptr if none.
    QThread *threadFor(const QUrl &url) const
    {
        QMutexLocker lock(&mutex);
        const qsizetype i = urls.indexOf(url);
        return i < 0 ? nullptr : threads.at(i);
    }
};

class RecordingPolicyNam : public PolicyEnforcingNam
{
public:
    RecordingPolicyNam(RequestLog *log, QObject *parent)
        : PolicyEnforcingNam(TransportPolicy::current(), parent), m_log(log) {}

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        {
            QMutexLocker lock(&m_log->mutex);
            m_log->urls << request.url();
            m_log->threads << QThread::currentThread();
        }
        return PolicyEnforcingNam::createRequest(op, request, outgoingData);
    }

private:
    RequestLog *m_log;
};

class RecordingPolicyNamFactory : public PolicyNamFactory
{
public:
    explicit RecordingPolicyNamFactory(RequestLog *log) : m_log(log) {}
    QNetworkAccessManager *create(QObject *parent) override
    {
        return new RecordingPolicyNam(m_log, parent);
    }

private:
    RequestLog *m_log;
};

#endif // RECORDINGPOLICYNAM_H
```

Create `qt-app/quick/tests/tst_qmlfactorygate.cpp`:

```cpp
#include <QtTest>
#include <QBuffer>
#include <QImage>
#include <QNetworkProxy>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QThread>

#include "RecordingPolicyNam.h"
#include "tinyhttpserver.h"

// S1a GATE G2: proves on this Qt build that QML image fetches (Image and
// Canvas.loadImage) go through a manager created by the engine's installed
// factory. RecordingPolicyNamFactory's managers log every request that passes
// through them; the in-process TinyHttpServer (127.0.0.1, ephemeral port)
// serves the image — never XAMPP.
namespace {
QByteArray pngBytes()
{
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}
} // namespace

class TestQmlFactoryGate : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void imageLoadUsesInstalledFactoryManager();
    void canvasLoadImageUsesInstalledFactoryManager();

private:
    QQuickItem *load(QQuickView &view, const QByteArray &qml);
    TinyHttpServer m_server;
};

void TestQmlFactoryGate::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    QVERIFY(m_server.listen());                         // 127.0.0.1, port 0
    TinyHttpServer::Response png;
    png.headers = {{"Content-Type", "image/png"}};
    png.body = pngBytes();
    m_server.route("/loams_api/uploads/students/gate-image.png", png);
    m_server.route("/loams_api/uploads/students/gate-canvas.png", png);
}

QQuickItem *TestQmlFactoryGate::load(QQuickView &view, const QByteArray &qml)
{
    QQmlComponent component(view.engine());
    component.setData(qml, QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return nullptr;
    }
    auto *item = qobject_cast<QQuickItem *>(component.create());
    if (!item)
        return nullptr;
    item->setParent(view.contentItem());
    item->setParentItem(view.contentItem());
    return item;
}

void TestQmlFactoryGate::imageLoadUsesInstalledFactoryManager()
{
    RequestLog log;
    RecordingPolicyNamFactory factory(&log);            // declared before the view: outlives it
    QQuickView view;
    QVERIFY(PolicyNamFactory::installOn(*view.engine(), factory));   // before any QML/request
    view.resize(100, 100);
    view.show();

    const QUrl url = m_server.url(QStringLiteral("/loams_api/uploads/students/gate-image.png"));
    QQuickItem *image = load(view, QByteArrayLiteral("import QtQuick\nImage { cache: false; asynchronous: true; source: \"")
                                   + url.toString().toUtf8() + "\" }");
    QVERIFY(image);
    QTRY_COMPARE_WITH_TIMEOUT(image->property("status").toInt(), 1 /* Image.Ready */, 5000);
    QVERIFY(m_server.countFor("/loams_api/uploads/students/gate-image.png") >= 1);

    QThread *issuer = log.threadFor(url);
    QVERIFY2(issuer, "GATE G2 FAILED: the Image request did not pass through a factory-made manager");
    qInfo("GATE G2: image request ran on the %s thread",
          issuer == QThread::currentThread() ? "GUI" : "non-GUI (pixmap reader)");
}

void TestQmlFactoryGate::canvasLoadImageUsesInstalledFactoryManager()
{
    RequestLog log;
    RecordingPolicyNamFactory factory(&log);
    QQuickView view;
    QVERIFY(PolicyNamFactory::installOn(*view.engine(), factory));
    view.resize(100, 100);
    view.show();

    const QUrl url = m_server.url(QStringLiteral("/loams_api/uploads/students/gate-canvas.png"));
    QQuickItem *canvas = load(view, QByteArrayLiteral(
        "import QtQuick\nCanvas { width: 10; height: 10; property bool done: false\n"
        "  Component.onCompleted: loadImage(\"") + url.toString().toUtf8() + QByteArrayLiteral("\")\n"
        "  onImageLoaded: done = true }"));
    QVERIFY(canvas);
    QTRY_VERIFY_WITH_TIMEOUT(canvas->property("done").toBool(), 5000);

    QVERIFY2(log.contains(url),
             "GATE G2 FAILED: the Canvas.loadImage request did not pass through a factory-made manager");
}

QTEST_MAIN(TestQmlFactoryGate)
#include "tst_qmlfactorygate.moc"
```

In `qt-app/quick/CMakeLists.txt`, after the `tst_policynamfactory` registration add:

```cmake

# --- S1a GATE G2 (C++ QtTest, offscreen): Qt's QML Image / Canvas.loadImage
# fetch through the installed QQmlNetworkAccessManagerFactory's managers.
# Loopback TinyHttpServer (127.0.0.1, ephemeral port) only. ---
wits_add_qttest(tst_qmlfactorygate
    SOURCES tests/tst_qmlfactorygate.cpp tests/RecordingPolicyNam.h
        ${CMAKE_SOURCE_DIR}/testsupport/tinyhttpserver.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/tinyhttpserver.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport ${CMAKE_CURRENT_SOURCE_DIR}/tests
    OFFSCREEN)
```

- [ ] **Step 2: Prove the detector — expected FAIL.** Temporarily delete the line `QVERIFY(PolicyNamFactory::installOn(*view.engine(), factory));   // before any QML/request` in `imageLoadUsesInstalledFactoryManager` only, then:

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_qmlfactorygate
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^tst_qmlfactorygate$' --output-on-failure
```

Expected: `imageLoadUsesInstalledFactoryManager` FAILS with `GATE G2 FAILED: the Image request did not pass through a factory-made manager` (the image still loads via Qt's default manager — exactly the bypass the gate exists to catch). Restore the deleted line exactly.

- [ ] **Step 3: GATE G2 (MANDATORY) — run, expected PASS:**

```bash
cmake --build C:/b/s1a --target tst_qmlfactorygate
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^tst_qmlfactorygate$' --output-on-failure -V | tee C:/b/s1a-gate-g2.log
grep "GATE G2:" C:/b/s1a-gate-g2.log
```

Expected: both functions pass; the log line records which thread issued the image request (keep it for the proof doc).

**If GATE G2 fails** (either function, with the factory installed): **STOP.** Do not start Task 6 or any later task, and do not substitute a workaround (no custom `image://` provider, no C++-side image download, no URL interceptor, no reliance on the default manager). Record the failing output and escalate to the owner for a design review of spec §2's QML-factory design.

- [ ] **Step 4: Commit** via the project `commit` skill. Intended subject: `test(quick): gate QML image loading on the installed network factory`.

---

### Task 6: Seam guard — no raw manager outside the seam, no SSL-error bypass

**Files:**
- Create: `qt-app/quick/tests/tst_transportseamguard.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (register after the `tst_notokenaliases` block, line 481)
- Test: the guard itself

**Interfaces:**
- Consumes: source tree under `SRC_ROOT` (= `qt-app/`).
- Produces: CTest `tst_transportseamguard`; constant `kPendingMigration` (the not-yet-migrated owners, shrunk by Tasks 8-12, empty at the end of Task 12).

- [ ] **Step 1: Write the guard with an EMPTY pending list** (to prove it detects today's owners). Create `qt-app/quick/tests/tst_transportseamguard.cpp`:

```cpp
#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

// S1a transport-seam source guard (S1 spec §3 "Injection boundary" and
// "SSL errors are never ignored ... a source grep check enforces this").
//
// 1. No QNetworkAccessManager is constructed (heap, make_unique/shared, stack
//    or by-value member) or subclassed in production code under qt-app/core
//    or qt-app/quick outside the seam: owners use HttpClient (C++) or
//    PolicyNamFactory (QML). Exempt: quick/tests/, qt-app/tests/ and
//    qt-app/testsupport/ (test doubles); the frozen legacy Widgets sources
//    (qt-app root) are not scanned.
// 2. Every core network class asserts a non-null injected manager (its raw
//    pointer constructor is a test seam; production passes HttpClient::manager()).
// 3. The SSL-error bypass call never appears anywhere in client source.
//
// Matching is on comment-stripped code (approximate: a "//" inside a string
// literal truncates that line, which can only hide code written after a URL).
//
// kPendingMigration lists owners not yet moved onto the seam. Each migration
// task deletes its entries; a listed file that no longer offends FAILS as
// stale, so the list can only shrink. It is empty once S1a is complete.
namespace {
const QStringList kPendingMigration = {
};

const QRegularExpression kNamConstruction(QStringLiteral(
    "\\bnew\\s+QNetworkAccessManager\\b"
    "|\\bmake_(?:unique|shared)\\s*<\\s*QNetworkAccessManager\\s*>"
    "|\\bQNetworkAccessManager\\s+[A-Za-z_]\\w*\\s*[;({=]"));
const QRegularExpression kNamSubclass(QStringLiteral(":\\s*public\\s+QNetworkAccessManager\\b"));
const QString kSeamSubclassHeader = QStringLiteral("core/transport/policyenforcingnam.h");

// Core classes that hold an injected manager (Task 4b contract).
const QStringList kCoreNetworkClasses = {
    QStringLiteral("core/studentcontroller.cpp"),
    QStringLiteral("core/visitorcontroller.cpp"),
    QStringLiteral("core/reportcontroller.cpp"),
    QStringLiteral("core/importcontroller.cpp"),
    QStringLiteral("core/brandingcontroller.cpp"),
    QStringLiteral("core/accesscontrol/accessdecisionservice.cpp"),
    QStringLiteral("core/accesscontrol/turnstileprovider.cpp"),
};
} // namespace

class TestTransportSeamGuard : public QObject
{
    Q_OBJECT
private slots:
    void namConstructedOnlyInsideSeam();
    void coreNetworkClassesRequireInjectedManager();
    void noSslErrorBypassInClientSource();

private:
    static QStringList sourceFiles(const QString &subdir, const QStringList &globs,
                                   const QStringList &skipPrefixes);
    static QString stripComments(const QString &src);
    static QString readSource(const QString &rel, bool *ok);
};

QStringList TestTransportSeamGuard::sourceFiles(const QString &subdir, const QStringList &globs,
                                                const QStringList &skipPrefixes)
{
    const QDir root(QStringLiteral(SRC_ROOT));
    QStringList out;
    QDirIterator it(subdir.isEmpty() ? root.path() : root.filePath(subdir), globs,
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString rel = root.relativeFilePath(it.next());
        const QStringList segments = rel.split(QLatin1Char('/'));
        const bool inBuildDir = std::any_of(segments.cbegin(), segments.cend() - 1,
            [](const QString &s) { return s.startsWith(QLatin1String("build")); });
        const bool skipped = std::any_of(skipPrefixes.cbegin(), skipPrefixes.cend(),
            [&rel](const QString &p) { return rel.startsWith(p); });
        if (!inBuildDir && !skipped)
            out << rel;
    }
    out.sort();
    return out;
}

QString TestTransportSeamGuard::stripComments(const QString &src)
{
    QString out = src;
    out.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"),
                                  QRegularExpression::DotMatchesEverythingOption));
    out.remove(QRegularExpression(QStringLiteral("//[^\\n]*")));
    return out;
}

QString TestTransportSeamGuard::readSource(const QString &rel, bool *ok)
{
    QFile f(QDir(QStringLiteral(SRC_ROOT)).filePath(rel));
    *ok = f.open(QIODevice::ReadOnly);
    return *ok ? QString::fromUtf8(f.readAll()) : QString();
}

void TestTransportSeamGuard::namConstructedOnlyInsideSeam()
{
    const QStringList globs = {QStringLiteral("*.cpp"), QStringLiteral("*.h")};
    const QStringList files = sourceFiles(QStringLiteral("core"), globs, {})
        + sourceFiles(QStringLiteral("quick"), globs, {QStringLiteral("quick/tests/")});
    QVERIFY2(files.size() > 50, "scan found too few files — is SRC_ROOT wrong?");

    QStringList offenders;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        const bool constructs = kNamConstruction.match(code).hasMatch();
        const bool subclasses = rel != kSeamSubclassHeader && kNamSubclass.match(code).hasMatch();
        if (constructs || subclasses)
            offenders << rel;
    }

    QStringList unexpected;
    QStringList stale;
    for (const QString &o : offenders) {
        if (!kPendingMigration.contains(o))
            unexpected << o;
    }
    for (const QString &p : kPendingMigration) {
        if (!offenders.contains(p))
            stale << p;
    }
    QVERIFY2(unexpected.isEmpty(), qPrintable(
        QStringLiteral("QNetworkAccessManager constructed/subclassed outside the transport "
                       "seam (use HttpClient / PolicyNamFactory): ") + unexpected.join(QStringLiteral(", "))));
    QVERIFY2(stale.isEmpty(), qPrintable(
        QStringLiteral("Stale kPendingMigration entries (already migrated — delete them): ")
        + stale.join(QStringLiteral(", "))));
}

void TestTransportSeamGuard::coreNetworkClassesRequireInjectedManager()
{
    // Every core class that stores a QNetworkAccessManager* must (a) be listed
    // here, so a new one cannot slip in unchecked, and (b) assert non-null in
    // its constructor (Task 4b contract).
    const QStringList files = sourceFiles(QStringLiteral("core"), {QStringLiteral("*.cpp")}, {});
    const QRegularExpression storesManager(QStringLiteral("\\bm_nam\\s*\\(\\s*nam\\s*\\)"));
    QStringList unlisted;
    QStringList missingAssert;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (!storesManager.match(code).hasMatch())
            continue;
        if (!kCoreNetworkClasses.contains(rel))
            unlisted << rel;
        if (!code.contains(QLatin1String("Q_ASSERT_X(nam,")))
            missingAssert << rel;
    }
    for (const QString &rel : kCoreNetworkClasses) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (!code.contains(QLatin1String("Q_ASSERT_X(nam,")) && !missingAssert.contains(rel))
            missingAssert << rel;
    }
    QVERIFY2(unlisted.isEmpty(), qPrintable(
        QStringLiteral("core class stores an injected manager but is not in kCoreNetworkClasses: ")
        + unlisted.join(QStringLiteral(", "))));
    QVERIFY2(missingAssert.isEmpty(), qPrintable(
        QStringLiteral("core network class lacks Q_ASSERT_X(nam, ...) non-null contract: ")
        + missingAssert.join(QStringLiteral(", "))));
}

void TestTransportSeamGuard::noSslErrorBypassInClientSource()
{
    // Needle assembled at runtime so this file never matches itself.
    const QString needle = QStringLiteral("ignoreSsl") + QStringLiteral("Errors");
    const QStringList files = sourceFiles(QString(),
        {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.qml"), QStringLiteral("*.js")},
        {QStringLiteral("libs/")});
    QVERIFY2(files.size() > 50, "scan found too few files — is SRC_ROOT wrong?");

    QStringList hits;
    for (const QString &rel : files) {
        bool ok = false;
        const QString raw = readSource(rel, &ok);           // raw: comments count too
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (raw.contains(needle))
            hits << rel;
    }
    QVERIFY2(hits.isEmpty(), qPrintable(
        QStringLiteral("SSL errors must never be ignored (spec §3): ") + hits.join(QStringLiteral(", "))));
}

QTEST_APPLESS_MAIN(TestTransportSeamGuard)
#include "tst_transportseamguard.moc"
```

In `qt-app/quick/CMakeLists.txt`, after the `tst_notokenaliases` registration (ends line 481) add:

```cmake

# --- S1a transport-seam source guard (C++ QtTest). Pure file I/O over qt-app/
# (no GUI -> QTEST_APPLESS_MAIN, no OFFSCREEN): no QNetworkAccessManager built
# outside HttpClient/PolicyNamFactory, no SSL-error bypass anywhere in client
# source, and quick/main.cpp installs the seam before the first QML load. ---
wits_add_qttest(tst_transportseamguard
    SOURCES tests/tst_transportseamguard.cpp
    DEFINES SRC_ROOT="${CMAKE_SOURCE_DIR}")
```

- [ ] **Step 2: Run — expected FAIL, listing exactly today's 11 owners:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

Expected: `namConstructedOnlyInsideSeam` FAILS with `... outside the transport seam ...: quick/AccessControlHub.cpp, quick/viewmodels/AccessControlViewModel.cpp, quick/viewmodels/DashboardViewModel.cpp, quick/viewmodels/DatabaseViewModel.cpp, quick/viewmodels/GuestViewModel.cpp, quick/viewmodels/ImportViewModel.cpp, quick/viewmodels/KioskViewModel.cpp, quick/viewmodels/ReportingViewModel.cpp, quick/viewmodels/SearchViewModel.cpp, quick/viewmodels/SettingsViewModel.cpp, quick/viewmodels/VisitLogsViewModel.cpp`; `coreNetworkClassesRequireInjectedManager` and `noSslErrorBypassInClientSource` PASS (Task 4b already landed the contract — to see the contract check detect a regression, temporarily delete one `Q_ASSERT_X(nam, ...)` line, confirm it fails naming that file, then restore it). If the list differs from these 11, stop and reconcile with the Manager-owner inventory above before continuing.

- [ ] **Step 3: Record the pending owners.** Replace the empty `kPendingMigration` initializer with:

```cpp
const QStringList kPendingMigration = {
    QStringLiteral("quick/AccessControlHub.cpp"),                    // Task 8
    QStringLiteral("quick/viewmodels/AccessControlViewModel.cpp"),   // Task 9
    QStringLiteral("quick/viewmodels/DashboardViewModel.cpp"),       // Task 9
    QStringLiteral("quick/viewmodels/VisitLogsViewModel.cpp"),       // Task 9
    QStringLiteral("quick/viewmodels/GuestViewModel.cpp"),           // Task 10
    QStringLiteral("quick/viewmodels/KioskViewModel.cpp"),           // Task 10
    QStringLiteral("quick/viewmodels/DatabaseViewModel.cpp"),        // Task 11
    QStringLiteral("quick/viewmodels/SearchViewModel.cpp"),          // Task 11
    QStringLiteral("quick/viewmodels/ImportViewModel.cpp"),          // Task 12
    QStringLiteral("quick/viewmodels/ReportingViewModel.cpp"),       // Task 12
    QStringLiteral("quick/viewmodels/SettingsViewModel.cpp"),        // Task 12
};
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake --build C:/b/s1a --target tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `test(transport): guard manager construction and SSL-error bypass at the seam`.

---

### Task 6b: Offline test harness — no test may reach a real backend

Today many ViewModel tests default-construct their VM, which builds a real manager aimed at `http://localhost/loams_api/`; some drive request paths (e.g. `tst_databaseviewmodel.cpp` delete/edit/register/department paths around lines 154, 445, 770, 786, 801) and could mutate a running XAMPP. After S1a a default-constructed owner builds a real `PolicyEnforcingNam` (Passthrough), so the risk would persist. This task adds `OfflineHttp` — an `HttpClient` over a `SequencedNam` that records every request and **never answers** (stalled until aborted) — and a guard that forbids default-constructed seam owners in `quick/tests/tst_*.cpp` outside dedicated `defaultConstruction*` / `defaultHub*` ownership tests (which issue no request). Tasks 8-12 then migrate every such site (213 ViewModel sites in 10 files + the hubs in `tst_accesscontrolhub.cpp:261` and `tst_appshell.cpp:30`).

**Audit result (from the code on `master`):** default-constructed seam owners per test file — `tst_dashboardviewmodel` 4, `tst_visitlogsviewmodel` 8, `tst_accesscontrolviewmodel` 8, `tst_kioskviewmodel` 18, `tst_guestviewmodel` 3, `tst_searchviewmodel` 9, `tst_databaseviewmodel` 63 (`vm`×57, `vm2`×2, `del`, `del2`, `bulk`, `bulk2`), `tst_importviewmodel` 5, `tst_reportingviewmodel` 57 (`vm`×56, `vm2`), `tst_settingsviewmodel` 38 (`vm`×35, `vm2`, `info`, `reset`) = **213**; plus `AccessControlHub hub;` in `tst_accesscontrolhub.cpp:261` and `tst_appshell.cpp:30`. All are migrated, not only the ones known to issue requests — that is provably complete and costs nothing for network-free tests. QML tests that instantiate real VMs (`tst_qml_kiosk.qml:134` `KioskScreen`, only `reloadSchoolInfo()`; `tst_qml_adminshell.qml:26` `AdminScreen { autoLoad: false }`; `tst_qml_admin.qml:1685` vm-less `SettingsScreen` theme picker; `DatabaseScreen.qml:298`'s own `ImportViewModel`, never driven to `startImport()`) issue no request; `QuickTestSetup.h`'s static hub is never `initialize()`d, so it never polls. Task 14 Step 1 proves the whole suite stays offline empirically (Apache access-log unchanged across a full `ctest` run).

**Why no runtime "offline default" inside `HttpClient`:** the only S1a policy is Passthrough, so making an un-injected test client offline would need either a new policy mode or a manager-swap hook in the production seam — a code path a reviewer must prove can never be selected in production (a swap hook is itself a bypass vector). The static guard + empirical access-log check give the same assurance with zero production change, and S1e's fail-closed default policy (no configured `https` origin → no request) then makes an un-injected test client unable to reach `http://localhost` at all.

**Files:**
- Modify: `qt-app/testsupport/sequencednam.h` (public API + one member), `qt-app/testsupport/sequencednam.cpp` (lines 66-78 `createRequest`)
- Create: `qt-app/testsupport/offlinehttp.h`
- Modify: `qt-app/tests/tst_httpclient.cpp`, `qt-app/tests/CMakeLists.txt` (`tst_httpclient` registration from Task 4)
- Modify: `qt-app/quick/CMakeLists.txt` (new `WITS_TEST_NAM_SOURCES` after line 158; re-register 12 test targets)
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (new `ownersInTestsUseFakeManagers` + `kPendingTestMigration`)
- Test: `tst_httpclient::offlineHttpNeverAnswers`, `tst_transportseamguard::ownersInTestsUseFakeManagers`

**Interfaces:**
- Consumes: `SequencedNam`, `HttpClient(QObject*, QNetworkAccessManager*)`.
- Produces: `void SequencedNam::setStallWhenEmpty(bool on);` (default `false` — every existing `SequencedNam` user keeps today's "valid empty poll" answer); `struct OfflineHttp { SequencedNam nam; HttpClient http; }` (header-only, test-only); CMake list `WITS_TEST_NAM_SOURCES`; guard list `kPendingTestMigration` (12 entries, shrunk by Tasks 8-12, empty at the end of Task 12).

- [ ] **Step 1: Write the failing tests.** In `qt-app/tests/tst_httpclient.cpp` add after `#include "capturingnam.h"`:

```cpp
#include "offlinehttp.h"
```

add `void offlineHttpNeverAnswers();` to `private slots:` and before `QTEST_MAIN`:

```cpp
void TestHttpClient::offlineHttpNeverAnswers()
{
    // Test-safety helper: a request through OfflineHttp is recorded and then
    // stays in flight — no backend, no canned answer — until it is aborted.
    OfflineHttp offline;
    std::unique_ptr<QNetworkReply> reply(offline.http.manager()->get(
        QNetworkRequest(QUrl(QStringLiteral("http://localhost/loams_api/get_departments.php")))));
    QCOMPARE(offline.nam.requestCount(), 1);
    QCOMPARE(offline.nam.lastUrl.path(), QStringLiteral("/loams_api/get_departments.php"));
    QTest::qWait(100);                       // negative assertion: nothing may answer
    QVERIFY(!reply->isFinished());
    reply->abort();
    QVERIFY(reply->isFinished());
    QCOMPARE(reply->error(), QNetworkReply::OperationCanceledError);
}
```

In `qt-app/tests/CMakeLists.txt`, in the `tst_httpclient` registration (Task 4) add these lines to `SOURCES` after the `capturingnam.h` line:

```cmake
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
        ${CMAKE_SOURCE_DIR}/testsupport/offlinehttp.h
```

In `qt-app/quick/tests/tst_transportseamguard.cpp`, after the `kCoreNetworkClasses` list (inside the anonymous namespace) add:

```cpp
// Test files that still construct a seam owner (network-owning ViewModel or
// AccessControlHub) WITHOUT an injected fake manager. Tasks 8-12 delete their
// entries; empty once S1a is complete. A listed file that no longer offends
// FAILS as stale.
const QStringList kPendingTestMigration = {
};

const QString kOwnerTypes = QStringLiteral(
    "(?:(?:Dashboard|VisitLogs|AccessControl|Kiosk|Guest|Search|Database|Import|Reporting|Settings)"
    "ViewModel|AccessControlHub)");
```

add `void ownersInTestsUseFakeManagers();` to `private slots:`, and before `QTEST_APPLESS_MAIN`:

```cpp
void TestTransportSeamGuard::ownersInTestsUseFakeManagers()
{
    // A seam owner constructed in a test with no HttpClient argument builds a
    // real manager aimed at the configured backend (default
    // http://localhost/loams_api/) and could mutate a running XAMPP. Every test
    // must pass an HttpClient over CapturingNam / SequencedNam / OfflineHttp.
    // Exempt: functions named defaultConstruction* / defaultHub*, which only
    // assert seam ownership and issue no request.
    const QStringList files = sourceFiles(QStringLiteral("quick/tests"), {QStringLiteral("tst_*.cpp")}, {});
    QVERIFY2(files.size() > 20, "scan found too few test files — is SRC_ROOT wrong?");
    const QRegularExpression header(QStringLiteral("^void\\s+\\w+::(\\w+)\\s*\\("),
                                    QRegularExpression::MultilineOption);
    const QRegularExpression stackDefault(
        QStringLiteral("^[ \\t]*(?:static\\s+)?") + kOwnerTypes
            + QStringLiteral("\\s+\\w+\\s*(?:;|\\{\\s*\\}|\\([^,()]*\\))"),
        QRegularExpression::MultilineOption);
    const QRegularExpression heapDefault(
        QStringLiteral("\\b(?:new\\s+|make_unique\\s*<\\s*)") + kOwnerTypes
            + QStringLiteral("\\s*>?\\s*\\([^,()]*\\)"));

    QStringList offenders;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        QList<QPair<qsizetype, QString>> functions;   // (start offset, name), in order
        for (auto it = header.globalMatch(code); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            functions.append({m.capturedStart(), m.captured(1)});
        }
        auto enclosing = [&functions](qsizetype pos) {
            QString name;
            for (const auto &f : functions) {
                if (f.first > pos)
                    break;
                name = f.second;
            }
            return name;
        };
        bool offends = false;
        for (const QRegularExpression *re : {&stackDefault, &heapDefault}) {
            for (auto it = re->globalMatch(code); it.hasNext();) {
                const QString fn = enclosing(it.next().capturedStart());
                if (!fn.startsWith(QLatin1String("defaultConstruction"))
                    && !fn.startsWith(QLatin1String("defaultHub")))
                    offends = true;
            }
        }
        if (offends)
            offenders << rel;
    }

    QStringList unexpected;
    QStringList stale;
    for (const QString &o : offenders) {
        if (!kPendingTestMigration.contains(o))
            unexpected << o;
    }
    for (const QString &p : kPendingTestMigration) {
        if (!offenders.contains(p))
            stale << p;
    }
    QVERIFY2(unexpected.isEmpty(), qPrintable(
        QStringLiteral("test constructs a seam owner without a fake manager (inject "
                       "OfflineHttp / CapturingNam / SequencedNam via HttpClient): ")
        + unexpected.join(QStringLiteral(", "))));
    QVERIFY2(stale.isEmpty(), qPrintable(
        QStringLiteral("Stale kPendingTestMigration entries (already migrated — delete them): ")
        + stale.join(QStringLiteral(", "))));
}
```

- [ ] **Step 2: Run — expected FAIL:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_httpclient tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

Expected: `tst_httpclient` fails to compile (`offlinehttp.h: No such file or directory`); `ownersInTestsUseFakeManagers` FAILS listing exactly these 12 files: `quick/tests/tst_accesscontrolhub.cpp, quick/tests/tst_accesscontrolviewmodel.cpp, quick/tests/tst_appshell.cpp, quick/tests/tst_dashboardviewmodel.cpp, quick/tests/tst_databaseviewmodel.cpp, quick/tests/tst_guestviewmodel.cpp, quick/tests/tst_importviewmodel.cpp, quick/tests/tst_kioskviewmodel.cpp, quick/tests/tst_reportingviewmodel.cpp, quick/tests/tst_searchviewmodel.cpp, quick/tests/tst_settingsviewmodel.cpp, quick/tests/tst_visitlogsviewmodel.cpp`. If the list differs, stop and reconcile with the audit above.

- [ ] **Step 3: Implement.** In `qt-app/testsupport/sequencednam.h` add after `void enqueueStall();`:

```cpp
    // When on, a request with nothing queued STALLS (finishes only via abort())
    // instead of getting the default valid-empty-poll answer. Used by
    // OfflineHttp so tests can never reach — or be answered as if by — a backend.
    void setStallWhenEmpty(bool on) { m_stallWhenEmpty = on; }
```

and after `int m_maxActive = 0;`:

```cpp
    bool m_stallWhenEmpty = false;
```

In `qt-app/testsupport/sequencednam.cpp` replace lines 73-76 (the `Canned c = ...` statement) with:

```cpp
    Canned c = !m_queue.isEmpty() ? m_queue.dequeue()
               : m_stallWhenEmpty ? Canned{QByteArray(), QNetworkReply::NoError, true}
                                  : Canned{QByteArrayLiteral("{\"status\":\"success\",\"latest_id\":0,\"entry\":null}"),
                                           QNetworkReply::NoError, false};
```

Create `qt-app/testsupport/offlinehttp.h`:

```cpp
#ifndef OFFLINEHTTP_H
#define OFFLINEHTTP_H

#include "sequencednam.h"
#include "transport/httpclient.h"

// Test-only seam client that can never reach a real backend: every request is
// recorded by SequencedNam (requestCount(), lastUrl, urls) and then stays in
// flight — never answered — until aborted. Inject it into every seam owner a
// test constructs (ViewModel / AccessControlHub) unless the test injects its
// own CapturingNam / SequencedNam:
//
//     OfflineHttp vmHttp;
//     DatabaseViewModel vm(nullptr, &vmHttp.http);
//
// Declare it BEFORE the owner so it outlives it. Enforced by
// tst_transportseamguard::ownersInTestsUseFakeManagers.
struct OfflineHttp
{
    OfflineHttp() { nam.setStallWhenEmpty(true); }

    SequencedNam nam;
    HttpClient http{nullptr, &nam};   // declared after nam: constructed after it
};

#endif // OFFLINEHTTP_H
```

In `qt-app/quick/CMakeLists.txt`, after the `set(WITS_LOAMS_STATIC_LIBS ...)` statement (lines 157-158) add:

```cmake

# Fake managers for tests that construct seam owners (S1a transport seam):
# CapturingNam / SequencedNam + the header-only OfflineHttp. Every quick test
# that builds a ViewModel or AccessControlHub compiles these and adds
# ${CMAKE_SOURCE_DIR}/testsupport to INCLUDES.
set(WITS_TEST_NAM_SOURCES
    ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
    ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
    ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
    ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
    ${CMAKE_SOURCE_DIR}/testsupport/offlinehttp.h)
```

and re-register the 12 owner-constructing test targets with it (replace each existing `wits_add_qttest(<name> ...)` call, keeping the comment block above it):

```cmake
wits_add_qttest(tst_appshell
    SOURCES tests/tst_appshell.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS ${WITS_LOAMS_STATIC_LIBS}
         Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_kioskviewmodel
    SOURCES tests/tst_kioskviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_guestviewmodel
    SOURCES tests/tst_guestviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_accesscontrolhub
    SOURCES tests/tst_accesscontrolhub.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_dashboardviewmodel
    SOURCES tests/tst_dashboardviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_visitlogsviewmodel
    SOURCES tests/tst_visitlogsviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_accesscontrolviewmodel
    SOURCES tests/tst_accesscontrolviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_searchviewmodel
    SOURCES tests/tst_searchviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_databaseviewmodel
    SOURCES tests/tst_databaseviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_settingsviewmodel
    SOURCES tests/tst_settingsviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_importviewmodel
    SOURCES tests/tst_importviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

```cmake
wits_add_qttest(tst_reportingviewmodel
    SOURCES tests/tst_reportingviewmodel.cpp ${WITS_TEST_NAM_SOURCES}
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

In `tst_transportseamguard.cpp`, fill `kPendingTestMigration` with the 12 files the red run listed:

```cpp
const QStringList kPendingTestMigration = {
    QStringLiteral("quick/tests/tst_appshell.cpp"),                 // Task 8 (needs the hub's HttpClient ctor)
    QStringLiteral("quick/tests/tst_accesscontrolhub.cpp"),         // Task 8
    QStringLiteral("quick/tests/tst_accesscontrolviewmodel.cpp"),   // Task 9
    QStringLiteral("quick/tests/tst_dashboardviewmodel.cpp"),       // Task 9
    QStringLiteral("quick/tests/tst_visitlogsviewmodel.cpp"),       // Task 9
    QStringLiteral("quick/tests/tst_guestviewmodel.cpp"),           // Task 10
    QStringLiteral("quick/tests/tst_kioskviewmodel.cpp"),           // Task 10
    QStringLiteral("quick/tests/tst_databaseviewmodel.cpp"),        // Task 11
    QStringLiteral("quick/tests/tst_searchviewmodel.cpp"),          // Task 11
    QStringLiteral("quick/tests/tst_importviewmodel.cpp"),          // Task 12
    QStringLiteral("quick/tests/tst_reportingviewmodel.cpp"),       // Task 12
    QStringLiteral("quick/tests/tst_settingsviewmodel.cpp"),        // Task 12
};
```

- [ ] **Step 4: Run — expected PASS** (and the existing `SequencedNam` suites are unaffected because the stall mode defaults off):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_httpclient tst_transportseamguard tst_turnstileprovider tst_accesscontrolservice tst_accesscontrolhub
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_httpclient|tst_transportseamguard|tst_turnstileprovider|tst_accesscontrolservice|tst_accesscontrolhub)$' --output-on-failure -j 4
```

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `test(transport): add offline test client and forbid live managers in tests`.

---

### Task 7: Install the seam in `WITSQuick`, the QuickTest harness and `tst_appshell`

**Files:**
- Modify: `qt-app/quick/main.cpp` (lines 1-58)
- Modify: `qt-app/quick/tests/QuickTestSetup.h` (whole file)
- Modify: `qt-app/quick/tests/tst_appshell.cpp` (whole file)
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (add one slot)
- Test: `tst_transportseamguard::quickMainInstallsTransportBeforeLoad`, `tst_appshell`, all `tst_qml_*`

**Interfaces:**
- Consumes: `TransportPolicy::setCurrent/makePassthrough`, `PolicyNamFactory`, `PolicyEnforcingNam`.
- Produces: production composition order — policy published before `AccessControlHub` (whose `HttpClient`, after Task 8, snapshots it); `PolicyNamFactory namFactory` declared before `QQmlApplicationEngine engine` (outlives it) and installed before `loadFromModule`.

- [ ] **Step 1: Write the failing wiring check.** In `qt-app/quick/tests/tst_transportseamguard.cpp` add `void quickMainInstallsTransportBeforeLoad();` to `private slots:` and this function before `QTEST_APPLESS_MAIN`:

```cpp
void TestTransportSeamGuard::quickMainInstallsTransportBeforeLoad()
{
    bool ok = false;
    const QString code = stripComments(readSource(QStringLiteral("quick/main.cpp"), &ok));
    QVERIFY2(ok, "cannot read quick/main.cpp");
    const qsizetype policy  = code.indexOf(QLatin1String("TransportPolicy::setCurrent("));
    const qsizetype hub     = code.indexOf(QLatin1String("AccessControlHub accessControl"));
    const qsizetype factory = code.indexOf(QLatin1String("PolicyNamFactory namFactory"));
    const qsizetype engine  = code.indexOf(QLatin1String("QQmlApplicationEngine engine"));
    const qsizetype install = code.indexOf(QLatin1String("PolicyNamFactory::installOn(engine, namFactory)"));
    QVERIFY2(!code.contains(QLatin1String("setNetworkAccessManagerFactory(")),
             "install the factory only via PolicyNamFactory::installOn (it refuses a late install)");
    const qsizetype load    = code.indexOf(QLatin1String("engine.loadFromModule("));
    QVERIFY2(policy >= 0 && hub >= 0 && policy < hub,
             "the transport policy must be published before AccessControlHub builds its HttpClient");
    QVERIFY2(factory >= 0 && engine >= 0 && factory < engine,
             "PolicyNamFactory must be declared before (so it outlives) the engine");
    QVERIFY2(install > engine && load > install,
             "the factory must be installed on the engine before the first QML load");
}
```

- [ ] **Step 2: Run — expected FAIL:**

```bash
cmake --build C:/b/s1a --target tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

Expected: `quickMainInstallsTransportBeforeLoad` FAILS with `the transport policy must be published before AccessControlHub ...`.

- [ ] **Step 3: Implement.** Replace lines 1-58 of `qt-app/quick/main.cpp` (everything up to and including `return -1;` after `loadFromModule`) with:

```cpp
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>

#include "AccessControlHub.h"
#include "PolicyNamFactory.h"
#include "apiconfigloader.h"
#include "appsettings.h"
#include <QSGRendererInterface>
#include "brandtheme.h"
#include "transport/transportpolicy.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Runtime backend URL (WITS_API_BASE_URL > config.ini > localhost default),
    // applied FIRST: AccessControlHub::initialize() captures ApiConfig::baseUrl()
    // into the TurnstileProvider factory, and LoginParser's same-origin photo
    // check compares against that base -- both must see the runtime value.
    ApiConfigLoader::applyFromRuntime(QCoreApplication::applicationDirPath());

    // S1a transport seam (S1 spec §3): one immutable policy for every manager
    // in the process, published before any network object exists. Passthrough
    // keeps today's behaviour exactly; S1e replaces this line with the
    // TransportBootstrap Phase C policy and deletes Passthrough.
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());

    // Deployment-hardware fallback (proposal §19/spec §10 Risk 4): the library
    // PC may lack a working OpenGL/RHI path. "--software" or
    // QT_QUICK_BACKEND=software forces the software rasterizer scene-graph
    // backend. Must precede the first QQuickWindow creation.
    const QStringList args = QCoreApplication::arguments();
    if (args.contains(QStringLiteral("--software"))
        || qEnvironmentVariable("QT_QUICK_BACKEND") == QLatin1String("software")) {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    // Use the "Basic" Controls style (not the native Windows style) so the
    // LButton/TextField `background`/`contentItem` customizations in the
    // component library actually apply — the native style ignores them and
    // emits a QWARN per customized control otherwise. Must precede the first
    // Controls type instantiation.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Cache-first branding (spec §6): apply the cached palette before the first
    // frame so there is no unbranded flash. Background re-sync lands in Phase 4.
    {
        AppSettings brandingStore;
        BrandTheme::setCurrent(BrandTheme::loadCachedConfig(brandingStore).palette);
    }

    // QML-engine managers come from the seam too. Declared before `engine` so
    // it outlives it (QQmlEngine does not take ownership of the factory).
    PolicyNamFactory namFactory;

    AccessControlHub accessControl;      // stack-owned; outlives `engine`
    accessControl.initialize();          // reads the flag; polls only if enabled
    AccessControlHub::setInstance(&accessControl);

    QQmlApplicationEngine engine;
    // Before any QML loads or any request starts; installOn() refuses (and we
    // fail closed) if that ordering is ever broken.
    if (!PolicyNamFactory::installOn(engine, namFactory))
        return -1;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("LOAMS", "AppShell");
    if (engine.rootObjects().isEmpty())
        return -1;
```

(Lines 59-72 — the `--fullscreen` block and `return app.exec();` — stay unchanged.)

Replace `qt-app/quick/tests/QuickTestSetup.h` with:

```cpp
#ifndef QUICKTESTSETUP_H
#define QUICKTESTSETUP_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include "AccessControlHub.h"
#include "PolicyNamFactory.h"

// Shared QUICK_TEST_MAIN_WITH_SETUP setup object for every generated QuickTest
// main: the tst_qml_* targets (registered via wits_add_qmltest in
// quick/CMakeLists.txt; each runs exactly one tst_qml_*.qml) and
// tst_quicktestmain_missingfile.
class QuickTestSetup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // S1a transport seam: QuickTest engines get their managers from the
        // same PolicyNamFactory as WITSQuick (quick/main.cpp), installed before
        // anything loads. Static: outlives every engine in the process.
        static PolicyNamFactory namFactory;
        if (!PolicyNamFactory::installOn(*engine, namFactory))
            qFatal("QuickTestSetup: transport seam could not be installed before QML load");
        // The statically-linked witsquickmodule embeds its qmldir under
        // qrc:/qt/qml; make it importable from the .qml test files. The literal
        // "import LOAMS" lives only in the QUICK_TEST_MAIN .qml data file,
        // which qmlimportscanner never sees, so the automatic static-plugin
        // import never fires.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // AccessControlScreen/KioskScreen resolve the AccessControl singleton.
        // Live-but-disabled hub (never initialize() -> no polling), robust
        // against an inherited WITS_ACCESS_CONTROL.
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

#endif // QUICKTESTSETUP_H
```

Replace `qt-app/quick/tests/tst_appshell.cpp` with:

```cpp
#include <QtTest>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtGlobal>
#include <vector>

#include "AccessControlHub.h"
#include "PolicyNamFactory.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

// Capture QML warnings so a load that "succeeds" but logs binding/type
// warnings still fails the test (premium-shell discipline: zero warnings).
static std::vector<QString> g_messages;
static void captureHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
        g_messages.push_back(msg);
}

class TestAppShell : public QObject
{
    Q_OBJECT
private slots:
    void loadsWithZeroWarnings();
};

void TestAppShell::loadsWithZeroWarnings()
{
    g_messages.clear();
    QtMessageHandler prev = qInstallMessageHandler(captureHandler);

    // Same composition order as quick/main.cpp (S1a transport seam).
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
    PolicyNamFactory namFactory;          // declared BEFORE engine -> outlives it

    AccessControlHub hub;                 // default settings, no env -> disabled
    AccessControlHub::setInstance(&hub);

    QQmlApplicationEngine engine;         // declared AFTER hub -> engine dies first
    const bool installed = PolicyNamFactory::installOn(engine, namFactory);   // before the load
    engine.loadFromModule("LOAMS", "AppShell");

    qInstallMessageHandler(prev);

    QVERIFY(installed);
    QCOMPARE(engine.networkAccessManagerFactory(),
             static_cast<QQmlNetworkAccessManagerFactory *>(&namFactory));
    QCOMPARE(engine.rootObjects().size(), 1);
    if (!g_messages.empty())
        qWarning() << "Unexpected QML messages:" << g_messages;
    QVERIFY(g_messages.empty());
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(engine.networkAccessManager()));
    QVERIFY(namFactory.createdCount() >= 1);
}

QTEST_MAIN(TestAppShell)
#include "tst_appshell.moc"
```

- [ ] **Step 4: Run — expected PASS** (guard, AppShell, every QuickTest, and the app links):

```bash
cmake --build C:/b/s1a --target WITSQuick tst_transportseamguard tst_appshell tst_qml_theme tst_qml_components tst_qml_kiosk tst_qml_admin tst_qml_adminshell tst_qml_accesscontrol tst_quicktestmain_missingfile
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_transportseamguard|tst_appshell|tst_qml_.*|tst_quicktestmain_missingfile)$' --output-on-failure -j 8
```

Expected: all pass (QuickTests unchanged in content, now running through the factory).

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `feat(quick): install transport seam in WITSQuick and QuickTest harness`.

---

### Task 8: `AccessControlHub` obtains its manager from `HttpClient`

**Files:**
- Modify: `qt-app/quick/AccessControlHub.h` (line 14 forward decls, lines 34-35 ctor, lines 70-78 members)
- Modify: `qt-app/quick/AccessControlHub.cpp` (lines 1-30)
- Modify: `qt-app/quick/tests/tst_accesscontrolhub.cpp` (include block lines 1-8; 13 `AccessControlHub hub(&nam);` sites; the default `AccessControlHub hub;` at line 261)
- Modify: `qt-app/quick/tests/tst_appshell.cpp` (the hub declaration, as rewritten in Task 7)
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (`kPendingMigration`, `kPendingTestMigration`)
- Test: `tst_accesscontrolhub`, `tst_transportseamguard`

**Interfaces:**
- Consumes: `HttpClient(QObject*, QNetworkAccessManager*)`, `HttpClient::manager()`, `OfflineHttp` (Task 6b).
- Produces: `explicit AccessControlHub(QObject *parent = nullptr, HttpClient *http = nullptr);` — the locked owner-constructor order shared with every ViewModel (was `(QNetworkAccessManager *injectedNam = nullptr, QObject *parent = nullptr)`); `QNetworkAccessManager *AccessControlHub::networkManager() const`. Default path owns `std::unique_ptr<HttpClient> m_ownedClient` in the slot `m_ownedNam` occupied, so teardown stays service → factory → owned client (+ its manager) → bus. Production call sites are unaffected: `quick/main.cpp` and `QuickTestSetup.h` default-construct the hub.

- [ ] **Step 1: Write the failing change.** In `qt-app/quick/tests/tst_transportseamguard.cpp` delete the `QStringLiteral("quick/AccessControlHub.cpp"),` entry from `kPendingMigration` and the `QStringLiteral("quick/tests/tst_accesscontrolhub.cpp"),` entry from `kPendingTestMigration`. In `qt-app/quick/tests/tst_accesscontrolhub.cpp` add after line 7 (`#include "sequencednam.h"`):

```cpp
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

inject every `SequencedNam` through an `HttpClient` (13 sites; `http` is declared after `nam` and before `hub`, so it is destroyed after the hub and before `nam`), and make the never-initialized default hub at line 261 offline too. By-type audit on `master` (`grep -nE "AccessControlHub\s+\w+\s*[;({]|new\s+AccessControlHub" qt-app/quick/tests/*.cpp qt-app/tests/*.cpp`): every test construction uses the variable `hub` — 13 × `hub(&nam)` and 1 × `hub;` in this file, plus 1 × `hub;` in `tst_appshell.cpp:30` (handled below); each test function holds at most one hub, so the single name `http` cannot collide:

```bash
sed -i 's/^\(\s*\)AccessControlHub hub(&nam);/\1HttpClient http(nullptr, \&nam);\n\1AccessControlHub hub(nullptr, \&http);/' qt-app/quick/tests/tst_accesscontrolhub.cpp
sed -i 's/^\(\s*\)AccessControlHub hub;\(.*\)$/\1OfflineHttp hubHttp;\n\1AccessControlHub hub(nullptr, \&hubHttp.http);\2/' qt-app/quick/tests/tst_accesscontrolhub.cpp
grep -c "AccessControlHub hub(nullptr, &http);" qt-app/quick/tests/tst_accesscontrolhub.cpp          # expect 13
grep -c "AccessControlHub hub(nullptr, &hubHttp.http);" qt-app/quick/tests/tst_accesscontrolhub.cpp  # expect 1
grep -cE "AccessControlHub hub(\(&nam\))?;" qt-app/quick/tests/tst_accesscontrolhub.cpp              # expect 0
```

Also make `tst_appshell`'s hub offline (delete the `QStringLiteral("quick/tests/tst_appshell.cpp"),` entry from `kPendingTestMigration` too). In `qt-app/quick/tests/tst_appshell.cpp` (as rewritten in Task 7) add `#include "offlinehttp.h"` after `#include "PolicyNamFactory.h"` and replace

```cpp
    AccessControlHub hub;                 // default settings, no env -> disabled
```

with

```cpp
    OfflineHttp hubHttp;                  // the hub can never reach a backend in tests
    AccessControlHub hub(nullptr, &hubHttp.http);   // default settings, no env -> disabled
```

Only **after** running the seds above (so the new ownership test stays default-constructed — it is exempt from the guard by its `defaultHub` name and never initializes the hub), add `void defaultHubManagerIsSeamManager();` and `void injectedClientManagerReachesTheProvider();` to `private slots:`; and before `QTEST_MAIN`:

```cpp
void TestAccessControlHub::defaultHubManagerIsSeamManager()
{
    // Production path: the manager every TurnstileProvider receives is the
    // hub's own HttpClient's PolicyEnforcingNam (never initialize()d -> no polling).
    AccessControlHub hub;
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(hub.networkManager()));
}

void TestAccessControlHub::injectedClientManagerReachesTheProvider()
{
    SequencedNam nam;                        // unqueued requests: valid empty polls
    HttpClient http(nullptr, &nam);
    AccessControlHub hub(nullptr, &http);
    QCOMPARE(hub.networkManager(), static_cast<QNetworkAccessManager *>(&nam));
    hub.initialize();
    hub.setAccessEnabled(true);              // provider is created with hub.networkManager()
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() >= 1, 3000);
    QVERIFY(nam.lastUrl.path().endsWith(QStringLiteral("turnstile_display.php")));
}
```

- [ ] **Step 2: Run — expected FAIL** (compile: no `AccessControlHub(QObject*, HttpClient*)` ctor; guard: `quick/AccessControlHub.cpp` unexpected):

```bash
cmake --build C:/b/s1a --target tst_accesscontrolhub tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

Expected: `tst_accesscontrolhub` fails to compile (no `AccessControlHub(std::nullptr_t, HttpClient*)` constructor yet); `namConstructedOnlyInsideSeam` fails listing `quick/AccessControlHub.cpp`. (`ownersInTestsUseFakeManagers` already passes for this file because the sed removed its only default-constructed hub.)

- [ ] **Step 3: Implement.** In `qt-app/quick/AccessControlHub.h` replace line 14:

```cpp
class QNetworkAccessManager;
```

with:

```cpp
class HttpClient;
class QNetworkAccessManager;
```

replace lines 34-35:

```cpp
    explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr,
                              QObject *parent = nullptr);
```

with:

```cpp
    // http: test seam (an HttpClient wrapping SequencedNam / OfflineHttp).
    // Null in production: the hub owns an HttpClient (transport seam, S1 spec
    // §3). Same (parent, http) order as every ViewModel.
    explicit AccessControlHub(QObject *parent = nullptr, HttpClient *http = nullptr);

    // The client's manager — the one handed to every TurnstileProvider this
    // hub creates. Lets tests prove production wiring (PolicyEnforcingNam).
    QNetworkAccessManager *networkManager() const { return m_nam; }
```

and replace lines 70-78:

```cpp
    // Declaration order fixes teardown order: reverse destruction is
    // service -> factory -> owned NAM -> bus, so the service tears down the
    // provider (which aborts its reply) while the NAM is still alive.
    std::unique_ptr<AccessControl::EventBus> m_bus;
    std::unique_ptr<QNetworkAccessManager> m_ownedNam;   // only when self-created
    AccessControl::AccessProviderFactory m_factory;      // plain value member
    std::unique_ptr<AccessControl::AccessControlService> m_service;

    QNetworkAccessManager *m_nam = nullptr;   // owned-or-injected; non-owning ptr
```

with:

```cpp
    // Declaration order fixes teardown order: reverse destruction is
    // service -> factory -> owned client (and its manager) -> bus, so the
    // service tears down the provider (which aborts its reply) while the
    // manager is still alive.
    std::unique_ptr<AccessControl::EventBus> m_bus;
    std::unique_ptr<HttpClient> m_ownedClient;            // only when self-created
    AccessControl::AccessProviderFactory m_factory;      // plain value member
    std::unique_ptr<AccessControl::AccessControlService> m_service;

    QNetworkAccessManager *m_nam = nullptr;   // the client's manager; non-owning ptr
```

In `qt-app/quick/AccessControlHub.cpp` replace lines 1-30 with:

```cpp
#include "AccessControlHub.h"

#include <QNetworkAccessManager>

#include "apiconfig.h"
#include "appsettings.h"
#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/contactage.h"
#include "accesscontrol/eventbus.h"
#include "accesscontrol/healthmonitor.h"
#include "accesscontrol/turnstileprovider.h"
#include "transport/httpclient.h"

using namespace AccessControl;

namespace { AccessControlHub *g_instance = nullptr; }

AccessControlHub *AccessControlHub::instance() { return g_instance; }
void AccessControlHub::setInstance(AccessControlHub *hub) { g_instance = hub; }

AccessControlHub::AccessControlHub(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_bus(std::make_unique<EventBus>())
    , m_service(std::make_unique<AccessControlService>(m_bus.get(), &m_factory))
{
    if (http) {
        m_nam = http->manager();             // externally owned; must outlive the hub
    } else {
        m_ownedClient = std::make_unique<HttpClient>();
        m_nam = m_ownedClient->manager();
    }
```

(The rest of the file — from `connect(m_bus.get(), ...` onward — is unchanged.)

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake --build C:/b/s1a --target WITSQuick tst_accesscontrolhub tst_transportseamguard tst_appshell
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_accesscontrolhub|tst_transportseamguard|tst_appshell|tst_qml_kiosk|tst_qml_accesscontrol)$' --output-on-failure -j 4
```

Expected: all pass; every pre-existing hub assertion (cursor, reconnect, enable-lock, base URL capture) unchanged.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `refactor(quick): obtain AccessControlHub manager from HttpClient`.

---

### Task 9: Dashboard, VisitLogs and AccessControl ViewModels through `HttpClient`

**Files:**
- Modify: `qt-app/quick/viewmodels/DashboardViewModel.h` (line 10, line 30, line 69), `DashboardViewModel.cpp` (lines 1-15)
- Modify: `qt-app/quick/viewmodels/VisitLogsViewModel.h` (line 11, lines 34-37, line 88), `VisitLogsViewModel.cpp` (lines 1-22)
- Modify: `qt-app/quick/viewmodels/AccessControlViewModel.h` (line 10, lines 43-45, line 84), `AccessControlViewModel.cpp` (lines 1-18)
- Modify: `qt-app/quick/tests/tst_dashboardviewmodel.cpp`, `tst_visitlogsviewmodel.cpp`, `tst_accesscontrolviewmodel.cpp`
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (`kPendingMigration`, `kPendingTestMigration`)

**Interfaces:**
- Consumes: `HttpClient`, `PolicyEnforcingNam` (for `findChild` assertions), `CapturingNam`, `OfflineHttp`.
- Produces:
  - `explicit DashboardViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`
  - `explicit VisitLogsViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`
  - `explicit AccessControlViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`
  - each with private `HttpClient *m_http = nullptr;` declared immediately before the unchanged `QNetworkAccessManager *m_nam` (now `m_http->manager()`, non-owning).

- [ ] **Step 1: Write the failing tests.**

Guard: delete the three entries `AccessControlViewModel.cpp`, `DashboardViewModel.cpp`, `VisitLogsViewModel.cpp` from `kPendingMigration`, and the three entries `quick/tests/tst_accesscontrolviewmodel.cpp`, `quick/tests/tst_dashboardviewmodel.cpp`, `quick/tests/tst_visitlogsviewmodel.cpp` from `kPendingTestMigration`.

**Safety injection first** — every default-constructed VM in these files gets an `OfflineHttp` (Task 6b) so no test can reach a backend (run this before adding the new ownership tests below, which must stay default-constructed):

```bash
for f in dashboardviewmodel:DashboardViewModel visitlogsviewmodel:VisitLogsViewModel accesscontrolviewmodel:AccessControlViewModel; do
  t=${f%%:*}; c=${f##*:}
  sed -i "s/^\(\s*\)$c \([A-Za-z_][A-Za-z0-9_]*\);/\1OfflineHttp \2Http;\n\1$c \2(nullptr, \&\2Http.http);/" qt-app/quick/tests/tst_$t.cpp
  echo "$t: $(grep -c 'Http.http);' qt-app/quick/tests/tst_$t.cpp) offline, $(grep -cE "^\s*$c \w+;" qt-app/quick/tests/tst_$t.cpp) default left"
done
```

Expected: dashboard `4 offline, 0 default left`; visitlogs `8 offline, 0 default left`; accesscontrol `8 offline, 0 default left`.

Then route every existing fake-manager construction through an `HttpClient`. The match is **by class name, for any variable name and any manager variable** (audit on `master`, `grep -nE "(Dashboard|VisitLogs|AccessControl)ViewModel \w+\(nullptr, &\w+\);" qt-app/quick/tests/*.cpp`: dashboard `vm`×2 at 91, 107; visit logs `vm`×2 at 183, 199; access control `vm`×10, `authVm` at 207, `vm2` at 341 = 12). Each site gets its own client named after the ViewModel variable (`vmClient`, `authVmClient`, `vm2Client`), because `vm` (line 190) and `authVm` (line 207) share one function and one `nam`:

```bash
for f in dashboardviewmodel:DashboardViewModel visitlogsviewmodel:VisitLogsViewModel accesscontrolviewmodel:AccessControlViewModel; do
  t=${f%%:*}; c=${f##*:}
  sed -i "s/^\(\s*\)$c \([A-Za-z_][A-Za-z0-9_]*\)(nullptr, &\([A-Za-z_][A-Za-z0-9_]*\));/\1HttpClient \2Client(nullptr, \&\3);\n\1$c \2(nullptr, \&\2Client);/" qt-app/quick/tests/tst_$t.cpp
  routed=$(grep -cE "$c \w+\(nullptr, &\w+Client\);" qt-app/quick/tests/tst_$t.cpp || true)
  unrouted=$(grep -E "$c \w+\(nullptr, &\w+\);" qt-app/quick/tests/tst_$t.cpp | grep -vcE "&\w+Client\);" || true)
  echo "$t: $routed routed, $unrouted unrouted"
done
grep -nE "(Dashboard|VisitLogs|AccessControl)ViewModel \w+\(nullptr, &\w+\);" qt-app/quick/tests/*.cpp | grep -vE "&\w+(Client|Http\.http)\);"   # expect: no output
```

Expected: dashboard `2 routed, 0 unrouted`; visitlogs `2 routed, 0 unrouted`; accesscontrol `12 routed, 0 unrouted`; the final `grep` prints nothing (trailing comments such as `// mode defaults to Student` at `tst_visitlogsviewmodel.cpp:183` stay on the rewritten ViewModel line).

In each of the three test files add after the `#include "capturingnam.h"` line:

```cpp
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

add `void defaultConstructionUsesTransportSeam();` to the class's `private slots:` list, and add before `QTEST_MAIN` (shown for Dashboard; use `VisitLogsViewModel` / `AccessControlViewModel` and the file's own test class name `TestVisitLogsViewModel` / `TestAccessControlViewModel` in the other two files):

```cpp
void TestDashboardViewModel::defaultConstructionUsesTransportSeam()
{
    // Production path (QML default construction): the VM owns an HttpClient
    // whose manager is a PolicyEnforcingNam — never a raw QNetworkAccessManager.
    DashboardViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);

    // Injected path: the test's manager is used; nothing is created.
    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    DashboardViewModel injected(nullptr, &http);
    QVERIFY(injected.findChildren<PolicyEnforcingNam *>().isEmpty());
}
```

```cpp
void TestVisitLogsViewModel::defaultConstructionUsesTransportSeam()
{
    VisitLogsViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);

    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    VisitLogsViewModel injected(nullptr, &http);
    QVERIFY(injected.findChildren<PolicyEnforcingNam *>().isEmpty());
}
```

```cpp
void TestAccessControlViewModel::defaultConstructionUsesTransportSeam()
{
    AccessControlViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);

    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    AccessControlViewModel injected(nullptr, &http);
    QVERIFY(injected.findChildren<PolicyEnforcingNam *>().isEmpty());
}
```

- [ ] **Step 2: Run — expected FAIL** (compile: ctor takes `QNetworkAccessManager*`; guard lists the three files):

```bash
cmake --build C:/b/s1a --target tst_dashboardviewmodel tst_visitlogsviewmodel tst_accesscontrolviewmodel tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

- [ ] **Step 3: Implement.**

`DashboardViewModel.h`: after line 10 (`class QNetworkAccessManager;`) add `class HttpClient;`; replace line 30 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit DashboardViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 69 (`QNetworkAccessManager *m_nam = nullptr;`) with

```cpp
    HttpClient *m_http = nullptr;              // injected, or an owned child
    QNetworkAccessManager *m_nam = nullptr;    // m_http->manager(); non-owning
```

`DashboardViewModel.cpp`: after line 9 (`#include "dashboardparser.h"`) add `#include "transport/httpclient.h"`; replace lines 11-15 with

```cpp
DashboardViewModel::DashboardViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
{
}
```

`VisitLogsViewModel.h`: after line 11 add `class HttpClient;`; replace lines 34-37 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam) —
    // when null (production, and every existing QML default-construction
    // path), the VM owns an HttpClient (transport seam).
    explicit VisitLogsViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 88 with the same two member lines as Dashboard.

`VisitLogsViewModel.cpp`: after line 15 (`#include "visitordata.h"`) add `#include "transport/httpclient.h"`; replace lines 17-22 with

```cpp
VisitLogsViewModel::VisitLogsViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
{
    m_rangeLabel = computeRangeLabel();
}
```

`AccessControlViewModel.h`: after line 10 add `class HttpClient;`; replace lines 43-45 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit AccessControlViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 84 with the same two member lines as Dashboard.

`AccessControlViewModel.cpp`: after line 12 (`#include "loginparser.h"`) add `#include "transport/httpclient.h"`; replace lines 14-18 with

```cpp
AccessControlViewModel::AccessControlViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
{
}
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake --build C:/b/s1a --target WITSQuick tst_dashboardviewmodel tst_visitlogsviewmodel tst_accesscontrolviewmodel tst_transportseamguard tst_qml_admin tst_qml_adminshell tst_qml_accesscontrol
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_dashboardviewmodel|tst_visitlogsviewmodel|tst_accesscontrolviewmodel|tst_transportseamguard|tst_qml_admin|tst_qml_adminshell|tst_qml_accesscontrol)$' --output-on-failure -j 4
```

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `refactor(quick): route dashboard, visit-log and access-control VMs through HttpClient`.

---

### Task 10: Kiosk and Guest ViewModels through `HttpClient`

**Files:**
- Modify: `qt-app/quick/viewmodels/KioskViewModel.h` (line 14, line 66, line 151), `KioskViewModel.cpp` (lines 16-22)
- Modify: `qt-app/quick/viewmodels/GuestViewModel.h` (line 9, line 17, line 33), `GuestViewModel.cpp` (lines 1-13)
- Modify: `qt-app/quick/tests/tst_kioskviewmodel.cpp`, `qt-app/quick/tests/tst_guestviewmodel.cpp`
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (`kPendingMigration`, `kPendingTestMigration`)
- (No CMake change: Task 6b already registers both tests with `${WITS_TEST_NAM_SOURCES}` and the `testsupport` include dir.)

**Interfaces:**
- Consumes: `HttpClient`, `PolicyEnforcingNam`, `CapturingNam`, `OfflineHttp`, `ApiConfig::endpoint`.
- Produces: `explicit KioskViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`, `explicit GuestViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);` (+ `m_http` members as in Task 9).

- [ ] **Step 1: Write the failing tests.**

Guard: delete `GuestViewModel.cpp` and `KioskViewModel.cpp` from `kPendingMigration`, and `quick/tests/tst_guestviewmodel.cpp`, `quick/tests/tst_kioskviewmodel.cpp` from `kPendingTestMigration`.

**Safety injection first** (before adding the new ownership tests below):

```bash
for f in kioskviewmodel:KioskViewModel guestviewmodel:GuestViewModel; do
  t=${f%%:*}; c=${f##*:}
  sed -i "s/^\(\s*\)$c \([A-Za-z_][A-Za-z0-9_]*\);/\1OfflineHttp \2Http;\n\1$c \2(nullptr, \&\2Http.http);/" qt-app/quick/tests/tst_$t.cpp
  echo "$t: $(grep -c 'Http.http);' qt-app/quick/tests/tst_$t.cpp) offline, $(grep -cE "^\s*$c \w+;" qt-app/quick/tests/tst_$t.cpp) default left"
done
```

Expected: kiosk `18 offline, 0 default left`; guest `3 offline, 0 default left`.

`tst_kioskviewmodel.cpp`: after line 11 (`#include "appsettings.h"`) add

```cpp
#include <QUrlQuery>
#include "apiconfig.h"
#include "capturingnam.h"
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

add `void defaultConstructionUsesTransportSeam();` and `void studentLoginGoesThroughInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestKioskViewModel::defaultConstructionUsesTransportSeam()
{
    KioskViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
}

void TestKioskViewModel::studentLoginGoesThroughInjectedClient()
{
    CapturingNam nam(QByteArrayLiteral(
        "{\"status\":\"success\",\"student\":{\"name\":\"Test Student A\",\"course\":\"BSIT\"}}"));
    HttpClient http(nullptr, &nam);
    KioskViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());

    vm.submitLogin(QStringLiteral("20260001"));
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("student_login.php")));
    QCOMPARE(QUrlQuery(QString::fromUtf8(nam.lastBody)).queryItemValue(QStringLiteral("school_id")),
             QStringLiteral("20260001"));
    QTRY_VERIFY(vm.hasStudent());   // reply decoded through the production path
    QCOMPARE(vm.currentFullName(), QStringLiteral("Test Student A"));
}
```

`tst_guestviewmodel.cpp`: after line 3 (`#include "GuestViewModel.h"`) add

```cpp
#include <QUrlQuery>
#include "apiconfig.h"
#include "capturingnam.h"
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

add `void defaultConstructionUsesTransportSeam();` and `void guestSubmitGoesThroughInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestGuestViewModel::defaultConstructionUsesTransportSeam()
{
    GuestViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
}

void TestGuestViewModel::guestSubmitGoesThroughInjectedClient()
{
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"success\",\"message\":\"Welcome\"}"));
    HttpClient http(nullptr, &nam);
    GuestViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    QSignalSpy ok(&vm, &GuestViewModel::guestSucceeded);

    vm.submitGuest(QStringLiteral("Test Guest"), QStringLiteral("0000"),
                   QStringLiteral("Example Org"), QStringLiteral("Visit"));
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("guest_login.php")));
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    const QUrlQuery body(QString::fromUtf8(nam.lastBody));
    QCOMPARE(body.queryItemValue(QStringLiteral("name")), QStringLiteral("Test Guest"));
    QCOMPARE(body.queryItemValue(QStringLiteral("company")), QStringLiteral("Example Org"));
    QTRY_COMPARE(ok.count(), 1);
}
```

- [ ] **Step 2: Run — expected FAIL** (compile: no `(QObject*, HttpClient*)` ctor; guard lists the two files):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_kioskviewmodel tst_guestviewmodel tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

- [ ] **Step 3: Implement.**

`KioskViewModel.h`: after line 14 (`class QNetworkAccessManager;`) add `class HttpClient;`; replace line 66 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit KioskViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 151 (`QNetworkAccessManager *m_nam = nullptr;`) with

```cpp
    HttpClient *m_http = nullptr;              // injected, or an owned child
    QNetworkAccessManager *m_nam = nullptr;    // m_http->manager(); non-owning
```

`KioskViewModel.cpp`: after line 16 (`#include "Initials.h"`) add `#include "transport/httpclient.h"`; replace lines 18-21 with

```cpp
KioskViewModel::KioskViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
    , m_clockTimer(new QTimer(this))
```

(the `{` on line 22 and the body stay unchanged).

`GuestViewModel.h`: after line 9 add `class HttpClient;`; replace line 17 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit GuestViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 33 with the two member lines above.

`GuestViewModel.cpp`: replace lines 1-13 with

```cpp
#include "GuestViewModel.h"

#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include "apiconfig.h"
#include "HttpForm.h"
#include "transport/httpclient.h"

GuestViewModel::GuestViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
{
}
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake --build C:/b/s1a --target WITSQuick tst_kioskviewmodel tst_guestviewmodel tst_transportseamguard tst_qml_kiosk
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_kioskviewmodel|tst_guestviewmodel|tst_transportseamguard|tst_qml_kiosk)$' --output-on-failure -j 4
```

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `refactor(quick): route kiosk and guest VMs through HttpClient`.

---

### Task 11: Search and Database ViewModels through `HttpClient`

**Files:**
- Modify: `qt-app/quick/viewmodels/SearchViewModel.h` (line 11, line 28, line 93), `SearchViewModel.cpp` (lines 1-10)
- Modify: `qt-app/quick/viewmodels/DatabaseViewModel.h` (line 12, line 79, lines 273 and 285), `DatabaseViewModel.cpp` (lines 1-24)
- Modify: `qt-app/quick/tests/tst_searchviewmodel.cpp`, `qt-app/quick/tests/tst_databaseviewmodel.cpp`
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (`kPendingMigration`, `kPendingTestMigration`)
- (No CMake change: Task 6b already registers both tests with `${WITS_TEST_NAM_SOURCES}`.)

**Interfaces:**
- Consumes: `HttpClient`, `PolicyEnforcingNam`, `CapturingNam`, `OfflineHttp`, `StudentController(QNetworkAccessManager*, QObject*)` + `networkManager()` (Task 4b).
- Produces:
  - `explicit SearchViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`
  - `explicit DatabaseViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);` — owns **one** `HttpClient` (injected or an owned child), and **both** of its `StudentController`s (table `m_controller`, edit `m_editController`) receive that client's manager (locked decision: one client per owner).

**DatabaseViewModel single-client confirmation (from the code on `master`):** today `DatabaseViewModel.cpp:13` and `:23` create two managers, but nothing depends on them being separate: `m_editNam` is referenced only at construction (`DatabaseViewModel.cpp:23-24`); neither the VM nor `StudentController` ever calls `abort()`, `clearConnectionCache()`, `clearAccessCache()` or `setTransferTimeout()` on a manager, or connects to any `QNetworkAccessManager` signal (`finished`, `authenticationRequired`, `sslErrors`) — every reply is handled through its own `QNetworkReply::finished` connection with the controller as context; and both managers were children of the VM with the same lifetime. Sharing one manager therefore changes only HTTP connection pooling (one per-host pool instead of two), which has no functional effect for these sequential admin requests. `m_editNam` is removed; `m_nam` serves both controllers.

- [ ] **Step 1: Write the failing tests.**

Guard: delete `DatabaseViewModel.cpp` and `SearchViewModel.cpp` from `kPendingMigration`, and `quick/tests/tst_databaseviewmodel.cpp`, `quick/tests/tst_searchviewmodel.cpp` from `kPendingTestMigration`.

**Safety injection first** (before adding the new ownership tests below). The Database suite drives delete, edit, bulk edit, register and department deactivate/delete paths (e.g. around `tst_databaseviewmodel.cpp:154, 445, 770, 786, 801`) — after this, none of them can reach a backend:

```bash
for f in searchviewmodel:SearchViewModel databaseviewmodel:DatabaseViewModel; do
  t=${f%%:*}; c=${f##*:}
  sed -i "s/^\(\s*\)$c \([A-Za-z_][A-Za-z0-9_]*\);/\1OfflineHttp \2Http;\n\1$c \2(nullptr, \&\2Http.http);/" qt-app/quick/tests/tst_$t.cpp
  echo "$t: $(grep -c 'Http.http);' qt-app/quick/tests/tst_$t.cpp) offline, $(grep -cE "^\s*$c \w+;" qt-app/quick/tests/tst_$t.cpp) default left"
done
```

Expected: search `9 offline, 0 default left`; database `63 offline, 0 default left` (incl. the one-line `del2` / `bulk2` constructions at lines 423-424, whose trailing statements stay on the rewritten line).

`tst_searchviewmodel.cpp`: after line 5 (`#include "studentdata.h"`) add

```cpp
#include "apiconfig.h"
#include "capturingnam.h"
#include "offlinehttp.h"
#include "studentcontroller.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

add `void defaultConstructionUsesTransportSeam();` and `void searchGoesThroughInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestSearchViewModel::defaultConstructionUsesTransportSeam()
{
    SearchViewModel vm;
    auto *http = vm.findChild<HttpClient *>();
    QVERIFY(http);
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
    // The controller's manager IS the VM's HttpClient manager (no bypass).
    auto *controller = vm.findChild<StudentController *>();
    QVERIFY(controller);
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(controller->networkManager()));
    QCOMPARE(controller->networkManager(), http->manager());
}

void TestSearchViewModel::searchGoesThroughInjectedClient()
{
    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    SearchViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    QCOMPARE(vm.findChild<StudentController *>()->networkManager(),
             static_cast<QNetworkAccessManager *>(&nam));
    vm.search(QStringLiteral("Test Student"), QString());
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("search_students.php")));
}
```

`tst_databaseviewmodel.cpp`: after line 11 (`#include "studentcontroller.h"`) add

```cpp
#include "apiconfig.h"
#include "capturingnam.h"
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

add `void defaultConstructionUsesOneSeamManager();` and `void bothControllersUseInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestDatabaseViewModel::defaultConstructionUsesOneSeamManager()
{
    // One client per owner: the VM owns exactly one HttpClient, and both
    // StudentControllers (table + edit) hold that client's PolicyEnforcingNam.
    DatabaseViewModel vm;
    const QList<HttpClient *> clients = vm.findChildren<HttpClient *>();
    QCOMPARE(clients.size(), 1);
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
    QNetworkAccessManager *seamManager = clients.first()->manager();
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(seamManager));

    const QList<StudentController *> controllers = vm.findChildren<StudentController *>();
    QCOMPARE(controllers.size(), 2);
    for (StudentController *c : controllers)
        QCOMPARE(c->networkManager(), seamManager);
}

void TestDatabaseViewModel::bothControllersUseInjectedClient()
{
    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    DatabaseViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    const QList<StudentController *> controllers = vm.findChildren<StudentController *>();
    QCOMPARE(controllers.size(), 2);
    for (StudentController *c : controllers)
        QCOMPARE(c->networkManager(), static_cast<QNetworkAccessManager *>(&nam));

    vm.reloadTable();                                   // table controller
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("search_students.php")));
    vm.setEditDepartment(QStringLiteral("CCS"));        // edit controller
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("get_courses_by_department.php")));
}
```

- [ ] **Step 2: Run — expected FAIL:**

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_searchviewmodel tst_databaseviewmodel tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

Expected: both VM tests fail to compile (no `(QObject*, HttpClient*)` ctor); guard lists the two files.

- [ ] **Step 3: Implement.**

`SearchViewModel.h`: after line 11 add `class HttpClient;`; replace line 28 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit SearchViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 93 with

```cpp
    HttpClient *m_http = nullptr;              // injected, or an owned child
    QNetworkAccessManager *m_nam = nullptr;    // m_http->manager(); non-owning
```

`SearchViewModel.cpp`: replace lines 1-10 with

```cpp
#include "SearchViewModel.h"

#include <QNetworkAccessManager>
#include "studentcontroller.h"
#include "AdminSession.h"
#include "transport/httpclient.h"

SearchViewModel::SearchViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
    , m_controller(new StudentController(m_nam, this))
```

`DatabaseViewModel.h`: after line 12 add `class HttpClient;`; replace line 79 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam /
    // OfflineHttp). Null in production: the VM owns ONE HttpClient (transport
    // seam); the table and the edit StudentController both use its manager.
    explicit DatabaseViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

replace line 273 (`QNetworkAccessManager *m_nam = nullptr;`) with

```cpp
    HttpClient *m_http = nullptr;              // injected, or an owned child
    QNetworkAccessManager *m_nam = nullptr;    // m_http->manager(); non-owning; both controllers
```

and delete line 285 (`QNetworkAccessManager *m_editNam = nullptr;`) — the edit controller now uses `m_nam` (see the single-client confirmation above).

`DatabaseViewModel.cpp`: after line 9 (`#include "SettingsViewModel.h"`) add `#include "transport/httpclient.h"`; replace lines 11-14 with

```cpp
DatabaseViewModel::DatabaseViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
    , m_controller(new StudentController(m_nam, this))
```

and replace lines 23-24 with

```cpp
    // Same seam manager as the table controller: one client per owner.
    m_editController = new StudentController(m_nam, this);
```

Confirm no other reference to the removed member remains:

```bash
grep -n "m_editNam" qt-app/quick/viewmodels/DatabaseViewModel.h qt-app/quick/viewmodels/DatabaseViewModel.cpp   # expect: no output
```

- [ ] **Step 4: Run — expected PASS:**

```bash
cmake --build C:/b/s1a --target WITSQuick tst_searchviewmodel tst_databaseviewmodel tst_transportseamguard tst_qml_admin
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_searchviewmodel|tst_databaseviewmodel|tst_transportseamguard|tst_qml_admin)$' --output-on-failure -j 4
```

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `refactor(quick): route search and database VMs through HttpClient`.

---

### Task 12: Import, Reporting and Settings ViewModels through `HttpClient` (pending list emptied)

**Files:**
- Modify: `qt-app/quick/viewmodels/ImportViewModel.h` (lines 11, 13-16 class comment, 25, 95), `ImportViewModel.cpp` (lines 1-14)
- Modify: `qt-app/quick/viewmodels/ReportingViewModel.h` (lines 20, 84, 261), `ReportingViewModel.cpp` (lines 18-23)
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h` (lines 13, 39, 193), `SettingsViewModel.cpp` (lines 23-28)
- Modify: `qt-app/quick/tests/tst_importviewmodel.cpp`, `tst_reportingviewmodel.cpp`, `tst_settingsviewmodel.cpp`
- Modify: `qt-app/quick/tests/tst_transportseamguard.cpp` (`kPendingMigration` and `kPendingTestMigration` → empty)

**Interfaces:**
- Consumes: `HttpClient`, `PolicyEnforcingNam`, `CapturingNam`, `ImportController` / `ReportController` (unchanged).
- Produces: `explicit ImportViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`, `explicit ReportingViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`, `explicit SettingsViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);`. After this task `kPendingMigration` is `{}`.

- [ ] **Step 1: Write the failing tests.**

Guard: replace the remaining `kPendingMigration` body with an empty list:

```cpp
const QStringList kPendingMigration = {
};
```

and likewise empty `kPendingTestMigration` (its last three entries — `tst_importviewmodel.cpp`, `tst_reportingviewmodel.cpp`, `tst_settingsviewmodel.cpp` — are migrated now):

```cpp
const QStringList kPendingTestMigration = {
};
```

**Safety injection first** (before adding the new ownership tests below; no CMake change — Task 6b already registers these tests with `${WITS_TEST_NAM_SOURCES}`). The Settings suite reaches admin-key change, admin-info save and visit-reset paths; after this none can reach a backend:

```bash
for f in importviewmodel:ImportViewModel reportingviewmodel:ReportingViewModel settingsviewmodel:SettingsViewModel; do
  t=${f%%:*}; c=${f##*:}
  sed -i "s/^\(\s*\)$c \([A-Za-z_][A-Za-z0-9_]*\);/\1OfflineHttp \2Http;\n\1$c \2(nullptr, \&\2Http.http);/" qt-app/quick/tests/tst_$t.cpp
  echo "$t: $(grep -c 'Http.http);' qt-app/quick/tests/tst_$t.cpp) offline, $(grep -cE "^\s*$c \w+;" qt-app/quick/tests/tst_$t.cpp) default left"
done
```

Expected: import `5 offline, 0 default left`; reporting `57 offline, 0 default left`; settings `38 offline, 0 default left`.

In each of `tst_importviewmodel.cpp` (after line 7 `#include "AdminSession.h"`), `tst_reportingviewmodel.cpp` (after line 11 `#include "ReportingViewModel.h"`) and `tst_settingsviewmodel.cpp` (after line 15 `#include "appsettings.h"`) add:

```cpp
#include "apiconfig.h"
#include "capturingnam.h"
#include "offlinehttp.h"
#include "transport/httpclient.h"
#include "transport/policyenforcingnam.h"
```

and additionally `#include "importcontroller.h"` in `tst_importviewmodel.cpp` and `#include "reportcontroller.h"` in `tst_reportingviewmodel.cpp` (for the controller-identity assertions).

`tst_importviewmodel.cpp` — add `void defaultConstructionUsesTransportSeam();` and `void duplicateCheckGoesThroughInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestImportViewModel::defaultConstructionUsesTransportSeam()
{
    ImportViewModel vm;
    auto *http = vm.findChild<HttpClient *>();
    QVERIFY(http);
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
    auto *controller = vm.findChild<ImportController *>();
    QVERIFY(controller);
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(controller->networkManager()));
    QCOMPARE(controller->networkManager(), http->manager());
}

void TestImportViewModel::duplicateCheckGoesThroughInjectedClient()
{
    QTemporaryDir dir;
    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    ImportViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    QCOMPARE(vm.findChild<ImportController *>()->networkManager(),
             static_cast<QNetworkAccessManager *>(&nam));
    vm.setDataFile(writeCsv(dir, QStringLiteral("in.csv"),
        QStringLiteral("School ID,Full Name\n21-1-0001,Test Student A\n")));
    vm.startImport();
    QCOMPARE(vm.phase(), ImportViewModel::Phase::CheckingDuplicates);
    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("check_duplicates.php")));
}
```

`tst_reportingviewmodel.cpp` — add `void defaultConstructionUsesTransportSeam();` and `void bootstrapGoesThroughInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestReportingViewModel::defaultConstructionUsesTransportSeam()
{
    ReportingViewModel vm;
    auto *http = vm.findChild<HttpClient *>();
    QVERIFY(http);
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
    auto *controller = vm.findChild<ReportController *>();
    QVERIFY(controller);
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(controller->networkManager()));
    QCOMPARE(controller->networkManager(), http->manager());
}

void TestReportingViewModel::bootstrapGoesThroughInjectedClient()
{
    CapturingNam nam;
    HttpClient http(nullptr, &nam);
    ReportingViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    QCOMPARE(vm.findChild<ReportController *>()->networkManager(),
             static_cast<QNetworkAccessManager *>(&nam));
    vm.loadDepartments();                               // departments, then years
    QCOMPARE(nam.lastOp, QNetworkAccessManager::GetOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("get_years.php")));
}
```

`tst_settingsviewmodel.cpp` — add `void defaultConstructionUsesTransportSeam();` and `void departmentsLoadGoesThroughInjectedClient();` to `private slots:`, and before `QTEST_MAIN`:

```cpp
void TestSettingsViewModel::defaultConstructionUsesTransportSeam()
{
    SettingsViewModel vm;
    QVERIFY(vm.findChild<HttpClient *>());
    QCOMPARE(vm.findChildren<PolicyEnforcingNam *>().size(), 1);
}

void TestSettingsViewModel::departmentsLoadGoesThroughInjectedClient()
{
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"success\",\"departments\":[]}"));
    HttpClient http(nullptr, &nam);
    SettingsViewModel vm(nullptr, &http);
    QVERIFY(vm.findChildren<PolicyEnforcingNam *>().isEmpty());
    vm.loadDepartments();
    QCOMPARE(nam.lastOp, QNetworkAccessManager::GetOperation);
    QCOMPARE(nam.lastUrl, ApiConfig::endpoint(QStringLiteral("get_departments.php")));
}
```

- [ ] **Step 2: Run — expected FAIL** (compile errors for the three VM tests; guard lists `ImportViewModel.cpp, ReportingViewModel.cpp, SettingsViewModel.cpp`):

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_importviewmodel tst_reportingviewmodel tst_settingsviewmodel tst_transportseamguard
ctest --test-dir C:/b/s1a -R '^tst_transportseamguard$' --output-on-failure
```

- [ ] **Step 3: Implement.**

`ImportViewModel.h`: after line 11 add `class HttpClient;`; in the class comment (lines 13-16) replace `Owns an ImportController + its own QNetworkAccessManager` with `Owns an ImportController + its own HttpClient (transport seam)`; replace line 25 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit ImportViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 95 with

```cpp
    HttpClient *m_http = nullptr;              // injected, or an owned child
    QNetworkAccessManager *m_nam = nullptr;    // m_http->manager(); non-owning
```

`ImportViewModel.cpp`: after line 9 (`#include "SettingsViewModel.h"`) add `#include "transport/httpclient.h"`; replace lines 11-14 with

```cpp
ImportViewModel::ImportViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
    , m_controller(new ImportController(m_nam, this))
```

`ReportingViewModel.h`: after line 20 add `class HttpClient;`; replace line 84 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit ReportingViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 261 with the two member lines above.

`ReportingViewModel.cpp`: after line 18 (`#include "xlsxdocument.h"`) add `#include "transport/httpclient.h"`; replace lines 20-23 with

```cpp
ReportingViewModel::ReportingViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
    , m_controller(new ReportController(m_nam, this))
```

`SettingsViewModel.h`: after line 13 add `class HttpClient;`; replace line 39 with

```cpp
    // http: injection seam for tests (HttpClient wrapping a CapturingNam).
    // Null in production: the VM owns an HttpClient (transport seam).
    explicit SettingsViewModel(QObject *parent = nullptr, HttpClient *http = nullptr);
```

and replace line 193 (`QNetworkAccessManager *m_nam = nullptr;`, directly after `SettingsController m_controller;`) with the two member lines above.

`SettingsViewModel.cpp`: after line 23 (`#include "reportcontroller.h"`) add `#include "transport/httpclient.h"`; replace lines 25-28 with

```cpp
SettingsViewModel::SettingsViewModel(QObject *parent, HttpClient *http)
    : QObject(parent)
    , m_controller(this)
    , m_http(http ? http : new HttpClient(this))
    , m_nam(m_http->manager())
```

- [ ] **Step 4: Run — expected PASS, and the seam is complete:**

```bash
cmake --build C:/b/s1a --target WITSQuick tst_importviewmodel tst_reportingviewmodel tst_settingsviewmodel tst_transportseamguard tst_qml_admin
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^(tst_importviewmodel|tst_reportingviewmodel|tst_settingsviewmodel|tst_transportseamguard|tst_qml_admin)$' --output-on-failure -j 4
grep -rnE "new\s+QNetworkAccessManager|make_(unique|shared)\s*<\s*QNetworkAccessManager" qt-app/core qt-app/quick --include=*.cpp --include=*.h | grep -v "/build" | grep -v "quick/tests/"
```

Expected: tests pass; the final `grep` prints nothing.

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `refactor(quick): route import, reporting and settings VMs through HttpClient`.

---

### Task 13: Resource inventory — every loaded resource still loads through the QML factory

**Files:**
- Create: `qt-app/quick/tests/tst_qmlresourceinventory.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (register before the coverage-guard comment block, line 483)
- Test: `tst_qmlresourceinventory`

**Interfaces:**
- Consumes: `PolicyNamFactory::installOn`, `RecordingPolicyNamFactory` / `RequestLog` (Task 5b, `quick/tests/RecordingPolicyNam.h`), `OfflineHttp` (Task 6b), `TinyHttpServer` (Task 3), `LoginParser::parseEntryEvent(const QByteArray&, const QUrl&)`, `LoginParser::parseRfidResponse(const QByteArray&)`, `SearchResultsModel::setRecords` / `PhotoRole`, `SchoolInfoUtil::resolveLogoUrl(const QString&, bool*)`, `SettingsViewModel(QObject*, HttpClient*)` / `load()` / `logoUrl()`, `ApiConfig::setBaseUrl/resetBaseUrl`, QML `LAvatar` (`imageStatus`, `showInitials`), `LLogoCircle` (child `objectName: "logoImage"`).
- Produces: CTest `tst_qmlresourceinventory` covering inventory rows R1-R8 and R10 (R9 and R11 stay covered by the existing suites named in the inventory). Remote loads are proven by the **request log of the factory-made manager** (the same evidence as GATE G2), never by counting `create()` calls — Qt may serve a load from a manager it already has.

- [ ] **Step 1: Write the test.** Create `qt-app/quick/tests/tst_qmlresourceinventory.cpp`:

```cpp
#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QNetworkProxy>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QStandardPaths>
#include <memory>

#include "RecordingPolicyNam.h"
#include "SchoolInfoUtil.h"
#include "SearchResultsModel.h"
#include "SettingsViewModel.h"
#include "apiconfig.h"
#include "appsettings.h"
#include "loginparser.h"
#include "offlinehttp.h"
#include "studentdata.h"
#include "tinyhttpserver.h"
#include "transport/policyenforcingnam.h"

// S1a resource inventory (S1 spec §3 "Resource inventory": every resource
// actually loaded becomes a test). Each row of the plan's inventory table is
// loaded exactly as production does — URL built by the production C++ code,
// rendered by the production QML component — on an engine whose managers come
// from a (recording) PolicyNamFactory installed via installOn(), against the
// in-process loopback TinyHttpServer (127.0.0.1, ephemeral port; never XAMPP).
namespace {
constexpr int kImageReady = 1;   // QQuickImageBase::Ready (QML Image.Ready)
constexpr int kImageError = 3;   // QQuickImageBase::Error (QML Image.Error)

QByteArray pngBytes()
{
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}

QString qmlString(const QString &s)
{
    QString escaped = s;
    escaped.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    escaped.replace(QLatin1String("\""), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString writePng(const QString &fileName)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);   // test-mode AppDataLocation (AppSettings::isolateForTesting)
    const QString path = dir + QLatin1Char('/') + fileName;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(pngBytes());
    return path;
}
} // namespace

class TestQmlResourceInventory : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void bundledModuleQmlLoadsFromQrc();                      // R1 (module lookup)
    void everyBundledQmlFileCompilesFromQrc();                // R1 (all 43 files)
    void turnstilePhotoLoadsThroughFactory();                 // R2
    void loginPhotoUrlLoadsThroughFactory();                  // R3
    void searchAvatarLoadsThroughFactory();                   // R4
    void defaultSentinelAndEmptyPhotoNeverRequested();        // R5
    void missingRemotePhotoFallsBackToInitials();             // R6
    void importedLogoFileUrlLoads();                          // R7
    void settingsLogoPreviewLoads();                          // R8
    void posterIsNotDisplayedByAnyQml();                      // R10

private:
    std::unique_ptr<QQuickView> makeView();
    static QQuickItem *create(QQuickView &view, const QString &qml);
    void expectRemoteAvatarLoads(const QString &photoUrl, const QByteArray &path);

    TinyHttpServer m_server;
    RequestLog m_log;                               // every request through a factory-made manager
    RecordingPolicyNamFactory m_factory{&m_log};    // outlives every view/engine created below
};

void TestQmlResourceInventory::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    QVERIFY(m_server.listen());
    TinyHttpServer::Response png;
    png.headers = {{"Content-Type", "image/png"}};
    png.body = pngBytes();
    m_server.route("/loams_api/uploads/students/s1.png", png);
    m_server.route("/loams_api/uploads/students/s2.png", png);
    m_server.route("/loams_api/uploads/students/s3.png", png);
    // Routed on purpose: R5 proves the sentinel is never REQUESTED, not that it 404s.
    m_server.route("/loams_api/uploads/default.jpg", png);
}

void TestQmlResourceInventory::cleanup()
{
    ApiConfig::resetBaseUrl();
}

std::unique_ptr<QQuickView> TestQmlResourceInventory::makeView()
{
    auto view = std::make_unique<QQuickView>();
    if (!PolicyNamFactory::installOn(*view->engine(), m_factory))   // before any manager exists
        qFatal("inventory: transport seam could not be installed on a fresh engine");
    view->engine()->addImportPath(QStringLiteral("qrc:/qt/qml"));
    view->resize(200, 200);
    view->show();
    return view;
}

QQuickItem *TestQmlResourceInventory::create(QQuickView &view, const QString &qml)
{
    QQmlComponent component(view.engine());
    component.setData(qml.toUtf8(), QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return nullptr;
    }
    auto *item = qobject_cast<QQuickItem *>(component.create());
    if (!item)
        return nullptr;
    item->setParent(view.contentItem());       // freed with the view
    item->setParentItem(view.contentItem());
    return item;
}

void TestQmlResourceInventory::expectRemoteAvatarLoads(const QString &photoUrl,
                                                       const QByteArray &path)
{
    auto view = makeView();
    // The engine's own manager is a seam manager...
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(view->engine()->networkAccessManager()));
    QQuickItem *avatar = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { size: 40; initials: \"TA\"; source: %1 }")
        .arg(qmlString(photoUrl)));
    QVERIFY(avatar);
    QTRY_COMPARE_WITH_TIMEOUT(avatar->property("imageStatus").toInt(), kImageReady, 5000);
    QVERIFY(!avatar->property("showInitials").toBool());
    QVERIFY(m_server.countFor(path) >= 1);
    // ...and the photo request itself passed through a factory-made
    // PolicyEnforcingNam (whichever thread issued it).
    QVERIFY2(m_log.contains(QUrl(photoUrl)),
             qPrintable(QStringLiteral("not fetched through the seam: ") + photoUrl));
}

void TestQmlResourceInventory::bundledModuleQmlLoadsFromQrc()
{
    auto view = makeView();
    QQmlComponent component(view->engine());
    component.loadFromModule("LOAMS", "LAvatar");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QCOMPARE(component.url().scheme(), QStringLiteral("qrc"));
}

void TestQmlResourceInventory::everyBundledQmlFileCompilesFromQrc()
{
    // R1 per file: every .qml in the source tree (quick/qml — the same 43 files
    // listed in qt_add_qml_module QML_FILES) is loaded from the module's qrc
    // copy and compiled by a seam-installed engine. Compile/load only:
    // instantiating whole screens (e.g. AdminScreen with autoLoad) could start
    // requests; live instantiation stays covered by tst_appshell (AppShell) and
    // the tst_qml_* suites. A source file missing from the module fails here.
    QVERIFY2(QFile::exists(QStringLiteral(":/qt/qml/LOAMS/qml/AppShell.qml")),
             "module resources are not under :/qt/qml/LOAMS/ — check qt_add_qml_module's resource prefix");
    auto view = makeView();
    const QDir srcRoot(QStringLiteral(SRC_QML_DIR));
    QDirIterator it(srcRoot.path(), {QStringLiteral("*.qml")}, QDir::Files,
                    QDirIterator::Subdirectories);
    QStringList failures;
    int compiled = 0;
    while (it.hasNext()) {
        const QString rel = srcRoot.relativeFilePath(it.next());   // e.g. components/LAvatar.qml
        QQmlComponent component(view->engine(),
                                QUrl(QStringLiteral("qrc:/qt/qml/LOAMS/qml/") + rel));
        if (component.isReady())
            ++compiled;
        else
            failures << rel + QStringLiteral(": ") + component.errorString();
    }
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    QCOMPARE(compiled, 43);   // update together with QML_FILES in quick/CMakeLists.txt
}

void TestQmlResourceInventory::turnstilePhotoLoadsThroughFactory()
{
    const QUrl base = m_server.url(QStringLiteral("/loams_api/"));
    const QByteArray payload =
        "{\"status\":\"success\",\"latest_id\":4,\"entry\":{\"id\":4,\"card\":\"C\","
        "\"created_at\":\"2026-09-29 08:30:00\",\"reader\":0,"
        "\"student\":{\"name\":\"Test Student A\",\"photo_path\":\"uploads/students/s1.png\"}}}";
    const LoginParser::EntryEventResult r = LoginParser::parseEntryEvent(payload, base);
    QVERIFY(r.valid);
    QVERIFY(r.hasStudent);
    const QString photo = r.student.value(QStringLiteral("photo_url")).toString();
    QCOMPARE(photo, m_server.url(QStringLiteral("/loams_api/uploads/students/s1.png")).toString());
    expectRemoteAvatarLoads(photo, "/loams_api/uploads/students/s1.png");
}

void TestQmlResourceInventory::loginPhotoUrlLoadsThroughFactory()
{
    const QString serverPhoto =
        m_server.url(QStringLiteral("/loams_api/uploads/students/s2.png")).toString();
    const QByteArray body = QStringLiteral(
        "{\"status\":\"success\",\"student\":{\"name\":\"Test Student B\",\"photo_url\":\"%1\"}}")
        .arg(serverPhoto).toUtf8();
    const LoginParser::RfidResult r = LoginParser::parseRfidResponse(body);
    QVERIFY(r.ok);
    const QString photo = r.student.value(QStringLiteral("photo_url")).toString();
    QCOMPARE(photo, serverPhoto);   // KioskViewModel::applyStudentLogin binds this verbatim
    expectRemoteAvatarLoads(photo, "/loams_api/uploads/students/s2.png");
}

void TestQmlResourceInventory::searchAvatarLoadsThroughFactory()
{
    QVERIFY(ApiConfig::setBaseUrl(m_server.url(QStringLiteral("/loams_api/")).toString()));
    SearchResultsModel model;
    StudentRecord rec;
    rec.schoolId = QStringLiteral("TEST-0001");
    rec.name = QStringLiteral("Test Student C");
    rec.photo = QStringLiteral("uploads/students/s3.png");
    model.setRecords({rec});
    const QString photo = model.data(model.index(0), SearchResultsModel::PhotoRole).toString();
    QCOMPARE(photo, m_server.url(QStringLiteral("/loams_api/uploads/students/s3.png")).toString());
    expectRemoteAvatarLoads(photo, "/loams_api/uploads/students/s3.png");
}

void TestQmlResourceInventory::defaultSentinelAndEmptyPhotoNeverRequested()
{
    auto view = makeView();
    const QString sentinel = m_server.url(QStringLiteral("/loams_api/uploads/default.jpg")).toString();
    QQuickItem *withSentinel = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { initials: \"TD\"; source: %1 }").arg(qmlString(sentinel)));
    QQuickItem *empty = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { initials: \"TE\"; source: \"\" }"));
    QVERIFY(withSentinel);
    QVERIFY(empty);
    QVERIFY(withSentinel->property("showInitials").toBool());
    QVERIFY(empty->property("showInitials").toBool());
    QTest::qWait(300);   // negative assertion: give a (wrong) load time to reach the server
    QCOMPARE(m_server.countFor("/loams_api/uploads/default.jpg"), 0);
}

void TestQmlResourceInventory::missingRemotePhotoFallsBackToInitials()
{
    auto view = makeView();
    const QString missing = m_server.url(QStringLiteral("/loams_api/uploads/students/missing.png")).toString();
    QQuickItem *avatar = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { initials: \"TM\"; source: %1 }").arg(qmlString(missing)));
    QVERIFY(avatar);
    QTRY_COMPARE_WITH_TIMEOUT(avatar->property("imageStatus").toInt(), kImageError, 5000);
    QVERIFY(avatar->property("showInitials").toBool());
    QVERIFY(m_server.countFor("/loams_api/uploads/students/missing.png") >= 1);
}

void TestQmlResourceInventory::importedLogoFileUrlLoads()
{
    const QString path = writePng(QStringLiteral("logo_inventory.png"));
    bool hasLogo = false;
    const QUrl logo = SchoolInfoUtil::resolveLogoUrl(path, &hasLogo);
    QVERIFY(hasLogo);
    QVERIFY(logo.isLocalFile());

    const qsizetype requestsBefore = m_server.requests().size();
    auto view = makeView();
    QQuickItem *badge = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLLogoCircle { size: 52; hasLogo: true; logoUrl: %1 }")
        .arg(qmlString(logo.toString())));
    QVERIFY(badge);
    QObject *image = badge->findChild<QObject *>(QStringLiteral("logoImage"));
    QVERIFY(image);
    QTRY_COMPARE_WITH_TIMEOUT(image->property("status").toInt(), kImageReady, 5000);
    QCOMPARE(m_server.requests().size(), requestsBefore);   // local asset: no network
}

void TestQmlResourceInventory::settingsLogoPreviewLoads()
{
    const QString path = writePng(QStringLiteral("logo_settings_inventory.png"));
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/logoPath"), path);
        s.sync();
    }
    OfflineHttp vmHttp;                          // seam owner in a test: never a live manager
    SettingsViewModel vm(nullptr, &vmHttp.http);
    vm.load();
    const QUrl url = vm.logoUrl();
    QVERIFY(url.isLocalFile());

    auto view = makeView();
    QQuickItem *preview = create(*view, QStringLiteral(
        "import QtQuick\nImage { width: 48; height: 48; fillMode: Image.PreserveAspectFit; source: %1 }")
        .arg(qmlString(url.toString())));
    QVERIFY(preview);
    QTRY_COMPARE_WITH_TIMEOUT(preview->property("status").toInt(), kImageReady, 5000);

    AppSettings s;
    s.remove(QStringLiteral("school/logoPath"));
}

void TestQmlResourceInventory::posterIsNotDisplayedByAnyQml()
{
    // Inventory row R10: imported posters are stored (school/posterPath) but no
    // 2.0 QML displays them. If a screen starts showing one, this fails so the
    // poster gets its own load test here.
    QDirIterator it(QStringLiteral(SRC_QML_DIR), {QStringLiteral("*.qml")},
                    QDir::Files, QDirIterator::Subdirectories);
    QStringList hits;
    int scanned = 0;
    while (it.hasNext()) {
        const QString file = it.next();
        QFile f(file);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(file));
        ++scanned;
        if (QString::fromUtf8(f.readAll()).contains(QLatin1String("poster"), Qt::CaseInsensitive))
            hits << file;
    }
    QVERIFY2(scanned > 20, "scan found too few QML files — is SRC_QML_DIR wrong?");
    QVERIFY2(hits.isEmpty(), qPrintable(hits.join(QStringLiteral(", "))));
}

QTEST_MAIN(TestQmlResourceInventory)
#include "tst_qmlresourceinventory.moc"
```

Register in `qt-app/quick/CMakeLists.txt` immediately **before** the `# Coverage guard:` comment block (line 483; it must stay last):

```cmake
# --- S1a resource inventory (C++ QtTest, offscreen): every resource the 2.0
# client loads (remote photos, fallback avatars, imported logos, bundled QML)
# still loads through PolicyNamFactory managers. Loopback TinyHttpServer only. ---
wits_add_qttest(tst_qmlresourceinventory
    SOURCES tests/tst_qmlresourceinventory.cpp tests/RecordingPolicyNam.h
        ${CMAKE_SOURCE_DIR}/testsupport/tinyhttpserver.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/tinyhttpserver.h
        ${WITS_TEST_NAM_SOURCES}
    LIBS ${WITS_LOAMS_STATIC_LIBS}
         Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport ${CMAKE_CURRENT_SOURCE_DIR}/tests
    DEFINES SRC_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    OFFSCREEN)
```

- [ ] **Step 2: Prove the test detects a missing seam — expected FAIL.** The resources already load today, so the red step proves the *detector*: temporarily delete the two-line `if (!PolicyNamFactory::installOn(*view->engine(), m_factory)) qFatal(...);` statement from `makeView()` in the new test file (production code is not touched), then:

```bash
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a --target tst_qmlresourceinventory
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^tst_qmlresourceinventory$' --output-on-failure
```

Expected: `turnstilePhotoLoadsThroughFactory`, `loginPhotoUrlLoadsThroughFactory` and `searchAvatarLoadsThroughFactory` FAIL at `qobject_cast<PolicyEnforcingNam *>(view->engine()->networkAccessManager())` (the engine fell back to Qt's default manager, and the photo never appears in `m_log`). Then restore the deleted statement exactly.

- [ ] **Step 3: Run — expected PASS** with the real factory:

```bash
cmake --build C:/b/s1a --target tst_qmlresourceinventory
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a -R '^tst_qmlresourceinventory$' --output-on-failure
```

Expected: all 10 inventory functions pass (incl. `everyBundledQmlFileCompilesFromQrc` reporting 43 files). Do not loosen a timeout to make a row pass; investigate via `superpowers:systematic-debugging`.

- [ ] **Step 4: Cross-check the inventory is complete.** Re-run the discovery greps and confirm every hit maps to an inventory row (R1-R12); add a row + test if anything new appears:

```bash
grep -rnE "source:|Image \{|AnimatedImage|BorderImage|image://|FontLoader|XMLHttpRequest|loadImage" qt-app/quick/qml
grep -rnE "fromLocalFile|endpoint\(.*photo|photo_url|logoUrl" qt-app/quick qt-app/core --include=*.cpp | grep -v "/build" | grep -v "/tests/"
```

- [ ] **Step 5: Commit** via the project `commit` skill. Intended subject: `test(quick): inventory QML resource loads through the transport seam`.

---

### Task 14: Layer 9 functional regression, Passthrough evidence and hand-off

**Files:**
- Create: `docs/superpowers/proofs/2026-10-05-loams-s1a-proof.md`
- Test: full suite + manual `WITSQuick` smoke

**Interfaces:**
- Consumes: everything above.
- Produces: recorded evidence for the slice-approval conditions (spec §7) and the PR body.

- [ ] **Step 1: Clean full build + full suite (default OFF), proven offline.** Start the dev XAMPP Apache first, so that any stray request from a test would be logged, and keep the machine otherwise idle (no browser or other client hitting `localhost`) during the run:

```bash
rm -rf C:/b/s1a
cmake -S qt-app -B C:/b/s1a -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64
cmake --build C:/b/s1a 2>&1 | tee C:/b/s1a-build.log
grep -niE "warning:" C:/b/s1a-build.log | grep -E "core/transport|quick/PolicyNamFactory|AccessControlHub|viewmodels/" || echo "no new warnings in S1a files"
ACCESS_LOG=C:/xampp/apache/logs/access.log            # dev XAMPP access log
before=$(wc -l < "$ACCESS_LOG")
QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=Basic QT_QPA_FONTDIR=C:/Windows/Fonts \
  ctest --test-dir C:/b/s1a --output-on-failure -j 8
after=$(wc -l < "$ACCESS_LOG")
echo "access.log lines added during ctest: $((after - before))"   # expect 0
tail -n "$((after - before))" "$ACCESS_LOG" | grep "/loams_api/" || echo "no backend request from the test run"
```

Expected: build succeeds with no new warnings in S1a files; **68 tests, 100% passed, 0 Not Run**; **0 access-log lines added** (if any line was added, every one must be attributable to something other than the test run — any `/loams_api/` request during the run is a test that reached the backend: find it via `superpowers:systematic-debugging` and inject a fake manager):

- 60 pre-existing targets that must stay green: `tst_appshell tst_rfidquickfilter tst_themeviewmodel tst_quicktestmain_missingfile tst_quicktestsourcepath tst_qml_theme tst_navigator tst_adminsession tst_kioskviewmodel tst_schoolinfoviewmodel tst_schoolinfoutil tst_guestviewmodel tst_httpform tst_initials tst_barsmodel tst_studentstablemodel tst_searchresultsmodel tst_reportrowsmodel tst_accessentriesmodel tst_qml_components tst_qml_kiosk tst_accesscontrolhub tst_dashboardviewmodel tst_visitlogsviewmodel tst_accesscontrolviewmodel tst_searchviewmodel tst_databaseviewmodel tst_qml_admin tst_qml_adminshell tst_qml_accesscontrol tst_settingsviewmodel tst_importviewmodel tst_reportingviewmodel tst_notokenaliases tst_rfidscandetector tst_apiconfig tst_theme tst_visitorcontroller tst_importcontroller tst_studentcontroller tst_reportcontroller tst_reportrenderer tst_brandtheme tst_brandingcontroller tst_loginparser tst_dashboardparser tst_visitlogparser tst_csvutil tst_reportanalytics tst_timeanalytics tst_accesstypes tst_eventbus tst_mockprovider tst_accessproviderfactory tst_accessdecisionservice tst_healthmonitor tst_accesscontrolservice tst_turnstileprovider tst_contactage tst_apiconfigloader`
- 8 new: `tst_legacywidgetsgate tst_transportpolicy tst_policyenforcingnam tst_httpclient tst_policynamfactory tst_qmlfactorygate tst_transportseamguard tst_qmlresourceinventory`
- Mandatory gates re-confirmed in this full run: GATE G1 (`transportpolicy.cpp` built under `-Werror`, `tst_transportpolicy` green) and GATE G2 (`tst_qmlfactorygate` green).
- Gated (built only with ON): `tst_rfidkeyboardfilter tst_responsive_ui`

- [ ] **Step 2: Developer ON configuration still works.**

```bash
cmake -S qt-app -B C:/b/s1a-legacy -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -DLOAMS_BUILD_LEGACY_WIDGETS=ON
cmake --build C:/b/s1a-legacy --target WITS tst_rfidkeyboardfilter tst_responsive_ui
QT_QPA_PLATFORM=offscreen ctest --test-dir C:/b/s1a-legacy -R '^(tst_legacywidgetsgate|tst_rfidkeyboardfilter|tst_responsive_ui)$' --output-on-failure
```

Expected: all three pass (`WITS` builds; S1a did not change the core controller signatures it uses).

- [ ] **Step 3: Passthrough evidence — tests changed only by injection plumbing.** A pre-existing test line may be removed or modified (a modified line shows as a removed `-` line) **only** at these **249** plumbing sites:
  - 34 fake-manager re-routes: 13 `AccessControlHub hub(&nam);` (Task 8) + 16 `XViewModel <any>(nullptr, &<any>);` (Task 9: dashboard 2, visit logs 2, access control 12 incl. `authVm`) + 5 `XController controller(nullptr);` (Task 4b);
  - 215 offline safety injections: 213 default-constructed ViewModels (Tasks 9-12: dashboard 4, visit logs 8, access control 8, kiosk 18, guest 3, search 9, database 63, import 5, reporting 57, settings 38) + 2 default-constructed hubs (`tst_accesscontrolhub.cpp:261`, `tst_appshell.cpp:30`; Task 8).

The patterns match by class name and accept **any** variable name:

```bash
git diff master...HEAD -U0 -- 'qt-app/tests/*' 'qt-app/quick/tests/*' | grep -E '^-[^-]' > C:/b/s1a-removed.txt
PLUMBING='AccessControlHub \w+(\(&\w+\))?;|ViewModel \w+\(nullptr, &\w+\);|Controller \w+\(nullptr\);|^-\s*\w+ViewModel \w+;'
grep -cE "$PLUMBING" C:/b/s1a-removed.txt    # expect 249
grep -vE "$PLUMBING" C:/b/s1a-removed.txt    # expect: no output
```

If the second `grep` prints anything, it is a changed pre-existing test line: justify it (it must not weaken or remove an assertion) or restore it. Paste both commands and their output into the proof doc.

- [ ] **Step 4: Static checks.**

```bash
grep -rnE "new\s+QNetworkAccessManager|make_(unique|shared)\s*<\s*QNetworkAccessManager" qt-app/core qt-app/quick --include=*.cpp --include=*.h | grep -v "/build" | grep -v "quick/tests/"    # expect: no output
grep -rn "ignoreSslErrors" qt-app --include=*.cpp --include=*.h --include=*.qml --include=*.js | grep -v "^qt-app/libs/" | grep -v "/build"   # expect: no output
grep -nE "^\s*(static\s+)?((Dashboard|VisitLogs|AccessControl|Kiosk|Guest|Search|Database|Import|Reporting|Settings)ViewModel|AccessControlHub) \w+;" qt-app/quick/tests/tst_*.cpp   # expect: only lines inside defaultConstruction*/defaultHub* functions
cmake -LA -N C:/b/s1a | grep LOAMS_BUILD_LEGACY_WIDGETS    # expect: LOAMS_BUILD_LEGACY_WIDGETS:BOOL=OFF
git diff --stat master...HEAD -- qt-app/quick/qml   # expect: no output (S1a changes no production QML, so no Theme-token risk)
```

- [ ] **Step 5: Prepare an isolated smoke environment with a reproducible start state (MANDATORY before Step 6).** The smoke mutates persistent state on both sides: server (student register/delete, bulk edit, department deactivate/delete, imports, visit reset, admin-info save, admin-key rotation, uploaded photos) and client (`HKCU\Software\MyCompany\MyApp` QSettings — school info, guest toggle, theme; `%APPDATA%\MyCompany\MyApp` imported logos). **Never** smoke against the production gate PC, the client's real database, or any shared or real-data database, and never with real student data or the real production admin key. Use synthetic data and a smoke-only admin key (`s1a-smoke-key-<random suffix>`, never committed or pasted into the proof doc). The Layer 9 comparison runs the smoke **twice** — once with the `master` build, once with the branch build — and both passes MUST start from the **same** state, proven by an identical state fingerprint (below).

  **Option A — DEFAULT: disposable VM with a golden checkpoint.** Use a throwaway Windows VM (Hyper-V or VirtualBox) that holds the whole environment, server **and** client, so one checkpoint resets everything:
  1. In the VM install the dev XAMPP version, Git for Windows (for the helper below), and deploy `deliverables/loams_api` from this branch to `C:\xampp\htdocs\loams_api\`.
  2. Create the database from **structure only** taken on the dev box (`mysqldump --no-data <DB_NAME>` — no rows leave the dev box), set the smoke-only admin key with `hash_admin.php`, then create synthetic records through `WITSQuick` itself (Register, and Import of a synthetic CSV + ZIP with names like `Test Student A`, IDs like `21-1-0001`).
  3. Deploy both clients into the VM: on the dev box run `C:/Qt/6.11.1/mingw_64/bin/windeployqt.exe --qmldir qt-app/quick/qml <exe>` for the `master` build's `WITSQuick.exe` and for `C:/b/s1a/quick/WITSQuick.exe`, copy the two deployed folders to `C:\s1a-smoke\master\` and `C:\s1a-smoke\branch\` in the VM (no `config.ini` in either → both use `http://localhost/loams_api/` inside the VM). Start one of them once, set school name/logo/guest toggle, quit.
  4. Shut down Apache and both clients, save the helper below as `C:\s1a-smoke\smoke-state.sh` in the VM, record `bash smoke-state.sh fingerprint > golden.fingerprint`, and take the VM checkpoint **`S1A-GOLDEN`**.
  5. Destroy the VM after Step 7. Nothing on the dev box or anywhere else is mutated.

  **Option B — SECONDARY, only if no VM is available:** the developer's local dev backend, allowed **only** if its database already contains synthetic data exclusively. All state handling goes through the fail-fast helper below (every command checked via `set -euo pipefail`; every backup hash-manifested and verified; DB and registry backups restore-tested into scratch copies before anything is touched; restores are copy-then-swap). With Apache and both `WITSQuick` builds stopped and MySQL running:

```bash
bash C:/b/s1a-smoke/smoke-state.sh snapshot C:/b/s1a-smoke/S0     # creates, manifests AND verifies the snapshot
bash C:/b/s1a-smoke/smoke-state.sh verify C:/b/s1a-smoke/S0       # re-verify immediately before the smoke starts
```

  If either command exits non-zero, **do not start the smoke** — use Option A.

  **Helper `smoke-state.sh`** (operator scratch file outside the repo — `C:/b/s1a-smoke/smoke-state.sh` on the dev box, `C:\s1a-smoke\smoke-state.sh` in the VM; never committed). Database subcommands prompt for the MySQL password once per run and keep it in a private temp option file, passed to the native MariaDB tools as a Windows path (`cygpath -w`) and deleted on exit — never on a command line. Git Bash path conversion is suppressed **only** per `reg.exe` call (`regx`), never globally. `verify` restores into a scratch database and a scratch registry key whose names are unique per run (`<DB_NAME>_s1av_<UTC timestamp>_<random>`, `HKCU\Software\S1aSmokeVerify_<UTC timestamp>_<random>`); it refuses to proceed if either already exists, and the exit trap drops/deletes only the exact database/key this run created. `backend` pins a client install's effective backend (env override > `config.ini` > default) to the intended URL and prints it as evidence. `DB_USER` / `DB_NAME` come from the deployed `loams_api/config.php`:

```bash
#!/usr/bin/env bash
# S1a Layer 9 smoke-state helper. usage:
#   DB_USER=<user> DB_NAME=<db> bash smoke-state.sh snapshot|verify|restore <dir>
#   DB_USER=<user> DB_NAME=<db> bash smoke-state.sh fingerprint
#   bash smoke-state.sh backend <exe-dir> <intended-base-url>
set -euo pipefail
# NOTE: no global MSYS_NO_PATHCONV. Paths handed to native .exe tools are
# converted explicitly with `cygpath -w`; only reg.exe calls (whose /s /f /y
# switches Git Bash would otherwise rewrite) disable path conversion, per call.

MYSQLBIN=/c/xampp/mysql/bin
HTDOCS=/c/xampp/htdocs/loams_api
UPLOAD_DIRS=(uploads loams_api.uploads)
REGKEY='HKCU\Software\MyCompany\MyApp'
APPDATA_DIR="$(cygpath -u "$APPDATA")/MyCompany/MyApp"
DEFAULT_BASE_URL='http://localhost/loams_api/'     # ApiConfig::defaultBaseUrl()

TMPD="$(mktemp -d)"; CNF="$TMPD/client.cnf"
CREATED_DB=""        # scratch database created by THIS run (the only one we may drop)
CREATED_REGKEY=""    # scratch registry root created by THIS run (the only one we may delete)

regx()   { MSYS_NO_PATHCONV=1 reg "$@"; }                 # reg.exe with switches left intact
mysqlc() { "$MYSQLBIN/mysql.exe" --defaults-extra-file="$(cygpath -w "$CNF")" "$@"; }
dumpc()  { "$MYSQLBIN/mysqldump.exe" --defaults-extra-file="$(cygpath -w "$CNF")" --single-transaction \
             --routines --triggers --skip-dump-date --skip-comments --order-by-primary "$@"; }
fail()   { echo "smoke-state: $*" >&2; exit 1; }

cleanup() {
  if [ -n "$CREATED_DB" ]; then
    mysqlc -e "DROP DATABASE \`$CREATED_DB\`" || echo "smoke-state: WARNING could not drop scratch DB $CREATED_DB" >&2
  fi
  if [ -n "$CREATED_REGKEY" ]; then
    regx delete "$CREATED_REGKEY" /f >/dev/null 2>&1 || echo "smoke-state: WARNING could not delete $CREATED_REGKEY" >&2
  fi
  rm -rf "$TMPD"
}
trap cleanup EXIT

need_db() {
  : "${DB_USER:?set DB_USER}"; : "${DB_NAME:?set DB_NAME}"
  read -rs -p "MySQL password for $DB_USER (empty if none): " DBPW; echo
  ( umask 077; printf '[client]\nuser=%s\npassword=%s\n' "$DB_USER" "$DBPW" > "$CNF" ); unset DBPW
}

uniq_suffix() { printf '%s_%05d%05d' "$(date -u +%Y%m%d%H%M%S)" "$RANDOM" "$RANDOM"; }
normdump() { sed -E 's/ AUTO_INCREMENT=[0-9]+//'; }     # restore-invariant form of a dump
dbhash()   { dumpc "$1" | normdump | sha256sum | cut -d' ' -f1; }
dirmanifest() { ( cd "$1" && find . -type f -print0 | sort -z | xargs -0 -r sha256sum ); }
dircount() { find "$1" -type f | wc -l | tr -d ' '; }
reghash()  { regx query "$1" /s | tr -d '\r' \
               | sed -E 's#HKEY_CURRENT_USER\\Software\\[^\\]+\\MyApp#KEY#' | sha256sum | cut -d' ' -f1; }
db_exists() { [ -n "$(mysqlc -N -e "SELECT SCHEMA_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='$1'")" ]; }

# Value of WITS_API_BASE_URL in a registry environment block ('' if absent).
regenv() { regx query "$1" /v WITS_API_BASE_URL 2>/dev/null | tr -d '\r' \
             | awk '$1 == "WITS_API_BASE_URL" { print $3; exit }' || true; }

# Effective backend of a WITSQuick install: WITS_API_BASE_URL (env) > config.ini
# [Server] BaseURL next to the exe > built-in default (apiconfigloader.cpp).
# Every source a launch can inherit the override from (this shell; the user and
# system environments that Explorer/GUI launches inherit) must be unset OR equal
# the intended backend; a config.ini BaseURL, when present, must equal it too;
# and the resulting effective URL must equal it. Prints one evidence line.
backend() {
  local dir="$1" intended="${2%/}/"
  [ -f "$dir/WITSQuick.exe" ] || fail "no WITSQuick.exe in $dir"
  local shellenv="${WITS_API_BASE_URL:-}"
  local userenv;   userenv="$(regenv 'HKCU\Environment')"
  local systemenv; systemenv="$(regenv 'HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment')"
  local src v
  for src in shell user system; do
    case "$src" in shell) v="$shellenv" ;; user) v="$userenv" ;; system) v="$systemenv" ;; esac
    [ -z "$v" ] || [ "${v%/}/" = "$intended" ] \
      || fail "WITS_API_BASE_URL in the $src environment is '$v', not the intended '$intended' — unset it"
  done
  local ini="$dir/config.ini" inihash="absent" configured=""
  if [ -f "$ini" ]; then
    inihash="$(sha256sum "$ini" | cut -d' ' -f1)"
    configured="$(tr -d '\r' < "$ini" | awk -F= '
        /^\[/ { sec = $0; next }
        sec == "[Server]" && $1 ~ /^[ \t]*BaseURL[ \t]*$/ { sub(/^[^=]*=[ \t]*/, ""); print; exit }')"
    [ -z "$configured" ] || [ "${configured%/}/" = "$intended" ] \
      || fail "config.ini [Server] BaseURL in $dir is '$configured', not the intended '$intended'"
  fi
  local effective source
  if   [ -n "$shellenv" ];   then effective="$shellenv";   source="env(shell)"
  elif [ -n "$userenv" ];    then effective="$userenv";    source="env(user)"
  elif [ -n "$systemenv" ];  then effective="$systemenv";  source="env(system)"
  elif [ -n "$configured" ]; then effective="$configured"; source="config.ini"
  else                            effective="$DEFAULT_BASE_URL"; source="default"; fi
  [ "${effective%/}/" = "$intended" ] \
    || fail "effective backend for $dir is '$effective' ($source), not the intended '$intended'"
  echo "backend $(basename "$dir") effective=${effective%/}/ source=$source env.shell=${shellenv:-unset} env.user=${userenv:-unset} env.system=${systemenv:-unset} config.ini=$inihash config.BaseURL=${configured:-absent}"
}

fingerprint() {
  echo "db $(dbhash "$DB_NAME")"
  for d in "${UPLOAD_DIRS[@]}"; do
    if [ -d "$HTDOCS/$d" ]; then echo "$d $(dirmanifest "$HTDOCS/$d" | sha256sum | cut -d' ' -f1) $(dircount "$HTDOCS/$d")"
    else echo "$d absent"; fi
  done
  if regx query "$REGKEY" >/dev/null 2>&1; then echo "registry $(reghash "$REGKEY")"; else echo "registry absent"; fi
  if [ -d "$APPDATA_DIR" ]; then echo "appdata $(dirmanifest "$APPDATA_DIR" | sha256sum | cut -d' ' -f1) $(dircount "$APPDATA_DIR")"
  else echo "appdata absent"; fi
  local t
  for t in $(mysqlc -N -e "SELECT table_name FROM information_schema.tables WHERE table_schema='$DB_NAME' AND table_type='BASE TABLE' ORDER BY table_name"); do
    echo "count $t $(mysqlc -N -e "SELECT COUNT(*) FROM \`$DB_NAME\`.\`$t\`")"
  done
}

verify() {
  local S="$1"
  [ -f "$S/MANIFEST.sha256" ] || fail "no manifest in $S"
  ( cd "$S" && sha256sum -c --quiet MANIFEST.sha256 ) || fail "snapshot artifacts changed or unreadable"
  # DB: restore into a scratch database unique to this run and require the
  # identical normalized dump. Never reuse or drop a database we did not create.
  local scratch="${DB_NAME}_s1av_$(uniq_suffix)"
  ! db_exists "$scratch" || fail "scratch database $scratch already exists — refusing"
  mysqlc -e "CREATE DATABASE \`$scratch\`"            # no IF NOT EXISTS: errors if it appeared meanwhile
  CREATED_DB="$scratch"
  mysqlc "$scratch" < "$S/db.sql"
  [ "$(dbhash "$scratch")" = "$(cat "$S/db.normhash")" ] || fail "DB snapshot does not restore identically"
  mysqlc -e "DROP DATABASE \`$scratch\`"; CREATED_DB=""
  # Upload / app-data copies: per-file hashes and file counts.
  local d
  for d in "${UPLOAD_DIRS[@]}" appdata; do
    [ -e "$S/$d.absent" ] && continue
    ( cd "$S/$d" && sha256sum -c --quiet "$S/$d.manifest" ) || fail "$d copy does not match its manifest"
    [ "$(dircount "$S/$d")" -eq "$(wc -l < "$S/$d.manifest")" ] || fail "$d copy file count differs"
  done
  # Registry: import a re-rooted copy under a scratch key unique to this run.
  if [ ! -e "$S/registry.absent" ]; then
    local name="S1aSmokeVerify_$(uniq_suffix)"
    local root="HKCU\\Software\\$name"
    ! regx query "$root" >/dev/null 2>&1 || fail "scratch registry key $root already exists — refusing"
    iconv -f UTF-16LE -t UTF-8 "$S/qsettings.reg" \
      | sed "s#\\\\Software\\\\MyCompany\\\\MyApp#\\\\Software\\\\$name\\\\MyApp#g" \
      | iconv -f UTF-8 -t UTF-16LE > "$TMPD/regverify.reg"
    CREATED_REGKEY="$root"
    regx import "$(cygpath -w "$TMPD/regverify.reg")"
    [ "$(reghash "$root\\MyApp")" = "$(cat "$S/reg.hash")" ] || fail "registry export does not re-import identically"
    regx delete "$root" /f; CREATED_REGKEY=""
  fi
  echo "snapshot $S verified"
}

snapshot() {
  local S="$1"
  [ ! -e "$S" ] || fail "refusing to overwrite $S"
  mkdir -p "$S"
  dumpc "$DB_NAME" > "$S/db.sql";            [ -s "$S/db.sql" ]    || fail "empty DB dump"
  dumpc "$DB_NAME" admin > "$S/admin.sql";   [ -s "$S/admin.sql" ] || fail "empty admin dump"
  normdump < "$S/db.sql" | sha256sum | cut -d' ' -f1 > "$S/db.normhash"
  [ "$(cat "$S/db.normhash")" = "$(dbhash "$DB_NAME")" ] || fail "DB changed while dumping"
  local d
  for d in "${UPLOAD_DIRS[@]}"; do
    if [ -d "$HTDOCS/$d" ]; then
      dirmanifest "$HTDOCS/$d" > "$S/$d.manifest"
      cp -a "$HTDOCS/$d" "$S/$d"
    else : > "$S/$d.absent"; fi
  done
  if regx query "$REGKEY" >/dev/null 2>&1; then
    regx export "$REGKEY" "$(cygpath -w "$S/qsettings.reg")" /y
    [ -s "$S/qsettings.reg" ] || fail "empty registry export"
    reghash "$REGKEY" > "$S/reg.hash"
  else : > "$S/registry.absent"; fi
  if [ -d "$APPDATA_DIR" ]; then
    dirmanifest "$APPDATA_DIR" > "$S/appdata.manifest"
    cp -a "$APPDATA_DIR" "$S/appdata"
  else : > "$S/appdata.absent"; fi
  fingerprint > "$S/fingerprint.txt"
  ( cd "$S" && find . -type f ! -name MANIFEST.sha256 -print0 | sort -z | xargs -0 -r sha256sum ) > "$S/MANIFEST.sha256"
  verify "$S"
}

restore_dir() {   # copy-then-swap: the live dir is replaced only after the staged copy is verified
  local S="$1" name="$2" dst="$3"
  if [ -e "$S/$name.absent" ]; then
    if [ -e "$dst" ]; then mv "$dst" "$dst.s1a-old"; rm -rf "$dst.s1a-old"; fi
    return
  fi
  mkdir -p "$(dirname "$dst")"
  rm -rf "$dst.s1a-stage"
  cp -a "$S/$name" "$dst.s1a-stage"
  ( cd "$dst.s1a-stage" && sha256sum -c --quiet "$S/$name.manifest" ) || fail "staged $name does not verify"
  [ "$(dircount "$dst.s1a-stage")" -eq "$(wc -l < "$S/$name.manifest")" ] || fail "staged $name file count differs"
  if [ -e "$dst" ]; then mv "$dst" "$dst.s1a-old"; fi
  mv "$dst.s1a-stage" "$dst"
  rm -rf "$dst.s1a-old"
}

restore() {
  local S="$1"
  verify "$S"                                   # nothing is touched unless the snapshot verifies NOW
  # DB: the verified snapshot is the authority and is never deleted; the live
  # content being replaced is post-smoke data. Reload, then prove equality.
  mysqlc "$DB_NAME" < "$S/db.sql"
  [ "$(dbhash "$DB_NAME")" = "$(cat "$S/db.normhash")" ] \
    || fail "live DB differs from snapshot after reload (snapshot intact; re-run restore)"
  local d
  for d in "${UPLOAD_DIRS[@]}"; do restore_dir "$S" "$d" "$HTDOCS/$d"; done
  restore_dir "$S" appdata "$APPDATA_DIR"
  if [ -e "$S/registry.absent" ]; then
    regx delete "$REGKEY" /f >/dev/null 2>&1 || true
  else
    local post="${S}.post-smoke-$(date -u +%Y%m%dT%H%M%SZ).reg"
    if regx query "$REGKEY" >/dev/null 2>&1; then
      regx export "$REGKEY" "$(cygpath -w "$post")" /y   # kept until the restore verifies
      regx delete "$REGKEY" /f
    fi
    regx import "$(cygpath -w "$S/qsettings.reg")"
    [ "$(reghash "$REGKEY")" = "$(cat "$S/reg.hash")" ] || fail "registry restore mismatch (post-smoke tree kept at $post)"
    rm -f "$post"
  fi
  [ "$(fingerprint)" = "$(cat "$S/fingerprint.txt")" ] || fail "post-restore fingerprint differs from snapshot"
  echo "restore of $S verified: fingerprint matches"
}

case "${1:-}" in
  snapshot)    need_db; snapshot "${2:?dir}" ;;
  verify)      need_db; verify "${2:?dir}" ;;
  restore)     need_db; restore "${2:?dir}" ;;
  fingerprint) need_db; fingerprint ;;
  backend)     backend "${2:?exe-dir}" "${3:?intended-base-url}" ;;
  *) echo "usage: $0 snapshot|verify|restore <dir> | fingerprint | backend <exe-dir> <intended-base-url>" >&2; exit 2 ;;
esac
```

- [ ] **Step 6: Manual Layer 9 smoke — `master` pass, then branch pass, from the identical start state.** Order (record every fingerprint in the proof doc):

  | Order | Option A (VM) | Option B (dev box) |
  |---|---|---|
  | 1 | Revert the VM to `S1A-GOLDEN`; `bash smoke-state.sh fingerprint > start-master.fp`; `diff golden.fingerprint start-master.fp` must be empty | `bash smoke-state.sh fingerprint > start-master.fp`; `diff C:/b/s1a-smoke/S0/fingerprint.txt start-master.fp` must be empty |
  | 2 | `bash smoke-state.sh backend C:/s1a-smoke/master http://localhost/loams_api/ > master.backend` (must exit 0); start Apache; from that same shell run `C:/s1a-smoke/master/WITSQuick.exe` and the table below; stop Apache + client | `bash C:/b/s1a-smoke/smoke-state.sh backend <master build>/quick http://localhost/loams_api/ > master.backend` (must exit 0); start Apache; from that same shell run `<master build>/quick/WITSQuick.exe` and the table below; stop Apache + client |
  | 3 | Revert the VM to `S1A-GOLDEN`; `bash smoke-state.sh fingerprint > start-branch.fp`; `diff golden.fingerprint start-branch.fp` must be empty | `bash smoke-state.sh restore C:/b/s1a-smoke/S0` (exits 0 only if the restored fingerprint equals `S0/fingerprint.txt`); `bash smoke-state.sh fingerprint > start-branch.fp`; `diff start-master.fp start-branch.fp` must be empty |
  | 4 | `bash smoke-state.sh backend C:/s1a-smoke/branch http://localhost/loams_api/ > branch.backend` (must exit 0); start Apache; from that same shell run `C:/s1a-smoke/branch/WITSQuick.exe` (built from this branch — stale-binary trap) and the table below; stop Apache + client | `bash C:/b/s1a-smoke/smoke-state.sh backend C:/b/s1a/quick http://localhost/loams_api/ > branch.backend` (must exit 0); start Apache; from that same shell run `C:/b/s1a/quick/WITSQuick.exe` (this branch's build dir — stale-binary trap) and the table below; stop Apache + client |

  The proof that both passes started from the same state is `diff start-master.fp start-branch.fp` = empty: the fingerprint covers the normalized DB dump hash, every table's row count, the upload directories' hash manifests + file counts, the QSettings registry tree hash and the app-data hash manifest + file count. A non-empty diff invalidates the comparison — fix the environment and rerun both passes.

  The proof that both passes talked to the intended isolated backend is `master.backend` and `branch.backend`: `backend` aborts (non-zero exit, pass not started) unless `WITS_API_BASE_URL` is unset or equal to the intended URL in the launching shell **and** in the user (`HKCU\Environment`) and system environments that GUI launches inherit, any `config.ini` `[Server] BaseURL` next to that exe equals it, and the resulting effective URL (env > `config.ini` > built-in default) equals it. Each line records the effective URL, its source, all three env values, the `config.ini` SHA-256 (or `absent`) and its `BaseURL`; both lines must show the same `effective=` value. Launch each client from the shell that ran its `backend` check (Option B: the dev box shell; Option A: the VM's Git Bash).

  Run each item in both passes and record pass/fail per pass:

| # | Area | Check |
|---|---|---|
| K1 | Kiosk student | Numeric school ID login → welcome card, remote photo (or initials fallback), feed row, counters |
| K2 | Kiosk RFID | Reader / keyboard-wedge scan → same as K1; one tap = one visit (debounce) |
| K3 | Kiosk errors | Unknown ID / unregistered card → error toast; network-down (stop Apache) → "Network error" toast |
| K4 | Guest | Enable guest in Settings → kiosk guest dialog submit → success toast |
| K5 | Admin entry | Smoke-only admin key at kiosk → admin shell opens |
| A1 | Dashboard | Stats and hourly bars load |
| A2 | Search | Department/course chips load; search returns cards with photos / initials |
| A3 | Visit logs | Today/Week, student/visitor modes load |
| A4 | Database | Table + filters; single edit (course list loads); bulk edit; register with photo; delete a synthetic record; department deactivate/delete on a synthetic department; CSV export |
| A5 | Import | Template download; synthetic CSV + ZIP → duplicate check → upload → result |
| A6 | Reporting | Departments/years load; generate report (`api.php/reports/data`); time analytics; export PDF + Excel; print dialog opens |
| A7 | Settings | Save school info; import logo → preview, sidebar and kiosk BrandPanel update; admin info save; admin key change to a second synthetic key **and back** (verify login with the smoke-only key afterwards); reset visits on a synthetic department with manifest |
| A8 | Guarded ops | Wrong admin key → auth-failure message on a guarded operation |
| A9 | Access Control page | Feed loads; monitoring toggle on/off |
| T1 | Turnstile polling | Monitoring on (bench/bridge simulation inside the isolated environment — never the production gate PC) → entry appears on kiosk with photo |
| T2 | Reconnect + cursor | Stop Apache → connection state degrades; restart → reconnects; no duplicated or skipped entries |
| R | Assets | Remote photos (kiosk + search); `default.jpg` → initials; missing photo → initials; imported logo (kiosk, sidebar, settings preview); bundled QML renders; `--software` backend still renders |

Any difference between the two passes is a regression: route it through `superpowers:systematic-debugging`, fix with a failing test first, and re-run Steps 1-6 (both passes, from the reset start state).

- [ ] **Step 7: Return the environment to its pre-smoke state.**
  - **Option A:** delete the VM (and its checkpoints). Nothing else was touched.
  - **Option B:** with Apache and both clients stopped: `bash C:/b/s1a-smoke/smoke-state.sh restore C:/b/s1a-smoke/S0` — it re-verifies the snapshot first (manifest, scratch DB restore, scratch registry import), reloads the DB and proves its normalized dump equals the snapshot, restores uploads/app-data by copy-then-swap (live directories are replaced only after the staged copy verifies by hash and file count), re-imports the registry (keeping the post-smoke export until the restored tree verifies), and exits 0 only if the final fingerprint equals `S0/fingerprint.txt`. Then start Apache and confirm with the `master` `WITSQuick` that login with the **pre-smoke** dev admin key succeeds and dashboard counts match.

  **If admin-key reversion fails** (A7 left an unknown key) the `restore` above already resets the `admin` table with the rest of the verified DB. If `restore` itself fails it changes nothing before its verification passes; fix the reported cause and re-run it — the snapshot is never modified or deleted. If the snapshot is unusable, rebuild the dev database from structure + synthetic data and set a new dev key with `hash_admin.php`. Record any incident in the proof doc. Keep `C:/b/s1a-smoke/S0` until the proof doc is committed, then delete it (it holds only synthetic data).

- [ ] **Step 8: Write the proof doc** `docs/superpowers/proofs/2026-10-05-loams-s1a-proof.md` with: the commit SHA tested, the ctest summary (68/68, 0 Not Run) with the access-log offline evidence, the GATE G1/G2 results (incl. the `GATE G2:` thread log line), the ON-config result, the Step 3 diff output, the Step 4 grep outputs, which smoke option was used (A: VM + `S1A-GOLDEN`; B: the `S0` snapshot's `MANIFEST.sha256` hash and the `snapshot`/`verify`/`restore` exit status), the fingerprints `start-master.fp` / `start-branch.fp` (and `golden.fingerprint` or `S0/fingerprint.txt`) with the empty `diff`, the backend-pinning lines `master.backend` / `branch.backend` (same `effective=` URL, env values, `config.ini` hashes), the Step 6 smoke table with pass/fail per pass and which turnstile setup was used, the Step 7 result, and a link to this plan's Rollback Considerations. No real student data, no admin key (real or smoke), no DB password, no screenshots containing PII.

- [ ] **Step 9: Commit** via the project `commit` skill. Intended subject: `docs(security): record S1a Layer 9 regression evidence`.

- [ ] **Step 10: Review gates** (not commits): `/codex-review` (owner override: `gpt-5.6-sol`, high effort) and/or `/claude-review` in branch mode until APPROVE (≤ 3 rounds); fix Critical/Important findings via new commits; then the project `create-pr` gate (three agents: `dry-checker`, `security-reviewer`, `general-code-reviewer` — if a loaded skill names `api-checker`, re-read `.claude/skills/create-pr/SKILL.md`); open the PR against `master`. Do **not** merge — hand off to the owner.

---

## Acceptance Tests

**Automated (Windows, Qt 6.11.1 MinGW, `C:/b/s1a`, default `LOAMS_BUILD_LEGACY_WIDGETS=OFF`) — 68/68 passed, 0 Not Run, 0 skipped:**

- New: `tst_legacywidgetsgate`, `tst_transportpolicy` (GATE G1), `tst_policyenforcingnam`, `tst_httpclient`, `tst_policynamfactory` (incl. the four `installOn` ordering tests), `tst_qmlfactorygate` (GATE G2), `tst_transportseamguard` (incl. `namConstructedOnlyInsideSeam` and `ownersInTestsUseFakeManagers` with empty pending lists, `coreNetworkClassesRequireInjectedManager`, `noSslErrorBypassInClientSource`, `quickMainInstallsTransportBeforeLoad`), `tst_qmlresourceinventory` (incl. the per-file `everyBundledQmlFileCompilesFromQrc`).
- Test safety: every seam owner constructed in `quick/tests/tst_*.cpp` uses an injected fake manager (`OfflineHttp` / `CapturingNam` / `SequencedNam`) except the request-free `defaultConstruction*`/`defaultHub*` ownership tests; `tst_httpclient::offlineHttpNeverAnswers` green; the full `ctest` run adds **0** lines to the running dev Apache's access log (Task 14 Step 1).
- Extended: `tst_appshell` (seam installed via `installOn` before load, zero warnings), `tst_accesscontrolhub` (+ `defaultHubManagerIsSeamManager`, `injectedClientManagerReachesTheProvider`), `tst_dashboardviewmodel`, `tst_visitlogsviewmodel`, `tst_accesscontrolviewmodel`, `tst_kioskviewmodel`, `tst_guestviewmodel`, `tst_searchviewmodel`, `tst_databaseviewmodel`, `tst_importviewmodel`, `tst_reportingviewmodel`, `tst_settingsviewmodel` (injection through `HttpClient`, seam ownership, and — for controller-owning VMs — controller manager identity), and the seven core suites `tst_studentcontroller`, `tst_visitorcontroller`, `tst_reportcontroller`, `tst_importcontroller`, `tst_brandingcontroller`, `tst_accessdecisionservice`, `tst_turnstileprovider` (`networkManagerIsTheInjectedOne`).
- Unchanged and green: the remaining pre-existing targets listed in Task 14 Step 1, including every `tst_qml_*` QuickTest (now running on factory-made managers) and all core `CapturingNam` / `SequencedNam` controller suites.
- Developer config `-DLOAMS_BUILD_LEGACY_WIDGETS=ON`: `WITS`, `tst_rfidkeyboardfilter`, `tst_responsive_ui` build; `tst_legacywidgetsgate`, `tst_rfidkeyboardfilter`, `tst_responsive_ui` pass.

**Manual:** Layer 9 smoke table (Task 14 Step 6) all PASS on `WITSQuick` built from this branch with zero differences from the `master` pass, both passes run in an isolated environment (default: disposable VM reverted to checkpoint `S1A-GOLDEN` before each pass; secondary: verified `S0` snapshot of a synthetic-only dev backend restored between passes) and proven to start from the same state (`diff start-master.fp start-branch.fp` empty), followed by the Step 7 reset.

**"Passthrough proves no behaviour change" — evidence required in the proof doc and PR body:**

1. `tst_transportpolicy::prepareIsIdentityInPassthrough` — the request leaving the policy equals the request entering it (URL incl. `http`, headers, redirect attribute present/absent, `QSslConfiguration`).
2. `tst_policyenforcingnam` — byte-identical request line, header block and body on the wire versus a plain `QNetworkAccessManager` for GET, GET+query, form POST, JSON POST to `api.php/reports/data`, and multipart upload; identical status/body/error; identical redirect following (Qt default) and identical manual-redirect behaviour; identical `ConnectionRefused` error; identical `reply->request()`.
3. `tst_qmlresourceinventory` — every inventoried resource loads (or falls back) exactly as before, on factory-made managers.
4. The Task 14 Step 3 diff: no pre-existing test assertion modified or removed — only injection plumbing.
5. The Layer 9 manual smoke table with zero differences from `master`.

## Rollback Considerations

- **Nature of the change:** S1a is behaviour-neutral (Passthrough), client-only and confined to source + build configuration. No server, database, PHP endpoint, `config.ini` key, `QSettings` key or on-disk format changes; no migration to undo.
- **How to revert:** revert the squash/merge commit of the S1a PR on `master` (`git revert <merge-sha>` via a normal PR). Each task is also an independent commit, so a single faulty migration (e.g. one ViewModel) can be reverted alone; the guard's `kPendingMigration` entry for that file (and its test file's `kPendingTestMigration` entry) must be restored in the same revert. Reverting Task 4b alone (the core non-null contract) also requires removing `coreNetworkClassesRequireInjectedManager` from the guard; it changes no runtime behaviour for correctly wired callers.
- **Legacy Widgets:** the freeze only changes the default. Developers who need the Widgets build use `-DLOAMS_BUILD_LEGACY_WIDGETS=ON` (no revert needed). Existing build directories that cached the old `BUILD_LEGACY_WIDGETS=ON` get a warning and must reconfigure (`cmake -U BUILD_LEGACY_WIDGETS -B <dir> -DLOAMS_BUILD_LEGACY_WIDGETS=ON`).
- **Smoke-test state (Task 14 Steps 5-7):** the manual smoke is the only S1a activity that mutates data, server and client side. **Default (Option A):** a disposable VM holding server and both clients, reverted to checkpoint `S1A-GOLDEN` before each pass and deleted afterwards — nothing to restore. **Secondary (Option B, synthetic-only dev backend):** the fail-fast `smoke-state.sh` helper snapshots the DB, the `admin` table, the upload directories, the `HKCU\Software\MyCompany\MyApp` QSettings tree and `%APPDATA%\MyCompany\MyApp`; writes a SHA-256 manifest of every artifact; and verifies the snapshot (manifest check, restore into a per-run uniquely named scratch database with identical normalized dump, per-file hashes + file counts, re-import into a per-run uniquely named scratch registry key — it refuses if either name exists and only ever drops/deletes what that run created) at creation, immediately before the smoke, and again at the start of every restore. Restore reloads the DB and proves equality, swaps directories in only after the staged copy verifies, keeps the post-smoke registry export until the restored tree verifies, and succeeds only if the final state fingerprint equals the snapshot's. A failed restore changes nothing before its verification and never modifies the snapshot. Never against production, the gate PC, or any shared/real-data database.
- **Production impact:** none possible — S1a is **never shipped to production before S1f** (spec invariant 8; only S1f packaging can produce a release, and it requires S1e, which deletes Passthrough). The deployed gate-PC `WITS.exe` is untouched by this slice and is not rebuilt from this tree.
- **Forward compatibility:** reverting S1a after S1e work has started would also revert S1e's call-site assumptions; S1e must therefore start from a merged, green S1a.

## Completion Criteria

- [ ] Tasks 1-14 (incl. 4b, 5b and 6b) done; every task committed via the project `commit` skill, Conventional Commits, no Claude/Anthropic co-author trailer.
- [ ] GATE G1 and GATE G2 passed **when first run** (Tasks 2 and 5b) and again in the final full run; no gate failure was worked around.
- [ ] Clean full build + full `ctest` green on Windows MinGW (`C:/b/s1a`): 68/68, 0 failed, 0 Not Run; no new compiler warnings in S1a files.
- [ ] Every production network request passes through the policy: core network classes assert a non-null injected manager (`tst_transportseamguard::coreNetworkClassesRequireInjectedManager`), and runtime identity tests prove each production owner's controllers hold its `HttpClient`'s `PolicyEnforcingNam` (Tasks 8, 11, 12).
- [ ] Factory ordering enforced: `PolicyNamFactory::installOn` refusal tests (Task 5) and `quickMainInstallsTransportBeforeLoad` (Task 7) green.
- [ ] `-DLOAMS_BUILD_LEGACY_WIDGETS=ON` configures and builds `WITS` + its two tests; default is `OFF` (`cmake -LA -N C:/b/s1a | grep LOAMS_BUILD_LEGACY_WIDGETS` → `OFF`).
- [ ] No test can reach a backend: `tst_transportseamguard::ownersInTestsUseFakeManagers` green with an **empty** `kPendingTestMigration`, and the Task 14 Step 1 run added 0 lines to the running dev Apache's access log.
- [ ] Layer 9 manual smoke done on `WITSQuick` from this branch in an isolated environment (Task 14 Step 5; Option A VM by default), both passes started from the same fingerprinted state (`diff start-master.fp start-branch.fp` empty) against the same pinned isolated backend (`master.backend` / `branch.backend` from `smoke-state.sh backend`, identical `effective=` URL), zero differences from the `master` pass, environment reset (Step 7), recorded in `docs/superpowers/proofs/2026-10-05-loams-s1a-proof.md`.
- [ ] Passthrough evidence items 1-5 (Acceptance Tests) present in the proof doc and PR body.
- [ ] No raw manager outside the seam — `tst_transportseamguard` green with an **empty** `kPendingMigration`, and `grep -rnE "new\s+QNetworkAccessManager|make_(unique|shared)\s*<\s*QNetworkAccessManager" qt-app/core qt-app/quick --include=*.cpp --include=*.h | grep -v "/build" | grep -v "quick/tests/"` prints nothing.
- [ ] No `ignoreSslErrors` in client source — `tst_transportseamguard::noSslErrorBypassInClientSource` green and `grep -rn "ignoreSslErrors" qt-app --include=*.cpp --include=*.h --include=*.qml --include=*.js | grep -v "^qt-app/libs/" | grep -v "/build"` prints nothing.
- [ ] `/codex-review` and/or `/claude-review` APPROVE (≤ 3 rounds), Critical/Important findings fixed.
- [ ] Project `create-pr` three-agent gate (`dry-checker`, `security-reviewer`, `general-code-reviewer`) clean; the `security-reviewer` pass is the slice's security review.
- [ ] Rollback considerations documented (this plan + proof doc) — required by spec §7 slice conditions.
- [ ] PR opened against `master`; merge left to the owner.

## Spec Coverage

| S1a-relevant spec requirement (section) | Implemented / verified by |
|---|---|
| `TransportPolicy` immutable, `std::shared_ptr<const TransportPolicy>`, thread-safe (§2) | Task 2 (`static_assert`s, `concurrentReadersAlwaysSeeACompleteSnapshot`) |
| Atomic snapshot via `std::atomic_load` / `std::atomic_store` (§2) | Task 2 `TransportPolicy::current/setCurrent`; Task 5 factory reads it per `create()` |
| `HttpClient` owns the controller-facing manager (§2) | Task 4; owners in Tasks 8-12 |
| QML factory creates a NEW manager per `create()`, possibly multi-threaded, same policy (§2) | Task 5 (`createReturnsNewParentedPolicyNam`, `createIsSafeFromManyThreads`, `createUsesPolicySnapshotAtCallTime`) |
| Both manager kinds are `PolicyEnforcingNam` (§2) | Task 4 (`defaultOwnsAPolicyEnforcingNam`), Task 5, Task 5b, Task 7 (`tst_appshell`), Tasks 8-12 (`findChildren<PolicyEnforcingNam *>`, controller identity) |
| `PolicyEnforcingNam` overrides `createRequest()`; single choke point (§3) | Task 3; Task 6/12 guard (no other manager in core/quick) |
| S1a Passthrough preserves today's behaviour: `http` allowed, no TLS enforcement (§3) | Task 2 `prepareIsIdentityInPassthrough`; Task 3 wire-parity suite incl. `plainHttpOriginIsAllowed`, redirect parity |
| Passthrough exists only until S1e (§3) | Single `Mode::Passthrough` enum + `main.cpp` comment marking the S1e replacement point; no release logic |
| Injection boundary: `HttpClient` accepts injected manager; `CapturingNam`/`SequencedNam` stay realistic (§3) | Task 4 (`injectedManagerIsUsedNotOwned`, `requestsThroughInjectedManagerReachIt`); Tasks 8-12 test adaptations |
| ViewModels/hubs that construct managers obtain them from the seam (`DatabaseViewModel.cpp:11-24`, `GuestViewModel.cpp:11`, `KioskViewModel.cpp:20`, `AccessControlHub.cpp:20-29`, …) (§3) | Tasks 8-12; Task 6 guard with shrinking `kPendingMigration` |
| QML engine networking onto the seam (§7 S1a row) | Task 7 (`main.cpp`, `QuickTestSetup.h`, `tst_appshell`, guard `quickMainInstallsTransportBeforeLoad`) |
| Factory installed before any QML load / any request (approval condition; §2) | Task 7 source-order guard `quickMainInstallsTransportBeforeLoad` (fresh engine, install immediately before `loadFromModule`) — the primary guarantee; Task 5 `PolicyNamFactory::installOn` runtime refusal for its three detectable late states (`installRefusedAfterEngineCreatedAManager` / `installRefusedAfterQmlLoaded` / `installRefusedWhenAnotherFactoryInstalled`), with `main.cpp` failing closed on refusal |
| QML image loads use the factory's managers, incl. from loader threads (§2; approval condition — mandatory early gate) | Task 5b GATE G2 `tst_qmlfactorygate` (Image + Canvas.loadImage), STOP-on-fail |
| Atomic `shared_ptr` snapshot compiles cleanly on the target toolchain (§2; approval condition — mandatory early gate) | Task 2 GATE G1 (`-Wall -Wextra -Werror` on `transportpolicy.cpp`), STOP-on-fail |
| Every production request passes through the policy; raw-manager injection on core classes is a test seam only (approval condition) | Task 4b (non-null `Q_ASSERT_X` contract + `networkManager()` on all seven core network classes); Task 6 guard `coreNetworkClassesRequireInjectedManager` + `namConstructedOnlyInsideSeam`; runtime identity tests Tasks 8 (hub → provider), 11 (`StudentController` ×3), 12 (`ImportController`, `ReportController`) |
| Resource inventory: every loaded resource becomes a test (§3) | "S1a resource inventory" table + Task 13 `tst_qmlresourceinventory` (R1-R8, R10); R9/R11 existing suites |
| `ignoreSslErrors` never in client source; grep check (§3) | Task 6 `noSslErrorBypassInClientSource`; Completion Criteria grep |
| Legacy target behind `LOAMS_BUILD_LEGACY_WIDGETS=OFF` (§3, §7 S1a row) | Task 1 (option, gate, `tst_legacywidgetsgate`) |
| Legacy source deprecated / reference-only; no security migrations (§3) | Task 1 `LEGACY-WIDGETS.md`, `CLAUDE.md`; legacy owners excluded from migration and guard scope |
| Release packaging rejects the legacy target (§3, §7 S1f) | Out of S1a scope by design — S1f `New-LoamsReleasePackage.ps1` (noted in `LEGACY-WIDGETS.md` and the CMake option comment) |
| Shared `core/` not constrained by legacy buildability; incompatibilities documented (§3) | `LEGACY-WIDGETS.md` "Known incompatibilities" (none introduced: controller APIs unchanged; Task 14 Step 2 proves ON builds) |
| Layer 9 functional regression incl. `api.php/reports/data`, turnstile polling/reconnect/cursor, photos/logos/avatars/bundled assets, `CapturingNam`/`SequencedNam` suites via `HttpClient` (§6) | Task 3 (`api.php/reports/data` parity), Task 6b + Tasks 8-12 (every owner-constructing suite injects through `HttpClient` — fakes or `OfflineHttp`), Task 8 (hub cursor/reconnect suite), Task 13, Task 14 Steps 1 (incl. offline access-log evidence), 3, 6 |
| "S1a MUST NOT change behaviour … Layer 9 proves it" (§6) | Acceptance Tests "Passthrough proves no behaviour change" items 1-5 |
| Slice needs passing tests, security review, documented rollback (§7) | Task 14 Steps 1-10 (incl. isolated smoke backend + restore, Steps 5-7); Rollback Considerations; Completion Criteria |
| No production deployment before S1f (§1 invariant 8, §7) | Global Constraints; Rollback Considerations; no packaging/deploy step in this plan |
| Security tests fail, never skip (§6 Gate) | Guard/inventory use `QVERIFY`/`QVERIFY2` only; no `QSKIP` anywhere in new tests |

## Hand-off notes for S1e (informational — not S1a work)

- `TransportPolicy::current()` defaults to Passthrough when nothing is published; S1e's fail-closed bootstrap must replace that default (no network objects before Phase C).
- `HttpClient` snapshots the policy at construction; S1e's trust-epoch handling attaches here and in `PolicyEnforcingNam`.
- Injected test managers (`CapturingNam`/`SequencedNam`/`OfflineHttp`) bypass `PolicyEnforcingNam` by design in S1a; S1e decides whether enforcement suites wrap them. S1e's fail-closed default policy also becomes the runtime backstop that makes an un-injected test client unable to reach `http://localhost` (S1a relies on the static guard + access-log evidence instead — see Task 6b).
- QuickTest fixtures use `data:` image URIs (inventory R12); S1e's URL interceptor rejects `data:`, so those fixtures must move to `qrc:` or the allowed `file:` locations then.
