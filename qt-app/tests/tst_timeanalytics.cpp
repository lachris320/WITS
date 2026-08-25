#include <QtTest>
#include <QList>
#include "timeanalytics.h"

class TstTimeAnalytics : public QObject
{
    Q_OBJECT
private slots:
    void reorder_sundayFirstToMondayFirst();
    void peakHour_picksHighestIndexAndCount();
    void peakWeekday_picksHighestMonFirstIndexAndCount();
    void tieBreak_earliestBucketWins();
    void allZero_hasDataFalse();
    void hasData_trueWhenAnyCountPositive();
    void badLength_hasDataFalseNoCrash();

private:
    static QList<int> zeros(int n) {
        QList<int> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.append(0);
        return v;
    }
    // A valid dense 24-hour array with a single peak at 14 (2 PM).
    static QList<int> hoursPeak14() {
        QList<int> v = zeros(24);
        v[8] = 5; v[14] = 12; v[19] = 9;
        return v;
    }
    // A valid dense 7-weekday array, Sunday-first, no ties.
    static QList<int> weekSunFirst() { return QList<int>{2, 40, 8, 30, 8, 5, 1}; }
};

void TstTimeAnalytics::reorder_sundayFirstToMondayFirst() {
    QList<int> byHour = zeros(24); byHour[9] = 3;          // some data -> hasData true
    QList<int> byWeekday{10, 20, 30, 40, 50, 60, 70};      // Sun..Sat
    const TimeAnalytics a = TimeAnalytics::compute(byHour, byWeekday);
    QCOMPARE(a.weekdayMonFirst, (QList<int>{20, 30, 40, 50, 60, 70, 10})); // Mon..Sun
    QCOMPARE(a.hourly, byHour);                            // passed through unchanged
    QVERIFY(a.hasData);
}

void TstTimeAnalytics::peakHour_picksHighestIndexAndCount() {
    const TimeAnalytics a = TimeAnalytics::compute(hoursPeak14(), weekSunFirst());
    QCOMPARE(a.peakHour, 14);
    QCOMPARE(a.peakHourCount, 12);
}

void TstTimeAnalytics::peakWeekday_picksHighestMonFirstIndexAndCount() {
    // Sun-first {2,40,8,30,8,5,1}; Mon-first = [40,8,30,8,5,1,2]; peak idx0 = Monday.
    const TimeAnalytics a = TimeAnalytics::compute(hoursPeak14(), weekSunFirst());
    QCOMPARE(a.peakWeekdayMonFirst, 0);   // Monday
    QCOMPARE(a.peakWeekdayCount, 40);
}

void TstTimeAnalytics::tieBreak_earliestBucketWins() {
    QList<int> byHour = zeros(24); byHour[6] = 10; byHour[18] = 10;   // tie -> earliest (6)
    QList<int> byWeekday{0, 7, 0, 0, 0, 7, 0};   // Sun-first: Mon & Fri tie at 7
    const TimeAnalytics a = TimeAnalytics::compute(byHour, byWeekday);
    QCOMPARE(a.peakHour, 6);
    QCOMPARE(a.peakHourCount, 10);
    // Mon-first = [7,0,0,0,7,0,0]; earliest max is index 0 (Monday).
    QCOMPARE(a.peakWeekdayMonFirst, 0);
    QCOMPARE(a.peakWeekdayCount, 7);
}

void TstTimeAnalytics::allZero_hasDataFalse() {
    const TimeAnalytics a = TimeAnalytics::compute(zeros(24), zeros(7));
    QVERIFY(!a.hasData);
    QCOMPARE(a.peakHour, 0);
    QCOMPARE(a.peakHourCount, 0);
    QCOMPARE(a.peakWeekdayMonFirst, 0);
    QCOMPARE(a.peakWeekdayCount, 0);
    QCOMPARE(a.weekdayMonFirst.size(), 7);   // still dense/reordered
    QCOMPARE(a.hourly.size(), 24);
}

void TstTimeAnalytics::hasData_trueWhenAnyCountPositive() {
    QList<int> byHour = zeros(24); byHour[0] = 1;
    const TimeAnalytics a = TimeAnalytics::compute(byHour, zeros(7));
    QVERIFY(a.hasData);
}

void TstTimeAnalytics::badLength_hasDataFalseNoCrash() {
    const TimeAnalytics shortHour = TimeAnalytics::compute(zeros(23), zeros(7));
    QVERIFY(!shortHour.hasData);
    QVERIFY(shortHour.hourly.isEmpty());        // never populated on bad input
    const TimeAnalytics shortWeek = TimeAnalytics::compute(zeros(24), zeros(6));
    QVERIFY(!shortWeek.hasData);
    const TimeAnalytics emptyBoth = TimeAnalytics::compute(QList<int>{}, QList<int>{});
    QVERIFY(!emptyBoth.hasData);
}

QTEST_APPLESS_MAIN(TstTimeAnalytics)
#include "tst_timeanalytics.moc"
