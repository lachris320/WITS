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

QTEST_MAIN(TestVisitLogsViewModel)
#include "tst_visitlogsviewmodel.moc"
