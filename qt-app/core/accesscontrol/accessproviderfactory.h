#ifndef ACCESSCONTROL_ACCESSPROVIDERFACTORY_H
#define ACCESSCONTROL_ACCESSPROVIDERFACTORY_H

#include <functional>
#include <QList>
#include <QVariantMap>
#include "accesscontrol/accesstypes.h"

namespace AccessControl {

class IAccessProvider;

// Registry mapping a ProviderDescriptor (keyed by providerId) to a creator.
// Providers register their creator at startup; the admin UI (a later sub-plan)
// reads available() to populate a picker and calls create() for the chosen one.
// Created providers are parented to the caller-supplied parent — ownership via
// the Qt object tree, never the factory.
class AccessProviderFactory
{
public:
    using CreatorFn =
        std::function<IAccessProvider *(const ProviderDescriptor &, const QVariantMap &, QObject *)>;

    void registerProvider(const ProviderDescriptor &descriptor, CreatorFn creator);
    QList<ProviderDescriptor> available() const;
    // parent is REQUIRED (non-null): ownership is mandatory, so a null parent
    // yields nullptr rather than an unowned provider.
    IAccessProvider *create(const ProviderDescriptor &descriptor,
                            const QVariantMap &config,
                            QObject *parent) const;

private:
    struct Entry {
        ProviderDescriptor descriptor;
        CreatorFn creator;
    };
    QList<Entry> m_entries;   // registration order preserved
};

} // namespace AccessControl

#endif // ACCESSCONTROL_ACCESSPROVIDERFACTORY_H
