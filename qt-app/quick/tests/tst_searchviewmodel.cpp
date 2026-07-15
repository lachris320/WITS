#include <QtTest>
#include <QSignalSpy>
#include "SearchViewModel.h"
#include "SearchResultsModel.h"
#include "studentdata.h"

class TestSearchViewModel : public QObject
{
    Q_OBJECT
private slots:
    void resultsPopulateWithVisits();
    void searchFailedSetsErrorText();
    void searchFailedClearsStaleResults();
    void coursesLoadedExposesChips();
    void supersededSearchFinishedReplyIsDropped();
    void supersededSearchFailedReplyIsDropped();
    void setDepartmentUpdatesPropertyAndEmits();
    void setDepartmentSameValueIsNoop();
};

void TestSearchViewModel::resultsPopulateWithVisits()
{
    SearchViewModel vm;
    StudentRecord r;
    r.schoolId = "2023-0001";
    r.name = "Maria Santos";
    r.course = "BSCE";
    r.department = "CE";
    r.visits = 42;
    vm.onSearchFinished(SearchOutcome::Results, { r }, QString(), QStringLiteral("Maria"), 0);

    QCOMPARE(vm.results()->rowCount(), 1);
    const QModelIndex i0 = vm.results()->index(0, 0);
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::VisitsRole).toInt(), 42);
    QVERIFY(vm.errorText().isEmpty());
}

void TestSearchViewModel::searchFailedSetsErrorText()
{
    SearchViewModel vm;
    vm.onSearchFailed(QStringLiteral("boom"), 0);
    QVERIFY(!vm.errorText().isEmpty());
    // The user-facing string must not leak the raw transport error.
    QVERIFY(!vm.errorText().contains(QStringLiteral("boom")));
}

// Reviewer finding (scope correction): onSearchFailed had the identical
// error-path-inconsistency shape as Dashboard/VisitLogs — onSearchFinished's
// parse-failure path unconditionally calls setRecords(records) (line 40,
// clearing stale results), but onSearchFailed left m_results untouched. So a
// search that returns good results, then a subsequent search that fails on
// the network, would keep showing the PREVIOUS search's rows behind the
// error banner. Fully network-free: onSearchFailed is a public slot.
void TestSearchViewModel::searchFailedClearsStaleResults()
{
    SearchViewModel vm;
    StudentRecord r;
    r.schoolId = "2023-0001";
    r.name = "Maria Santos";
    r.course = "BSCE";
    r.department = "CE";
    r.visits = 42;
    vm.onSearchFinished(SearchOutcome::Results, { r }, QString(), QStringLiteral("Maria"), 0);
    QCOMPARE(vm.results()->rowCount(), 1);

    vm.onSearchFailed(QStringLiteral("boom"), 0);

    QCOMPARE(vm.results()->rowCount(), 0);
    QVERIFY(!vm.errorText().isEmpty());
}

void TestSearchViewModel::coursesLoadedExposesChips()
{
    SearchViewModel vm;
    QSignalSpy spy(&vm, &SearchViewModel::coursesChanged);
    vm.onCoursesLoaded({ QStringLiteral("BSCE"), QStringLiteral("BSEE") });
    QVERIFY(spy.count() >= 1);
    QCOMPARE(vm.courses().size(), 2);
    QCOMPARE(vm.courses().at(0), QStringLiteral("BSCE"));
}

// StudentController fires an independent QNetworkReply per searchStudents()
// call and does not abort/serialize a prior in-flight search — every
// course-chip click calls search(), so two racing searches can resolve in
// ARRIVAL order rather than request order. StudentController now echoes a
// monotonic request id through searchFinished/searchFailed; SearchViewModel
// must keep only the result from the highest id it has seen, because ids are
// assigned in issue order (a lower id can never legitimately arrive after a
// higher one — it can only be a stale, superseded reply).
//
// This drives onSearchFinished directly (fully network-free, per the
// project's existing seam) with the NEWER request's id (2) resolving first,
// then the OLDER, already-superseded request's id (1) resolving late — the
// exact "last arrival wins" ordering that produces the bug. Against
// pre-fix code (no requestId parameter at all) this test does not compile;
// against a fix that accepts every reply unconditionally, the second call
// would overwrite the row with the stale "Older Result", which is what this
// asserts against.
void TestSearchViewModel::supersededSearchFinishedReplyIsDropped()
{
    SearchViewModel vm;
    StudentRecord newer;
    newer.schoolId = "2023-0002";
    newer.name = "Newer Result";
    StudentRecord older;
    older.schoolId = "2023-0001";
    older.name = "Older Result";

    vm.onSearchFinished(SearchOutcome::Results, { newer }, QString(), QStringLiteral("term"), 2);
    vm.onSearchFinished(SearchOutcome::Results, { older }, QString(), QStringLiteral("term"), 1);

    QCOMPARE(vm.results()->rowCount(), 1);
    const QModelIndex i0 = vm.results()->index(0, 0);
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::NameRole).toString(),
             QStringLiteral("Newer Result"));
}

// Same race, but the STALE reply arrives via the failure path instead of a
// second success. A late network error for a superseded request must not
// clobber the newer, already-applied results with the generic error banner.
void TestSearchViewModel::supersededSearchFailedReplyIsDropped()
{
    SearchViewModel vm;
    StudentRecord r;
    r.schoolId = "2023-0001";
    r.name = "Maria Santos";

    vm.onSearchFinished(SearchOutcome::Results, { r }, QString(), QStringLiteral("term"), 2);
    vm.onSearchFailed(QStringLiteral("boom"), 1);   // late failure for the superseded id=1 request

    QCOMPARE(vm.results()->rowCount(), 1);   // newer result untouched
    QVERIFY(vm.errorText().isEmpty());       // no stale error banner
}

// setDepartment() fires a real StudentController::loadCourses() network
// request as a side effect (fire-and-forget, same pattern already used by
// tst_visitlogsviewmodel's setRangeUpdatesRangeLabelImmediately); no
// QTest::qWait, so no reply can have arrived by the time these assertions
// run — property/signal state is checked synchronously.
void TestSearchViewModel::setDepartmentUpdatesPropertyAndEmits()
{
    SearchViewModel vm;
    QSignalSpy spy(&vm, &SearchViewModel::departmentChanged);
    vm.setDepartment(QStringLiteral("CE"));
    QCOMPARE(vm.department(), QStringLiteral("CE"));
    QCOMPARE(spy.count(), 1);
}

// A redundant setDepartment() call (same value as current) must not re-fire
// departmentChanged (and, by extension, must not re-issue a courses request).
void TestSearchViewModel::setDepartmentSameValueIsNoop()
{
    SearchViewModel vm;
    vm.setDepartment(QStringLiteral("CE"));
    QSignalSpy spy(&vm, &SearchViewModel::departmentChanged);
    vm.setDepartment(QStringLiteral("CE"));
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestSearchViewModel)
#include "tst_searchviewmodel.moc"
