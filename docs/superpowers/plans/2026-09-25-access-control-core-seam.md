# Access Control Core Seam Implementation Plan

> For agentic workers: execute this plan with `superpowers:subagent-driven-development`. Each numbered Task is one independent, TDD-driven unit of work — dispatch one subagent per Task, in order. A worker sees only its own Task, so every Task's **Interfaces** block restates the exact C++ signatures of the neighbouring types it consumes or produces.

**Goal:** Build the hardware-agnostic, fully unit-testable Access Control *core seam* in `witscore` — value types, an in-process event bus, a provider interface + mock, a provider factory, an async decision service, a health monitor, and the lifecycle service that wires them — all behind an `accessControl.enabled` flag that defaults OFF so the seam is zero-behaviour-change until a later sub-plan turns it on.

**Architecture:** All new code is plain `witscore` C++ (no Widgets, no QML, no hardware). Adapters (`IAccessProvider` implementations) capture credentials internally and emit *decided* `AccessEvent`s; `AccessControlService` owns the active provider, republishes its events onto the `EventBus`, and tracks connection health via a `HealthMonitor`. The only network touch is `AccessDecisionService`, which follows the repo's injected-`QNetworkAccessManager` idiom so it is testable with `CapturingNam` and a synthetic hanging reply.

**Tech Stack:** Qt 6 (Core + Network) · C++17 · CMake · Qt Test (`QtTest`) driven by `ctest` · `wits_add_qttest()` from `qt-app/cmake/WitsTest.cmake` · `qt-app/testsupport/capturingnam.{h,cpp}` for network tests.

## Global Constraints

- **Qt 6 / C++17 only.** No Widgets, QML, or hardware SDK includes anywhere in `core/accesscontrol/` — the seam must compile and test with Core + Network alone.
- **QObject ownership:** every `QObject` lives in a parent-owned tree; no raw `new` escapes without a parent, and no manual `delete` on a parented `QObject` (use `deleteLater()` for replies, parent everything else to its owner).
- **Injected, not owned:** services take collaborators (`QNetworkAccessManager`, `EventBus`, `AccessProviderFactory`) by pointer and never take ownership of them.
- **Function-pointer `connect` syntax only** — `connect(obj, &T::sig, this, &U::slot)` or a lambda; never the `SIGNAL()/SLOT()` string macros.
- **Flag OFF by default:** `AccessControlService` starts disabled (`m_enabled == false`); with no `enable()` call the seam does nothing, so adding it changes no existing behaviour.
- **No live network in tests:** every network path is exercised through `CapturingNam` (immediate canned reply) or a test-local hanging `QNetworkAccessManager`; never a real `http://` call.
- **Decision ≠ Entry:** `AccessEvent::Type::EntryObserved` (confirmed physical entry) is a distinct type from `AccessEvent::Type::AccessGranted` (permission). The mock must emit each independently.
- **Timeout = Error, never Denied:** an `AccessDecisionService::verify()` that times out or hits a transport failure yields `AccessDecision::Result::Error`, never a fabricated `Denied`. `Denied` is reserved for a well-formed policy rejection from the backend.
- **Bus currency is DECIDED events:** a raw `Credential` stays internal to the adapter that captured it and is NEVER published on the `EventBus`. (This is enforced *structurally* — `EventBus::publish` only accepts `AccessEvent`, and no type on the bus carries a `Credential` — not by a runtime test.)
- **Single-thread affinity (v1):** the whole seam is single-threaded — `AccessControlService`, its active provider, the `EventBus`, and every subscriber live in the same thread, so provider signals are delivered by **direct** connection. A future cross-thread provider (e.g. a blocking USB SDK on a worker thread) must marshal its signals back to the service's thread; that is out of scope here. Every callback is nonetheless written to be safe against a late/stale delivery — guarded by `m_enabled`, a live-provider check, **and a source-provider check** (the callback compares the captured provider instance / `sender()` against the current `m_provider`) — so a queued event from a torn-down or superseded provider is dropped rather than causing a use-after-free or being misattributed to the new provider.
- **EventBus delivery is synchronous, same-thread, re-entrant:** `publish()` emits a direct signal, so subscribers run to completion on the caller's stack before `publish()` returns; a subscriber that itself calls `publish()` is delivered depth-first (nested) before the outer delivery resumes. Subscribers must not block. There is no internal queue in v1.
- **The `accessControl.enabled` flag seam:** in this sub-plan the flag is exposed only as `AccessControlService::isEnabled()` / `enable()` / `disable()`, and its invariant is "constructed disabled → nothing built, no bus traffic, until `enable()`." Persisting the flag to `QSettings` and auto-enabling the chosen provider at app startup is wired by the later admin/config sub-plan, not here.

---

## Path reconciliation (read once before Task 1)

The brief names the seam directory `qt-app/witscore/accesscontrol/`. In this repo the shared static library **target is `witscore`**, but its sources live in **`qt-app/core/`** (see `qt-app/core/CMakeLists.txt`, `add_library(witscore STATIC ...)`). There is no `qt-app/witscore/` directory. This plan therefore places the seam at **`qt-app/core/accesscontrol/`** and adds its sources to the existing `witscore` target. Wherever the brief said "witscore/accesscontrol", read "core/accesscontrol" — same library, real path.

## File Structure

```
qt-app/
  core/
    CMakeLists.txt                         # MODIFY (Task 8): add accesscontrol/*.{h,cpp} to the witscore target
    accesscontrol/
      accesstypes.h                        # NEW (T1): Credential, AccessDecision, AccessEvent, GateDescriptor,
                                           #           ProviderDescriptor, ConfigFieldDescriptor, ConnectionState + metatype decls
      accesstypes.cpp                      # NEW (T1): registerMetaTypes() body
      eventbus.h / eventbus.cpp            # NEW (T2): typed in-process publish/subscribe over a Qt signal
      iaccessprovider.h                    # NEW (T3): QObject provider interface (pure virtual)
      mockprovider.h / mockprovider.cpp    # NEW (T3): descriptor-driven simulator, one simulate* per event type
      accessproviderfactory.h / .cpp       # NEW (T4): register/available/create registry
      accessdecisionservice.h / .cpp       # NEW (T5): async verify() -> decided(AccessDecision); timeout = Error
      healthmonitor.h / healthmonitor.cpp  # NEW (T6): per-provider HealthSnapshot (state/lastComm/latency/retries)
      accesscontrolservice.h / .cpp        # NEW (T7): enable/disable lifecycle, provider->bus wiring, state machine + backoff
  tests/
    CMakeLists.txt                         # MODIFY (T1..T7): one wits_add_qttest() block per test below
    tst_accesstypes.cpp                    # NEW (T1)
    tst_eventbus.cpp                       # NEW (T2)
    tst_mockprovider.cpp                   # NEW (T3)
    tst_accessproviderfactory.cpp          # NEW (T4)
    tst_accessdecisionservice.cpp          # NEW (T5)
    tst_healthmonitor.cpp                  # NEW (T6)
    tst_accesscontrolservice.cpp           # NEW (T7)
```

**Build/test commands** (Qt 6.11 MinGW kit under `C:\Qt` is NOT on `PATH` — put the kit's `bin` on `PATH` and pass `-DCMAKE_PREFIX_PATH=<Qt>/lib/cmake` at configure time, per project docs):

- Configure / reconfigure (required after every `CMakeLists.txt` edit so a new target appears): `cmake -S qt-app -B qt-app/build`
- Build one target: `cmake --build qt-app/build --target <tst_name>`
- Run one test: `ctest --test-dir qt-app/build -R <tst_name> --output-on-failure`
- Full suite (Task 8): `ctest --test-dir qt-app/build --output-on-failure`

**TDD note for C++:** the RED step here is a genuine failure — the test target does not link (or a `static_assert`/`QCOMPARE` fails) because the type under test does not yet exist. That compile/link failure IS red. Add each `wits_add_qttest()` block in the same commit as its test file so the target exists to fail.

---

## Task 1 — Domain value types + metatype registration

**Files**
- Create: `qt-app/core/accesscontrol/accesstypes.h`, `qt-app/core/accesscontrol/accesstypes.cpp`
- Test: `qt-app/tests/tst_accesstypes.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (add the `tst_accesstypes` block)

**Interfaces**
- Produces (namespace `AccessControl`):
  - `enum class CredentialKind { Rfid, Qr, Nfc, Fingerprint, Face, Mobile, Pin };`
  - `enum class ConnectionState { Disconnected, Connecting, Connected, Degraded, Error };`
  - `struct Credential { CredentialKind kind; QString raw; QString gateId; QDateTime presentedAt; };`
  - `struct AccessDecision { enum class Result { Granted, Denied, Error }; Result result; QString subjectId; QString reason; QString correlationId; };`
  - `struct AccessEvent { enum class Type { AccessGranted, AccessDenied, AccessError, EntryObserved, ControllerConnected, ControllerDisconnected, HardwareError }; Type type; QJsonObject subject; QString gateId; QString providerId; CredentialKind credentialKind; QString reason; QString correlationId; QDateTime at; };` (`providerId` identifies the emitting provider/controller; `gateId` identifies a physical gate — a provider can serve several gates, so controller/hardware events set `providerId` and leave `gateId` for actual gate-scoped events like `EntryObserved`.)
  - `struct ConfigFieldDescriptor { QString key; QString displayName; QString type; bool required; };`
  - `struct GateDescriptor { QString gateId; QString displayName; };`
  - `struct ProviderDescriptor { QString providerId; QString displayName; QList<ConfigFieldDescriptor> configSchema; };`
  - `void registerMetaTypes();`
- Consumes: nothing (leaf).

**Steps**

- [ ] Write the failing test `qt-app/tests/tst_accesstypes.cpp`:
  ```cpp
  #include <QtTest>
  #include <QJsonObject>
  #include <QMetaType>
  #include "accesscontrol/accesstypes.h"

  using namespace AccessControl;

  // A tiny sender so the test can exercise an actual QUEUED signal carrying an
  // AccessEvent — the property the seam truly needs (an AccessEvent must survive
  // a cross-thread/queued hop). A plain isValid() check would be tautological:
  // Q_DECLARE_METATYPE alone makes QMetaType::fromType<T>() valid.
  class TypeEmitter : public QObject
  {
      Q_OBJECT
  signals:
      void fire(const AccessControl::AccessEvent &event);
  };

  class TestAccessTypes : public QObject
  {
      Q_OBJECT
  private slots:
      void credentialHoldsRawAndKind();
      void decisionDefaultsToError();
      void entryObservedIsDistinctFromGranted();
      void providerDescriptorCarriesConfigSchema();
      void accessEventSurvivesQueuedConnection();
  };

  void TestAccessTypes::credentialHoldsRawAndKind()
  {
      Credential c;
      c.kind = CredentialKind::Qr;
      c.raw = QStringLiteral("TOKEN-123");
      c.gateId = QStringLiteral("gate-a");
      QCOMPARE(c.kind, CredentialKind::Qr);
      QCOMPARE(c.raw, QStringLiteral("TOKEN-123"));
      QCOMPARE(c.gateId, QStringLiteral("gate-a"));
  }

  void TestAccessTypes::decisionDefaultsToError()
  {
      // A default-constructed decision must NOT read as Granted or Denied —
      // absence of a real answer is an Error, never an accidental allow.
      AccessDecision d;
      QCOMPARE(d.result, AccessDecision::Result::Error);
  }

  void TestAccessTypes::entryObservedIsDistinctFromGranted()
  {
      QVERIFY(AccessEvent::Type::EntryObserved != AccessEvent::Type::AccessGranted);
  }

  void TestAccessTypes::providerDescriptorCarriesConfigSchema()
  {
      ProviderDescriptor pd;
      pd.providerId = QStringLiteral("mock");
      pd.displayName = QStringLiteral("Mock Provider");
      pd.configSchema.append(ConfigFieldDescriptor{
          QStringLiteral("baseUrl"), QStringLiteral("Base URL"),
          QStringLiteral("string"), true });
      QCOMPARE(pd.configSchema.size(), 1);
      QCOMPARE(pd.configSchema.first().key, QStringLiteral("baseUrl"));
      QVERIFY(pd.configSchema.first().required);
  }

  void TestAccessTypes::accessEventSurvivesQueuedConnection()
  {
      // registerMetaTypes() runs automatically at library load (Q_CONSTRUCTOR_FUNCTION
      // in accesstypes.cpp); call it again to prove idempotency.
      registerMetaTypes();
      registerMetaTypes();

      TypeEmitter em;
      AccessEvent received;
      bool got = false;
      // Qt::QueuedConnection forces the payload through the metatype system: if
      // AccessEvent were not a usable queued metatype this delivery would be
      // dropped and `got` would stay false.
      connect(&em, &TypeEmitter::fire, this,
              [&](const AccessEvent &e) { received = e; got = true; },
              Qt::QueuedConnection);

      AccessEvent e;
      e.type = AccessEvent::Type::EntryObserved;
      e.gateId = QStringLiteral("gate-a");
      e.correlationId = QStringLiteral("corr-q");
      emit em.fire(e);

      QVERIFY(QTest::qWaitFor([&]() { return got; }, 1000));
      QCOMPARE(received.type, AccessEvent::Type::EntryObserved);
      QCOMPARE(received.gateId, QStringLiteral("gate-a"));
      QCOMPARE(received.correlationId, QStringLiteral("corr-q"));
  }

  QTEST_MAIN(TestAccessTypes)
  #include "tst_accesstypes.moc"
  ```
  > Note: `TypeEmitter` and `TestAccessTypes` are both moc'd via the trailing
  > `#include "tst_accesstypes.moc"`. Qt 6 also auto-registers a `Q_DECLARE_METATYPE`
  > type on first queued use, so this test guards the behaviour we depend on rather
  > than the mechanism; `registerMetaTypes()` remains an explicit, name-lookup-safe
  > registration for any code path that resolves these types by name before a
  > connection exists.
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append):
  ```cmake
  # --- Access Control: domain value types (pure core, no offscreen) ---
  wits_add_qttest(tst_accesstypes
      SOURCES
          tst_accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_accesstypes` — build fails with `fatal error: accesscontrol/accesstypes.h: No such file or directory` (RED: the header does not exist yet).
- [ ] Write the minimal implementation `qt-app/core/accesscontrol/accesstypes.h`:
  ```cpp
  #ifndef ACCESSCONTROL_ACCESSTYPES_H
  #define ACCESSCONTROL_ACCESSTYPES_H

  #include <QDateTime>
  #include <QJsonObject>
  #include <QList>
  #include <QMetaType>
  #include <QString>

  namespace AccessControl {

  enum class CredentialKind { Rfid, Qr, Nfc, Fingerprint, Face, Mobile, Pin };

  enum class ConnectionState { Disconnected, Connecting, Connected, Degraded, Error };

  // Raw presented credential. Captured and used ONLY inside an adapter; never
  // published on the EventBus (bus currency is decided events).
  struct Credential {
      CredentialKind kind = CredentialKind::Rfid;
      QString raw;
      QString gateId;
      QDateTime presentedAt;
  };

  // Outcome of verifying a Credential. Error (infra/timeout/unreachable) is
  // deliberately distinct from Denied (a real policy rejection).
  struct AccessDecision {
      enum class Result { Granted, Denied, Error };
      Result result = Result::Error;   // safe default: no answer == Error, not allow
      QString subjectId;
      QString reason;
      QString correlationId;
  };

  // A decided, publishable event. EntryObserved (confirmed physical entry) is a
  // separate type from AccessGranted (permission), on purpose.
  struct AccessEvent {
      enum class Type {
          AccessGranted, AccessDenied, AccessError, EntryObserved,
          ControllerConnected, ControllerDisconnected, HardwareError
      };
      Type type = Type::AccessError;
      QJsonObject subject;   // resolved subject/student JSON (empty until a later sub-plan resolves it)
      QString gateId;        // physical gate id — set for gate-scoped events (e.g. EntryObserved)
      QString providerId;    // emitting provider/controller id — set for Controller*/HardwareError
      CredentialKind credentialKind = CredentialKind::Rfid;
      QString reason;
      QString correlationId;
      QDateTime at;
  };

  // One configurable field a provider needs; drives the future admin config form.
  struct ConfigFieldDescriptor {
      QString key;
      QString displayName;
      QString type;          // "string" | "int" | "bool" | "secret"
      bool required = false;
  };

  struct GateDescriptor {
      QString gateId;
      QString displayName;
  };

  struct ProviderDescriptor {
      QString providerId;
      QString displayName;
      QList<ConfigFieldDescriptor> configSchema;
  };

  // Registers every value type above (and ConnectionState) as a Qt metatype so
  // it survives a queued signal/slot hop and is capturable by QSignalSpy.
  // Idempotent — safe to call more than once (main.cpp and each test init).
  void registerMetaTypes();

  } // namespace AccessControl

  Q_DECLARE_METATYPE(AccessControl::Credential)
  Q_DECLARE_METATYPE(AccessControl::AccessDecision)
  Q_DECLARE_METATYPE(AccessControl::AccessEvent)
  Q_DECLARE_METATYPE(AccessControl::GateDescriptor)
  Q_DECLARE_METATYPE(AccessControl::ProviderDescriptor)
  Q_DECLARE_METATYPE(AccessControl::ConnectionState)

  #endif // ACCESSCONTROL_ACCESSTYPES_H
  ```
- [ ] Write `qt-app/core/accesscontrol/accesstypes.cpp`:
  ```cpp
  #include "accesscontrol/accesstypes.h"

  namespace AccessControl {

  void registerMetaTypes()
  {
      // qRegisterMetaType is idempotent, so this is safe to call repeatedly.
      qRegisterMetaType<Credential>();
      qRegisterMetaType<AccessDecision>();
      qRegisterMetaType<AccessEvent>();
      qRegisterMetaType<GateDescriptor>();
      qRegisterMetaType<ProviderDescriptor>();
      qRegisterMetaType<ConnectionState>();
  }

  } // namespace AccessControl

  // Register automatically when witscore loads, so the types are name-resolvable
  // before any consumer sets up a connection — no reliance on a caller remembering
  // to call registerMetaTypes(). Q_CONSTRUCTOR_FUNCTION must sit at file scope,
  // outside the namespace, and takes a free function.
  namespace { void accesscontrolRegisterMetaTypes() { AccessControl::registerMetaTypes(); } }
  Q_CONSTRUCTOR_FUNCTION(accesscontrolRegisterMetaTypes)
  ```
  > Note: the `.cpp` includes its header as `"accesscontrol/accesstypes.h"`; the `witscore` target and the test both have `core/` on the include path, so this resolves in both. `Q_CONSTRUCTOR_FUNCTION` (from `<QtGlobal>`, pulled in transitively) runs `accesscontrolRegisterMetaTypes()` during static init of the translation unit.
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_accesstypes && ctest --test-dir qt-app/build -R tst_accesstypes --output-on-failure` — 5 slots pass.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/accesstypes.h qt-app/core/accesscontrol/accesstypes.cpp qt-app/tests/tst_accesstypes.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add access-control domain value types

  Introduce the hardware-agnostic value types for the access-control core
  seam: Credential, AccessDecision (Granted/Denied/Error), AccessEvent
  (with EntryObserved distinct from AccessGranted), and the provider/gate/
  config descriptors, plus registerMetaTypes() so they cross queued signals.
  Result defaults to Error so a missing answer never reads as an allow."
  ```

---

## Task 2 — EventBus (typed in-process publish/subscribe)

**Files**
- Create: `qt-app/core/accesscontrol/eventbus.h`, `qt-app/core/accesscontrol/eventbus.cpp`
- Test: `qt-app/tests/tst_eventbus.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces**
- Consumes: `AccessControl::AccessEvent`, `AccessControl::registerMetaTypes()` (Task 1).
- Produces: `class AccessControl::EventBus : public QObject` with
  - `explicit EventBus(QObject *parent = nullptr);`
  - `void publish(const AccessEvent &event);`
  - `signals: void eventPublished(const AccessControl::AccessEvent &event);`
  - Subscription = connecting to `eventPublished`; the typed signal *is* the subscription mechanism.

**Steps**

- [ ] Write the failing test `qt-app/tests/tst_eventbus.cpp`:
  ```cpp
  #include <QtTest>
  #include <QSignalSpy>
  #include "accesscontrol/accesstypes.h"
  #include "accesscontrol/eventbus.h"

  using namespace AccessControl;

  class TestEventBus : public QObject
  {
      Q_OBJECT
  private slots:
      void initTestCase() { registerMetaTypes(); }
      void publishReachesSubscriber();
      void publishCarriesEventPayload();
      void nestedPublishFromSubscriberIsDeliveredDepthFirst();
  };

  void TestEventBus::publishReachesSubscriber()
  {
      EventBus bus;
      QSignalSpy spy(&bus, &EventBus::eventPublished);
      AccessEvent e;
      e.type = AccessEvent::Type::EntryObserved;
      bus.publish(e);
      QCOMPARE(spy.count(), 1);
  }

  void TestEventBus::publishCarriesEventPayload()
  {
      EventBus bus;
      QSignalSpy spy(&bus, &EventBus::eventPublished);
      AccessEvent e;
      e.type = AccessEvent::Type::AccessGranted;
      e.gateId = QStringLiteral("gate-9");
      e.correlationId = QStringLiteral("corr-1");
      bus.publish(e);
      QCOMPARE(spy.count(), 1);
      const auto received = qvariant_cast<AccessEvent>(spy.at(0).at(0));
      QCOMPARE(received.type, AccessEvent::Type::AccessGranted);
      QCOMPARE(received.gateId, QStringLiteral("gate-9"));
      QCOMPARE(received.correlationId, QStringLiteral("corr-1"));
  }

  void TestEventBus::nestedPublishFromSubscriberIsDeliveredDepthFirst()
  {
      EventBus bus;
      QSignalSpy spy(&bus, &EventBus::eventPublished);   // connected first
      QList<AccessEvent::Type> order;
      bool nested = false;
      // A subscriber that republishes a nested event once, on an AccessGranted.
      connect(&bus, &EventBus::eventPublished, this, [&](const AccessEvent &e) {
          order.append(e.type);
          if (e.type == AccessEvent::Type::AccessGranted && !nested) {
              nested = true;
              AccessEvent inner;
              inner.type = AccessEvent::Type::EntryObserved;
              bus.publish(inner);   // re-entrant publish
          }
      });

      AccessEvent outer;
      outer.type = AccessEvent::Type::AccessGranted;
      bus.publish(outer);

      QCOMPARE(spy.count(), 2);   // both the outer and the nested event were delivered
      // Depth-first: the nested EntryObserved completes before the outer resumes.
      QCOMPARE(order.size(), 2);
      QCOMPARE(order.at(0), AccessEvent::Type::AccessGranted);
      QCOMPARE(order.at(1), AccessEvent::Type::EntryObserved);
  }

  QTEST_MAIN(TestEventBus)
  #include "tst_eventbus.moc"
  ```
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append):
  ```cmake
  # --- Access Control: in-process event bus (pure core, no offscreen) ---
  wits_add_qttest(tst_eventbus
      SOURCES
          tst_eventbus.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/eventbus.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/eventbus.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_eventbus` — build fails: `accesscontrol/eventbus.h: No such file or directory` (RED).
- [ ] Write `qt-app/core/accesscontrol/eventbus.h`:
  ```cpp
  #ifndef ACCESSCONTROL_EVENTBUS_H
  #define ACCESSCONTROL_EVENTBUS_H

  #include <QObject>
  #include "accesscontrol/accesstypes.h"

  namespace AccessControl {

  // Typed, in-process publish/subscribe over a single Qt signal. The bus's
  // currency is DECIDED AccessEvents; a raw Credential is never published here.
  // A subscriber "subscribes" by connecting to eventPublished().
  //
  // Delivery contract (v1): publish() emits a DIRECT signal, so delivery is
  // synchronous and on the CALLER'S thread — every subscriber runs to completion
  // before publish() returns. The bus is single-threaded (see Global Constraints);
  // a cross-thread publisher must marshal onto the bus's thread. Re-entrant
  // (nested) publish() from inside a subscriber IS supported and delivered
  // depth-first: the inner event reaches all subscribers before the outer
  // delivery resumes. Subscribers must therefore not block. No internal queue.
  class EventBus : public QObject
  {
      Q_OBJECT
  public:
      explicit EventBus(QObject *parent = nullptr);
      void publish(const AccessEvent &event);

  signals:
      void eventPublished(const AccessControl::AccessEvent &event);
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_EVENTBUS_H
  ```
- [ ] Write `qt-app/core/accesscontrol/eventbus.cpp`:
  ```cpp
  #include "accesscontrol/eventbus.h"

  namespace AccessControl {

  EventBus::EventBus(QObject *parent)
      : QObject(parent)
  {
      // Guarantee the access-control value types are registered the moment a bus
      // exists. This is the seam's self-contained registration point: because the
      // bus is central to every access-control flow, any binary that constructs one
      // references this translation unit, so accesstypes.cpp is force-linked and
      // registration can never be elided from the static library. (registerMetaTypes
      // is idempotent; the Q_CONSTRUCTOR_FUNCTION in accesstypes.cpp is a backstop.)
      registerMetaTypes();
  }

  void EventBus::publish(const AccessEvent &event)
  {
      emit eventPublished(event);
  }

  } // namespace AccessControl
  ```
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_eventbus && ctest --test-dir qt-app/build -R tst_eventbus --output-on-failure` — 3 slots pass.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/eventbus.h qt-app/core/accesscontrol/eventbus.cpp qt-app/tests/tst_eventbus.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add typed in-process EventBus

  A QObject bus that publishes decided AccessEvents over one typed Qt signal;
  subscribers connect to eventPublished(). Keeps raw credential capture off
  the bus by construction — only decided events flow through publish()."
  ```

---

## Task 3 — IAccessProvider interface + MockProvider

**Files**
- Create: `qt-app/core/accesscontrol/iaccessprovider.h`, `qt-app/core/accesscontrol/mockprovider.h`, `qt-app/core/accesscontrol/mockprovider.cpp`
- Test: `qt-app/tests/tst_mockprovider.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces**
- Consumes: `AccessEvent`, `ProviderDescriptor`, `ConnectionState`, `ConfigFieldDescriptor` (Task 1).
- Produces:
  - `class AccessControl::IAccessProvider : public QObject` (abstract):
    - `explicit IAccessProvider(QObject *parent = nullptr);`
    - `virtual ProviderDescriptor descriptor() const = 0;`
    - `virtual void start() = 0;`
    - `virtual void stop() = 0;`
    - `virtual ConnectionState state() const = 0;`
    - `signals: void accessEvent(const AccessControl::AccessEvent&); void stateChanged(AccessControl::ConnectionState); void hardwareError(const QString&);`
  - `class AccessControl::MockProvider : public IAccessProvider`:
    - `explicit MockProvider(ProviderDescriptor descriptor, QObject *parent = nullptr);`
    - overrides of the four pure virtuals
    - `void simulateGranted(const QString &subjectId, const QString &gateId);`
    - `void simulateDenied(const QString &subjectId, const QString &reason);`
    - `void simulateError(const QString &reason);`
    - `void simulateEntryObserved(const QString &gateId);`
    - `void simulateHardwareError(const QString &message);`
    - `void simulateDisconnect();`
    - `static ProviderDescriptor defaultDescriptor();`
- Produced for later sub-plans: the real `TurnstileProvider` will implement this same `IAccessProvider` interface (consumed by a later sub-plan; not in scope here).

**Steps**

- [ ] Write the failing test `qt-app/tests/tst_mockprovider.cpp`:
  ```cpp
  #include <QtTest>
  #include <QSignalSpy>
  #include "accesscontrol/accesstypes.h"
  #include "accesscontrol/mockprovider.h"

  using namespace AccessControl;

  class TestMockProvider : public QObject
  {
      Q_OBJECT
  private slots:
      void initTestCase() { registerMetaTypes(); }
      void startsIntoConnectedState();
      void startIsIdempotentWhenConnected();
      void stopReturnsToDisconnected();
      void simulateEmitsEachEventTypeIndependently();
      void simulateHardwareErrorEmitsSignal();
      void descriptorRoundTrips();
  };

  void TestMockProvider::startsIntoConnectedState()
  {
      MockProvider p(MockProvider::defaultDescriptor());
      QSignalSpy states(&p, &IAccessProvider::stateChanged);
      QCOMPARE(p.state(), ConnectionState::Disconnected);
      p.start();
      QCOMPARE(p.state(), ConnectionState::Connected);
      // Connecting then Connected — two transitions.
      QCOMPARE(states.count(), 2);
      QCOMPARE(qvariant_cast<ConnectionState>(states.at(1).at(0)),
               ConnectionState::Connected);
  }

  void TestMockProvider::startIsIdempotentWhenConnected()
  {
      MockProvider p(MockProvider::defaultDescriptor());
      p.start();
      QCOMPARE(p.state(), ConnectionState::Connected);
      QSignalSpy states(&p, &IAccessProvider::stateChanged);
      p.start();   // already connected — must be a no-op
      QCOMPARE(states.count(), 0);
      QCOMPARE(p.state(), ConnectionState::Connected);
  }

  void TestMockProvider::stopReturnsToDisconnected()
  {
      MockProvider p(MockProvider::defaultDescriptor());
      p.start();
      p.stop();
      QCOMPARE(p.state(), ConnectionState::Disconnected);
  }

  void TestMockProvider::simulateEmitsEachEventTypeIndependently()
  {
      MockProvider p(MockProvider::defaultDescriptor());
      QSignalSpy events(&p, &IAccessProvider::accessEvent);
      p.simulateGranted(QStringLiteral("S-1"), QStringLiteral("gate-a"));
      p.simulateDenied(QStringLiteral("S-2"), QStringLiteral("expired"));
      p.simulateError(QStringLiteral("reader offline"));
      p.simulateEntryObserved(QStringLiteral("gate-a"));
      QCOMPARE(events.count(), 4);
      QCOMPARE(qvariant_cast<AccessEvent>(events.at(0).at(0)).type,
               AccessEvent::Type::AccessGranted);
      QCOMPARE(qvariant_cast<AccessEvent>(events.at(1).at(0)).type,
               AccessEvent::Type::AccessDenied);
      QCOMPARE(qvariant_cast<AccessEvent>(events.at(2).at(0)).type,
               AccessEvent::Type::AccessError);
      // EntryObserved is a DISTINCT type from AccessGranted.
      const auto entry = qvariant_cast<AccessEvent>(events.at(3).at(0));
      QCOMPARE(entry.type, AccessEvent::Type::EntryObserved);
      QCOMPARE(entry.gateId, QStringLiteral("gate-a"));
  }

  void TestMockProvider::simulateHardwareErrorEmitsSignal()
  {
      MockProvider p(MockProvider::defaultDescriptor());
      QSignalSpy hw(&p, &IAccessProvider::hardwareError);
      p.simulateHardwareError(QStringLiteral("USB unplugged"));
      QCOMPARE(hw.count(), 1);
      QCOMPARE(hw.at(0).at(0).toString(), QStringLiteral("USB unplugged"));
  }

  void TestMockProvider::descriptorRoundTrips()
  {
      const ProviderDescriptor d = MockProvider::defaultDescriptor();
      MockProvider p(d);
      QCOMPARE(p.descriptor().providerId, d.providerId);
      QCOMPARE(p.descriptor().providerId, QStringLiteral("mock"));
  }

  QTEST_MAIN(TestMockProvider)
  #include "tst_mockprovider.moc"
  ```
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append):
  ```cmake
  # --- Access Control: mock provider + interface (pure core, no offscreen) ---
  wits_add_qttest(tst_mockprovider
      SOURCES
          tst_mockprovider.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/mockprovider.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/mockprovider.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/iaccessprovider.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_mockprovider` — build fails: `accesscontrol/mockprovider.h: No such file or directory` (RED).
- [ ] Write `qt-app/core/accesscontrol/iaccessprovider.h`:
  ```cpp
  #ifndef ACCESSCONTROL_IACCESSPROVIDER_H
  #define ACCESSCONTROL_IACCESSPROVIDER_H

  #include <QObject>
  #include <QString>
  #include "accesscontrol/accesstypes.h"

  namespace AccessControl {

  // QObject interface every access-control adapter implements. It is a QObject
  // (not a pure C++ interface) because it must emit signals; concrete providers
  // subclass it. A provider captures credentials internally and emits DECIDED
  // AccessEvents — it never hands a raw Credential to its owner.
  class IAccessProvider : public QObject
  {
      Q_OBJECT
  public:
      explicit IAccessProvider(QObject *parent = nullptr) : QObject(parent) {}
      ~IAccessProvider() override = default;

      virtual ProviderDescriptor descriptor() const = 0;
      virtual void start() = 0;
      virtual void stop() = 0;
      virtual ConnectionState state() const = 0;

  signals:
      void accessEvent(const AccessControl::AccessEvent &event);
      void stateChanged(AccessControl::ConnectionState state);
      void hardwareError(const QString &message);
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_IACCESSPROVIDER_H
  ```
- [ ] Write `qt-app/core/accesscontrol/mockprovider.h`:
  ```cpp
  #ifndef ACCESSCONTROL_MOCKPROVIDER_H
  #define ACCESSCONTROL_MOCKPROVIDER_H

  #include "accesscontrol/iaccessprovider.h"

  namespace AccessControl {

  // Descriptor-driven simulator implementing IAccessProvider with NO hardware.
  // Each simulate*() fires one synthetic event so every downstream path is
  // testable hardware-free. This is the engine behind all seam tests and the
  // future "demo mode".
  class MockProvider : public IAccessProvider
  {
      Q_OBJECT
  public:
      explicit MockProvider(ProviderDescriptor descriptor, QObject *parent = nullptr);

      ProviderDescriptor descriptor() const override;
      void start() override;
      void stop() override;
      ConnectionState state() const override;

      void simulateGranted(const QString &subjectId, const QString &gateId);
      void simulateDenied(const QString &subjectId, const QString &reason);
      void simulateError(const QString &reason);
      void simulateEntryObserved(const QString &gateId);
      void simulateHardwareError(const QString &message);
      void simulateDisconnect();

      static ProviderDescriptor defaultDescriptor();

  private:
      void setState(ConnectionState next);

      ProviderDescriptor m_descriptor;
      ConnectionState m_state = ConnectionState::Disconnected;
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_MOCKPROVIDER_H
  ```
- [ ] Write `qt-app/core/accesscontrol/mockprovider.cpp`:
  ```cpp
  #include "accesscontrol/mockprovider.h"

  #include <QDateTime>
  #include <QJsonObject>

  namespace AccessControl {

  MockProvider::MockProvider(ProviderDescriptor descriptor, QObject *parent)
      : IAccessProvider(parent)
      , m_descriptor(std::move(descriptor))
  {}

  ProviderDescriptor MockProvider::descriptor() const { return m_descriptor; }

  ConnectionState MockProvider::state() const { return m_state; }

  void MockProvider::setState(ConnectionState next)
  {
      if (m_state == next)
          return;
      m_state = next;
      emit stateChanged(m_state);
  }

  void MockProvider::start()
  {
      if (m_state == ConnectionState::Connected)
          return;   // idempotent: starting an already-connected provider is a no-op
      setState(ConnectionState::Connecting);
      setState(ConnectionState::Connected);
  }

  void MockProvider::stop()
  {
      setState(ConnectionState::Disconnected);
  }

  void MockProvider::simulateGranted(const QString &subjectId, const QString &gateId)
  {
      AccessEvent e;
      e.type = AccessEvent::Type::AccessGranted;
      e.subject = QJsonObject{ { QStringLiteral("subjectId"), subjectId } };
      e.gateId = gateId;
      e.credentialKind = CredentialKind::Rfid;
      e.at = QDateTime::currentDateTimeUtc();
      emit accessEvent(e);
  }

  void MockProvider::simulateDenied(const QString &subjectId, const QString &reason)
  {
      AccessEvent e;
      e.type = AccessEvent::Type::AccessDenied;
      e.subject = QJsonObject{ { QStringLiteral("subjectId"), subjectId } };
      e.reason = reason;
      e.at = QDateTime::currentDateTimeUtc();
      emit accessEvent(e);
  }

  void MockProvider::simulateError(const QString &reason)
  {
      AccessEvent e;
      e.type = AccessEvent::Type::AccessError;
      e.reason = reason;
      e.at = QDateTime::currentDateTimeUtc();
      emit accessEvent(e);
  }

  void MockProvider::simulateEntryObserved(const QString &gateId)
  {
      AccessEvent e;
      e.type = AccessEvent::Type::EntryObserved;   // distinct from AccessGranted
      e.gateId = gateId;
      e.at = QDateTime::currentDateTimeUtc();
      emit accessEvent(e);
  }

  void MockProvider::simulateHardwareError(const QString &message)
  {
      setState(ConnectionState::Error);
      emit hardwareError(message);
  }

  void MockProvider::simulateDisconnect()
  {
      setState(ConnectionState::Degraded);
  }

  ProviderDescriptor MockProvider::defaultDescriptor()
  {
      ProviderDescriptor d;
      d.providerId = QStringLiteral("mock");
      d.displayName = QStringLiteral("Mock Provider");
      // No config fields — the mock needs no external configuration.
      return d;
  }

  } // namespace AccessControl
  ```
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_mockprovider && ctest --test-dir qt-app/build -R tst_mockprovider --output-on-failure` — 6 slots pass.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/iaccessprovider.h qt-app/core/accesscontrol/mockprovider.h qt-app/core/accesscontrol/mockprovider.cpp qt-app/tests/tst_mockprovider.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add IAccessProvider interface and MockProvider

  IAccessProvider is the QObject seam every adapter implements (descriptor/
  start/stop/state + accessEvent/stateChanged/hardwareError). MockProvider is
  a hardware-free simulator that fires each event type on demand, including
  EntryObserved independently of AccessGranted, so the whole seam is testable
  with no reader present."
  ```

---

## Task 4 — AccessProviderFactory (register / available / create)

**Files**
- Create: `qt-app/core/accesscontrol/accessproviderfactory.h`, `qt-app/core/accesscontrol/accessproviderfactory.cpp`
- Test: `qt-app/tests/tst_accessproviderfactory.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces**
- Consumes: `ProviderDescriptor`, `IAccessProvider`, `MockProvider` (Tasks 1, 3).
- Produces: `class AccessControl::AccessProviderFactory` (plain class, not a QObject):
  - `using CreatorFn = std::function<IAccessProvider*(const ProviderDescriptor&, const QVariantMap&, QObject*)>;`
  - `void registerProvider(const ProviderDescriptor &descriptor, CreatorFn creator);`
  - `QList<ProviderDescriptor> available() const;`
  - `IAccessProvider *create(const ProviderDescriptor &descriptor, const QVariantMap &config, QObject *parent) const;` (**`parent` is required, non-null** — ownership is mandatory)
  - `create()` returns `nullptr` for an unknown id **or a null `parent`**; otherwise it dispatches on `descriptor.providerId`, builds the provider, and guarantees it is parented to `parent` (reparenting it if the creator ignored the argument) so no provider ever escapes unowned.

**Steps**

- [ ] Write the failing test `qt-app/tests/tst_accessproviderfactory.cpp`:
  ```cpp
  #include <QtTest>
  #include <QVariantMap>
  #include "accesscontrol/accesstypes.h"
  #include "accesscontrol/accessproviderfactory.h"
  #include "accesscontrol/mockprovider.h"

  using namespace AccessControl;

  class TestAccessProviderFactory : public QObject
  {
      Q_OBJECT
  private slots:
      void availableListsRegisteredDescriptors();
      void createBuildsRegisteredProvider();
      void createUnknownIdReturnsNull();
      void createWithNullParentReturnsNull();
      void createParentsProviderToOwner();
      void createEnforcesParentWhenCreatorIgnoresIt();
  };

  static AccessProviderFactory makeFactoryWithMock()
  {
      AccessProviderFactory f;
      f.registerProvider(MockProvider::defaultDescriptor(),
          [](const ProviderDescriptor &d, const QVariantMap &, QObject *parent) -> IAccessProvider * {
              return new MockProvider(d, parent);
          });
      return f;
  }

  void TestAccessProviderFactory::availableListsRegisteredDescriptors()
  {
      const AccessProviderFactory f = makeFactoryWithMock();
      const QList<ProviderDescriptor> list = f.available();
      QCOMPARE(list.size(), 1);
      QCOMPARE(list.first().providerId, QStringLiteral("mock"));
  }

  void TestAccessProviderFactory::createBuildsRegisteredProvider()
  {
      const AccessProviderFactory f = makeFactoryWithMock();
      QObject owner;
      IAccessProvider *p = f.create(MockProvider::defaultDescriptor(), {}, &owner);
      QVERIFY(p != nullptr);
      QCOMPARE(p->descriptor().providerId, QStringLiteral("mock"));
  }

  void TestAccessProviderFactory::createUnknownIdReturnsNull()
  {
      const AccessProviderFactory f = makeFactoryWithMock();
      QObject owner;
      ProviderDescriptor unknown;
      unknown.providerId = QStringLiteral("does-not-exist");
      QVERIFY(f.create(unknown, {}, &owner) == nullptr);
  }

  void TestAccessProviderFactory::createWithNullParentReturnsNull()
  {
      // Ownership is mandatory: a null parent must yield nullptr, never an
      // unowned provider, even for a registered id.
      const AccessProviderFactory f = makeFactoryWithMock();
      QVERIFY(f.create(MockProvider::defaultDescriptor(), {}, nullptr) == nullptr);
  }

  void TestAccessProviderFactory::createParentsProviderToOwner()
  {
      const AccessProviderFactory f = makeFactoryWithMock();
      auto *owner = new QObject;
      IAccessProvider *p = f.create(MockProvider::defaultDescriptor(), {}, owner);
      QVERIFY(p != nullptr);
      QCOMPARE(p->parent(), owner);
      delete owner;   // deleting the parent must delete the provider — no leak
  }

  void TestAccessProviderFactory::createEnforcesParentWhenCreatorIgnoresIt()
  {
      // A misbehaving creator that ignores the parent argument must NOT be able to
      // leave the provider unowned — the factory reparents the result.
      AccessProviderFactory f;
      f.registerProvider(MockProvider::defaultDescriptor(),
          [](const ProviderDescriptor &d, const QVariantMap &, QObject *) -> IAccessProvider * {
              return new MockProvider(d, nullptr);   // forgets to pass the parent
          });
      auto *owner = new QObject;
      IAccessProvider *p = f.create(MockProvider::defaultDescriptor(), {}, owner);
      QVERIFY(p != nullptr);
      QCOMPARE(p->parent(), owner);   // factory enforced ownership anyway
      delete owner;                    // no leak
  }

  QTEST_MAIN(TestAccessProviderFactory)
  #include "tst_accessproviderfactory.moc"
  ```
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append):
  ```cmake
  # --- Access Control: provider factory (pure core, no offscreen) ---
  wits_add_qttest(tst_accessproviderfactory
      SOURCES
          tst_accessproviderfactory.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accessproviderfactory.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accessproviderfactory.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/mockprovider.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/mockprovider.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/iaccessprovider.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_accessproviderfactory` — build fails: `accesscontrol/accessproviderfactory.h: No such file or directory` (RED).
- [ ] Write `qt-app/core/accesscontrol/accessproviderfactory.h`:
  ```cpp
  #ifndef ACCESSCONTROL_ACCESSPROVIDERFACTORY_H
  #define ACCESSCONTROL_ACCESSPROVIDERFACTORY_H

  #include <functional>
  #include <QList>
  #include <QVariantMap>
  #include "accesscontrol/accesstypes.h"

  namespace AccessControl {

  class IAccessProvider;

  // Registry mapping a ProviderDescriptor (keyed by providerId) to a creator.
  // Providers register their creator at startup; the admin UI (a later sub-plan)
  // reads available() to populate a picker and calls create() for the chosen one.
  // Created providers are parented to the caller-supplied parent — ownership via
  // the Qt object tree, never the factory.
  class AccessProviderFactory
  {
  public:
      using CreatorFn =
          std::function<IAccessProvider *(const ProviderDescriptor &, const QVariantMap &, QObject *)>;

      void registerProvider(const ProviderDescriptor &descriptor, CreatorFn creator);
      QList<ProviderDescriptor> available() const;
      // parent is REQUIRED (non-null): ownership is mandatory, so a null parent
      // yields nullptr rather than an unowned provider.
      IAccessProvider *create(const ProviderDescriptor &descriptor,
                              const QVariantMap &config,
                              QObject *parent) const;

  private:
      struct Entry {
          ProviderDescriptor descriptor;
          CreatorFn creator;
      };
      QList<Entry> m_entries;   // registration order preserved
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_ACCESSPROVIDERFACTORY_H
  ```
- [ ] Write `qt-app/core/accesscontrol/accessproviderfactory.cpp`:
  ```cpp
  #include "accesscontrol/accessproviderfactory.h"
  #include "accesscontrol/iaccessprovider.h"

  namespace AccessControl {

  void AccessProviderFactory::registerProvider(const ProviderDescriptor &descriptor,
                                               CreatorFn creator)
  {
      m_entries.append(Entry{ descriptor, std::move(creator) });
  }

  QList<ProviderDescriptor> AccessProviderFactory::available() const
  {
      QList<ProviderDescriptor> out;
      out.reserve(m_entries.size());
      for (const Entry &e : m_entries)
          out.append(e.descriptor);
      return out;
  }

  IAccessProvider *AccessProviderFactory::create(const ProviderDescriptor &descriptor,
                                                 const QVariantMap &config,
                                                 QObject *parent) const
  {
      if (!parent)
          return nullptr;   // ownership is mandatory — never return an unowned provider
      for (const Entry &e : m_entries) {
          if (e.descriptor.providerId == descriptor.providerId && e.creator) {
              IAccessProvider *p = e.creator(descriptor, config, parent);
              // Enforce the ownership contract regardless of what the creator did:
              // a creator that forgot to pass `parent` (or parented elsewhere) must
              // not leave the provider unowned. setParent is a no-op if already correct.
              if (p && p->parent() != parent)
                  p->setParent(parent);
              return p;
          }
      }
      return nullptr;   // unknown id — caller decides how to surface it
  }

  } // namespace AccessControl
  ```
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_accessproviderfactory && ctest --test-dir qt-app/build -R tst_accessproviderfactory --output-on-failure` — 6 slots pass.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/accessproviderfactory.h qt-app/core/accesscontrol/accessproviderfactory.cpp qt-app/tests/tst_accessproviderfactory.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add AccessProviderFactory

  A registration-based registry mapping providerId -> creator. available()
  feeds the future admin picker; create() builds the chosen provider parented
  to its owner and returns nullptr for an unknown id."
  ```

---

## Task 5 — AccessDecisionService (async verify; timeout = Error)

**Files**
- Create: `qt-app/core/accesscontrol/accessdecisionservice.h`, `qt-app/core/accesscontrol/accessdecisionservice.cpp`
- Test: `qt-app/tests/tst_accessdecisionservice.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces**
- Consumes: `Credential`, `AccessDecision` (Task 1), `QNetworkAccessManager` (injected), `ApiConfig::endpoint()` from `core/apiconfig.h`, `CapturingNam` from `testsupport/`.
- Produces: `class AccessControl::AccessDecisionService : public QObject`:
  - `explicit AccessDecisionService(QNetworkAccessManager *nam, QObject *parent = nullptr);`
  - `void setTimeoutMs(int ms);`
  - `void verify(const Credential &credential);`  // async → emits `decided`
  - `void cancel();`  // invalidate all in-flight verifies; a capture adapter calls this from its `stop()` so a decision that arrives after shutdown is dropped, never emitted
  - `static AccessDecision decodeResponse(const QByteArray &raw, const QString &correlationId);`  // pure
  - `signals: void decided(const AccessControl::AccessDecision &decision);`
- Response contract decoded (from the future backend endpoint, a later sub-plan): a JSON object `{"decision":"granted"|"denied", "subject_id":"...", "message":"..."}`. `granted` **with a non-empty string `subject_id`** → `Granted`; `granted` **missing/empty/non-string `subject_id`** → `Error` (a grant with no identifiable subject is a broken contract, not a real allow); `denied` → `Denied`; anything else / non-object → `Error`. Transport failure or timeout → `Error` (never `Denied`).

**Steps**

- [ ] Write the failing test `qt-app/tests/tst_accessdecisionservice.cpp` (includes a test-local hanging NAM so the timeout path needs no live network):
  ```cpp
  #include <QtTest>
  #include <QNetworkAccessManager>
  #include <QNetworkReply>
  #include <QNetworkRequest>
  #include <QSignalSpy>
  #include "capturingnam.h"
  #include "accesscontrol/accesstypes.h"
  #include "accesscontrol/accessdecisionservice.h"

  using namespace AccessControl;

  // A reply that never finishes on its own — only abort() (fired by the
  // service's timeout timer) completes it, with OperationCanceledError.
  class HangingReply : public QNetworkReply
  {
      Q_OBJECT
  public:
      explicit HangingReply(QObject *parent = nullptr) : QNetworkReply(parent)
      {
          open(QIODevice::ReadOnly);
      }
      void abort() override
      {
          setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted"));
          setFinished(true);
          emit errorOccurred(QNetworkReply::OperationCanceledError);
          emit finished();
      }
  protected:
      qint64 readData(char *, qint64) override { return -1; }
  };

  class HangingNam : public QNetworkAccessManager
  {
  public:
      using QNetworkAccessManager::QNetworkAccessManager;
  protected:
      QNetworkReply *createRequest(Operation, const QNetworkRequest &, QIODevice *) override
      {
          return new HangingReply(this);
      }
  };

  class TestAccessDecisionService : public QObject
  {
      Q_OBJECT
  private slots:
      void initTestCase() { registerMetaTypes(); }
      void decodeGrantedResponse();
      void decodeGrantedWithoutSubjectIsError();
      void decodeDeniedResponse();
      void decodeInvalidBodyIsError();
      void verifyGrantedEmitsDecidedGrantedAndSendsRequest();
      void verifyTransportFailureEmitsError();
      void verifyTimeoutEmitsError();   // the required invariant
      void cancelDropsPendingDecision();
  };

  void TestAccessDecisionService::decodeGrantedResponse()
  {
      const AccessDecision d = AccessDecisionService::decodeResponse(
          QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"S-1\"}"),
          QStringLiteral("corr-x"));
      QCOMPARE(d.result, AccessDecision::Result::Granted);
      QCOMPARE(d.subjectId, QStringLiteral("S-1"));
      QCOMPARE(d.correlationId, QStringLiteral("corr-x"));
  }

  void TestAccessDecisionService::decodeGrantedWithoutSubjectIsError()
  {
      // A 200 that says "granted" but carries no subject_id must NOT authorize.
      const AccessDecision d = AccessDecisionService::decodeResponse(
          QByteArrayLiteral("{\"decision\":\"granted\"}"), QStringLiteral("corr-n"));
      QCOMPARE(d.result, AccessDecision::Result::Error);
      QVERIFY(d.result != AccessDecision::Result::Granted);
      // An empty-string subject_id is equally unacceptable.
      const AccessDecision d2 = AccessDecisionService::decodeResponse(
          QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"\"}"),
          QStringLiteral("corr-n2"));
      QCOMPARE(d2.result, AccessDecision::Result::Error);
  }

  void TestAccessDecisionService::decodeDeniedResponse()
  {
      const AccessDecision d = AccessDecisionService::decodeResponse(
          QByteArrayLiteral("{\"decision\":\"denied\",\"message\":\"expired card\"}"),
          QStringLiteral("corr-y"));
      QCOMPARE(d.result, AccessDecision::Result::Denied);
      QCOMPARE(d.reason, QStringLiteral("expired card"));
  }

  void TestAccessDecisionService::decodeInvalidBodyIsError()
  {
      // Malformed / non-JSON body is infrastructure trouble, not a policy denial.
      const AccessDecision d = AccessDecisionService::decodeResponse(
          QByteArrayLiteral("<html>502</html>"), QStringLiteral("corr-z"));
      QCOMPARE(d.result, AccessDecision::Result::Error);
  }

  void TestAccessDecisionService::verifyGrantedEmitsDecidedGrantedAndSendsRequest()
  {
      CapturingNam nam(QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"S-9\"}"));
      AccessDecisionService svc(&nam);
      QSignalSpy spy(&svc, &AccessDecisionService::decided);
      Credential c;
      c.kind = CredentialKind::Rfid;
      c.raw = QStringLiteral("CARD-9");
      c.gateId = QStringLiteral("gate-a");
      svc.verify(c);
      QVERIFY(spy.wait(1000));
      QCOMPARE(spy.count(), 1);
      const auto d = qvariant_cast<AccessDecision>(spy.at(0).at(0));
      QCOMPARE(d.result, AccessDecision::Result::Granted);
      QCOMPARE(d.subjectId, QStringLiteral("S-9"));

      // Assert the request the service actually assembled (CapturingNam records it).
      QCOMPARE(nam.lastOp, QNetworkAccessManager::PostOperation);
      QVERIFY(nam.lastUrl.toString().endsWith(QStringLiteral("access_verify.php")));
      QCOMPARE(nam.lastContentType, QStringLiteral("application/x-www-form-urlencoded"));
      const QString body = QString::fromUtf8(nam.lastBody);
      QVERIFY(body.contains(QStringLiteral("raw=CARD-9")));
      QVERIFY(body.contains(QStringLiteral("gate_id=gate-a")));
      QVERIFY(body.contains(QStringLiteral("kind=")));   // CredentialKind encoded
  }

  void TestAccessDecisionService::verifyTransportFailureEmitsError()
  {
      // A transport failure (connection refused) carries no decodable decision:
      // it is Error, never a fabricated Denied — and its reason is NOT the timeout
      // message (this proves the transport branch, distinct from the timeout branch).
      CapturingNam nam(QByteArrayLiteral(""), QNetworkReply::ConnectionRefusedError, 0);
      AccessDecisionService svc(&nam);
      QSignalSpy spy(&svc, &AccessDecisionService::decided);
      Credential c;
      c.raw = QStringLiteral("CARD-X");
      svc.verify(c);
      QVERIFY(spy.wait(1000));
      QCOMPARE(spy.count(), 1);
      const auto d = qvariant_cast<AccessDecision>(spy.at(0).at(0));
      QCOMPARE(d.result, AccessDecision::Result::Error);
      QVERIFY(d.result != AccessDecision::Result::Denied);
      QVERIFY(d.reason != QStringLiteral("Verification timed out"));
  }

  void TestAccessDecisionService::verifyTimeoutEmitsError()
  {
      HangingNam nam;
      AccessDecisionService svc(&nam);
      svc.setTimeoutMs(20);   // tiny timeout; reply never finishes on its own
      QSignalSpy spy(&svc, &AccessDecisionService::decided);
      Credential c;
      c.raw = QStringLiteral("CARD-STUCK");
      svc.verify(c);
      QVERIFY(spy.wait(1000));
      QCOMPARE(spy.count(), 1);
      const auto d = qvariant_cast<AccessDecision>(spy.at(0).at(0));
      // A timeout is Error, NEVER a fabricated Denied — and the reason proves it was
      // the timeout/abort path, not some other transport error masquerading as one.
      QCOMPARE(d.result, AccessDecision::Result::Error);
      QVERIFY(d.result != AccessDecision::Result::Denied);
      QCOMPARE(d.reason, QStringLiteral("Verification timed out"));
  }

  void TestAccessDecisionService::cancelDropsPendingDecision()
  {
      // CapturingNam finishes its reply on the event loop (not synchronously), so a
      // cancel() issued before we spin the loop invalidates the in-flight verify: the
      // finished handler sees a newer generation and emits nothing. This is the
      // contract a capture adapter relies on when it stops mid-verify.
      CapturingNam nam(QByteArrayLiteral("{\"decision\":\"granted\",\"subject_id\":\"S-1\"}"));
      AccessDecisionService svc(&nam);
      QSignalSpy spy(&svc, &AccessDecisionService::decided);
      Credential c;
      c.raw = QStringLiteral("CARD-1");
      svc.verify(c);
      svc.cancel();          // invalidate before the reply is delivered
      QTest::qWait(100);
      QCOMPARE(spy.count(), 0);   // the late decision was dropped, not emitted
  }

  QTEST_MAIN(TestAccessDecisionService)
  #include "tst_accessdecisionservice.moc"
  ```
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append) — links `Qt::Network` and pulls in `capturingnam` like the controller tests:
  ```cmake
  # --- Access Control: decision service (Network; no offscreen) ---
  wits_add_qttest(tst_accessdecisionservice
      SOURCES
          tst_accessdecisionservice.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accessdecisionservice.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accessdecisionservice.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
          ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
          ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
      LIBS Qt${QT_VERSION_MAJOR}::Network
      INCLUDES ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_accessdecisionservice` — build fails: `accesscontrol/accessdecisionservice.h: No such file or directory` (RED).
- [ ] Write `qt-app/core/accesscontrol/accessdecisionservice.h`:
  ```cpp
  #ifndef ACCESSCONTROL_ACCESSDECISIONSERVICE_H
  #define ACCESSCONTROL_ACCESSDECISIONSERVICE_H

  #include <QByteArray>
  #include <QObject>
  #include <QString>
  #include "accesscontrol/accesstypes.h"

  class QNetworkAccessManager;

  namespace AccessControl {

  // Verifies a Credential against the backend and emits a decided()
  // AccessDecision. Follows the repo idiom: an injected QNetworkAccessManager
  // (not owned) so it is testable with CapturingNam, and it decodes the reply
  // through a pure static so decode logic is unit-testable with no network.
  // A timeout or transport failure yields Result::Error — NEVER a fabricated
  // Denied (Denied is reserved for a well-formed policy rejection).
  //
  // Lifecycle: verify() is fire-and-forget. cancel() invalidates every verify still
  // in flight (via a generation counter) so a capture adapter can call it from its
  // own stop() and be sure no late decided() fires after shutdown. Reply cleanup is
  // bound to the reply itself, so replies never leak against the injected NAM even
  // if this service is destroyed mid-request.
  class AccessDecisionService : public QObject
  {
      Q_OBJECT
  public:
      explicit AccessDecisionService(QNetworkAccessManager *nam, QObject *parent = nullptr);

      void setTimeoutMs(int ms);
      void verify(const Credential &credential);
      void cancel();

      static AccessDecision decodeResponse(const QByteArray &raw,
                                           const QString &correlationId);

  signals:
      void decided(const AccessControl::AccessDecision &decision);

  private:
      QNetworkAccessManager *m_nam;   // injected, not owned
      int m_timeoutMs = 5000;
      quint64 m_generation = 0;       // bumped by cancel(); a verify from an older gen is dropped
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_ACCESSDECISIONSERVICE_H
  ```
- [ ] Write `qt-app/core/accesscontrol/accessdecisionservice.cpp`:
  ```cpp
  #include "accesscontrol/accessdecisionservice.h"
  #include "apiconfig.h"

  #include <QJsonDocument>
  #include <QJsonObject>
  #include <QNetworkAccessManager>
  #include <QNetworkReply>
  #include <QNetworkRequest>
  #include <QTimer>
  #include <QUrlQuery>
  #include <QUuid>

  namespace AccessControl {

  AccessDecisionService::AccessDecisionService(QNetworkAccessManager *nam, QObject *parent)
      : QObject(parent)
      , m_nam(nam)
  {}

  void AccessDecisionService::setTimeoutMs(int ms) { m_timeoutMs = ms; }

  void AccessDecisionService::cancel()
  {
      // Bump the generation: any verify still in flight captured an older generation,
      // so its finished handler will drop the result instead of emitting decided().
      ++m_generation;
  }

  AccessDecision AccessDecisionService::decodeResponse(const QByteArray &raw,
                                                       const QString &correlationId)
  {
      AccessDecision d;
      d.correlationId = correlationId;

      const QJsonDocument doc = QJsonDocument::fromJson(raw);
      if (!doc.isObject()) {
          d.result = AccessDecision::Result::Error;   // malformed body == infra trouble
          d.reason = QStringLiteral("Invalid server response");
          return d;
      }

      const QJsonObject obj = doc.object();
      const QString decision = obj.value(QStringLiteral("decision")).toString();
      if (decision == QLatin1String("granted")) {
          const QJsonValue subj = obj.value(QStringLiteral("subject_id"));
          if (!subj.isString() || subj.toString().isEmpty()) {
              // A grant with no identifiable subject is a broken contract, not a
              // real allow. Fail safe to Error so a malformed 200 cannot authorize.
              d.result = AccessDecision::Result::Error;
              d.reason = QStringLiteral("Granted response missing subject_id");
              return d;
          }
          d.result = AccessDecision::Result::Granted;
          d.subjectId = subj.toString();
      } else if (decision == QLatin1String("denied")) {
          d.result = AccessDecision::Result::Denied;   // real policy rejection
          d.subjectId = obj.value(QStringLiteral("subject_id")).toString();
          d.reason = obj.value(QStringLiteral("message")).toString();
      } else {
          d.result = AccessDecision::Result::Error;     // unknown/absent decision
          d.reason = QStringLiteral("Unrecognized decision");
      }
      return d;
  }

  void AccessDecisionService::verify(const Credential &credential)
  {
      const QString correlationId =
          QUuid::createUuid().toString(QUuid::WithoutBraces);
      const quint64 gen = m_generation;   // snapshot; cancel() bumps this to invalidate

      QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("access_verify.php")));
      request.setHeader(QNetworkRequest::ContentTypeHeader,
                        QStringLiteral("application/x-www-form-urlencoded"));

      QUrlQuery form;
      form.addQueryItem(QStringLiteral("raw"), credential.raw);
      form.addQueryItem(QStringLiteral("gate_id"), credential.gateId);
      form.addQueryItem(QStringLiteral("kind"),
                        QString::number(static_cast<int>(credential.kind)));

      QNetworkReply *reply =
          m_nam->post(request, form.query(QUrl::FullyEncoded).toUtf8());

      // Lifecycle: verify() is fire-and-forget. The reply's cleanup is bound to
      // the REPLY itself (not to `this`), so it is always deleted when it finishes
      // even if this service is destroyed while the request is in flight — no leak
      // against the long-lived injected NAM. The decode handler below uses `this`
      // as its context object, so Qt auto-disconnects it if the service dies first;
      // a late reply is then simply dropped (no use-after-free, no spurious emit).
      connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);

      // Timeout timer parented to the reply so it dies with the reply (no leak,
      // no manual delete). On timeout it aborts the reply, which finishes it with
      // OperationCanceledError and routes to the Error branch below.
      QTimer *timer = new QTimer(reply);
      timer->setSingleShot(true);
      connect(timer, &QTimer::timeout, reply, [reply]() { reply->abort(); });
      timer->start(m_timeoutMs);

      connect(reply, &QNetworkReply::finished, this, [this, reply, correlationId, gen]() {
          if (gen != m_generation)
              return;   // cancelled since this request began — drop the late decision
          if (reply->error() != QNetworkReply::NoError) {
              AccessDecision d;
              d.result = AccessDecision::Result::Error;   // timeout OR transport == Error
              d.correlationId = correlationId;
              d.reason = (reply->error() == QNetworkReply::OperationCanceledError)
                             ? QStringLiteral("Verification timed out")
                             : reply->errorString();
              emit decided(d);
              return;
          }
          emit decided(decodeResponse(reply->readAll(), correlationId));
      });
  }

  } // namespace AccessControl
  ```
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_accessdecisionservice && ctest --test-dir qt-app/build -R tst_accessdecisionservice --output-on-failure` — 8 slots pass, including `verifyTimeoutEmitsError`, `verifyTransportFailureEmitsError`, and `cancelDropsPendingDecision`.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/accessdecisionservice.h qt-app/core/accesscontrol/accessdecisionservice.cpp qt-app/tests/tst_accessdecisionservice.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add AccessDecisionService with timeout=Error

  Async verify() posts a credential and emits decided(AccessDecision) via an
  injected QNetworkAccessManager. Decode is a pure static (granted/denied/
  error). A per-request timeout timer aborts a stuck reply so a timeout or
  transport failure yields Result::Error, never a fabricated Denied."
  ```

---

## Task 6 — HealthMonitor (per-provider HealthSnapshot)

**Files**
- Create: `qt-app/core/accesscontrol/healthmonitor.h`, `qt-app/core/accesscontrol/healthmonitor.cpp`
- Test: `qt-app/tests/tst_healthmonitor.cpp`
- Modify: `qt-app/core/accesscontrol/accesstypes.h` (add `HealthSnapshot` + its metatype), `qt-app/tests/CMakeLists.txt`

**Interfaces**
- Consumes: `ConnectionState` (Task 1).
- Produces:
  - `struct AccessControl::HealthSnapshot { QString providerId; ConnectionState state; QDateTime lastCommTime; qint64 latencyMs; int retryCount; };` (added to `accesstypes.h`, with `Q_DECLARE_METATYPE` and registration in `registerMetaTypes()`).
  - `class AccessControl::HealthMonitor : public QObject`:
    - `explicit HealthMonitor(QObject *parent = nullptr);`
    - `void recordState(const QString &providerId, ConnectionState state);`
    - `void recordComm(const QString &providerId, qint64 latencyMs, const QDateTime &at);` (records a comm WITH a measured latency)
    - `void recordCommTime(const QString &providerId, const QDateTime &at);` (records that a comm happened but leaves `latencyMs` untouched — used when latency is unknown, e.g. on connect; never fabricates a zero)
    - `void recordRetry(const QString &providerId);`
    - `HealthSnapshot snapshot(const QString &providerId) const;`
    - `signals: void healthChanged(const AccessControl::HealthSnapshot &snapshot);`

**Steps**

- [ ] Add `HealthSnapshot` to `qt-app/core/accesscontrol/accesstypes.h` — insert this struct just after `ProviderDescriptor`, inside `namespace AccessControl`:
  ```cpp
  // A point-in-time view of one provider's connection health.
  struct HealthSnapshot {
      QString providerId;
      ConnectionState state = ConnectionState::Disconnected;
      QDateTime lastCommTime;
      qint64 latencyMs = -1;     // -1 == no successful comm recorded yet
      int retryCount = 0;
  };
  ```
  add its metatype declaration alongside the others at file scope:
  ```cpp
  Q_DECLARE_METATYPE(AccessControl::HealthSnapshot)
  ```
  and register it inside `registerMetaTypes()` in `accesstypes.cpp`:
  ```cpp
      qRegisterMetaType<HealthSnapshot>();
  ```
- [ ] Write the failing test `qt-app/tests/tst_healthmonitor.cpp`:
  ```cpp
  #include <QtTest>
  #include <QDateTime>
  #include <QSignalSpy>
  #include "accesscontrol/accesstypes.h"
  #include "accesscontrol/healthmonitor.h"

  using namespace AccessControl;

  class TestHealthMonitor : public QObject
  {
      Q_OBJECT
  private slots:
      void initTestCase() { registerMetaTypes(); }
      void unknownProviderReturnsDefaultSnapshot();
      void recordStateUpdatesSnapshotAndEmits();
      void recordCommTracksLatencyAndTime();
      void recordCommTimeLeavesLatencyUnknown();
      void recordRetryIncrements();
  };

  void TestHealthMonitor::unknownProviderReturnsDefaultSnapshot()
  {
      HealthMonitor m;
      const HealthSnapshot s = m.snapshot(QStringLiteral("nope"));
      QCOMPARE(s.state, ConnectionState::Disconnected);
      QCOMPARE(s.latencyMs, qint64(-1));
      QCOMPARE(s.retryCount, 0);
  }

  void TestHealthMonitor::recordStateUpdatesSnapshotAndEmits()
  {
      HealthMonitor m;
      QSignalSpy spy(&m, &HealthMonitor::healthChanged);
      m.recordState(QStringLiteral("mock"), ConnectionState::Connected);
      QCOMPARE(spy.count(), 1);
      QCOMPARE(m.snapshot(QStringLiteral("mock")).state, ConnectionState::Connected);
      QCOMPARE(qvariant_cast<HealthSnapshot>(spy.at(0).at(0)).providerId,
               QStringLiteral("mock"));
  }

  void TestHealthMonitor::recordCommTracksLatencyAndTime()
  {
      HealthMonitor m;
      const QDateTime t = QDateTime::currentDateTimeUtc();
      m.recordComm(QStringLiteral("mock"), 42, t);
      const HealthSnapshot s = m.snapshot(QStringLiteral("mock"));
      QCOMPARE(s.latencyMs, qint64(42));
      QCOMPARE(s.lastCommTime, t);
  }

  void TestHealthMonitor::recordCommTimeLeavesLatencyUnknown()
  {
      HealthMonitor m;
      const QDateTime t = QDateTime::currentDateTimeUtc();
      m.recordCommTime(QStringLiteral("mock"), t);
      const HealthSnapshot s = m.snapshot(QStringLiteral("mock"));
      QCOMPARE(s.lastCommTime, t);
      QCOMPARE(s.latencyMs, qint64(-1));   // latency stays unknown — never fabricated
  }

  void TestHealthMonitor::recordRetryIncrements()
  {
      HealthMonitor m;
      m.recordRetry(QStringLiteral("mock"));
      m.recordRetry(QStringLiteral("mock"));
      QCOMPARE(m.snapshot(QStringLiteral("mock")).retryCount, 2);
  }

  QTEST_MAIN(TestHealthMonitor)
  #include "tst_healthmonitor.moc"
  ```
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append):
  ```cmake
  # --- Access Control: health monitor (pure core, no offscreen) ---
  wits_add_qttest(tst_healthmonitor
      SOURCES
          tst_healthmonitor.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/healthmonitor.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/healthmonitor.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_healthmonitor` — build fails: `accesscontrol/healthmonitor.h: No such file or directory` (RED).
- [ ] Write `qt-app/core/accesscontrol/healthmonitor.h`:
  ```cpp
  #ifndef ACCESSCONTROL_HEALTHMONITOR_H
  #define ACCESSCONTROL_HEALTHMONITOR_H

  #include <QDateTime>
  #include <QHash>
  #include <QObject>
  #include <QString>
  #include "accesscontrol/accesstypes.h"

  namespace AccessControl {

  // Tracks per-provider connection health (state, last comm time, latency, retry
  // count) and emits healthChanged() whenever a fact changes. Owned by
  // AccessControlService; read by a future admin health panel (a later sub-plan).
  class HealthMonitor : public QObject
  {
      Q_OBJECT
  public:
      explicit HealthMonitor(QObject *parent = nullptr);

      void recordState(const QString &providerId, ConnectionState state);
      void recordComm(const QString &providerId, qint64 latencyMs, const QDateTime &at);
      void recordCommTime(const QString &providerId, const QDateTime &at);
      void recordRetry(const QString &providerId);

      HealthSnapshot snapshot(const QString &providerId) const;

  signals:
      void healthChanged(const AccessControl::HealthSnapshot &snapshot);

  private:
      HealthSnapshot &entry(const QString &providerId);   // creates on first use

      QHash<QString, HealthSnapshot> m_byProvider;
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_HEALTHMONITOR_H
  ```
- [ ] Write `qt-app/core/accesscontrol/healthmonitor.cpp`:
  ```cpp
  #include "accesscontrol/healthmonitor.h"

  namespace AccessControl {

  HealthMonitor::HealthMonitor(QObject *parent)
      : QObject(parent)
  {}

  HealthSnapshot &HealthMonitor::entry(const QString &providerId)
  {
      auto it = m_byProvider.find(providerId);
      if (it == m_byProvider.end()) {
          HealthSnapshot fresh;
          fresh.providerId = providerId;
          it = m_byProvider.insert(providerId, fresh);
      }
      return it.value();
  }

  void HealthMonitor::recordState(const QString &providerId, ConnectionState state)
  {
      HealthSnapshot &s = entry(providerId);
      s.state = state;
      emit healthChanged(s);
  }

  void HealthMonitor::recordComm(const QString &providerId, qint64 latencyMs,
                                 const QDateTime &at)
  {
      HealthSnapshot &s = entry(providerId);
      s.latencyMs = latencyMs;
      s.lastCommTime = at;
      emit healthChanged(s);
  }

  void HealthMonitor::recordCommTime(const QString &providerId, const QDateTime &at)
  {
      // A comm happened but we have no latency measurement — record the time only,
      // leaving latencyMs at whatever it was (-1 == still unknown). Never fabricate.
      HealthSnapshot &s = entry(providerId);
      s.lastCommTime = at;
      emit healthChanged(s);
  }

  void HealthMonitor::recordRetry(const QString &providerId)
  {
      HealthSnapshot &s = entry(providerId);
      ++s.retryCount;
      emit healthChanged(s);
  }

  HealthSnapshot HealthMonitor::snapshot(const QString &providerId) const
  {
      const auto it = m_byProvider.constFind(providerId);
      if (it == m_byProvider.constEnd()) {
          HealthSnapshot fresh;
          fresh.providerId = providerId;
          return fresh;
      }
      return it.value();
  }

  } // namespace AccessControl
  ```
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_healthmonitor && ctest --test-dir qt-app/build -R tst_healthmonitor --output-on-failure` — 5 slots pass.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/healthmonitor.h qt-app/core/accesscontrol/healthmonitor.cpp qt-app/core/accesscontrol/accesstypes.h qt-app/core/accesscontrol/accesstypes.cpp qt-app/tests/tst_healthmonitor.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add HealthMonitor and HealthSnapshot

  Per-provider health tracking (state, last comm time, latency, retry count)
  exposed as a HealthSnapshot value type, with healthChanged() emitted on every
  update. HealthSnapshot joins registerMetaTypes() so it crosses queued signals."
  ```

---

## Task 7 — AccessControlService (enable/disable, provider→bus wiring, state machine + backoff)

**Files**
- Create: `qt-app/core/accesscontrol/accesscontrolservice.h`, `qt-app/core/accesscontrol/accesscontrolservice.cpp`
- Test: `qt-app/tests/tst_accesscontrolservice.cpp`
- Modify: `qt-app/tests/CMakeLists.txt`

**Interfaces**
- Consumes: `EventBus` (T2), `AccessProviderFactory` (T4), `IAccessProvider`/`MockProvider` (T3), `HealthMonitor` (T6), `AccessEvent`/`ProviderDescriptor`/`ConnectionState` (T1).
- Produces: `class AccessControl::AccessControlService : public QObject`:
  - `explicit AccessControlService(EventBus *bus, AccessProviderFactory *factory, QObject *parent = nullptr);`
  - `bool isEnabled() const;`  // default false — the flag-OFF invariant
  - `void enable(const ProviderDescriptor &descriptor, const QVariantMap &config);`
  - `void disable();`
  - `ConnectionState connectionState() const;`
  - `HealthMonitor *healthMonitor() const;`
  - `bool isReconnectPending() const;`  // true while a reconnect timer is armed (diagnostics + deterministic tests)
  - `void setReconnectBaseMs(int ms);`  // injectable backoff base for tests
  - `signals: void enabledChanged(bool enabled); void connectionStateChanged(AccessControl::ConnectionState state);`
- Produced for later sub-plans: `KioskViewModel` and the admin `AccessControlViewModel` will consume `EventBus::eventPublished` and this service's `enable()/disable()` (later sub-plans; not in scope here).

**Steps**

- [ ] Write the failing test `qt-app/tests/tst_accesscontrolservice.cpp` (a captured creator hands the test a handle to the mock the factory builds):
  ```cpp
  #include <QtTest>
  #include <QSignalSpy>
  #include <QVariantMap>
  #include "accesscontrol/accesstypes.h"
  #include "accesscontrol/eventbus.h"
  #include "accesscontrol/accessproviderfactory.h"
  #include "accesscontrol/mockprovider.h"
  #include "accesscontrol/accesscontrolservice.h"

  using namespace AccessControl;

  class TestAccessControlService : public QObject
  {
      Q_OBJECT
  private slots:
      void initTestCase() { registerMetaTypes(); }
      void defaultsToDisabled();
      void enableStartsProviderAndPublishesConnected();
      void providerEventIsRepublishedOnBus();
      void disableStopsRepublishing();
      void degradedSchedulesReconnect();
      void reconnectTimerCancelledWhenConnected();
      void unexpectedDisconnectPublishesAndReconnects();
  };

  // Builds a factory whose "mock" creator stores the created instance in *out so
  // the test can drive it after enable().
  static AccessProviderFactory factoryCapturing(MockProvider **out)
  {
      AccessProviderFactory f;
      f.registerProvider(MockProvider::defaultDescriptor(),
          [out](const ProviderDescriptor &d, const QVariantMap &, QObject *parent) -> IAccessProvider * {
              auto *p = new MockProvider(d, parent);
              *out = p;
              return p;
          });
      return f;
  }

  void TestAccessControlService::defaultsToDisabled()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      QSignalSpy busSpy(&bus, &EventBus::eventPublished);
      AccessControlService svc(&bus, &f);

      // Flag OFF by default means MORE than a bool: nothing is built and nothing
      // is emitted. Prove all three so "zero behaviour change" is actually tested.
      QVERIFY(!svc.isEnabled());
      QVERIFY(mock == nullptr);                 // factory creator was never called
      QCOMPARE(busSpy.count(), 0);              // no traffic on the bus
      QCOMPARE(svc.connectionState(), ConnectionState::Disconnected);
  }

  void TestAccessControlService::enableStartsProviderAndPublishesConnected()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      AccessControlService svc(&bus, &f);
      QSignalSpy enabledSpy(&svc, &AccessControlService::enabledChanged);
      QSignalSpy busSpy(&bus, &EventBus::eventPublished);

      svc.enable(MockProvider::defaultDescriptor(), {});
      QVERIFY(svc.isEnabled());
      QCOMPARE(enabledSpy.count(), 1);
      QCOMPARE(svc.connectionState(), ConnectionState::Connected);
      // Reaching Connected republishes a ControllerConnected event on the bus.
      bool sawConnected = false;
      for (const auto &call : busSpy)
          if (qvariant_cast<AccessEvent>(call.at(0)).type == AccessEvent::Type::ControllerConnected)
              sawConnected = true;
      QVERIFY(sawConnected);
  }

  void TestAccessControlService::providerEventIsRepublishedOnBus()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      AccessControlService svc(&bus, &f);
      svc.enable(MockProvider::defaultDescriptor(), {});
      QVERIFY(mock != nullptr);

      QSignalSpy busSpy(&bus, &EventBus::eventPublished);
      mock->simulateGranted(QStringLiteral("S-1"), QStringLiteral("gate-a"));
      QCOMPARE(busSpy.count(), 1);
      const auto ev = qvariant_cast<AccessEvent>(busSpy.at(0).at(0));
      QCOMPARE(ev.type, AccessEvent::Type::AccessGranted);
      // The service stamps the emitting provider's id even though the mock left it
      // empty — bus consumers always know the provenance.
      QCOMPARE(ev.providerId, QStringLiteral("mock"));
  }

  void TestAccessControlService::disableStopsRepublishing()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      AccessControlService svc(&bus, &f);
      svc.enable(MockProvider::defaultDescriptor(), {});
      QVERIFY(mock != nullptr);
      MockProvider *captured = mock;   // keep a handle; service will drop its own

      svc.disable();
      QVERIFY(!svc.isEnabled());

      // The provider is torn down; nothing further reaches the bus.
      QSignalSpy busSpy(&bus, &EventBus::eventPublished);
      captured->simulateGranted(QStringLiteral("S-2"), QStringLiteral("gate-a"));
      QCOMPARE(busSpy.count(), 0);
  }

  void TestAccessControlService::degradedSchedulesReconnect()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      AccessControlService svc(&bus, &f);
      svc.setReconnectBaseMs(5);   // tiny backoff so the test is fast
      svc.enable(MockProvider::defaultDescriptor(), {});
      QVERIFY(mock != nullptr);

      QSignalSpy stateSpy(&svc, &AccessControlService::connectionStateChanged);
      mock->simulateDisconnect();   // -> Degraded, which schedules a reconnect
      QCOMPARE(svc.connectionState(), ConnectionState::Degraded);
      // Backoff timer fires provider->start(), returning to Connected.
      QVERIFY(QTest::qWaitFor([&]() {
          return svc.connectionState() == ConnectionState::Connected;
      }, 1000));
      QVERIFY(svc.healthMonitor()->snapshot(QStringLiteral("mock")).retryCount >= 1);
  }

  void TestAccessControlService::reconnectTimerCancelledWhenConnected()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      AccessControlService svc(&bus, &f);
      svc.setReconnectBaseMs(30000);   // == cap; far longer than the test, so the timer will NOT fire
      svc.enable(MockProvider::defaultDescriptor(), {});
      QVERIFY(mock != nullptr);

      mock->simulateDisconnect();               // -> Degraded, arms the reconnect timer
      QCOMPARE(svc.connectionState(), ConnectionState::Degraded);
      QVERIFY(svc.isReconnectPending());        // timer is armed (won't fire for 60s)

      mock->start();                            // provider recovers on its own -> Connected
      QCOMPARE(svc.connectionState(), ConnectionState::Connected);
      QVERIFY(!svc.isReconnectPending());       // reaching Connected CANCELLED the timer
  }

  void TestAccessControlService::unexpectedDisconnectPublishesAndReconnects()
  {
      EventBus bus;
      MockProvider *mock = nullptr;
      AccessProviderFactory f = factoryCapturing(&mock);
      AccessControlService svc(&bus, &f);
      svc.setReconnectBaseMs(5);
      svc.enable(MockProvider::defaultDescriptor(), {});
      QVERIFY(mock != nullptr);

      QSignalSpy busSpy(&bus, &EventBus::eventPublished);
      mock->stop();   // an UNEXPECTED drop while enabled (the service did not ask for it)
      QCOMPARE(svc.connectionState(), ConnectionState::Disconnected);
      bool sawDisconnected = false;
      for (const auto &call : busSpy)
          if (qvariant_cast<AccessEvent>(call.at(0)).type == AccessEvent::Type::ControllerDisconnected)
              sawDisconnected = true;
      QVERIFY(sawDisconnected);
      // ...and the scheduled reconnect brings it back to Connected.
      QVERIFY(QTest::qWaitFor([&]() {
          return svc.connectionState() == ConnectionState::Connected;
      }, 1000));
  }

  QTEST_MAIN(TestAccessControlService)
  #include "tst_accesscontrolservice.moc"
  ```
- [ ] Register the test target in `qt-app/tests/CMakeLists.txt` (append):
  ```cmake
  # --- Access Control: lifecycle service + state machine (pure core, no offscreen) ---
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
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/iaccessprovider.h
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
          ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```
- [ ] Run it, expect FAIL: `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build --target tst_accesscontrolservice` — build fails: `accesscontrol/accesscontrolservice.h: No such file or directory` (RED).
- [ ] Write `qt-app/core/accesscontrol/accesscontrolservice.h`:
  ```cpp
  #ifndef ACCESSCONTROL_ACCESSCONTROLSERVICE_H
  #define ACCESSCONTROL_ACCESSCONTROLSERVICE_H

  #include <QObject>
  #include <QVariantMap>
  #include "accesscontrol/accesstypes.h"

  class QTimer;

  namespace AccessControl {

  class EventBus;
  class AccessProviderFactory;
  class IAccessProvider;
  class HealthMonitor;

  // Lifecycle owner for the active access-control provider. Disabled by default
  // (accessControl.enabled == false) so the seam is zero behaviour change until
  // enable() is called. enable() builds the provider via the factory, republishes
  // its accessEvents onto the EventBus, and drives a per-provider connection
  // state machine (Disconnected -> Connecting -> Connected -> Degraded/Error)
  // with exponential backoff reconnect. Owns its HealthMonitor.
  class AccessControlService : public QObject
  {
      Q_OBJECT
  public:
      explicit AccessControlService(EventBus *bus, AccessProviderFactory *factory,
                                    QObject *parent = nullptr);

      bool isEnabled() const;
      void enable(const ProviderDescriptor &descriptor, const QVariantMap &config);
      void disable();

      ConnectionState connectionState() const;
      HealthMonitor *healthMonitor() const;
      bool isReconnectPending() const;
      void setReconnectBaseMs(int ms);

  signals:
      void enabledChanged(bool enabled);
      void connectionStateChanged(AccessControl::ConnectionState state);

  private:
      void onProviderState(ConnectionState state);
      void onHardwareError(const QString &message);
      void publishControllerEvent(AccessEvent::Type type);
      void scheduleReconnect();

      EventBus *m_bus;                    // injected, not owned
      AccessProviderFactory *m_factory;   // injected, not owned
      IAccessProvider *m_provider = nullptr;  // owned (parented to this)
      HealthMonitor *m_health;            // owned (parented to this)
      QTimer *m_reconnectTimer;           // owned (parented to this)

      bool m_enabled = false;             // flag OFF by default
      ConnectionState m_state = ConnectionState::Disconnected;
      int m_reconnectBaseMs = 1000;
      int m_reconnectNextMs = 1000;
      static constexpr int kReconnectMaxMs = 30000;
  };

  } // namespace AccessControl

  #endif // ACCESSCONTROL_ACCESSCONTROLSERVICE_H
  ```
- [ ] Write `qt-app/core/accesscontrol/accesscontrolservice.cpp`:
  ```cpp
  #include "accesscontrol/accesscontrolservice.h"
  #include "accesscontrol/accessproviderfactory.h"
  #include "accesscontrol/eventbus.h"
  #include "accesscontrol/healthmonitor.h"
  #include "accesscontrol/iaccessprovider.h"

  #include <QDateTime>
  #include <QTimer>

  namespace AccessControl {

  AccessControlService::AccessControlService(EventBus *bus,
                                             AccessProviderFactory *factory,
                                             QObject *parent)
      : QObject(parent)
      , m_bus(bus)
      , m_factory(factory)
      , m_health(new HealthMonitor(this))
      , m_reconnectTimer(new QTimer(this))
  {
      m_reconnectTimer->setSingleShot(true);
      connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
          if (m_enabled && m_provider) {
              m_health->recordRetry(m_provider->descriptor().providerId);
              m_provider->start();
          }
      });
  }

  bool AccessControlService::isEnabled() const { return m_enabled; }

  ConnectionState AccessControlService::connectionState() const { return m_state; }

  HealthMonitor *AccessControlService::healthMonitor() const { return m_health; }

  bool AccessControlService::isReconnectPending() const
  {
      return m_reconnectTimer->isActive();
  }

  void AccessControlService::setReconnectBaseMs(int ms)
  {
      // Clamp to [1, kReconnectMaxMs]: a base of 0/negative would busy-loop, and a
      // base above the cap makes the doubling in scheduleReconnect pointless and
      // risks int overflow. Bounding the base keeps every backoff value <= the cap.
      m_reconnectBaseMs = qBound(1, ms, kReconnectMaxMs);
      m_reconnectNextMs = m_reconnectBaseMs;
  }

  void AccessControlService::enable(const ProviderDescriptor &descriptor,
                                    const QVariantMap &config)
  {
      if (m_enabled)
          return;

      m_provider = m_factory->create(descriptor, config, this);   // parented to this
      if (!m_provider)
          return;   // unknown provider id — stay disabled

      // Capture the specific provider instance so a stale (e.g. queued) event from
      // a previous provider — after a disable()/enable() cycle — is dropped instead
      // of being republished or attributed to the new provider. Also stamp the
      // emitting provider's id onto every republished event so bus consumers always
      // know the provenance (adapters may leave AccessEvent::providerId empty).
      IAccessProvider *const p = m_provider;
      const QString providerId = m_provider->descriptor().providerId;
      connect(m_provider, &IAccessProvider::accessEvent, this,
              [this, p, providerId](AccessEvent e) {
                  if (!m_enabled || p != m_provider)
                      return;
                  e.providerId = providerId;   // stamp provenance on republish
                  m_bus->publish(e);
              });
      connect(m_provider, &IAccessProvider::stateChanged, this,
              &AccessControlService::onProviderState);
      connect(m_provider, &IAccessProvider::hardwareError, this,
              &AccessControlService::onHardwareError);

      m_enabled = true;
      m_reconnectNextMs = m_reconnectBaseMs;
      emit enabledChanged(true);
      m_provider->start();
  }

  void AccessControlService::disable()
  {
      if (!m_enabled)
          return;

      // Flip the flag and sever the provider's signals BEFORE stopping it, so the
      // provider's own Disconnected transition during teardown is NOT mistaken for
      // an unexpected drop (which would schedule a reconnect). Any transition already
      // queued from the provider is also ignored, because onProviderState and the
      // republish lambda both bail when !m_enabled.
      m_enabled = false;
      m_reconnectTimer->stop();
      if (m_provider) {
          disconnect(m_provider, nullptr, this, nullptr);
          m_provider->stop();
          m_provider->deleteLater();   // parented to this, torn down cleanly
          m_provider = nullptr;
      }
      m_state = ConnectionState::Disconnected;
      emit enabledChanged(false);
      emit connectionStateChanged(m_state);
  }

  void AccessControlService::onProviderState(ConnectionState state)
  {
      // Guard against a late/stale delivery: once disabled (provider torn down) a
      // still-queued transition must not run — it would deref a null provider or
      // emit after shutdown. sender() pins the event to the CURRENT provider, so a
      // transition from a previous provider (after a disable()/enable() cycle) is
      // dropped rather than applied to the new one. (v1 is single-threaded per the
      // affinity note; these checks also make a future cross-thread provider safe.)
      if (!m_enabled || !m_provider || sender() != m_provider)
          return;

      m_state = state;
      m_health->recordState(m_provider->descriptor().providerId, state);
      emit connectionStateChanged(state);

      switch (state) {
      case ConnectionState::Connected:
          m_reconnectNextMs = m_reconnectBaseMs;   // reset backoff on success
          m_reconnectTimer->stop();                // cancel any pending reconnect
          // A successful connect IS a real comm moment — record the time. Latency
          // stays -1 (unknown) until the verify/decision path measures a real one;
          // never fabricate a zero.
          m_health->recordCommTime(m_provider->descriptor().providerId,
                                   QDateTime::currentDateTimeUtc());
          publishControllerEvent(AccessEvent::Type::ControllerConnected);
          break;
      case ConnectionState::Disconnected:
          // An unexpected drop while enabled (teardown severs signals first, so this
          // is never the intentional disable() path): report it and try to recover.
          publishControllerEvent(AccessEvent::Type::ControllerDisconnected);
          scheduleReconnect();
          break;
      case ConnectionState::Degraded:
      case ConnectionState::Error:
          scheduleReconnect();
          break;
      case ConnectionState::Connecting:
          break;
      }
  }

  void AccessControlService::onHardwareError(const QString &message)
  {
      if (!m_enabled || !m_provider || sender() != m_provider)
          return;
      AccessEvent e;
      e.type = AccessEvent::Type::HardwareError;
      e.reason = message;
      e.at = QDateTime::currentDateTimeUtc();
      e.providerId = m_provider->descriptor().providerId;   // provider identity, not a gate
      m_bus->publish(e);
  }

  void AccessControlService::publishControllerEvent(AccessEvent::Type type)
  {
      AccessEvent e;
      e.type = type;
      e.at = QDateTime::currentDateTimeUtc();
      if (m_provider)
          e.providerId = m_provider->descriptor().providerId;   // provider identity, not a gate
      m_bus->publish(e);
  }

  void AccessControlService::scheduleReconnect()
  {
      if (!m_enabled)
          return;
      m_reconnectTimer->start(m_reconnectNextMs);
      // Exponential backoff, capped.
      m_reconnectNextMs = qMin(m_reconnectNextMs * 2, kReconnectMaxMs);
  }

  } // namespace AccessControl
  ```
- [ ] Run it, expect PASS: `cmake --build qt-app/build --target tst_accesscontrolservice && ctest --test-dir qt-app/build -R tst_accesscontrolservice --output-on-failure` — 7 slots pass.
- [ ] Commit:
  ```bash
  git add qt-app/core/accesscontrol/accesscontrolservice.h qt-app/core/accesscontrol/accesscontrolservice.cpp qt-app/tests/tst_accesscontrolservice.cpp qt-app/tests/CMakeLists.txt
  git commit -m "feat(accesscontrol): add AccessControlService lifecycle owner

  Disabled by default (accessControl.enabled == false). enable() builds the
  provider via the factory, republishes its accessEvents onto the EventBus, and
  runs a Disconnected->Connecting->Connected->Degraded/Error state machine with
  exponential-backoff reconnect, recording every transition on the HealthMonitor.
  disable() tears the provider down cleanly via the parent-owned Qt tree."
  ```

---

## Task 8 — witscore CMake integration + full-suite green

**Files**
- Modify: `qt-app/core/CMakeLists.txt` (add every `accesscontrol/*.{h,cpp}` to the `witscore` target so the library — and thus `WITSQuick`/`WITS` — compiles the seam).

**Interfaces**
- Consumes: all sources from Tasks 1–7.
- Produces: the `witscore` static library now contains the access-control seam; no new public API beyond the headers already added.

> Rationale: Tasks 1–7 compile each seam source *directly* into its own test target, so the tests are green before this task. This task folds the same sources into the shared `witscore` library so the application binaries link them once and future sub-plans (kiosk/admin wiring) can `#include "accesscontrol/..."` from any `witscore` consumer. AUTOMOC is already ON for the `witscore` target, so the new `Q_OBJECT` classes (`EventBus`, `IAccessProvider`, `MockProvider`, `AccessDecisionService`, `HealthMonitor`, `AccessControlService`) are moc'd automatically.
>
> Metatype registration is self-contained: `EventBus`'s constructor calls `AccessControl::registerMetaTypes()`, so registration happens the moment any access-control flow spins up a bus, and constructing a bus force-links `accesstypes.o` into the binary — the registration cannot be elided from the static library. (`registerMetaTypes()` is idempotent and the `Q_CONSTRUCTOR_FUNCTION` in `accesstypes.cpp` is an additional backstop; tests also call it in `initTestCase`.) A later sub-plan that wires the seam into an app surface therefore needs no separate registration step — it gets one for free by owning an `EventBus`.

**Steps**

- [ ] Add the seam sources to the `witscore` target in `qt-app/core/CMakeLists.txt` — insert these lines inside the `add_library(witscore STATIC ...)` list, just before the closing `)` (after the `rfideventfilterbase.h rfideventfilterbase.cpp` line):
  ```cmake
      accesscontrol/accesstypes.h accesscontrol/accesstypes.cpp
      accesscontrol/eventbus.h accesscontrol/eventbus.cpp
      accesscontrol/iaccessprovider.h
      accesscontrol/mockprovider.h accesscontrol/mockprovider.cpp
      accesscontrol/accessproviderfactory.h accesscontrol/accessproviderfactory.cpp
      accesscontrol/accessdecisionservice.h accesscontrol/accessdecisionservice.cpp
      accesscontrol/healthmonitor.h accesscontrol/healthmonitor.cpp
      accesscontrol/accesscontrolservice.h accesscontrol/accesscontrolservice.cpp
  ```
  > `witscore` already lists `core/` PUBLIC on its include path, so `#include "accesscontrol/eventbus.h"` and the in-seam `#include "apiconfig.h"` both resolve for every consumer. `Qt::Network` is already a PUBLIC dependency of `witscore` (needed by `AccessDecisionService`).
- [ ] Reconfigure and build the whole project, expect PASS (library + app link the new sources, no new warnings): `cmake -S qt-app -B qt-app/build && cmake --build qt-app/build`.
- [ ] Run the FULL test suite, expect PASS (all pre-existing tests plus the 7 new access-control targets green): `ctest --test-dir qt-app/build --output-on-failure`.
- [ ] Commit:
  ```bash
  git add qt-app/core/CMakeLists.txt
  git commit -m "build(accesscontrol): compile the access-control seam into witscore

  Fold the access-control sources into the witscore static library so the app
  binaries link the seam once and future kiosk/admin sub-plans can include it
  from any witscore consumer. AUTOMOC (already on for witscore) handles the new
  Q_OBJECT classes; Qt::Network is already a public witscore dependency."
  ```

---

## Self-Review

**Spec coverage** — every in-scope item maps to a task:
- Value types (`Credential`, `AccessDecision` with Granted/Denied/Error, `AccessEvent` with all 7 types incl. `EntryObserved`, `GateDescriptor`, `ProviderDescriptor` + `ConfigFieldDescriptor`, `ConnectionState`) → **T1** (+ `HealthSnapshot` in **T6**).
- `EventBus` typed pub/sub → **T2**.
- `IAccessProvider` interface + `MockProvider` (each event type independently) → **T3**.
- `AccessProviderFactory` register/available/create → **T4**.
- `AccessDecisionService` verify Granted/Denied/**Error-on-timeout** with injected NAM → **T5** (dedicated `verifyTimeoutEmitsError` test).
- `HealthMonitor` + `HealthSnapshot` → **T6**.
- `AccessControlService` enable/disable, provider→bus wiring, state machine + backoff, flag OFF default → **T7**.
- witscore CMake integration + full ctest green → **T8**.
- Out-of-scope items (PHP endpoints, `TurnstileProvider`, `LoginParser::parseEntryEvent`, `KioskViewModel`, admin `AccessControlViewModel`/`AccessControlScreen`/`AdminScreen`/`Navigator`) are named only as "consumed by later sub-plans" in the relevant Interfaces blocks — none are planned here.

**Placeholder scan** — no `TODO`, "add error handling", "similar to Task N", or elided bodies. Every code step contains complete, compilable C++ and complete QtTest slots with real assertions. Every `wits_add_qttest()` block lists real source paths; the two CMake edits show the exact lines to insert.

**Type consistency** — signatures are identical wherever a type crosses a task boundary: `AccessEvent` (with both `gateId` and `providerId`), `AccessDecision`, `ConnectionState`, `ProviderDescriptor`, and `HealthSnapshot` are defined once in `accesstypes.h` (T1/T6) and consumed by value/const-ref everywhere; `IAccessProvider`'s three signals match the `connect` targets in `AccessControlService` (T7); `AccessProviderFactory::CreatorFn` matches the lambdas used in the T4 and T7 tests; `AccessDecisionService::decodeResponse`/`verify`/`decided` signatures match their test call sites (T5); `HealthMonitor::recordComm` (with latency) and `recordCommTime` (time-only) are both consumed by T7. All new `QObject`s are parent-owned (constructors take `QObject *parent`; `AccessControlService` parents `HealthMonitor`, the reconnect `QTimer`, and the factory-built provider to itself; `AccessDecisionService` binds each reply's `deleteLater` to the reply itself, so a reply may safely finish after the service is destroyed — the `this`-context decode handler is auto-disconnected on destruction, preventing a late callback, and the reply-bound cleanup prevents any leak), the factory reparents any provider a creator leaves unowned, and every `connect` uses function-pointer or lambda syntax — consistent with the Global Constraints.

**Invariants enforced by tests:**
- **decision ≠ entry** — `entryObservedIsDistinctFromGranted` (T1), `simulateEmitsEachEventTypeIndependently` (T3).
- **timeout = Error, transport failure = Error (never a fabricated Denied)** — `verifyTimeoutEmitsError` (asserts the timeout *reason*, proving the abort path) and `verifyTransportFailureEmitsError` (T5); malformed body → Error via `decodeInvalidBodyIsError`; a `granted` with no `subject_id` → Error via `decodeGrantedWithoutSubjectIsError` (T5).
- **request shape** — `verifyGrantedEmitsDecidedGrantedAndSendsRequest` asserts method/URL/content-type/body via `CapturingNam` (T5).
- **flag-OFF default is real** — `defaultsToDisabled` proves the factory creator was never called and the bus stayed silent (T7).
- **an AccessEvent survives a queued hop** — `accessEventSurvivesQueuedConnection` exercises a real `Qt::QueuedConnection` (T1); auto-registration via `Q_CONSTRUCTOR_FUNCTION`.
- **EventBus nested/re-entrant publish** — `nestedPublishFromSubscriberIsDeliveredDepthFirst` (T2).
- **provider ownership enforced** — `createEnforcesParentWhenCreatorIgnoresIt` (T4); `MockProvider::start()` idempotency — `startIsIdempotentWhenConnected` (T3).
- **reconnect on degrade / unexpected drop, and cancel-on-connect** — `degradedSchedulesReconnect` (Degraded → reconnect → Connected), `unexpectedDisconnectPublishesAndReconnects` (enabled-provider Disconnected → ControllerDisconnected + reconnect), and `reconnectTimerCancelledWhenConnected` (arms a long-backoff timer, then a self-recovery to Connected must disarm it — the deterministic cancel-on-connect proof) (T7).
- **no fabricated latency** — `recordCommTimeLeavesLatencyUnknown` keeps `latencyMs == -1` (T6).

- **provenance + ownership** — the service stamps `providerId` on every republished event (`providerEventIsRepublishedOnBus` asserts it, T7); the factory refuses a null parent (`createWithNullParentReturnsNull`, T4) and reparents a provider a creator left unowned (`createEnforcesParentWhenCreatorIgnoresIt`, T4).
- **in-flight verify cancellation** — `cancelDropsPendingDecision` proves a decision arriving after `cancel()` is never emitted (T5), the contract a capture adapter uses on `stop()`.

**Structurally enforced (not a runtime test), by design:** the bus carries only decided events — `EventBus::publish` accepts only `AccessEvent`, and no bus type embeds a `Credential`; a raw `Credential` exists only as the input to `AccessDecisionService::verify` and inside an adapter, so it can never reach a subscriber. The seven `AccessEvent` types are emitted across the seam as follows: `MockProvider` emits `AccessGranted`/`AccessDenied`/`AccessError`/`EntryObserved` directly; `HardwareError`, `ControllerConnected`, and `ControllerDisconnected` are **synthesized by `AccessControlService`** — the mock emits a `hardwareError(QString)` signal and raw state transitions, and the service turns those into the corresponding `AccessEvent`s (verified in T7). The mock does **not** emit `HardwareError` (or the controller events) as an `AccessEvent` itself.

**Enforced by implementation (not a dedicated test):** the source-provider guard that drops a stale event from a superseded provider (captured-instance / `sender()` checks in the republish lambda, `onProviderState`, and `onHardwareError`). In the single-thread v1 there is no way to deterministically construct a queued cross-provider delivery, so this defensive check is verified by inspection; it becomes directly testable once (and if) a cross-thread provider is added.
