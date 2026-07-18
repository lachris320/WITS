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
    void sameLabelsUpdateGranularlyNotReset();
    void differentLabelsTriggerFullReset();
    void granularPathRecomputesMaxValueDownAndUp();
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

// Same labels, same order, same count → the model MUST update values in
// place (dataChanged) rather than reset. A reset destroys/recreates every
// QML delegate, which permanently defeats the `Behavior on width` (dept
// bars) and `Behavior on Layout.preferredHeight` (hourly bars) glide — a
// Behavior never animates a delegate's construction-time initial binding.
// Mutation check: reverting setBars() to always beginResetModel/
// endResetModel makes the `resetSpy.count(), 0` assertion fail (it fires
// exactly once instead).
void TestBarsModel::sameLabelsUpdateGranularlyNotReset()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("CE"), 5.0}, {QStringLiteral("IT"), 9.0} });

    QSignalSpy resetSpy(&m, &QAbstractItemModel::modelReset);
    QSignalSpy dataChangedSpy(&m, &QAbstractItemModel::dataChanged);
    QSignalSpy maxValueSpy(&m, &BarsModel::maxValueChanged);

    m.setBars({ {QStringLiteral("CE"), 8.0}, {QStringLiteral("IT"), 3.0} });

    QCOMPARE(resetSpy.count(), 0);
    QVERIFY(dataChangedSpy.count() >= 1);
    QCOMPARE(m.data(m.index(0, 0), BarsModel::ValueRole).toDouble(), 8.0);
    QCOMPARE(m.data(m.index(1, 0), BarsModel::ValueRole).toDouble(), 3.0);
    QCOMPARE(m.maxValue(), 8.0);
    QCOMPARE(maxValueSpy.count(), 1);
    // Rows survive untouched — same count, same labels.
    QCOMPARE(m.rowCount(), 2);
    QCOMPARE(m.data(m.index(0, 0), BarsModel::LabelRole).toString(), QStringLiteral("CE"));
}

// Label set actually changes (different count, or a different label at some
// index) → must fall back to the full reset. Guards the slow path so rows
// that genuinely appear/disappear (first load, a department-set change)
// still go through beginResetModel/endResetModel.
// Mutation check: forcing the fast (granular) path unconditionally makes
// `resetSpy.count(), 1` fail (it would stay 0).
void TestBarsModel::differentLabelsTriggerFullReset()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("CE"), 5.0}, {QStringLiteral("IT"), 9.0} });

    QSignalSpy resetSpy(&m, &QAbstractItemModel::modelReset);
    m.setBars({ {QStringLiteral("CE"), 5.0}, {QStringLiteral("ME"), 9.0} });
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(m.rowCount(), 2);

    // Different COUNT with identical leading labels is also a slow-path case.
    QSignalSpy resetSpy2(&m, &QAbstractItemModel::modelReset);
    m.setBars({ {QStringLiteral("CE"), 5.0} });
    QCOMPARE(resetSpy2.count(), 1);
    QCOMPARE(m.rowCount(), 1);
}

// The fast path must still recompute maxValue from the NEW values, both
// when the max moves down and when it moves up — proves the granular
// update doesn't just patch dataChanged and skip the maxValue recompute.
// Mutation check: hardcoding/skipping the max recompute on the fast path
// (e.g. leaving m_maxValue at its old value) makes either QCOMPARE fail.
void TestBarsModel::granularPathRecomputesMaxValueDownAndUp()
{
    BarsModel m;
    m.setBars({ {QStringLiteral("CE"), 10.0}, {QStringLiteral("IT"), 2.0} });
    QCOMPARE(m.maxValue(), 10.0);

    // Move the max DOWN: new largest value is smaller than the old max.
    m.setBars({ {QStringLiteral("CE"), 4.0}, {QStringLiteral("IT"), 2.0} });
    QCOMPARE(m.maxValue(), 4.0);

    // Move the max UP: a value now exceeds the previous max.
    m.setBars({ {QStringLiteral("CE"), 4.0}, {QStringLiteral("IT"), 20.0} });
    QCOMPARE(m.maxValue(), 20.0);
}

QTEST_MAIN(TestBarsModel)
#include "tst_barsmodel.moc"
