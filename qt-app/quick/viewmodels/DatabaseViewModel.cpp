#include "DatabaseViewModel.h"

#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QSaveFile>
#include <QUrl>
#include "studentcontroller.h"
#include "AdminSession.h"
#include "SettingsViewModel.h"

DatabaseViewModel::DatabaseViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new StudentController(m_nam, this))
{
    connect(m_controller, &StudentController::searchFinished, this, &DatabaseViewModel::onSearchFinished);
    connect(m_controller, &StudentController::searchFailed, this, &DatabaseViewModel::onSearchFailed);
    connect(m_controller, &StudentController::departmentsLoaded, this, &DatabaseViewModel::onDepartmentsLoaded);
    connect(m_controller, &StudentController::coursesLoaded, this, &DatabaseViewModel::onCoursesLoaded);
    connect(m_controller, &StudentController::deleteFinished, this, &DatabaseViewModel::onDeleteFinished);
    connect(m_controller, &StudentController::deleteFailed, this, &DatabaseViewModel::onDeleteFailed);
}

void DatabaseViewModel::refresh()
{
    m_controller->loadDepartments();
    reloadTable();
}

void DatabaseViewModel::reloadTable()
{
    setError(QString());
    setLoading(true);
    // Empty search + current dept/course filter = "all matching students".
    m_controller->searchStudents(QString(), m_department, m_course);
}

void DatabaseViewModel::setDepartment(const QString &department)
{
    if (m_department != department) {
        m_department = department;
        emit departmentChanged();
        // Dependent-clear (Critical fix): a new department invalidates the
        // course filter. The QML cascade clears its own combo, but the VM must
        // also drop m_course or reloadTable() would send a course from the OLD
        // department and return wrong/empty rows.
        if (!m_course.isEmpty()) { m_course.clear(); emit courseChanged(); }
        m_controller->loadCourses(department);   // re-scope the course list
    }
    reloadTable();
}

void DatabaseViewModel::setCourse(const QString &course)
{
    if (m_course != course) { m_course = course; emit courseChanged(); }
    reloadTable();
}

void DatabaseViewModel::onSearchFinished(SearchOutcome outcome, const QList<StudentRecord> &records,
                                         const QString &message, const QString &searchTerm, quint64 requestId)
{
    Q_UNUSED(searchTerm)
    if (!acceptRequest(requestId)) return;
    setLoading(false);
    m_students.setRecords(records);
    if (outcome == SearchOutcome::InvalidResponse)
        setError(QStringLiteral("Invalid server response."));
    else if (outcome == SearchOutcome::NotSuccess)
        setError(message.isEmpty() ? QStringLiteral("No students found.") : message);
    else
        setError(QString());
}

void DatabaseViewModel::onSearchFailed(const QString & /*errorString*/, quint64 requestId)
{
    if (!acceptRequest(requestId)) return;
    setLoading(false);
    m_students.setRecords({});                 // never leave stale rows behind an error
    setError(QStringLiteral("Network error. Please try again."));
}

void DatabaseViewModel::onDepartmentsLoaded(const QStringList &departments)
{
    m_departments = departments; emit departmentsChanged();
}

void DatabaseViewModel::onCoursesLoaded(const QStringList &courses)
{
    m_courses = courses; emit coursesChanged();
}

bool DatabaseViewModel::acceptRequest(quint64 requestId)
{
    if (requestId < m_latestAppliedRequestId) return false;
    m_latestAppliedRequestId = requestId; return true;
}

void DatabaseViewModel::deleteSelected()
{
    const QStringList ids = m_students.selectedIds();
    if (ids.isEmpty())
        return;                       // nothing selected — guard
    m_deleteInFlight = true;
    m_controller->deleteStudents(ids, AdminSession::instance().key());
}

void DatabaseViewModel::onDeleteFinished(bool ok, int requestedCount, const QString &message)
{
    m_deleteInFlight = false;
    if (ok) {
        setAuthFailure(false);
        setStatusMessage(tr("Deleted %1 students").arg(requestedCount));
        reloadTable();                // re-fetch the current dept/course filter
        m_students.clearSelection();  // deleted rows also drop via setRecords' intersect
        return;
    }
    // Server rejection. Tell the 401 held-key failure apart from a generic
    // server error via the SAME predicate SettingsViewModel uses (§Error Taxonomy).
    if (SettingsViewModel::isAuthFailureMessage(message)) {
        setAuthFailure(true);
        setStatusMessage(tr("Admin authentication failed — re-enter via admin login."));
    } else {
        setAuthFailure(false);
        setStatusMessage(message.isEmpty() ? tr("Delete failed.") : message);
    }
}

void DatabaseViewModel::onDeleteFailed(const QString & /*errorString*/)
{
    m_deleteInFlight = false;
    setAuthFailure(false);
    setStatusMessage(tr("Delete failed — check your connection."));
}

// Temporary stub — real implementation lands in Task 5. Present so the moc
// metacall for the Q_INVOKABLE resolves and this task's target links.
bool DatabaseViewModel::exportCsv(const QUrl & /*fileUrl*/) { return false; }

void DatabaseViewModel::setLoading(bool v) { if (m_loading != v) { m_loading = v; emit loadingChanged(); } }
void DatabaseViewModel::setError(const QString &e) { if (m_errorText != e) { m_errorText = e; emit errorTextChanged(); } }
void DatabaseViewModel::setStatusMessage(const QString &m) { m_statusMessage = m; emit statusMessageChanged(); }
void DatabaseViewModel::setAuthFailure(bool v) { if (m_authFailure != v) { m_authFailure = v; emit authFailureChanged(); } }
