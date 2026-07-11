#include "rfideventfilterbase.h"

#include <QKeyEvent>

RfidEventFilterBase::RfidEventFilterBase(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
}

bool RfidEventFilterBase::isTerminator(int key)
{
    return key == Qt::Key_Return || key == Qt::Key_Enter;
}

bool RfidEventFilterBase::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    if (!gateOpen(watched))
        return QObject::eventFilter(watched, event);

    auto *ke = static_cast<QKeyEvent *>(event);
    const qint64 ts = m_clock.elapsed();

    if (isTerminator(ke->key())) {
        std::optional<QString> code = m_detector.feedKey(QChar('\n'), ts);
        if (code) {
            emit rfidScanned(*code);
            return true;   // consume Enter so no default activation fires
        }
        return QObject::eventFilter(watched, event);
    }

    const QString text = ke->text();
    if (text.isEmpty())   // modifiers / function keys
        return QObject::eventFilter(watched, event);

    m_detector.feedKey(text.at(0), ts);
    return QObject::eventFilter(watched, event);   // never consume printable keys
}
