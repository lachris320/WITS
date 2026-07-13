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
    void onSearchFinished(SearchOutcome outcome,
                          const QList<StudentRecord> &records,
                          const QString &message,
                          const QString &searchTerm);
    void onSearchFailed(const QString &errorString);
    void onCoursesLoaded(const QStringList &courses);

signals:
    void coursesChanged();
    void loadingChanged();
    void errorTextChanged();
    void resultsChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);

    QNetworkAccessManager *m_nam = nullptr;
    StudentController *m_controller = nullptr;
    SearchResultsModel m_results;
    QStringList m_courses;
    bool m_loading = false;
    QString m_errorText;
};

#endif // SEARCHVIEWMODEL_H
