#ifndef ACCESSCONTROL_EVENTBUS_H
#define ACCESSCONTROL_EVENTBUS_H

#include <QObject>
#include "accesscontrol/accesstypes.h"

namespace AccessControl {

// Typed, in-process publish/subscribe over a single Qt signal. The bus's
// currency is DECIDED AccessEvents; a raw Credential is never published here.
// A subscriber "subscribes" by connecting to eventPublished().
//
// Delivery contract (v1): publish() emits a DIRECT signal, so delivery is
// synchronous and on the CALLER'S thread — every subscriber runs to completion
// before publish() returns. The bus is single-threaded (see Global Constraints);
// a cross-thread publisher must marshal onto the bus's thread. Re-entrant
// (nested) publish() from inside a subscriber IS supported and delivered
// depth-first: the inner event reaches all subscribers before the outer
// delivery resumes. Subscribers must therefore not block. No internal queue.
class EventBus : public QObject
{
    Q_OBJECT
public:
    explicit EventBus(QObject *parent = nullptr);
    void publish(const AccessEvent &event);

signals:
    void eventPublished(const AccessControl::AccessEvent &event);
};

} // namespace AccessControl

#endif // ACCESSCONTROL_EVENTBUS_H
