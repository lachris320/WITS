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
    void refresh_repeatedIdenticalFailureReEmitsErrorText();
    void refresh_clearsErrorButKeepsStaleAuthAndUpdatedAtWhileLoading();
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

void TestAccessControlViewModel::refresh_repeatedIdenticalFailureReEmitsErrorText()
{
    // Backend down -> Refresh -> same error again. The view raises its toast on
    // errorTextChanged, so refresh() must clear the error on start or the second
    // identical failure would be swallowed by setError's unchanged-string guard.
    CapturingNam nam(QByteArray(), QNetworkReply::HostNotFoundError, 0);
    AccessControlViewModel vm(nullptr, &nam);
    QSignalSpy err(&vm, &AccessControlViewModel::errorTextChanged);
    vm.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(err.count(), 1, 1000);          // "" -> message
    QCOMPARE(vm.errorText(), QStringLiteral("Network error. Please try again."));
    vm.refresh();
    QVERIFY(vm.errorText().isEmpty());                        // cleared when the request starts
    QCOMPARE(err.count(), 2);
    QTRY_COMPARE_WITH_TIMEOUT(err.count(), 3, 1000);          // same message raised AGAIN
    QCOMPARE(vm.errorText(), QStringLiteral("Network error. Please try again."));
}

void TestAccessControlViewModel::refresh_clearsErrorButKeepsStaleAuthAndUpdatedAtWhileLoading()
{
    // Starting a refresh clears ONLY the error text. Rows, stale, updatedAt and
    // authFailure are untouched until the reply lands.
    CapturingNam nam(QByteArray(), QNetworkReply::HostNotFoundError, 0);
    AccessControlViewModel vm(nullptr, &nam);
    vm.applyRecent(feedBody());
    const QString firstUpdated = vm.updatedAt();
    vm.applyRecent("not json");                               // stale + error
    QVERIFY(vm.stale());
    QVERIFY(!vm.errorText().isEmpty());
    vm.refresh();
    QVERIFY(vm.loading());
    QVERIFY(vm.errorText().isEmpty());
    QVERIFY(vm.stale());                                      // stays stale while loading
    QCOMPARE(vm.updatedAt(), firstUpdated);
    QCOMPARE(vm.entries()->rowCount(), 2);
    QVERIFY(!vm.emptyFeed());
    QVERIFY(!vm.initialLoadFailed());
    QTRY_VERIFY_WITH_TIMEOUT(!vm.loading(), 1000);

    // authFailure survives the start of a refresh (only a later success resets it).
    AccessControlViewModel authVm(nullptr, &nam);
    authVm.applyRecent(R"({"status":"error","message":"Invalid admin key"})");
    QVERIFY(authVm.authFailure());
    authVm.refresh();
    QVERIFY(authVm.authFailure());
    QVERIFY(authVm.errorText().isEmpty());
    QVERIFY(!authVm.initialLoadFailed());
    QTRY_VERIFY_WITH_TIMEOUT(!authVm.loading(), 1000);
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
