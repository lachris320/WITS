#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <memory>

#include "transport/transportpolicy.h"

class QNetworkAccessManager;

// The controller-facing side of the S1 transport seam (spec §2, §3 "Injection
// boundary"). Every C++ owner that used to build its own QNetworkAccessManager
// (ViewModels, AccessControlHub) now builds — or is injected with — an
// HttpClient and hands manager() to its controllers / HttpForm calls.
//
// Production (injected == nullptr): owns one PolicyEnforcingNam as a QObject
// child, carrying the TransportPolicy snapshot current at construction.
// Tests: pass a CapturingNam / SequencedNam as `injected`; it is used as-is,
// never reparented or deleted (the test owns it and must outlive the client).
// Argument order mirrors the ViewModel idiom (parent first): HttpClient(&nam)
// would make `nam` the PARENT, not the injected manager.
class HttpClient : public QObject
{
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr, QNetworkAccessManager *injected = nullptr);

    QNetworkAccessManager *manager() const { return m_manager; }
    std::shared_ptr<const TransportPolicy> policy() const { return m_policy; }
    bool ownsManager() const { return m_ownsManager; }

private:
    const std::shared_ptr<const TransportPolicy> m_policy;
    QNetworkAccessManager *m_manager = nullptr;   // owned child, or injected (not owned)
    bool m_ownsManager = false;
};

#endif // HTTPCLIENT_H
