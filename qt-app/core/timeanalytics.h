#ifndef TIMEANALYTICS_H
#define TIMEANALYTICS_H

#include <QList>

// "When?" time-of-visit analytics (spec 4b-iv-a §4.2). PURE and
// presentation-agnostic: deals only in indices + counts. All human-readable
// formatting (12-hour range, weekday name) is the ViewModel's job, never here —
// same rule as ReportAnalytics::compute in 4b-iii.
struct TimeAnalytics {
    QList<int> hourly;                    // 24 entries, index = hour 0..23 (as received)
    QList<int> weekdayMonFirst;          // 7 entries, index 0=Mon .. 6=Sun (reordered)
    int        peakHour            = 0;  // 0..23, hour bucket with the highest count
    int        peakHourCount       = 0;
    int        peakWeekdayMonFirst = 0;  // 0=Mon .. 6=Sun
    int        peakWeekdayCount    = 0;
    bool       hasData             = false;  // true iff any input count > 0

    // byHour: dense 24-element array, index = hour 0..23.
    // byWeekdaySunFirst: dense 7-element array, [0]=Sunday .. [6]=Saturday (the wire
    // format). Tie-break for both peaks: earliest/first bucket wins. Defensive: if
    // either array is not the expected length, returns hasData=false with no OOB access
    // (a second net beneath ReportController's parse-boundary validation, spec §4.2).
    static TimeAnalytics compute(const QList<int> &byHour,
                                 const QList<int> &byWeekdaySunFirst);
};

#endif // TIMEANALYTICS_H
