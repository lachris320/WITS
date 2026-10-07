#include "PolicyNamFactory.h"

#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlEngine>

#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

QNetworkAccessManager *PolicyNamFactory::create(QObject *parent)
{
    m_created.fetch_add(1);
    return new PolicyEnforcingNam(TransportPolicy::current(), parent);
}

bool PolicyNamFactory::installOn(QQmlEngine &engine, PolicyNamFactory &factory)
{
    if (engine.networkAccessManagerFactory()) {
        qCritical("PolicyNamFactory::installOn: engine already has a network factory");
        return false;
    }
    if (auto *app = qobject_cast<QQmlApplicationEngine *>(&engine);
        app && !app->rootObjects().isEmpty()) {
        qCritical("PolicyNamFactory::installOn: QML already loaded");
        return false;
    }
    engine.setNetworkAccessManagerFactory(&factory);
    // Qt keeps a manager the engine created before the factory was set, so
    // verify rather than assume. A real runtime branch (not Q_ASSERT): it must
    // hold in Release builds.
    if (!qobject_cast<PolicyEnforcingNam *>(engine.networkAccessManager())) {
        qCritical("PolicyNamFactory::installOn: engine already created a network manager");
        return false;
    }
    return true;
}
