#include <QtTest>
#include <QAbstractItemModelTester>
#include "BarsModel.h"

class TestBarsModel : public QObject
{
    Q_OBJECT
private slots:
    void emptyByDefault();
    void setBarsPopulatesRowsAndRoles();
    void maxValueTracksLargestBar();
    void resetClearsWhenEmpty();
};

void TestBarsModel::emptyByDefault()
{
    BarsModel m;
    QAbstractItemModelTester tester(&m);   // sanity-checks model contract
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.maxValue(), 0.0);
}

void TestBarsModel::setBarsPopulatesRowsAndRoles()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("8"), 12.0}, {QStringLiteral("9"), 34.0} });
    QCOMPARE(m.rowCount(), 2);
    const QModelIndex i0 = m.index(0, 0);
    QCOMPARE(m.data(i0, BarsModel::LabelRole).toString(), QStringLiteral("8"));
    QCOMPARE(m.data(i0, BarsModel::ValueRole).toDouble(), 12.0);
}

void TestBarsModel::maxValueTracksLargestBar()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("a"), 5.0}, {QStringLiteral("b"), 41.0}, {QStringLiteral("c"), 9.0} });
    QCOMPARE(m.maxValue(), 41.0);
}

void TestBarsModel::resetClearsWhenEmpty()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("a"), 5.0} });
    m.setBars({});
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.maxValue(), 0.0);
}

QTEST_MAIN(TestBarsModel)
#include "tst_barsmodel.moc"
