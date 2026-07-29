#ifndef SCHOOLINFOUTIL_H
#define SCHOOLINFOUTIL_H

#include <QString>
#include <QUrl>

// Shared school-info derivation used by both school-identity ViewModels
// (KioskViewModel + SchoolInfoViewModel) so the robustness-critical logo
// seam cannot drift between the kiosk and admin surfaces. Pure + Qt-Gui-free,
// mirroring the Initials.h / HttpForm.h namespace pattern for a small shared
// helper used by multiple quick/ classes. Internal C++ util, NOT QML-facing:
// it is called BY the VMs, never exposed to QML.
namespace SchoolInfoUtil {

// Resolve a configured logo path to a URL, exposing one ONLY when the file
// actually exists right now (the graceful-degradation seam: a rotted path
// must never reach a QML Image). Re-statting on every call is what lets a
// deleted logo flip back to "no logo". *hasLogo is set to whether a usable
// file exists; the returned QUrl is QUrl::fromLocalFile(path) when it does,
// an empty QUrl otherwise. hasLogo must be non-null.
QUrl resolveLogoUrl(const QString &path, bool *hasLogo);

} // namespace SchoolInfoUtil

#endif // SCHOOLINFOUTIL_H
