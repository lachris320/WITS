#ifndef INITIALS_H
#define INITIALS_H

#include <QString>

// Shared "up to two uppercase initials from a name" derivation, used by the
// circular avatar chip on both the kiosk attendance feed (RecentLoginsModel)
// and the admin search results (SearchResultsModel) so the two surfaces
// cannot drift apart. Pure + Qt-Gui-free, mirroring the HttpForm.h namespace
// pattern (quick/HttpForm.h) for a small shared helper used by multiple
// quick/ classes.
namespace Initials {

// "Maria Santos" -> "MS", "Cher" -> "C", "" -> "", "  Ana   Reyes " -> "AR".
QString of(const QString &name);

} // namespace Initials

#endif // INITIALS_H
