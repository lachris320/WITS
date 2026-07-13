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
    void coursesLoadedExposesChips();
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
    vm.onSearchFinished(SearchOutcome::Results, { r }, QString(), QStringLiteral("Maria"));

    QCOMPARE(vm.results()->rowCount(), 1);
    const QModelIndex i0 = vm.results()->index(0, 0);
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(vm.results()->data(i0, SearchResultsModel::VisitsRole).toInt(), 42);
    QVERIFY(vm.errorText().isEmpty());
}

void TestSearchViewModel::searchFailedSetsErrorText()
{
    SearchViewModel vm;
    vm.onSearchFailed(QStringLiteral("boom"));
    QVERIFY(!vm.errorText().isEmpty());
    // The user-facing string must not leak the raw transport error.
    QVERIFY(!vm.errorText().contains(QStringLiteral("boom")));
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

QTEST_MAIN(TestSearchViewModel)
#include "tst_searchviewmodel.moc"
