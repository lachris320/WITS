#ifndef DATABASEVIEWMODEL_H
#define DATABASEVIEWMODEL_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <qqml.h>                 // QML_ELEMENT — AdminScreen instantiates this type
#include "studentdata.h"
#include "StudentsTableModel.h"

class QNetworkAccessManager;
class StudentController;

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
public:
    explicit DatabaseViewModel(QObject *parent = nullptr);

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
};

#endif // DATABASEVIEWMODEL_H
