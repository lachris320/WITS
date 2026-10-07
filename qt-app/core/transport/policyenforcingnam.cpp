#include "transport/policyenforcingnam.h"

#include <utility>

PolicyEnforcingNam::PolicyEnforcingNam(std::shared_ptr<const TransportPolicy> policy,
                                       QObject *parent)
    : QNetworkAccessManager(parent)
    , m_policy(policy ? std::move(policy) : TransportPolicy::current())
{
}

QNetworkReply *PolicyEnforcingNam::createRequest(Operation op, const QNetworkRequest &request,
                                                 QIODevice *outgoingData)
{
    return QNetworkAccessManager::createRequest(op, m_policy->prepare(request), outgoingData);
}
