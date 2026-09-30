# Access Control Admin Page (Sub-plan 4) — Design

**Status:** design approved 2026-09-30 (owner); codex-review APPROVE (2 rounds).
**Predecessor:** Sub-plan 3 (TurnstileProvider + native kiosk display), merged to
master (PR #58, squash `13d072a`). This sub-plan consumes the already-merged
`access_recent.php` (Sub-plan 2) and the already-built Access Control seam
(Sub-plan 1) + hub/singleton (Sub-plan 3).

## Goal

One focused **Access Control** admin page that shows the live turnstile feed and
its connection health, and lets an admin turn this app's monitoring on/off at
runtime. Client-only: `access_recent.php` already exists on master, so there are
**no backend changes**.

This also discharges the forward requirement carried over from the Sub-plan 3
spec: **service-owned per-poll communication-age freshness** before displaying
communication-age health.

## Scope (locked with owner)

**In:** a single admin page = recent-entries table (`access_recent.php`) + count
tiles (`entries_today`, `last_entry_at`) + live connection-health tiles
(connection state + contact age) + a runtime **Enable turnstile monitoring**
toggle. Behind `accessControl.enabled` (default off) exactly as today — a fresh
install shows the page but monitoring starts off.

**Out (later sub-plans / follow-ups):** Providers schema-driven config tab,
Testing/Mock tab, photo thumbnails in the table, auto/live table refresh, a
gate/site-name field.

## Architecture — two data sources, clean split

The screen binds to **two** objects. This separation is deliberate (rejected
alternatives below).

1. **`AccessControl` singleton (the existing `AccessControlHub`)** — *live,
   in-process, app-global state*: the enable toggle, the connection-state pill,
   and the contact-age tile. This state lives in the already-running service and
   the singleton is always alive, so it is its natural home.
2. **`AccessControlViewModel` (new, page-scoped)** — *admin-authenticated
   endpoint data*: the recent-entries table and the `entries_today` /
   `last_entry_at` counts, fetched from `access_recent.php` with the admin key.

**Rejected:** *VM-owns-everything* — the VM is created/destroyed with the admin
page, but enable state is app-global, so the VM would have to proxy and duplicate
the singleton's signals and lifetime. *Hub-owns-everything* — pushing
admin-key'd endpoint fetching into the always-alive global singleton is a
layering violation (the singleton has no admin session and outlives the page).

## The two-timestamp rule (refinement 1)

The table and counts are **snapshots**; the connection status is **live**. The UI
must not let a healthy connection imply current table data. So two independent,
separately-labeled timestamps:

- **"Updated HH:MM:SS"** — the client wall-clock time of the last *successful*
  `access_recent.php` fetch. Owned by `AccessControlViewModel`. Frozen on refresh
  failure (see refinement 6).
- **"Last contact …"** — the live per-poll communication age from the service's
  HealthMonitor (refinements 3–4). Owned by the `AccessControl` singleton.

The table auto-loads when the page opens, and a **Refresh** button re-fetches on
demand. No auto/live refresh in this slice.

## New client pieces

### `AccessControlViewModel` (`qt-app/quick/viewmodels/`, `QML_ELEMENT`)

Mirrors `DashboardViewModel` (admin-key POST to a read endpoint) and
`VisitLogsViewModel` (DI-NAM + generation guard + model + loading/error).

- Constructor `(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr)`
  — `nam` null in production (owns a fresh NAM), injected `CapturingNam` in tests.
- Owns an `AccessEntriesModel m_entries` (exposed `CONSTANT`).
- `Q_INVOKABLE void refresh()` — POSTs `ApiConfig::endpoint("access_recent.php")`
  via `HttpForm::formRequest` + `HttpForm::encodeForm({{"admin_key",
  AdminSession::instance().key()}})`. **admin_key travels in the POST body only,
  never a query string** (security-hygiene).
- Properties: `entriesToday:int`, `lastEntryAt:QString`, `updatedAt:QString`
  ("Updated …" source), `loading:bool`, `stale:bool`, `errorText:QString`,
  `authFailure:bool`, `AccessEntriesModel *entries`.
- Signals: standard `*Changed` only. Auth loss is surfaced through the
  `authFailure` property, mirroring `DatabaseViewModel` / `ImportViewModel`
  (`Q_PROPERTY(bool authFailure …)` + a fixed "re-enter via admin login"
  message) — see refinement 6. There is **no** app-wide session-expiry flow in
  the codebase today, and this slice does not invent one.
- Generation guard (`nextRequestSeq()` / `isCurrentRequest(seq)`) so a superseded
  refresh reply is dropped — same idiom as `VisitLogsViewModel`.
- Network-free seam `void applyRecent(const QByteArray &raw)` for tests and the
  reply handler.

### `AccessEntriesModel` (`qt-app/quick/models/`, `QAbstractListModel`)

- Roles: `name`, `schoolId`, `course`, `department`, `createdAt`, `reader`
  (gate/lane), `card`, and a display flag `known` (false when the endpoint's
  `student` is null → the row renders **"Unknown card"**).
- **Text rows only — no photo thumbnails** in this slice, so the Sub-plan 3
  photo-origin problem is not re-introduced into the table (a thumbnail column
  is an explicit follow-up and would reuse the same-origin composition).

### Pure parser `LoginParser::parseRecentFeed(const QByteArray &body)`

Mirrors `parseEntryEvent`: returns a small `RecentFeedResult { bool valid;
QVector<RecentEntry> entries; int entriesToday; QString lastEntryAt; QString
error; }`, so decoding the feed (including validity vs. malformed) is network-free
testable. `valid` is true for a well-formed `status:"success"` response
**including an empty `entries` array**; malformed JSON / wrong shape / non-success
status → `valid=false` with `error` set (feeds the refresh-failure path).

## Hub extensions — `AccessControlHub` (the `AccessControl` singleton)

Today the hub exposes only `isAccessEnabled()` (a plain method returning
`m_service->isEnabled()`) and `entryObserved`; the descriptor and config are
locals inside `initialize()` (`AccessControlHub.cpp`). This slice adds, as
explicit implementation requirements:

- **Retained state:** `m_descriptor` (`ProviderDescriptor`), `m_config`
  (`QVariantMap`, the same raw pollIntervalMs/gateId map `initialize()` builds
  today), `m_providerId` (= `m_descriptor.providerId`), `m_intentEnabled` (bool) and
  `m_enableLocked` (bool). `initialize()` fills all five on **every** path —
  including the flag-off early return, so a later runtime enable has a
  descriptor + config to pass.
- **Q_PROPERTYs + NOTIFY signals:** `accessEnabled`/`accessEnabledChanged`,
  `enableLocked` (CONSTANT), `connectionState`/`connectionStateChanged`,
  `lastContactAt`/`lastContactChanged`, plus `Q_INVOKABLE setAccessEnabled`.
  `isAccessEnabled()` now returns `m_intentEnabled` (intent), not the service
  flag — existing callers (`main.cpp`, `tst_accesscontrolhub`) must keep
  passing, which holds because intent == service-enabled on every path except a
  refused disable.
- **Signal plumbing (constructor):** `AccessControlService::connectionStateChanged`
  → hub re-emits `connectionStateChanged`; `HealthMonitor::healthChanged`
  (via `m_service->healthMonitor()`) → hub emits `lastContactChanged` **only**
  when the snapshot's `providerId == m_providerId` and its `lastCommTime`
  actually changed (so state-only health updates don't churn the view).
  `lastContactAt()` reads `healthMonitor()->snapshot(m_providerId).lastCommTime`.
- `connectionState` is exposed as `int` (the `ConnectionState` enum value);
  the QML maps ints to labels. `ConnectionState` is a plain `enum class` with no
  `Q_ENUM`, so the mapping order (Disconnected=0 … Error=4) is pinned by a
  hub test rather than a QML enum import.

### Enable = requested intent, separate from connection outcome (refinement 5)

- `Q_PROPERTY(bool accessEnabled READ isAccessEnabled NOTIFY accessEnabledChanged)`
  — the *requested / persisted* monitoring intent, **not** the connection result.
- `Q_INVOKABLE void setAccessEnabled(bool on)`:
  - **Idempotent** — if `on == isAccessEnabled()`, no-op (no re-enable churn, no
    duplicate persist/signal).
  - On enable: persist `accessControl.enabled = true` to `AppSettings` **first**
    (the user asked for monitoring), then `m_service->enable(descriptor, config)`.
  - On disable: refuse when `enableLocked` is true (env override, below);
    otherwise persist `false`, then `m_service->disable()`.
  - **Persistence on startup failure:** a failed connect does **not** revert the
    persisted intent. `accessEnabled` stays true; the page shows
    enabled-but-Error while the service's own reconnect/backoff retries. Intent
    and outcome are independent.
- `Q_PROPERTY(bool enableLocked READ isEnableLocked CONSTANT)` — true when
  `WITS_ACCESS_CONTROL` forces on. The QML switch renders read-only and the page
  shows an inline note explaining the override (refinement 2).

### Connection + contact-age surface

- `Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)`
  — proxies `m_service->connectionState()` (the `ConnectionState` enum), driving
  the status pill. (`Disconnected`/`Connecting`/`Connected`/`Degraded`/`Error`.)
- `Q_PROPERTY(QDateTime lastContactAt READ lastContactAt NOTIFY lastContactChanged)`
  — the HealthMonitor's last comm time for the active provider (empty/null when
  none yet). The **age string** is computed in the view against a presentation
  timer (refinement 4), not stored as frozen text on the hub.

## Service-owned per-poll comm freshness (refinements 3–4; discharges the SP3 defer)

Today comm-time is recorded only on the `Connected` transition, so during steady
empty polling "last contact" would freeze. Fix while keeping the Sub-plan 3 rule
that **the provider never touches HealthMonitor**:

- Add an `IAccessProvider` signal `void polled(const QDateTime &at)`.
- **`TurnstileProvider` emits `polled(now)` only after a response is validated as
  a successful poll**, i.e. `parseEntryEvent(...).valid` is true — which
  **includes a valid empty poll** (`entry: null`). It must **not** emit on:
  transport error, non-2xx HTTP, or a malformed/failed-validation payload (those
  route to `fail()` → `Degraded`, no freshness bump). The timestamp is the
  **client's response-completion time** (`QDateTime::currentDateTimeUtc()` at
  handling), not a server field.
- The existing generation guard already drops replies from a stopped/replaced
  provider **before** any emit, so a late callback after `disable()`/restart
  cannot emit `polled` (refinement 3, and the "late response after disable" test).
- `AccessControlService` connects `polled` → `m_health->recordCommTime(providerId,
  at)`. The **service owns the recording**; the provider only reports the raw
  comm event. `MockProvider` implements the new signal (no-op or on synthetic
  fire) to satisfy the interface.

### Contact age must actually advance (refinement 4)

A timestamp notification alone does not update "N seconds ago" while nothing new
arrives (e.g. during an outage the age must keep climbing). So:

- The **view** recomputes the age from `AccessControl.lastContactAt` on a
  lightweight presentation `Timer` (≈1 s) — a pure formatting concern, no polling.
- The age formatter is a **pure, testable function** with three explicit states:
  - **"No contact yet"** — monitoring on but `lastContactAt` is null.
  - **"Monitoring off"** — `accessEnabled` is false.
  - **"Last contact 3 s ago" / "… 2 min ago"** — a prior contact timestamp.
- The tile is labeled **feed/API connection health**, never hardware health —
  `access_recent.php` / the display endpoint prove the app↔backend feed is
  alive, not that the physical gate hardware is healthy.

## Screen + wiring

### `AccessControlScreen.qml` (`qt-app/quick/qml/admin/`, `property var vm`)

- Header **LCard** with an **Enable turnstile monitoring** toggle built from
  the existing `LCheckbox` component (there is no `LSwitch`; adding a new
  switch primitive is out of scope). `checked` binds to
  `AccessControl.accessEnabled`; `onToggled(checked)` calls
  `AccessControl.setAccessEnabled(checked)` (the control must not hold its own
  diverging state — after a refused/locked call the binding re-asserts the
  hub's value). `enabled: !AccessControl.enableLocked`. Helper text: *"Controls this
  app's turnstile event polling and kiosk display. It does not disable the
  physical gate or stop server-side attendance recording."* When locked, an
  inline note explains the environment override.
- **LStatTile** row: `entries_today`, `last_entry_at`, connection-state pill
  (from `connectionState`), and the contact-age tile (presentation timer).
- The **"Updated HH:MM:SS"** label sits with the table/counts (VM `updatedAt`),
  visually separate from the connection tiles, with a **stale** badge when
  `vm.stale`.
- Recent-entries **LTable**/list bound to `vm.entries`; **Refresh** `LButton`
  (calls `vm.refresh()`, shows `vm.loading`); `LToast` for `vm.errorText`.
- Distinct **empty state** ("No entries yet") when a load succeeds with zero rows,
  vs. a **failed-initial-load** state (refinement 6). All colors via `Theme.*`.

### Nav + registration

- `Navigator::AdminPage` (`Navigator.h`) — append `AccessControl` to the enum
  (appended last so existing enum values don't shift).
- `AdminScreen.qml` — every place that enumerates pages must gain the new
  entry, or the page is half-wired:
  1. the `AccessControlViewModel` instance (alongside the other page VMs);
  2. `pageTitle` switch → `qsTr("Access Control")`;
  3. `LSideNav.currentPage` string mapping → `"accesscontrol"`;
  4. `LSideNav.items` → `{ page: "accesscontrol", label: qsTr("Access Control"), enabled: true }`;
  5. `onPageActivated` explicit key branch → `Navigator.showAdminPage(Navigator.AccessControl)`;
  6. `pageLoader.sourceComponent` switch → `accessControlComponent`, plus the
     `Component { AccessControlScreen { vm: … } }` definition; the Loader's
     existing feature-detected `refresh()` on page show provides the
     auto-load-on-open (so stub-VM QuickTests stay offline).
- `qt-app/quick/CMakeLists.txt` — register the new VM, model, screen.

## Refresh-failure behavior (refinement 6)

`AccessControlViewModel` distinguishes:

- **Ordinary failure after a prior successful load** (network error, HTTP 5xx,
  malformed): **keep** the previously loaded rows, set `stale = true`, set
  `errorText`, and **freeze** `updatedAt` at the last success. The user still sees
  the last-known feed, clearly marked stale.
- **Auth loss (HTTP 401):** set `authFailure = true` and `errorText` to the
  same fixed message `DatabaseViewModel` uses ("Admin authentication failed —
  re-enter via admin login."), and **clear** the protected page data (empty the
  model, reset counts, clear `updatedAt`, `stale = false`) so protected rows
  aren't left on screen under a rejected key. The page renders this as an
  inline auth-error state. It does **not** clear `AdminSession` or navigate
  away — no admin page does that today; an app-wide session-expiry flow is a
  separate follow-up. A later successful refresh resets `authFailure = false`.
- **Empty feed** (valid `success`, `entries: []`): a distinct **"No entries yet"**
  state — never conflated with a **failed initial load** (no prior rows +
  error).

## Testing (TDD)

- **VM** (`CapturingNam`): `refresh()` puts `admin_key` in the POST body (not the
  URL); `applyRecent` fills model + `entriesToday`/`lastEntryAt`/`updatedAt`;
  `authFailure` set on 401 + protected data cleared (and reset by a later success); ordinary failure keeps rows +
  sets `stale` + freezes `updatedAt`; empty feed → empty state (not error);
  generation guard drops a superseded reply.
- **Parser**: `parseRecentFeed` valid list / null-student row / empty-but-valid /
  malformed / counts extraction.
- **Model**: row role shapes; unknown-card row when `student` null.
- **Hub**: `setAccessEnabled(true/false)` enables/disables the service + persists
  to `AppSettings` (isolated) + is idempotent; `enableLocked` refuses disable and
  persists intent through a simulated startup failure without reverting;
  `connectionState` / `lastContactAt` proxy the service.
- **Service (per-poll freshness)** — multi-poll via `SequencedNam`:
  - **Invalid polls do not advance freshness** — a transport error / non-2xx /
    malformed response leaves `recordCommTime` uncalled (HealthMonitor last-comm
    unchanged); a **valid empty** poll **does** advance it.
  - **Late response after disable is ignored** — a reply completing after
    `disable()` (stale generation) emits no `polled` and does not record comm.
- **Presentation (age)** — pure age formatter: **contact age increases without
  new events** (same `lastContactAt`, advancing "now" → larger age); the three
  states ("No contact yet" / "Monitoring off" / prior timestamp) render correctly.
- **QML**: `AccessControlScreen` with a stub VM + a null-VM mount in the admin
  QML harness (defensive, matching the SP3 pattern).

Register C++ tests via `wits_add_qttest(... OFFSCREEN)` for any GUI/Quick test.

## Backward compatibility

Additive. Behind `accessControl.enabled` (default false): the page appears in
admin nav but monitoring stays off until toggled (or `WITS_ACCESS_CONTROL`
forces on). No change to the kiosk, existing endpoints, or legacy paths. Removing
the page = drop the nav entry + the three files + the `polled` wiring (leaf).

## Security notes

- `admin_key` in the POST body only, never a query string; 401 → `authFailure`
  + clear protected data.
- Cleartext HTTP + admin-key remains the accepted debt closed by the parallel
  TLS/RBAC go-live track — not a new finding for this slice.
- No real student PII in fixtures/tests; synthetic data only.

## Verification

`cmake --build <build> --target WITSQuick`; `ctest --output-on-failure` (full
suite incl. the new VM/parser/model/hub/service/QML tests). Manual GUI smoke:
flag-off page inert; toggle **Enable turnstile monitoring** on → service starts,
pill goes Connecting→Connected, contact age ticks, kiosk display live; seed a
`turnstile_events` row → it appears on Refresh with the right counts; toggle off →
"Monitoring off". With `WITS_ACCESS_CONTROL=1` the switch is locked-on with the
inline override note.
