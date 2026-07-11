#include "RfidQuickFilter.h"

#include <QQuickWindow>

RfidQuickFilter::RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent)
    : RfidEventFilterBase(parent)
    , m_loginWindow(loginWindow)
{
}

bool RfidQuickFilter::gateOpen(QObject *watched) const
{
    return !m_loginWindow.isNull()
        && watched == m_loginWindow
        && m_loginWindow->isActive();
}
