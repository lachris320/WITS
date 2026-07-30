#include <QtTest>
#include <QSignalSpy>
#include "DatabaseViewModel.h"
#include "StudentsTableModel.h"
#include "studentdata.h"

class TestDatabaseViewModel : public QObject
{
    Q_OBJECT
private slots:
    void departmentsLoadedPopulatesProp();
    void studentsLoadedFillTable();
    void setDepartmentReloadsCoursesAndClears();
    void networkErrorSetsErrorAndClearsRows();
};

// A DatabaseViewModel test-ctor takes a StudentController*; but StudentController
// needs a QNetworkAccessManager. Reuse the CapturingNam harness (qt-app/testsupport)
// so no live network is hit; drive results by emitting the controller's signals
// via a friend/test seam is heavy — instead assert the VM's slot handlers directly
// (they are public, like SearchViewModel's onSearchFinished).
void TestDatabaseViewModel::departmentsLoadedPopulatesProp()
{
    DatabaseViewModel vm;
    vm.onDepartmentsLoaded({"CCS","CBA"});
    QCOMPARE(vm.departments(), (QStringList{"CCS","CBA"}));
}

void TestDatabaseViewModel::studentsLoadedFillTable()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId="A"; r.name="Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    QCOMPARE(vm.students()->count(), 1);
    QVERIFY(!vm.loading());
}

void TestDatabaseViewModel::setDepartmentReloadsCoursesAndClears()
{
    DatabaseViewModel vm;
    vm.onCoursesLoaded({"BSIT"});
    QCOMPARE(vm.courses(), (QStringList{"BSIT"}));
    vm.setCourse("BSIT");
    QCOMPARE(vm.course(), QStringLiteral("BSIT"));
    vm.setDepartment("CCS");
    QCOMPARE(vm.department(), QStringLiteral("CCS"));
    QCOMPARE(vm.course(), QString());   // Critical fix: dept change drops the stale course filter
}

void TestDatabaseViewModel::networkErrorSetsErrorAndClearsRows()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId="A"; r.name="Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.onSearchFailed("boom", 2);
    QVERIFY(!vm.errorText().isEmpty());
    QCOMPARE(vm.students()->count(), 0);   // stale rows cleared
    QVERIFY(!vm.loading());
}

QTEST_MAIN(TestDatabaseViewModel)
#include "tst_databaseviewmodel.moc"
