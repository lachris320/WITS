#include "accesscontrol/accesstypes.h"

namespace AccessControl {

void registerMetaTypes()
{
    // qRegisterMetaType is idempotent, so this is safe to call repeatedly.
    qRegisterMetaType<Credential>();
    qRegisterMetaType<AccessDecision>();
    qRegisterMetaType<AccessEvent>();
    qRegisterMetaType<GateDescriptor>();
    qRegisterMetaType<ProviderDescriptor>();
    qRegisterMetaType<ConnectionState>();
}

} // namespace AccessControl

// Register automatically when witscore loads, so the types are name-resolvable
// before any consumer sets up a connection — no reliance on a caller remembering
// to call registerMetaTypes(). Q_CONSTRUCTOR_FUNCTION must sit at file scope,
// outside the namespace, and takes a free function.
namespace { void accesscontrolRegisterMetaTypes() { AccessControl::registerMetaTypes(); } }
Q_CONSTRUCTOR_FUNCTION(accesscontrolRegisterMetaTypes)
