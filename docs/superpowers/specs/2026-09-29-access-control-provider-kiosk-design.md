# Access Control — TurnstileProvider + Native Kiosk Display (Sub-plan 3) Design

**Status:** Approved 2026-09-29 (owner sign-off, four spec corrections folded in).
**Depends on:** Sub-plan 1 (core seam — MERGED, PR #56) + Sub-plan 2 (backend endpoints — MERGED, PR #57).
**Feeds:** Sub-plan 4 (Access Control admin page + runtime settings UI).

## Context

LOAMS 2.0 (`WITSQuick`) currently has an unwired Access Control seam (`qt-app/core/accesscontrol/`: `IAccessProvider`, `EventBus`, `AccessProviderFactory`, `AccessControlService`, `HealthMonitor`, `MockProvider`, `accesstypes`) behind `accessControl.enabled` (default off), and two merged read-only backend endpoints (`turnstile_display.php`, `access_recent.php`). Nothing connects the seam to the app or the kiosk yet.

This sub-plan is the **full vertical slice** that makes a real gate swipe surface the student natively on the LOAMS 2.0 kiosk — retiring the legacy PowerShell keyboard-injection bridge for LOAMS 2.0 — end to end, still behind `accessControl.enabled` (default off = zero behavior change). It adds a server-observed `TurnstileProvider`, a pure `LoginParser::parseEntryEvent`, an application-owned `AccessControlHub` composition root exposed to QML as a singleton, `main.cpp` wiring, and a presentation-scoped kiosk subscription.

The deployed legacy `WITS.exe` (Widgets) and its PowerShell bridge / `turnstile_pull.php` / `rfid_login.php` suppression are **untouched** — this is LOAMS-2.0-only.

## Locked decisions

- **Scope:** full vertical slice (provider + parser + app-level wiring + kiosk subscription), flag-gated.
- **Ownership:** `AccessControlHub` is an application-owned composition root for Access Control **only**, owning `EventBus` + `AccessProviderFactory` + `AccessControlService`. Per Sub-plan 1, the service owns `HealthMonitor` and the `IAccessProvider` (the `TurnstileProvider`) — there is **no** separate provider `unique_ptr` in the hub (no double ownership).
- **QML boundary:** the hub emits a `QVariantMap` (`AccessEntry`), never `QJsonObject`. JSON does not cross past the hub. No raw `card` and no `reader` on the bus or the signal (raw credential stays inside the adapter; public-display privacy).
- **Lifetime:** C++ owns the lifecycle; the app-owned instance is exposed as a declarative `QML_SINGLETON` via a create-function (not `qmlRegisterSingletonInstance` — that fights the declaratively-built `LOAMS` module; see §5) with `CppOwnership` so QML never deletes it. Provider / polling cursor / health are application-lifetime; the kiosk↔presentation connection lives and dies with the QML surface.
- **Config:** via `AppSettings` — `accessControl.enabled` (default false), `accessControl.pollIntervalMs` (default 1500, validated), `accessControl.gateId` (default `"turnstile"`); default/only v1 provider is `TurnstileProvider`; base URL from `ApiConfig::baseUrl()`. `WITS_ACCESS_CONTROL=1`/`true` force-enables for the current process (non-persistent). Config read once at app init; runtime mutation deferred to Sub-plan 4.

### The four sign-off corrections (binding)

1. **`parseEntryEvent` returns an explicit validity/error result.** `hasEntry=false` must not double as "malformed." A malformed HTTP-200 body is a protocol failure, never a healthy empty poll.
2. **Photo contract — reconciled against merged code.** The merged `turnstile_display.php` returns a **relative `photo_path`** (via `normalizeStudentPhotoPath`), deliberately host-free — **not** `photo_url`. The backend is not churned. `parseEntryEvent(body, baseUrl)` stays pure and composes `photo_url` from `photo_path` + an **explicitly passed** `baseUrl`; the `ApiConfig::baseUrl()` read happens at the provider call site, never inside the parser.
3. **Retry ownership lives in exactly one layer.** The provider owns only healthy-state polling/draining; on any transport/protocol failure it stops its poll timer and reports `Degraded`/`Error`, and `AccessControlService` owns reconnect/backoff. The two retry mechanisms never run at once.
4. **Unknown-vs-known subject is an explicit convention on `AccessEvent`.** Empty `subject` on an `EntryObserved` event = "entry observed, subject unresolved"; non-empty `subject` = known student.

## Components

### 1. `LoginParser::parseEntryEvent` — pure response decoder

**Files:** `qt-app/core/loginparser.h` / `.cpp` (extend), `qt-app/tests/tst_loginparser*` (extend or add).

Mirrors the existing pure, network-free `parseRfidResponse`. New result type:

```cpp
struct EntryEventResult {
    bool        valid     = false;  // false = malformed JSON/schema (protocol failure)
    bool        hasEntry  = false;  // valid && an entry row was returned
    qint64      latestId  = 0;      // MAX(id) informational; 0 on empty table
    qint64      eventId   = 0;      // the returned entry's id (cursor advance target)
    bool        hasStudent = false; // entry present && student resolved
    QJsonObject student;            // normalized student incl. composed photo_url; empty if unresolved
    QDateTime   at;                 // entry timestamp (UTC, converted from server-local — see below)
    QString     error;              // human-readable reason when !valid
};

EntryEventResult parseEntryEvent(const QByteArray &body, const QUrl &baseUrl);
```

The endpoint's actual entry shape (verified on master, [turnstile_display.php:61-76](../../../deliverables/loams_api/turnstile_display.php)) is `{ id:int, card:string, created_at:string, reader:int, student:object|null }` inside `{ status:"success", latest_id:int, entry:object|null }`. The parser reads `id`, `created_at`, and `student`; it ignores `card` and `reader` (raw credential/reader never leave the adapter).

**Rules (unambiguous):**
- Well-formed `{"status":"success","latest_id":N,"entry":null}` → `valid=true, hasEntry=false` (healthy empty poll).
- Well-formed with an `entry` object → `valid=true, hasEntry=true`, with **all** of: `latest_id` a **non-negative** integer (matching `MAX(id)` / the 0-on-empty-table semantics); `entry.id` a **positive** integer (→ `eventId`); `entry.created_at` a timestamp parseable in the exact form below (→ `at`); `entry.student` either `null` (→ `hasStudent=false`, empty `student`, the merged endpoint's defensive orphaned-row case) or a JSON **object** (→ `hasStudent=true`, normalized). Any of these malformed → the whole result is **invalid** (see below).
- Not JSON; wrong top-level shape; `status != "success"`; `entry` present but neither `null` nor an object; missing/non-positive `entry.id`; unparseable `latest_id` or `created_at`; `entry.student` present but neither `null` nor an object → `valid=false`, `error` set, everything else default. **Never fabricates an entry**, and a malformed HTTP-200 body is a protocol failure, not a healthy empty poll.
- **Timestamp (server-local, not UTC):** `created_at` is a MySQL `DATETIME` in server-local time, exact form `yyyy-MM-dd HH:mm:ss` (same form the legacy bridge parses — `deliverables/loams_api/bridge/loams-turnstile-bridge.ps1`). Parse it with `QDateTime::fromString(created_at, "yyyy-MM-dd HH:mm:ss")` as `Qt::LocalTime` (same-host deployment assumption), then `.toUTC()` for `at`. An unparseable value makes the result invalid.
- **Photo composition (tolerant, pure):** in the student object, if `photo_url` is present and non-empty → keep it; else if `photo_path` (relative) is present → set `photo_url = baseUrl.resolved(QUrl(photo_path)).toString()`; else leave `photo_url` empty. `baseUrl` is passed in — the parser never reads `ApiConfig`. Output student always carries a `photo_url` key (the key `KioskViewModel::applyStudentLogin` reads at [KioskViewModel.cpp:123](../../../qt-app/quick/viewmodels/KioskViewModel.cpp)).

### 2. `TurnstileProvider` — server-observed `IAccessProvider`

**Files:** `qt-app/core/accesscontrol/turnstileprovider.h` / `.cpp` (new, folded into `witscore`), `qt-app/tests/tst_turnstileprovider*` (new).

Implements `IAccessProvider` (`descriptor()`, `start()`, `stop()`, `state()`; signals `accessEvent`, `stateChanged`, `hardwareError`). Mirrors the `AccessDecisionService` idiom ([accessdecisionservice.cpp:70-115](../../../qt-app/core/accesscontrol/accessdecisionservice.cpp)): an **injected, not-owned** `QNetworkAccessManager`, a pure `parseEntryEvent`-based decode, a per-reply **timeout** that `abort()`s a stalled GET, and a **generation counter** so a stale in-flight reply cannot mutate state.

- **Construction/config:** `pollIntervalMs` and `gateId` (from descriptor config), `baseUrl` (`ApiConfig::baseUrl()`, supplied by the hub at construction), and the injected `QNetworkAccessManager*`. `providerId = "turnstile"`, human name "Turnstile (server-observed)".
- **NAM ownership/affinity:** the app-lifetime `QNetworkAccessManager` is supplied by the hub and injected into the provider via the factory `CreatorFn` closure (see §4 — the registered creator captures the hub's NAM; §4 defines the owned-vs-injected roles). An externally injected NAM must outlive the hub. v1 is single-threaded: the poll timer, the NAM, and the provider share the main-thread affinity (matching the service's affinity note in `accesscontrolservice.cpp`).
- **In-flight reply ownership:** each request's `QNetworkReply` is **parented to the provider** (not left on the NAM) and tracked as `m_reply`, so the provider fully owns its reply lifecycle: `stop()` and `~TurnstileProvider` abort + delete it independently of when the NAM is torn down.
- **Descriptor config schema:** advertises `pollIntervalMs` (int) and `gateId` (string) fields. `ConfigFieldDescriptor` carries only `key`/`displayName`/`type`/`required` ([accesstypes.h:53-58](../../../qt-app/core/accesscontrol/accesstypes.h)) — it has **no** default-value member — so the **hub** supplies the defaults when reading config (`gateId` default `"turnstile"`; an empty/whitespace value falls back to `"turnstile"`). `AccessEvent.gateId` is stamped from the resolved config value — **not** derived from the backend's `reader` field (a lane index, not a stable gate identity).
- **Cursor:** a `qint64 m_since` plus a `bool m_baselined` (see start()). `latestId`/`eventId`/cursor are `qint64` throughout.
- **In-flight ownership + timeout:** at most one GET is outstanding. Each request's `QNetworkReply` is tracked (`m_reply`); a per-reply single-shot `QTimer` (default 5000 ms) `abort()`s it on timeout so a stalled GET can never wedge the drain or hide a failure from the service. `stop()`/restart abort the tracked reply. On `finished`, the reply is `deleteLater`d and the generation is checked before any state change.
- **Start:** state → `Connecting`, then issue one GET whose URL depends on `m_baselined`:
  - **First application-lifetime start** (`!m_baselined`): GET bare `turnstile_display.php` (no `?since`). On a `valid` response set `m_since = latestId`, `m_baselined = true`, state → `Connected` — do **not** emit the returned historical entry (skip process-start history) — **and then arm the steady-state poll** (below), so the baseline immediately transitions into normal polling; otherwise the provider would make one startup request and never observe a live swipe. The service records comm-time on the `Connected` transition (see health note).
  - **Subsequent (service-triggered reconnect) start** (`m_baselined`): GET `?since=<m_since>` — **preserve** `m_since`, never re-baseline (a transient outage would otherwise silently discard every swipe accumulated during it). On the response, transition to `Connected` **and process the body under the normal steady-state rules below** — a reconnect GET can itself return the oldest unseen entry, so it must be emitted (exactly once) and drained, not merely acknowledged. (Cross-*process* cursor persistence is out of scope: each process re-baselines on its first start; entries between process exit and next start are an accepted display-only miss — attendance stays authoritative on the backend.)
  - On failure during either: a single failure transition (see failure rule below), cursor unmoved.
- **Steady-state poll (healthy-only, this provider's sole retry surface):** GET `turnstile_display.php?since=<m_since>`.
  - `valid && hasEntry` with **`eventId > m_since`** → emit `AccessEvent{type=EntryObserved, subject=<student or empty>, gateId, correlationId=QString::number(eventId), at}`, set `m_since = eventId`, then **immediately** issue the next GET (async, one-at-a-time drain — never more than one request in flight).
  - `valid && hasEntry` but **`eventId <= m_since`** → a protocol anomaly (a well-formed body that fails to advance would otherwise infinite-drain): failure transition, cursor unmoved, no event.
  - `valid && !hasEntry` (empty poll) → **arm the single-shot `pollIntervalMs` timer** for the next poll.
  - `!valid` (protocol failure) or transport failure/timeout → **stop the poll timer**, failure transition; do **not** move the cursor, do **not** fabricate an event. Reconnect is the service's job.
- **Failure transition (exactly one per failed request):** every runtime poll failure — transport error, timeout, `!valid` protocol failure, or a non-advancing `eventId<=m_since` — emits **`stateChanged(Degraded)`** and nothing else. The provider does **not** emit `Error` in this slice: the service treats `Degraded` and `Error` identically (both map to `scheduleReconnect()`, [accesscontrolservice.cpp:140-143](../../../qt-app/core/accesscontrol/accesscontrolservice.cpp)) and has no terminal-error state, so a second `Error` emission for the same failure would just arm the backoff twice. Invalid *configuration* (e.g. an unparseable base URL) is rejected by the hub **before** `service.enable()` rather than surfaced as a runtime `Error`.
- **Generation guard** applies to `stop()` **and** service-triggered restarts: a reply tagged generation N is dropped once N+1 has begun, so a stale reply can never advance the cursor or emit after a restart. `stop()` also aborts the tracked reply and halts the timer.
- **Read-only:** never POSTs; the backend stays the sole attendance writer.

**Retry ownership (the one-layer rule):** the provider re-arms its single-shot timer **only** after a successful empty poll (and chains immediately only while draining real entries). Every failure path stops that timer and defers to `AccessControlService`'s existing state machine + exponential backoff reconnect ([accesscontrolservice.cpp:139-146](../../../qt-app/core/accesscontrol/accesscontrolservice.cpp), which calls `provider->start()` on the reconnect timer). The two mechanisms are mutually exclusive by construction.

**Health accounting (single owner):** comm-time is recorded **only** by `AccessControlService`, on the `Connected` transition (existing merged behavior, [accesscontrolservice.cpp:129-131](../../../qt-app/core/accesscontrol/accesscontrolservice.cpp)). The provider does **not** call the service-owned `HealthMonitor` — the `CreatorFn` gives it no handle to the service's monitor, and a provider-side baseline record would double-count against the `Connected` transition. **Forward-note (deferred):** per-poll comm-health freshness (updating last-comm on every successful empty/entry poll, as originally requested) is deferred until a `HealthMonitor` injection seam exists, to avoid coupling the provider to a service-owned monitor or changing the merged factory/interface in this slice.

### 3. `AccessEvent` unknown-student convention

**Files:** `qt-app/core/accesscontrol/accesstypes.h` (document the convention; no struct change — `subject` already exists at `accesstypes.h:43`).

For an `EntryObserved` event: **empty `subject`** = "entry observed, subject unresolved" (orphaned/deleted student); **non-empty `subject`** = resolved student. `correlationId` carries the `eventId`. `gateId` set from the provider's gate. Raw `card`/`reader` are never placed on the event.

### 4. `AccessControlHub` — application-owned composition root + QML singleton

**Files:** `qt-app/quick/AccessControlHub.h` / `.cpp` (new), registered in `qt-app/quick/CMakeLists.txt`; `qt-app/tests/tst_accesscontrolhub*` (new).

`QObject` subclass named **`AccessControlHub`** (exposed to QML as **`AccessControl`** via the §5 wrapper; the class is not named `AccessControl` to avoid clashing with the existing `AccessControl` C++ namespace).

**Ownership & teardown order.** Parenting `EventBus`/`AccessControlService` as `QObject` children of the hub would delete them in the `QObject` base destructor, *after* the hub's own members are gone — so a service child could outlive the NAM it uses. The stack rule ("parent every QObject … use smart pointers only for non-parented objects") explicitly permits smart-pointer ownership for exactly this case, so the hub holds these top-level objects as **`std::unique_ptr` members** (`parent=nullptr`; the factory a plain value member), declared **bus → owned-NAM → factory → service** so reverse-order destruction is **service → factory → owned-NAM → bus**: the service's dtor tears down the provider while the NAM is still alive, and the bus outlives every publisher. The provider is parented to the *service* (via `create(descriptor, config, service)`), so it dies with the service.

Deterministic member order is *necessary but not sufficient* — `~AccessControlService` does not call `disable()`, and neither the service nor `IAccessProvider` dtor calls `stop()`. So **`~TurnstileProvider` calls `stop()`** (aborts + deletes its provider-parented `m_reply`, halts the timer). Because the reply is parented to the provider (§2) and the provider dies before the owned-NAM, the abort always runs against a live reply regardless of NAM teardown timing. (The hub does not rely on `deleteLater`-based `disable()` at shutdown, which needs an event loop that may not run.)

- **NAM injection seam:** the hub's constructor takes an **optional externally-owned `QNetworkAccessManager*`**. When null (production) the hub creates and owns one as `std::unique_ptr<QNetworkAccessManager> m_ownedNam`; when injected (tests pass a `CapturingNam`) `m_ownedNam` stays null and the injected NAM must outlive the hub. A non-owning `QNetworkAccessManager *m_nam` points at whichever is in use, and that is what the creator closure injects into the provider — so the hub never issues a real localhost request under test.
- `void initialize()` — reads enablement (see §7). Registers `TurnstileProvider` with the factory via a `CreatorFn` **lambda that captures the hub's NAM, `gateId`, and `baseUrl`** and returns `new TurnstileProvider(nam, config, parent)` (the `CreatorFn` signature is fixed at `(descriptor, config, parent)`, [accessproviderfactory.h:21-22](../../../qt-app/core/accesscontrol/accessproviderfactory.h), so the NAM enters via the closure, not a new parameter). Registration happens **whether or not** the feature is enabled (so a later Sub-plan-4 runtime toggle can enable without re-registering). If enabled, calls `service.enable(descriptor, {pollIntervalMs, gateId})` (the service builds + owns + starts the provider). If disabled, registers only and stays inert.
- `Q_SIGNAL void entryObserved(const QVariantMap &entry)` — the QML-facing signal.

The hub subscribes to the bus's `EntryObserved` and maps each event to an `AccessEntry` `QVariantMap` at its boundary (the JSON→variant conversion happens here, once):

```
AccessEntry (QVariantMap):
  hasStudent : bool      // !event.subject.isEmpty()
  student    : QVariantMap// event.subject.toVariantMap() (empty map if unresolved)
  eventId    : QString    // event.correlationId
  at         : QDateTime   // event.at
```

Empty `subject` → `{ hasStudent:false, student:{}, eventId, at }`. No `card`/`reader` field is ever populated.

### 5. QML registration + `main.cpp` wiring

**Files:** `qt-app/quick/AccessControlHub.h`/`.cpp` (the hub — no QML macros), `qt-app/quick/AccessControlSingleton.h` (the `QML_FOREIGN` registration wrapper), `qt-app/quick/main.cpp` (extend). Both new headers are added to the `witsquickmodule` `SOURCES` in [quick/CMakeLists.txt](../../../qt-app/quick/CMakeLists.txt) so the module's `qmltyperegistrar` picks up the singleton.

The `LOAMS` module is registered **declaratively** via `qt_add_qml_module` ([quick/CMakeLists.txt:33](../../../qt-app/quick/CMakeLists.txt)), and its singletons use declarative `QML_SINGLETON` (e.g. [Navigator.h:13](../../../qt-app/quick/viewmodels/Navigator.h)). `qmlRegisterSingletonInstance` (procedural registration into that same declaratively-built URI) is therefore **not** used — it fights the generated `qmldir`.

`AccessControlHub` is a normal `QObject` **without** QML macros on it. Because `main` constructs `AccessControlHub accessControl;` (default-constructible), putting `QML_SINGLETON` + `create()` directly on it is unreliable — Qt can pick the accessible default constructor over `create()` and hand QML a *separate, uninitialized, engine-owned* instance (never applying `CppOwnership`). Instead expose the app-owned instance via a dedicated **`QML_FOREIGN` singleton wrapper** whose `create()` is the only construction path QML sees:

```cpp
// AccessControlSingleton.h — a registration shim, never instantiated by QML.
struct AccessControlSingleton {
    Q_GADGET
    QML_FOREIGN(AccessControlHub)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AccessControl)   // QML name "AccessControl"; C++ type stays AccessControlHub
public:
    static AccessControlHub *create(QQmlEngine *, QJSEngine *) {
        AccessControlHub *inst = AccessControlHub::instance();   // the main-owned instance
        Q_ASSERT_X(inst, "AccessControlSingleton::create",
                   "AccessControlHub::setInstance() must run before the engine loads");
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }
};
```

`AccessControlHub` exposes a narrowly-scoped `static AccessControlHub *instance()` / `static void setInstance(AccessControlHub *)`, set by `main` before `loadFromModule`. The `create()` fail-fast assertion guards the (test) case where the engine loads `AppShell` without an instance installed.

**Existing QML tests must install a hub.** `AppShell`'s kiosk surface now references the `AccessControl` singleton, so the QML harnesses that load it ([tst_appshell.cpp](../../../qt-app/quick/tests/tst_appshell.cpp), [tst_qml_kiosk.cpp](../../../qt-app/quick/tests/tst_qml_kiosk.cpp)) must, before creating the engine, construct a **disabled** `AccessControlHub` and keep it alive past the engine's teardown. To be robust against an inherited developer `WITS_ACCESS_CONTROL` env var, the harness either skips `initialize()` entirely or explicitly clears `WITS_ACCESS_CONTROL` and sets `accessControl.enabled=false` (via `AppSettings::isolateForTesting()`) so no polling starts. This is part of this slice's test wiring, not a follow-up. `main.cpp` constructs the hub **before** the `QQmlApplicationEngine` so ordinary reverse-stack destruction tears the engine down first and the hub second:

```cpp
QApplication app(argc, argv);
// ... existing --software / QQuickStyle / cached-branding setup ...

AccessControlHub accessControl;
accessControl.initialize();                 // reads flag; registers provider; builds+starts iff enabled
AccessControlHub::setInstance(&accessControl);

QQmlApplicationEngine engine;               // dies before accessControl
// ... existing objectCreationFailed + loadFromModule("LOAMS","AppShell") ...
```

QML then references it as the `AccessControl` singleton from `import LOAMS`, backed by the C++-owned instance (the `QML_NAMED_ELEMENT` name also sidesteps the `AccessControl` C++-namespace clash).

### 6. Kiosk subscription + `showUnknownEntry`

**Files:** `qt-app/quick/viewmodels/KioskViewModel.h` / `.cpp` (extend), the kiosk screen QML under `qt-app/quick/` (extend), `qt-app/tests/tst_*` (extend).

- `KioskViewModel` gains `Q_INVOKABLE void onEntryObserved(const QVariantMap &entry)`: if `entry["hasStudent"].toBool()` → `applyStudentLogin(QJsonObject::fromVariantMap(entry["student"].toMap()))` (the existing welcome-display + counter-bump + recent-feed seam, no backend POST); else → `showUnknownEntry()`.
- `KioskViewModel::showUnknownEntry()` sets a neutral status/toast ("Card not recognized") with **no** counter bump and **no** recent-feed entry.
- The kiosk screen QML adds `Connections { target: AccessControl; function onEntryObserved(entry) { kioskVm.onEntryObserved(entry) } }` — the VM's local id in [KioskScreen.qml:11](../../../qt-app/quick/qml/kiosk/KioskScreen.qml) is `kioskVm`, not `vm`. The subscription is scoped to the kiosk surface (torn down when admin shows). The provider keeps running while admin is shown; entries during that window are an accepted at-most-once display miss (attendance stays authoritative on the backend).

### 7. Enablement precedence

`AccessControlHub::initialize()` resolves enablement once, in this order:

1. `WITS_ACCESS_CONTROL` set to `1`/`true` (case-insensitive) → **force on** for this process (non-persistent dev override).
2. else `accessControl.enabled` (default `false`).
3. else `false`.

`WITS_ACCESS_CONTROL=0` is **not** a production force-off in this slice (only force-on is honored). Settings are read through **`AppSettings`** ([appsettings.h](../../../qt-app/core/appsettings.h)), the repository's mandatory `QSettings` subclass — **not** a raw `QSettings` — so hub tests use `AppSettings::isolateForTesting()` and never touch the developer's real `HKCU` hive. `accessControl.pollIntervalMs` (default 1500) is read and **validated**: a non-integer, zero, or negative value (which would busy-loop the poller) falls back to 1500, and a sane floor (e.g. 250 ms) is clamped. `gateId` (default `"turnstile"`) is read the same way. Runtime mutation of these is deferred to the Sub-plan 4 settings UI.

## Backward compatibility

Flag off (the default) → `initialize()` registers the provider with the factory but builds/starts nothing, the service stays disabled, no polling, no bus traffic. The `AccessControl` singleton exists but is inert; the kiosk `Connections` never fires. Zero behavior change. Removing the hub construction + the QML `Connections` + the flag removes the feature cleanly.

## Testing (TDD)

All via `wits_add_qttest()`; `OFFSCREEN` for any Quick/VM test. No live network — feed synthetic `QByteArray` payloads and a `CapturingNam` (the AccessDecisionService test idiom). Synthetic data only (no real PII).

- **`parseEntryEvent`** (pure): entry present maps `id`/`created_at`/`student` + composes `photo_url` from `photo_path`+`baseUrl`; `photo_url`-present passthrough; `photo`-absent → empty `photo_url`; `created_at` parsed as server-local then converted to UTC (assert the offset conversion, not a raw-UTC reinterpretation); `entry:null` → `valid,!hasEntry`; `student:null` → `!hasStudent`, empty student; `latest_id`/`id` parsed as `qint64`; malformed JSON / wrong shape / `status!="success"` / non-object-non-null entry / missing-or-non-positive `id` / unparseable `latest_id` or `created_at` / non-object-non-null `student` → `!valid` with `error`, no fabricated entry.
- **`TurnstileProvider`** (CapturingNam): first start sets `m_since=latestId`, emits nothing for history, state→Connected, **and issues/schedules the next `?since=<m_since>` poll** (regression assertion — baseline must transition into steady polling, not stop after one request); **a second (reconnect) start preserves `m_since` and resumes `?since=<m_since>` — does NOT re-baseline** (regression test for the discarded-during-outage bug), **and an entry returned in that reconnect response is emitted exactly once** (regression test that the reconnect GET is processed under normal steady-state rules, not merely acknowledged); steady-state drains oldest-first, one `EntryObserved` per entry with normalized subject + `gateId` from config, cursor advances, never >1 in-flight; a `valid` entry with `eventId<=m_since` → `Degraded`, no event, no advance (drain-loop guard); empty poll re-arms the single-shot timer; transport failure, timeout (simulated stalled reply → abort), and protocol-invalid each → **exactly one `Degraded`** transition, timer stopped, cursor unmoved, no event; `stop()` aborts the in-flight reply, and a simulated restart drops a stale generation-N reply.
- **`AccessControlHub`** (`AppSettings::isolateForTesting()` + a `CapturingNam` passed to the hub constructor so no real request is issued): flag-on builds + enables and maps `EntryObserved`→`entryObserved(QVariantMap)` with correct `hasStudent`; empty-subject event → `hasStudent:false, student:{}`; flag-off → registered-but-inert (no signal, no polling); `WITS_ACCESS_CONTROL` precedence over the setting; `pollIntervalMs` of 0/negative/non-integer falls back to 1500; empty/whitespace `gateId` falls back to `"turnstile"`.
- **`KioskViewModel`**: `onEntryObserved` with `hasStudent:true` drives the existing `applyStudentLogin` seam (student Q_PROPERTYs + counters); `hasStudent:false` calls `showUnknownEntry` (status only, no counter bump, no feed entry).

## Verification

```
cmake -S qt-app -B <build> -G Ninja -DCMAKE_PREFIX_PATH=... && cmake --build <build> --target WITSQuick
ctest --test-dir <build> --output-on-failure
```

End-to-end (manual): enable via `WITS_ACCESS_CONTROL=1`, run `WITSQuick`, insert/seed a `turnstile_events` row → the student surfaces natively on the kiosk (no bridge, `WITS.exe` uninvolved); an orphaned-card row shows the neutral unknown toast; toggle off → silent.

## Out of scope (deferred)

Admin Access Control page + runtime toggle and settings mutation (Sub-plan 4); Mock/Testing tab UI; real USB/SDK capture adapters; TLS + per-user identity/RBAC + device tokens (the go-live security gate); multi-gate registry; runtime base-URL config.
