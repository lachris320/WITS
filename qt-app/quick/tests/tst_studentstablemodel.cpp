#include <QtTest>
#include "StudentsTableModel.h"
#include "studentdata.h"

static StudentRecord rec(const QString &id, const QString &name) {
    StudentRecord r; r.schoolId = id; r.name = name; r.course = "BSIT";
    r.department = "CCS"; r.yearLevel = "2"; r.status = "Active"; r.visits = 3;
    return r;
}

class TestStudentsTableModel : public QObject
{
    Q_OBJECT
private slots:
    void emptyByDefault();
    void setRecordsPopulatesCount();
    void toggleMarksSelected();
    void selectionSurvivesRefreshBySchoolId();
    void setAllAndClear();
    void selectedIdsReturnsOnlySelected();
    void selectedRecordsReturnsOnlySelectedInOrder();
    void selectedRecordsEmptyWhenNoneSelected();
    void allRecordsReturnsEveryLoadedRow();
};

void TestStudentsTableModel::emptyByDefault()
{
    StudentsTableModel m;
    QCOMPARE(m.count(), 0);
    QCOMPARE(m.selectedCount(), 0);
    QVERIFY(!m.anySelected());
    QVERIFY(!m.allSelected());
}

void TestStudentsTableModel::setRecordsPopulatesCount()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    QCOMPARE(m.count(), 2);
    QCOMPARE(m.rowCount(), 2);
}

void TestStudentsTableModel::toggleMarksSelected()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.toggle("A");
    QCOMPARE(m.selectedCount(), 1);
    QVERIFY(m.anySelected());
    QVERIFY(!m.allSelected());
    QCOMPARE(m.selectedIds(), QStringList{"A"});
    m.toggle("A");                       // toggle off
    QCOMPARE(m.selectedCount(), 0);
}

void TestStudentsTableModel::selectionSurvivesRefreshBySchoolId()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.toggle("B");
    // A refresh returns B (renamed) + a new row C; A is gone.
    m.setRecords({rec("B","Ben Updated"), rec("C","Cara")});
    QCOMPARE(m.selectedCount(), 1);      // B still selected
    QCOMPARE(m.selectedIds(), QStringList{"B"});
}

void TestStudentsTableModel::setAllAndClear()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.setAllSelected(true);
    QVERIFY(m.allSelected());
    QCOMPARE(m.selectedCount(), 2);
    m.clearSelection();
    QCOMPARE(m.selectedCount(), 0);
}

void TestStudentsTableModel::selectedIdsReturnsOnlySelected()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben"), rec("C","Cara")});
    m.toggle("A"); m.toggle("C");
    QStringList ids = m.selectedIds(); ids.sort();
    QCOMPARE(ids, (QStringList{"A","C"}));
}

void TestStudentsTableModel::selectedRecordsReturnsOnlySelectedInOrder()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben"), rec("C","Cara")});
    m.toggle("C"); m.toggle("A");
    const QList<StudentRecord> sel = m.selectedRecords();
    QCOMPARE(sel.size(), 2);
    // m_records order (A before C), not selection/insertion order.
    QCOMPARE(sel.at(0).schoolId, QStringLiteral("A"));
    QCOMPARE(sel.at(1).schoolId, QStringLiteral("C"));
}

void TestStudentsTableModel::selectedRecordsEmptyWhenNoneSelected()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    QVERIFY(m.selectedRecords().isEmpty());
}

void TestStudentsTableModel::allRecordsReturnsEveryLoadedRow()
{
    StudentsTableModel m;
    m.setRecords({rec("A","Ann"), rec("B","Ben")});
    m.toggle("A");                       // selection must not affect allRecords()
    const QList<StudentRecord> all = m.allRecords();
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.at(0).schoolId, QStringLiteral("A"));
    QCOMPARE(all.at(1).schoolId, QStringLiteral("B"));
}

QTEST_APPLESS_MAIN(TestStudentsTableModel)
#include "tst_studentstablemodel.moc"
