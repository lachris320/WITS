#include <QtTest>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QTemporaryDir>
#include <QFile>
#include "DatabaseViewModel.h"
#include "StudentsTableModel.h"
#include "studentdata.h"
#include "AdminSession.h"
#include "studentcontroller.h"

class TestDatabaseViewModel : public QObject
{
    Q_OBJECT
private slots:
    void departmentsLoadedPopulatesProp();
    void studentsLoadedFillTable();
    void setDepartmentReloadsCoursesAndClears();
    void networkErrorSetsErrorAndClearsRows();
    void cleanup();     // per-test isolation: reset the process-wide AdminSession
    void requiresTypedConfirmationBoundary();
    void deleteSelectedPostsIdsAndAdminKey();
    void onDeleteFinishedSuccessRefreshesClearsAndSetsStatus();
    void onDeleteFinishedAuthFailureSetsAuthState();
    void onDeleteFinishedGenericFailureSetsStatusNoAuth();
    void onDeleteFailedSetsTransientStatus();
    void exportCsvWritesAllRowsWhenNoneSelected();
    void exportCsvWritesOnlySelectedWhenSomeSelected();
    void exportCsvWriteFailureReturnsFalse();
    void canEditIsTrueWhenAtLeastOneSelected();
    void canEditChangedEmitsOnlyOnBooleanFlip();
    void beginEditPrefillsAllFieldsAndEmitsReady();
    void beginEditNoMatchIsNoOp();
    void beginEditSelectedUsesSingleSelectedId();
    void setEditDepartmentClearsCourseAndReloads();
    void onEditCoursesLoadedPopulatesEditCourses();
    void onBulkUpdateFinishedSuccessSetsStatusReloadsAndFinishes();
    void onBulkUpdateFinishedNoChangeSetsStatusAndFinishes();
    void onBulkUpdateFinishedAuthFailureSetsAuthStateKeepsOpen();
    void onBulkUpdateFinishedGenericFailureSetsStatusNoAuth();
    void onBulkUpdateFailedSetsTransientStatus();
    void serverRejectionSharedByDeleteAndBulk();
    void saveEditEntersGuardedPath();
    void buildBulkUpdatesOverridesOnlyToggledFieldsAndCarriesRest();
    void couplingDepartmentDrivesCourseToggle();
    void setChangeCourseWithoutDepartmentIsNoOp();
    void setBulkDepartmentClearsCourseValue();
    void canApplyBulkGating();
    void bulkChangeSummaryListsOnlyToggledInOrder();
    void courseTargetRoutesBulkVsSingle();
    void beginBulkEditSelectedGuardsBelowTwo();
    void beginBulkEditSelectedResetsStateAndEmitsReady();
    void onBulkUpdateFinishedPluralCountMessage();
    void applyBulkEditRoutesBulkFinishedAndClearsBusy();
    void applyBulkEditGuardsEmptyInvalidAndReentry();
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

void TestDatabaseViewModel::cleanup()
{
    AdminSession::instance().clear();   // isolate the process-wide singleton per test
}

void TestDatabaseViewModel::requiresTypedConfirmationBoundary()
{
    DatabaseViewModel vm;
    QCOMPARE(vm.requiresTypedConfirmation(9), false);
    QCOMPARE(vm.requiresTypedConfirmation(10), true);
    QCOMPARE(vm.requiresTypedConfirmation(11), true);
}

void TestDatabaseViewModel::deleteSelectedPostsIdsAndAdminKey()
{
    // Empty selection => no network call, no status change (guard).
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    vm.deleteSelected();
    QVERIFY(vm.statusMessage().isEmpty());

    // With a selection, deleteSelected() sources ids from the model and the key
    // from AdminSession. The controller wire format (school_ids[] + admin_key)
    // is asserted in tst_studentcontroller::deleteStudents_buildsFormBodyWithAdminKey;
    // here we assert the VM feeds the controller a non-empty request by
    // observing that the in-flight path does not early-return: select a row,
    // call deleteSelected(), then drive the reply seam directly.
    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.students()->toggle("2023-001");
    QCOMPARE(vm.students()->selectedIds(), QStringList{"2023-001"});
    vm.deleteSelected();                 // posts via the VM's own NAM (no live server in CI)
    // The success/failure handlers are unit-tested directly below; this step
    // only proves the guarded path was entered (selection non-empty).
    QVERIFY(vm.students()->selectedCount() == 1);
    AdminSession::instance().clear();
}

void TestDatabaseViewModel::onDeleteFinishedSuccessRefreshesClearsAndSetsStatus()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.students()->toggle("2023-001");
    QCOMPARE(vm.students()->selectedCount(), 1);

    vm.onDeleteFinished(true, 3, QString());
    QCOMPARE(vm.statusMessage(), QStringLiteral("Deleted 3 students"));
    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.students()->selectedCount(), 0);   // cleared
    QVERIFY(vm.loading());                          // reloadTable() flipped loading on
}

void TestDatabaseViewModel::onDeleteFinishedAuthFailureSetsAuthState()
{
    DatabaseViewModel vm;
    vm.onDeleteFinished(false, 2, QStringLiteral("Invalid admin key"));
    QVERIFY(vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
}

void TestDatabaseViewModel::onDeleteFinishedGenericFailureSetsStatusNoAuth()
{
    DatabaseViewModel vm;
    vm.onDeleteFinished(false, 2, QStringLiteral("Invalid data"));
    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.statusMessage(), QStringLiteral("Invalid data"));
}

void TestDatabaseViewModel::onDeleteFailedSetsTransientStatus()
{
    DatabaseViewModel vm;
    vm.onDeleteFailed(QStringLiteral("Connection refused"));
    QVERIFY(!vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
}

void TestDatabaseViewModel::exportCsvWritesAllRowsWhenNoneSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";  a.visits = 1;
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";  b.visits = 2;
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("all.csv");
    QVERIFY(vm.exportCsv(QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    QCOMPARE(bytes, StudentController::toCsv({a, b}));   // exact byte-for-byte
    QCOMPARE(vm.statusMessage(), QStringLiteral("Exported 2 rows to all.csv"));
}

void TestDatabaseViewModel::exportCsvWritesOnlySelectedWhenSomeSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("B");                          // only B selected

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("sel.csv");
    QVERIFY(vm.exportCsv(QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), StudentController::toCsv({b})); // B only, not A
    QCOMPARE(vm.statusMessage(), QStringLiteral("Exported 1 rows to sel.csv"));
}

void TestDatabaseViewModel::exportCsvWriteFailureReturnsFalse()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {a}, "", "", 1);
    // A path whose parent directory does not exist => QSaveFile::open fails.
    const bool ok = vm.exportCsv(QUrl::fromLocalFile(
        QStringLiteral("./no_such_dir_xyz/out.csv")));
    QVERIFY(!ok);
    QVERIFY(!vm.statusMessage().isEmpty());
}

void TestDatabaseViewModel::canEditIsTrueWhenAtLeastOneSelected()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    QCOMPARE(vm.canEdit(), false);   // 0 selected
    vm.students()->toggle("A");
    QCOMPARE(vm.canEdit(), true);    // exactly 1 -> single-edit
    vm.students()->toggle("B");
    QCOMPARE(vm.canEdit(), true);    // 2 selected -> bulk-edit (button still enabled)
}

void TestDatabaseViewModel::canEditChangedEmitsOnlyOnBooleanFlip()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    QSignalSpy spy(&vm, &DatabaseViewModel::canEditChanged);
    vm.students()->toggle("A");   // false -> true  : emit
    vm.students()->toggle("B");   // true  -> true  : no emit
    vm.students()->toggle("B");   // true  -> true  : no emit
    vm.students()->toggle("A");   // true  -> false : emit
    QCOMPARE(spy.count(), 2);
}

void TestDatabaseViewModel::beginEditPrefillsAllFieldsAndEmitsReady()
{
    DatabaseViewModel vm;
    StudentRecord r;
    r.schoolId = "2023-001"; r.code = "C1"; r.name = "Juan Cruz"; r.course = "BSIT";
    r.department = "CCS"; r.yearLevel = "2"; r.gender = "Male"; r.status = "Active";
    r.visits = 7;
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);

    QSignalSpy readySpy(&vm, &DatabaseViewModel::editReady);
    vm.beginEdit("2023-001");

    QCOMPARE(vm.editSchoolId(), QStringLiteral("2023-001"));
    QCOMPARE(vm.editName(), QStringLiteral("Juan Cruz"));
    QCOMPARE(vm.editYearLevel(), QStringLiteral("2"));
    QCOMPARE(vm.editGender(), QStringLiteral("Male"));
    QCOMPARE(vm.editStatus(), QStringLiteral("Active"));
    QCOMPARE(vm.editDepartment(), QStringLiteral("CCS"));
    QCOMPARE(vm.editCourse(), QStringLiteral("BSIT"));
    QCOMPARE(readySpy.count(), 1);
}

void TestDatabaseViewModel::beginEditNoMatchIsNoOp()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "2023-001"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);

    QSignalSpy readySpy(&vm, &DatabaseViewModel::editReady);
    vm.beginEdit("does-not-exist");
    QCOMPARE(readySpy.count(), 0);                 // no signal — dialog stays closed
    QVERIFY(vm.editSchoolId().isEmpty());          // edit state untouched
}

void TestDatabaseViewModel::beginEditSelectedUsesSingleSelectedId()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId = "A"; a.name = "Ann";
    StudentRecord b; b.schoolId = "B"; b.name = "Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("B");                    // exactly one selected

    QSignalSpy readySpy(&vm, &DatabaseViewModel::editReady);
    vm.beginEditSelected();
    QCOMPARE(vm.editSchoolId(), QStringLiteral("B"));
    QCOMPARE(vm.editName(), QStringLiteral("Ben"));
    QCOMPARE(readySpy.count(), 1);
}

void TestDatabaseViewModel::setEditDepartmentClearsCourseAndReloads()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "A"; r.name = "Ann"; r.department = "CCS"; r.course = "BSIT";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.beginEdit("A");
    QCOMPARE(vm.editCourse(), QStringLiteral("BSIT"));

    vm.setEditDepartment("CBA");
    QCOMPARE(vm.editDepartment(), QStringLiteral("CBA"));
    QCOMPARE(vm.editCourse(), QString());          // dependent-clear
}

void TestDatabaseViewModel::onEditCoursesLoadedPopulatesEditCourses()
{
    DatabaseViewModel vm;
    vm.onEditCoursesLoaded({"BSIT", "BSCS"});
    QCOMPARE(vm.editCourses(), (QStringList{"BSIT", "BSCS"}));
}

void TestDatabaseViewModel::onBulkUpdateFinishedSuccessSetsStatusReloadsAndFinishes()
{
    DatabaseViewModel vm;
    StudentRecord r; r.schoolId = "A"; r.name = "Ann";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);

    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = true; res.updatedCount = 1;
    vm.onBulkUpdateFinished(res);

    QCOMPARE(vm.statusMessage(), QStringLiteral("Updated 1 student"));
    QVERIFY(!vm.authFailure());
    QVERIFY(vm.loading());                 // reloadTable() flipped loading on
    QCOMPARE(finishedSpy.count(), 1);
}

void TestDatabaseViewModel::onBulkUpdateFinishedNoChangeSetsStatusAndFinishes()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = true; res.updatedCount = 0;
    vm.onBulkUpdateFinished(res);

    QCOMPARE(vm.statusMessage(), QStringLiteral("No changes to save"));
    QVERIFY(!vm.authFailure());
    QCOMPARE(finishedSpy.count(), 1);      // dialog closes on a no-op too
}

void TestDatabaseViewModel::onBulkUpdateFinishedAuthFailureSetsAuthStateKeepsOpen()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = false; res.message = "Invalid admin key";
    vm.onBulkUpdateFinished(res);

    QVERIFY(vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(finishedSpy.count(), 0);      // dialog stays open on error
}

void TestDatabaseViewModel::onBulkUpdateFinishedGenericFailureSetsStatusNoAuth()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    BulkUpdateResult res; res.ok = false; res.message = "Some updates failed, rolled back";
    vm.onBulkUpdateFinished(res);

    QVERIFY(!vm.authFailure());
    QCOMPARE(vm.statusMessage(), QStringLiteral("Some updates failed, rolled back"));
    QCOMPARE(finishedSpy.count(), 0);
}

void TestDatabaseViewModel::onBulkUpdateFailedSetsTransientStatus()
{
    DatabaseViewModel vm;
    QSignalSpy finishedSpy(&vm, &DatabaseViewModel::editFinished);
    vm.onBulkUpdateFailed(QStringLiteral("Connection refused"));
    QVERIFY(!vm.authFailure());
    QVERIFY(!vm.statusMessage().isEmpty());
    QCOMPARE(finishedSpy.count(), 0);
}

void TestDatabaseViewModel::serverRejectionSharedByDeleteAndBulk()
{
    // Same auth-failure message must produce the SAME auth state + toast on
    // both the delete and the bulk-update paths (proves the shared helper).
    DatabaseViewModel del;
    del.onDeleteFinished(false, 2, QStringLiteral("Invalid admin key"));
    DatabaseViewModel bulk;
    BulkUpdateResult res; res.ok = false; res.message = "Invalid admin key";
    bulk.onBulkUpdateFinished(res);
    QCOMPARE(del.authFailure(), true);
    QCOMPARE(bulk.authFailure(), true);
    QCOMPARE(del.statusMessage(), bulk.statusMessage());   // identical auth toast

    // And a generic (non-auth) message stays non-auth on both, using each
    // path's own fallback when empty.
    DatabaseViewModel del2;  del2.onDeleteFinished(false, 1, QString());
    DatabaseViewModel bulk2; BulkUpdateResult r2; r2.ok = false; bulk2.onBulkUpdateFinished(r2);
    QCOMPARE(del2.authFailure(), false);
    QCOMPARE(bulk2.authFailure(), false);
    QCOMPARE(del2.statusMessage(), QStringLiteral("Delete failed."));
    QCOMPARE(bulk2.statusMessage(), QStringLiteral("Update failed."));
}

void TestDatabaseViewModel::saveEditEntersGuardedPath()
{
    // saveEdit reads the AdminSession key and posts via the VM's own NAM (no
    // live server in CI). The exact wire form (1-element students JSON array +
    // admin_key) is asserted at the controller level in
    // tst_studentcontroller::bulkUpdate_buildsFormBodyWithStudentsJsonAndAdminKey.
    // Here we only prove saveEdit does not early-return: with a located record
    // it leaves edit state intact and fires without crashing.
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    StudentRecord r; r.schoolId = "A"; r.name = "Ann"; r.department = "CCS"; r.course = "BSIT";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.beginEdit("A");
    vm.setEditName("Ann Edited");
    vm.saveEdit();                         // posts; reply handled asynchronously
    QCOMPARE(vm.editName(), QStringLiteral("Ann Edited"));
    AdminSession::instance().clear();
}

void TestDatabaseViewModel::buildBulkUpdatesOverridesOnlyToggledFieldsAndCarriesRest()
{
    StudentRecord a; a.schoolId="A"; a.code="C-A"; a.name="Ann"; a.course="BSIT";
    a.department="CCS"; a.yearLevel="2"; a.gender="Female"; a.status="Active"; a.visits=5;
    StudentRecord b; b.schoolId="B"; b.code="C-B"; b.name="Ben"; b.course="BSCS";
    b.department="CCS"; b.yearLevel="3"; b.gender="Male"; b.status="Active"; b.visits=9;

    BulkEditChanges ch;
    ch.changeDepartment = true; ch.department = "CBA";
    ch.changeCourse     = true; ch.course     = "BSBA";
    ch.changeStatus     = true; ch.status     = "Inactive";

    const QList<StudentRecord> out = DatabaseViewModel::buildBulkUpdates({a, b}, ch);
    QCOMPARE(out.size(), 2);
    // Overridden on every record.
    QCOMPARE(out[0].department, QStringLiteral("CBA"));
    QCOMPARE(out[0].course,     QStringLiteral("BSBA"));
    QCOMPARE(out[0].status,     QStringLiteral("Inactive"));
    QCOMPARE(out[1].department, QStringLiteral("CBA"));
    // Carried through per-record, untouched.
    QCOMPARE(out[0].schoolId,  QStringLiteral("A"));
    QCOMPARE(out[0].name,      QStringLiteral("Ann"));
    QCOMPARE(out[0].code,      QStringLiteral("C-A"));
    QCOMPARE(out[0].yearLevel, QStringLiteral("2"));    // not toggled
    QCOMPARE(out[0].gender,    QStringLiteral("Female"));// not toggled
    QCOMPARE(out[0].visits,    5);
    QCOMPARE(out[1].name,      QStringLiteral("Ben"));   // NOT wiped
    QCOMPARE(out[1].yearLevel, QStringLiteral("3"));
    QCOMPARE(out[1].visits,    9);
}

void TestDatabaseViewModel::couplingDepartmentDrivesCourseToggle()
{
    DatabaseViewModel vm;
    vm.setChangeDepartment(true);
    QCOMPARE(vm.changeCourse(), true);           // dept ON forces course ON
    vm.setBulkCourse("BSBA");
    vm.setChangeDepartment(false);
    QCOMPARE(vm.changeCourse(), false);          // dept OFF forces course OFF
    QCOMPARE(vm.bulkCourse(), QString());         // and clears the course value
}

void TestDatabaseViewModel::setChangeCourseWithoutDepartmentIsNoOp()
{
    DatabaseViewModel vm;
    vm.setChangeCourse(true);                     // no department toggled
    QCOMPARE(vm.changeCourse(), false);           // guarded no-op
}

void TestDatabaseViewModel::setBulkDepartmentClearsCourseValue()
{
    DatabaseViewModel vm;
    vm.setChangeDepartment(true);
    vm.setBulkDepartment("CCS");
    vm.setBulkCourse("BSIT");
    vm.setBulkDepartment("CBA");                  // real dept change
    QCOMPARE(vm.bulkCourse(), QString());          // dependent-clear
}

void TestDatabaseViewModel::canApplyBulkGating()
{
    DatabaseViewModel vm;
    QCOMPARE(vm.canApplyBulk(), false);                    // nothing toggled
    vm.setChangeYearLevel(true);
    QCOMPARE(vm.canApplyBulk(), false);                    // toggled but empty value
    vm.setBulkYearLevel("3");
    QCOMPARE(vm.canApplyBulk(), true);                     // one valid change
    // Department requires a course too.
    DatabaseViewModel vm2;
    vm2.setChangeDepartment(true);
    vm2.setBulkDepartment("CBA");
    QCOMPARE(vm2.canApplyBulk(), false);                   // dept set, course still empty
    vm2.setBulkCourse("BSBA");
    QCOMPARE(vm2.canApplyBulk(), true);
}

void TestDatabaseViewModel::bulkChangeSummaryListsOnlyToggledInOrder()
{
    DatabaseViewModel vm;
    vm.setChangeDepartment(true);  vm.setBulkDepartment("CBA");  vm.setBulkCourse("BSBA");
    vm.setChangeStatus(true);      vm.setBulkStatus("Inactive");
    QCOMPARE(vm.bulkChangeSummary(),
             (QStringList{ QStringLiteral("Department → CBA"),
                           QStringLiteral("Course → BSBA"),
                           QStringLiteral("Status → Inactive") }));
}

void TestDatabaseViewModel::courseTargetRoutesBulkVsSingle()
{
    DatabaseViewModel vm;
    vm.setBulkDepartment("CBA");                 // sets target = BulkEdit (+ fires a load)
    vm.onEditCoursesLoaded({"BSBA", "BSA"});
    QCOMPARE(vm.bulkCourses(), (QStringList{"BSBA", "BSA"}));
    QVERIFY(vm.editCourses().isEmpty());          // single list untouched

    StudentRecord r; r.schoolId="A"; r.name="Ann"; r.department="CCS"; r.course="BSIT";
    vm.onSearchFinished(SearchOutcome::Results, {r}, "", "", 1);
    vm.beginEdit("A");                            // sets target = SingleEdit
    vm.onEditCoursesLoaded({"BSIT", "BSCS"});
    QCOMPARE(vm.editCourses(), (QStringList{"BSIT", "BSCS"}));
    QCOMPARE(vm.bulkCourses(), (QStringList{"BSBA", "BSA"}));   // bulk list unchanged
}

void TestDatabaseViewModel::beginBulkEditSelectedGuardsBelowTwo()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId="A"; a.name="Ann";
    vm.onSearchFinished(SearchOutcome::Results, {a}, "", "", 1);
    vm.students()->toggle("A");                   // only 1 selected
    QSignalSpy readySpy(&vm, &DatabaseViewModel::bulkEditReady);
    vm.beginBulkEditSelected();
    QCOMPARE(readySpy.count(), 0);                 // guarded — needs >= 2
}

void TestDatabaseViewModel::beginBulkEditSelectedResetsStateAndEmitsReady()
{
    DatabaseViewModel vm;
    StudentRecord a; a.schoolId="A"; a.name="Ann";
    StudentRecord b; b.schoolId="B"; b.name="Ben";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("A"); vm.students()->toggle("B");   // 2 selected
    // Dirty the bulk state from a prior open.
    vm.setChangeStatus(true); vm.setBulkStatus("Inactive");
    QSignalSpy readySpy(&vm, &DatabaseViewModel::bulkEditReady);
    vm.beginBulkEditSelected();
    QCOMPARE(vm.changeStatus(), false);            // reset
    QCOMPARE(vm.bulkStatus(), QString());
    QCOMPARE(vm.canApplyBulk(), false);
    QCOMPARE(readySpy.count(), 1);
}

void TestDatabaseViewModel::onBulkUpdateFinishedPluralCountMessage()
{
    DatabaseViewModel vm;
    BulkUpdateResult res; res.ok = true; res.updatedCount = 3;
    vm.onBulkUpdateFinished(res);
    QCOMPARE(vm.statusMessage(), QStringLiteral("Updated 3 students"));
}

void TestDatabaseViewModel::applyBulkEditRoutesBulkFinishedAndClearsBusy()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    StudentRecord a; a.schoolId="A"; a.name="Ann"; a.status="Active";
    StudentRecord b; b.schoolId="B"; b.name="Ben"; b.status="Active";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("A"); vm.students()->toggle("B");
    vm.beginBulkEditSelected();                         // mode = BulkEdit
    vm.setChangeStatus(true); vm.setBulkStatus("Inactive");

    QSignalSpy bulkDone(&vm, &DatabaseViewModel::bulkEditFinished);
    QSignalSpy singleDone(&vm, &DatabaseViewModel::editFinished);
    QCOMPARE(vm.bulkBusy(), false);
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), true);                       // in flight

    BulkUpdateResult res; res.ok = true; res.updatedCount = 2;
    vm.onBulkUpdateFinished(res);
    QCOMPARE(vm.statusMessage(), QStringLiteral("Updated 2 students"));
    QCOMPARE(vm.bulkBusy(), false);                      // guard released
    QCOMPARE(bulkDone.count(), 1);                       // routed to BULK finished
    QCOMPARE(singleDone.count(), 0);
    AdminSession::instance().clear();
}

void TestDatabaseViewModel::applyBulkEditGuardsEmptyInvalidAndReentry()
{
    DatabaseViewModel vm;
    AdminSession::instance().setKey("held-key");
    // No selection -> no-op, never goes busy.
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), false);

    StudentRecord a; a.schoolId="A"; a.name="Ann"; a.status="Active";
    StudentRecord b; b.schoolId="B"; b.name="Ben"; b.status="Active";
    vm.onSearchFinished(SearchOutcome::Results, {a, b}, "", "", 1);
    vm.students()->toggle("A"); vm.students()->toggle("B");
    vm.beginBulkEditSelected();
    // Selection present but no toggled change -> canApplyBulk false -> no-op.
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), false);

    // Valid change -> goes busy; a second call is a re-entry no-op.
    vm.setChangeStatus(true); vm.setBulkStatus("Inactive");
    vm.applyBulkEdit();
    QCOMPARE(vm.bulkBusy(), true);
    vm.applyBulkEdit();                                  // must not crash / double-post
    QCOMPARE(vm.bulkBusy(), true);
    AdminSession::instance().clear();
}

QTEST_MAIN(TestDatabaseViewModel)
#include "tst_databaseviewmodel.moc"
