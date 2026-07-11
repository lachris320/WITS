#ifndef RFIDQUICKFILTER_H
#define RFIDQUICKFILTER_H

#include <QObject>
#include <QElapsedTimer>
#include "rfidscandetector.h"

class QQuickWindow;

// Quick-side replacement for RfidKeyboardFilter. Reuses the RfidScanDetector
// state machine verbatim; the QApplication::activeWindow()/focusWidget() gates
// (rfidkeyboardfilter.cpp:25,33-36) are re-authored as QQuickWindow focus
// semantics: Qt Quick delivers key events to the QQuickWindow itself (a
// QWindow), so this filter installs on the specific login window and gates on
// that window being the active window — there is no qApp-global ancestor
// double-delivery to de-duplicate.
class RfidQuickFilter : public QObject
{
    Q_OBJECT
public:
    explicit RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent = nullptr);

    static bool isTerminator(int key);

signals:
    void rfidScanned(const QString &code);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    // Seam: production gate is "watched is the login window AND it is active".
    // Tests override to force it open under the offscreen QPA.
    virtual bool gateOpen(QObject *watched) const;

private:
    QQuickWindow *m_loginWindow;
    RfidScanDetector m_detector;
    QElapsedTimer m_clock;
};

#endif // RFIDQUICKFILTER_H
