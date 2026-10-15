#include "transport/transportpolicy.h"

#include <QtGlobal>
#include <atomic>
#include <utility>

namespace {
// Function-local static: thread-safe initialisation (C++11 magic statics).
// Only ever accessed through std::atomic_load / std::atomic_store below.
std::shared_ptr<const TransportPolicy> &currentSlot()
{
    static std::shared_ptr<const TransportPolicy> slot = TransportPolicy::makePassthrough();
    return slot;
}
} // namespace

std::shared_ptr<const TransportPolicy> TransportPolicy::makePassthrough()
{
    return std::shared_ptr<const TransportPolicy>(new TransportPolicy(Mode::Passthrough));
}

QNetworkRequest TransportPolicy::prepare(const QNetworkRequest &request) const
{
    switch (m_mode) {
    case Mode::Passthrough:
        return request;
    }
    return request;
}

std::shared_ptr<const TransportPolicy> TransportPolicy::current()
{
    return std::atomic_load(&currentSlot());
}

void TransportPolicy::setCurrent(std::shared_ptr<const TransportPolicy> policy)
{
    if (!policy) {
        qWarning("TransportPolicy::setCurrent: null policy ignored");
        return;
    }
    std::atomic_store(&currentSlot(), std::move(policy));
}
