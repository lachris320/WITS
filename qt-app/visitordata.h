#ifndef VISITORDATA_H
#define VISITORDATA_H
#include <QString>

enum class DateFilterType { All, SpecificDay, Month, DateRange };

struct VisitorRecord
{
    QString name;
    QString company;
    QString purpose;
    QString date;   // "yyyy-MM-dd" or "N/A"
    QString time;   // "hh:mm AP"  or "N/A"
};

struct VisitorFilter
{
    QString        search;
    DateFilterType dateType = DateFilterType::All;
    QString        startDate;   // "yyyy-MM-dd", empty for All
    QString        endDate;     // "yyyy-MM-dd", empty for All
};

#endif // VISITORDATA_H
