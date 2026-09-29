# TurnstileProvider + Native Kiosk Display Implementation Plan (Sub-plan 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a real turnstile swipe surface the student natively on the LOAMS 2.0 (`WITSQuick`) kiosk, retiring the PowerShell bridge, behind `accessControl.enabled` (default off = zero behavior change).

**Architecture:** A pure `LoginParser::parseEntryEvent` decodes `turnstile_display.php`; a server-observed `TurnstileProvider` (an `IAccessProvider`) polls that endpoint through an injected `QNetworkAccessManager`, tracks an oldest-next cursor, and emits `EntryObserved` `AccessEvent`s; an application-owned `AccessControlHub` composition root (exposed to QML as the `AccessControl` singleton via a `QML_FOREIGN` wrapper) owns the `EventBus`/`AccessProviderFactory`/`AccessControlService`, and maps `EntryObserved` to a QML-facing `entryObserved(QVariantMap)`; a presentation-scoped `Connections { target: AccessControl }` on the kiosk surface forwards to a new `KioskViewModel::onEntryObserved` slot.

**Tech Stack:** Qt 6 / C++17, CMake + Ninja, QtTest (`wits_add_qttest`), QML/Qt Quick (MVVM), PHP 8.2 backend (already merged, read-only).

## Global Constraints

- **Feature flag:** every runtime effect is behind `accessControl.enabled` (default **false**). Flag off ⇒ no provider built, no polling, no bus traffic, inert singleton — zero behavior change. Copied verbatim from the spec.
- **Read-only:** the provider **never** POSTs. `turnstile.php` remains the sole authoritative attendance writer. The kiosk display path issues no backend writes.
- **Privacy:** raw `card` and `reader` never leave the adapter — they are never placed on `AccessEvent`, the `EventBus`, or the QML signal.
- **Settings:** all reads go through **`AppSettings`** (never raw `QSettings`) so tests isolate via `AppSettings::isolateForTesting()`. Keys: `accessControl/enabled` (bool), `accessControl/pollIntervalMs` (int), `accessControl/gateId` (string).
- **Enablement precedence:** `WITS_ACCESS_CONTROL` = `1`/`true` (case-insensitive) force-on for the process → else `accessControl/enabled` → else false. `WITS_ACCESS_CONTROL=0` is **not** a force-off in this slice.
- **Integer widths:** `latest_id`, `eventId`, and the poll cursor are `qint64` throughout.
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
| `qt-app/core/accesscontrol/turnstileprovider.h` / `.cpp` | Server-observed `IAccessProvider`: poll/cursor/timeout/generation | 2 |
| `qt-app/testsupport/sequencednam.h` / `.cpp` | Reusable multi-response fake NAM for tests | 2 |
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

Append to `qt-app/tests/tst_loginparser.cpp` — add these declarations under the existing `private slots:` block and the implementations below the existing ones. Add `#include <QUrl>` near the top if not present.

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
    // Same instant as the local wall-clock string reinterpreted as local time.
    QDateTime local(QDate(2026, 9, 29), QTime(8, 30, 0));   // Qt::LocalTime
    QCOMPARE(r.at, local.toUTC());
}

void TestLoginParser::parseEntryEvent_malformedIsInvalid()
{
    const QUrl base("http://localhost/loams_api/");
    QVERIFY(!LoginParser::parseEntryEvent("not json", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(R"({"status":"error"})", base).valid);
    // entry present but neither null nor object
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":3})", base).valid);
    // non-positive id
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":0,"created_at":"2026-09-29 08:30:00","student":null}})", base).valid);
    // unparseable created_at
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":1,"created_at":"nope","student":null}})", base).valid);
    // negative latest_id
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":-1,"entry":null})", base).valid);
    // student present but not null/object
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":1,"created_at":"2026-09-29 08:30:00","student":5}})", base).valid);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

```
cmake --build C:/b/loams-sp3 --target tst_loginparser
ctest --test-dir C:/b/loams-sp3 -R tst_loginparser --output-on-failure
```
Expected: FAIL to compile (`parseEntryEvent`/`EntryEventResult` undeclared).

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

Add `#include <QJsonValue>` if not present (`QJsonDocument`/`QJsonObject` are already used). Append:

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

- [ ] **Step 6: Commit** (via the `commit` skill)

Message subject: `feat(accesscontrol): add pure parseEntryEvent decoder for turnstile_display`.

---

## Task 2: `TurnstileProvider` (server-observed IAccessProvider)

**Files:**
- Create: `qt-app/core/accesscontrol/turnstileprovider.h`, `qt-app/core/accesscontrol/turnstileprovider.cpp`
- Create: `qt-app/testsupport/sequencednam.h`, `qt-app/testsupport/sequencednam.cpp`
- Create: `qt-app/tests/tst_turnstileprovider.cpp`
- Modify: `qt-app/core/CMakeLists.txt` (add sources to `witscore`), `qt-app/tests/CMakeLists.txt` (register test)

**Interfaces:**
- Consumes: `LoginParser::parseEntryEvent` (Task 1); `IAccessProvider`, `AccessEvent`, `ProviderDescriptor`, `ConnectionState` (`accesstypes.h`).
- Produces:
  - `class TurnstileProvider : public AccessControl::IAccessProvider` with ctor `TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl, const QVariantMap &config, QObject *parent = nullptr)` (reads `config["pollIntervalMs"]` int, `config["gateId"]` string) and `static AccessControl::ProviderDescriptor defaultDescriptor();` (`providerId == "turnstile"`).
  - `SequencedNam` (test util): `explicit SequencedNam(QObject *parent = nullptr);` + `void enqueue(const QByteArray &body, QNetworkReply::NetworkError error = QNetworkReply::NoError);` + `int requestCount() const;`. Each `get()` pops the next queued response (finishes immediately); an empty queue yields an empty successful body.

- [ ] **Step 1: Create the reusable `SequencedNam` test double**

`qt-app/testsupport/sequencednam.h`:

```cpp
#ifndef SEQUENCEDNAM_H
#define SEQUENCEDNAM_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>

// Test-only NAM that answers each request with the NEXT enqueued canned
// response, finishing immediately. Unlike CapturingNam (one fixed payload),
// this drives multi-request flows (baseline -> poll -> poll ...). No live net.
class SequencedNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit SequencedNam(QObject *parent = nullptr);
    void enqueue(const QByteArray &body,
                 QNetworkReply::NetworkError error = QNetworkReply::NoError);
    int requestCount() const { return m_requestCount; }
    QUrl lastUrl;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    struct Canned { QByteArray body; QNetworkReply::NetworkError error; };
    QQueue<Canned> m_queue;
    int m_requestCount = 0;
};

#endif // SEQUENCEDNAM_H
```

`qt-app/testsupport/sequencednam.cpp`:

```cpp
#include "sequencednam.h"

#include <QBuffer>
#include <QTimer>
#include <QNetworkRequest>

// Minimal QNetworkReply that finishes on the next event-loop turn with a fixed
// body + error code. Body is served from an internal buffer.
namespace {
class CannedReply : public QNetworkReply
{
public:
    CannedReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req,
                const QByteArray &body, QNetworkReply::NetworkError error,
                QObject *parent)
        : QNetworkReply(parent), m_body(body)
    {
        setRequest(req);
        setUrl(req.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        m_buffer.setData(m_body);
        m_buffer.open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,
                     error == QNetworkReply::NoError ? 200 : 500);
        QTimer::singleShot(0, this, [this, error]() {
            if (error != QNetworkReply::NoError) {
                setError(error, QStringLiteral("canned error"));
                emit errorOccurred(error);
            }
            setFinished(true);
            emit finished();
        });
    }
    void abort() override {
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted"));
        setFinished(true);
        emit finished();
    }
    qint64 readData(char *data, qint64 maxlen) override { return m_buffer.read(data, maxlen); }
    qint64 bytesAvailable() const override {
        return m_buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
    }
private:
    QByteArray m_body;
    QBuffer m_buffer;
};
} // namespace

SequencedNam::SequencedNam(QObject *parent) : QNetworkAccessManager(parent) {}

void SequencedNam::enqueue(const QByteArray &body, QNetworkReply::NetworkError error)
{
    m_queue.enqueue({body, error});
}

QNetworkReply *SequencedNam::createRequest(Operation op, const QNetworkRequest &request,
                                           QIODevice *)
{
    ++m_requestCount;
    lastUrl = request.url();
    Canned c = m_queue.isEmpty()
                   ? Canned{QByteArrayLiteral("{\"status\":\"success\",\"latest_id\":0,\"entry\":null}"),
                            QNetworkReply::NoError}
                   : m_queue.dequeue();
    return new CannedReply(op, request, c.body, c.error, this);
}
```

- [ ] **Step 2: Write the failing provider tests**

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
    void baselineSkipsHistoryThenPolls();
    void drainsEntriesOldestFirst();
    void reconnectPreservesCursorAndEmitsReconnectEntry();
    void nonAdvancingEntryDegrades();
    void transportFailureDegradesCursorUnmoved();

private:
    static QVariantMap cfg(int pollMs = 100, const QString &gate = QStringLiteral("g1"))
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
};

void TestTurnstileProvider::baselineSkipsHistoryThenPolls()
{
    SequencedNam nam;
    nam.enqueue(entryPayload(5, 5));   // baseline: has history, must NOT be emitted
    nam.enqueue(emptyPayload(5));      // first steady poll: empty
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);  // baseline + one steady poll
    QCOMPARE(events.count(), 0);                              // history not emitted
    QCOMPARE(p.state(), ConnectionState::Connected);
    QVERIFY(nam.lastUrl.query().contains(QStringLiteral("since=5")));
    p.stop();
}

void TestTurnstileProvider::drainsEntriesOldestFirst()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(0));       // baseline: empty, cursor = 0
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
    const auto e2 = qvariant_cast<AccessEvent>(events.at(1).at(0));
    QCOMPARE(e2.correlationId, QStringLiteral("2"));
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

    // Reconnect: the service calls start() again. Cursor must stay 3, and an
    // entry returned by the reconnect response must be emitted exactly once.
    nam.enqueue(entryPayload(4, 4));    // reconnect ?since=3 -> entry 4
    nam.enqueue(emptyPayload(4));       // drain end
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    const auto e = qvariant_cast<AccessEvent>(events.at(0).at(0));
    QCOMPARE(e.correlationId, QStringLiteral("4"));
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
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(events.count(), 0);
    p.stop();
}

void TestTurnstileProvider::transportFailureDegradesCursorUnmoved()
{
    SequencedNam nam;
    nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);   // baseline fails
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(events.count(), 0);
    p.stop();
}

QTEST_MAIN(TestTurnstileProvider)
#include "tst_turnstileprovider.moc"
```

- [ ] **Step 3: Register the sources + test (so the red step compiles/links)**

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

- [ ] **Step 4: Run the tests to verify they fail**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_turnstileprovider
```
Expected: FAIL to compile (`turnstileprovider.h` does not exist).

- [ ] **Step 5: Create `turnstileprovider.h`**

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
// See design spec 2026-09-29-access-control-provider-kiosk-design.md.
class TurnstileProvider : public IAccessProvider
{
    Q_OBJECT
public:
    TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                      const QVariantMap &config, QObject *parent = nullptr);
    ~TurnstileProvider() override;

    static ProviderDescriptor defaultDescriptor();

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

- [ ] **Step 6: Implement `turnstileprovider.cpp`**

```cpp
#include "accesscontrol/turnstileprovider.h"
#include "loginparser.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace AccessControl {

TurnstileProvider::TurnstileProvider(QNetworkAccessManager *nam, QUrl baseUrl,
                                     const QVariantMap &config, QObject *parent)
    : IAccessProvider(parent)
    , m_nam(nam)
    , m_baseUrl(std::move(baseUrl))
    , m_gateId(config.value(QStringLiteral("gateId"), QStringLiteral("turnstile")).toString())
    , m_pollTimer(new QTimer(this))
{
    const int poll = config.value(QStringLiteral("pollIntervalMs"), 1500).toInt();
    m_pollIntervalMs = poll >= 250 ? poll : 1500;   // guard against a hot loop
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
    // A restart (including a service-triggered reconnect) invalidates any reply
    // still in flight, so a stale response can never mutate the cursor.
    ++m_generation;
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

- [ ] **Step 7: Run the tests to verify they pass**

```
cmake --build C:/b/loams-sp3 --target tst_turnstileprovider
ctest --test-dir C:/b/loams-sp3 -R tst_turnstileprovider --output-on-failure
```
Expected: PASS (5/5).

- [ ] **Step 8: Commit** (via the `commit` skill)

Subject: `feat(accesscontrol): add server-observed TurnstileProvider`.

---

## Task 3: `AccessControlHub` + QML singleton wrapper

**Files:**
- Create: `qt-app/quick/AccessControlHub.h`, `qt-app/quick/AccessControlHub.cpp`, `qt-app/quick/AccessControlSingleton.h`
- Create: `qt-app/quick/tests/tst_accesscontrolhub.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (SOURCES + register test)

**Interfaces:**
- Consumes: `TurnstileProvider` (Task 2); `EventBus`, `AccessProviderFactory`, `AccessControlService`, `AccessEvent` (`witscore`); `ApiConfig::baseUrl()`; `AppSettings`.
- Produces:
  - `class AccessControlHub : public QObject` with `explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr, QObject *parent = nullptr);`, `void initialize();`, `bool isAccessEnabled() const;`, `Q_SIGNAL void entryObserved(const QVariantMap &entry);`, `static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);`, `static AccessControlHub *instance();`, `static void setInstance(AccessControlHub *);`.
  - `AccessEntry` map keys: `hasStudent` (bool), `student` (QVariantMap), `eventId` (QString), `at` (QDateTime).
  - `struct AccessControlSingleton` — `QML_FOREIGN(AccessControlHub)` + `QML_SINGLETON` + `QML_NAMED_ELEMENT(AccessControl)`, `static AccessControlHub *create(QQmlEngine *, QJSEngine *)`.

- [ ] **Step 1: Write the failing hub tests**

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
    void initTestCase() { registerMetaTypes(); AppSettings::isolateForTesting(); }
    void init() { qunsetenv("WITS_ACCESS_CONTROL"); }
    void toAccessEntry_knownStudent();
    void toAccessEntry_unknownStudent();
    void disabledByDefault_inert();
    void envForceEnablesAndEmitsEntry();

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

void TestAccessControlHub::envForceEnablesAndEmitsEntry()
{
    qputenv("WITS_ACCESS_CONTROL", "1");
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

- [ ] **Step 2: Register the sources + test (so the red step compiles/links)**

In `qt-app/quick/CMakeLists.txt`, add to the `witsquickmodule` `SOURCES` list (after the `viewmodels/...` lines):

```cmake
        AccessControlHub.h AccessControlHub.cpp
        AccessControlSingleton.h
```

Then, after the `tst_qml_kiosk` block, register the hub test (it needs `witscore` symbols, `Network`, `capturingnam`/`sequencednam`, and `testsupport` on the include path):

```cmake
# --- AccessControlHub unit test (C++ QtTest, offscreen). Uses a SequencedNam so
# the provider drains with no live network; AppSettings isolation is compiled in
# by wits_add_qttest. ---
wits_add_qttest(tst_accesscontrolhub
    SOURCES
        tests/tst_accesscontrolhub.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)
```

- [ ] **Step 3: Run the test to verify it fails**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_accesscontrolhub
```
Expected: FAIL to compile (`AccessControlHub.h` does not exist).

- [ ] **Step 4: Create `AccessControlHub.h`**

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
// and the provider). Maps EntryObserved bus events to a QML-facing
// entryObserved(QVariantMap). Exposed to QML as the "AccessControl" singleton
// via AccessControlSingleton (QML_FOREIGN). See the design spec.
class AccessControlHub : public QObject
{
    Q_OBJECT
public:
    explicit AccessControlHub(QNetworkAccessManager *injectedNam = nullptr,
                              QObject *parent = nullptr);
    ~AccessControlHub() override;

    // Reads enablement (WITS_ACCESS_CONTROL > accessControl/enabled > false),
    // registers the TurnstileProvider, and enables the service iff on.
    void initialize();
    bool isAccessEnabled() const;

    // Pure map: EntryObserved AccessEvent -> AccessEntry QVariantMap.
    static QVariantMap toAccessEntry(const AccessControl::AccessEvent &e);

    // The app-owned instance the QML singleton factory hands out.
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

- [ ] **Step 5: Implement `AccessControlHub.cpp`**

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

AccessControlHub::~AccessControlHub()
{
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

bool AccessControlHub::isAccessEnabled() const { return m_service->isEnabled(); }

void AccessControlHub::initialize()
{
    // Config (through AppSettings so tests isolate via isolateForTesting()).
    AppSettings settings;
    const int rawPoll = settings.value(QStringLiteral("accessControl/pollIntervalMs"), 1500).toInt();
    const int pollMs = rawPoll >= 250 ? rawPoll : 1500;
    QString gateId = settings.value(QStringLiteral("accessControl/gateId"),
                                    QStringLiteral("turnstile")).toString().trimmed();
    if (gateId.isEmpty()) gateId = QStringLiteral("turnstile");

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

    m_service->enable(descriptor, QVariantMap{
        {QStringLiteral("pollIntervalMs"), pollMs},
        {QStringLiteral("gateId"), gateId},
    });
}
```

- [ ] **Step 6: Create `AccessControlSingleton.h`**

```cpp
#ifndef ACCESSCONTROLSINGLETON_H
#define ACCESSCONTROLSINGLETON_H

#include <QQmlEngine>
#include "AccessControlHub.h"

// Registration shim only — never instantiated by QML. Exposes the app-owned
// AccessControlHub instance as the "AccessControl" QML singleton. A QML_FOREIGN
// wrapper (not QML_SINGLETON on the hub directly) because the hub is
// default-constructible in main(); Qt would otherwise prefer the default
// constructor over create() and hand QML a separate, uninitialized instance.
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

- [ ] **Step 7: Run the test to verify it passes**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3 --target tst_accesscontrolhub
ctest --test-dir C:/b/loams-sp3 -R tst_accesscontrolhub --output-on-failure
```
Expected: PASS (4/4).

- [ ] **Step 8: Commit** (via the `commit` skill)

Subject: `feat(accesscontrol): add AccessControlHub composition root + QML singleton`.

---

## Task 4: `KioskViewModel::onEntryObserved` + `showUnknownEntry`

**Files:**
- Modify: `qt-app/quick/viewmodels/KioskViewModel.h`, `qt-app/quick/viewmodels/KioskViewModel.cpp`
- Test: `qt-app/quick/tests/tst_kioskviewmodel.cpp`

**Interfaces:**
- Consumes: the `AccessEntry` map shape from Task 3 (`hasStudent`, `student`).
- Produces: `Q_INVOKABLE void KioskViewModel::onEntryObserved(const QVariantMap &entry);` and `void KioskViewModel::showUnknownEntry();`.

- [ ] **Step 1: Write the failing tests**

Append to `qt-app/quick/tests/tst_kioskviewmodel.cpp` — add to the `private slots:` block and implement:

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
Expected: FAIL to compile (`onEntryObserved` not a member).

- [ ] **Step 3: Declare the members in `KioskViewModel.h`**

After `Q_INVOKABLE void requestGuest();` in the public QML entry points:

```cpp
    // Access Control (Sub-plan 3): a confirmed gate entry from the
    // AccessControl singleton. hasStudent -> display + count; else neutral toast.
    Q_INVOKABLE void onEntryObserved(const QVariantMap &entry);
```

And in the private helpers (near `setStatus`):

```cpp
    void showUnknownEntry();
```

Add `#include <QVariantMap>` to the header includes.

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

- [ ] **Step 6: Commit** (via the `commit` skill)

Subject: `feat(kiosk): route observed gate entries to the kiosk display`.

---

## Task 5: App wiring + existing-test fixup

**Files:**
- Modify: `qt-app/quick/main.cpp`, `qt-app/quick/qml/kiosk/KioskScreen.qml`
- Modify: `qt-app/quick/tests/tst_appshell.cpp`, `qt-app/quick/tests/tst_qml_kiosk.cpp`

**Interfaces:**
- Consumes: `AccessControlHub` (Task 3), the `AccessControl` QML singleton (Task 3), `KioskViewModel::onEntryObserved` (Task 4).
- Produces: the fully wired app (flag-gated) and green QML harnesses.

This task's deliverable is verified by the **existing** QML suites staying green with the singleton present, plus a clean `WITSQuick` build. The `Connections` block breaks `tst_appshell`/`tst_qml_kiosk` the moment it is added (the singleton must resolve), so the harness fixes and the wiring land together.

- [ ] **Step 1: Add the failing wiring to `tst_appshell.cpp` first (red)**

Update `TestAppShell::loadsWithZeroWarnings()` to construct and install a **disabled** hub before loading, but do **not** yet touch `KioskScreen.qml` — run it to confirm the harness still passes (baseline), then in Step 2 add the QML `Connections` that requires the hub. Concretely, add the include and instance:

```cpp
#include "AccessControlHub.h"
// ... inside loadsWithZeroWarnings(), before creating the engine:
    AccessControlHub hub;              // default settings, no WITS_ACCESS_CONTROL -> disabled
    AccessControlHub::setInstance(&hub);
    // (hub is a stack local; it outlives `engine` below by declaration order)
```

Move the `QQmlApplicationEngine engine;` declaration to **after** `setInstance(&hub)` so the hub outlives the engine.

- [ ] **Step 2: Add the QML `Connections` to `KioskScreen.qml`**

Inside the root `Rectangle` (e.g. right after the `GuestViewModel { id: guestVm }` line), add:

```qml
    // Access Control (Sub-plan 3): a confirmed gate entry surfaces natively.
    // Presentation-scoped — this Connections lives and dies with the kiosk
    // surface. The AccessControl singleton is inert unless accessControl.enabled.
    Connections {
        target: AccessControl
        function onEntryObserved(entry) { kioskVm.onEntryObserved(entry) }
    }
```

- [ ] **Step 3: Fix `tst_qml_kiosk.cpp` to install a disabled hub**

The `Setup::qmlEngineAvailable` runs before the QML fixtures instantiate `KioskScreen`, so install the hub there:

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
        // Robust against an inherited WITS_ACCESS_CONTROL: never initialize(),
        // so the hub is a live-but-disabled singleton (no polling).
        static AccessControlHub hub;
        AccessControlHub::setInstance(&hub);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml_kiosk, Setup)
#include "tst_qml_kiosk.moc"
```

`tst_qml_kiosk` and `tst_appshell` already link `witsquickmodule` (which now contains `AccessControlHub.cpp`), so no CMake change is needed for them.

- [ ] **Step 4: Wire `main.cpp`**

Add the include and, after the cached-branding block and **before** `QQmlApplicationEngine engine;`:

```cpp
#include "AccessControlHub.h"
// ...
    AccessControlHub accessControl;      // stack-owned; outlives `engine`
    accessControl.initialize();          // reads the flag; polls only if enabled
    AccessControlHub::setInstance(&accessControl);

    QQmlApplicationEngine engine;
```

- [ ] **Step 5: Build everything and run the full quick suite**

```
cmake -S qt-app -B C:/b/loams-sp3 -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build C:/b/loams-sp3
ctest --test-dir C:/b/loams-sp3 --output-on-failure
```
Expected: clean build (no new warnings); **all** tests pass, including `tst_appshell` (zero-QML-warnings) and `tst_qml_kiosk` with the `AccessControl` singleton resolving.

- [ ] **Step 6: Manual smoke (documented, run once)**

With the app flag off (default), launch `WITSQuick` and confirm the kiosk behaves exactly as before (no Access Control effect). Then, with a seeded `turnstile_events` row and `WITS_ACCESS_CONTROL=1`, confirm the student surfaces natively (see spec §Verification). Record the result in the commit/PR body.

- [ ] **Step 7: Commit** (via the `commit` skill)

Subject: `feat(accesscontrol): wire the hub singleton into main + the kiosk surface`.

---

## Self-Review

**1. Spec coverage:**
- §1 parser + `EntryEventResult` + photo composition + local→UTC + strict validity → Task 1. ✅
- §2 provider: baseline-once, reconnect-preserves-cursor + processes response, drain oldest-first, `eventId>since` guard, timeout+abort, generation guard, single `Degraded`, provider-owned reply, `gateId` config, never-Error-at-runtime → Task 2. ✅
- §3 unknown-student convention (empty subject) → encoded in `toAccessEntry` (Task 3) + provider emit (Task 2). ✅
- §4 hub ownership/teardown order, NAM owned-vs-injected seam, register-when-disabled, event→`QVariantMap` map → Task 3. ✅
- §5 `QML_FOREIGN` singleton wrapper + `create()` fail-fast + `main.cpp` order + CMake SOURCES + existing-QML-test install → Tasks 3 & 5. ✅
- §6 kiosk `onEntryObserved` (hasStudent branch / `showUnknownEntry`) + presentation-scoped `Connections` (`kioskVm`) → Tasks 4 & 5. ✅
- §7 enablement precedence + `AppSettings` + `pollIntervalMs` clamp + `gateId` default → Task 3. ✅
- Testing section (baseline→next-poll, reconnect-emits-once, drain-loop guard, transport failure, unknown/known kiosk branch, disabled-inert, env precedence) → Tasks 1–5. ✅
- Forward-note (per-poll comm-health deferred to Sub-plan 4): intentionally **not** implemented; the provider does not touch `HealthMonitor`. ✅

**2. Placeholder scan:** No TBD/TODO/"handle edge cases"/"similar to". Every code step shows full code. ✅

**3. Type consistency:** `EntryEventResult`/`parseEntryEvent(QByteArray, QUrl)` (Task 1) is consumed with the same signature in Task 2. `TurnstileProvider(nam, QUrl, QVariantMap, parent)` + `defaultDescriptor()` (Task 2) match the creator lambda in Task 3. `AccessControlHub(QNetworkAccessManager*, QObject*)`, `initialize()`, `isAccessEnabled()`, `toAccessEntry`, `instance()/setInstance()`, `entryObserved(QVariantMap)` (Task 3) match Tasks 4/5 usage. `AccessEntry` keys (`hasStudent`/`student`/`eventId`/`at`) are identical in Task 3 (`toAccessEntry`), Task 3 tests, and Task 4 (`onEntryObserved`). ✅

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-09-29-access-control-provider-kiosk.md`. Two execution options:

1. **Subagent-Driven (recommended)** — a fresh subagent per task, two-stage review between tasks, fast iteration.
2. **Inline Execution** — execute tasks in this session with checkpoints.

Which approach?
