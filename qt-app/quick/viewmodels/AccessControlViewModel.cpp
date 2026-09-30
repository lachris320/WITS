#include "AccessControlViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QVariant>
#include "AdminSession.h"
#include "HttpForm.h"
#include "SettingsViewModel.h"
#include "apiconfig.h"
#include "loginparser.h"

AccessControlViewModel::AccessControlViewModel(QObject *parent, QNetworkAccessManager *nam)
    : QObject(parent)
    , m_nam(nam ? nam : new QNetworkAccessManager(this))
{
}

bool AccessControlViewModel::emptyFeed() const
{
    return m_hasLoaded && m_entries.rowCount() == 0;
}

bool AccessControlViewModel::initialLoadFailed() const
{
    return !m_hasLoaded && !m_authFailure && !m_errorText.isEmpty();
}

void AccessControlViewModel::refresh()
{
    setLoading(true);
    // Clear ONLY the error text when a request starts, so a repeated identical
    // failure re-emits errorTextChanged (the view toasts on it). Rows, stale,
    // updatedAt and authFailure are deliberately left as-is until the reply.
    setError(QString());
    // admin_key rides the urlencoded POST body ONLY (never the query string):
    // the backend's extractAdminKey() reads $_POST, and a secret in the URL
    // would leak into access logs.
    QNetworkRequest req = HttpForm::formRequest(
        ApiConfig::endpoint(QStringLiteral("access_recent.php")));
    QNetworkReply *reply = m_nam->post(req, HttpForm::encodeForm(
        {{QStringLiteral("admin_key"), AdminSession::instance().key()}}));
    const quint64 seq = nextRequestSeq();
    connect(reply, &QNetworkReply::finished, this, [this, reply, seq]() {
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (!isCurrentRequest(seq)) return;   // superseded — drop
        setLoading(false);
        // HTTP 401 is authoritative and checked BEFORE any body handling, so
        // an empty/non-JSON 401 still clears protected data. The in-band
        // "Invalid admin key" message (applyRecent) is only a secondary path.
        if (httpStatus == 401) { applyAuthFailure(); return; }
        if (!HttpForm::isServerAnswer(hadError, httpStatus, body)) {
            applyFailure(tr("Network error. Please try again."));
            return;
        }
        applyRecent(body);                    // 5xx-with-body lands here as invalid
    });
}

void AccessControlViewModel::applyRecent(const QByteArray &raw)
{
    const LoginParser::RecentFeedResult r = LoginParser::parseRecentFeed(raw);
    if (!r.valid) {
        if (SettingsViewModel::isAuthFailureMessage(r.error))
            applyAuthFailure();
        else
            applyFailure(tr("Could not refresh the access feed."));
        return;
    }
    m_entries.setEntries(r.entries);
    m_entriesToday = r.entriesToday;
    m_lastEntryAt = r.lastEntryAt;
    m_updatedAt = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));   // client wall clock
    m_hasLoaded = true;
    setStale(false);
    setAuthFailure(false);
    setError(QString());
    emit dataChanged();
}

void AccessControlViewModel::applyFailure(const QString &message)
{
    // Keep the last-known rows + counts and FREEZE updatedAt; the view marks
    // them stale. Only stale when there is a prior successful load on screen.
    setStale(m_hasLoaded);
    setAuthFailure(false);
    setError(message);
    emit dataChanged();
}

void AccessControlViewModel::applyAuthFailure()
{
    // Rejected key: do not leave protected rows on screen. Does NOT clear
    // AdminSession or navigate away — no admin page does that today.
    m_entries.clear();
    m_entriesToday = 0;
    m_lastEntryAt.clear();
    m_updatedAt.clear();
    m_hasLoaded = false;
    setStale(false);
    setAuthFailure(true);
    setError(tr("Admin authentication failed — re-enter via admin login."));
    emit dataChanged();
}

void AccessControlViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void AccessControlViewModel::setStale(bool v)
{
    if (m_stale == v) return;
    m_stale = v;
    emit staleChanged();
}

void AccessControlViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}

void AccessControlViewModel::setAuthFailure(bool v)
{
    if (m_authFailure == v) return;
    m_authFailure = v;
    emit authFailureChanged();
}

quint64 AccessControlViewModel::nextRequestSeq()
{
    return ++m_requestSeq;
}

bool AccessControlViewModel::isCurrentRequest(quint64 seq) const
{
    return seq == m_requestSeq;
}
