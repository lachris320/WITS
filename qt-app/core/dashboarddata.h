#ifndef DASHBOARDDATA_H
#define DASHBOARDDATA_H

#include <QList>
#include <QString>

// One hourly histogram bucket (spec §5.1). hour is 0..23 (server-local).
struct HourBucket { int hour = 0; int count = 0; };

// One department breakdown bucket (spec §5.1).
struct DeptBucket { QString name; int count = 0; };

// Decoded dashboard_summary.php response. Peak hour is NOT here — it is
// derived client-side in DashboardViewModel (spec §5.1/§6.1).
struct DashboardSummary
{
    int today = 0;
    int week = 0;
    int students = 0;
    QList<HourBucket> hourly;
    QList<DeptBucket> departments;
};

#endif // DASHBOARDDATA_H
