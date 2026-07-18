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
    connect(m_controller, &StudentController::departmentsLoaded,
            this, &SearchViewModel::onDepartmentsLoaded);
    // NOTE: no network in the constructor — the filter chips load in refresh(),
    // triggered when the Search page is navigated to (AdminScreen autoLoad).
}

void SearchViewModel::refresh()
{
    m_controller->loadCourses(QString());   // populate the course filter chips
    m_controller->loadDepartments();        // populate the department filter chips
}

void SearchViewModel::search(const QString &search, const QString &course)
{
    setError(QString());
    setLoading(true);
    m_controller->searchStudents(search, m_department, course);
}

void SearchViewModel::setDepartment(const QString &department)
{
    if (m_department == department)
        return;
    m_department = department;
    emit departmentChanged();
    m_controller->loadCourses(department);   // re-scope the course filter chips
}

void SearchViewModel::onSearchFinished(SearchOutcome outcome,
                                       const QList<StudentRecord> &records,
                                       const QString &message,
                                       const QString &searchTerm,
                                       quint64 requestId)
{
    Q_UNUSED(searchTerm)
    if (!acceptRequest(requestId))
        return;   // superseded by a newer search() call — drop

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

void SearchViewModel::onSearchFailed(const QString & /*errorString*/, quint64 requestId)
{
    if (!acceptRequest(requestId))
        return;   // superseded by a newer search() call — drop

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

void SearchViewModel::onDepartmentsLoaded(const QStringList &departments)
{
    m_departments = departments;
    emit departmentsChanged();
}

bool SearchViewModel::acceptRequest(quint64 requestId)
{
    if (requestId < m_latestAppliedRequestId)
        return false;
    m_latestAppliedRequestId = requestId;
    return true;
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
