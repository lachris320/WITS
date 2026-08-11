#ifndef DATABASEVIEWMODEL_H
#define DATABASEVIEWMODEL_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <qqml.h>                 // QML_ELEMENT — AdminScreen instantiates this type
#include "studentdata.h"
#include "StudentsTableModel.h"
#include "studentcontroller.h"

class QNetworkAccessManager;

// The operator's bulk-edit choices: a toggle + value per editable field.
// Passed to DatabaseViewModel::buildBulkUpdates (pure, unit-tested).
struct BulkEditChanges {
    bool changeDepartment = false; QString department;
    bool changeCourse     = false; QString course;
    bool changeYearLevel  = false; QString yearLevel;
    bool changeGender     = false; QString gender;
    bool changeStatus     = false; QString status;
};

// Database screen VM (spec §4.2, increment 4a.2a — read/filter/select only).
// Wraps StudentController (no new endpoint: an empty-search searchStudents with
// the dept/course filter loads the table). Mirrors SearchViewModel's wiring +
// requestId race guard. Mutations (register/edit/bulk/delete/dept-ops) are 4a.2b.
class DatabaseViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT                    // declarative registration, mirrors SearchViewModel.h
    Q_PROPERTY(StudentsTableModel *students READ students CONSTANT)
    Q_PROPERTY(QStringList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QStringList courses READ courses NOTIFY coursesChanged)
    Q_PROPERTY(QString department READ department NOTIFY departmentChanged)
    Q_PROPERTY(QString course READ course NOTIFY courseChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY canEditChanged)
    Q_PROPERTY(QString editSchoolId READ editSchoolId NOTIFY editSchoolIdChanged)
    Q_PROPERTY(QString editName READ editName WRITE setEditName NOTIFY editNameChanged)
    Q_PROPERTY(QString editYearLevel READ editYearLevel WRITE setEditYearLevel NOTIFY editYearLevelChanged)
    Q_PROPERTY(QString editGender READ editGender WRITE setEditGender NOTIFY editGenderChanged)
    Q_PROPERTY(QString editStatus READ editStatus WRITE setEditStatus NOTIFY editStatusChanged)
    Q_PROPERTY(QString editDepartment READ editDepartment NOTIFY editDepartmentChanged)
    Q_PROPERTY(QString editCourse READ editCourse WRITE setEditCourse NOTIFY editCourseChanged)
    Q_PROPERTY(QStringList editCourses READ editCourses NOTIFY editCoursesChanged)
    Q_PROPERTY(bool changeDepartment READ changeDepartment WRITE setChangeDepartment NOTIFY changeDepartmentChanged)
    Q_PROPERTY(bool changeCourse     READ changeCourse     WRITE setChangeCourse     NOTIFY changeCourseChanged)
    Q_PROPERTY(bool changeYearLevel  READ changeYearLevel  WRITE setChangeYearLevel  NOTIFY changeYearLevelChanged)
    Q_PROPERTY(bool changeGender     READ changeGender     WRITE setChangeGender     NOTIFY changeGenderChanged)
    Q_PROPERTY(bool changeStatus     READ changeStatus     WRITE setChangeStatus     NOTIFY changeStatusChanged)
    Q_PROPERTY(QString bulkDepartment READ bulkDepartment WRITE setBulkDepartment NOTIFY bulkDepartmentChanged)
    Q_PROPERTY(QString bulkCourse     READ bulkCourse     WRITE setBulkCourse     NOTIFY bulkCourseChanged)
    Q_PROPERTY(QString bulkYearLevel  READ bulkYearLevel  WRITE setBulkYearLevel  NOTIFY bulkYearLevelChanged)
    Q_PROPERTY(QString bulkGender     READ bulkGender     WRITE setBulkGender     NOTIFY bulkGenderChanged)
    Q_PROPERTY(QString bulkStatus     READ bulkStatus     WRITE setBulkStatus     NOTIFY bulkStatusChanged)
    Q_PROPERTY(QStringList bulkCourses READ bulkCourses NOTIFY bulkCoursesChanged)
    Q_PROPERTY(bool canApplyBulk READ canApplyBulk NOTIFY canApplyBulkChanged)
    Q_PROPERTY(QStringList bulkChangeSummary READ bulkChangeSummary NOTIFY bulkChangeSummaryChanged)
    Q_PROPERTY(bool bulkBusy READ bulkBusy NOTIFY bulkBusyChanged)
    Q_PROPERTY(QString regSchoolId READ regSchoolId WRITE setRegSchoolId NOTIFY regSchoolIdChanged)
    Q_PROPERTY(QString regName READ regName WRITE setRegName NOTIFY regNameChanged)
    Q_PROPERTY(QString regCode READ regCode WRITE setRegCode NOTIFY regCodeChanged)
    Q_PROPERTY(QString regYearLevel READ regYearLevel WRITE setRegYearLevel NOTIFY regYearLevelChanged)
    Q_PROPERTY(QString regGender READ regGender WRITE setRegGender NOTIFY regGenderChanged)
    Q_PROPERTY(QString regStatus READ regStatus WRITE setRegStatus NOTIFY regStatusChanged)
    Q_PROPERTY(QString regDepartment READ regDepartment NOTIFY regDepartmentChanged)
    Q_PROPERTY(QString regCourse READ regCourse WRITE setRegCourse NOTIFY regCourseChanged)
    Q_PROPERTY(QStringList regCourses READ regCourses NOTIFY regCoursesChanged)
    Q_PROPERTY(QString regPhotoName READ regPhotoName NOTIFY regPhotoNameChanged)
    Q_PROPERTY(bool canRegister READ canRegister NOTIFY canRegisterChanged)
    Q_PROPERTY(bool regBusy READ regBusy NOTIFY regBusyChanged)
    Q_PROPERTY(bool regDuplicate READ regDuplicate NOTIFY regDuplicateChanged)
    Q_PROPERTY(bool deptOpBusy READ deptOpBusy NOTIFY deptOpBusyChanged)
public:
    explicit DatabaseViewModel(QObject *parent = nullptr);

    enum class EditMode     { NoEdit, SingleEdit, BulkEdit };
    enum class CourseTarget { SingleEdit, BulkEdit, Register };

    static constexpr int kTypeToConfirmThreshold = 10;

    // Pure: copies each selected record, overriding ONLY the toggled fields;
    // name/schoolId/code/visits carry through untouched. A free static because
    // the VM news up its own NAM (no injection seam) — this is the only way to
    // unit-test the override/carry-through rules network-free.
    static QList<StudentRecord> buildBulkUpdates(const QList<StudentRecord> &selected,
                                                 const BulkEditChanges &changes);

    StudentsTableModel *students() { return &m_students; }
    QStringList departments() const { return m_departments; }
    QStringList courses() const { return m_courses; }
    QString department() const { return m_department; }
    QString course() const { return m_course; }   // exposed so a test/screen can observe the dependent-clear
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }
    QString statusMessage() const { return m_statusMessage; }
    bool authFailure() const { return m_authFailure; }
    bool canEdit() const { return m_students.selectedCount() >= 1; }
    QString editSchoolId() const { return m_editSchoolId; }
    QString editName() const { return m_editName; }
    QString editYearLevel() const { return m_editYearLevel; }
    QString editGender() const { return m_editGender; }
    QString editStatus() const { return m_editStatus; }
    QString editDepartment() const { return m_editDepartment; }
    QString editCourse() const { return m_editCourse; }
    QStringList editCourses() const { return m_editCourses; }
    bool changeDepartment() const { return m_changeDepartment; }
    bool changeCourse() const { return m_changeCourse; }
    bool changeYearLevel() const { return m_changeYearLevel; }
    bool changeGender() const { return m_changeGender; }
    bool changeStatus() const { return m_changeStatus; }
    QString bulkDepartment() const { return m_bulkDepartment; }
    QString bulkCourse() const { return m_bulkCourse; }
    QString bulkYearLevel() const { return m_bulkYearLevel; }
    QString bulkGender() const { return m_bulkGender; }
    QString bulkStatus() const { return m_bulkStatus; }
    QStringList bulkCourses() const { return m_bulkCourses; }
    bool canApplyBulk() const;
    QStringList bulkChangeSummary() const;
    bool bulkBusy() const { return m_bulkInFlight; }

    QString regSchoolId() const { return m_regSchoolId; }
    QString regName() const { return m_regName; }
    QString regCode() const { return m_regCode; }
    QString regYearLevel() const { return m_regYearLevel; }
    QString regGender() const { return m_regGender; }
    QString regStatus() const { return m_regStatus; }
    QString regDepartment() const { return m_regDepartment; }
    QString regCourse() const { return m_regCourse; }
    QStringList regCourses() const { return m_regCourses; }
    QString regPhotoName() const { return m_regPhotoName; }
    bool canRegister() const
    { return !m_regSchoolId.trimmed().isEmpty() && !m_regName.trimmed().isEmpty(); }
    bool regBusy() const { return m_regInFlight; }
    bool regDuplicate() const { return m_regDuplicate; }
    bool deptOpBusy() const { return m_deptOpInFlight; }

    Q_INVOKABLE void refresh();                          // load departments + all students
    Q_INVOKABLE void setDepartment(const QString &department);
    Q_INVOKABLE void setCourse(const QString &course);
    Q_INVOKABLE void reloadTable();

    // The VM owns the small-vs-large decision; QML consumes only this boolean.
    Q_INVOKABLE bool requiresTypedConfirmation(int count) const
    { return count >= kTypeToConfirmThreshold; }
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE bool exportCsv(const QUrl &fileUrl);   // implemented in Task 5

    Q_INVOKABLE void beginEdit(const QString &schoolId);
    Q_INVOKABLE void beginEditSelected();
    Q_INVOKABLE void setEditDepartment(const QString &dept);
    Q_INVOKABLE void setEditName(const QString &v);
    Q_INVOKABLE void setEditYearLevel(const QString &v);
    Q_INVOKABLE void setEditGender(const QString &v);
    Q_INVOKABLE void setEditStatus(const QString &v);
    Q_INVOKABLE void setEditCourse(const QString &v);
    Q_INVOKABLE void saveEdit();

    Q_INVOKABLE void setChangeDepartment(bool v);
    Q_INVOKABLE void setChangeCourse(bool v);
    Q_INVOKABLE void setChangeYearLevel(bool v);
    Q_INVOKABLE void setChangeGender(bool v);
    Q_INVOKABLE void setChangeStatus(bool v);
    Q_INVOKABLE void setBulkDepartment(const QString &v);
    Q_INVOKABLE void setBulkCourse(const QString &v);
    Q_INVOKABLE void setBulkYearLevel(const QString &v);
    Q_INVOKABLE void setBulkGender(const QString &v);
    Q_INVOKABLE void setBulkStatus(const QString &v);
    Q_INVOKABLE void beginBulkEditSelected();
    Q_INVOKABLE void applyBulkEdit();
    Q_INVOKABLE void deactivateDepartment();
    Q_INVOKABLE void deleteDepartment();

    Q_INVOKABLE void beginRegister();
    Q_INVOKABLE void setRegSchoolId(const QString &v);
    Q_INVOKABLE void setRegName(const QString &v);
    Q_INVOKABLE void setRegCode(const QString &v);
    Q_INVOKABLE void setRegYearLevel(const QString &v);
    Q_INVOKABLE void setRegGender(const QString &v);
    Q_INVOKABLE void setRegStatus(const QString &v);
    Q_INVOKABLE void setRegCourse(const QString &v);
    Q_INVOKABLE void setRegDepartment(const QString &dept);
    Q_INVOKABLE void setRegPhoto(const QUrl &fileUrl);
    Q_INVOKABLE void clearRegPhoto();
    Q_INVOKABLE void registerStudent();

    // Public slots (test seam — driven network-free, like SearchViewModel).
    void onSearchFinished(SearchOutcome outcome, const QList<StudentRecord> &records,
                          const QString &message, const QString &searchTerm, quint64 requestId);
    void onSearchFailed(const QString &errorString, quint64 requestId);
    void onDepartmentsLoaded(const QStringList &departments);
    void onCoursesLoaded(const QStringList &courses);
    void onDeleteFinished(bool ok, int requestedCount, const QString &message);
    void onDeleteFailed(const QString &errorString);
    void onEditCoursesLoaded(const QStringList &courses);
    void onBulkUpdateFinished(const BulkUpdateResult &result);
    void onBulkUpdateFailed(const QString &errorString);
    void onRegisterFinished(RegisterOutcome outcome, const QString &message);
    void onRegisterFailed(const QString &errorString);
    void onDepartmentOpFinished(StudentController::DeptOp op, bool ok, const QString &message);
    void onDepartmentOpFailed(StudentController::DeptOp op, const QString &errorString);

signals:
    void departmentsChanged();
    void coursesChanged();
    void departmentChanged();
    void courseChanged();
    void loadingChanged();
    void errorTextChanged();
    void statusMessageChanged();
    void authFailureChanged();
    void canEditChanged();
    void editSchoolIdChanged();
    void editNameChanged();
    void editYearLevelChanged();
    void editGenderChanged();
    void editStatusChanged();
    void editDepartmentChanged();
    void editCourseChanged();
    void editCoursesChanged();
    void editReady();
    void editFinished();
    void changeDepartmentChanged();
    void changeCourseChanged();
    void changeYearLevelChanged();
    void changeGenderChanged();
    void changeStatusChanged();
    void bulkDepartmentChanged();
    void bulkCourseChanged();
    void bulkYearLevelChanged();
    void bulkGenderChanged();
    void bulkStatusChanged();
    void bulkCoursesChanged();
    void canApplyBulkChanged();
    void bulkChangeSummaryChanged();
    void bulkEditReady();
    void bulkEditFinished();
    void bulkBusyChanged();
    void regSchoolIdChanged();
    void regNameChanged();
    void regCodeChanged();
    void regYearLevelChanged();
    void regGenderChanged();
    void regStatusChanged();
    void regDepartmentChanged();
    void regCourseChanged();
    void regCoursesChanged();
    void regPhotoNameChanged();
    void canRegisterChanged();
    void regBusyChanged();
    void regDuplicateChanged();
    void registerReady();
    void registerFinished();
    void deptOpBusyChanged();

private:
    bool acceptRequest(quint64 requestId);
    void setLoading(bool v);
    void setError(const QString &e);
    void setStatusMessage(const QString &m);
    void setAuthFailure(bool v);
    void applyServerRejection(const QString &message, const QString &genericFallback);
    // Packs the live toggle/value state into the POD for buildBulkUpdates,
    // canApplyBulk and bulkChangeSummary.
    BulkEditChanges currentChanges() const;
    // Every setter re-emits the two derived read-outs.
    void emitBulkDerivedChanged();

    QNetworkAccessManager *m_nam = nullptr;
    StudentController *m_controller = nullptr;
    StudentsTableModel m_students;
    QStringList m_departments, m_courses;
    QString m_department, m_course, m_errorText;
    QString m_statusMessage;
    bool m_loading = false;
    bool m_authFailure = false;
    bool m_deleteInFlight = false;
    bool m_lastCanEdit = false;
    quint64 m_latestAppliedRequestId = 0;

    QNetworkAccessManager *m_editNam = nullptr;
    StudentController *m_editController = nullptr;
    QString m_editSchoolId, m_editName, m_editYearLevel, m_editGender, m_editStatus;
    QString m_editDepartment, m_editCourse, m_editCode;
    int m_editVisits = 0;
    QStringList m_editCourses;

    bool m_changeDepartment = false, m_changeCourse = false, m_changeYearLevel = false,
         m_changeGender = false, m_changeStatus = false;
    QString m_bulkDepartment, m_bulkCourse, m_bulkYearLevel, m_bulkGender, m_bulkStatus;
    QStringList m_bulkCourses;

    EditMode     m_editMode     = EditMode::NoEdit;
    CourseTarget m_courseTarget = CourseTarget::SingleEdit;
    bool m_bulkInFlight = false;

    QString m_regSchoolId, m_regName, m_regCode, m_regYearLevel, m_regGender, m_regStatus;
    QString m_regDepartment, m_regCourse, m_regPhotoName, m_regPhotoPath;
    QStringList m_regCourses;
    bool m_regInFlight = false;
    bool m_regDuplicate = false;
    bool m_deptOpInFlight = false;
};

#endif // DATABASEVIEWMODEL_H
