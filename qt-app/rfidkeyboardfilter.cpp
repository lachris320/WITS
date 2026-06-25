#include "rfidkeyboardfilter.h"

#include <QApplication>
#include <QKeyEvent>
#include <QWidget>

RfidKeyboardFilter::RfidKeyboardFilter(QWidget *loginWindow, QObject *parent)
    : QObject(parent)
    , m_loginWindow(loginWindow)
{
    m_clock.start();
}

bool RfidKeyboardFilter::isTerminator(int key)
{
    return key == Qt::Key_Return || key == Qt::Key_Enter;
}

bool RfidKeyboardFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    // Only intercept on the kiosk/login window — never the admin window etc.
    if (QApplication::activeWindow() != m_loginWindow)
        return QObject::eventFilter(watched, event);

    // A qApp-global event filter is invoked once per recipient as an unconsumed
    // key propagates up the parent chain — which double-counts each scanned
    // character. Act only on the key's primary target (the focus widget, or the
    // active window when nothing is focused); skip the propagated ancestor
    // deliveries.
    QWidget *target = QApplication::focusWidget();
    if (!target)
        target = QApplication::activeWindow();
    if (watched != target)
        return QObject::eventFilter(watched, event);

    auto *ke = static_cast<QKeyEvent *>(event);
    const qint64 ts = m_clock.elapsed();

    if (isTerminator(ke->key())) {
        std::optional<QString> code = m_detector.feedKey(QChar('\n'), ts);
        if (code) {
            emit rfidScanned(*code);
            return true; // consume Enter so returnPressed/handleLogin never fires
        }
        return QObject::eventFilter(watched, event);
    }

    const QString text = ke->text();
    if (text.isEmpty()) // modifiers (Shift for uppercase hex), function keys
        return QObject::eventFilter(watched, event);

    m_detector.feedKey(text.at(0), ts);
    return QObject::eventFilter(watched, event); // do NOT consume printable keys
}
