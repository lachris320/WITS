#include "SearchViewModel.h"

#include <QNetworkAccessManager>
#include "studentcontroller.h"

SearchViewModel::SearchViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new StudentController(m_nam, this))
{
    connect(m_controller, &StudentController::searchFinished,
            this, &SearchViewModel::onSearchFinished);
    connect(m_controller, &StudentController::searchFailed,
            this, &SearchViewModel::onSearchFailed);
    connect(m_controller, &StudentController::coursesLoaded,
            this, &SearchViewModel::onCoursesLoaded);
    // NOTE: no network in the constructor — the filter chips load in refresh(),
    // triggered when the Search page is navigated to (AdminScreen autoLoad).
}

void SearchViewModel::refresh()
{
    m_controller->loadCourses(QString());   // populate the filter chips
}

void SearchViewModel::search(const QString &search, const QString &course)
{
    setError(QString());
    setLoading(true);
    m_controller->searchStudents(search, QString(), course);
}

void SearchViewModel::onSearchFinished(SearchOutcome outcome,
                                       const QList<StudentRecord> &records,
                                       const QString &message,
                                       const QString &searchTerm)
{
    Q_UNUSED(searchTerm)
    setLoading(false);
    m_results.setRecords(records);
    emit resultsChanged();
    if (outcome == SearchOutcome::InvalidResponse) {
        setError(QStringLiteral("Invalid server response."));
    } else if (outcome == SearchOutcome::NotSuccess) {
        setError(message.isEmpty() ? QStringLiteral("No students found.") : message);
    } else {
        setError(QString());   // Results or Empty — empty state handled by the screen
    }
}

void SearchViewModel::onSearchFailed(const QString & /*errorString*/)
{
    // Do NOT surface the raw transport string (security-hygiene).
    setLoading(false);
    // Consistent with onSearchFinished, which unconditionally calls
    // setRecords(records) (clearing stale rows on InvalidResponse/NotSuccess
    // too) — a failed re-search must not leave the PREVIOUS search's results
    // on screen behind the error banner.
    m_results.setRecords({});
    emit resultsChanged();
    setError(QStringLiteral("Network error. Please try again."));
}

void SearchViewModel::onCoursesLoaded(const QStringList &courses)
{
    m_courses = courses;
    emit coursesChanged();
}

void SearchViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void SearchViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}
