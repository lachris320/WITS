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
    // canEdit is a derived bool over selection size; only re-emit when it
    // actually flips (was: fired on every selectionChanged -> over-emit).
    connect(&m_students, &StudentsTableModel::selectionChanged, this, [this] {
        const bool now = canEdit();
        if (now != m_lastCanEdit) { m_lastCanEdit = now; emit canEditChanged(); }
    });

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
    applyServerRejection(message, tr("Delete failed."));
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

    m_editMode = EditMode::SingleEdit;
    m_courseTarget = CourseTarget::SingleEdit;
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
    m_courseTarget = CourseTarget::SingleEdit;
    m_editController->loadCourses(dept);           // re-scope the edit course list
}

void DatabaseViewModel::onEditCoursesLoaded(const QStringList &courses)
{
    if (m_courseTarget == CourseTarget::BulkEdit) {
        m_bulkCourses = courses; emit bulkCoursesChanged();
    } else {
        m_editCourses = courses; emit editCoursesChanged();
    }
}

void DatabaseViewModel::applyServerRejection(const QString &message,
                                             const QString &genericFallback)
{
    // Tell a 401 held-key failure apart from a generic server error via the
    // SAME predicate SettingsViewModel uses (§Error Taxonomy).
    if (SettingsViewModel::isAuthFailureMessage(message)) {
        setAuthFailure(true);
        setStatusMessage(tr("Admin authentication failed — re-enter via admin login."));
    } else {
        setAuthFailure(false);
        setStatusMessage(message.isEmpty() ? genericFallback : message);
    }
}

QList<StudentRecord> DatabaseViewModel::buildBulkUpdates(const QList<StudentRecord> &selected,
                                                         const BulkEditChanges &changes)
{
    QList<StudentRecord> out;
    out.reserve(selected.size());
    for (StudentRecord r : selected) {   // copy — carries name/schoolId/code/visits
        if (changes.changeDepartment) r.department = changes.department;
        if (changes.changeCourse)     r.course     = changes.course;
        if (changes.changeYearLevel)  r.yearLevel  = changes.yearLevel;
        if (changes.changeGender)     r.gender     = changes.gender;
        if (changes.changeStatus)     r.status     = changes.status;
        out.append(r);
    }
    return out;
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

void DatabaseViewModel::applyBulkEdit()
{
    if (m_bulkInFlight) return;                             // re-entry guard
    const QList<StudentRecord> sel = m_students.selectedRecords();
    if (sel.isEmpty() || !canApplyBulk()) return;           // nothing to do
    const QList<StudentRecord> updates = buildBulkUpdates(sel, currentChanges());
    m_bulkInFlight = true; emit bulkBusyChanged();
    m_controller->bulkUpdateStudents(updates, AdminSession::instance().key());
}

void DatabaseViewModel::onBulkUpdateFinished(const BulkUpdateResult &result)
{
    m_bulkInFlight = false; emit bulkBusyChanged();
    if (result.ok) {
        setAuthFailure(false);
        if (result.updatedCount >= 1) {
            setStatusMessage(result.updatedCount == 1
                ? tr("Updated 1 student")
                : tr("Updated %1 students").arg(result.updatedCount));
            reloadTable();                 // re-fetch the current dept/course filter
            m_students.clearSelection();
        } else {
            setStatusMessage(tr("No changes to save"));   // no-op is a success
        }
        // Close whichever dialog is open (single vs bulk); NoEdit -> single, so
        // the existing single-edit tests that never set BulkEdit still get editFinished.
        if (m_editMode == EditMode::BulkEdit) emit bulkEditFinished();
        else                                  emit editFinished();
        m_editMode = EditMode::NoEdit;        // consumed — reset for the next round
        return;
    }
    // Server rejection. Distinguish a 401 held-key failure from a generic error
    // via the SAME predicate delete uses (§Error Taxonomy). Keep the dialog open.
    applyServerRejection(result.message, tr("Update failed."));
}

void DatabaseViewModel::onBulkUpdateFailed(const QString & /*errorString*/)
{
    m_bulkInFlight = false; emit bulkBusyChanged();
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

BulkEditChanges DatabaseViewModel::currentChanges() const
{
    BulkEditChanges c;
    c.changeDepartment = m_changeDepartment; c.department = m_bulkDepartment;
    c.changeCourse     = m_changeCourse;     c.course     = m_bulkCourse;
    c.changeYearLevel  = m_changeYearLevel;  c.yearLevel  = m_bulkYearLevel;
    c.changeGender     = m_changeGender;     c.gender     = m_bulkGender;
    c.changeStatus     = m_changeStatus;     c.status     = m_bulkStatus;
    return c;
}

void DatabaseViewModel::emitBulkDerivedChanged()
{
    emit canApplyBulkChanged();
    emit bulkChangeSummaryChanged();
}

bool DatabaseViewModel::canApplyBulk() const
{
    const BulkEditChanges c = currentChanges();
    bool any = false;
    if (c.changeDepartment) {                  // dept + course move together
        if (c.department.isEmpty() || c.course.isEmpty()) return false;
        any = true;
    } else if (c.changeCourse) {
        return false;                          // course without dept is invalid
    }
    if (c.changeYearLevel) { if (c.yearLevel.isEmpty()) return false; any = true; }
    if (c.changeGender)    { if (c.gender.isEmpty())    return false; any = true; }
    if (c.changeStatus)    { if (c.status.isEmpty())    return false; any = true; }
    return any;
}

QStringList DatabaseViewModel::bulkChangeSummary() const
{
    const BulkEditChanges c = currentChanges();
    QStringList lines;
    if (c.changeDepartment) lines << tr("%1 → %2").arg(tr("Department"), c.department);
    if (c.changeCourse)     lines << tr("%1 → %2").arg(tr("Course"),     c.course);
    if (c.changeYearLevel)  lines << tr("%1 → %2").arg(tr("Year Level"), c.yearLevel);
    if (c.changeGender)     lines << tr("%1 → %2").arg(tr("Gender"),     c.gender);
    if (c.changeStatus)     lines << tr("%1 → %2").arg(tr("Status"),     c.status);
    return lines;
}

void DatabaseViewModel::setChangeDepartment(bool v)
{
    if (m_changeDepartment == v) return;
    m_changeDepartment = v; emit changeDepartmentChanged();
    // Coupling: Department and Course toggle together.
    if (v) {
        if (!m_changeCourse) { m_changeCourse = true; emit changeCourseChanged(); }
    } else {
        if (m_changeCourse) { m_changeCourse = false; emit changeCourseChanged(); }
        if (!m_bulkCourse.isEmpty()) { m_bulkCourse.clear(); emit bulkCourseChanged(); }
    }
    emitBulkDerivedChanged();
}

void DatabaseViewModel::setChangeCourse(bool v)
{
    // Course-change is 1:1 COUPLED to Department-change and is driven
    // exclusively by setChangeDepartment. A direct write is honored only when
    // it agrees with the current department-toggle; a disagreeing write (which
    // would desync the pair and resend a stale course on a dept change) is
    // rejected. Defensive: the dialog's Course toggle is disabled and never
    // calls this.
    if (v != m_changeDepartment)
        return;                       // reject desync (independent enable OR disable)
    if (m_changeCourse == v)
        return;                       // already consistent
    m_changeCourse = v;
    emit changeCourseChanged();
    emitBulkDerivedChanged();
}

void DatabaseViewModel::setChangeYearLevel(bool v)
{ if (m_changeYearLevel == v) return; m_changeYearLevel = v; emit changeYearLevelChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setChangeGender(bool v)
{ if (m_changeGender == v) return; m_changeGender = v; emit changeGenderChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setChangeStatus(bool v)
{ if (m_changeStatus == v) return; m_changeStatus = v; emit changeStatusChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkDepartment(const QString &v)
{
    if (m_bulkDepartment == v) return;
    m_bulkDepartment = v; emit bulkDepartmentChanged();
    if (!m_bulkCourse.isEmpty()) { m_bulkCourse.clear(); emit bulkCourseChanged(); }
    m_courseTarget = CourseTarget::BulkEdit;
    m_editController->loadCourses(v);   // dependent course list for the bulk dialog
    emitBulkDerivedChanged();
}

void DatabaseViewModel::beginBulkEditSelected()
{
    if (m_students.selectedCount() < 2) return;   // header button only branches here at >= 2
    // Clean reset so a reopened dialog never inherits a prior session's state.
    m_changeDepartment = m_changeCourse = m_changeYearLevel = false;
    m_changeGender = m_changeStatus = false;
    m_bulkDepartment.clear(); m_bulkCourse.clear(); m_bulkYearLevel.clear();
    m_bulkGender.clear(); m_bulkStatus.clear(); m_bulkCourses.clear();
    emit changeDepartmentChanged(); emit changeCourseChanged(); emit changeYearLevelChanged();
    emit changeGenderChanged(); emit changeStatusChanged();
    emit bulkDepartmentChanged(); emit bulkCourseChanged(); emit bulkYearLevelChanged();
    emit bulkGenderChanged(); emit bulkStatusChanged(); emit bulkCoursesChanged();
    emitBulkDerivedChanged();
    m_editMode = EditMode::BulkEdit;
    emit bulkEditReady();
}

void DatabaseViewModel::setBulkCourse(const QString &v)
{ if (m_bulkCourse == v) return; m_bulkCourse = v; emit bulkCourseChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkYearLevel(const QString &v)
{ if (m_bulkYearLevel == v) return; m_bulkYearLevel = v; emit bulkYearLevelChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkGender(const QString &v)
{ if (m_bulkGender == v) return; m_bulkGender = v; emit bulkGenderChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setBulkStatus(const QString &v)
{ if (m_bulkStatus == v) return; m_bulkStatus = v; emit bulkStatusChanged(); emitBulkDerivedChanged(); }

void DatabaseViewModel::setLoading(bool v) { if (m_loading != v) { m_loading = v; emit loadingChanged(); } }
void DatabaseViewModel::setError(const QString &e) { if (m_errorText != e) { m_errorText = e; emit errorTextChanged(); } }
void DatabaseViewModel::setStatusMessage(const QString &m) { m_statusMessage = m; emit statusMessageChanged(); }
void DatabaseViewModel::setAuthFailure(bool v) { if (m_authFailure != v) { m_authFailure = v; emit authFailureChanged(); } }
