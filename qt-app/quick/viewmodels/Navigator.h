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
public:
    enum Surface { Kiosk, Admin };
    Q_ENUM(Surface)

    explicit Navigator(QObject *parent = nullptr);

    Surface currentSurface() const { return m_surface; }

public slots:
    void showKiosk();
    void showAdmin();

signals:
    void currentSurfaceChanged();

private:
    void setSurface(Surface s);
    Surface m_surface = Kiosk;
};

#endif // NAVIGATOR_H
