#ifndef TIMEANALYTICS_H
#define TIMEANALYTICS_H

#include <QList>

// Clamp a library-hours window to a safe, non-empty [lo,hi] within 0..23.
// SINGLE SOURCE of the decision-4 fallback (spec §5.1): used by
// TimeAnalytics::compute AND the ViewModel's hourly-bar / export builders, so the
// window arithmetic is byte-identical everywhere and the reported peak can never
// name a bar that is not drawn. If the stored hours are nonsensical
// (openHour<0 || closeHour>23 || openHour>closeHour) fall back to the full 0..23
// range so the chart never blanks from a hand-edited settings typo.
inline void clampLibraryHours(int &lo, int &hi)
{
    if (lo < 0 || hi > 23 || lo > hi) {
        lo = 0;
        hi = 23;
    }
}

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
    // format). openHour/closeHour bound the hourly peak scan to [openHour,closeHour]
    // INCLUSIVE (decision 1/3); nonsensical windows clamp to 0..23 (decision 4).
    // hourly is still returned RAW/24-wide (the window is applied by consumers, not
    // by mutating the array). Tie-break for both peaks: earliest/first bucket wins.
    // Defensive: if either array is not the expected length, returns hasData=false
    // with no OOB access (a second net beneath ReportController's parse boundary).
    static TimeAnalytics compute(const QList<int> &byHour,
                                 const QList<int> &byWeekdaySunFirst,
                                 int openHour, int closeHour);
};

#endif // TIMEANALYTICS_H
