#include "transport/httpclient.h"

#include "transport/policyenforcingnam.h"

HttpClient::HttpClient(QObject *parent, QNetworkAccessManager *injected)
    : QObject(parent)
    , m_policy(TransportPolicy::current())
{
    if (injected) {
        m_manager = injected;
        m_ownsManager = false;
    } else {
        m_manager = new PolicyEnforcingNam(m_policy, this);
        m_ownsManager = true;
    }
}
