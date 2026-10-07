#ifndef POLICYNAMFACTORY_H
#define POLICYNAMFACTORY_H

#include <QQmlNetworkAccessManagerFactory>
#include <atomic>

class QQmlEngine;

// QML-engine side of the S1 transport seam (spec §2). Installed on the engine
// before the first QML load (quick/main.cpp, QuickTestSetup.h). Qt calls
// create() for every manager the engine needs — including from its
// image-reader / loader threads — and each call returns a NEW
// PolicyEnforcingNam owned by `parent`, built with the TransportPolicy
// snapshot current at that instant (atomic load). Not a QML type.
// The factory must outlive every engine it is installed on.
class PolicyNamFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;

    // Diagnostic: how many managers this factory has created (thread-safe).
    int createdCount() const { return m_created.load(); }

    // The only sanctioned way to install the factory. Returns false (after a
    // qCritical) when the engine is already unusable for the seam, and the
    // caller MUST then abort initialization. Refusal cases:
    //  1. the engine already has a factory;
    //  2. a QQmlApplicationEngine already has root objects;
    //  3. after installing, the engine's common (GUI-thread) manager is not a
    //     PolicyEnforcingNam — Qt keeps a manager the engine created before the
    //     factory was set, and does not guarantee that changing the factory
    //     replaces it. Cases 1-2 leave the engine untouched; in case 3 the
    //     factory stays set and the engine must not be used.
    // Limits: case 3 checks only the engine's common manager (this call
    // eagerly creates it through create()), not the managers Qt creates on QML
    // worker threads (those come from create(); proven by the image-loading
    // test). It also cannot see components compiled on a plain
    // QQmlEngine/QQuickView before the call. Installation must therefore
    // still happen on a freshly constructed engine before any QML executes
    // (quick/main.cpp is pinned by a source-order guard test).
    static bool installOn(QQmlEngine &engine, PolicyNamFactory &factory);

private:
    std::atomic<int> m_created{0};
};

#endif // POLICYNAMFACTORY_H
