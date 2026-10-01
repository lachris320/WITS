#ifndef ACCESSCONTROL_CONTACTAGE_H
#define ACCESSCONTROL_CONTACTAGE_H

#include <QDateTime>
#include <QString>

namespace AccessControl {

// Pure presentation formatter for the admin page's feed-contact tile
// (Sub-plan 4, refinement 4). The view re-evaluates it against an advancing
// presentation clock, so the age keeps climbing with no new events.
//   !monitoringOn            -> "Monitoring off"
//   no prior contact         -> "No contact yet"
//   age < 60 s               -> "Last contact N s ago"
//   otherwise                -> "Last contact N min ago"   (floored minutes)
// A negative age (clock skew) clamps to 0.
QString formatContactAge(bool monitoringOn, const QDateTime &lastContact, const QDateTime &now);

} // namespace AccessControl

#endif // ACCESSCONTROL_CONTACTAGE_H
