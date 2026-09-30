#ifndef ACCESSCONTROL_REPLYLIFECYCLE_H
#define ACCESSCONTROL_REPLYLIFECYCLE_H

#include <QNetworkReply>
#include <QObject>
#include <QTimer>

namespace AccessControl {

// Binds the shared network-reply lifecycle to a freshly-created reply:
//  - self-cleanup: the reply deleteLater()s itself when it finishes;
//  - a single-shot timeout timer (parented to the reply, so it dies with it)
//    that abort()s the reply on expiry — the abort finishes the reply with
//    OperationCanceledError, which each caller routes to its own error/fail path.
// Callers add their own finished-handler connect (with a context object) and any
// ownership beyond this (e.g. reparenting / QPointer tracking) after calling this.
inline void armReplyLifecycle(QNetworkReply *reply, int timeoutMs)
{
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, [reply]() { reply->abort(); });
    timer->start(timeoutMs);
}

} // namespace AccessControl

#endif // ACCESSCONTROL_REPLYLIFECYCLE_H
