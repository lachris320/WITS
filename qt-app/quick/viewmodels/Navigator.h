#ifndef NAVIGATOR_H
#define NAVIGATOR_H

#include <QObject>
#include <qqml.h>

// Owns which top-level surface is showing. A C++ singleton so surface flow is
// unit-testable and (later) keyboard shortcuts route through one place.
class Navigator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(Surface currentSurface READ currentSurface NOTIFY currentSurfaceChanged)
    Q_PROPERTY(AdminPage adminPage READ adminPage NOTIFY adminPageChanged)
public:
    enum Surface { Kiosk, Admin };
    Q_ENUM(Surface)
    enum AdminPage { Dashboard, Search, VisitLogs };
    Q_ENUM(AdminPage)

    explicit Navigator(QObject *parent = nullptr);

    Surface currentSurface() const { return m_surface; }
    AdminPage adminPage() const { return m_adminPage; }

public slots:
    void showKiosk();
    void showAdmin();
    void showAdminPage(AdminPage page);

signals:
    void currentSurfaceChanged();
    void adminPageChanged();

private:
    void setSurface(Surface s);
    Surface m_surface = Kiosk;
    AdminPage m_adminPage = Dashboard;
};

#endif // NAVIGATOR_H
