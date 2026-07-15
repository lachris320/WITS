#ifndef SEARCHVIEWMODEL_H
#define SEARCHVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <qqml.h>
#include "SearchResultsModel.h"
#include "studentdata.h"

class QNetworkAccessManager;
class StudentController;

// Search screen VM (spec §6.2). Wraps StudentController: no new query endpoint,
// but exposes the lifetime `visits` count per result as "Total Visits: N".
class SearchViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(SearchResultsModel *results READ results CONSTANT)
    Q_PROPERTY(QStringList courses READ courses NOTIFY coursesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    explicit SearchViewModel(QObject *parent = nullptr);

    SearchResultsModel *results() { return &m_results; }
    QStringList courses() const { return m_courses; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    // Empty course => no course filter. department is left empty in Phase 3.
    Q_INVOKABLE void search(const QString &search, const QString &course);

    // Load-on-navigation hook (called by AdminScreen when the Search page is
    // shown). Populates the course filter chips; does NOT auto-run a search.
    // Kept out of the constructor so constructing the VM issues no network
    // (unit tests + the shell smoke test stay network-free).
    Q_INVOKABLE void refresh();

public slots:
    // Wired to StudentController; public so tests drive them network-free.
    // requestId echoes StudentController::searchStudents()'s return value —
    // see m_latestAppliedRequestId for why.
    void onSearchFinished(SearchOutcome outcome,
                          const QList<StudentRecord> &records,
                          const QString &message,
                          const QString &searchTerm,
                          quint64 requestId);
    void onSearchFailed(const QString &errorString, quint64 requestId);
    void onCoursesLoaded(const QStringList &courses);

signals:
    void coursesChanged();
    void loadingChanged();
    void errorTextChanged();
    void resultsChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);
    // True when requestId is stale (a reply for a search() call that has
    // since been superseded by a newer one) and must be dropped. On accept,
    // advances the high-water mark. See the class-level race-fix note below.
    bool acceptRequest(quint64 requestId);

    QNetworkAccessManager *m_nam = nullptr;
    StudentController *m_controller = nullptr;
    SearchResultsModel m_results;
    QStringList m_courses;
    bool m_loading = false;
    QString m_errorText;

    // Race fix: StudentController fires one independent QNetworkReply per
    // searchStudents() call and does not serialize/abort a prior in-flight
    // search (e.g. every course-chip click calls search()), so replies can
    // arrive out of request order. Rather than track "the id we're currently
    // waiting for" (which search() would have to set before the network call
    // races the setter — awkward, and untestable without hitting the
    // network), this tracks the highest request id whose result has actually
    // been applied. Ids are strictly increasing in issue order (one counter,
    // incremented per searchStudents() call), so a reply carrying an id lower
    // than one already applied can only be a stale, superseded reply — never
    // a legitimately newer one arriving "early". Starts at 0, which never
    // matches a real request id (StudentController's counter starts at 1),
    // so the very first reply is always accepted.
    quint64 m_latestAppliedRequestId = 0;
};

#endif // SEARCHVIEWMODEL_H
