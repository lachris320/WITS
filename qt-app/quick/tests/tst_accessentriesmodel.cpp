#include <QtTest>
#include <QSignalSpy>
#include "AccessEntriesModel.h"
#include "loginparser.h"

class TestAccessEntriesModel : public QObject
{
    Q_OBJECT
private slots:
    void roleNamesShape();
    void knownRowRoles();
    void unknownCardRow();
    void setEntriesResetsAndClearEmpties();
    void outOfRangeIndexIsInvalid();

private:
    static QVector<LoginParser::RecentEntry> twoRows()
    {
        LoginParser::RecentEntry known;
        known.id = 12; known.card = QStringLiteral("CARD0012");
        known.createdAt = QStringLiteral("2026-09-30 08:15:00"); known.reader = 1;
        known.known = true; known.name = QStringLiteral("Test Student A");
        known.schoolId = QStringLiteral("TEST-0001"); known.course = QStringLiteral("BS Test");
        known.department = QStringLiteral("Dept Test");
        LoginParser::RecentEntry unknown;
        unknown.id = 11; unknown.card = QStringLiteral("CARD0011");
        unknown.createdAt = QStringLiteral("2026-09-30 08:10:00"); unknown.reader = 2;
        unknown.known = false;
        return {known, unknown};
    }
};

void TestAccessEntriesModel::roleNamesShape()
{
    AccessEntriesModel m;
    const QHash<int, QByteArray> names = m.roleNames();
    QCOMPARE(names.size(), 8);
    QCOMPARE(names.value(AccessEntriesModel::NameRole), QByteArray("name"));
    QCOMPARE(names.value(AccessEntriesModel::SchoolIdRole), QByteArray("schoolId"));
    QCOMPARE(names.value(AccessEntriesModel::CourseRole), QByteArray("course"));
    QCOMPARE(names.value(AccessEntriesModel::DepartmentRole), QByteArray("department"));
    QCOMPARE(names.value(AccessEntriesModel::CreatedAtRole), QByteArray("createdAt"));
    QCOMPARE(names.value(AccessEntriesModel::ReaderRole), QByteArray("reader"));
    QCOMPARE(names.value(AccessEntriesModel::CardRole), QByteArray("card"));
    QCOMPARE(names.value(AccessEntriesModel::KnownRole), QByteArray("known"));
}

void TestAccessEntriesModel::knownRowRoles()
{
    AccessEntriesModel m;
    m.setEntries(twoRows());
    QCOMPARE(m.rowCount(), 2);
    const QModelIndex i = m.index(0);
    QCOMPARE(m.data(i, AccessEntriesModel::NameRole).toString(), QStringLiteral("Test Student A"));
    QCOMPARE(m.data(i, AccessEntriesModel::SchoolIdRole).toString(), QStringLiteral("TEST-0001"));
    QCOMPARE(m.data(i, AccessEntriesModel::CourseRole).toString(), QStringLiteral("BS Test"));
    QCOMPARE(m.data(i, AccessEntriesModel::DepartmentRole).toString(), QStringLiteral("Dept Test"));
    QCOMPARE(m.data(i, AccessEntriesModel::CreatedAtRole).toString(),
             QStringLiteral("2026-09-30 08:15:00"));
    QCOMPARE(m.data(i, AccessEntriesModel::ReaderRole).toString(), QStringLiteral("1"));
    QCOMPARE(m.data(i, AccessEntriesModel::CardRole).toString(), QStringLiteral("CARD0012"));
    QCOMPARE(m.data(i, AccessEntriesModel::KnownRole).toBool(), true);
}

void TestAccessEntriesModel::unknownCardRow()
{
    AccessEntriesModel m;
    m.setEntries(twoRows());
    const QModelIndex i = m.index(1);
    QCOMPARE(m.data(i, AccessEntriesModel::NameRole).toString(), QStringLiteral("Unknown card"));
    QCOMPARE(m.data(i, AccessEntriesModel::KnownRole).toBool(), false);
    QVERIFY(m.data(i, AccessEntriesModel::SchoolIdRole).toString().isEmpty());
    QCOMPARE(m.data(i, AccessEntriesModel::CardRole).toString(), QStringLiteral("CARD0011"));
    QCOMPARE(m.data(i, AccessEntriesModel::ReaderRole).toString(), QStringLiteral("2"));
}

void TestAccessEntriesModel::setEntriesResetsAndClearEmpties()
{
    AccessEntriesModel m;
    QSignalSpy reset(&m, &QAbstractItemModel::modelReset);
    m.setEntries(twoRows());
    QCOMPARE(reset.count(), 1);
    QCOMPARE(m.rowCount(), 2);
    m.clear();
    QCOMPARE(reset.count(), 2);
    QCOMPARE(m.rowCount(), 0);
}

void TestAccessEntriesModel::outOfRangeIndexIsInvalid()
{
    AccessEntriesModel m;
    m.setEntries(twoRows());
    QVERIFY(!m.data(m.index(5), AccessEntriesModel::NameRole).isValid());
    QVERIFY(!m.data(QModelIndex(), AccessEntriesModel::NameRole).isValid());
    QCOMPARE(m.rowCount(m.index(0)), 0);   // list model: children have no rows
}

QTEST_APPLESS_MAIN(TestAccessEntriesModel)
#include "tst_accessentriesmodel.moc"
