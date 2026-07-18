#include <QtTest>
#include <QSignalSpy>
#include <QDate>
#include "VisitLogsViewModel.h"
#include "VisitLogRowsModel.h"

class TestVisitLogsViewModel : public QObject
{
    Q_OBJECT
private slots:
    void defaultsToStudentToday();
    void applyStudentVisitsPopulatesRowsWithDashTimeOut();
    void applyGuestVisitsPopulatesGuestColumns();
    void applyInvalidSetsErrorText();
    void setModeEmitsAndChanges();
    void formatWeekLabelSpansMonToSun();
    void supersededRequestSeqIsNotCurrent();
    void setRangeUpdatesRangeLabelImmediately();
    void parseErrorAfterGoodDataClearsStaleRowsAndRecomputesLabel();
};

void TestVisitLogsViewModel::defaultsToStudentToday()
{
    VisitLogsViewModel vm;
    QCOMPARE(vm.mode(), VisitLogsViewModel::Student);
    QCOMPARE(vm.range(), VisitLogsViewModel::Today);
}

void TestVisitLogsViewModel::applyStudentVisitsPopulatesRowsWithDashTimeOut()
{
    VisitLogsViewModel vm;
    QSignalSpy spy(&vm, &VisitLogsViewModel::dataChanged);
    vm.applyStudentVisits(R"({
        "status":"success","count":1,
        "visits":[{"date":"2026-07-13","name":"Maria Santos","course":"BSCE",
                   "department":"CE","time_in":"08:14"}]
    })");
    QVERIFY(spy.count() >= 1);
    QCOMPARE(vm.count(), 1);
    VisitLogRowsModel *rows = vm.rows();
    const QModelIndex i0 = rows->index(0, 0);
    QCOMPARE(rows->data(i0, VisitLogRowsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::CourseRole).toString(), QStringLiteral("BSCE"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::TimeInRole).toString(), QStringLiteral("08:14"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::TimeOutRole).toString(), QStringLiteral("—"));
    QVERIFY(vm.errorText().isEmpty());
}

void TestVisitLogsViewModel::applyGuestVisitsPopulatesGuestColumns()
{
    VisitLogsViewModel vm;
    vm.setMode(VisitLogsViewModel::Guest);
    vm.applyGuestVisits(R"({
        "status":"success","count":1,
        "visitors":[{"name":"Ana Cruz","company":"Acme","purpose":"Research",
                     "time_in":"2026-07-13 09:20:00"}]
    })");
    QCOMPARE(vm.count(), 1);
    VisitLogRowsModel *rows = vm.rows();
    const QModelIndex i0 = rows->index(0, 0);
    QCOMPARE(rows->data(i0, VisitLogRowsModel::NameRole).toString(), QStringLiteral("Ana Cruz"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::CompanyRole).toString(), QStringLiteral("Acme"));
    QCOMPARE(rows->data(i0, VisitLogRowsModel::PurposeRole).toString(), QStringLiteral("Research"));
}

void TestVisitLogsViewModel::applyInvalidSetsErrorText()
{
    VisitLogsViewModel vm;
    vm.applyStudentVisits(R"({"status":"error"})");
    QVERIFY(!vm.errorText().isEmpty());
    QCOMPARE(vm.count(), 0);
}

void TestVisitLogsViewModel::setModeEmitsAndChanges()
{
    VisitLogsViewModel vm;
    QSignalSpy spy(&vm, &VisitLogsViewModel::modeChanged);
    vm.setMode(VisitLogsViewModel::Guest);
    QCOMPARE(vm.mode(), VisitLogsViewModel::Guest);
    QVERIFY(spy.count() >= 1);
}

void TestVisitLogsViewModel::formatWeekLabelSpansMonToSun()
{
    // 2026-07-13 is a Monday.
    QCOMPARE(VisitLogsViewModel::formatWeekLabel(QDate(2026, 7, 13)),
             QStringLiteral("Jul 13 – Jul 19, 2026"));
}

// Pins the in-flight request-generation guard's increment/compare arithmetic
// (VisitLogsViewModel::nextRequestSeq / isCurrentRequest) — the mechanism
// refresh()'s reply-finished lambdas use to drop a superseded reply (e.g. a
// slow Guest POST that returns after the user has already switched to
// Student, which would otherwise render guest rows under student columns).
//
// What this proves: once a second request is issued, the seq handed out to
// the first request is no longer "current" — so a reply carrying it would be
// dropped by the `if (!isCurrentRequest(seq)) return;` guard in refresh().
// What this does NOT prove: that refresh() actually wires nextRequestSeq()/
// isCurrentRequest() into its QNetworkReply::finished lambdas correctly, or
// that a real out-of-order network race is fixed end-to-end — refresh() opens
// a genuine QNetworkAccessManager request against ApiConfig::endpoint(), so a
// unit test cannot drive it without hitting the network (against this
// project's "no real network in unit tests" rule). That wiring was verified
// by inspection of VisitLogsViewModel.cpp instead.
void TestVisitLogsViewModel::supersededRequestSeqIsNotCurrent()
{
    VisitLogsViewModel vm;
    const quint64 first = vm.nextRequestSeq();     // e.g. Guest click -> POST in flight
    QVERIFY(vm.isCurrentRequest(first));           // still the only request outstanding

    const quint64 second = vm.nextRequestSeq();    // Student click before the POST returns
    QVERIFY(!vm.isCurrentRequest(first));          // first request is now superseded
    QVERIFY(vm.isCurrentRequest(second));          // second request is the current one
}

// Root cause of the user-reported bug: refresh()'s network-error branch used
// to setError(...) and return without touching m_rangeLabel, which was only
// ever recomputed inside applyStudentVisits/applyGuestVisits — i.e. only
// after data arrived. If the fetch never succeeded, the label silently kept
// showing the PREVIOUS selection's range while the toggle itself (mode/range)
// had already switched to Week. This test drives only the public selection
// setter — no reply, no apply* call — proving the label is now derived from
// the user's selection eagerly, not from the last successful reply.
void TestVisitLogsViewModel::setRangeUpdatesRangeLabelImmediately()
{
    VisitLogsViewModel vm;
    QCOMPARE(vm.range(), VisitLogsViewModel::Today);
    QVERIFY(!vm.rangeLabel().contains(QStringLiteral("–")));   // "Today, ..." has no en-dash range

    vm.setRange(VisitLogsViewModel::Week);   // fires refresh() as a side effect (fire-and-forget,
                                              // same pattern already used by setModeEmitsAndChanges);
                                              // no QTest::qWait, so no reply can have arrived yet.

    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
    QCOMPARE(vm.rangeLabel(), VisitLogsViewModel::formatWeekLabel(monday));
}

// The reviewer finding: network-error branches didn't clear prior state while
// parse-error branches did, making the two error paths internally
// inconsistent. This seeds a real (nonzero) prior result set — unlike the
// pre-existing applyInvalidSetsErrorText test, which starts from a fresh VM
// where count is already 0 by construction and so can't distinguish "cleared"
// from "never populated" — then drives a parse failure via the network-free
// seam and asserts the stale row is actually dropped, not merely absent.
void TestVisitLogsViewModel::parseErrorAfterGoodDataClearsStaleRowsAndRecomputesLabel()
{
    VisitLogsViewModel vm;
    vm.applyStudentVisits(R"({
        "status":"success","count":1,
        "visits":[{"date":"2026-07-13","name":"Maria Santos","course":"BSCE",
                   "department":"CE","time_in":"08:14"}]
    })");
    QCOMPARE(vm.count(), 1);

    vm.setRange(VisitLogsViewModel::Week);
    vm.applyStudentVisits(R"({"status":"error"})");

    QCOMPARE(vm.count(), 0);
    QCOMPARE(vm.rows()->rowCount(), 0);
    QVERIFY(!vm.errorText().isEmpty());
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
    QCOMPARE(vm.rangeLabel(), VisitLogsViewModel::formatWeekLabel(monday));
}

QTEST_MAIN(TestVisitLogsViewModel)
#include "tst_visitlogsviewmodel.moc"
