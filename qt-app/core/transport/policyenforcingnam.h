#ifndef POLICYENFORCINGNAM_H
#define POLICYENFORCINGNAM_H

#include <QNetworkAccessManager>
#include <memory>

#include "transport/transportpolicy.h"

// The single choke point for every request in the process (S1 spec §3
// "PolicyEnforcingNam"). Both manager kinds — HttpClient's controller-facing
// manager and every QML-engine manager from PolicyNamFactory — are instances
// of this class. createRequest() runs each request through the policy before
// Qt's own pipeline.
//
// S1a: the policy is Passthrough, so the request is forwarded unchanged and
// behaviour is identical to a plain QNetworkAccessManager (proved on the wire
// by tst_policyenforcingnam). S1e adds rejection (error reply without touching
// the network), forced ManualRedirectPolicy and per-request QSslConfiguration
// here, without changing any caller.
class PolicyEnforcingNam : public QNetworkAccessManager
{
    Q_OBJECT
public:
    // A null policy falls back to TransportPolicy::current(), so a manager is
    // never policy-less.
    explicit PolicyEnforcingNam(std::shared_ptr<const TransportPolicy> policy,
                                QObject *parent = nullptr);

    std::shared_ptr<const TransportPolicy> policy() const { return m_policy; }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override;

private:
    const std::shared_ptr<const TransportPolicy> m_policy;
};

#endif // POLICYENFORCINGNAM_H
