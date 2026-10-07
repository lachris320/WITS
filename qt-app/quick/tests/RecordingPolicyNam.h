#ifndef RECORDINGPOLICYNAM_H
#define RECORDINGPOLICYNAM_H

#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QThread>
#include <QUrl>

#include "PolicyNamFactory.h"
#include "transport/policyenforcingnam.h"
#include "transport/transportpolicy.h"

// Test-only proof that a request really went through a manager made by the
// installed factory. RecordingPolicyNamFactory is a PolicyNamFactory whose
// managers are PolicyEnforcingNams that log each request (URL + issuing
// thread) and then hand it to the real PolicyEnforcingNam pipeline. Counting
// create() calls is NOT proof (an engine may reuse a manager), so tests assert
// on this log instead. Thread-safe: managers may live on loader threads.
struct RequestLog
{
    mutable QMutex mutex;
    QList<QUrl> urls;
    QList<QThread *> threads;

    bool contains(const QUrl &url) const
    {
        QMutexLocker lock(&mutex);
        return urls.contains(url);
    }
    // Thread that issued the first request for `url`, or nullptr if none.
    QThread *threadFor(const QUrl &url) const
    {
        QMutexLocker lock(&mutex);
        const qsizetype i = urls.indexOf(url);
        return i < 0 ? nullptr : threads.at(i);
    }
};

class RecordingPolicyNam : public PolicyEnforcingNam
{
public:
    RecordingPolicyNam(RequestLog *log, QObject *parent)
        : PolicyEnforcingNam(TransportPolicy::current(), parent), m_log(log) {}

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        {
            QMutexLocker lock(&m_log->mutex);
            m_log->urls << request.url();
            m_log->threads << QThread::currentThread();
        }
        return PolicyEnforcingNam::createRequest(op, request, outgoingData);
    }

private:
    RequestLog *m_log;
};

class RecordingPolicyNamFactory : public PolicyNamFactory
{
public:
    explicit RecordingPolicyNamFactory(RequestLog *log) : m_log(log) {}
    QNetworkAccessManager *create(QObject *parent) override
    {
        return new RecordingPolicyNam(m_log, parent);
    }

private:
    RequestLog *m_log;
};

#endif // RECORDINGPOLICYNAM_H
