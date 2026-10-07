#ifndef TRANSPORTPOLICY_H
#define TRANSPORTPOLICY_H

#include <QNetworkRequest>
#include <memory>

// The one client transport policy (S1 spec §2 "one security policy, not one
// physical manager"). Immutable and shared as std::shared_ptr<const
// TransportPolicy>; the process-wide snapshot is read/written with C++17
// std::atomic_load / std::atomic_store, so the QML factory and (S1e) the URL
// interceptor can read it from any thread.
//
// S1a ships ONLY Mode::Passthrough: prepare() returns the request unchanged —
// the current http default is allowed and no TLS/redirect enforcement is
// applied (spec §3 "S1a seam"). S1e adds the enforcing mode behind this same
// interface and deletes Passthrough; call sites (HttpClient, PolicyEnforcingNam,
// PolicyNamFactory) do not change.
class TransportPolicy
{
public:
    enum class Mode { Passthrough };

    static std::shared_ptr<const TransportPolicy> makePassthrough();

    Mode mode() const { return m_mode; }

    // The per-request hook PolicyEnforcingNam::createRequest() applies to every
    // request in the process. Passthrough: identity.
    QNetworkRequest prepare(const QNetworkRequest &request) const;

    // Process-wide snapshot. Never null: defaults to a Passthrough policy until
    // setCurrent() publishes another one.
    static std::shared_ptr<const TransportPolicy> current();
    // Atomically publishes `policy`. A null policy is ignored (warning), so
    // current() can never become null.
    static void setCurrent(std::shared_ptr<const TransportPolicy> policy);

    TransportPolicy(const TransportPolicy &) = delete;
    TransportPolicy &operator=(const TransportPolicy &) = delete;

private:
    explicit TransportPolicy(Mode mode) : m_mode(mode) {}

    const Mode m_mode;
};

#endif // TRANSPORTPOLICY_H
