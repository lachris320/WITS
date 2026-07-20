#include "Navigator.h"

#include "AdminSession.h"

Navigator::Navigator(QObject *parent)
    : QObject(parent)
{
}

void Navigator::setSurface(Surface s)
{
    if (m_surface == s)
        return;                      // idempotent: no redundant re-theme/re-layout
    m_surface = s;
    emit currentSurfaceChanged();
}

void Navigator::showKiosk()
{
    // Leaving the admin surface ENDS the admin session, so the held admin key
    // goes with it. Without this the plaintext key sat in process memory for
    // the rest of an unattended kiosk's uptime (crash dumps, swap, debugger
    // attach) — precisely what AdminSession's "RAM ONLY" contract exists to
    // bound. Not an auth change: re-entering admin still re-posts the key to
    // admin_login.php.
    //
    // Safe for in-flight requests. Every guarded POST reads the key when it
    // BUILDS its field list and HttpForm::submit URL-encodes it into the body
    // there and then, so a request already on the wire carries its own copy.
    // And this surface change is exactly what makes AppShell's Loader tear
    // down AdminScreen — destroying the ViewModels that are those replies'
    // connect() context, so their callbacks never run at all. The one path
    // that WRITES the key after a response (changeAdminKey -> refresh()) lives
    // on that same destroyed VM.
    //
    // Deliberately unconditional, outside setSurface()'s idempotence guard: a
    // redundant "back to kiosk" is still an explicit request to end the
    // session, and hanging the wipe off the surface-changed edge would
    // silently skip it. Cleared BEFORE the surface change so nothing reached
    // by currentSurfaceChanged can observe a stale key.
    AdminSession::instance().clear();
    setSurface(Kiosk);
}

void Navigator::showAdmin()
{
    setSurface(Admin);
    // Entering admin always lands on Dashboard, even if a prior admin
    // session left a different sub-page selected — the user wants a
    // predictable Dashboard-first landing, not to resume where they left off.
    showAdminPage(Dashboard);
}

void Navigator::showAdminPage(AdminPage page)
{
    if (m_adminPage == page)
        return;                      // idempotent — mirrors setSurface's guard
    m_adminPage = page;
    emit adminPageChanged();
}
