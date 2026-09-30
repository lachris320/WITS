# Access Control Admin Page (Sub-plan 4) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one focused **Access Control** admin page to LOAMS 2.0 (`WITSQuick`) that shows the recent turnstile feed (`access_recent.php`) with counts, shows live feed-connection health (state + a contact age that keeps advancing), and lets an admin turn this app's turnstile monitoring on/off at runtime — client-only, no backend changes, behind `accessControl.enabled` (default off).

**Architecture:** Two data sources, deliberately split. (1) The existing app-global `AccessControlHub` (QML singleton `AccessControl`) gains retained descriptor/config/intent/lock state, an idempotent persist-first `setAccessEnabled`, and live `accessEnabled` / `enableLocked` / `connectionState` / `lastContactAt` properties relayed from `AccessControlService` + its `HealthMonitor`. Freshness is made per-poll by a new `IAccessProvider::polled(QDateTime)` signal that `TurnstileProvider` emits only on a validated successful poll and that the **service** records into `HealthMonitor` (the provider never touches it). (2) A new page-scoped `AccessControlViewModel` fetches `access_recent.php` (admin key in the POST body), decodes it with a pure `LoginParser::parseRecentFeed`, fills an `AccessEntriesModel`, and owns the snapshot "Updated HH:MM:SS" time plus stale / auth-failure / empty / failed-initial states. A pure `AccessControl::formatContactAge` renders the age tile on a 1 s presentation timer in `AccessControlScreen.qml`.

**Tech Stack:** Qt 6.11 / C++17, CMake + Ninja (MinGW), QtTest (`wits_add_qttest`), QML / Qt Quick (MVVM), Qt Quick Test. PHP backend already merged (read-only; `access_recent.php` from Sub-plan 2).

## Global Constraints

- **Feature flag:** every runtime effect stays behind `accessControl.enabled` (default **false**); a fresh install shows the page but monitoring starts off.
- **Enablement precedence (unchanged):** `WITS_ACCESS_CONTROL` = `1`/`true` (case-insensitive) force-on → else `accessControl/enabled` → else false; `WITS_ACCESS_CONTROL=0` is **not** a force-off.
- **Intent ≠ outcome:** `accessEnabled` is the requested/persisted intent, never the connection result; a failed connect does **not** revert the persisted intent.
- **Env lock:** `enableLocked` is true exactly when `WITS_ACCESS_CONTROL` forces on; a locked hub refuses disable.
- **Settings:** all reads/writes go through **`AppSettings`** (never raw `QSettings`); tests isolate via `AppSettings::isolateForTesting()`. Key written by the toggle: `accessControl/enabled` (bool).
- **Provider never touches HealthMonitor:** `TurnstileProvider` only emits `polled(at)`; `AccessControlService` owns `recordCommTime`.
- **`polled` timestamp** = client response-completion time, `QDateTime::currentDateTimeUtc()` at handling — never a server field.
- **No `polled` on:** transport error, non-2xx, malformed/failed-validation payload (incl. non-advancing entry), or any stale-generation reply.
- **Security:** `admin_key` travels **in the POST body only**, never a query string; HTTP 401 → `authFailure` + clear protected page data.
- **Auth-failure message (fixed):** `Admin authentication failed — re-enter via admin login.` (same as `DatabaseViewModel`).
- **Two timestamps:** "Updated HH:MM:SS" (VM, last successful fetch, frozen on failure) is visually separate from "Last contact …" (hub, live).
- **Table:** text rows only — no photo thumbnails; `student: null` renders **"Unknown card"**.
- **QML module target is `witsquickmodule`** (URI `LOAMS`), never `witsquick`.
- **MVVM:** ViewModels in `qt-app/quick/viewmodels/` are the only QML-facing C++ besides the existing `AccessControl` singleton; screens take `property var vm`.
- **Theming:** every color comes from `Theme.*`; **zero raw hex** outside `Theme.qml`; opacity via `Qt.alpha(Theme.<token>, a)`.
- **Naming:** QML types / C++ VM + model classes `PascalCase`; C++ members `m_camelCase`.
- **Tests:** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`); add `OFFSCREEN` for any GUI/Quick/painting test; no live network (use `CapturingNam` / `SequencedNam`).
- **Fixtures:** synthetic data only (`Test Student A`, `TEST-0001`, `CARD0012`, key `sp4-test-key`) — no real student PII, no real admin key.
- **QObject/ownership:** parent every `QObject`; function-pointer `connect` syntax.
- **Commits:** via the `commit` skill; Conventional Commits; **no Claude/Anthropic co-author trailer** (standing user rule).

### Build & test commands (this machine)

Qt tools are **not** on `PATH`, and the in-tree build path overflows Windows MAX_PATH for the QML module — build into the **short external dir `C:/b/loams-sp4`**. Prepend the kit in the **same** PowerShell command (shell state does not persist):

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4 --target <target>
ctest --test-dir C:/b/loams-sp4 -R <name> --output-on-failure
```

Every command block below assumes the `$env:PATH = ...` line was prepended in the same invocation. Ignore the harmless `LF will be replaced by CRLF` and the pre-existing `QXlsx ... GuiPrivate` CMake warnings. **Baseline: 54 ctest tests, all green.**

---

## File Structure

| File | Responsibility | Task |
|---|---|---|
| `qt-app/core/accesscontrol/iaccessprovider.h` | Add `polled(const QDateTime &at)` signal | 1 |
| `qt-app/core/accesscontrol/turnstileprovider.cpp` | Emit `polled` only after a validated successful poll (anomaly check hoisted) | 1 |
| `qt-app/core/accesscontrol/mockprovider.h` / `.cpp` | `simulatePolled(QDateTime)` synthetic fire | 1 |
| `qt-app/core/accesscontrol/accesscontrolservice.h` / `.cpp` | Connect `polled` → `HealthMonitor::recordCommTime` (stale-provider guarded) | 1 |
| `qt-app/tests/tst_turnstileprovider.cpp` | `polled` emitted / not-emitted cases | 1 |
| `qt-app/tests/tst_accesscontrolservice.cpp` | Per-poll freshness via `SequencedNam` + mock | 1 |
| `qt-app/tests/tst_mockprovider.cpp` | `simulatePolled` emits | 1 |
| `qt-app/tests/CMakeLists.txt` | `tst_accesscontrolservice` compiles TurnstileProvider + SequencedNam | 1 |
| `qt-app/core/loginparser.h` / `.cpp` | `RecentEntry`, `RecentFeedResult`, pure `parseRecentFeed` | 2 |
| `qt-app/tests/tst_loginparser.cpp` | `parseRecentFeed` cases | 2 |
| `qt-app/quick/models/AccessEntriesModel.h` / `.cpp` | Text-only recent-entries list model ("Unknown card" rows) | 3 |
| `qt-app/quick/tests/tst_accessentriesmodel.cpp` | Model role/shape tests (**new ctest**) | 3 |
| `qt-app/quick/viewmodels/AccessControlViewModel.h` / `.cpp` | Page VM: admin-key POST, generation guard, stale/auth/empty states | 4 |
| `qt-app/quick/tests/tst_accesscontrolviewmodel.cpp` | VM tests via `CapturingNam` (**new ctest**) | 4 |
| `qt-app/quick/AccessControlHub.h` / `.cpp` | Retained state, intent/lock, live props + relays | 5 (+6) |
| `qt-app/quick/tests/tst_accesscontrolhub.cpp` | Toggle/lock/relay tests | 5 (+6) |
| `qt-app/core/accesscontrol/contactage.h` / `.cpp` | Pure `formatContactAge` | 6 |
| `qt-app/tests/tst_contactage.cpp` | Formatter tests (**new ctest**) | 6 |
| `qt-app/core/CMakeLists.txt` | Add `contactage.*` to `witscore` | 6 |
| `qt-app/quick/qml/admin/AccessControlScreen.qml` | The page | 7 |
| `qt-app/quick/viewmodels/Navigator.h` | Append `AccessControl` to `AdminPage` | 7 |
| `qt-app/quick/qml/admin/AdminScreen.qml` | All six page-enumeration touch points | 7 |
| `qt-app/quick/tests/tst_qml_accesscontrol.qml` | Screen QuickTests (stub VM + stub hub, null-VM, default-singleton) | 7 |
| `qt-app/quick/tests/tst_qml_adminshell.qml` | Route/title/sidebar coverage for the new page | 7 |
| `qt-app/quick/tests/tst_qml_admin.cpp`, `tst_qml_theme.cpp`, `tst_qml_components.cpp` | Install a disabled hub (every QuickTest target runs every `tst_*.qml`) | 7 |
| `qt-app/quick/tests/tst_navigator.cpp` | Pin `AccessControl` appended last | 7 |
| `qt-app/quick/CMakeLists.txt` | Register model (3), VM (4), screen (7) + the two new quick ctests | 3, 4, 7 |
| `qt-app/tests/CMakeLists.txt` | Register `tst_contactage` | 6 |

Task order is dependency order: 1 → 2 → 3 → 4 → 5 → 6 → 7 (T4 needs T2+T3; T5's contact test needs T1; T6 extends the T5 hub; T7 needs everything).

---

## Task 1: Per-poll communication freshness (`IAccessProvider::polled`)

**Files:**
- Modify: `qt-app/core/accesscontrol/iaccessprovider.h`, `qt-app/core/accesscontrol/turnstileprovider.cpp`, `qt-app/core/accesscontrol/mockprovider.h`, `qt-app/core/accesscontrol/mockprovider.cpp`, `qt-app/core/accesscontrol/accesscontrolservice.h` (comment), `qt-app/core/accesscontrol/accesscontrolservice.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`
- Test: `qt-app/tests/tst_turnstileprovider.cpp`, `qt-app/tests/tst_accesscontrolservice.cpp`, `qt-app/tests/tst_mockprovider.cpp`

**Interfaces:**
- Consumes: existing `TurnstileProvider` (generation guard `m_generation`, `fail()`, `LoginParser::parseEntryEvent(...).valid`), `AccessControlService::enable()`, `HealthMonitor::recordCommTime(const QString &providerId, const QDateTime &at)`, `HealthMonitor::snapshot(providerId).lastCommTime`, `SequencedNam` (`enqueue(body, error)`, `enqueueStall()`, `requestCount()`; stamps HTTP 500 when `error != NoError`).
- Produces:
  - `signal void AccessControl::IAccessProvider::polled(const QDateTime &at);`
  - `void AccessControl::MockProvider::simulatePolled(const QDateTime &at);`
  - Service behavior: every `polled(at)` from the **current** provider while enabled → `m_health->recordCommTime(providerId, at)` (→ `HealthMonitor::healthChanged`).

**Two recording paths, both service-owned (decision: keep both).** `AccessControlService::onProviderState` already calls `m_health->recordCommTime(providerId, QDateTime::currentDateTimeUtc())` on every `Connected` transition (`accesscontrolservice.cpp`, the `case ConnectionState::Connected:` branch). That record **stays**: a Connected transition is itself a successful communication (for `TurnstileProvider` it is the validated baseline response), it is the *only* freshness source for providers that never emit `polled` (`MockProvider`, Sub-plan 1 behaviour), and existing tests rely on it. This task **adds** the per-poll path next to it; the provider still touches neither. **Pinned baseline behaviour:** `TurnstileProvider`'s baseline success **does** emit `polled` (exactly once, immediately before its `Connected` transition), so on (re)connect the same instant is recorded twice — once by `polled`, once by the Connected transition, milliseconds apart. That double record is harmless (`recordCommTime` is an idempotent overwrite with a monotonically non-decreasing client time) and is stated here so nobody "fixes" it. Tests pin all of it: `baselineSuccessEmitsPolledOnce` (provider), `connectedTransitionAndPolledAreBothServiceRecorded` (Connected records alone; a later `polled` advances further), `turnstileValidEmptyPollAdvancesFreshness` (steady valid polls advance beyond the Connected record), `turnstileInvalidPollDoesNotAdvanceFreshness` (invalid polls don't).

- [ ] **Step 1: Declare the signal + the mock driver (stub-first, so the red is a runtime failure)**

In `qt-app/core/accesscontrol/iaccessprovider.h`, add `#include <QDateTime>` next to `#include <QObject>`, and extend the `signals:` block:

```cpp
signals:
    void accessEvent(const AccessControl::AccessEvent &event);
    void stateChanged(AccessControl::ConnectionState state);
    void hardwareError(const QString &message);
    // One successful, VALIDATED poll/communication completed at `at` (client
    // clock, UTC). Emitted for an empty-but-valid poll too — it is the raw
    // freshness fact. The provider only reports it; AccessControlService owns
    // recording it into HealthMonitor. Never emitted on transport error,
    // non-2xx, malformed/failed-validation payloads, or stale-generation replies.
    void polled(const QDateTime &at);
```

In `qt-app/core/accesscontrol/mockprovider.h`, after `void simulateDisconnect();`:

```cpp
    void simulatePolled(const QDateTime &at);
```

In `qt-app/core/accesscontrol/mockprovider.cpp`, after `simulateDisconnect()`:

```cpp
void MockProvider::simulatePolled(const QDateTime &at)
{
    emit polled(at);   // synthetic per-poll freshness fact (drives service tests)
}
```

(`TurnstileProvider` and `AccessControlService` are untouched in this step, so the tests below fail at runtime, not at compile/link time.)

- [ ] **Step 2: Write the provider tests**

In `qt-app/tests/tst_turnstileprovider.cpp`, add to the `private slots:` block (after `void stopFromEntrySlotHaltsDrain();`):

```cpp
    void baselineSuccessEmitsPolledOnce();
    void polledOnValidEmptyPolls();
    void polledOnEntryPolls();
    void noPolledOnTransportError();
    void noPolledOnNon2xx();
    void noPolledOnMalformed();
    void noPolledOnNonAdvancingEntry();
    void noPolledFromReplyCompletingAfterStop();
```

Append the implementations above `QTEST_MAIN(TestTurnstileProvider)`:

```cpp
void TestTurnstileProvider::baselineSuccessEmitsPolledOnce()
{
    // Pinned behaviour: the BASELINE response is a validated successful poll,
    // so it emits polled exactly once (before the Connected transition).
    SequencedNam nam;
    nam.enqueue(emptyPayload(5));   // baseline: valid
    nam.enqueueStall();             // first steady poll: stays in flight (never completes here)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);   // baseline done, steady in flight
    QCOMPARE(p.state(), ConnectionState::Connected);
    QCOMPARE(polled.count(), 1);                                // the baseline, once
    p.stop();
}

void TestTurnstileProvider::polledOnValidEmptyPolls()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(5));   // baseline: valid, entry:null
    nam.enqueue(emptyPayload(5));   // steady empty poll: valid, entry:null
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    const QDateTime before = QDateTime::currentDateTimeUtc();
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(polled.count() >= 2, 3000);   // baseline + steady empty poll
    const QDateTime first = polled.at(0).at(0).toDateTime();
    const QDateTime second = polled.at(1).at(0).toDateTime();
    QVERIFY(first.isValid());
    QVERIFY(first >= before);                               // client completion time
    QVERIFY(second >= first);
    QVERIFY(second <= QDateTime::currentDateTimeUtc());
    p.stop();
}

void TestTurnstileProvider::polledOnEntryPolls()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(0));       // baseline
    nam.enqueue(entryPayload(1, 1));    // entry poll (valid, advancing)
    nam.enqueue(emptyPayload(1));       // drain end (valid empty)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(polled.count() >= 3, 3000);   // baseline + entry + drain-end
    p.stop();
}

void TestTurnstileProvider::noPolledOnTransportError()
{
    SequencedNam nam;
    nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);   // baseline: transport failure
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(polled.count(), 0);
}

void TestTurnstileProvider::noPolledOnNon2xx()
{
    SequencedNam nam;
    // SequencedNam stamps HTTP 500 whenever error != NoError: a non-2xx answer
    // that even carries a JSON body must not count as a successful poll.
    nam.enqueue(QByteArrayLiteral("{\"status\":\"error\",\"message\":\"Internal server error\"}"),
                QNetworkReply::InternalServerError);
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(polled.count(), 0);
}

void TestTurnstileProvider::noPolledOnMalformed()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(1));                  // baseline ok -> exactly one polled
    nam.enqueue(QByteArrayLiteral("not json"));    // steady poll: malformed
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(polled.count(), 1);                   // the baseline only
}

void TestTurnstileProvider::noPolledOnNonAdvancingEntry()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(7));       // baseline: cursor = 7 -> one polled
    nam.enqueue(entryPayload(7, 7));    // id == cursor: failed validation (protocol anomaly)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(polled.count(), 1);        // the anomalous poll bumped nothing
}

void TestTurnstileProvider::noPolledFromReplyCompletingAfterStop()
{
    SequencedNam nam;
    nam.enqueueStall();                 // baseline stays in flight
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 1, 3000);
    p.stop();                           // generation bump, then the reply completes (abort)
    QTest::qWait(100);                  // let the late finished() run
    QCOMPARE(polled.count(), 0);        // the generation guard dropped it before any emit
}
```

- [ ] **Step 3: Write the service + mock tests**

In `qt-app/tests/tst_accesscontrolservice.cpp`, add includes after the existing ones:

```cpp
#include <QNetworkReply>
#include <QUrl>
#include "accesscontrol/turnstileprovider.h"
#include "sequencednam.h"
```

Add to `private slots:` (after `void unexpectedDisconnectPublishesAndReconnects();`):

```cpp
    void polledRecordsCommTimeForActiveProvider();
    void connectedTransitionAndPolledAreBothServiceRecorded();
    void polledFromTornDownProviderIsIgnored();
    void turnstileValidEmptyPollAdvancesFreshness();
    void turnstileInvalidPollDoesNotAdvanceFreshness_data();
    void turnstileInvalidPollDoesNotAdvanceFreshness();
    void turnstileLateResponseAfterDisableIsIgnored();
```

Add these helpers below the existing `factoryCapturing` helper:

```cpp
// Factory whose "turnstile" creator builds a REAL TurnstileProvider on the
// injected SequencedNam — multi-poll freshness end to end, no live network.
static AccessProviderFactory turnstileFactory(SequencedNam *nam)
{
    AccessProviderFactory f;
    f.registerProvider(TurnstileProvider::defaultDescriptor(),
        [nam](const ProviderDescriptor &, const QVariantMap &cfg, QObject *parent)
            -> IAccessProvider * {
            return new TurnstileProvider(nam, QUrl(QStringLiteral("http://localhost/loams_api/")),
                                         cfg, parent);
        });
    return f;
}

static QVariantMap fastCfg()
{
    return QVariantMap{{QStringLiteral("pollIntervalMs"), 250},
                       {QStringLiteral("gateId"), QStringLiteral("g1")}};
}

static QByteArray emptyPoll(qint64 latest)
{
    return QStringLiteral("{\"status\":\"success\",\"latest_id\":%1,\"entry\":null}")
        .arg(latest).toUtf8();
}

static QByteArray entryPoll(qint64 latest, qint64 id)
{
    return QStringLiteral(
        "{\"status\":\"success\",\"latest_id\":%1,\"entry\":{\"id\":%2,"
        "\"card\":\"CARD0001\",\"created_at\":\"2026-09-30 08:30:00\",\"reader\":0,"
        "\"student\":null}}").arg(latest).arg(id).toUtf8();
}
```

Append the implementations above `QTEST_MAIN(TestAccessControlService)`:

```cpp
void TestAccessControlService::polledRecordsCommTimeForActiveProvider()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);

    const QDateTime at(QDate(2026, 9, 30), QTime(10, 0, 0), QTimeZone::UTC);
    QSignalSpy health(svc.healthMonitor(), &HealthMonitor::healthChanged);
    mock->simulatePolled(at);
    QCOMPARE(svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime, at);
    QVERIFY(health.count() >= 1);
}

void TestAccessControlService::connectedTransitionAndPolledAreBothServiceRecorded()
{
    // Two service-owned recording paths, both kept (see "Two recording paths"
    // above): (1) the Connected transition records the comm time on its own —
    // MockProvider never emits polled, yet lastCommTime is set; (2) a later
    // polled(at) advances it further. The provider touches neither.
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    const QDateTime beforeEnable = QDateTime::currentDateTimeUtc();
    svc.enable(MockProvider::defaultDescriptor(), {});       // Mock: Connecting -> Connected
    const QDateTime onConnected = svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime;
    QVERIFY(onConnected.isValid());                           // path (1), no polled involved
    QVERIFY(onConnected >= beforeEnable);

    const QDateTime later = onConnected.addSecs(30);
    mock->simulatePolled(later);                              // path (2)
    QCOMPARE(svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime, later);
}

void TestAccessControlService::polledFromTornDownProviderIsIgnored()
{
    EventBus bus;
    MockProvider *mock = nullptr;
    AccessProviderFactory f = factoryCapturing(&mock);
    AccessControlService svc(&bus, &f);
    svc.enable(MockProvider::defaultDescriptor(), {});
    QVERIFY(mock != nullptr);
    MockProvider *captured = mock;
    const QDateTime before = svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime;

    svc.disable();                     // severs the provider's signals + deleteLater
    const QDateTime late(QDate(2026, 9, 30), QTime(11, 0, 0), QTimeZone::UTC);
    captured->simulatePolled(late);    // a late callback from the torn-down provider
    QCOMPARE(svc.healthMonitor()->snapshot(QStringLiteral("mock")).lastCommTime, before);
}

void TestAccessControlService::turnstileValidEmptyPollAdvancesFreshness()
{
    SequencedNam nam;                  // unqueued requests answer a valid empty poll
    AccessProviderFactory f = turnstileFactory(&nam);
    EventBus bus;
    AccessControlService svc(&bus, &f);
    const QString id = QStringLiteral("turnstile");
    svc.enable(TurnstileProvider::defaultDescriptor(), fastCfg());
    QTRY_COMPARE_WITH_TIMEOUT(svc.connectionState(), ConnectionState::Connected, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(svc.healthMonitor()->snapshot(id).lastCommTime.isValid(), 3000);
    const QDateTime first = svc.healthMonitor()->snapshot(id).lastCommTime;
    // Steady-state empty polls never re-enter Connected, so ONLY the per-poll
    // polled -> recordCommTime wiring can move lastCommTime forward.
    QTRY_VERIFY_WITH_TIMEOUT(svc.healthMonitor()->snapshot(id).lastCommTime > first, 3000);
    svc.disable();
}

void TestAccessControlService::turnstileInvalidPollDoesNotAdvanceFreshness_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("error");   // QNetworkReply::NetworkError
    QTest::newRow("transport") << QByteArray() << int(QNetworkReply::HostNotFoundError);
    QTest::newRow("non-2xx")
        << QByteArrayLiteral("{\"status\":\"error\",\"message\":\"Internal server error\"}")
        << int(QNetworkReply::InternalServerError);
    QTest::newRow("malformed") << QByteArrayLiteral("not json") << int(QNetworkReply::NoError);
    QTest::newRow("non-advancing") << entryPoll(3, 3) << int(QNetworkReply::NoError);
}

void TestAccessControlService::turnstileInvalidPollDoesNotAdvanceFreshness()
{
    QFETCH(QByteArray, body);
    QFETCH(int, error);
    SequencedNam nam;
    nam.enqueue(emptyPoll(3));                                    // baseline: valid
    nam.enqueue(body, static_cast<QNetworkReply::NetworkError>(error));   // steady poll: invalid
    AccessProviderFactory f = turnstileFactory(&nam);
    EventBus bus;
    AccessControlService svc(&bus, &f);
    svc.setReconnectBaseMs(30000);    // no reconnect inside the observation window
    const QString id = QStringLiteral("turnstile");
    svc.enable(TurnstileProvider::defaultDescriptor(), fastCfg());

    // Capture freshness right after the valid baseline (the invalid steady poll
    // only fires one 250 ms interval later) ...
    QTRY_COMPARE_WITH_TIMEOUT(svc.connectionState(), ConnectionState::Connected, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(svc.healthMonitor()->snapshot(id).lastCommTime.isValid(), 3000);
    const QDateTime afterBaseline = svc.healthMonitor()->snapshot(id).lastCommTime;

    // ... then the invalid poll must degrade WITHOUT bumping it.
    QTRY_COMPARE_WITH_TIMEOUT(svc.connectionState(), ConnectionState::Degraded, 3000);
    QCOMPARE(svc.healthMonitor()->snapshot(id).lastCommTime, afterBaseline);
    svc.disable();
}

void TestAccessControlService::turnstileLateResponseAfterDisableIsIgnored()
{
    SequencedNam nam;
    nam.enqueue(emptyPoll(3));        // baseline: valid
    nam.enqueueStall();               // first steady poll: stays in flight
    AccessProviderFactory f = turnstileFactory(&nam);
    EventBus bus;
    AccessControlService svc(&bus, &f);
    const QString id = QStringLiteral("turnstile");
    svc.enable(TurnstileProvider::defaultDescriptor(), fastCfg());
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);     // stalled poll in flight
    const QDateTime before = svc.healthMonitor()->snapshot(id).lastCommTime;
    QVERIFY(before.isValid());

    svc.disable();                    // stop(): generation bump + abort -> late completion
    QTest::qWait(100);
    QCOMPARE(svc.healthMonitor()->snapshot(id).lastCommTime, before);
}
```

Also add `#include <QTimeZone>` to the includes of `tst_accesscontrolservice.cpp`.

In `qt-app/tests/tst_mockprovider.cpp`, add `void simulatePolledEmitsPolled();` to `private slots:` and implement above `QTEST_MAIN`:

```cpp
void TestMockProvider::simulatePolledEmitsPolled()
{
    MockProvider p(MockProvider::defaultDescriptor());
    QSignalSpy polled(&p, &IAccessProvider::polled);
    const QDateTime at = QDateTime::currentDateTimeUtc();
    p.simulatePolled(at);
    QCOMPARE(polled.count(), 1);
    QCOMPARE(polled.at(0).at(0).toDateTime(), at);
}
```

- [ ] **Step 4: Let `tst_accesscontrolservice` compile the real provider**

In `qt-app/tests/CMakeLists.txt`, replace the whole `tst_accesscontrolservice` block with:

```cmake
# --- Access Control: lifecycle service + state machine + per-poll freshness
# (Network: drives a real TurnstileProvider over SequencedNam; no offscreen) ---
wits_add_qttest(tst_accesscontrolservice
    SOURCES
        tst_accesscontrolservice.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesscontrolservice.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesscontrolservice.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/healthmonitor.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/healthmonitor.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accessproviderfactory.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accessproviderfactory.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/eventbus.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/eventbus.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/mockprovider.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/mockprovider.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/turnstileprovider.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/turnstileprovider.h
        ${CMAKE_SOURCE_DIR}/core/loginparser.cpp
        ${CMAKE_SOURCE_DIR}/core/loginparser.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/iaccessprovider.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
    LIBS Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
```

- [ ] **Step 5: Build + run to verify RED**

```powershell
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4 --target tst_turnstileprovider tst_accesscontrolservice tst_mockprovider
ctest --test-dir C:/b/loams-sp4 -R "tst_turnstileprovider|tst_accesscontrolservice|tst_mockprovider" --output-on-failure
```

Expected: configure + build succeed. `tst_mockprovider` PASSES (the driver is real). **FAIL** at assertions: `tst_turnstileprovider::baselineSuccessEmitsPolledOnce` / `polledOnValidEmptyPolls` / `polledOnEntryPolls` / `noPolledOnMalformed` / `noPolledOnNonAdvancingEntry` (provider never emits — counts stay 0, expected 1 / ≥2 / ≥3 / 1 / 1); `tst_accesscontrolservice::polledRecordsCommTimeForActiveProvider` (lastCommTime stays invalid), `connectedTransitionAndPolledAreBothServiceRecorded` (the Connected record passes, but `simulatePolled(later)` doesn't advance it) and `turnstileValidEmptyPollAdvancesFreshness` (QTRY timeout — freshness frozen at the Connected transition). The remaining `noPolled*` / invalid / late cases pass vacuously now and act as regression guards once emission exists.

- [ ] **Step 6: Implement — provider emission**

In `qt-app/core/accesscontrol/turnstileprovider.cpp`, add `#include <QDateTime>` to the includes and replace `TurnstileProvider::onFinished` with:

```cpp
void TurnstileProvider::onFinished(QNetworkReply *reply, quint64 gen)
{
    if (gen != m_generation) return;   // stale (stopped/restarted since) — drop, never emit
    m_reply.clear();

    if (reply->error() != QNetworkReply::NoError) { fail(); return; }   // transport / non-2xx

    const LoginParser::EntryEventResult r =
        LoginParser::parseEntryEvent(reply->readAll(), m_baseUrl);
    if (!r.valid) { fail(); return; }  // malformed / failed schema validation

    // A non-advancing entry is a protocol anomaly (failed validation), checked
    // BEFORE the freshness report so an anomalous poll never bumps comm age and
    // a bad reconnect response goes Connecting -> Degraded with no transient
    // Connected.
    if (m_baselined && r.hasEntry && r.eventId <= m_since) { fail(); return; }

    // Validated successful poll (incl. a valid empty poll AND the baseline):
    // report the raw freshness fact. The SERVICE records it into HealthMonitor.
    // For the baseline this precedes the Connected transition, which the
    // service also records — same instant, harmless double record (pinned).
    emit polled(QDateTime::currentDateTimeUtc());
    if (gen != m_generation) return;   // a polled subscriber stopped/restarted us

    if (!m_baselined) {                    // first start: baseline, skip history
        m_since = r.latestId;
        m_baselined = true;
        setState(ConnectionState::Connected);
        armTimer();                        // transition INTO steady polling
        return;
    }

    if (r.hasEntry) {
        setState(ConnectionState::Connected);
        if (gen != m_generation) return;   // a state subscriber stopped/restarted us
        AccessEvent e;
        e.type = AccessEvent::Type::EntryObserved;
        e.subject = r.student;             // empty => unresolved (see convention)
        e.gateId = m_gateId;
        e.credentialKind = CredentialKind::Rfid;
        e.correlationId = QString::number(r.eventId);
        e.at = r.at;
        m_since = r.eventId;               // advance cursor BEFORE the synchronous emit
        emit accessEvent(e);
        if (gen != m_generation) return;   // subscriber called stop()/start(): halt the drain
        sendPoll();                        // drain: immediately request the next
    } else {
        setState(ConnectionState::Connected);   // (re)confirm after a reconnect start
        if (gen != m_generation) return;
        armTimer();                        // empty poll: wait one interval
    }
}
```

- [ ] **Step 7: Implement — service recording**

In `qt-app/core/accesscontrol/accesscontrolservice.cpp`, inside `enable()`, directly after the `connect(m_provider, &IAccessProvider::hardwareError, ...)` line:

```cpp
    // Per-poll freshness (Sub-plan 4): the provider reports each validated
    // successful poll; the SERVICE owns recording it (the provider never touches
    // HealthMonitor). Pinned to this provider instance and dropped once
    // disabled, so a late callback from a torn-down provider records nothing.
    connect(m_provider, &IAccessProvider::polled, this,
            [this, p, providerId](const QDateTime &at) {
                if (!m_enabled || p != m_provider)
                    return;
                m_health->recordCommTime(providerId, at);
            });
```

In the same file, in `onProviderState`, replace the comment above the existing `Connected` record (keep the call itself unchanged) with:

```cpp
    case ConnectionState::Connected:
        m_reconnectNextMs = m_reconnectBaseMs;   // reset backoff on success
        m_reconnectTimer->stop();                // cancel any pending reconnect
        // A successful connect IS a real comm moment — record the time. This is
        // one of TWO service-owned freshness sources (the other is the per-poll
        // polled(at) connection in enable()); it is kept deliberately: it is the
        // only source for providers that never emit polled (MockProvider), and
        // for TurnstileProvider it coincides with the baseline's own polled
        // record (same instant, harmless overwrite). Latency stays -1 (unknown)
        // until the verify/decision path measures a real one; never fabricate.
        m_health->recordCommTime(m_provider->descriptor().providerId,
                                 QDateTime::currentDateTimeUtc());
        publishControllerEvent(AccessEvent::Type::ControllerConnected);
        break;
```

In `qt-app/core/accesscontrol/accesscontrolservice.h`, extend the class comment's last sentence to: `... with exponential backoff reconnect. Owns its HealthMonitor and records each provider-reported polled(at) into it (per-poll comm freshness).`

- [ ] **Step 8: Build + run to verify GREEN**

```powershell
cmake --build C:/b/loams-sp4 --target tst_turnstileprovider tst_accesscontrolservice tst_mockprovider
ctest --test-dir C:/b/loams-sp4 -R "tst_turnstileprovider|tst_accesscontrolservice|tst_mockprovider" --output-on-failure
```

Expected: PASS — `tst_turnstileprovider` 23/23 (15 existing + 8), `tst_accesscontrolservice` 14 functions (7 existing + 7; the `_data` test runs 4 rows), `tst_mockprovider` 7/7.

- [ ] **Step 9: Commit** (via the `commit` skill) — `feat(accesscontrol): service-owned per-poll comm freshness via IAccessProvider::polled`.

---

## Task 2: `LoginParser::parseRecentFeed` (pure decoder for `access_recent.php`)

**Files:**
- Modify: `qt-app/core/loginparser.h`, `qt-app/core/loginparser.cpp`
- Test: `qt-app/tests/tst_loginparser.cpp`

**Interfaces:**
- Consumes: nothing new (pure). Response shape (from `deliverables/loams_api/access_recent.php`): `{"status":"success","entries":[{"id":int,"card":string,"created_at":"yyyy-MM-dd HH:mm:ss","reader":int,"student":null|{"name","school_id","course","department","photo_path"}}],"entries_today":int,"last_entry_at":string|null}`; errors are `{"status":"error","message":...}`.
- Produces:
  - `struct LoginParser::RecentEntry { qint64 id; QString card; QString createdAt; int reader; bool known; QString name; QString schoolId; QString course; QString department; };`
  - `struct LoginParser::RecentFeedResult { bool valid; QVector<RecentEntry> entries; int entriesToday; QString lastEntryAt; QString error; };`
  - `RecentFeedResult LoginParser::parseRecentFeed(const QByteArray &body);` — `valid` for well-formed `status:"success"` **including `entries: []`**; `lastEntryAt` is `""` when the server sends `null`; on `status != success` the server `message` (if any) is carried in `error` so the VM can classify auth failures. **Strict row shape:** `id` must be a positive JSON number, `created_at` a JSON string, `card` a JSON string, `reader` a JSON number, `student` an object or `null` — a missing or wrong-typed field makes the whole result `valid=false` with `error` set (never silently defaulted to `""`/`0`).

- [ ] **Step 1: Declare the types + a stub (stub-first)**

In `qt-app/core/loginparser.h`, add `#include <QVector>` to the includes, then inside `namespace LoginParser` after `EntryEventResult`:

```cpp
// One row of access_recent.php (admin Access Control feed). Text only — the
// endpoint's photo_path is deliberately NOT carried (no thumbnails this slice).
struct RecentEntry {
    qint64  id       = 0;
    QString card;              // raw card (admin-authenticated feed only)
    QString createdAt;         // server-local "yyyy-MM-dd HH:mm:ss", verbatim
    int     reader   = 0;      // gate/lane number
    bool    known    = false;  // false when the endpoint's student is null
    QString name;
    QString schoolId;
    QString course;
    QString department;
};

struct RecentFeedResult {
    bool                 valid        = false;  // false = malformed / wrong shape / non-success
    QVector<RecentEntry> entries;               // newest first, as served
    int                  entriesToday = 0;
    QString              lastEntryAt;           // "" when the server sent null (no entries ever)
    QString              error;                 // reason when !valid (server message if any)
};

// Pure decode of access_recent.php. valid is true for a well-formed
// status:"success" response INCLUDING an empty entries array.
RecentFeedResult parseRecentFeed(const QByteArray &body);
```

In `qt-app/core/loginparser.cpp`, append (stub — deliberately behaviour-free so the tests go red):

```cpp
LoginParser::RecentFeedResult LoginParser::parseRecentFeed(const QByteArray &)
{
    return RecentFeedResult{};   // stub — replaced in Step 4
}
```

- [ ] **Step 2: Write the failing tests**

In `qt-app/tests/tst_loginparser.cpp`, add to `private slots:` (after `void parseEntryEvent_malformedIsInvalid();`):

```cpp
    void parseRecentFeed_validListWithCounts();
    void parseRecentFeed_nullStudentIsUnknownRow();
    void parseRecentFeed_emptyButValid();
    void parseRecentFeed_serverErrorCarriesMessage();
    void parseRecentFeed_malformedIsInvalid();
    void parseRecentFeed_cardAndReaderAreStrict();
```

Append above `QTEST_MAIN(TestLoginParser)`:

```cpp
static QByteArray recentFeedBody()
{
    return QByteArray(R"({"status":"success","entries":[
        {"id":12,"card":"CARD0012","created_at":"2026-09-30 08:15:00","reader":1,
         "student":{"name":"Test Student A","school_id":"TEST-0001","course":"BS Test",
                    "department":"Dept Test","photo_path":"uploads/default.jpg"}},
        {"id":11,"card":"CARD0011","created_at":"2026-09-30 08:10:00","reader":2,"student":null}],
        "entries_today":7,"last_entry_at":"2026-09-30 08:15:00"})");
}

void TestLoginParser::parseRecentFeed_validListWithCounts()
{
    const auto r = LoginParser::parseRecentFeed(recentFeedBody());
    QVERIFY(r.valid);
    QCOMPARE(r.entries.size(), 2);
    QCOMPARE(r.entriesToday, 7);
    QCOMPARE(r.lastEntryAt, QStringLiteral("2026-09-30 08:15:00"));
    const LoginParser::RecentEntry &e = r.entries.at(0);   // newest first, as served
    QCOMPARE(e.id, Q_INT64_C(12));
    QCOMPARE(e.card, QStringLiteral("CARD0012"));
    QCOMPARE(e.createdAt, QStringLiteral("2026-09-30 08:15:00"));
    QCOMPARE(e.reader, 1);
    QVERIFY(e.known);
    QCOMPARE(e.name, QStringLiteral("Test Student A"));
    QCOMPARE(e.schoolId, QStringLiteral("TEST-0001"));
    QCOMPARE(e.course, QStringLiteral("BS Test"));
    QCOMPARE(e.department, QStringLiteral("Dept Test"));
}

void TestLoginParser::parseRecentFeed_nullStudentIsUnknownRow()
{
    const auto r = LoginParser::parseRecentFeed(recentFeedBody());
    QVERIFY(r.valid);
    const LoginParser::RecentEntry &e = r.entries.at(1);
    QCOMPARE(e.id, Q_INT64_C(11));
    QVERIFY(!e.known);
    QVERIFY(e.name.isEmpty());
    QVERIFY(e.schoolId.isEmpty());
    QCOMPARE(e.card, QStringLiteral("CARD0011"));   // still carried for the admin
    QCOMPARE(e.reader, 2);
}

void TestLoginParser::parseRecentFeed_emptyButValid()
{
    const auto r = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[],"entries_today":0,"last_entry_at":null})");
    QVERIFY(r.valid);                    // an empty feed is NOT an error
    QVERIFY(r.entries.isEmpty());
    QCOMPARE(r.entriesToday, 0);
    QVERIFY(r.lastEntryAt.isEmpty());    // null -> ""
}

void TestLoginParser::parseRecentFeed_serverErrorCarriesMessage()
{
    const auto r = LoginParser::parseRecentFeed(
        R"({"status":"error","message":"Invalid admin key"})");
    QVERIFY(!r.valid);
    QCOMPARE(r.error, QStringLiteral("Invalid admin key"));
}

void TestLoginParser::parseRecentFeed_malformedIsInvalid()
{
    QVERIFY(!LoginParser::parseRecentFeed("not json").valid);
    QVERIFY(!LoginParser::parseRecentFeed(R"({"status":"error"})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entries not an array
        R"({"status":"success","entries":{},"entries_today":0,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entries_today missing
        R"({"status":"success","entries":[],"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entries_today negative
        R"({"status":"success","entries":[],"entries_today":-1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // last_entry_at wrong type
        R"({"status":"success","entries":[],"entries_today":0,"last_entry_at":5})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entry not an object
        R"({"status":"success","entries":[3],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // id not positive
        R"({"status":"success","entries":[{"id":0,"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // created_at not a string
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":5,"reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // student wrong type
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":5}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // id missing
        R"({"status":"success","entries":[{"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // id non-number
        R"({"status":"success","entries":[{"id":"1","card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // created_at missing
        R"({"status":"success","entries":[{"id":1,"card":"C","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
}

void TestLoginParser::parseRecentFeed_cardAndReaderAreStrict()
{
    // card must be a JSON string and reader a JSON number — never silently
    // defaulted to ""/0 (a shape drift must fail loudly, not render blanks).
    const auto cardMissing = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!cardMissing.valid);
    QVERIFY(!cardMissing.error.isEmpty());

    const auto cardNonString = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":1234,"created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!cardNonString.valid);
    QVERIFY(!cardNonString.error.isEmpty());

    const auto readerMissing = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!readerMissing.valid);
    QVERIFY(!readerMissing.error.isEmpty());

    const auto readerNonNumber = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","reader":"1","student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!readerNonNumber.valid);
    QVERIFY(!readerNonNumber.error.isEmpty());

    // Control: the same row with a string card and numeric reader is valid.
    QVERIFY(LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
}
```

- [ ] **Step 3: Run the tests to verify RED**

```powershell
cmake --build C:/b/loams-sp4 --target tst_loginparser
ctest --test-dir C:/b/loams-sp4 -R tst_loginparser --output-on-failure
```

Expected: builds; **FAIL** at assertions — `parseRecentFeed_validListWithCounts`, `_nullStudentIsUnknownRow`, `_emptyButValid` (`r.valid` false from the stub), `_serverErrorCarriesMessage` (`error` empty), `_cardAndReaderAreStrict` (the stub returns an empty `error` and fails the valid control row). `_malformedIsInvalid` passes vacuously (guard).

- [ ] **Step 4: Implement `parseRecentFeed`**

In `qt-app/core/loginparser.cpp`, add `#include <QJsonArray>` to the includes and replace the stub with:

```cpp
LoginParser::RecentFeedResult LoginParser::parseRecentFeed(const QByteArray &body)
{
    RecentFeedResult r;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) { r.error = QStringLiteral("Not a JSON object"); return r; }
    const QJsonObject obj = doc.object();

    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        // Carry the server's message (e.g. requireAdminAuth's "Invalid admin
        // key") so the caller can tell an auth rejection from a generic error.
        const QString msg = obj.value(QStringLiteral("message")).toString();
        r.error = msg.isEmpty() ? QStringLiteral("status != success") : msg;
        return r;
    }

    const QJsonValue entriesVal = obj.value(QStringLiteral("entries"));
    if (!entriesVal.isArray()) { r.error = QStringLiteral("entries not an array"); return r; }

    const QJsonValue todayVal = obj.value(QStringLiteral("entries_today"));
    if (!todayVal.isDouble() || todayVal.toInteger(-1) < 0) {
        r.error = QStringLiteral("entries_today not a non-negative number"); return r;
    }

    const QJsonValue lastVal = obj.value(QStringLiteral("last_entry_at"));
    if (!lastVal.isString() && !lastVal.isNull()) {
        r.error = QStringLiteral("last_entry_at not string/null"); return r;
    }

    const QJsonArray arr = entriesVal.toArray();
    QVector<RecentEntry> entries;
    entries.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) { r.error = QStringLiteral("entry not an object"); return r; }
        const QJsonObject e = v.toObject();

        const QJsonValue idVal = e.value(QStringLiteral("id"));
        if (!idVal.isDouble() || idVal.toInteger(-1) <= 0) {
            r.error = QStringLiteral("entry.id not positive"); return r;
        }
        const QJsonValue createdVal = e.value(QStringLiteral("created_at"));
        if (!createdVal.isString()) { r.error = QStringLiteral("created_at not a string"); return r; }
        // Strict: missing (Undefined) or wrong-typed card/reader is a shape
        // failure, never silently defaulted to ""/0.
        const QJsonValue cardVal = e.value(QStringLiteral("card"));
        if (!cardVal.isString()) { r.error = QStringLiteral("card not a string"); return r; }
        const QJsonValue readerVal = e.value(QStringLiteral("reader"));
        if (!readerVal.isDouble()) { r.error = QStringLiteral("reader not a number"); return r; }

        RecentEntry out;
        out.id = idVal.toInteger();
        out.card = cardVal.toString();
        out.createdAt = createdVal.toString();
        out.reader = readerVal.toInt();

        const QJsonValue studentVal = e.value(QStringLiteral("student"));
        if (studentVal.isObject()) {
            const QJsonObject s = studentVal.toObject();
            out.known = true;
            out.name = s.value(QStringLiteral("name")).toString();
            out.schoolId = s.value(QStringLiteral("school_id")).toString();
            out.course = s.value(QStringLiteral("course")).toString();
            out.department = s.value(QStringLiteral("department")).toString();
        } else if (!studentVal.isNull()) {
            r.error = QStringLiteral("student not object/null"); return r;
        }
        entries.append(out);
    }

    r.valid = true;
    r.entries = entries;
    r.entriesToday = static_cast<int>(todayVal.toInteger());
    r.lastEntryAt = lastVal.isString() ? lastVal.toString() : QString();
    return r;
}
```

- [ ] **Step 5: Run the tests to verify GREEN**

```powershell
cmake --build C:/b/loams-sp4 --target tst_loginparser
ctest --test-dir C:/b/loams-sp4 -R tst_loginparser --output-on-failure
```

Expected: PASS (all six `parseRecentFeed_*` cases plus the pre-existing cases).

- [ ] **Step 6: Commit** (via the `commit` skill) — `feat(accesscontrol): add pure parseRecentFeed decoder for access_recent`.

---

## Task 3: `AccessEntriesModel` (text-only recent-entries list model)

**Files:**
- Create: `qt-app/quick/models/AccessEntriesModel.h`, `qt-app/quick/models/AccessEntriesModel.cpp`
- Create: `qt-app/quick/tests/tst_accessentriesmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Consumes: `LoginParser::RecentEntry` (Task 2), `"loginparser.h"` via `witscore`'s PUBLIC include dir.
- Produces: `class AccessEntriesModel : public QAbstractListModel` with `enum Roles { NameRole = Qt::UserRole + 1, SchoolIdRole, CourseRole, DepartmentRole, CreatedAtRole, ReaderRole, CardRole, KnownRole };`, role names `name`, `schoolId`, `course`, `department`, `createdAt`, `reader`, `card`, `known`; `void setEntries(const QVector<LoginParser::RecentEntry> &entries);` `void clear();`. `name` is `"Unknown card"` and `known` is `false` when the endpoint's student was null; `reader` is served as text (`QString::number`).

- [ ] **Step 1: Create the header + a skeleton `.cpp` (stub-first)**

`qt-app/quick/models/AccessEntriesModel.h`:

```cpp
#ifndef ACCESSENTRIESMODEL_H
#define ACCESSENTRIESMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include "loginparser.h"

// Access Control recent-entries rows (Sub-plan 4). TEXT ONLY — no photo
// thumbnails this slice, so the Sub-plan 3 photo-origin problem is not
// re-introduced into the admin table. A row whose endpoint student was null
// renders name "Unknown card" with known == false.
class AccessEntriesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        SchoolIdRole,
        CourseRole,
        DepartmentRole,
        CreatedAtRole,
        ReaderRole,
        CardRole,
        KnownRole,
    };

    explicit AccessEntriesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QVector<LoginParser::RecentEntry> &entries);
    void clear();

private:
    QVector<LoginParser::RecentEntry> m_entries;
};

#endif // ACCESSENTRIESMODEL_H
```

`qt-app/quick/models/AccessEntriesModel.cpp` — **skeleton**:

```cpp
#include "AccessEntriesModel.h"

AccessEntriesModel::AccessEntriesModel(QObject *parent) : QAbstractListModel(parent) {}
int AccessEntriesModel::rowCount(const QModelIndex &) const { return 0; }          // stub
QVariant AccessEntriesModel::data(const QModelIndex &, int) const { return {}; }   // stub
QHash<int, QByteArray> AccessEntriesModel::roleNames() const { return {}; }        // stub
void AccessEntriesModel::setEntries(const QVector<LoginParser::RecentEntry> &) {}  // stub
void AccessEntriesModel::clear() {}                                                // stub
```

- [ ] **Step 2: Write the model tests**

`qt-app/quick/tests/tst_accessentriesmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "AccessEntriesModel.h"
#include "loginparser.h"

class TestAccessEntriesModel : public QObject
{
    Q_OBJECT
private slots:
    void roleNamesShape();
    void knownRowRoles();
    void unknownCardRow();
    void setEntriesResetsAndClearEmpties();
    void outOfRangeIndexIsInvalid();

private:
    static QVector<LoginParser::RecentEntry> twoRows()
    {
        LoginParser::RecentEntry known;
        known.id = 12; known.card = QStringLiteral("CARD0012");
        known.createdAt = QStringLiteral("2026-09-30 08:15:00"); known.reader = 1;
        known.known = true; known.name = QStringLiteral("Test Student A");
        known.schoolId = QStringLiteral("TEST-0001"); known.course = QStringLiteral("BS Test");
        known.department = QStringLiteral("Dept Test");
        LoginParser::RecentEntry unknown;
        unknown.id = 11; unknown.card = QStringLiteral("CARD0011");
        unknown.createdAt = QStringLiteral("2026-09-30 08:10:00"); unknown.reader = 2;
        unknown.known = false;
        return {known, unknown};
    }
};

void TestAccessEntriesModel::roleNamesShape()
{
    AccessEntriesModel m;
    const QHash<int, QByteArray> names = m.roleNames();
    QCOMPARE(names.size(), 8);
    QCOMPARE(names.value(AccessEntriesModel::NameRole), QByteArray("name"));
    QCOMPARE(names.value(AccessEntriesModel::SchoolIdRole), QByteArray("schoolId"));
    QCOMPARE(names.value(AccessEntriesModel::CourseRole), QByteArray("course"));
    QCOMPARE(names.value(AccessEntriesModel::DepartmentRole), QByteArray("department"));
    QCOMPARE(names.value(AccessEntriesModel::CreatedAtRole), QByteArray("createdAt"));
    QCOMPARE(names.value(AccessEntriesModel::ReaderRole), QByteArray("reader"));
    QCOMPARE(names.value(AccessEntriesModel::CardRole), QByteArray("card"));
    QCOMPARE(names.value(AccessEntriesModel::KnownRole), QByteArray("known"));
}

void TestAccessEntriesModel::knownRowRoles()
{
    AccessEntriesModel m;
    m.setEntries(twoRows());
    QCOMPARE(m.rowCount(), 2);
    const QModelIndex i = m.index(0);
    QCOMPARE(m.data(i, AccessEntriesModel::NameRole).toString(), QStringLiteral("Test Student A"));
    QCOMPARE(m.data(i, AccessEntriesModel::SchoolIdRole).toString(), QStringLiteral("TEST-0001"));
    QCOMPARE(m.data(i, AccessEntriesModel::CourseRole).toString(), QStringLiteral("BS Test"));
    QCOMPARE(m.data(i, AccessEntriesModel::DepartmentRole).toString(), QStringLiteral("Dept Test"));
    QCOMPARE(m.data(i, AccessEntriesModel::CreatedAtRole).toString(),
             QStringLiteral("2026-09-30 08:15:00"));
    QCOMPARE(m.data(i, AccessEntriesModel::ReaderRole).toString(), QStringLiteral("1"));
    QCOMPARE(m.data(i, AccessEntriesModel::CardRole).toString(), QStringLiteral("CARD0012"));
    QCOMPARE(m.data(i, AccessEntriesModel::KnownRole).toBool(), true);
}

void TestAccessEntriesModel::unknownCardRow()
{
    AccessEntriesModel m;
    m.setEntries(twoRows());
    const QModelIndex i = m.index(1);
    QCOMPARE(m.data(i, AccessEntriesModel::NameRole).toString(), QStringLiteral("Unknown card"));
    QCOMPARE(m.data(i, AccessEntriesModel::KnownRole).toBool(), false);
    QVERIFY(m.data(i, AccessEntriesModel::SchoolIdRole).toString().isEmpty());
    QCOMPARE(m.data(i, AccessEntriesModel::CardRole).toString(), QStringLiteral("CARD0011"));
    QCOMPARE(m.data(i, AccessEntriesModel::ReaderRole).toString(), QStringLiteral("2"));
}

void TestAccessEntriesModel::setEntriesResetsAndClearEmpties()
{
    AccessEntriesModel m;
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);
    m.setEntries(twoRows());
    QCOMPARE(reset.count(), 1);
    QCOMPARE(m.rowCount(), 2);
    m.clear();
    QCOMPARE(reset.count(), 2);
    QCOMPARE(m.rowCount(), 0);
}

void TestAccessEntriesModel::outOfRangeIndexIsInvalid()
{
    AccessEntriesModel m;
    m.setEntries(twoRows());
    QVERIFY(!m.data(m.index(5), AccessEntriesModel::NameRole).isValid());
    QVERIFY(!m.data(QModelIndex(), AccessEntriesModel::NameRole).isValid());
    QCOMPARE(m.rowCount(m.index(0)), 0);   // list model: children have no rows
}

QTEST_APPLESS_MAIN(TestAccessEntriesModel)
#include "tst_accessentriesmodel.moc"
```

- [ ] **Step 3: Register the model + test in CMake**

In `qt-app/quick/CMakeLists.txt`, add to the `witsquickmodule` `SOURCES` list after `models/RankingModel.h`:

```cmake
        models/AccessEntriesModel.h models/AccessEntriesModel.cpp
```

After the `tst_reportrowsmodel` block, register:

```cmake
# --- AccessEntriesModel unit test (C++ QtTest). Pure QAbstractListModel over
# LoginParser::RecentEntry, no NAM -> QTEST_APPLESS_MAIN, no OFFSCREEN. ---
wits_add_qttest(tst_accessentriesmodel
    SOURCES tests/tst_accessentriesmodel.cpp
    LIBS witsquickmodule)
```

- [ ] **Step 4: Build + run to verify RED**

```powershell
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4 --target tst_accessentriesmodel
ctest --test-dir C:/b/loams-sp4 -R tst_accessentriesmodel --output-on-failure
```

Expected: configure + build succeed (skeleton links); **FAIL** at assertions (`roleNames().size()` 0 ≠ 8; `rowCount()` 0 ≠ 2; no `modelReset`).

- [ ] **Step 5: Implement `AccessEntriesModel.cpp`**

Replace the skeleton with:

```cpp
#include "AccessEntriesModel.h"

AccessEntriesModel::AccessEntriesModel(QObject *parent) : QAbstractListModel(parent) {}

int AccessEntriesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant AccessEntriesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const LoginParser::RecentEntry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole:       return e.known ? e.name : tr("Unknown card");
    case SchoolIdRole:   return e.schoolId;
    case CourseRole:     return e.course;
    case DepartmentRole: return e.department;
    case CreatedAtRole:  return e.createdAt;
    case ReaderRole:     return QString::number(e.reader);
    case CardRole:       return e.card;
    case KnownRole:      return e.known;
    default:             return {};
    }
}

QHash<int, QByteArray> AccessEntriesModel::roleNames() const
{
    return {
        { NameRole, "name" }, { SchoolIdRole, "schoolId" }, { CourseRole, "course" },
        { DepartmentRole, "department" }, { CreatedAtRole, "createdAt" },
        { ReaderRole, "reader" }, { CardRole, "card" }, { KnownRole, "known" },
    };
}

void AccessEntriesModel::setEntries(const QVector<LoginParser::RecentEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void AccessEntriesModel::clear()
{
    setEntries({});
}
```

- [ ] **Step 6: Build + run to verify GREEN**

```powershell
cmake --build C:/b/loams-sp4 --target tst_accessentriesmodel
ctest --test-dir C:/b/loams-sp4 -R tst_accessentriesmodel --output-on-failure
```

Expected: PASS (5/5).

- [ ] **Step 7: Commit** (via the `commit` skill) — `feat(accesscontrol): add text-only AccessEntriesModel for the recent feed`.

---

## Task 4: `AccessControlViewModel` (page-scoped, admin-authenticated snapshot)

**Files:**
- Create: `qt-app/quick/viewmodels/AccessControlViewModel.h`, `qt-app/quick/viewmodels/AccessControlViewModel.cpp`
- Create: `qt-app/quick/tests/tst_accesscontrolviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Consumes: `LoginParser::parseRecentFeed` (Task 2); `AccessEntriesModel` (Task 3); `HttpForm::formRequest(const QUrl &)`, `HttpForm::encodeForm(const QList<QPair<QString,QString>> &)`, `HttpForm::isServerAnswer(bool, int, const QByteArray &)`; `AdminSession::instance().key()`; `ApiConfig::endpoint(QStringLiteral("access_recent.php"))`; `SettingsViewModel::isAuthFailureMessage(const QString &)` (public static); test double `CapturingNam(const QByteArray &canned, QNetworkReply::NetworkError err, int httpStatus)` with `lastOp`, `lastUrl`, `lastContentType`, `lastBody`.
- Produces: `class AccessControlViewModel : public QObject` (`QML_ELEMENT`):
  - ctor `explicit AccessControlViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr);`
  - `Q_PROPERTY(int entriesToday … NOTIFY dataChanged)`, `Q_PROPERTY(QString lastEntryAt … NOTIFY dataChanged)`, `Q_PROPERTY(QString updatedAt … NOTIFY dataChanged)` (`"HH:mm:ss"` of the last success, `""` before/after auth clear), `Q_PROPERTY(bool emptyFeed … NOTIFY dataChanged)`, `Q_PROPERTY(bool initialLoadFailed … NOTIFY dataChanged)`, `Q_PROPERTY(bool loading … NOTIFY loadingChanged)`, `Q_PROPERTY(bool stale … NOTIFY staleChanged)`, `Q_PROPERTY(QString errorText … NOTIFY errorTextChanged)`, `Q_PROPERTY(bool authFailure … NOTIFY authFailureChanged)`, `Q_PROPERTY(AccessEntriesModel *entries READ entries CONSTANT)`.
  - `Q_INVOKABLE void refresh();` · `void applyRecent(const QByteArray &raw);` · `quint64 nextRequestSeq();` · `bool isCurrentRequest(quint64 seq) const;`
  - Fixed texts: network `Network error. Please try again.`; bad payload `Could not refresh the access feed.`; auth `Admin authentication failed — re-enter via admin login.`

- [ ] **Step 1: Create the header + a skeleton `.cpp` (stub-first)**

`qt-app/quick/viewmodels/AccessControlViewModel.h`:

```cpp
#ifndef ACCESSCONTROLVIEWMODEL_H
#define ACCESSCONTROLVIEWMODEL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <qqml.h>
#include "AccessEntriesModel.h"

class QNetworkAccessManager;

// Access Control page VM (Sub-plan 4). Owns the admin-authenticated SNAPSHOT
// of access_recent.php: the recent-entries table, entries_today /
// last_entry_at, and the "Updated HH:MM:SS" time of the last SUCCESSFUL fetch.
// Live state (toggle, connection pill, contact age) lives on the AccessControl
// singleton, never here. Failure contract (spec refinement 6):
//  - ordinary failure after a success: keep rows, stale = true, freeze updatedAt;
//  - auth loss (401 / "Invalid admin key"): authFailure = true + clear the
//    protected data (rows, counts, updatedAt), stale = false;
//  - empty-but-valid feed (emptyFeed) is distinct from a failed initial load
//    (initialLoadFailed).
class AccessControlViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int entriesToday READ entriesToday NOTIFY dataChanged)
    Q_PROPERTY(QString lastEntryAt READ lastEntryAt NOTIFY dataChanged)
    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY dataChanged)
    Q_PROPERTY(bool emptyFeed READ emptyFeed NOTIFY dataChanged)
    Q_PROPERTY(bool initialLoadFailed READ initialLoadFailed NOTIFY dataChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)
    Q_PROPERTY(AccessEntriesModel *entries READ entries CONSTANT)

public:
    // nam: injection seam for tests (CapturingNam). Null in production: the VM
    // owns a fresh QNetworkAccessManager.
    explicit AccessControlViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr);

    int entriesToday() const { return m_entriesToday; }
    QString lastEntryAt() const { return m_lastEntryAt; }
    QString updatedAt() const { return m_updatedAt; }
    bool emptyFeed() const;
    bool initialLoadFailed() const;
    bool loading() const { return m_loading; }
    bool stale() const { return m_stale; }
    QString errorText() const { return m_errorText; }
    bool authFailure() const { return m_authFailure; }
    AccessEntriesModel *entries() { return &m_entries; }

    Q_INVOKABLE void refresh();

    // Network-free seam (tests + the reply handler).
    void applyRecent(const QByteArray &raw);

    // In-flight request generation guard (same idiom as VisitLogsViewModel):
    // a reply superseded by a newer refresh() is dropped.
    quint64 nextRequestSeq();
    bool isCurrentRequest(quint64 seq) const;

signals:
    void dataChanged();
    void loadingChanged();
    void staleChanged();
    void errorTextChanged();
    void authFailureChanged();

private:
    void applyFailure(const QString &message);
    void applyAuthFailure();
    void setLoading(bool v);
    void setStale(bool v);
    void setError(const QString &e);
    void setAuthFailure(bool v);

    QNetworkAccessManager *m_nam = nullptr;
    AccessEntriesModel m_entries;
    int m_entriesToday = 0;
    QString m_lastEntryAt;
    QString m_updatedAt;
    bool m_hasLoaded = false;   // a successful load is currently on screen
    bool m_loading = false;
    bool m_stale = false;
    QString m_errorText;
    bool m_authFailure = false;
    quint64 m_requestSeq = 0;
};

#endif // ACCESSCONTROLVIEWMODEL_H
```

`qt-app/quick/viewmodels/AccessControlViewModel.cpp` — **skeleton**:

```cpp
#include "AccessControlViewModel.h"

#include <QNetworkAccessManager>

AccessControlViewModel::AccessControlViewModel(QObject *parent, QNetworkAccessManager *nam)
    : QObject(parent)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
}
bool AccessControlViewModel::emptyFeed() const { return false; }            // stub
bool AccessControlViewModel::initialLoadFailed() const { return false; }    // stub
void AccessControlViewModel::refresh() {}                                   // stub
void AccessControlViewModel::applyRecent(const QByteArray &) {}             // stub
quint64 AccessControlViewModel::nextRequestSeq() { return 0; }              // stub
bool AccessControlViewModel::isCurrentRequest(quint64) const { return false; }   // stub
void AccessControlViewModel::applyFailure(const QString &) {}
void AccessControlViewModel::applyAuthFailure() {}
void AccessControlViewModel::setLoading(bool) {}
void AccessControlViewModel::setStale(bool) {}
void AccessControlViewModel::setError(const QString &) {}
void AccessControlViewModel::setAuthFailure(bool) {}
```

- [ ] **Step 2: Write the VM tests**

`qt-app/quick/tests/tst_accesscontrolviewmodel.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QRegularExpression>
#include <QUrlQuery>
#include "AccessControlViewModel.h"
#include "AccessEntriesModel.h"
#include "AdminSession.h"
#include "capturingnam.h"

class TestAccessControlViewModel : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { AdminSession::instance().clear(); }

    void refresh_postsAdminKeyInBodyNotUrl();
    void refresh_togglesLoading();
    void applyRecent_fillsModelCountsAndUpdatedAt();
    void emptyFeed_isEmptyStateNotError();
    void malformedFirstLoad_isInitialFailureNotEmpty();
    void ordinaryFailure_keepsRowsMarksStaleFreezesUpdatedAt();
    void refresh_transportFailureAfterSuccessIsStale();
    void refresh_http5xxAfterSuccessIsStale();
    void refresh_http401_setsAuthFailureAndClearsProtectedData();
    void refresh_http401WithEmptyOrMalformedBodyIsStillAuthFailure();
    void authFailure_resetByLaterSuccess();
    void supersededRequestSeqIsNotCurrent();
    void refresh_supersededReplyIsDropped();

private:
    static QByteArray feedBody()
    {
        return QByteArray(R"({"status":"success","entries":[
            {"id":12,"card":"CARD0012","created_at":"2026-09-30 08:15:00","reader":1,
             "student":{"name":"Test Student A","school_id":"TEST-0001","course":"BS Test",
                        "department":"Dept Test","photo_path":"uploads/default.jpg"}},
            {"id":11,"card":"CARD0011","created_at":"2026-09-30 08:10:00","reader":2,"student":null}],
            "entries_today":7,"last_entry_at":"2026-09-30 08:15:00"})");
    }
    static bool isClockText(const QString &s)
    {
        return QRegularExpression(QStringLiteral("^\\d{2}:\\d{2}:\\d{2}$")).match(s).hasMatch();
    }
};

void TestAccessControlViewModel::refresh_postsAdminKeyInBodyNotUrl()
{
    AdminSession::instance().setKey(QStringLiteral("sp4-test-key"));
    CapturingNam nam(feedBody());
    AccessControlViewModel vm(nullptr, &nam);
    vm.refresh();

    QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
    QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
    QVERIFY(nam.lastUrl.path().endsWith(QStringLiteral("access_recent.php")));
    const QUrlQuery form(QString::fromUtf8(nam.lastBody));
    QCOMPARE(form.queryItemValue(QStringLiteral("admin_key")), QStringLiteral("sp4-test-key"));
    QVERIFY(!QUrlQuery(nam.lastUrl).hasQueryItem(QStringLiteral("admin_key")));   // never in the URL
    QVERIFY(nam.lastUrl.query().isEmpty());
}

void TestAccessControlViewModel::refresh_togglesLoading()
{
    CapturingNam nam(feedBody());
    AccessControlViewModel vm(nullptr, &nam);
    vm.refresh();
    QVERIFY(vm.loading());
    QTRY_VERIFY_WITH_TIMEOUT(!vm.loading(), 1000);
    QCOMPARE(vm.entries()->rowCount(), 2);
}

void TestAccessControlViewModel::applyRecent_fillsModelCountsAndUpdatedAt()
{
    AccessControlViewModel vm;
    QSignalSpy data(&vm, &AccessControlViewModel::dataChanged);
    vm.applyRecent(feedBody());
    QCOMPARE(data.count(), 1);
    QCOMPARE(vm.entries()->rowCount(), 2);
    QCOMPARE(vm.entries()->data(vm.entries()->index(1), AccessEntriesModel::NameRole).toString(),
             QStringLiteral("Unknown card"));
    QCOMPARE(vm.entriesToday(), 7);
    QCOMPARE(vm.lastEntryAt(), QStringLiteral("2026-09-30 08:15:00"));
    QVERIFY(isClockText(vm.updatedAt()));
    QVERIFY(!vm.stale());
    QVERIFY(!vm.authFailure());
    QVERIFY(vm.errorText().isEmpty());
    QVERIFY(!vm.emptyFeed());
    QVERIFY(!vm.initialLoadFailed());
}

void TestAccessControlViewModel::emptyFeed_isEmptyStateNotError()
{
    AccessControlViewModel vm;
    vm.applyRecent(R"({"status":"success","entries":[],"entries_today":0,"last_entry_at":null})");
    QVERIFY(vm.emptyFeed());
    QVERIFY(!vm.initialLoadFailed());
    QVERIFY(vm.errorText().isEmpty());
    QVERIFY(isClockText(vm.updatedAt()));   // a successful (empty) load still stamps Updated
    QCOMPARE(vm.entriesToday(), 0);
    QVERIFY(vm.lastEntryAt().isEmpty());
}

void TestAccessControlViewModel::malformedFirstLoad_isInitialFailureNotEmpty()
{
    AccessControlViewModel vm;
    vm.applyRecent("not json");
    QVERIFY(vm.initialLoadFailed());
    QVERIFY(!vm.emptyFeed());
    QVERIFY(!vm.stale());                   // nothing to be stale ABOUT yet
    QCOMPARE(vm.errorText(), QStringLiteral("Could not refresh the access feed."));
    QVERIFY(vm.updatedAt().isEmpty());
}

void TestAccessControlViewModel::ordinaryFailure_keepsRowsMarksStaleFreezesUpdatedAt()
{
    AccessControlViewModel vm;
    vm.applyRecent(feedBody());
    const QString firstUpdated = vm.updatedAt();
    QTest::qWait(1100);                     // a later failure must NOT restamp the clock
    vm.applyRecent("not json");
    QCOMPARE(vm.entries()->rowCount(), 2);  // last-known rows kept
    QCOMPARE(vm.entriesToday(), 7);
    QVERIFY(vm.stale());
    QCOMPARE(vm.updatedAt(), firstUpdated); // frozen at the last success
    QVERIFY(!vm.errorText().isEmpty());
    QVERIFY(!vm.authFailure());
    QVERIFY(!vm.initialLoadFailed());
}

void TestAccessControlViewModel::refresh_transportFailureAfterSuccessIsStale()
{
    CapturingNam nam(QByteArray(), QNetworkReply::HostNotFoundError, 0);   // no status line
    AccessControlViewModel vm(nullptr, &nam);
    vm.applyRecent(feedBody());
    const QString firstUpdated = vm.updatedAt();
    QSignalSpy err(&vm, &AccessControlViewModel::errorTextChanged);
    vm.refresh();
    QVERIFY(err.wait(1000));
    QCOMPARE(vm.errorText(), QStringLiteral("Network error. Please try again."));
    QVERIFY(vm.stale());
    QCOMPARE(vm.entries()->rowCount(), 2);
    QCOMPARE(vm.updatedAt(), firstUpdated);
    QVERIFY(!vm.authFailure());
}

void TestAccessControlViewModel::refresh_http5xxAfterSuccessIsStale()
{
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"error\",\"message\":\"Internal server error\"}"),
                     QNetworkReply::InternalServerError, 500);
    AccessControlViewModel vm(nullptr, &nam);
    vm.applyRecent(feedBody());
    QSignalSpy stale(&vm, &AccessControlViewModel::staleChanged);
    vm.refresh();
    QVERIFY(stale.wait(1000));
    QVERIFY(vm.stale());
    QCOMPARE(vm.errorText(), QStringLiteral("Could not refresh the access feed."));
    QCOMPARE(vm.entries()->rowCount(), 2);
    QVERIFY(!vm.authFailure());
}

void TestAccessControlViewModel::refresh_http401_setsAuthFailureAndClearsProtectedData()
{
    AdminSession::instance().setKey(QStringLiteral("sp4-test-key"));
    CapturingNam nam(QByteArrayLiteral("{\"status\":\"error\",\"message\":\"Invalid admin key\"}"),
                     QNetworkReply::AuthenticationRequiredError, 401);
    AccessControlViewModel vm(nullptr, &nam);
    vm.applyRecent(feedBody());                 // protected rows on screen
    QSignalSpy auth(&vm, &AccessControlViewModel::authFailureChanged);
    vm.refresh();
    QVERIFY(auth.wait(1000));
    QVERIFY(vm.authFailure());
    QCOMPARE(vm.errorText(),
             QStringLiteral("Admin authentication failed — re-enter via admin login."));
    QCOMPARE(vm.entries()->rowCount(), 0);      // protected data cleared
    QCOMPARE(vm.entriesToday(), 0);
    QVERIFY(vm.lastEntryAt().isEmpty());
    QVERIFY(vm.updatedAt().isEmpty());
    QVERIFY(!vm.stale());
    QVERIFY(!vm.emptyFeed());
    QVERIFY(!vm.initialLoadFailed());           // auth has its own state
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("sp4-test-key"));   // not cleared here
}

void TestAccessControlViewModel::refresh_http401WithEmptyOrMalformedBodyIsStillAuthFailure()
{
    // HTTP 401 is AUTHORITATIVE: it must not depend on parsing the body. The
    // in-band "Invalid admin key" message match is only a secondary path.
    const QList<QByteArray> bodies{ QByteArray(), QByteArrayLiteral("<html>401</html>") };
    for (const QByteArray &body : bodies) {
        CapturingNam nam(body, QNetworkReply::AuthenticationRequiredError, 401);
        AccessControlViewModel vm(nullptr, &nam);
        vm.applyRecent(feedBody());             // protected rows on screen
        QSignalSpy auth(&vm, &AccessControlViewModel::authFailureChanged);
        vm.refresh();
        QVERIFY(auth.wait(1000));
        QVERIFY(vm.authFailure());
        QCOMPARE(vm.errorText(),
                 QStringLiteral("Admin authentication failed — re-enter via admin login."));
        QCOMPARE(vm.entries()->rowCount(), 0);  // protected data cleared
        QCOMPARE(vm.entriesToday(), 0);
        QVERIFY(vm.lastEntryAt().isEmpty());
        QVERIFY(vm.updatedAt().isEmpty());
        QVERIFY(!vm.stale());
    }
}

void TestAccessControlViewModel::authFailure_resetByLaterSuccess()
{
    AccessControlViewModel vm;
    vm.applyRecent(R"({"status":"error","message":"Invalid admin key"})");
    QVERIFY(vm.authFailure());
    vm.applyRecent(feedBody());
    QVERIFY(!vm.authFailure());
    QVERIFY(vm.errorText().isEmpty());
    QCOMPARE(vm.entries()->rowCount(), 2);
}

void TestAccessControlViewModel::supersededRequestSeqIsNotCurrent()
{
    AccessControlViewModel vm;
    const quint64 first = vm.nextRequestSeq();
    QVERIFY(vm.isCurrentRequest(first));
    const quint64 second = vm.nextRequestSeq();
    QVERIFY(!vm.isCurrentRequest(first));
    QVERIFY(vm.isCurrentRequest(second));
}

void TestAccessControlViewModel::refresh_supersededReplyIsDropped()
{
    CapturingNam nam(feedBody());
    AccessControlViewModel vm(nullptr, &nam);
    QSignalSpy data(&vm, &AccessControlViewModel::dataChanged);
    vm.refresh();                               // seq 1 — superseded before it lands
    vm.refresh();                               // seq 2 — the only one applied
    QTRY_COMPARE_WITH_TIMEOUT(data.count(), 1, 1000);
    QTest::qWait(50);                           // both replies have finished by now
    QCOMPARE(data.count(), 1);                  // the seq-1 reply was dropped
    QVERIFY(!vm.loading());
}

QTEST_MAIN(TestAccessControlViewModel)
#include "tst_accesscontrolviewmodel.moc"
```

- [ ] **Step 3: Register the VM + test in CMake**

In `qt-app/quick/CMakeLists.txt`, add to the `witsquickmodule` `SOURCES` list after `viewmodels/ReportingViewModel.h viewmodels/ReportingViewModel.cpp`:

```cmake
        viewmodels/AccessControlViewModel.h viewmodels/AccessControlViewModel.cpp
```

After the `tst_visitlogsviewmodel` block, register:

```cmake
# --- AccessControlViewModel unit test (C++ QtTest, offscreen). admin_key in
# the POST body, 401/5xx/transport paths and the generation guard, all via the
# CapturingNam harness (qt-app/testsupport) — no live network. ---
wits_add_qttest(tst_accesscontrolviewmodel
    SOURCES tests/tst_accesscontrolviewmodel.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

- [ ] **Step 4: Build + run to verify RED**

```powershell
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4 --target tst_accesscontrolviewmodel
ctest --test-dir C:/b/loams-sp4 -R tst_accesscontrolviewmodel --output-on-failure
```

Expected: configure + build succeed (skeleton links, `witsquickmodule` compiles the VM + moc/QML registration); **FAIL** at assertions (no POST issued so `lastOp` is `UnknownOperation`; `loading()` false; `rowCount()` 0; signals never emitted → `wait()` times out; `isCurrentRequest` false).

- [ ] **Step 5: Implement `AccessControlViewModel.cpp`**

Replace the skeleton with:

```cpp
#include "AccessControlViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QVariant>
#include "AdminSession.h"
#include "HttpForm.h"
#include "SettingsViewModel.h"
#include "apiconfig.h"
#include "loginparser.h"

AccessControlViewModel::AccessControlViewModel(QObject *parent, QNetworkAccessManager *nam)
    : QObject(parent)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
}

bool AccessControlViewModel::emptyFeed() const
{
    return m_hasLoaded && m_entries.rowCount() == 0;
}

bool AccessControlViewModel::initialLoadFailed() const
{
    return !m_hasLoaded && !m_authFailure && !m_errorText.isEmpty();
}

void AccessControlViewModel::refresh()
{
    setLoading(true);
    // admin_key rides the urlencoded POST body ONLY (never the query string):
    // the backend's extractAdminKey() reads $_POST, and a secret in the URL
    // would leak into access logs.
    QNetworkRequest req = HttpForm::formRequest(
        ApiConfig::endpoint(QStringLiteral("access_recent.php")));
    QNetworkReply *reply = m_nam->post(req, HttpForm::encodeForm(
        {{QStringLiteral("admin_key"), AdminSession::instance().key()}}));
    const quint64 seq = nextRequestSeq();
    connect(reply, &QNetworkReply::finished, this, [this, reply, seq]() {
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (!isCurrentRequest(seq)) return;   // superseded — drop
        setLoading(false);
        // HTTP 401 is authoritative and checked BEFORE any body handling, so
        // an empty/non-JSON 401 still clears protected data. The in-band
        // "Invalid admin key" message (applyRecent) is only a secondary path.
        if (httpStatus == 401) { applyAuthFailure(); return; }
        if (!HttpForm::isServerAnswer(hadError, httpStatus, body)) {
            applyFailure(tr("Network error. Please try again."));
            return;
        }
        applyRecent(body);                    // 5xx-with-body lands here as invalid
    });
}

void AccessControlViewModel::applyRecent(const QByteArray &raw)
{
    const LoginParser::RecentFeedResult r = LoginParser::parseRecentFeed(raw);
    if (!r.valid) {
        if (SettingsViewModel::isAuthFailureMessage(r.error))
            applyAuthFailure();
        else
            applyFailure(tr("Could not refresh the access feed."));
        return;
    }
    m_entries.setEntries(r.entries);
    m_entriesToday = r.entriesToday;
    m_lastEntryAt = r.lastEntryAt;
    m_updatedAt = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));   // client wall clock
    m_hasLoaded = true;
    setStale(false);
    setAuthFailure(false);
    setError(QString());
    emit dataChanged();
}

void AccessControlViewModel::applyFailure(const QString &message)
{
    // Keep the last-known rows + counts and FREEZE updatedAt; the view marks
    // them stale. Only stale when there is a prior successful load on screen.
    setStale(m_hasLoaded);
    setAuthFailure(false);
    setError(message);
    emit dataChanged();
}

void AccessControlViewModel::applyAuthFailure()
{
    // Rejected key: do not leave protected rows on screen. Does NOT clear
    // AdminSession or navigate away — no admin page does that today.
    m_entries.clear();
    m_entriesToday = 0;
    m_lastEntryAt.clear();
    m_updatedAt.clear();
    m_hasLoaded = false;
    setStale(false);
    setAuthFailure(true);
    setError(tr("Admin authentication failed — re-enter via admin login."));
    emit dataChanged();
}

void AccessControlViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void AccessControlViewModel::setStale(bool v)
{
    if (m_stale == v) return;
    m_stale = v;
    emit staleChanged();
}

void AccessControlViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}

void AccessControlViewModel::setAuthFailure(bool v)
{
    if (m_authFailure == v) return;
    m_authFailure = v;
    emit authFailureChanged();
}

quint64 AccessControlViewModel::nextRequestSeq()
{
    return ++m_requestSeq;
}

bool AccessControlViewModel::isCurrentRequest(quint64 seq) const
{
    return seq == m_requestSeq;
}
```

- [ ] **Step 6: Build + run to verify GREEN**

```powershell
cmake --build C:/b/loams-sp4 --target tst_accesscontrolviewmodel
ctest --test-dir C:/b/loams-sp4 -R tst_accesscontrolviewmodel --output-on-failure
```

Expected: PASS (13/13).

- [ ] **Step 7: Commit** (via the `commit` skill) — `feat(accesscontrol): add AccessControlViewModel for the admin recent feed`.

---

## Task 5: `AccessControlHub` extensions (intent, lock, live connection surface)

**Files:**
- Modify: `qt-app/quick/AccessControlHub.h`, `qt-app/quick/AccessControlHub.cpp`
- Test: `qt-app/quick/tests/tst_accesscontrolhub.cpp`

**Interfaces:**
- Consumes: `AccessControlService::{enable(const ProviderDescriptor &, const QVariantMap &), disable(), isEnabled(), connectionState(), healthMonitor()}`, `AccessControlService::connectionStateChanged(AccessControl::ConnectionState)`, `HealthMonitor::healthChanged(const AccessControl::HealthSnapshot &)`, `HealthMonitor::snapshot(const QString &)`, `TurnstileProvider::defaultDescriptor()` (`providerId == "turnstile"`), `AppSettings`, per-poll freshness (Task 1).
- Produces (on `AccessControlHub`, exposed to QML as `AccessControl`):
  - `Q_PROPERTY(bool accessEnabled READ isAccessEnabled NOTIFY accessEnabledChanged)` — intent.
  - `Q_PROPERTY(bool enableLocked READ isEnableLocked CONSTANT)`.
  - `Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)` — `ConnectionState` as int (Disconnected=0, Connecting=1, Connected=2, Degraded=3, Error=4).
  - `Q_PROPERTY(QDateTime lastContactAt READ lastContactAt NOTIFY lastContactChanged)`.
  - `Q_INVOKABLE void setAccessEnabled(bool on);` · `bool isEnableLocked() const;` · `int connectionState() const;` · `QDateTime lastContactAt() const;`
  - Signals `accessEnabledChanged()`, `connectionStateChanged()`, `lastContactChanged()` (existing `entryObserved(QVariantMap)` unchanged).
  - `isAccessEnabled()` now returns the intent (`m_intentEnabled`).

- [ ] **Step 1: Extend the header + append stubs (stub-first)**

Replace `qt-app/quick/AccessControlHub.h` with:

```cpp
#ifndef ACCESSCONTROLHUB_H
#define ACCESSCONTROLHUB_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <memory>

#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/accesstypes.h"

class QNetworkAccessManager;
namespace AccessControl { class EventBus; class AccessControlService; }

// Application-owned composition root for Access Control. Owns EventBus +
// AccessProviderFactory + AccessControlService (the service owns HealthMonitor
// and the provider). Maps EntryObserved bus events to entryObserved(QVariantMap).
// Exposed to QML as "AccessControl" via AccessControlSingleton (QML_FOREIGN).
//
// Sub-plan 4: also the home of the app-global LIVE state the admin page binds
// to — the requested monitoring intent (accessEnabled, persisted, separate from
// the connection outcome), the env lock (enableLocked), the service's
// connection state, and the HealthMonitor's last comm time for the provider.
class AccessControlHub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool accessEnabled READ isAccessEnabled NOTIFY accessEnabledChanged)
    Q_PROPERTY(bool enableLocked READ isEnableLocked CONSTANT)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QDateTime lastContactAt READ lastContactAt NOTIFY lastContactChanged)
public:
    explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr,
                              QObject *parent = nullptr);
    ~AccessControlHub() override;

    // Must run before the QML engine loads (enableLocked is CONSTANT).
    void initialize();

    bool isAccessEnabled() const;          // requested/persisted INTENT
    bool isEnableLocked() const { return m_enableLocked; }
    int connectionState() const;           // AccessControl::ConnectionState as int
    QDateTime lastContactAt() const;       // invalid until a first comm

    // Idempotent; persist-first; refuses disable when enableLocked.
    Q_INVOKABLE void setAccessEnabled(bool on);

    static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);

    static AccessControlHub *instance();
    static void setInstance(AccessControlHub *hub);

signals:
    void entryObserved(const QVariantMap &entry);
    void accessEnabledChanged();
    void connectionStateChanged();
    void lastContactChanged();

private:
    void onBusEvent(const AccessControl::AccessEvent &e);
    void onHealthChanged(const AccessControl::HealthSnapshot &s);
    static void persistEnabled(bool on);

    // Declaration order fixes teardown order: reverse destruction is
    // service -> factory -> owned NAM -> bus, so the service tears down the
    // provider (which aborts its reply) while the NAM is still alive.
    std::unique_ptr<AccessControl::EventBus> m_bus;
    std::unique_ptr<QNetworkAccessManager> m_ownedNam;   // only when self-created
    AccessControl::AccessProviderFactory m_factory;      // plain value member
    std::unique_ptr<AccessControl::AccessControlService> m_service;

    QNetworkAccessManager *m_nam = nullptr;   // owned-or-injected; non-owning ptr

    // Retained by initialize() on EVERY path (incl. flag-off) so a later
    // runtime enable has a descriptor + config to pass.
    AccessControl::ProviderDescriptor m_descriptor;
    QVariantMap m_config;                     // raw pollIntervalMs / gateId
    QString m_providerId;
    bool m_intentEnabled = false;
    bool m_enableLocked = false;
    QDateTime m_lastContactSeen;              // de-dupes lastContactChanged
};

#endif // ACCESSCONTROLHUB_H
```

Append stubs to the end of `qt-app/quick/AccessControlHub.cpp` (keep everything else as-is for now):

```cpp
// --- Sub-plan 4 stubs (replaced in Step 4) ---
int AccessControlHub::connectionState() const { return 0; }
QDateTime AccessControlHub::lastContactAt() const { return {}; }
void AccessControlHub::setAccessEnabled(bool) {}
void AccessControlHub::onHealthChanged(const HealthSnapshot &) {}
void AccessControlHub::persistEnabled(bool) {}
```

- [ ] **Step 2: Write the failing hub tests**

In `qt-app/quick/tests/tst_accesscontrolhub.cpp`, add to `private slots:` (after `void envForceEnablesAndEmitsEntry();`):

```cpp
    void connectionStateEnumOrderPinned();
    void setAccessEnabled_persistsTogglesAndIsIdempotent();
    void notLockedWhenEnabledBySetting();
    void enableLocked_refusesDisable();
    void failedStartupKeepsIntent();
    void connectionStateRelaysService();
    void lastContactAdvancesPerPoll();
    void stateOnlyHealthChangeDoesNotEmitLastContact();
```

Append above `QTEST_MAIN(TestAccessControlHub)`:

```cpp
void TestAccessControlHub::connectionStateEnumOrderPinned()
{
    // QML maps these ints to labels (ConnectionState has no Q_ENUM).
    QCOMPARE(int(ConnectionState::Disconnected), 0);
    QCOMPARE(int(ConnectionState::Connecting), 1);
    QCOMPARE(int(ConnectionState::Connected), 2);
    QCOMPARE(int(ConnectionState::Degraded), 3);
    QCOMPARE(int(ConnectionState::Error), 4);
}

void TestAccessControlHub::setAccessEnabled_persistsTogglesAndIsIdempotent()
{
    SequencedNam nam;                        // unqueued requests: valid empty polls
    AccessControlHub hub(&nam);
    hub.initialize();                        // flag off: descriptor/config still retained
    QVERIFY(!hub.isAccessEnabled());
    QSignalSpy spy(&hub, &AccessControlHub::accessEnabledChanged);

    hub.setAccessEnabled(true);
    QVERIFY(hub.isAccessEnabled());
    QCOMPARE(spy.count(), 1);
    { AppSettings s; QCOMPARE(s.value("accessControl/enabled").toBool(), true); }
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() >= 1, 3000);   // service really started
    QVERIFY(nam.lastUrl.path().endsWith(QStringLiteral("turnstile_display.php")));

    hub.setAccessEnabled(true);              // idempotent: no churn, no re-persist signal
    QCOMPARE(spy.count(), 1);

    hub.setAccessEnabled(false);
    QVERIFY(!hub.isAccessEnabled());
    QCOMPARE(spy.count(), 2);
    { AppSettings s; QCOMPARE(s.value("accessControl/enabled").toBool(), false); }
    QCOMPARE(hub.connectionState(), int(ConnectionState::Disconnected));

    hub.setAccessEnabled(false);             // idempotent
    QCOMPARE(spy.count(), 2);
}

void TestAccessControlHub::notLockedWhenEnabledBySetting()
{
    { AppSettings s; s.setValue("accessControl/enabled", true); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());
    QVERIFY(!hub.isEnableLocked());          // only the env var locks
    hub.setAccessEnabled(false);             // so disabling is allowed
    QVERIFY(!hub.isAccessEnabled());
}

void TestAccessControlHub::enableLocked_refusesDisable()
{
    qputenv("WITS_ACCESS_CONTROL", "1");
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isEnableLocked());
    QVERIFY(hub.isAccessEnabled());
    QSignalSpy spy(&hub, &AccessControlHub::accessEnabledChanged);

    hub.setAccessEnabled(false);             // refused
    QVERIFY(hub.isAccessEnabled());
    QCOMPARE(spy.count(), 0);
    { AppSettings s; QVERIFY(!s.contains("accessControl/enabled")); }   // nothing persisted
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() >= 1, 3000);            // still monitoring
}

void TestAccessControlHub::failedStartupKeepsIntent()
{
    SequencedNam nam;
    for (int i = 0; i < 4; ++i)              // baseline + backoff retries all fail
        nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);
    AccessControlHub hub(&nam);
    hub.initialize();                        // flag off
    hub.setAccessEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Degraded), 3000);
    QVERIFY(hub.isAccessEnabled());          // intent NOT reverted by the failed connect
    { AppSettings s; QCOMPARE(s.value("accessControl/enabled").toBool(), true); }
}

void TestAccessControlHub::connectionStateRelaysService()
{
    { AppSettings s; s.setValue("accessControl/enabled", true);
      s.setValue("accessControl/pollIntervalMs", 250); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    QSignalSpy spy(&hub, &AccessControlHub::connectionStateChanged);
    hub.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Connected), 3000);
    QVERIFY(spy.count() >= 2);               // Connecting, then Connected
}

void TestAccessControlHub::lastContactAdvancesPerPoll()
{
    { AppSettings s; s.setValue("accessControl/enabled", true);
      s.setValue("accessControl/pollIntervalMs", 250); s.sync(); }
    SequencedNam nam;                        // every poll is a valid empty poll
    AccessControlHub hub(&nam);
    hub.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Connected), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(hub.lastContactAt().isValid(), 3000);
    const QDateTime first = hub.lastContactAt();
    QSignalSpy spy(&hub, &AccessControlHub::lastContactChanged);
    QTRY_VERIFY_WITH_TIMEOUT(hub.lastContactAt() > first, 3000);   // steady empty poll
    QVERIFY(spy.count() >= 1);
}

void TestAccessControlHub::stateOnlyHealthChangeDoesNotEmitLastContact()
{
    SequencedNam nam;
    for (int i = 0; i < 4; ++i)
        nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);
    AccessControlHub hub(&nam);
    hub.initialize();
    QSignalSpy contact(&hub, &AccessControlHub::lastContactChanged);
    QSignalSpy state(&hub, &AccessControlHub::connectionStateChanged);
    hub.setAccessEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(hub.connectionState(), int(ConnectionState::Degraded), 3000);
    QVERIFY(state.count() >= 1);             // state-only HealthMonitor updates happened...
    QCOMPARE(contact.count(), 0);            // ...but lastCommTime never changed
    QVERIFY(!hub.lastContactAt().isValid());
}
```

- [ ] **Step 3: Build + run to verify RED**

```powershell
cmake --build C:/b/loams-sp4 --target tst_accesscontrolhub
ctest --test-dir C:/b/loams-sp4 -R tst_accesscontrolhub --output-on-failure
```

Expected: builds; the 6 pre-existing cases still PASS; **FAIL** at assertions: `setAccessEnabled_persistsTogglesAndIsIdempotent` (intent stays false), `notLockedWhenEnabledBySetting` (disable is a no-op), `enableLocked_refusesDisable` (`isEnableLocked()` false), `failedStartupKeepsIntent` / `connectionStateRelaysService` / `lastContactAdvancesPerPoll` / `stateOnlyHealthChangeDoesNotEmitLastContact` (stub `connectionState()` stays 0 → QTRY timeouts). `connectionStateEnumOrderPinned` passes (a pin, not behavior).

- [ ] **Step 4: Implement — replace `AccessControlHub.cpp`**

```cpp
#include "AccessControlHub.h"

#include <QNetworkAccessManager>

#include "apiconfig.h"
#include "appsettings.h"
#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/eventbus.h"
#include "accesscontrol/healthmonitor.h"
#include "accesscontrol/turnstileprovider.h"

using namespace AccessControl;

namespace { AccessControlHub *g_instance = nullptr; }

AccessControlHub *AccessControlHub::instance() { return g_instance; }
void AccessControlHub::setInstance(AccessControlHub *hub) { g_instance = hub; }

AccessControlHub::AccessControlHub(QNetworkAccessManager *injectedNam, QObject *parent)
    : QObject(parent)
    , m_bus(std::make_unique<EventBus>())
    , m_service(std::make_unique<AccessControlService>(m_bus.get(), &m_factory))
{
    if (injectedNam) {
        m_nam = injectedNam;                 // externally owned; must outlive the hub
    } else {
        m_ownedNam = std::make_unique<QNetworkAccessManager>();
        m_nam = m_ownedNam.get();
    }
    connect(m_bus.get(), &EventBus::eventPublished, this, &AccessControlHub::onBusEvent);

    // Live connection surface (Sub-plan 4): relay the service's state and the
    // HealthMonitor's per-provider comm time to QML.
    connect(m_service.get(), &AccessControlService::connectionStateChanged, this,
            [this](AccessControl::ConnectionState) { emit connectionStateChanged(); });
    connect(m_service->healthMonitor(), &HealthMonitor::healthChanged, this,
            &AccessControlHub::onHealthChanged);
}

AccessControlHub::~AccessControlHub()
{
    // Sever the relays first: members declared after m_service (m_providerId,
    // m_lastContactSeen) are destroyed BEFORE it, so nothing the service emits
    // during its own teardown may reach this half-destroyed hub.
    disconnect(m_service->healthMonitor(), nullptr, this, nullptr);
    disconnect(m_service.get(), nullptr, this, nullptr);
    if (g_instance == this) g_instance = nullptr;
}

QVariantMap AccessControlHub::toAccessEntry(const AccessEvent &e)
{
    const bool hasStudent = !e.subject.isEmpty();
    return QVariantMap{
        {QStringLiteral("hasStudent"), hasStudent},
        {QStringLiteral("student"), hasStudent ? e.subject.toVariantMap() : QVariantMap{}},
        {QStringLiteral("eventId"), e.correlationId},
        {QStringLiteral("at"), e.at},
    };
}

void AccessControlHub::onBusEvent(const AccessEvent &e)
{
    if (e.type == AccessEvent::Type::EntryObserved)
        emit entryObserved(toAccessEntry(e));
}

void AccessControlHub::onHealthChanged(const HealthSnapshot &s)
{
    if (m_providerId.isEmpty() || s.providerId != m_providerId)
        return;
    if (s.lastCommTime == m_lastContactSeen)
        return;                              // state-only update: don't churn the view
    m_lastContactSeen = s.lastCommTime;
    emit lastContactChanged();
}

bool AccessControlHub::isAccessEnabled() const { return m_intentEnabled; }

int AccessControlHub::connectionState() const
{
    return static_cast<int>(m_service->connectionState());
}

QDateTime AccessControlHub::lastContactAt() const
{
    if (m_providerId.isEmpty())
        return {};
    return m_service->healthMonitor()->snapshot(m_providerId).lastCommTime;
}

void AccessControlHub::persistEnabled(bool on)
{
    AppSettings settings;   // through AppSettings so tests isolate
    settings.setValue(QStringLiteral("accessControl/enabled"), on);
    settings.sync();
}

void AccessControlHub::setAccessEnabled(bool on)
{
    if (on == m_intentEnabled)
        return;                              // idempotent: no churn / duplicate persist
    if (!on && m_enableLocked)
        return;                              // WITS_ACCESS_CONTROL forces on: refuse

    persistEnabled(on);                      // the user's request is persisted FIRST
    m_intentEnabled = on;
    emit accessEnabledChanged();

    // Outcome is independent of intent: a failed connect leaves intent true and
    // the service's own reconnect/backoff keeps retrying.
    if (on)
        m_service->enable(m_descriptor, m_config);
    else
        m_service->disable();
}

void AccessControlHub::initialize()
{
    AppSettings settings;   // through AppSettings so tests isolate

    // Register the provider (whether or not enabled) so a later runtime toggle
    // can enable without re-registering. Creator captures the hub's NAM+baseUrl.
    m_descriptor = TurnstileProvider::defaultDescriptor();
    m_providerId = m_descriptor.providerId;
    QNetworkAccessManager *nam = m_nam;
    const QUrl baseUrl(ApiConfig::baseUrl());
    m_factory.registerProvider(m_descriptor,
        [nam, baseUrl](const ProviderDescriptor &, const QVariantMap &cfg, QObject *parent)
            -> IAccessProvider * {
            return new TurnstileProvider(nam, baseUrl, cfg, parent);
        });

    // Config retained RAW on every path — the provider owns normalization.
    m_config = QVariantMap{
        {QStringLiteral("pollIntervalMs"),
         settings.value(QStringLiteral("accessControl/pollIntervalMs"), 1500)},
        {QStringLiteral("gateId"),
         settings.value(QStringLiteral("accessControl/gateId"), QStringLiteral("turnstile"))},
    };

    // Enablement precedence: env force-on > setting > false.
    const QString env = qEnvironmentVariable("WITS_ACCESS_CONTROL").trimmed().toLower();
    const bool forceOn = (env == QLatin1String("1") || env == QLatin1String("true"));
    m_enableLocked = forceOn;
    m_intentEnabled = forceOn
                      || settings.value(QStringLiteral("accessControl/enabled"), false).toBool();
    if (!m_intentEnabled)
        return;

    m_service->enable(m_descriptor, m_config);
}
```

- [ ] **Step 5: Build + run to verify GREEN (and main.cpp still builds)**

```powershell
cmake --build C:/b/loams-sp4 --target tst_accesscontrolhub WITSQuick
ctest --test-dir C:/b/loams-sp4 -R tst_accesscontrolhub --output-on-failure
```

Expected: PASS (14/14: 6 existing + 8 new). `WITSQuick` links unchanged (`main.cpp` still calls `initialize()` then `setInstance()` before the engine, which is exactly what the CONSTANT `enableLocked` needs).

- [ ] **Step 6: Commit** (via the `commit` skill) — `feat(accesscontrol): hub runtime enable intent, env lock and live connection surface`.

---

## Task 6: Pure contact-age formatter (+ stateless hub wrapper)

**Files:**
- Create: `qt-app/core/accesscontrol/contactage.h`, `qt-app/core/accesscontrol/contactage.cpp`
- Create: `qt-app/tests/tst_contactage.cpp`
- Modify: `qt-app/core/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`
- Modify: `qt-app/quick/AccessControlHub.h`, `qt-app/quick/AccessControlHub.cpp`, `qt-app/quick/tests/tst_accesscontrolhub.cpp`

**Interfaces:**
- Consumes: `AccessControlHub` (Task 5).
- Produces:
  - `QString AccessControl::formatContactAge(bool monitoringOn, const QDateTime &lastContact, const QDateTime &now);` — `"Monitoring off"` when `!monitoringOn`; `"No contact yet"` when `lastContact` (or `now`) is invalid; else age `s = max(0, lastContact.secsTo(now))` → `"Last contact <s> s ago"` for `s < 60`, `"Last contact <s/60> min ago"` otherwise.
  - `Q_INVOKABLE QString AccessControlHub::contactAgeText(bool monitoringOn, const QVariant &lastContact, const QVariant &now) const;` — stateless QML entry point (`QVariant` so a JS `null`/`Invalid Date` maps cleanly to an invalid `QDateTime`).

- [ ] **Step 1: Create the header + a stub `.cpp`, and the hub wrapper (stub-first)**

`qt-app/core/accesscontrol/contactage.h`:

```cpp
#ifndef ACCESSCONTROL_CONTACTAGE_H
#define ACCESSCONTROL_CONTACTAGE_H

#include <QDateTime>
#include <QString>

namespace AccessControl {

// Pure presentation formatter for the admin page's feed-contact tile
// (Sub-plan 4, refinement 4). The view re-evaluates it against an advancing
// presentation clock, so the age keeps climbing with no new events.
//   !monitoringOn            -> "Monitoring off"
//   no prior contact         -> "No contact yet"
//   age < 60 s               -> "Last contact N s ago"
//   otherwise                -> "Last contact N min ago"   (floored minutes)
// A negative age (clock skew) clamps to 0.
QString formatContactAge(bool monitoringOn, const QDateTime &lastContact, const QDateTime &now);

} // namespace AccessControl

#endif // ACCESSCONTROL_CONTACTAGE_H
```

`qt-app/core/accesscontrol/contactage.cpp` — **stub**:

```cpp
#include "accesscontrol/contactage.h"

namespace AccessControl {

QString formatContactAge(bool, const QDateTime &, const QDateTime &)
{
    return QString();   // stub — replaced in Step 5
}

} // namespace AccessControl
```

In `qt-app/core/CMakeLists.txt`, add to the `witscore` source list after `accesscontrol/turnstileprovider.h accesscontrol/turnstileprovider.cpp`:

```cmake
    accesscontrol/contactage.h accesscontrol/contactage.cpp
```

In `qt-app/quick/AccessControlHub.h`, after the `Q_INVOKABLE void setAccessEnabled(bool on);` declaration:

```cpp
    // Stateless QML entry point for the pure contact-age formatter. QVariant
    // params so a JS null / Invalid Date becomes an invalid QDateTime.
    Q_INVOKABLE QString contactAgeText(bool monitoringOn, const QVariant &lastContact,
                                       const QVariant &now) const;
```

In `qt-app/quick/AccessControlHub.cpp`, add `#include "accesscontrol/contactage.h"` to the includes and append:

```cpp
QString AccessControlHub::contactAgeText(bool monitoringOn, const QVariant &lastContact,
                                         const QVariant &now) const
{
    return formatContactAge(monitoringOn, lastContact.toDateTime(), now.toDateTime());
}
```

- [ ] **Step 2: Write the formatter tests**

`qt-app/tests/tst_contactage.cpp`:

```cpp
#include <QtTest>
#include <QTimeZone>
#include "accesscontrol/contactage.h"

using AccessControl::formatContactAge;

class TestContactAge : public QObject
{
    Q_OBJECT
private slots:
    void monitoringOffWinsOverAnyTimestamp();
    void noContactYetWhenOnAndNoTimestamp();
    void secondsUnderAMinute();
    void minutesFromSixtySeconds();
    void ageIncreasesWithNowForSameContact();
    void clockSkewClampsToZero();

private:
    static QDateTime t0() { return QDateTime(QDate(2026, 9, 30), QTime(8, 0, 0), QTimeZone::UTC); }
};

void TestContactAge::monitoringOffWinsOverAnyTimestamp()
{
    QCOMPARE(formatContactAge(false, t0(), t0().addSecs(5)), QStringLiteral("Monitoring off"));
    QCOMPARE(formatContactAge(false, QDateTime(), t0()), QStringLiteral("Monitoring off"));
}

void TestContactAge::noContactYetWhenOnAndNoTimestamp()
{
    QCOMPARE(formatContactAge(true, QDateTime(), t0()), QStringLiteral("No contact yet"));
}

void TestContactAge::secondsUnderAMinute()
{
    QCOMPARE(formatContactAge(true, t0(), t0()), QStringLiteral("Last contact 0 s ago"));
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(3)), QStringLiteral("Last contact 3 s ago"));
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(59)), QStringLiteral("Last contact 59 s ago"));
}

void TestContactAge::minutesFromSixtySeconds()
{
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(60)), QStringLiteral("Last contact 1 min ago"));
    QCOMPARE(formatContactAge(true, t0(), t0().addSecs(150)), QStringLiteral("Last contact 2 min ago"));
}

void TestContactAge::ageIncreasesWithNowForSameContact()
{
    // Same lastContactAt, advancing "now" (no new events, e.g. an outage):
    // the age must keep climbing.
    const QDateTime last = t0();
    QCOMPARE(formatContactAge(true, last, last.addSecs(5)), QStringLiteral("Last contact 5 s ago"));
    QCOMPARE(formatContactAge(true, last, last.addSecs(20)), QStringLiteral("Last contact 20 s ago"));
    QCOMPARE(formatContactAge(true, last, last.addSecs(125)), QStringLiteral("Last contact 2 min ago"));
}

void TestContactAge::clockSkewClampsToZero()
{
    QCOMPARE(formatContactAge(true, t0().addSecs(10), t0()), QStringLiteral("Last contact 0 s ago"));
}

QTEST_APPLESS_MAIN(TestContactAge)
#include "tst_contactage.moc"
```

In `qt-app/quick/tests/tst_accesscontrolhub.cpp`, add `void contactAgeText_delegatesToPureFormatter();` to `private slots:` and implement above `QTEST_MAIN`:

```cpp
void TestAccessControlHub::contactAgeText_delegatesToPureFormatter()
{
    AccessControlHub hub;                    // never initialized: stateless helper still works
    const QDateTime t(QDate(2026, 9, 30), QTime(8, 0, 0), QTimeZone::UTC);
    QCOMPARE(hub.contactAgeText(true, QVariant(t), QVariant(t.addSecs(5))),
             QStringLiteral("Last contact 5 s ago"));
    QCOMPARE(hub.contactAgeText(true, QVariant(), QVariant(t)),
             QStringLiteral("No contact yet"));
    QCOMPARE(hub.contactAgeText(false, QVariant(t), QVariant(t)),
             QStringLiteral("Monitoring off"));
}
```

- [ ] **Step 3: Register `tst_contactage`**

In `qt-app/tests/CMakeLists.txt`, after the `tst_turnstileprovider` block:

```cmake
# --- Access Control: pure contact-age formatter (pure core, no offscreen) ---
wits_add_qttest(tst_contactage
    SOURCES
        tst_contactage.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/contactage.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/contactage.h
    INCLUDES ${CMAKE_SOURCE_DIR}/core)
```

- [ ] **Step 4: Build + run to verify RED**

```powershell
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4 --target tst_contactage tst_accesscontrolhub
ctest --test-dir C:/b/loams-sp4 -R "tst_contactage|tst_accesscontrolhub" --output-on-failure
```

Expected: configure + build succeed (stub links into both); **FAIL** at assertions — every `tst_contactage` case (stub returns `""`) and `tst_accesscontrolhub::contactAgeText_delegatesToPureFormatter`.

- [ ] **Step 5: Implement `contactage.cpp`**

Replace the stub with:

```cpp
#include "accesscontrol/contactage.h"

#include <QtGlobal>

namespace AccessControl {

QString formatContactAge(bool monitoringOn, const QDateTime &lastContact, const QDateTime &now)
{
    if (!monitoringOn)
        return QStringLiteral("Monitoring off");
    if (!lastContact.isValid() || !now.isValid())
        return QStringLiteral("No contact yet");
    const qint64 secs = qMax<qint64>(0, lastContact.secsTo(now));   // skew clamps to 0
    if (secs < 60)
        return QStringLiteral("Last contact %1 s ago").arg(secs);
    return QStringLiteral("Last contact %1 min ago").arg(secs / 60);
}

} // namespace AccessControl
```

- [ ] **Step 6: Build + run to verify GREEN**

```powershell
cmake --build C:/b/loams-sp4 --target tst_contactage tst_accesscontrolhub
ctest --test-dir C:/b/loams-sp4 -R "tst_contactage|tst_accesscontrolhub" --output-on-failure
```

Expected: PASS (`tst_contactage` 6/6, `tst_accesscontrolhub` 15/15).

- [ ] **Step 7: Commit** (via the `commit` skill) — `feat(accesscontrol): pure contact-age formatter with a QML hub entry point`.

---

## Task 7: `AccessControlScreen.qml` + navigation wiring + QuickTests

**Files:**
- Create: `qt-app/quick/qml/admin/AccessControlScreen.qml`
- Create: `qt-app/quick/tests/tst_qml_accesscontrol.qml`
- Modify: `qt-app/quick/viewmodels/Navigator.h`, `qt-app/quick/qml/admin/AdminScreen.qml`, `qt-app/quick/CMakeLists.txt`
- Modify: `qt-app/quick/tests/tst_qml_admin.cpp`, `qt-app/quick/tests/tst_qml_theme.cpp`, `qt-app/quick/tests/tst_qml_components.cpp`, `qt-app/quick/tests/tst_qml_adminshell.qml`, `qt-app/quick/tests/tst_navigator.cpp`

**Interfaces:**
- Consumes: `AccessControlViewModel` props (Task 4: `entriesToday`, `lastEntryAt`, `updatedAt`, `emptyFeed`, `initialLoadFailed`, `loading`, `stale`, `errorText`, `authFailure`, `entries`, `refresh()`); the `AccessControl` singleton (Tasks 5–6: `accessEnabled`, `enableLocked`, `connectionState`, `lastContactAt`, `setAccessEnabled(bool)`, `contactAgeText(bool, var, var)`); components `LCard` (`padding`, default `content`), `LCheckbox` (`checked`, `label`, `toggled(bool checked)`; its MouseArea assigns `checked` imperatively), `LStatTile` (`label`, `value`, `caption`, `variant`), `LTable` (`columns`, `model`, `emptyStateText`, `rowCount`, child `tableEmptyState`), `LButton`, `LToast` (`message`, `severity`; auto-dismiss sets `message = ""`).
- Produces:
  - `Navigator::AdminPage::AccessControl` (appended last, value 6).
  - `AccessControlScreen` QML type: `property var vm`, `property var hub: AccessControl` (injectable), `property var now` (presentation clock), readonly `monitoringOn`, `enableLocked`, `connectionState`, `connectionLabel`, `contactAgeText`; objectNames `monitorToggle`, `monitoringHelp`, `lockedNote`, `entriesTodayTile`, `lastEntryTile`, `connectionTile`, `contactTile`, `updatedLabel`, `staleBadge`, `authError`, `refreshButton`, `entriesTable`, `accessToast`, `ageTimer`.
  - `AdminScreen`: page key `"accesscontrol"`, title `Access Control`, loaded item objectName `accessControlPage`.

Every QuickTest target (`tst_qml_theme`, `tst_qml_components`, `tst_qml_kiosk`, `tst_qml_admin`) runs **every** `tst_*.qml` in `QUICK_TEST_SOURCE_DIR`. Today only `tst_qml_kiosk` installs a hub, so the other three log `QCRITICAL qmlRegisterSingletonType(): "AccessControl" is not available because the callback function returns a null pointer` (visible in the Sub-plan 3 run log; harmless only because `KioskScreen`'s `Connections` tolerates a null target). The new screen calls `AccessControl.contactAgeText(...)`, so all four targets must install a disabled hub.

- [ ] **Step 1: Skeleton wiring (stub-first)**

In `qt-app/quick/viewmodels/Navigator.h`, append the new page **last** so existing values don't shift:

```cpp
    enum AdminPage { Dashboard, Search, VisitLogs, Database, Reporting, Settings, AccessControl };
```

Create `qt-app/quick/qml/admin/AccessControlScreen.qml` — **skeleton** (replaced in Step 5):

```qml
import QtQuick
import LOAMS

// SKELETON (Sub-plan 4, Task 7 Step 1) — replaced in Step 5.
Rectangle {
    id: screen
    property var vm
    property var hub: AccessControl
    property var now: new Date()
}
```

In `qt-app/quick/CMakeLists.txt`, add to `QML_FILES` after `qml/admin/SettingsScreen.qml`:

```cmake
        qml/admin/AccessControlScreen.qml
```

Replace `qt-app/quick/tests/tst_qml_admin.cpp` with:

```cpp
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include "AccessControlHub.h"

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // Every QuickTest target runs every tst_*.qml in QUICK_TEST_SOURCE_DIR,
        // and AccessControlScreen/KioskScreen resolve the AccessControl
        // singleton. Live-but-disabled hub (never initialize() -> no polling).
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_admin, Setup)
#include "tst_qml_admin.moc"
```

Replace `qt-app/quick/tests/tst_qml_theme.cpp` with:

```cpp
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <QQmlContext>
#include "AccessControlHub.h"

class Setup : public QObject
{
    Q_OBJECT
public:
    Setup() = default;

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The statically-linked witsquick module embeds its qmldir under
        // qrc:/qt/qml; make it importable from the .qml test files.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // Every QuickTest target runs every tst_*.qml in QUICK_TEST_SOURCE_DIR,
        // so the AccessControl singleton must resolve here too. Live-but-
        // disabled hub (never initialize() -> no polling).
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_theme, Setup)
#include "tst_qml_theme.moc"
```

Replace `qt-app/quick/tests/tst_qml_components.cpp` with:

```cpp
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include "AccessControlHub.h"

class Setup : public QObject
{
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The statically-linked witsquick module embeds its qmldir under
        // qrc:/qt/qml; make it importable from the .qml test files. Same
        // escape hatch as tst_qml_theme (Task 5): the literal "import LOAMS"
        // lives only in this test's .qml data file, which qmlimportscanner
        // never sees, so the automatic static-plugin import never fires.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        // Every QuickTest target runs every tst_*.qml in QUICK_TEST_SOURCE_DIR,
        // so the AccessControl singleton must resolve here too. Live-but-
        // disabled hub (never initialize() -> no polling).
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_components, Setup)
#include "tst_qml_components.moc"
```

(All three already link `witsquickmodule`, which contains `AccessControlHub.cpp`; no CMake change needed for them.)

- [ ] **Step 2: Write the QML + navigator tests**

Create `qt-app/quick/tests/tst_qml_accesscontrol.qml` (a dedicated file, like `tst_qml_adminshell.qml`, so `tst_qml_admin.qml`'s y-band geometry ledger is untouched; it is discovered at runtime via `QUICK_TEST_SOURCE_DIR`):

```qml
import QtQuick
import QtTest
import LOAMS

// AccessControlScreen QuickTests (Sub-plan 4). Stub VM + stub hub drive the
// screen offline; a null-VM/null-hub mount proves the defensive fallbacks; a
// default mount proves `hub` defaults to the real (disabled) AccessControl
// singleton that every QuickTest target's Setup installs.
// Geometry: ac 0..1100 x 0..760 | vmlessAc 1150..2250 x 0..760 |
//           singletonAc 1150..2250 x 800..1560.
Item {
    id: host
    width: 2300; height: 1600

    ListModel { id: acRows }

    QtObject {
        id: acVmStub
        property int entriesToday: 7
        property string lastEntryAt: "2026-09-30 08:15:00"
        property string updatedAt: "08:15:05"
        property bool emptyFeed: false
        property bool initialLoadFailed: false
        property bool loading: false
        property bool stale: false
        property string errorText: ""
        property bool authFailure: false
        property var entries: acRows
        property int refreshCount: 0
        function refresh() { refreshCount++ }
    }

    QtObject {
        id: hubStub
        property bool accessEnabled: false
        property bool enableLocked: false
        property int connectionState: 0
        property var lastContactAt: null
        property bool refuse: false
        property int setCalls: 0
        function setAccessEnabled(on) { setCalls++; if (!refuse) accessEnabled = on }
    }

    AccessControlScreen { id: ac; width: 1100; height: 760; vm: acVmStub; hub: hubStub }
    AccessControlScreen { id: vmlessAc; x: 1150; width: 1100; height: 760; hub: null }
    AccessControlScreen { id: singletonAc; x: 1150; y: 800; width: 1100; height: 760 }

    TestCase {
        name: "AccessControlScreen"
        when: windowShown

        function init() {
            acRows.clear();
            acRows.append({ name: "Test Student A", schoolId: "TEST-0001", course: "BS Test",
                            department: "Dept Test", createdAt: "2026-09-30 08:15:00",
                            reader: "1", card: "CARD0012", known: true });
            acVmStub.entriesToday = 7;
            acVmStub.lastEntryAt = "2026-09-30 08:15:00";
            acVmStub.updatedAt = "08:15:05";
            acVmStub.emptyFeed = false;
            acVmStub.initialLoadFailed = false;
            acVmStub.loading = false;
            acVmStub.stale = false;
            acVmStub.errorText = "";
            acVmStub.authFailure = false;
            acVmStub.refreshCount = 0;
            hubStub.accessEnabled = false;
            hubStub.enableLocked = false;
            hubStub.connectionState = 0;
            hubStub.lastContactAt = null;
            hubStub.refuse = false;
            hubStub.setCalls = 0;
            ac.now = new Date();
        }

        function test_tilesRenderVmCounts() {
            compare(findChild(ac, "entriesTodayTile").value, "7");
            compare(findChild(ac, "lastEntryTile").value, "2026-09-30 08:15:00");
        }
        function test_lastEntryShowsDashWhenNone() {
            acVmStub.lastEntryAt = "";
            compare(findChild(ac, "lastEntryTile").value, "—");
        }
        function test_tableBindsVmEntries() {
            var table = findChild(ac, "entriesTable");
            verify(table !== null);
            tryCompare(table, "rowCount", 1);
        }
        function test_updatedLabelAndStaleBadge() {
            var label = findChild(ac, "updatedLabel");
            var badge = findChild(ac, "staleBadge");
            compare(label.text, "Updated 08:15:05");
            compare(badge.visible, false);
            acVmStub.stale = true;
            compare(badge.visible, true);
            acVmStub.updatedAt = "";
            compare(label.text, "Not loaded yet");
        }
        function test_refreshButtonInvokesVm() {
            var btn = findChild(ac, "refreshButton");
            mouseClick(btn);
            compare(acVmStub.refreshCount, 1);
        }
        function test_refreshDisabledWhileLoading() {
            var btn = findChild(ac, "refreshButton");
            acVmStub.loading = true;
            compare(btn.enabled, false);
            compare(btn.text, "Refreshing…");
        }
        function test_emptyFeedShowsNoEntriesYet() {
            acRows.clear();
            acVmStub.emptyFeed = true;
            var table = findChild(ac, "entriesTable");
            tryCompare(table, "rowCount", 0);
            compare(findChild(table, "tableEmptyState").text, "No entries yet");
        }
        function test_failedInitialLoadIsDistinctFromEmpty() {
            acRows.clear();
            acVmStub.updatedAt = "";
            acVmStub.errorText = "Network error. Please try again.";
            acVmStub.initialLoadFailed = true;
            var table = findChild(ac, "entriesTable");
            compare(findChild(table, "tableEmptyState").text,
                    "Could not load the access feed. Use Refresh to retry.");
        }
        function test_authFailureShowsInlineErrorState() {
            acRows.clear();
            acVmStub.authFailure = true;
            compare(findChild(ac, "authError").visible, true);
            var table = findChild(ac, "entriesTable");
            compare(findChild(table, "tableEmptyState").text,
                    "Admin authentication failed — re-enter via admin login.");
        }
        // LToast's auto-dismiss sets message="" imperatively, so the screen
        // must raise it imperatively too (the DatabaseScreen idiom).
        function test_toastShowsSecondErrorAfterFirstDismissed() {
            var toast = findChild(ac, "accessToast");
            acVmStub.errorText = "First error";
            compare(toast.message, "First error");
            toast.message = "";                       // simulate auto-dismiss
            acVmStub.errorText = "";
            acVmStub.errorText = "Second error";
            compare(toast.message, "Second error");
        }
        function test_toggleReflectsHubIntent() {
            var toggle = findChild(ac, "monitorToggle");
            compare(toggle.checked, false);
            hubStub.accessEnabled = true;
            compare(toggle.checked, true);
        }
        function test_toggleClickCallsHub() {
            var toggle = findChild(ac, "monitorToggle");
            mouseClick(toggle);
            compare(hubStub.setCalls, 1);
            compare(hubStub.accessEnabled, true);
            compare(toggle.checked, true);
        }
        function test_refusedToggleSnapsBackAndKeepsBinding() {
            var toggle = findChild(ac, "monitorToggle");
            hubStub.refuse = true;
            mouseClick(toggle);                       // LCheckbox flips locally...
            compare(hubStub.setCalls, 1);
            compare(toggle.checked, false);           // ...binding re-asserted to hub intent
            hubStub.accessEnabled = true;             // and the binding is still live
            compare(toggle.checked, true);
        }
        function test_lockedDisablesToggleAndShowsNote() {
            var toggle = findChild(ac, "monitorToggle");
            hubStub.accessEnabled = true;
            hubStub.enableLocked = true;
            compare(toggle.enabled, false);
            compare(findChild(ac, "lockedNote").visible, true);
            mouseClick(toggle);
            compare(hubStub.setCalls, 0);
            compare(toggle.checked, true);
        }
        function test_helperTextExplainsScope() {
            verify(findChild(ac, "monitoringHelp").text.indexOf("does not disable the physical gate") >= 0);
            compare(findChild(ac, "lockedNote").visible, false);
        }
        function test_connectionStateLabels() {
            var tile = findChild(ac, "connectionTile");
            var labels = ["Disconnected", "Connecting", "Connected", "Degraded", "Error"];
            for (var i = 0; i < labels.length; i++) {
                hubStub.connectionState = i;
                compare(tile.value, labels[i]);
            }
        }
        function test_contactAgeThreeStates() {
            var tile = findChild(ac, "contactTile");
            hubStub.accessEnabled = false;
            compare(tile.value, "Monitoring off");
            hubStub.accessEnabled = true;
            hubStub.lastContactAt = null;
            compare(tile.value, "No contact yet");
            var t = new Date(2026, 8, 30, 8, 0, 0);
            hubStub.lastContactAt = t;
            ac.now = new Date(t.getTime() + 3000);
            compare(tile.value, "Last contact 3 s ago");
        }
        function test_contactAgeAdvancesWithoutNewEvents() {
            var tile = findChild(ac, "contactTile");
            hubStub.accessEnabled = true;
            var t = new Date(2026, 8, 30, 8, 0, 0);
            hubStub.lastContactAt = t;                // never changes below
            ac.now = new Date(t.getTime() + 5000);
            compare(tile.value, "Last contact 5 s ago");
            ac.now = new Date(t.getTime() + 125000);
            compare(tile.value, "Last contact 2 min ago");
        }
        function test_ageTimerRunsOnlyWhileMonitoring() {
            var timer = findChild(ac, "ageTimer");
            compare(timer.running, false);
            hubStub.accessEnabled = true;
            compare(timer.running, true);
        }
        function test_vmlessMountRendersFallbacks() {
            compare(findChild(vmlessAc, "entriesTodayTile").value, "0");
            compare(findChild(vmlessAc, "lastEntryTile").value, "—");
            compare(findChild(vmlessAc, "connectionTile").value, "Disconnected");
            compare(findChild(vmlessAc, "contactTile").value, "");
            compare(findChild(vmlessAc, "monitorToggle").enabled, false);
            compare(findChild(vmlessAc, "refreshButton").enabled, false);
            compare(findChild(vmlessAc, "updatedLabel").text, "Not loaded yet");
            compare(findChild(vmlessAc, "authError").visible, false);
            compare(findChild(vmlessAc, "staleBadge").visible, false);
        }
        function test_defaultHubIsTheAccessControlSingleton() {
            compare(singletonAc.hub, AccessControl);
            compare(findChild(singletonAc, "monitorToggle").checked, false);   // disabled harness hub
            compare(findChild(singletonAc, "contactTile").value, "Monitoring off");
        }
    }
}
```

In `qt-app/quick/tests/tst_qml_adminshell.qml`:

1. In `test_allSidebarItemsActivate`, replace the `keys` line and its comment's "six" with seven:

```qml
            var keys = ["dashboard","search","visitlogs","database","reporting","settings","accesscontrol"];
```

2. Append before the closing `}` of the `TestCase`:

```qml
        // Sub-plan 4: the Access Control page is fully wired through the real
        // sidebar, Navigator, header title and Loader — and the autoLoad gate
        // keeps its real AccessControlViewModel offline.
        function test_accessControlItemRoutesToAccessControlScreen() {
            var nav = findChild(shell, "sideNav");
            var loader = findChild(shell, "pageLoader");
            var header = findChild(shell, "pageHeader");
            nav.activate("accesscontrol");
            compare(Navigator.adminPage, Navigator.AccessControl);
            compare(loader.item.objectName, "accessControlPage");
            compare(header.title, "Access Control");
            compare(nav.currentPage, "accesscontrol");
            compare(loader.item.vm.loading, false);    // no refresh() under autoLoad: false
            compare(loader.item.hub, AccessControl);   // production default binding
        }
```

In `qt-app/quick/tests/tst_navigator.cpp`, add `void accessControlAppendedLast();` to `private slots:` and implement above `QTEST_MAIN`:

```cpp
void TestNavigator::accessControlAppendedLast()
{
    // Appended LAST so no existing AdminPage value shifts.
    QCOMPARE(int(Navigator::Settings), 5);
    QCOMPARE(int(Navigator::AccessControl), 6);
    Navigator nav;
    QSignalSpy spy(&nav, &Navigator::adminPageChanged);
    nav.showAdminPage(Navigator::AccessControl);
    QCOMPARE(nav.adminPage(), Navigator::AccessControl);
    QCOMPARE(spy.count(), 1);
}
```

- [ ] **Step 3: Build + run to verify RED**

```powershell
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4 --target tst_qml_admin tst_qml_theme tst_qml_components tst_qml_kiosk tst_navigator tst_appshell
ctest --test-dir C:/b/loams-sp4 -R "tst_qml_|tst_navigator|tst_appshell" --output-on-failure
```

Expected: builds (skeleton QML compiles into the module). `tst_navigator` and `tst_appshell` PASS (`accessControlAppendedLast` is a pin; the enum was appended in Step 1). The four `tst_qml_*` targets **FAIL** at runtime in `AccessControlScreen::*` (`findChild(...)` returns null → `TypeError`/compare failures) and `AdminScreenShell::test_accessControlItemRoutesToAccessControlScreen` (no `"accesscontrol"` route: `Navigator.adminPage` stays `Dashboard`, the shell warns `no route for page key accesscontrol`). No more `"AccessControl" is not available` QCRITICAL in any target.

- [ ] **Step 4: Wire all six `AdminScreen.qml` touch points**

In `qt-app/quick/qml/admin/AdminScreen.qml`:

1. VM instance — after `ReportingViewModel { id: reportingVm }`:

```qml
    AccessControlViewModel { id: accessControlVm }
```

2. `pageTitle` switch — add before the `default:` line:

```qml
        case Navigator.AccessControl: return qsTr("Access Control");
```

3. `LSideNav.currentPage` mapping — replace the chain with:

```qml
            currentPage: Navigator.adminPage === Navigator.Search        ? "search"
                       : Navigator.adminPage === Navigator.VisitLogs     ? "visitlogs"
                       : Navigator.adminPage === Navigator.Database      ? "database"
                       : Navigator.adminPage === Navigator.Reporting     ? "reporting"
                       : Navigator.adminPage === Navigator.Settings      ? "settings"
                       : Navigator.adminPage === Navigator.AccessControl ? "accesscontrol"
                       : "dashboard"
```

4. `LSideNav.items` — replace the array with:

```qml
            items: [
                { page: "dashboard",     label: qsTr("Dashboard"),      enabled: true },
                { page: "search",        label: qsTr("Search"),         enabled: true },
                { page: "visitlogs",     label: qsTr("Visit Logs"),     enabled: true },
                { page: "database",      label: qsTr("Database"),       enabled: true },
                { page: "reporting",     label: qsTr("Reporting"),      enabled: true },
                { page: "settings",      label: qsTr("Settings"),       enabled: true },
                { page: "accesscontrol", label: qsTr("Access Control"), enabled: true }
            ]
```

5. `onPageActivated` — add before the final `else console.warn(...)`:

```qml
                else if (page === "accesscontrol")
                    Navigator.showAdminPage(Navigator.AccessControl)
```

6. `pageLoader.sourceComponent` — add before `default:`:

```qml
                    case Navigator.AccessControl: return accessControlComponent;
```

…and add the component definition after the `settingsComponent` line at the bottom of the file:

```qml
    Component { id: accessControlComponent; AccessControlScreen { objectName: "accessControlPage"; vm: accessControlVm } }
```

(The Loader's existing feature-detected `onLoaded` → `item.vm.refresh()` provides auto-load-on-open, gated by `autoLoad`, so stub-VM and shell QuickTests stay offline. The screen itself never fetches.)

- [ ] **Step 5: Implement `AccessControlScreen.qml`**

Replace the skeleton with:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOAMS

// Access Control admin page (Sub-plan 4). Two data sources, deliberately split:
//  - `hub` (default: the app-global AccessControl singleton) — LIVE state:
//    the monitoring-intent toggle, the feed connection state and contact age.
//  - `vm` (page-scoped AccessControlViewModel) — the admin-authenticated
//    SNAPSHOT of access_recent.php: recent-entries table, counts, "Updated".
// Both are injectable so QuickTests drive plain-QML stubs. The initial fetch is
// AdminScreen's Loader.onLoaded feature-detected vm.refresh() (never here), so a
// directly-instantiated screen issues no network. Every color is a Theme token.
Rectangle {
    id: screen
    property var vm
    property var hub: AccessControl
    // Presentation clock for the contact-age tile (refinement 4): a plain
    // property so tests can pin "now"; the ageTimer below advances it.
    property var now: new Date()

    readonly property bool monitoringOn: hub ? hub.accessEnabled === true : false
    readonly property bool enableLocked: hub ? hub.enableLocked === true : false
    readonly property int connectionState: hub ? hub.connectionState : 0
    readonly property string connectionLabel: stateLabel(connectionState)
    // Stateless pure formatter on the singleton; the INPUTS come from `hub`
    // (possibly a stub) and the advancing presentation clock.
    readonly property string contactAgeText: hub
        ? AccessControl.contactAgeText(monitoringOn, hub.lastContactAt, now)
        : ""

    // ConnectionState ints (pinned by tst_accesscontrolhub): 0..4.
    function stateLabel(s) {
        switch (s) {
        case 1:  return qsTr("Connecting");
        case 2:  return qsTr("Connected");
        case 3:  return qsTr("Degraded");
        case 4:  return qsTr("Error");
        default: return qsTr("Disconnected");
        }
    }

    // Empty feed vs failed initial load vs auth loss are distinct states
    // (refinement 6) — never conflate them in the table's empty text.
    function tableEmptyText() {
        if (!vm)
            return qsTr("No entries yet");
        if (vm.authFailure)
            return qsTr("Admin authentication failed — re-enter via admin login.");
        if (vm.initialLoadFailed)
            return qsTr("Could not load the access feed. Use Refresh to retry.");
        if (vm.loading && vm.updatedAt === "")
            return qsTr("Loading…");
        return qsTr("No entries yet");
    }

    color: Theme.appBackground

    Timer {
        id: ageTimer
        objectName: "ageTimer"
        interval: 1000
        repeat: true
        running: screen.visible && screen.monitoringOn
        onTriggered: screen.now = new Date()
    }

    ColumnLayout {
        id: content
        objectName: "accessContent"
        anchors.fill: parent
        anchors.margins: Theme.spacing.xxl
        spacing: Theme.spacing.xl

        // --- Live: monitoring intent -------------------------------------
        LCard {
            id: monitoringCard
            objectName: "monitoringCard"
            Layout.fillWidth: true
            implicitHeight: monitoringColumn.implicitHeight + 2 * monitoringCard.padding

            ColumnLayout {
                id: monitoringColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: Theme.spacing.sm

                LCheckbox {
                    id: monitorToggle
                    objectName: "monitorToggle"
                    Layout.fillWidth: true
                    label: qsTr("Enable turnstile monitoring")
                    // The control never holds its own diverging state: it
                    // mirrors the hub's persisted INTENT.
                    checked: screen.monitoringOn
                    enabled: screen.hub ? !screen.enableLocked : false
                    onToggled: function(checked) {
                        if (screen.hub)
                            screen.hub.setAccessEnabled(checked)
                        // LCheckbox's MouseArea assigns `checked` imperatively,
                        // which destroys the binding above. Re-assert it so a
                        // refused/locked call snaps back to the hub's value and
                        // later hub changes keep flowing in.
                        monitorToggle.checked = Qt.binding(function() { return screen.monitoringOn })
                    }
                }
                Text {
                    objectName: "monitoringHelp"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    text: qsTr("Controls this app's turnstile event polling and kiosk display. It does not disable the physical gate or stop server-side attendance recording.")
                    color: Theme.mutedText
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.body
                }
                Text {
                    objectName: "lockedNote"
                    Layout.fillWidth: true
                    visible: screen.enableLocked
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    text: qsTr("Monitoring is forced on for this PC by the WITS_ACCESS_CONTROL environment setting, so it can't be turned off here.")
                    color: Theme.error
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.body
                }
            }
        }

        // --- Tiles: snapshot counts (vm) + live feed health (hub) --------
        GridLayout {
            Layout.fillWidth: true
            columns: screen.width < 900 ? 2 : 4
            columnSpacing: Theme.spacing.lg
            rowSpacing: Theme.spacing.lg
            LStatTile {
                objectName: "entriesTodayTile"
                Layout.fillWidth: true
                variant: "Hero"
                label: qsTr("Entries Today")
                value: screen.vm ? String(screen.vm.entriesToday) : "0"
            }
            LStatTile {
                objectName: "lastEntryTile"
                Layout.fillWidth: true
                label: qsTr("Last Entry")
                value: screen.vm && screen.vm.lastEntryAt !== "" ? screen.vm.lastEntryAt : "—"
            }
            LStatTile {
                objectName: "connectionTile"
                Layout.fillWidth: true
                label: qsTr("Feed Connection")
                value: screen.connectionLabel
                caption: qsTr("App ↔ backend feed, not gate hardware")
            }
            LStatTile {
                objectName: "contactTile"
                Layout.fillWidth: true
                label: qsTr("Feed Contact")
                value: screen.contactAgeText
                caption: qsTr("Live — independent of the table below")
            }
        }

        // --- Snapshot header: "Updated" is separate from live contact ----
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.md
            Text {
                text: qsTr("Recent entries")
                color: Theme.text
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.cardTitle
            }
            Text {
                objectName: "updatedLabel"
                textFormat: Text.PlainText
                text: screen.vm && screen.vm.updatedAt !== ""
                      ? qsTr("Updated %1").arg(screen.vm.updatedAt)
                      : qsTr("Not loaded yet")
                color: Theme.mutedText
                font.family: Theme.typography.sans
                font.pixelSize: Theme.typography.body
            }
            Rectangle {
                objectName: "staleBadge"
                visible: screen.vm ? screen.vm.stale === true : false
                radius: Theme.radius.pill
                color: Theme.errorSoft
                border.width: 1
                border.color: Theme.errorBorder
                implicitWidth: staleText.implicitWidth + 2 * Theme.spacing.sm
                implicitHeight: staleText.implicitHeight + Theme.spacing.xs
                Text {
                    id: staleText
                    anchors.centerIn: parent
                    text: qsTr("Stale — last refresh failed")
                    color: Theme.error
                    font.family: Theme.typography.sans
                    font.pixelSize: Theme.typography.eyebrow
                }
            }
            Item { Layout.fillWidth: true }
            LButton {
                objectName: "refreshButton"
                text: screen.vm && screen.vm.loading ? qsTr("Refreshing…") : qsTr("Refresh")
                enabled: screen.vm ? !screen.vm.loading : false
                onClicked: if (screen.vm) screen.vm.refresh()
            }
        }

        Text {
            objectName: "authError"
            Layout.fillWidth: true
            visible: screen.vm ? screen.vm.authFailure === true : false
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            text: qsTr("Admin authentication failed — re-enter via admin login.")
            color: Theme.error
            font.family: Theme.typography.sans
            font.pixelSize: Theme.typography.body
        }

        LTable {
            id: entriesTable
            objectName: "entriesTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: screen.vm ? screen.vm.entries : null
            emptyStateText: screen.tableEmptyText()
            columns: [
                { key: "createdAt",  title: qsTr("Time"),       weight: 1.4 },
                { key: "name",       title: qsTr("Name"),       weight: 2 },
                { key: "schoolId",   title: qsTr("School ID"),  weight: 1.2 },
                { key: "course",     title: qsTr("Course"),     weight: 1.2 },
                { key: "department", title: qsTr("Department"), weight: 1.2 },
                { key: "reader",     title: qsTr("Lane"),       weight: 0.6 },
                { key: "card",       title: qsTr("Card"),       weight: 1 }
            ]
        }
    }

    LToast {
        id: accessToast
        objectName: "accessToast"
        severity: "Error"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.xxl
    }

    // NOT a declarative `message: vm.errorText` binding — LToast's auto-dismiss
    // Timer sets message="" imperatively, which would permanently destroy such
    // a binding after the first toast (the trap documented in KioskScreen.qml /
    // DatabaseScreen.qml). Raise imperatively on every non-empty change.
    Connections {
        target: screen.vm ? screen.vm : null
        function onErrorTextChanged() {
            if (screen.vm.errorText !== "")
                accessToast.message = screen.vm.errorText
        }
    }
}
```

- [ ] **Step 6: Build everything + run the full suite (GREEN)**

```powershell
cmake -S qt-app -B C:/b/loams-sp4 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp4
ctest --test-dir C:/b/loams-sp4 --output-on-failure
```

Expected: clean build (no new warnings); **57/57** tests pass (54 baseline + `tst_accessentriesmodel` + `tst_accesscontrolviewmodel` + `tst_contactage`). In the `tst_qml_*` logs: every `AccessControlScreen::*` and `AdminScreenShell::*` case passes under all four QuickTest targets, and there is no `"AccessControl" is not available` QCRITICAL. `tst_appshell` stays warning-free.

- [ ] **Step 7: Manual GUI smoke (documented, run once)**

Run `C:/b/loams-sp4/quick/WITSQuick.exe` against the local backend and record results in the PR body:
1. Flag off (fresh settings, no env): admin → **Access Control** page opens, table auto-loads, contact tile reads "Monitoring off", toggle unchecked; kiosk unchanged.
2. Tick **Enable turnstile monitoring** → connection tile goes Connecting → Connected, contact age ticks "Last contact N s ago" every second; kiosk shows a live gate swipe.
3. Seed a synthetic `turnstile_events` row (a test card, no real student) → **Refresh** shows it with the right `Entries Today` / `Last Entry`, and "Updated HH:MM:SS" moves; an unregistered card shows "Unknown card".
4. Stop the web server, **Refresh** → rows stay, "Stale — last refresh failed" badge, toast; contact age keeps climbing (connection Degraded).
5. Untick → "Monitoring off"; relaunch → still off (persisted).
6. Relaunch with `WITS_ACCESS_CONTROL=1` → toggle checked + disabled, locked note shown.

- [ ] **Step 8: Commit** (via the `commit` skill) — `feat(accesscontrol): Access Control admin page, nav wiring and QuickTests`.

---

## Self-Review

**1. Spec coverage:**

| Spec section / refinement | Task(s) |
|---|---|
| Scope: single page = table + count tiles + live health tiles + runtime toggle, behind `accessControl.enabled` | T4, T5, T7 |
| Architecture: two data sources (singleton = live, VM = admin endpoint data) | T4 (VM), T5 (hub), T7 (`vm` + `hub` bindings) |
| Refinement 1 — two timestamps ("Updated" VM, frozen on failure; "Last contact" hub) | T4 (`updatedAt` + freeze test), T5 (`lastContactAt`), T7 (`updatedLabel` separate from `contactTile`) |
| Refinement 2 — `enableLocked` read-only switch + inline override note | T5 (`enableLocked_refusesDisable`), T7 (`test_lockedDisablesToggleAndShowsNote`) |
| Refinement 3 — per-poll freshness; provider emits only on validated success incl. `entry:null`; never on transport/non-2xx/malformed; late reply after disable ignored; service records | T1 (provider + service tests incl. `_data` rows and `turnstileLateResponseAfterDisableIsIgnored`); the pre-existing Connected-transition record is deliberately KEPT as a second service-owned source, and baseline-emits-`polled`-once is pinned (`baselineSuccessEmitsPolledOnce`, `connectedTransitionAndPolledAreBothServiceRecorded`) |
| Refinement 4 — contact age advances on a presentation timer; pure 3-state formatter | T6 (`formatContactAge` + `ageIncreasesWithNowForSameContact`), T7 (`ageTimer`, `test_contactAgeAdvancesWithoutNewEvents`) |
| Refinement 5 — intent ≠ outcome; idempotent; persist-first; refuse disable when locked; failed startup doesn't revert | T5 |
| Refinement 6 — stale keep-rows / 401 clear + `authFailure` (HTTP 401 authoritative even with an empty/non-JSON body; in-band message secondary) / empty vs failed-initial | T4 (VM tests incl. `refresh_http401WithEmptyOrMalformedBodyIsStillAuthFailure`), T7 (empty/failed/auth QML tests) |
| `AccessControlViewModel` ctor/NAM seam, `refresh()` POST body, props, generation guard, `applyRecent` seam | T4 |
| `AccessEntriesModel` roles + "Unknown card", text-only | T3 |
| `LoginParser::parseRecentFeed` + `RecentFeedResult` (strict row shape: `id`/`created_at`/`card`/`reader`/`student` types enforced) | T2 (`_malformedIsInvalid`, `_cardAndReaderAreStrict`) |
| Hub retained state on every `initialize()` path; Q_PROPERTYs + NOTIFY; relays filtered by providerId + changed `lastCommTime`; `connectionState` int order pinned | T5 |
| Screen: LCard + LCheckbox toggle (binding re-asserted), helper text, tiles, Updated + stale badge, LTable, Refresh, LToast, Theme-only colors | T7 |
| Nav + registration: enum appended; all six `AdminScreen.qml` touch points; CMake | T7 (+ T3/T4 CMake) |
| Testing bullets — VM / parser / model / hub / service multi-poll / presentation age / QML stub + null-VM | T4 / T2 / T3 / T5 / T1 / T6+T7 / T7 |
| Security — `admin_key` body-only; 401 clears; synthetic fixtures | T4 (`refresh_postsAdminKeyInBodyNotUrl`, 401 test), all fixtures synthetic |
| Backward compat — additive, flag default off, kiosk untouched | T5 (`disabledByDefault_inert` still green), T7 smoke |

**2. Placeholder scan:** No TBD/TODO/"similar to"/"add error handling". Every stub/skeleton is explicitly labelled and later replaced by full code shown in the plan; every test body is complete.

**3. Type consistency:**
- `IAccessProvider::polled(const QDateTime &)` (T1) — emitted by `TurnstileProvider::onFinished`, `MockProvider::simulatePolled`; consumed by the `AccessControlService::enable` lambda → `HealthMonitor::recordCommTime(QString, QDateTime)` (existing signature).
- `LoginParser::RecentEntry` / `RecentFeedResult` / `parseRecentFeed(QByteArray)` (T2) — consumed identically by `AccessEntriesModel::setEntries(QVector<RecentEntry>)` (T3) and `AccessControlViewModel::applyRecent` (T4).
- `AccessEntriesModel` role names `name/schoolId/course/department/createdAt/reader/card/known` (T3) = the `LTable` column keys and the QML stub `ListModel` fields (T7).
- VM property names `entriesToday/lastEntryAt/updatedAt/emptyFeed/initialLoadFailed/loading/stale/errorText/authFailure/entries` + `refresh()` (T4) = the QML screen reads and the `acVmStub` (T7).
- Hub `accessEnabled/enableLocked/connectionState/lastContactAt/setAccessEnabled` (T5) + `contactAgeText(bool, QVariant, QVariant)` (T6) = the screen reads and the `hubStub` (T7; the stub deliberately omits `contactAgeText` because the screen calls it on the real `AccessControl` singleton).
- `Navigator::AccessControl` (T7) used by `AdminScreen.qml`, `tst_navigator`, `tst_qml_adminshell.qml`.
- Test helpers used exactly as defined in `qt-app/testsupport`: `SequencedNam::{enqueue, enqueueStall, requestCount, lastUrl}`, `CapturingNam(canned, error, httpStatus)` + `{lastOp, lastUrl, lastContentType, lastBody}`.

**4. Test count:** 54 baseline → **57** (new ctests: `tst_accessentriesmodel`, `tst_accesscontrolviewmodel`, `tst_contactage`; all other additions are new functions inside existing tests or the auto-discovered `tst_qml_accesscontrol.qml`).

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-09-30-access-control-admin-page.md`. Two execution options:

1. **Subagent-Driven (recommended)** — a fresh subagent per task, two-stage review between tasks, fast iteration.
2. **Inline Execution** — execute tasks in this session with checkpoints.

Which approach?
