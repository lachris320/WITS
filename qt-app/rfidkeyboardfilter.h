#ifndef RFIDKEYBOARDFILTER_H
#define RFIDKEYBOARDFILTER_H

#include <QObject>
#include <QElapsedTimer>
#include "rfidscandetector.h"

class QWidget;

// Application-wide key event filter. While the login window is the active
// window, feeds printable keys (and a normalized terminator) to an
// RfidScanDetector; on a recognized scan it consumes the terminating Enter
// (so QLineEdit::returnPressed / handleLogin never fires) and emits the code.
class RfidKeyboardFilter : public QObject
{
    Q_OBJECT
public:
    explicit RfidKeyboardFilter(QWidget *loginWindow, QObject *parent = nullptr);

    static bool isTerminator(int key);

signals:
    void rfidScanned(const QString &code);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_loginWindow;
    RfidScanDetector m_detector;
    QElapsedTimer m_clock;
};

#endif // RFIDKEYBOARDFILTER_H
