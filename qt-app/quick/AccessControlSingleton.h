#ifndef ACCESSCONTROLSINGLETON_H
#define ACCESSCONTROLSINGLETON_H

#include <QQmlEngine>
#include "AccessControlHub.h"

// Registration shim only — never instantiated by QML. Exposes the app-owned
// AccessControlHub instance as the "AccessControl" QML singleton. A QML_FOREIGN
// wrapper (not QML_SINGLETON on the hub directly) because the hub is
// default-constructible in main(); Qt would otherwise prefer the default ctor
// over create() and hand QML a separate, uninitialized instance.
struct AccessControlSingleton
{
    Q_GADGET
    QML_FOREIGN(AccessControlHub)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AccessControl)
public:
    static AccessControlHub *create(QQmlEngine *, QJSEngine *)
    {
        AccessControlHub *inst = AccessControlHub::instance();
        Q_ASSERT_X(inst, "AccessControlSingleton::create",
                   "AccessControlHub::setInstance() must run before the engine loads");
        QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
        return inst;
    }
};

#endif // ACCESSCONTROLSINGLETON_H
