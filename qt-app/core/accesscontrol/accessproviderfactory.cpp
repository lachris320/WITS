#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/iaccessprovider.h"

namespace AccessControl {

void AccessProviderFactory::registerProvider(const ProviderDescriptor &descriptor,
                                             CreatorFn creator)
{
    m_entries.append(Entry{ descriptor, std::move(creator) });
}

QList<ProviderDescriptor> AccessProviderFactory::available() const
{
    QList<ProviderDescriptor> out;
    out.reserve(m_entries.size());
    for (const Entry &e : m_entries)
        out.append(e.descriptor);
    return out;
}

IAccessProvider *AccessProviderFactory::create(const ProviderDescriptor &descriptor,
                                               const QVariantMap &config,
                                               QObject *parent) const
{
    if (!parent)
        return nullptr;   // ownership is mandatory — never return an unowned provider
    for (const Entry &e : m_entries) {
        if (e.descriptor.providerId == descriptor.providerId && e.creator) {
            IAccessProvider *p = e.creator(descriptor, config, parent);
            // Enforce the ownership contract regardless of what the creator did:
            // a creator that forgot to pass `parent` (or parented elsewhere) must
            // not leave the provider unowned. setParent is a no-op if already correct.
            if (p && p->parent() != parent)
                p->setParent(parent);
            return p;
        }
    }
    return nullptr;   // unknown id — caller decides how to surface it
}

} // namespace AccessControl
