#ifndef RFIDKEYBOARDFILTER_H
#define RFIDKEYBOARDFILTER_H

#include "rfideventfilterbase.h"

class QWidget;

// Widgets-side install of RfidEventFilterBase. Gates on the login window being
// QApplication::activeWindow() AND `watched` being the key's primary target
// (focusWidget else activeWindow) — the ancestor-dedup that stops a qApp-global
// filter from double-counting a propagating key.
class RfidKeyboardFilter : public RfidEventFilterBase
{
    Q_OBJECT
public:
    explicit RfidKeyboardFilter(QWidget *loginWindow, QObject *parent = nullptr);

protected:
    bool gateOpen(QObject *watched) const override;

private:
    QWidget *m_loginWindow;
};

#endif // RFIDKEYBOARDFILTER_H
