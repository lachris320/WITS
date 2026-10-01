#include <QtTest>
#include <QSignalSpy>
#include <QUrlQuery>
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
    void failureStopsTimerAndPreservesCursor();
    void stopHaltsSteadyStateTimer();
    void stopFromEntrySlotHaltsDrain();
    void baselineSuccessEmitsPolledOnce();
    void polledOnValidEmptyPolls();
    void polledOnEntryPolls();
    void noPolledOnTransportError();
    void noPolledOnNon2xx();
    void noPolledOnMalformed();
    void noPolledOnNonAdvancingEntry();
    void noPolledFromReplyCompletingAfterStop();

private:
    static QString sinceOf(const QUrl &url)
    { return QUrlQuery(url).queryItemValue(QStringLiteral("since")); }
    static int connectedCount(const QSignalSpy &states)
    {
        int n = 0;
        for (const auto &args : states)
            if (qvariant_cast<ConnectionState>(args.at(0)) == ConnectionState::Connected) ++n;
        return n;
    }
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
    QCOMPARE(sinceOf(nam.lastUrl), QStringLiteral("5"));
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
    // One-in-flight: baseline + 2 entries + drain-end empty == 4 requests, and
    // peak concurrency stayed at exactly one (overlap would push maxActive >= 2).
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 4, 3000);
    QCOMPARE(nam.maxActive(), 1);
    QCOMPARE(nam.abortCount(), 0);     // an overlap masked as maxActive==1 would abort
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

    const int beforeRestart = nam.requestCount();   // == 2
    nam.enqueue(entryPayload(4, 4));    // reconnect ?since=3 -> entry 4 (must emit once)
    nam.enqueue(emptyPayload(4));       // drain end
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QCOMPARE(qvariant_cast<AccessEvent>(events.at(0).at(0)).correlationId, QStringLiteral("4"));
    // The reconnect request ITSELF used the preserved cursor (no re-baseline).
    QCOMPARE(sinceOf(nam.urls.at(beforeRestart)), QStringLiteral("3"));
    // The following drain request advanced the cursor to 4.
    QTRY_VERIFY_WITH_TIMEOUT(nam.urls.size() > beforeRestart + 1, 3000);
    QCOMPARE(sinceOf(nam.urls.at(beforeRestart + 1)), QStringLiteral("4"));
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
    QCOMPARE(nam.requestCount(), 2);    // baseline + the anomalous poll; no drain

    // Reconnect anomaly: the FIRST response after a restart is non-advancing.
    // It must go Connecting -> Degraded with NO transient Connected (which
    // would reset the service's backoff every cycle).
    nam.enqueue(entryPayload(7, 7));
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 3, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    QCOMPARE(connectedCount(states), 0);
    QCOMPARE(degradedCount(states), 1);
    QCOMPARE(events.count(), 0);
    QTest::qWait(600);                  // > 2 * pollInterval(250): no runaway polling
    QCOMPARE(nam.requestCount(), 3);
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
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 1, 3000);
    p.stop();                                      // aborts the in-flight reply
    QCOMPARE(nam.abortCount(), 1);
    QTest::qWait(100);                             // let the aborted reply's finished fire
    QCOMPARE(events.count(), 0);                   // stale (generation-bumped) reply dropped
    // The dropped reply must NOT run the failure path: zero Degraded proves the
    // stop() generation bump suppressed it (without the guard, abort->fail()
    // would emit Degraded here).
    QCOMPARE(degradedCount(states), 0);
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
    QSignalSpy states(&p, &IAccessProvider::stateChanged);
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 1, 3000);   // reply-1 in flight
    p.start();                                                // restart: gen bump + abort reply-1
    QVERIFY(nam.abortCount() >= 1);
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Connected, 3000);  // baselined from reply-2
    QCOMPARE(events.count(), 0);                              // reply-1 never produced an effect
    // The aborted reply-1 must be DROPPED by the generation guard, not run
    // through fail(): assert zero Degraded transitions. Without the start()
    // generation bump, reply-1's abort would emit a transient Degraded that
    // reply-2 then hides — this assertion is what actually detects that.
    QCOMPARE(degradedCount(states), 0);
    QTRY_COMPARE_WITH_TIMEOUT(sinceOf(nam.lastUrl), QStringLiteral("9"), 3000);
    p.stop();
}

void TestTurnstileProvider::failureStopsTimerAndPreservesCursor()
{
    // After a failure the provider must (a) leave the cursor untouched and
    // (b) stop its own timer — reconnect is the service's job, not a
    // provider-owned retry.
    SequencedNam nam;
    nam.enqueue(emptyPayload(5));                                  // baseline: cursor = 5
    nam.enqueue(QByteArray(), QNetworkReply::HostNotFoundError);   // steady poll fails
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    p.start();
    QTRY_VERIFY_WITH_TIMEOUT(p.state() == ConnectionState::Degraded, 3000);
    const int afterFailure = nam.requestCount();                  // baseline + failed poll == 2
    QTest::qWait(600);                                            // > 2 * pollInterval(250)
    QCOMPARE(nam.requestCount(), afterFailure);                   // NO provider self-retry
    // Cursor preserved: a service-triggered restart resumes from since=5.
    nam.enqueue(emptyPayload(5));
    p.start();
    // A real NEW request must have been issued (not the stale lastUrl)...
    QTRY_VERIFY_WITH_TIMEOUT(nam.requestCount() > afterFailure, 3000);
    // ...and that reconnect request (index == afterFailure) carried the preserved cursor.
    QCOMPARE(sinceOf(nam.urls.at(afterFailure)), QStringLiteral("5"));
    p.stop();
}

void TestTurnstileProvider::stopHaltsSteadyStateTimer()
{
    SequencedNam nam;
    nam.enqueue(emptyPayload(0));       // baseline (arms the steady timer)
    nam.enqueue(emptyPayload(0));       // steady empty poll (re-arms the timer)
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(nam.requestCount(), 2, 3000);
    QTest::qWait(20);                   // let the steady empty reply's finished() run (timer re-armed)
    p.stop();                           // must kill the armed pollInterval timer
    const int atStop = nam.requestCount();
    QTest::qWait(600);                  // > 2 * pollInterval(250)
    QCOMPARE(nam.requestCount(), atStop);
}

void TestTurnstileProvider::stopFromEntrySlotHaltsDrain()
{
    // accessEvent is emitted synchronously; a subscriber that calls stop() from
    // its slot must halt the drain (no further request, no further emit).
    SequencedNam nam;
    nam.enqueue(emptyPayload(0));       // baseline
    nam.enqueue(entryPayload(2, 1));    // entry 1 -> subscriber stops the provider
    nam.enqueue(entryPayload(2, 2));    // would-be next entry: must never be requested
    TurnstileProvider p(&nam, QUrl("http://localhost/loams_api/"), cfg());
    QSignalSpy events(&p, &IAccessProvider::accessEvent);
    connect(&p, &IAccessProvider::accessEvent, &p, [&p]() { p.stop(); });
    p.start();
    QTRY_COMPARE_WITH_TIMEOUT(events.count(), 1, 3000);
    QTest::qWait(600);                  // > 2 * pollInterval(250)
    QCOMPARE(events.count(), 1);
    QCOMPARE(nam.requestCount(), 2);    // baseline + entry 1 only; no drain request
}

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

QTEST_MAIN(TestTurnstileProvider)
#include "tst_turnstileprovider.moc"
