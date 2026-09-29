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
- **Lifetime:** C++ owns the lifecycle; QML sees the instance via `qmlRegisterSingletonInstance` and never deletes it. Provider / polling cursor / health are application-lifetime; the kiosk↔presentation connection lives and dies with the QML surface.
- **Config:** `QSettings accessControl.enabled` (default false), `accessControl.pollIntervalMs` (default 1500); default/only v1 provider is `TurnstileProvider`; base URL from `ApiConfig::baseUrl()`. `WITS_ACCESS_CONTROL=1`/`true` force-enables for the current process (non-persistent). Config read once at app init; runtime mutation deferred to Sub-plan 4.

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
    QDateTime   at;                 // entry timestamp (UTC-parsed)
    QString     error;              // human-readable reason when !valid
};

EntryEventResult parseEntryEvent(const QByteArray &body, const QUrl &baseUrl);
```

**Rules (unambiguous):**
- Well-formed `{"status":"success","latest_id":N,"entry":null}` → `valid=true, hasEntry=false` (healthy empty poll).
- Well-formed with an `entry` object → `valid=true, hasEntry=true`, `eventId`/`at`/`latestId` populated; `student` normalized when the entry's `student` is a non-null object (`hasStudent=true`), else `hasStudent=false` and `student` empty (the merged endpoint's defensive orphaned-row case).
- Not JSON, wrong top-level shape, `status!="success"`, or a present-but-non-object `entry` → `valid=false`, `error` set, everything else default. **Never fabricates an entry.**
- **Photo composition (tolerant, pure):** in the student object, if `photo_url` is present and non-empty → keep it; else if `photo_path` (relative) is present → set `photo_url = baseUrl` resolved against `photo_path` (`QUrl::resolved`); else leave `photo_url` empty. `baseUrl` is passed in — the parser never reads `ApiConfig`. Output student always carries `photo_url` (the key `KioskViewModel::applyStudentLogin` reads at `KioskViewModel.cpp:123`).

### 2. `TurnstileProvider` — server-observed `IAccessProvider`

**Files:** `qt-app/core/accesscontrol/turnstileprovider.h` / `.cpp` (new, folded into `witscore`), `qt-app/tests/tst_turnstileprovider*` (new).

Implements `IAccessProvider` (`descriptor()`, `start()`, `stop()`, `state()`; signals `accessEvent`, `stateChanged`, `hardwareError`). Mirrors the `AccessDecisionService` idiom: **injected** `QNetworkAccessManager` (not owned), a pure static/`parseEntryEvent`-based decode, and a **generation counter** so a stale in-flight reply cannot mutate state.

- **Construction/config:** `pollIntervalMs` (from descriptor config), `baseUrl` (`ApiConfig::baseUrl()`, supplied by the hub at construction). `providerId = "turnstile"`, human name "Turnstile (server-observed)". `descriptor()` advertises a `pollIntervalMs` int in its config schema.
- **Cursor:** a `qint64 m_since` (also `latestId`/`eventId` are `qint64` throughout).
- **Start = baseline:** issue one GET to `turnstile_display.php` (no `?since` or `?since=0`). On a `valid` response: `m_since = latestId` (skip history — do **not** emit the returned historical entry), state → `Connected`, and `HealthMonitor.recordCommTime()`. On transport/protocol failure: `Degraded`/`Error` (see retry split).
- **Steady-state poll (healthy-only, this provider's sole retry surface):** GET `turnstile_display.php?since=<m_since>`.
  - `valid && hasEntry` → build and emit `AccessEvent{type=EntryObserved, subject=<student or empty>, gateId, correlationId=QString::number(eventId), at}`, set `m_since = eventId`, `recordCommTime()`, then **immediately** issue the next GET (async, one-at-a-time drain — never more than one request in flight).
  - `valid && !hasEntry` (empty poll) → `recordCommTime()` (a healthy empty poll is still comm evidence) and **arm the `pollIntervalMs` timer** for the next poll.
  - `!valid` (protocol failure) or transport failure/timeout → **stop the poll timer**, emit `stateChanged(Degraded/Error)`; do **not** move the cursor, do **not** fabricate an event. Reconnect is the service's job.
- **`stop()`** halts the timer and bumps the generation so any in-flight reply is ignored.
- **Generation guard** applies to `stop()` **and** service-triggered restarts: a reply tagged generation N is dropped once N+1 has begun, so a stale reply can never advance the cursor or emit after a restart.
- **Read-only:** never POSTs; the backend stays the sole attendance writer.

**Retry ownership (the one-layer rule):** the provider only ever re-arms its timer after a *successful empty* poll. Every failure path stops that timer and defers to `AccessControlService`'s existing state machine + exponential backoff reconnect (Sub-plan 1). The two mechanisms are mutually exclusive by construction.

### 3. `AccessEvent` unknown-student convention

**Files:** `qt-app/core/accesscontrol/accesstypes.h` (document the convention; no struct change — `subject` already exists at `accesstypes.h:43`).

For an `EntryObserved` event: **empty `subject`** = "entry observed, subject unresolved" (orphaned/deleted student); **non-empty `subject`** = resolved student. `correlationId` carries the `eventId`. `gateId` set from the provider's gate. Raw `card`/`reader` are never placed on the event.

### 4. `AccessControlHub` — application-owned composition root + QML singleton

**Files:** `qt-app/quick/AccessControlHub.h` / `.cpp` (new), registered in `qt-app/quick/CMakeLists.txt`; `qt-app/tests/tst_accesscontrolhub*` (new).

`QObject` subclass named **`AccessControlHub`** (the QML singleton is registered under the name **`AccessControl`**; the class is not named `AccessControl` to avoid clashing with the existing `AccessControl` C++ namespace). Owns `EventBus`, `AccessProviderFactory`, `AccessControlService` (all parented). Public surface:

- `void initialize()` — reads enablement (see §7). Registers `TurnstileProvider` with the factory. If enabled, calls `service.enable(descriptor, {pollIntervalMs})` (the service builds + owns + starts the provider). If disabled, builds nothing and stays inert.
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

### 5. `main.cpp` wiring

**Files:** `qt-app/quick/main.cpp` (extend).

Construct the stack-owned hub **before** the `QQmlApplicationEngine` and register that exact instance, so ordinary reverse-stack destruction tears the engine down first and the hub second (QML never deletes it):

```cpp
QApplication app(argc, argv);
// ... existing --software / QQuickStyle / cached-branding setup ...

AccessControlHub accessControl;
accessControl.initialize();                     // reads flag; builds+starts provider iff enabled
qmlRegisterSingletonInstance("LOAMS", 1, 0, "AccessControl", &accessControl);

QQmlApplicationEngine engine;                   // dies before accessControl
// ... existing objectCreationFailed + loadFromModule("LOAMS","AppShell") ...
```

### 6. Kiosk subscription + `showUnknownEntry`

**Files:** `qt-app/quick/viewmodels/KioskViewModel.h` / `.cpp` (extend), the kiosk screen QML under `qt-app/quick/` (extend), `qt-app/tests/tst_*` (extend).

- `KioskViewModel` gains `Q_INVOKABLE void onEntryObserved(const QVariantMap &entry)`: if `entry["hasStudent"].toBool()` → `applyStudentLogin(QJsonObject::fromVariantMap(entry["student"].toMap()))` (the existing welcome-display + counter-bump + recent-feed seam, no backend POST); else → `showUnknownEntry()`.
- `KioskViewModel::showUnknownEntry()` sets a neutral status/toast ("Card not recognized") with **no** counter bump and **no** recent-feed entry.
- The kiosk screen QML adds `Connections { target: AccessControl; function onEntryObserved(entry) { vm.onEntryObserved(entry) } }`, so the subscription is scoped to the kiosk surface (torn down when admin shows). The provider keeps running while admin is shown; entries during that window are an accepted at-most-once display miss (attendance stays authoritative on the backend).

### 7. Enablement precedence

`AccessControlHub::initialize()` resolves enablement once, in this order:

1. `WITS_ACCESS_CONTROL` set to `1`/`true` (case-insensitive) → **force on** for this process (non-persistent dev override).
2. else `QSettings` `accessControl.enabled` (default `false`).
3. else `false`.

`WITS_ACCESS_CONTROL=0` is **not** a production force-off in this slice (only force-on is honored). `accessControl.pollIntervalMs` (default 1500) is read from `QSettings`. Runtime mutation of these is deferred to the Sub-plan 4 settings UI.

## Backward compatibility

Flag off (the default) → `initialize()` registers the provider with the factory but builds/starts nothing, the service stays disabled, no polling, no bus traffic. The `AccessControl` singleton exists but is inert; the kiosk `Connections` never fires. Zero behavior change. Removing the hub construction + the QML `Connections` + the flag removes the feature cleanly.

## Testing (TDD)

All via `wits_add_qttest()`; `OFFSCREEN` for any Quick/VM test. No live network — feed synthetic `QByteArray` payloads and a `CapturingNam` (the AccessDecisionService test idiom). Synthetic data only (no real PII).

- **`parseEntryEvent`** (pure): entry present maps all fields + composes `photo_url` from `photo_path`+`baseUrl`; `photo_url`-present passthrough; `photo`-absent → empty `photo_url`; `entry:null` → `valid,!hasEntry`; `student:null` → `!hasStudent`, empty student; `latest_id` parsed as `qint64`; malformed JSON / wrong shape / `status!="success"` / non-object entry → `!valid` with `error`, no fabricated entry.
- **`TurnstileProvider`** (CapturingNam): baseline-on-start sets `m_since=latestId` and emits nothing for history, state→Connected, `recordCommTime` called; steady-state drains oldest-first, one `EntryObserved` per entry with normalized subject, cursor advances, never >1 in-flight; empty poll re-arms the timer + records comm; transport failure and protocol-invalid both → `Degraded`/`Error`, timer stopped, cursor unmoved, no event; `stop()` and a simulated restart drop a stale generation-N reply.
- **`AccessControlHub`**: flag-on builds + enables and maps `EntryObserved`→`entryObserved(QVariantMap)` with correct `hasStudent`; empty-subject event → `hasStudent:false, student:{}`; flag-off → inert (no signal, no polling). `WITS_ACCESS_CONTROL` precedence over `QSettings`.
- **`KioskViewModel`**: `onEntryObserved` with `hasStudent:true` drives the existing `applyStudentLogin` seam (student Q_PROPERTYs + counters); `hasStudent:false` calls `showUnknownEntry` (status only, no counter bump, no feed entry).

## Verification

```
cmake -S qt-app -B <build> -G Ninja -DCMAKE_PREFIX_PATH=... && cmake --build <build> --target WITSQuick
ctest --test-dir <build> --output-on-failure
```

End-to-end (manual): enable via `WITS_ACCESS_CONTROL=1`, run `WITSQuick`, insert/seed a `turnstile_events` row → the student surfaces natively on the kiosk (no bridge, `WITS.exe` uninvolved); an orphaned-card row shows the neutral unknown toast; toggle off → silent.

## Out of scope (deferred)

Admin Access Control page + runtime toggle and settings mutation (Sub-plan 4); Mock/Testing tab UI; real USB/SDK capture adapters; TLS + per-user identity/RBAC + device tokens (the go-live security gate); multi-gate registry; runtime base-URL config.
