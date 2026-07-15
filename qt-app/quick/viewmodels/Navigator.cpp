#include "Navigator.h"

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

void Navigator::showKiosk() { setSurface(Kiosk); }

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
