#ifndef RFIDQUICKFILTER_H
#define RFIDQUICKFILTER_H

#include <QPointer>
#include "rfideventfilterbase.h"

class QQuickWindow;

// Quick-side install of RfidEventFilterBase. Gates on the specific login
// QQuickWindow being active. QPointer so a destroyed window closes the gate
// instead of dangling.
class RfidQuickFilter : public RfidEventFilterBase
{
    Q_OBJECT
public:
    explicit RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent = nullptr);

protected:
    bool gateOpen(QObject *watched) const override;

private:
    QPointer<QQuickWindow> m_loginWindow;
};

#endif // RFIDQUICKFILTER_H
