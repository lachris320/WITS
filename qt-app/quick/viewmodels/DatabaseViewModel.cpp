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

    m_editNam = new QNetworkAccessManager(this);
    m_editController = new StudentController(m_editNam, this);
    connect(m_editController, &StudentController::coursesLoaded,
            this, &DatabaseViewModel::onEditCoursesLoaded);
    // canEdit tracks selection size — re-emit whenever the model's selection changes.
    connect(&m_students, &StudentsTableModel::selectionChanged,
            this, &DatabaseViewModel::canEditChanged);

    connect(m_controller, &StudentController::bulkUpdateFinished,
            this, &DatabaseViewModel::onBulkUpdateFinished);
    connect(m_controller, &StudentController::bulkUpdateFailed,
            this, &DatabaseViewModel::onBulkUpdateFailed);
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

void DatabaseViewModel::beginEdit(const QString &schoolId)
{
    const QList<StudentRecord> recs = m_students.allRecords();
    int idx = -1;
    for (int i = 0; i < recs.size(); ++i) {
        if (recs.at(i).schoolId == schoolId) { idx = i; break; }
    }
    if (idx < 0)
        return;   // not found — no-op; do not touch edit state or open the dialog

    const StudentRecord &r = recs.at(idx);
    m_editSchoolId = r.schoolId;   emit editSchoolIdChanged();
    m_editCode     = r.code;       // carried unchanged into saveEdit (not editable)
    m_editVisits   = r.visits;     // carried unchanged into saveEdit (not editable)
    m_editName     = r.name;       emit editNameChanged();
    m_editYearLevel= r.yearLevel;  emit editYearLevelChanged();
    m_editGender   = r.gender;     emit editGenderChanged();
    m_editStatus   = r.status;     emit editStatusChanged();
    m_editDepartment = r.department; emit editDepartmentChanged();
    m_editCourse   = r.course;     emit editCourseChanged();

    m_editController->loadCourses(r.department);   // independent of the filter's course list
    emit editReady();
}

void DatabaseViewModel::beginEditSelected()
{
    if (m_students.selectedCount() != 1)
        return;                                    // header button only enabled at 1
    beginEdit(m_students.selectedIds().first());
}

void DatabaseViewModel::setEditDepartment(const QString &dept)
{
    if (m_editDepartment == dept)
        return;   // no actual change — do NOT clear the course or reload (mirrors
                  // the filter's setDepartment at DatabaseViewModel.cpp:38-51, and
                  // is the second line of defense behind the dialog's prefill guard:
                  // re-selecting the same department must never blank the course).
    m_editDepartment = dept; emit editDepartmentChanged();
    if (!m_editCourse.isEmpty()) { m_editCourse.clear(); emit editCourseChanged(); }
    m_editController->loadCourses(dept);           // re-scope the edit course list
}

void DatabaseViewModel::onEditCoursesLoaded(const QStringList &courses)
{
    m_editCourses = courses; emit editCoursesChanged();
}

void DatabaseViewModel::setEditName(const QString &v)
{ if (m_editName != v) { m_editName = v; emit editNameChanged(); } }

void DatabaseViewModel::setEditYearLevel(const QString &v)
{ if (m_editYearLevel != v) { m_editYearLevel = v; emit editYearLevelChanged(); } }

void DatabaseViewModel::setEditGender(const QString &v)
{ if (m_editGender != v) { m_editGender = v; emit editGenderChanged(); } }

void DatabaseViewModel::setEditStatus(const QString &v)
{ if (m_editStatus != v) { m_editStatus = v; emit editStatusChanged(); } }

void DatabaseViewModel::setEditCourse(const QString &v)
{ if (m_editCourse != v) { m_editCourse = v; emit editCourseChanged(); } }

void DatabaseViewModel::saveEdit()
{
    StudentRecord rec;
    rec.schoolId   = m_editSchoolId;   // immutable identity (WHERE key)
    rec.code       = m_editCode;       // carried unchanged
    rec.visits     = m_editVisits;     // carried unchanged
    rec.name       = m_editName;
    rec.department = m_editDepartment;
    rec.course     = m_editCourse;
    rec.yearLevel  = m_editYearLevel;
    rec.gender     = m_editGender;
    rec.status     = m_editStatus;
    // Primary controller (search/delete/bulkUpdate) — NOT the edit course loader.
    m_controller->bulkUpdateStudents({rec}, AdminSession::instance().key());
}

void DatabaseViewModel::onBulkUpdateFinished(const BulkUpdateResult &result)
{
    if (result.ok && result.updatedCount >= 1) {
        setAuthFailure(false);
        setStatusMessage(tr("Student updated"));
        reloadTable();                 // re-fetch the current dept/course filter
        emit editFinished();
        return;
    }
    if (result.ok) {                   // updatedCount == 0: no-op edit is a success
        setAuthFailure(false);
        setStatusMessage(tr("No changes to save"));
        emit editFinished();
        return;
    }
    // Server rejection. Distinguish a 401 held-key failure from a generic error
    // via the SAME predicate delete uses (§Error Taxonomy). Keep the dialog open.
    if (SettingsViewModel::isAuthFailureMessage(result.message)) {
        setAuthFailure(true);
        setStatusMessage(tr("Admin authentication failed — re-enter via admin login."));
    } else {
        setAuthFailure(false);
        setStatusMessage(result.message.isEmpty() ? tr("Update failed.") : result.message);
    }
}

void DatabaseViewModel::onBulkUpdateFailed(const QString & /*errorString*/)
{
    setAuthFailure(false);
    setStatusMessage(tr("Update failed — check your connection."));
}

bool DatabaseViewModel::exportCsv(const QUrl &fileUrl)
{
    const QList<StudentRecord> rows =
        m_students.anySelected() ? m_students.selectedRecords() : m_students.allRecords();
    const QByteArray bytes = StudentController::toCsv(rows);

    const QString path = fileUrl.toLocalFile();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatusMessage(tr("Export failed — could not open the file for writing."));
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        setStatusMessage(tr("Export failed — could not write the file."));
        return false;
    }
    setStatusMessage(tr("Exported %1 rows to %2")
                         .arg(rows.size()).arg(QFileInfo(path).fileName()));
    return true;
}

void DatabaseViewModel::setLoading(bool v) { if (m_loading != v) { m_loading = v; emit loadingChanged(); } }
void DatabaseViewModel::setError(const QString &e) { if (m_errorText != e) { m_errorText = e; emit errorTextChanged(); } }
void DatabaseViewModel::setStatusMessage(const QString &m) { m_statusMessage = m; emit statusMessageChanged(); }
void DatabaseViewModel::setAuthFailure(bool v) { if (m_authFailure != v) { m_authFailure = v; emit authFailureChanged(); } }
