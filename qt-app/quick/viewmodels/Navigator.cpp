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
void Navigator::showAdmin() { setSurface(Admin); }

void Navigator::showAdminPage(AdminPage page)
{
    if (m_adminPage == page)
        return;                      // idempotent — mirrors setSurface's guard
    m_adminPage = page;
    emit adminPageChanged();
}
