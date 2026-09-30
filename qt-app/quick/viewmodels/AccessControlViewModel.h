#ifndef ACCESSCONTROLVIEWMODEL_H
#define ACCESSCONTROLVIEWMODEL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <qqml.h>
#include "AccessEntriesModel.h"

class QNetworkAccessManager;

// Access Control page VM (Sub-plan 4). Owns the admin-authenticated SNAPSHOT
// of access_recent.php: the recent-entries table, entries_today /
// last_entry_at, and the "Updated HH:MM:SS" time of the last SUCCESSFUL fetch.
// Live state (toggle, connection pill, contact age) lives on the AccessControl
// singleton, never here. Failure contract (spec refinement 6):
//  - ordinary failure after a success: keep rows, stale = true, freeze updatedAt;
//  - auth loss (401 / "Invalid admin key"): authFailure = true + clear the
//    protected data (rows, counts, updatedAt), stale = false. authFailure is
//    cleared ONLY by a later success — an ordinary failure keeps it (and keeps
//    the auth message as errorText) rather than hiding the re-login prompt;
//  - empty-but-valid feed (emptyFeed) is distinct from a failed initial load
//    (initialLoadFailed).
class AccessControlViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int entriesToday READ entriesToday NOTIFY dataChanged)
    Q_PROPERTY(QString lastEntryAt READ lastEntryAt NOTIFY dataChanged)
    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY dataChanged)
    Q_PROPERTY(bool emptyFeed READ emptyFeed NOTIFY dataChanged)
    Q_PROPERTY(bool initialLoadFailed READ initialLoadFailed NOTIFY dataChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)
    Q_PROPERTY(AccessEntriesModel *entries READ entries CONSTANT)

public:
    // nam: injection seam for tests (CapturingNam). Null in production: the VM
    // owns a fresh QNetworkAccessManager.
    explicit AccessControlViewModel(QObject *parent = nullptr, QNetworkAccessManager *nam = nullptr);

    int entriesToday() const { return m_entriesToday; }
    QString lastEntryAt() const { return m_lastEntryAt; }
    QString updatedAt() const { return m_updatedAt; }
    bool emptyFeed() const;
    bool initialLoadFailed() const;
    bool loading() const { return m_loading; }
    bool stale() const { return m_stale; }
    QString errorText() const { return m_errorText; }
    bool authFailure() const { return m_authFailure; }
    AccessEntriesModel *entries() { return &m_entries; }

    Q_INVOKABLE void refresh();

    // Network-free seam (tests + the reply handler).
    void applyRecent(const QByteArray &raw);

    // In-flight request generation guard (same idiom as VisitLogsViewModel):
    // a reply superseded by a newer refresh() is dropped.
    quint64 nextRequestSeq();
    bool isCurrentRequest(quint64 seq) const;

signals:
    void dataChanged();
    void loadingChanged();
    void staleChanged();
    void errorTextChanged();
    void authFailureChanged();

private:
    void applyFailure(const QString &message);
    void applyAuthFailure();
    static QString authFailureMessage();
    void setLoading(bool v);
    void setStale(bool v);
    void setError(const QString &e);
    void setAuthFailure(bool v);

    QNetworkAccessManager *m_nam = nullptr;
    AccessEntriesModel m_entries;
    int m_entriesToday = 0;
    QString m_lastEntryAt;
    QString m_updatedAt;
    bool m_hasLoaded = false;   // a successful load is currently on screen
    bool m_loading = false;
    bool m_stale = false;
    QString m_errorText;
    bool m_authFailure = false;
    quint64 m_requestSeq = 0;
};

#endif // ACCESSCONTROLVIEWMODEL_H
