#include "timeanalytics.h"

TimeAnalytics TimeAnalytics::compute(const QList<int> &byHour,
                                     const QList<int> &byWeekdaySunFirst,
                                     int openHour, int closeHour)
{
    TimeAnalytics a;

    // Defensive length gate (spec §4.2). A malformed array bails with hasData=false
    // rather than indexing out of bounds — a second net under ReportController's
    // parse-boundary validation.
    if (byHour.size() != 24 || byWeekdaySunFirst.size() != 7)
        return a;

    a.hourly = byHour;

    // Reorder Sunday-first (wire) -> Monday-first (presentation):
    //   monFirst[i] = sunFirst[(i + 1) % 7]
    //   i=0 -> sun[1]=Mon ... i=5 -> sun[6]=Sat, i=6 -> sun[0]=Sun.
    a.weekdayMonFirst.reserve(7);
    for (int i = 0; i < 7; ++i)
        a.weekdayMonFirst.append(byWeekdaySunFirst.at((i + 1) % 7));

    // Peak hour — highest count WITHIN the library-hours window [openHour,closeHour]
    // inclusive (decision 1/3); earliest bucket wins ties (strictly-greater keeps
    // the first max seen). A nonsensical window clamps to 0..23 (decision 4). When
    // the window holds no positive bucket, peakHour/peakHourCount stay 0 — the
    // sentinel decision 5 keys on. hourly stays RAW/24-wide (set above).
    int lo = openHour, hi = closeHour;
    clampLibraryHours(lo, hi);
    for (int h = lo; h <= hi; ++h) {
        if (byHour.at(h) > a.peakHourCount) {
            a.peakHourCount = byHour.at(h);
            a.peakHour = h;
        }
    }

    // Peak weekday — over the Monday-first array; earliest bucket wins ties.
    for (int d = 0; d < 7; ++d) {
        if (a.weekdayMonFirst.at(d) > a.peakWeekdayCount) {
            a.peakWeekdayCount = a.weekdayMonFirst.at(d);
            a.peakWeekdayMonFirst = d;
        }
    }

    // hasData = any count across either array > 0 (both agree in practice).
    for (int h = 0; h < 24 && !a.hasData; ++h)
        if (byHour.at(h) > 0) a.hasData = true;
    for (int d = 0; d < 7 && !a.hasData; ++d)
        if (byWeekdaySunFirst.at(d) > 0) a.hasData = true;

    return a;
}
