#include "accesscontrol/eventbus.h"

namespace AccessControl {

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
    // Guarantee the access-control value types are registered the moment a bus
    // exists. This is the seam's self-contained registration point: because the
    // bus is central to every access-control flow, any binary that constructs one
    // references this translation unit, so accesstypes.cpp is force-linked and
    // registration can never be elided from the static library. (registerMetaTypes
    // is idempotent; the Q_CONSTRUCTOR_FUNCTION in accesstypes.cpp is a backstop.)
    registerMetaTypes();
}

void EventBus::publish(const AccessEvent &event)
{
    emit eventPublished(event);
}

} // namespace AccessControl
