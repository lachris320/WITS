#ifndef VISITLOGDATA_H
#define VISITLOGDATA_H

#include <QString>

// One student attendance row from get_library_visits.php (spec §5.2). No
// time-out field: the system is login-only (library_visits has no logout
// column). The screen renders a literal "—" for time-out (spec §6.3).
struct StudentVisitRecord
{
    QString date;         // "yyyy-MM-dd"
    QString name;
    QString course;
    QString department;
    QString timeIn;       // "HH:mm"
};

#endif // VISITLOGDATA_H
