#include "rfidkeyboardfilter.h"

#include <QApplication>
#include <QWidget>

RfidKeyboardFilter::RfidKeyboardFilter(QWidget *loginWindow, QObject *parent)
    : RfidEventFilterBase(parent)
    , m_loginWindow(loginWindow)
{
}

bool RfidKeyboardFilter::gateOpen(QObject *watched) const
{
    // Only while the login window is the active window.
    if (QApplication::activeWindow() != m_loginWindow)
        return false;
    // Act only on the key's primary target (focus widget, or the active window
    // when nothing is focused); skip propagated ancestor deliveries so a
    // qApp-global filter never double-counts a character.
    QWidget *target = QApplication::focusWidget();
    if (!target)
        target = QApplication::activeWindow();
    return watched == target;
}
