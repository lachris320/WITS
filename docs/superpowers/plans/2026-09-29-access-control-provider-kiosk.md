# TurnstileProvider + Native Kiosk Display Implementation Plan (Sub-plan 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a real turnstile swipe surface the student natively on the LOAMS 2.0 (`WITSQuick`) kiosk, retiring the PowerShell bridge, behind `accessControl.enabled` (default off = zero behavior change).

**Architecture:** A pure `LoginParser::parseEntryEvent` decodes `turnstile_display.php`; a server-observed `TurnstileProvider` (an `IAccessProvider`) polls that endpoint through an injected `QNetworkAccessManager`, tracks an oldest-next cursor, and emits `EntryObserved` `AccessEvent`s; an application-owned `AccessControlHub` composition root (exposed to QML as the `AccessControl` singleton via a `QML_FOREIGN` wrapper) owns the `EventBus`/`AccessProviderFactory`/`AccessControlService`, and maps `EntryObserved` to a QML-facing `entryObserved(QVariantMap)`; a presentation-scoped `Connections { target: AccessControl }` on the kiosk surface forwards to a new `KioskViewModel::onEntryObserved` slot.

**Tech Stack:** Qt 6 / C++17, CMake + Ninja, QtTest (`wits_add_qttest`), QML/Qt Quick (MVVM), PHP 8.2 backend (already merged, read-only).

## Global Constraints

- **Feature flag:** every runtime effect is behind `accessControl.enabled` (default **false**). Flag off ⇒ no provider built, no polling, no bus traffic, inert singleton — zero behavior change.
- **Read-only:** the provider **never** POSTs. `turnstile.php` remains the sole authoritative attendance writer.
- **Privacy:** raw `card` and `reader` never leave the adapter — never on `AccessEvent`, the `EventBus`, or the QML signal.
- **Settings:** all reads go through **`AppSettings`** (never raw `QSettings`) so tests isolate via `AppSettings::isolateForTesting()`. Keys: `accessControl/enabled` (bool), `accessControl/pollIntervalMs` (int), `accessControl/gateId` (string).
- **Enablement precedence:** `WITS_ACCESS_CONTROL` = `1`/`true` (case-insensitive) force-on → else `accessControl/enabled` → else false. `WITS_ACCESS_CONTROL=0` is **not** a force-off (it falls through to the setting).
- **`pollIntervalMs` normalization (single owner = `TurnstileProvider::clampPollMs`):** a value `<= 0` (absent/invalid) → `1500`; a valid positive value below the `250` ms floor → clamped to `250`; otherwise used as-is.
- **`gateId` normalization (single owner = the provider):** trimmed; empty/whitespace → `"turnstile"`.
- **Integer widths:** `latest_id`, `eventId`, and the poll cursor are `qint64`.
- **No Claude/Anthropic attribution** in any commit message (standing user rule).
- **QObject/ownership:** parent every `QObject`; smart pointers only for the deliberately non-parented hub members (the sanctioned exception). Function-pointer `connect` syntax.

### Build & test commands (this machine)

Qt tools are **not** on `PATH`, and the in-tree build path overflows Windows MAX_PATH for the QML module — build into a **short external dir**. Prepend the kit in the **same** command (shell state does not persist). PowerShell:

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target <target>
ctest --test-dir C:/b/loams-sp3 -R <name> --output-on-failure
```

Ignore the harmless `LF will be replaced by CRLF` and the pre-existing `QXlsx ... GuiPrivate` CMake warnings.

---

## File Structure

| File | Responsibility | Task |
|---|---|---|
| `qt-app/core/loginparser.h` / `.cpp` | Add `EntryEventResult` + pure `parseEntryEvent(QByteArray, QUrl)` | 1 |
| `qt-app/tests/tst_loginparser.cpp` | Add `parseEntryEvent` cases | 1 |
| `qt-app/core/accesscontrol/turnstileprovider.h` / `.cpp` | Server-observed `IAccessProvider`: poll/cursor/timeout/generation; `clampPollMs` | 2 |
| `qt-app/core/accesscontrol/accesstypes.h` | Update the `subject` comment to state the empty-subject convention | 2 |
| `qt-app/testsupport/sequencednam.h` / `.cpp` | Reusable multi-response fake NAM (auto-finish, stall, abort count) | 2 |
| `qt-app/tests/tst_turnstileprovider.cpp` | Provider behavior tests | 2 |
| `qt-app/core/CMakeLists.txt` | Add `turnstileprovider.*` to `witscore` | 2 |
| `qt-app/tests/CMakeLists.txt` | Register `tst_turnstileprovider` | 2 |
| `qt-app/quick/AccessControlHub.h` / `.cpp` | App-owned composition root; event→`QVariantMap` map; config/enablement | 3 |
| `qt-app/quick/AccessControlSingleton.h` | `QML_FOREIGN` singleton wrapper exposing the hub as `AccessControl` | 3 |
| `qt-app/quick/tests/tst_accesscontrolhub.cpp` | Hub mapping/enablement/emission tests | 3 |
| `qt-app/quick/CMakeLists.txt` | Add hub + wrapper to `witsquickmodule` SOURCES; register `tst_accesscontrolhub` | 3 |
| `qt-app/quick/viewmodels/KioskViewModel.h` / `.cpp` | `Q_INVOKABLE onEntryObserved(QVariantMap)` + `showUnknownEntry()` | 4 |
| `qt-app/quick/tests/tst_kioskviewmodel.cpp` | Slot branch tests | 4 |
| `qt-app/quick/main.cpp` | Construct hub, `initialize()`, `setInstance()` before the engine | 5 |
| `qt-app/quick/qml/kiosk/KioskScreen.qml` | `Connections { target: AccessControl }` → `kioskVm.onEntryObserved` | 5 |
| `qt-app/quick/tests/tst_appshell.cpp` | Install a disabled hub before load | 5 |
| `qt-app/quick/tests/tst_qml_kiosk.cpp` | Install a disabled hub in `Setup` | 5 |

---

## Task 1: `LoginParser::parseEntryEvent` (pure decoder)

**Files:**
- Modify: `qt-app/core/loginparser.h`, `qt-app/core/loginparser.cpp`
- Test: `qt-app/tests/tst_loginparser.cpp`

**Interfaces:**
- Consumes: nothing new (pure).
- Produces: `LoginParser::EntryEventResult { bool valid; bool hasEntry; qint64 latestId; qint64 eventId; bool hasStudent; QJsonObject student; QDateTime at; QString error; }` and `EntryEventResult LoginParser::parseEntryEvent(const QByteArray &body, const QUrl &baseUrl);`. The `student` object always carries a `photo_url` string key.

- [ ] **Step 1: Write the failing tests**

Append to `qt-app/tests/tst_loginparser.cpp` — add these under the existing `private slots:` block, and the implementations below the existing ones. Add `#include <QUrl>` near the top if absent.

```cpp
// --- in the private slots: block ---
void parseEntryEvent_emptyPoll();
void parseEntryEvent_entryWithStudentComposesPhotoUrl();
void parseEntryEvent_photoUrlPassthrough();
void parseEntryEvent_noPhotoYieldsEmptyPhotoUrl();
void parseEntryEvent_orphanedStudentNull();
void parseEntryEvent_localTimeConvertedToUtc();
void parseEntryEvent_malformedIsInvalid();
```

```cpp
// --- implementations ---
void TestLoginParser::parseEntryEvent_emptyPoll()
{
    const QByteArray body = R"({"status":"success","latest_id":5,"entry":null})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.valid);
    QVERIFY(!r.hasEntry);
    QCOMPARE(r.latestId, Q_INT64_C(5));
}

void TestLoginParser::parseEntryEvent_entryWithStudentComposesPhotoUrl()
{
    const QByteArray body = R"({"status":"success","latest_id":11,"entry":{
        "id":11,"card":"ABC","created_at":"2026-09-29 08:30:00","reader":0,
        "student":{"name":"Jane Cruz","photo_path":"uploads/jane.jpg"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.valid);
    QVERIFY(r.hasEntry);
    QCOMPARE(r.eventId, Q_INT64_C(11));
    QVERIFY(r.hasStudent);
    QCOMPARE(r.student.value("photo_url").toString(),
             QStringLiteral("http://localhost/loams_api/uploads/jane.jpg"));
    QVERIFY(!r.student.contains("card"));   // raw card never surfaced
}

void TestLoginParser::parseEntryEvent_photoUrlPassthrough()
{
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00",
        "student":{"name":"A","photo_url":"http://cdn/x.jpg","photo_path":"uploads/y.jpg"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QCOMPARE(r.student.value("photo_url").toString(), QStringLiteral("http://cdn/x.jpg"));
}

void TestLoginParser::parseEntryEvent_noPhotoYieldsEmptyPhotoUrl()
{
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00","student":{"name":"A"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.hasStudent);
    QVERIFY(r.student.contains("photo_url"));
    QVERIFY(r.student.value("photo_url").toString().isEmpty());
}

void TestLoginParser::parseEntryEvent_orphanedStudentNull()
{
    const QByteArray body = R"({"status":"success","latest_id":9,"entry":{
        "id":9,"created_at":"2026-09-29 08:30:00","student":null}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.valid);
    QVERIFY(r.hasEntry);
    QVERIFY(!r.hasStudent);
    QVERIFY(r.student.isEmpty());
}

void TestLoginParser::parseEntryEvent_localTimeConvertedToUtc()
{
    const QByteArray body = R"({"status":"success","latest_id":2,"entry":{
        "id":2,"created_at":"2026-09-29 08:30:00","student":null}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.at.isValid());
    QCOMPARE(r.at.timeSpec(), Qt::UTC);
    QDateTime local(QDate(2026, 9, 29), QTime(8, 30, 0));   // Qt::LocalTime
    QCOMPARE(r.at, local.toUTC());
}

void TestLoginParser::parseEntryEvent_malformedIsInvalid()
{
    const QUrl base("http://localhost/loams_api/");
    QVERIFY(!LoginParser::parseEntryEvent("not json", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(R"({"status":"error"})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":3})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":0,"created_at":"2026-09-29 08:30:00","student":null}})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":1,"created_at":"nope","student":null}})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":-1,"entry":null})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":1,"created_at":"2026-09-29 08:30:00","student":5}})", base).valid);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-sp3 --target tst_loginparser
ctest --test-dir C:/b/loams-sp3 -R tst_loginparser --output-on-failure
```
Expected: FAIL to compile (`parseEntryEvent`/`EntryEventResult` undeclared). `loginparser.cpp` is compiled directly by the test target, so this is a clean compile error, not a CMake-configure error.

- [ ] **Step 3: Declare the result type + function in `loginparser.h`**

Add `#include <QDateTime>` and `#include <QUrl>` to the includes, then inside `namespace LoginParser` (after `RfidResult`):

```cpp
struct EntryEventResult {
    bool        valid      = false;   // false = malformed JSON/schema (protocol failure)
    bool        hasEntry   = false;   // valid && an entry row was returned
    qint64      latestId   = 0;       // MAX(id); 0 on empty table
    qint64      eventId    = 0;       // the returned entry's id (cursor advance target)
    bool        hasStudent = false;   // entry present && student resolved
    QJsonObject student;              // normalized student incl. photo_url; empty if unresolved
    QDateTime   at;                   // entry time, UTC (converted from server-local)
    QString     error;                // reason when !valid
};

// Pure decode of turnstile_display.php. photo_url is composed from a relative
// photo_path against baseUrl (parser stays ApiConfig-free). See design spec §1.
EntryEventResult parseEntryEvent(const QByteArray &body, const QUrl &baseUrl);
```

- [ ] **Step 4: Implement in `loginparser.cpp`**

Add `#include <QJsonValue>` if absent. Append:

```cpp
LoginParser::EntryEventResult
LoginParser::parseEntryEvent(const QByteArray &body, const QUrl &baseUrl)
{
    EntryEventResult r;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) { r.error = QStringLiteral("Not a JSON object"); return r; }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        r.error = QStringLiteral("status != success"); return r;
    }
    const QJsonValue latestVal = obj.value(QStringLiteral("latest_id"));
    if (!latestVal.isDouble()) { r.error = QStringLiteral("latest_id not a number"); return r; }
    const qint64 latestId = latestVal.toInteger(-1);
    if (latestId < 0) { r.error = QStringLiteral("latest_id negative"); return r; }

    const QJsonValue entryVal = obj.value(QStringLiteral("entry"));
    if (entryVal.isNull()) {                 // valid empty poll
        r.valid = true; r.latestId = latestId; return r;
    }
    if (!entryVal.isObject()) { r.error = QStringLiteral("entry not object/null"); return r; }

    const QJsonObject entry = entryVal.toObject();
    const QJsonValue idVal = entry.value(QStringLiteral("id"));
    if (!idVal.isDouble()) { r.error = QStringLiteral("entry.id not a number"); return r; }
    const qint64 eventId = idVal.toInteger(-1);
    if (eventId <= 0) { r.error = QStringLiteral("entry.id not positive"); return r; }

    const QString createdAt = entry.value(QStringLiteral("created_at")).toString();
    QDateTime at = QDateTime::fromString(createdAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!at.isValid()) { r.error = QStringLiteral("created_at unparseable"); return r; }
    at.setTimeSpec(Qt::LocalTime);   // server-local wall clock (same-host deployment)

    const QJsonValue studentVal = entry.value(QStringLiteral("student"));
    QJsonObject student;
    bool hasStudent = false;
    if (studentVal.isObject()) {
        hasStudent = true;
        student = studentVal.toObject();
        QString photoUrl = student.value(QStringLiteral("photo_url")).toString();
        if (photoUrl.isEmpty()) {
            const QString photoPath = student.value(QStringLiteral("photo_path")).toString();
            photoUrl = photoPath.isEmpty()
                           ? QString()
                           : baseUrl.resolved(QUrl(photoPath)).toString();
        }
        student.insert(QStringLiteral("photo_url"), photoUrl);   // always present
    } else if (!studentVal.isNull()) {
        r.error = QStringLiteral("student not object/null"); return r;   // present but wrong type
    }

    r.valid = true;
    r.hasEntry = true;
    r.latestId = latestId;
    r.eventId = eventId;
    r.hasStudent = hasStudent;
    r.student = student;
    r.at = at.toUTC();
    return r;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

```
cmake --build C:/b/loams-sp3 --target tst_loginparser
ctest --test-dir C:/b/loams-sp3 -R tst_loginparser --output-on-failure
```
Expected: PASS (all `parseEntryEvent_*` plus the pre-existing cases).

- [ ] **Step 6: Commit** (via the `commit` skill) — `feat(accesscontrol): add pure parseEntryEvent decoder for turnstile_display`.

---

## Task 2: `TurnstileProvider` (server-observed IAccessProvider)

**Files:**
- Create: `qt-app/core/accesscontrol/turnstileprovider.h`, `qt-app/core/accesscontrol/turnstileprovider.cpp`
- Create: `qt-app/testsupport/sequencednam.h`, `qt-app/testsupport/sequencednam.cpp`
- Create: `qt-app/tests/tst_turnstileprovider.cpp`
- Modify: `qt-app/core/accesscontrol/accesstypes.h` (comment only), `qt-app/core/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `LoginParser::parseEntryEvent` (Task 1); `IAccessProvider`, `AccessEvent`, `ProviderDescriptor`, `ConnectionState`.
- Produces:
  - `class TurnstileProvider : public AccessControl::IAccessProvider` with ctor `TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl, const QVariantMap &config, QObject *parent = nullptr)` (reads `config["pollIntervalMs"]` int, `config["gateId"]` string, both normalized inside the provider), `void setTimeoutMs(int)`, `static ProviderDescriptor defaultDescriptor()` (`providerId == "turnstile"`), `static int clampPollMs(int raw)`.
  - `SequencedNam` (test util): `explicit SequencedNam(QObject *parent = nullptr);` · `void enqueue(const QByteArray &body, QNetworkReply::NetworkError error = QNetworkReply::NoError);` (auto-finishes next tick) · `void enqueueStall();` (a reply that finishes only when aborted) · `int requestCount() const;` · `int abortCount() const;` · `QUrl lastUrl;`.

- [ ] **Step 1: Create the reusable `SequencedNam` test double**

`qt-app/testsupport/sequencednam.h`:

```cpp
#ifndef SEQUENCEDNAM_H
#define SEQUENCEDNAM_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QUrl>

// Test-only NAM that answers each request with the NEXT enqueued canned
// response. enqueue() finishes on the next event-loop turn; enqueueStall()
// finishes only when the reply is aborted (drives timeout/stop/generation
// paths). Tracks request + abort counts. No live network.
class SequencedNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit SequencedNam(QObject *parent = nullptr);
    void enqueue(const QByteArray &body,
                 QNetworkReply::NetworkError error = QNetworkReply::NoError);
    void enqueueStall();
    int requestCount() const { return m_requestCount; }
    int abortCount() const { return m_abortCount; }
    void noteAbort() { ++m_abortCount; }
    QUrl lastUrl;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    struct Canned { QByteArray body; QNetworkReply::NetworkError error; bool stall; };
    QQueue<Canned> m_queue;
    int m_requestCount = 0;
    int m_abortCount = 0;
};

#endif // SEQUENCEDNAM_H
```

`qt-app/testsupport/sequencednam.cpp`:

```cpp
#include "sequencednam.h"

#include <QBuffer>
#include <QPointer>
#include <QTimer>
#include <QNetworkRequest>

namespace {
class CannedReply : public QNetworkReply
{
public:
    CannedReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req,
                const QByteArray &body, QNetworkReply::NetworkError error,
                bool stall, SequencedNam *owner)
        : QNetworkReply(owner), m_body(body), m_owner(owner)
    {
        setRequest(req);
        setUrl(req.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        m_buffer.setData(m_body);
        m_buffer.open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,
                     error == QNetworkReply::NoError ? 200 : 500);
        if (!stall) {                          // auto-finish next tick
            QTimer::singleShot(0, this, [this, error]() {
                if (error != QNetworkReply::NoError) {
                    setError(error, QStringLiteral("canned error"));
                    emit errorOccurred(error);
                }
                setFinished(true);
                emit finished();
            });
        }
        // stall: finishes only via abort()
    }
    void abort() override
    {
        if (isFinished()) return;
        if (m_owner) m_owner->noteAbort();
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted"));
        emit errorOccurred(QNetworkReply::OperationCanceledError);
        setFinished(true);
        emit finished();
    }
    qint64 readData(char *data, qint64 maxlen) override { return m_buffer.read(data, maxlen); }
    qint64 bytesAvailable() const override
    { return m_buffer.bytesAvailable() + QNetworkReply::bytesAvailable(); }
private:
    QByteArray m_body;
    QBuffer m_buffer;
    QPointer<SequencedNam> m_owner;
};
} // namespace

SequencedNam::SequencedNam(QObject *parent) : QNetworkAccessManager(parent) {}

void SequencedNam::enqueue(const QByteArray &body, QNetworkReply::NetworkError error)
{ m_queue.enqueue({body, error, false}); }

void SequencedNam::enqueueStall()
{ m_queue.enqueue({QByteArray(), QNetworkReply::NoError, true}); }

QNetworkReply *SequencedNam::createRequest(Operation op, const QNetworkRequest &request,
                                           QIODevice *)
{
    ++m_requestCount;
    lastUrl = request.url();
    Canned c = m_queue.isEmpty()
                   ? Canned{QByteArrayLiteral("{\"status\":\"success\",\"latest_id\":0,\"entry\":null}"),
                            QNetworkReply::NoError, false}
                   : m_queue.dequeue();
    return new CannedReply(op, request, c.body, c.error, c.stall, this);
}
```

- [ ] **Step 2: Create the provider header + a skeleton `.cpp`**

Creating both files now (before touching CMake) keeps the red step a runtime assertion failure, not a CMake-configure failure over a missing source.

`qt-app/core/accesscontrol/turnstileprovider.h`:

```cpp
#ifndef ACCESSCONTROL_TURNSTILEPROVIDER_H
#define ACCESSCONTROL_TURNSTILEPROVIDER_H

#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include "accesscontrol/iaccessprovider.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace AccessControl {

// Server-observed provider: polls turnstile_display.php (oldest-next cursor)
// through an INJECTED, not-owned NAM and emits EntryObserved AccessEvents.
// Baselines the cursor to latest_id on the FIRST start only; reconnect starts
// preserve the cursor. Failures emit Degraded (the service owns reconnect).
class TurnstileProvider : public IAccessProvider
{
    Q_OBJECT
public:
    TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                      const QVariantMap &config, QObject *parent = nullptr);
    ~TurnstileProvider() override;

    static ProviderDescriptor defaultDescriptor();
    static int clampPollMs(int raw);   // <=0 -> 1500; positive -> max(raw, 250)

    ProviderDescriptor descriptor() const override;
    void start() override;
    void stop() override;
    ConnectionState state() const override { return m_state; }

    void setTimeoutMs(int ms) { m_timeoutMs = ms; }

private:
    void sendPoll();
    void onFinished(QNetworkReply *reply, quint64 gen);
    void armTimer();
    void setState(ConnectionState s);
    void fail();

    QNetworkAccessManager *m_nam;   // injected, not owned
    QUrl m_baseUrl;
    QString m_gateId;
    int m_pollIntervalMs = 1500;
    int m_timeoutMs = 5000;

    qint64 m_since = 0;
    bool m_baselined = false;
    ConnectionState m_state = ConnectionState::Disconnected;
    QTimer *m_pollTimer;            // single-shot, parented to this
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation = 0;
};

} // namespace AccessControl

#endif // ACCESSCONTROL_TURNSTILEPROVIDER_H
```

`qt-app/core/accesscontrol/turnstileprovider.cpp` — **skeleton** (compiles + links; behaviour deliberately absent so the tests go red):

```cpp
#include "accesscontrol/turnstileprovider.h"
#include <QTimer>

namespace AccessControl {

TurnstileProvider::TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                                     const QVariantMap &config, QObject *parent)
    : IAccessProvider(parent), m_nam(nam), m_baseUrl(std::move(baseUrl))
    , m_pollTimer(new QTimer(this))
{
    Q_UNUSED(config);
}
TurnstileProvider::~TurnstileProvider() {}
int TurnstileProvider::clampPollMs(int) { return 1500; }               // stub
ProviderDescriptor TurnstileProvider::defaultDescriptor() { return {}; } // stub
ProviderDescriptor TurnstileProvider::descriptor() const { return {}; }
void TurnstileProvider::start() {}                                      // stub
void TurnstileProvider::stop() {}
void TurnstileProvider::sendPoll() {}
void TurnstileProvider::onFinished(QNetworkReply *, quint64) {}
void TurnstileProvider::armTimer() {}
void TurnstileProvider::setState(ConnectionState) {}
void TurnstileProvider::fail() {}

} // namespace AccessControl
```

- [ ] **Step 3: Write the provider tests**

`qt-app/tests/tst_turnstileprovider.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "sequencednam.h"
#include "accesscontrol/turnstileprovider.h"
#include "accesscontrol/accesstypes.h"

using namespace AccessControl;

class TestTurnstileProvider : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void clampPollMs_rules();
    void blankGateIdFallsBack();
    void baselineSkipsHistoryThenPolls();
    void emptyPollReArmsNextPoll();
    void drainsEntriesOldestFirst();
    void reconnectPreservesCursorAndEmitsReconnectEntry();
    void nonAdvancingEntryDegrades();
    void malformedResponseDegrades();
    void transportFailureEmitsExactlyOneDegraded();
    void timeoutAbortsAndDegrades();
    void stopAbortsInFlightNoEmit();
    void restartDropsInFlightGeneration();

private:
    static QVariantMap cfg(int pollMs = 250, const QString &gate = QStringLiteral("g1"))
    { return QVariantMap{{"pollIntervalMs", pollMs}, {"gateId", gate}}; }
    static QByteArray entryPayload(qint64 latest, qint64 id)
    {
        return QStringLiteral(
            "{\"status\":\"success\",\"latest_id\":%1,\"entry\":{\"id\":%2,"
            "\"card\":\"C\",\"created_at\":\"2026-09-29 08:30:00\",\"reader\":0,"
            "\"student\":{\"name\":\"A\",\"photo_path\":\"uploads/a.jpg\"}}}")
            .arg(latest).arg(id).toUtf8();
    }
    static QByteArray emptyPayload(qint64 latest)
    {
        return QStringLiteral("{\"status\":\"success\",\"latest_id\":%1,\"entry\":null}")
            .arg(latest).toUtf8();
    }
    static int degradedCount(const QSignalSpy &states)
    {
        int n = 0;
        for (const auto &args : states)
            if (qvariant_cast<ConnectionState>(args.at(0)) == ConnectionState::Degraded) ++n;
        return n;
    }
};

void TestTurnstileProvider::clampPollMs_rules()
{
    QCOMPARE(TurnstileProvider::clampPollMs(0), 1500);      // absent/invalid
    QCOMPARE(TurnstileProvider::clampPollMs(-5), 1500);
    QCOMPARE(TurnstileProvider::clampPollMs(100), 250);     // below floor -> clamp
    QCOMPARE(TurnstileProvider::clampPollMs(2000), 2000);   // valid -> as-is
}

void TestTurnstileProvider::blankGateIdFallsBack()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(0));                 // baseline
    nam.enqueue(entryPayload(1, 1));              // poll -> entry 1
    nam.enqueue(emptyPayload(1));
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"),
                        cfg(250, QStringLiteral("  ")));   // whitespace gateId
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(0).at(0)).gateId,
             QStringLiteral("turnstile"));         // fell back
    p.stop();
}

void TestTurnstileProvider::baselineSkipsHistoryThenPolls()
{
    SequencedNam nam;
    nam.enqueue(entryPayload(5, 5));   // baseline: has history, must NOT be emitted
    nam.enqueue(emptyPayload(5));      // first steady poll: empty
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);  // baseline + one steady poll
    QCOMPARE(events.count(), 0);                              // history not emitted
    QCOMPARE(p.state(), ConnectionState::Connected);
    QVERIFY(nam.lastUrl.query().contains(QStringLiteral("since=5")));
    p.stop();
}

void TestTurnstileProvider::emptyPollReArmsNextPoll()
{
    // A steady-state empty poll must re-arm the timer for the NEXT poll (not
    // stop after one). Baseline empty + two steady empties => >= 3 requests.
    SequencedNam nam;
    nam.enqueue(emptyPayload(5));   // baseline
    nam.enqueue(emptyPayload(5));   // steady poll #1 (empty -> re-arm)
    nam.enqueue(emptyPayload(5));   // steady poll #2 (only reached if #1 re-armed)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() >= 3, 3000);
    QCOMPARE(events.count(), 0);
    p.stop();
}

void TestTurnstileProvider::drainsEntriesOldestFirst()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(0));       // baseline: cursor = 0
    nam.enqueue(entryPayload(2, 1));    // poll -> entry 1
    nam.enqueue(entryPayload(2, 2));    // drain -> entry 2
    nam.enqueue(emptyPayload(2));       // drain end
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 2, 3000);
    const auto e1 = qvariant_cast<AccessEvent>(events.at(0).at(0));
    QCOMPARE(e1.type, AccessEvent::Type::EntryObserved);
    QCOMPARE(e1.correlationId, QStringLiteral("1"));
    QCOMPARE(e1.gateId, QStringLiteral("g1"));
    QCOMPARE(e1.subject.value("name").toString(), QStringLiteral("A"));
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(1).at(0)).correlationId, QStringLiteral("2"));
    // One-in-flight: requests happen strictly in sequence (baseline + 2 entries
    // + drain-end empty == 4); more than one reply active would break ordering.
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 4, 3000);
    p.stop();
}

void TestTurnstileProvider::reconnectPreservesCursorAndEmitsReconnectEntry()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(3));       // baseline: cursor = 3
    nam.enqueue(emptyPayload(3));       // steady poll: empty (arms timer)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);
    p.stop();

    nam.enqueue(entryPayload(4, 4));    // reconnect ?since=3 -> entry 4 (must emit once)
    nam.enqueue(emptyPayload(4));       // drain end
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(0).at(0)).correlationId, QStringLiteral("4"));
    QVERIFY(nam.lastUrl.query().contains(QStringLiteral("since=4")));
    p.stop();
}

void TestTurnstileProvider::nonAdvancingEntryDegrades()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(7));       // baseline: cursor = 7
    nam.enqueue(entryPayload(7, 7));    // poll returns id == cursor (non-advancing)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(events.count(), 0);
    p.stop();
}

void TestTurnstileProvider::malformedResponseDegrades()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(1));                  // baseline ok, cursor = 1
    nam.enqueue(QByteArrayLiteral("not json"));    // poll: malformed -> protocol failure
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(events.count(), 0);
    p.stop();
}

void TestTurnstileProvider::transportFailureEmitsExactlyOneDegraded()
{
    SequencedNam nam;
    nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);   // baseline fails
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(degradedCount(states), 1);            // exactly one Degraded per failure
    p.stop();
}

void TestTurnstileProvider::timeoutAbortsAndDegrades()
{
    SequencedNam nam;
    nam.enqueueStall();                            // baseline never finishes on its own
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    p.setTimeoutMs(50);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QVERIFY(nam.abortCount() >= 1);                // timeout timer aborted the reply
    p.stop();
}

void TestTurnstileProvider::stopAbortsInFlightNoEmit()
{
    SequencedNam nam;
    nam.enqueueStall();                            // baseline in flight, never auto-finishes
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 1, 3000);
    p.stop();                                      // aborts the in-flight reply
    QCOMPARE(nam.abortCount(), 1);
    QTest::qWait(100);                             // let the aborted reply's finished fire
    QCOMPARE(events.count(), 0);                   // stale (generation-bumped) reply dropped
    QVERIFY(p.state() != ConnectionState::Connected);
}

void TestTurnstileProvider::restartDropsInFlightGeneration()
{
    // A restart (e.g. the service reconnect) while a reply is in flight bumps
    // the generation: the stalled reply-1 must be aborted and its late finish
    // dropped, while the provider baselines from reply-2 exactly once.
    SequencedNam nam;
    nam.enqueueStall();             // start #1: baseline stalls in flight (gen 1)
    nam.enqueue(emptyPayload(9));   // start #2: baseline responds (gen 2), cursor = 9
    nam.enqueue(emptyPayload(9));   // steady poll after baseline
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 1, 3000);   // reply-1 in flight
    p.start();                                                // restart: gen bump + abort reply-1
    QVERIFY(nam.abortCount() >= 1);
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Connected, 3000);  // baselined from reply-2
    QCOMPARE(events.count(), 0);                              // reply-1 never produced an effect
    QTRY_VERIFY_WITH_TIMEOUT(nam.lastUrl.query().contains(QStringLiteral("since=9")), 3000);
    p.stop();
}

QTEST_MAIN(TestTurnstileProvider)
#include "tst_turnstileprovider.moc"
```

- [ ] **Step 4: Register the sources + test in CMake**

In `qt-app/core/CMakeLists.txt`, add to the `witscore` source list (after `accesscontrol/accesscontrolservice.*`):

```cmake
    accesscontrol/turnstileprovider.h accesscontrol/turnstileprovider.cpp
```

In `qt-app/tests/CMakeLists.txt`, after the `tst_accesscontrolservice` block:

```cmake
# --- Access Control: turnstile provider (Network; no offscreen) ---
wits_add_qttest(tst_turnstileprovider
    SOURCES
        tst_turnstileprovider.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/turnstileprovider.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/turnstileprovider.h
        ${CMAKE_SOURCE_DIR}/core/loginparser.cpp
        ${CMAKE_SOURCE_DIR}/core/loginparser.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.cpp
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/accesstypes.h
        ${CMAKE_SOURCE_DIR}/core/accesscontrol/iaccessprovider.h
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
    LIBS Qt${QT_VERSION_MAJOR}::Network
    INCLUDES ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/testsupport)
```

- [ ] **Step 5: Build + run to verify the tests fail (RED)**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_turnstileprovider
ctest --test-dir C:/b/loams-sp3 -R tst_turnstileprovider --output-on-failure
```
Expected: configure + build succeed (skeleton links), tests **FAIL** at assertions (`clampPollMs` returns 1500 for 100; no events; state never `Connected`/`Degraded`).

- [ ] **Step 6: Implement the real `turnstileprovider.cpp`**

Replace the skeleton with:

```cpp
#include "accesscontrol/turnstileprovider.h"
#include "loginparser.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace AccessControl {

int TurnstileProvider::clampPollMs(int raw)
{
    if (raw <= 0) return 1500;              // absent/invalid -> default
    return raw < 250 ? 250 : raw;          // clamp valid values to the floor
}

TurnstileProvider::TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                                     const QVariantMap &config, QObject *parent)
    : IAccessProvider(parent)
    , m_nam(nam)
    , m_baseUrl(std::move(baseUrl))
    , m_pollIntervalMs(clampPollMs(config.value(QStringLiteral("pollIntervalMs"), 1500).toInt()))
    , m_pollTimer(new QTimer(this))
{
    QString gate = config.value(QStringLiteral("gateId")).toString().trimmed();
    m_gateId = gate.isEmpty() ? QStringLiteral("turnstile") : gate;
    m_pollTimer->setSingleShot(true);
    connect(m_pollTimer, &QTimer::timeout, this, &TurnstileProvider::sendPoll);
}

TurnstileProvider::~TurnstileProvider() { stop(); }

ProviderDescriptor TurnstileProvider::defaultDescriptor()
{
    ProviderDescriptor d;
    d.providerId = QStringLiteral("turnstile");
    d.displayName = QStringLiteral("Turnstile (server-observed)");
    d.configSchema = {
        {QStringLiteral("pollIntervalMs"), QStringLiteral("Poll interval (ms)"),
         QStringLiteral("int"), false},
        {QStringLiteral("gateId"), QStringLiteral("Gate ID"),
         QStringLiteral("string"), false},
    };
    return d;
}

ProviderDescriptor TurnstileProvider::descriptor() const { return defaultDescriptor(); }

void TurnstileProvider::setState(ConnectionState s)
{
    if (s == m_state) return;
    m_state = s;
    emit stateChanged(s);
}

void TurnstileProvider::armTimer() { m_pollTimer->start(m_pollIntervalMs); }

void TurnstileProvider::start()
{
    ++m_generation;                 // invalidate any in-flight reply from a prior run
    m_pollTimer->stop();
    setState(ConnectionState::Connecting);
    sendPoll();
}

void TurnstileProvider::stop()
{
    ++m_generation;
    m_pollTimer->stop();
    if (m_reply) { m_reply->abort(); m_reply.clear(); }
}

void TurnstileProvider::sendPoll()
{
    if (m_reply) { m_reply->abort(); m_reply.clear(); }   // enforce one-in-flight

    QUrl url = m_baseUrl.resolved(QUrl(QStringLiteral("turnstile_display.php")));
    if (m_baselined) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("since"), QString::number(m_since));
        url.setQuery(q);
    }
    const quint64 gen = m_generation;
    QNetworkReply *reply = m_nam->get(QNetworkRequest(url));
    reply->setParent(this);           // provider owns its reply lifecycle
    m_reply = reply;

    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]() { reply->abort(); });
    timer->start(m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, gen]() { onFinished(reply, gen); });
}

void TurnstileProvider::fail()
{
    m_pollTimer->stop();
    m_reply.clear();
    setState(ConnectionState::Degraded);   // service owns reconnect/backoff
}

void TurnstileProvider::onFinished(QNetworkReply *reply, quint64 gen)
{
    if (gen != m_generation) return;   // stale (stopped/restarted since) — drop
    m_reply.clear();

    if (reply->error() != QNetworkReply::NoError) { fail(); return; }

    const LoginParser::EntryEventResult r =
        LoginParser::parseEntryEvent(reply->readAll(), m_baseUrl);
    if (!r.valid) { fail(); return; }

    if (!m_baselined) {                    // first start: baseline, skip history
        m_since = r.latestId;
        m_baselined = true;
        setState(ConnectionState::Connected);
        armTimer();                        // transition INTO steady polling
        return;
    }

    setState(ConnectionState::Connected);  // (re)confirm after a reconnect start
    if (r.hasEntry) {
        if (r.eventId <= m_since) { fail(); return; }   // non-advancing: protocol anomaly
        AccessEvent e;
        e.type = AccessEvent::Type::EntryObserved;
        e.subject = r.student;             // empty => unresolved (see convention)
        e.gateId = m_gateId;
        e.credentialKind = CredentialKind::Rfid;
        e.correlationId = QString::number(r.eventId);
        e.at = r.at;
        emit accessEvent(e);
        m_since = r.eventId;
        sendPoll();                        // drain: immediately request the next
    } else {
        armTimer();                        // empty poll: wait one interval
    }
}

} // namespace AccessControl
```

- [ ] **Step 7: Build + run to verify the tests pass (GREEN)**

```
cmake --build C:/b/loams-sp3 --target tst_turnstileprovider
ctest --test-dir C:/b/loams-sp3 -R tst_turnstileprovider --output-on-failure
```
Expected: PASS (12/12).

- [ ] **Step 8: Update the unknown-subject convention comment in `accesstypes.h`**

The empty-subject-means-unresolved convention is now realized by this provider, so update the `subject` field comment ([accesstypes.h:43](../../../qt-app/core/accesscontrol/accesstypes.h)):

```cpp
    QJsonObject subject;   // resolved subject/student JSON. For EntryObserved,
                           // EMPTY means "entry observed, subject unresolved"
                           // (orphaned/deleted student); non-empty == resolved.
```

- [ ] **Step 9: Commit** (via the `commit` skill) — `feat(accesscontrol): add server-observed TurnstileProvider`.

---

## Task 3: `AccessControlHub` + QML singleton wrapper

**Files:**
- Create: `qt-app/quick/AccessControlHub.h`, `qt-app/quick/AccessControlHub.cpp`, `qt-app/quick/AccessControlSingleton.h`
- Create: `qt-app/quick/tests/tst_accesscontrolhub.cpp`
- Modify: `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Consumes: `TurnstileProvider` (Task 2); `EventBus`, `AccessProviderFactory`, `AccessControlService`, `AccessEvent`; `ApiConfig::baseUrl()`; `AppSettings`.
- Produces:
  - `class AccessControlHub : public QObject` with `explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr, QObject *parent = nullptr);`, `void initialize();`, `bool isAccessEnabled() const;`, `Q_SIGNAL void entryObserved(const QVariantMap &entry);`, `static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);`, `static AccessControlHub *instance();`, `static void setInstance(AccessControlHub *);`.
  - `AccessEntry` map keys: `hasStudent` (bool), `student` (QVariantMap), `eventId` (QString), `at` (QDateTime).
  - `struct AccessControlSingleton` — `QML_FOREIGN(AccessControlHub)` + `QML_SINGLETON` + `QML_NAMED_ELEMENT(AccessControl)`, `static AccessControlHub *create(QQmlEngine *, QJSEngine *)`.
  - The hub reads settings and passes `pollIntervalMs`/`gateId` **raw** into the provider config — the provider owns normalization (Task 2).

- [ ] **Step 1: Create the hub header + a skeleton `.cpp` + the singleton wrapper**

Create these before touching CMake so the red step is a runtime assertion failure, not a configure failure over a missing source.

`qt-app/quick/AccessControlHub.h`:

```cpp
#ifndef ACCESSCONTROLHUB_H
#define ACCESSCONTROLHUB_H

#include <QObject>
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
class AccessControlHub : public QObject
{
    Q_OBJECT
public:
    explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr,
                              QObject *parent = nullptr);
    ~AccessControlHub() override;

    void initialize();
    bool isAccessEnabled() const;
    static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);

    static AccessControlHub *instance();
    static void setInstance(AccessControlHub *hub);

signals:
    void entryObserved(const QVariantMap &entry);

private:
    void onBusEvent(const AccessControl::AccessEvent &e);

    // Declaration order fixes teardown order: reverse destruction is
    // service -> factory -> owned NAM -> bus, so the service tears down the
    // provider (which aborts its reply) while the NAM is still alive.
    std::unique_ptr<AccessControl::EventBus> m_bus;
    std::unique_ptr<QNetworkAccessManager> m_ownedNam;   // only when self-created
    AccessControl::AccessProviderFactory m_factory;      // plain value member
    std::unique_ptr<AccessControl::AccessControlService> m_service;

    QNetworkAccessManager *m_nam = nullptr;   // owned-or-injected; non-owning ptr
};

#endif // ACCESSCONTROLHUB_H
```

`qt-app/quick/AccessControlHub.cpp` — **skeleton** (compiles + links; behaviour absent so the tests go red):

```cpp
#include "AccessControlHub.h"
#include <QNetworkAccessManager>
#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/eventbus.h"

using namespace AccessControl;
namespace { AccessControlHub *g_instance = nullptr; }

AccessControlHub *AccessControlHub::instance() { return g_instance; }
void AccessControlHub::setInstance(AccessControlHub *hub) { g_instance = hub; }

AccessControlHub::AccessControlHub(QNetworkAccessManager *injectedNam, QObject *parent)
    : QObject(parent)
    , m_bus(std::make_unique<EventBus>())
    , m_service(std::make_unique<AccessControlService>(m_bus.get(), &m_factory))
{
    if (injectedNam) m_nam = injectedNam;
    else { m_ownedNam = std::make_unique<QNetworkAccessManager>(); m_nam = m_ownedNam.get(); }
}
AccessControlHub::~AccessControlHub() { if (g_instance == this) g_instance = nullptr; }
QVariantMap AccessControlHub::toAccessEntry(const AccessEvent &) { return {}; }   // stub
void AccessControlHub::onBusEvent(const AccessEvent &) {}                          // stub
bool AccessControlHub::isAccessEnabled() const { return false; }                  // stub
void AccessControlHub::initialize() {}                                            // stub
```

`qt-app/quick/AccessControlSingleton.h` (final — no stub needed):

```cpp
#ifndef ACCESSCONTROLSINGLETON_H
#define ACCESSCONTROLSINGLETON_H

#include <QQmlEngine>
#include "AccessControlHub.h"

// Registration shim only — never instantiated by QML. Exposes the app-owned
// AccessControlHub instance as the "AccessControl" QML singleton. A QML_FOREIGN
// wrapper (not QML_SINGLETON on the hub directly) because the hub is
// default-constructible in main(); Qt would otherwise prefer the default ctor
// over create() and hand QML a separate, uninitialized instance.
struct AccessControlSingleton
{
    Q_GADGET
    QML_FOREIGN(AccessControlHub)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AccessControl)
public:
    static AccessControlHub *create(QQmlEngine *, QJSEngine *)
    {
        AccessControlHub *inst = AccessControlHub::instance();
        Q_ASSERT_X(inst, "AccessControlSingleton::create",
                   "AccessControlHub::setInstance() must run before the engine loads");
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }
};

#endif // ACCESSCONTROLSINGLETON_H
```

- [ ] **Step 2: Write the hub tests**

`qt-app/quick/tests/tst_accesscontrolhub.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "AccessControlHub.h"
#include "appsettings.h"
#include "sequencednam.h"
#include "accesscontrol/accesstypes.h"

using namespace AccessControl;

class TestAccessControlHub : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void init()
    {
        AppSettings::isolateForTesting();       // fresh throwaway INI each test
        qunsetenv("WITS_ACCESS_CONTROL");
        AppSettings s; s.clear(); s.sync();
    }
    void toAccessEntry_knownStudent();
    void toAccessEntry_unknownStudent();
    void disabledByDefault_inert();
    void settingEnables();
    void envZeroDoesNotForceOff_settingWins();
    void envForceEnablesAndEmitsEntry();

private:
    static QByteArray entryPayload(qint64 latest, qint64 id)
    {
        return QStringLiteral(
            "{\"status\":\"success\",\"latest_id\":%1,\"entry\":{\"id\":%2,"
            "\"card\":\"C\",\"created_at\":\"2026-09-29 08:30:00\",\"reader\":0,"
            "\"student\":{\"name\":\"A\",\"photo_path\":\"uploads/a.jpg\"}}}")
            .arg(latest).arg(id).toUtf8();
    }
    static QByteArray emptyPayload(qint64 latest)
    {
        return QStringLiteral("{\"status\":\"success\",\"latest_id\":%1,\"entry\":null}")
            .arg(latest).toUtf8();
    }
};

void TestAccessControlHub::toAccessEntry_knownStudent()
{
    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;
    e.subject = QJsonObject{{"name", "A"}};
    e.correlationId = QStringLiteral("42");
    e.at = QDateTime(QDate(2026, 9, 29), QTime(8, 30), Qt::UTC);
    const QVariantMap m = AccessControlHub::toAccessEntry(e);
    QCOMPARE(m.value("hasStudent").toBool(), true);
    QCOMPARE(m.value("student").toMap().value("name").toString(), QStringLiteral("A"));
    QCOMPARE(m.value("eventId").toString(), QStringLiteral("42"));
    QCOMPARE(m.value("at").toDateTime(), e.at);
}

void TestAccessControlHub::toAccessEntry_unknownStudent()
{
    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;   // empty subject == unresolved
    e.correlationId = QStringLiteral("7");
    const QVariantMap m = AccessControlHub::toAccessEntry(e);
    QCOMPARE(m.value("hasStudent").toBool(), false);
    QVERIFY(m.value("student").toMap().isEmpty());
}

void TestAccessControlHub::disabledByDefault_inert()
{
    SequencedNam nam;
    AccessControlHub hub(&nam);
    QSignalSpy spy(&hub, &AccessControlHub::entryObserved);
    hub.initialize();                       // accessControl/enabled defaults false
    QVERIFY(!hub.isAccessEnabled());
    QTest::qWait(300);
    QCOMPARE(nam.requestCount(), 0);        // no polling
    QCOMPARE(spy.count(), 0);
}

void TestAccessControlHub::settingEnables()
{
    { AppSettings s; s.setValue("accessControl/enabled", true); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());         // enabled by setting, no env
}

void TestAccessControlHub::envZeroDoesNotForceOff_settingWins()
{
    qputenv("WITS_ACCESS_CONTROL", "0");    // 0 is NOT a force-off
    { AppSettings s; s.setValue("accessControl/enabled", true); s.sync(); }
    SequencedNam nam;
    AccessControlHub hub(&nam);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());         // falls through to the (true) setting
}

void TestAccessControlHub::envForceEnablesAndEmitsEntry()
{
    qputenv("WITS_ACCESS_CONTROL", "1");
    { AppSettings s; s.setValue("accessControl/pollIntervalMs", 250); s.sync(); }
    SequencedNam nam;
    nam.enqueue(emptyPayload(3));           // baseline: cursor = 3
    nam.enqueue(entryPayload(4, 4));        // poll -> entry 4
    nam.enqueue(emptyPayload(4));           // drain end
    AccessControlHub hub(&nam);
    QSignalSpy spy(&hub, &AccessControlHub::entryObserved);
    hub.initialize();
    QVERIFY(hub.isAccessEnabled());
    QVERIFY(spy.wait(3000));
    const QVariantMap m = spy.at(0).at(0).toMap();
    QCOMPARE(m.value("hasStudent").toBool(), true);
    QCOMPARE(m.value("eventId").toString(), QStringLiteral("4"));
}

QTEST_MAIN(TestAccessControlHub)
#include "tst_accesscontrolhub.moc"
```

- [ ] **Step 3: Register the sources + test in CMake**

In `qt-app/quick/CMakeLists.txt`, add to the `witsquickmodule` `SOURCES` list (after the `viewmodels/...` lines):

```cmake
        AccessControlHub.h AccessControlHub.cpp
        AccessControlSingleton.h
```

After the `tst_qml_kiosk` block, register the hub test:

```cmake
# --- AccessControlHub unit test (C++ QtTest, offscreen). SequencedNam drives
# the provider with no live network; AppSettings isolation is compiled in by
# wits_add_qttest. ---
wits_add_qttest(tst_accesscontrolhub
    SOURCES
        tests/tst_accesscontrolhub.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

- [ ] **Step 4: Build + run to verify the tests fail (RED)**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_accesscontrolhub
ctest --test-dir C:/b/loams-sp3 -R tst_accesscontrolhub --output-on-failure
```
Expected: configure + build succeed (skeleton links), tests **FAIL** (stub `toAccessEntry` returns `{}`; `isAccessEnabled()` returns false; no emission).

- [ ] **Step 5: Implement the real `AccessControlHub.cpp`**

Replace the skeleton with:

```cpp
#include "AccessControlHub.h"

#include <QNetworkAccessManager>

#include "apiconfig.h"
#include "appsettings.h"
#include "accesscontrol/accesscontrolservice.h"
#include "accesscontrol/eventbus.h"
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
}

AccessControlHub::~AccessControlHub() { if (g_instance == this) g_instance = nullptr; }

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

bool AccessControlHub::isAccessEnabled() const { return m_service->isEnabled(); }

void AccessControlHub::initialize()
{
    AppSettings settings;   // through AppSettings so tests isolate

    // Register the provider (whether or not enabled) so a later runtime toggle
    // can enable without re-registering. Creator captures the hub's NAM+baseUrl.
    const ProviderDescriptor descriptor = TurnstileProvider::defaultDescriptor();
    QNetworkAccessManager *nam = m_nam;
    const QUrl baseUrl(ApiConfig::baseUrl());
    m_factory.registerProvider(descriptor,
        [nam, baseUrl](const ProviderDescriptor &, const QVariantMap &cfg, QObject *parent)
            -> IAccessProvider * {
            return new TurnstileProvider(nam, baseUrl, cfg, parent);
        });

    // Enablement precedence: env force-on > setting > false.
    const QString env = qEnvironmentVariable("WITS_ACCESS_CONTROL").trimmed().toLower();
    const bool forceOn = (env == QLatin1String("1") || env == QLatin1String("true"));
    const bool enabled = forceOn
                         || settings.value(QStringLiteral("accessControl/enabled"), false).toBool();
    if (!enabled) return;

    // Pass config RAW — the provider owns pollIntervalMs/gateId normalization.
    m_service->enable(descriptor, QVariantMap{
        {QStringLiteral("pollIntervalMs"),
         settings.value(QStringLiteral("accessControl/pollIntervalMs"), 1500)},
        {QStringLiteral("gateId"),
         settings.value(QStringLiteral("accessControl/gateId"), QStringLiteral("turnstile"))},
    });
}
```

- [ ] **Step 6: Build + run to verify the tests pass (GREEN)**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_accesscontrolhub
ctest --test-dir C:/b/loams-sp3 -R tst_accesscontrolhub --output-on-failure
```
Expected: PASS (6/6).

- [ ] **Step 7: Commit** (via the `commit` skill) — `feat(accesscontrol): add AccessControlHub composition root + QML singleton`.

---

## Task 4: `KioskViewModel::onEntryObserved` + `showUnknownEntry`

**Files:**
- Modify: `qt-app/quick/viewmodels/KioskViewModel.h`, `qt-app/quick/viewmodels/KioskViewModel.cpp`
- Test: `qt-app/quick/tests/tst_kioskviewmodel.cpp`

**Interfaces:**
- Consumes: the `AccessEntry` map shape from Task 3 (`hasStudent`, `student`).
- Produces: `Q_INVOKABLE void KioskViewModel::onEntryObserved(const QVariantMap &entry);` and `void KioskViewModel::showUnknownEntry();`.

- [ ] **Step 1: Write the failing tests**

Append to `qt-app/quick/tests/tst_kioskviewmodel.cpp` — add to `private slots:` and implement:

```cpp
void onEntryObserved_knownStudentDisplaysAndCounts();
void onEntryObserved_unknownShowsNeutralToastNoCount();
```

```cpp
void TestKioskViewModel::onEntryObserved_knownStudentDisplaysAndCounts()
{
    KioskViewModel vm;
    const int before = vm.visitorsToday();
    QVariantMap entry{
        {"hasStudent", true},
        {"student", QVariantMap{{"name", "Jane Cruz"}, {"course", "BSCS"},
                                {"photo_url", "http://x/j.jpg"}}},
        {"eventId", "11"}, {"at", QDateTime::currentDateTimeUtc()}};
    vm.onEntryObserved(entry);
    QVERIFY(vm.hasStudent());
    QCOMPARE(vm.currentFullName(), QStringLiteral("Jane Cruz"));
    QCOMPARE(vm.currentPhotoUrl(), QStringLiteral("http://x/j.jpg"));
    QCOMPARE(vm.visitorsToday(), before + 1);
}

void TestKioskViewModel::onEntryObserved_unknownShowsNeutralToastNoCount()
{
    KioskViewModel vm;
    const int before = vm.visitorsToday();
    QSignalSpy status(&vm, &KioskViewModel::statusChanged);
    QVariantMap entry{{"hasStudent", false}, {"student", QVariantMap{}},
                      {"eventId", "12"}, {"at", QDateTime::currentDateTimeUtc()}};
    vm.onEntryObserved(entry);
    QVERIFY(status.count() >= 1);
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(vm.statusSeverity(), QStringLiteral("Info"));   // neutral, not Error
    QCOMPARE(vm.visitorsToday(), before);                    // no count bump
    QVERIFY(!vm.hasStudent());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-sp3 --target tst_kioskviewmodel
ctest --test-dir C:/b/loams-sp3 -R tst_kioskviewmodel --output-on-failure
```
Expected: FAIL to compile (`onEntryObserved` not a member) — `KioskViewModel` compiles into `witsquickmodule`, which the test links.

- [ ] **Step 3: Declare the members in `KioskViewModel.h`**

Add `#include <QVariantMap>`. After `Q_INVOKABLE void requestGuest();`:

```cpp
    // Access Control (Sub-plan 3): a confirmed gate entry from the
    // AccessControl singleton. hasStudent -> display + count; else neutral toast.
    Q_INVOKABLE void onEntryObserved(const QVariantMap &entry);
```

And in the private helpers (near `setStatus`):

```cpp
    void showUnknownEntry();
```

- [ ] **Step 4: Implement in `KioskViewModel.cpp`**

Add near `applyStudentLogin` (it reuses that existing seam):

```cpp
void KioskViewModel::onEntryObserved(const QVariantMap &entry)
{
    if (entry.value(QStringLiteral("hasStudent")).toBool()) {
        applyStudentLogin(QJsonObject::fromVariantMap(
            entry.value(QStringLiteral("student")).toMap()));
    } else {
        showUnknownEntry();
    }
}

void KioskViewModel::showUnknownEntry()
{
    // Gate already admitted the person; the card just didn't resolve to a
    // student. Neutral notice only — no welcome, no counter bump, no feed row.
    setStatus(QStringLiteral("Card not recognized"), QStringLiteral("Info"));
}
```

- [ ] **Step 5: Run the tests to verify they pass**

```
cmake --build C:/b/loams-sp3 --target tst_kioskviewmodel
ctest --test-dir C:/b/loams-sp3 -R tst_kioskviewmodel --output-on-failure
```
Expected: PASS (new cases + all pre-existing).

- [ ] **Step 6: Commit** (via the `commit` skill) — `feat(kiosk): route observed gate entries to the kiosk display`.

---

## Task 5: App wiring + existing-test fixup (integration)

**Files:**
- Modify: `qt-app/quick/qml/kiosk/KioskScreen.qml`, `qt-app/quick/main.cpp`
- Modify: `qt-app/quick/tests/tst_appshell.cpp`, `qt-app/quick/tests/tst_qml_kiosk.cpp`

**Interfaces:**
- Consumes: `AccessControlHub` + the `AccessControl` QML singleton (Task 3), `KioskViewModel::onEntryObserved` (Task 4).
- Produces: the fully wired app (flag-gated) and green QML harnesses.

This task has a genuine red: adding the `Connections { target: AccessControl }` to the kiosk surface makes `tst_appshell` load `AppShell` (whose default surface is `KioskScreen`) with **no hub installed**, so `AccessControlSingleton::create()` hits its `Q_ASSERT_X` / QML fails to resolve the singleton and the test fails. Installing a disabled hub in the harnesses (and wiring `main.cpp`) turns it green.

- [ ] **Step 1: Add the `Connections` to `KioskScreen.qml` (this is the red trigger)**

Inside the root `Rectangle`, right after `GuestViewModel { id: guestVm }`:

```qml
    // Access Control (Sub-plan 3): a confirmed gate entry surfaces natively.
    // Presentation-scoped — this Connections lives and dies with the kiosk
    // surface. The AccessControl singleton is inert unless accessControl.enabled.
    Connections {
        target: AccessControl
        function onEntryObserved(entry) { kioskVm.onEntryObserved(entry) }
    }
```

- [ ] **Step 2: Build + run the QML suites to verify they fail (RED)**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_appshell tst_qml_kiosk
ctest --test-dir C:/b/loams-sp3 -R "tst_appshell|tst_qml_kiosk" --output-on-failure
```
Expected: **FAIL** — `AccessControlSingleton::create()` asserts (no instance installed) / `AppShell` logs a singleton-resolution warning, failing `tst_appshell`'s zero-warning check. (Both targets are built first so the failure is real, not a "Not Run" from a missing executable.)

- [ ] **Step 3: Install a disabled hub in `tst_appshell.cpp`**

Add `#include "AccessControlHub.h"`, and in `loadsWithZeroWarnings()` construct + install the hub **before** the engine, declared so it outlives it:

```cpp
    AccessControlHub hub;                 // default settings, no env -> disabled
    AccessControlHub::setInstance(&hub);

    QQmlApplicationEngine engine;         // declared AFTER hub -> engine dies first
    engine.loadFromModule("LOAMS", "AppShell");
```

(Move the existing `QQmlApplicationEngine engine;` line down so it follows `setInstance`.)

- [ ] **Step 4: Install a disabled hub in `tst_qml_kiosk.cpp`**

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
        // Live-but-disabled singleton (never initialize() -> no polling), robust
        // against an inherited WITS_ACCESS_CONTROL.
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_kiosk, Setup)
#include "tst_qml_kiosk.moc"
```

`tst_appshell` and `tst_qml_kiosk` already link `witsquickmodule` (which now contains `AccessControlHub.cpp`), so no CMake change is needed for them.

- [ ] **Step 5: Wire `main.cpp`**

Add `#include "AccessControlHub.h"` and, after the cached-branding block and **before** `QQmlApplicationEngine engine;`:

```cpp
    AccessControlHub accessControl;      // stack-owned; outlives `engine`
    accessControl.initialize();          // reads the flag; polls only if enabled
    AccessControlHub::setInstance(&accessControl);

    QQmlApplicationEngine engine;
```

- [ ] **Step 6: Build everything and run the full suite (GREEN)**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3
ctest --test-dir C:/b/loams-sp3 --output-on-failure
```
Expected: clean build (no new warnings); **all** tests pass, including `tst_appshell` (zero QML warnings) and `tst_qml_kiosk` with the `AccessControl` singleton resolving.

- [ ] **Step 7: Manual smoke (documented, run once)**

Flag off (default): launch `WITSQuick`, confirm the kiosk behaves exactly as before (no Access Control effect). Then with a seeded `turnstile_events` row and `WITS_ACCESS_CONTROL=1`, confirm the student surfaces natively (spec §Verification). Record the result in the PR body.

- [ ] **Step 8: Commit** (via the `commit` skill) — `feat(accesscontrol): wire the hub singleton into main + the kiosk surface`.

---

## Self-Review

**1. Spec coverage:**
- §1 parser (`EntryEventResult`, photo composition, local→UTC, strict validity incl. `latest_id >= 0`) → Task 1. ✅
- §2 provider: baseline-once, reconnect-preserves-cursor + processes response, drain oldest-first, `eventId>since` guard, timeout+abort, generation guard, single `Degraded`, provider-owned reply, `gateId` config + fallback, `pollIntervalMs` clamp, never-runtime-`Error` → Task 2 (tests cover baseline-then-poll, empty-poll-re-arms, drain-oldest-first + one-in-flight sequencing, reconnect-emits-once, non-advancing, malformed, transport→one-Degraded, timeout→abort, stop→abort→no-emit, restart-drops-in-flight-generation, clampPollMs, blank-gateId). ✅
- §3 unknown-subject convention → `toAccessEntry` (Task 3) + provider emit + `accesstypes.h` comment (Task 2 Step 8). ✅
- §4 hub ownership/teardown order, owned-vs-injected NAM seam, register-when-disabled, event→`QVariantMap` → Task 3. ✅
- §5 `QML_FOREIGN` wrapper + `create()` fail-fast + `main.cpp` order + CMake SOURCES + existing-QML-test install → Tasks 3 & 5. ✅
- §6 kiosk `onEntryObserved`/`showUnknownEntry` + presentation-scoped `Connections` (`kioskVm`) → Tasks 4 & 5. ✅
- §7 enablement precedence (incl. `WITS_ACCESS_CONTROL=0` ⇒ setting wins) + `AppSettings` + normalization → Tasks 2 & 3. ✅
- Forward-note (per-poll comm-health deferred to Sub-plan 4): provider does **not** touch `HealthMonitor`. ✅

**2. Placeholder scan:** No TBD/TODO/"handle edge cases"/"similar to". Skeleton `.cpp`s are explicitly labelled and shown in full; real bodies shown in full. ✅

**3. Type consistency:** `parseEntryEvent(QByteArray, QUrl)`/`EntryEventResult` (T1) consumed identically in T2. `TurnstileProvider(nam, QUrl, QVariantMap, parent)` + `defaultDescriptor()` + `clampPollMs` (T2) match T3's creator lambda + T2 tests. `AccessControlHub(QNetworkAccessManager*, QObject*)`, `initialize()`, `isAccessEnabled()`, `toAccessEntry`, `instance()/setInstance()`, `entryObserved(QVariantMap)` (T3) match T3 tests + T5 wiring. `AccessEntry` keys `hasStudent`/`student`/`eventId`/`at` identical across `toAccessEntry` (T3) and `onEntryObserved` (T4). `SequencedNam` API (`enqueue`/`enqueueStall`/`requestCount`/`abortCount`) consistent between its definition (T2) and both consumers (T2, T3). ✅

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-09-29-access-control-provider-kiosk.md`. Two execution options:

1. **Subagent-Driven (recommended)** — a fresh subagent per task, two-stage review between tasks, fast iteration.
2. **Inline Execution** — execute tasks in this session with checkpoints.

Which approach?
