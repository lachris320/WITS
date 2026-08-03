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
public:
    explicit DatabaseViewModel(QObject *parent = nullptr);

    static constexpr int kTypeToConfirmThreshold = 10;

    StudentsTableModel *students() { return &m_students; }
    QStringList departments() const { return m_departments; }
    QStringList courses() const { return m_courses; }
    QString department() const { return m_department; }
    QString course() const { return m_course; }   // exposed so a test/screen can observe the dependent-clear
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }
    QString statusMessage() const { return m_statusMessage; }
    bool authFailure() const { return m_authFailure; }

    Q_INVOKABLE void refresh();                          // load departments + all students
    Q_INVOKABLE void setDepartment(const QString &department);
    Q_INVOKABLE void setCourse(const QString &course);
    Q_INVOKABLE void reloadTable();

    // The VM owns the small-vs-large decision; QML consumes only this boolean.
    Q_INVOKABLE bool requiresTypedConfirmation(int count) const
    { return count >= kTypeToConfirmThreshold; }
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE bool exportCsv(const QUrl &fileUrl);   // implemented in Task 5

    // Public slots (test seam — driven network-free, like SearchViewModel).
    void onSearchFinished(SearchOutcome outcome, const QList<StudentRecord> &records,
                          const QString &message, const QString &searchTerm, quint64 requestId);
    void onSearchFailed(const QString &errorString, quint64 requestId);
    void onDepartmentsLoaded(const QStringList &departments);
    void onCoursesLoaded(const QStringList &courses);
    void onDeleteFinished(bool ok, int requestedCount, const QString &message);
    void onDeleteFailed(const QString &errorString);

signals:
    void departmentsChanged();
    void coursesChanged();
    void departmentChanged();
    void courseChanged();
    void loadingChanged();
    void errorTextChanged();
    void statusMessageChanged();
    void authFailureChanged();

private:
    bool acceptRequest(quint64 requestId);
    void setLoading(bool v);
    void setError(const QString &e);
    void setStatusMessage(const QString &m);
    void setAuthFailure(bool v);

    QNetworkAccessManager *m_nam = nullptr;
    StudentController *m_controller = nullptr;
    StudentsTableModel m_students;
    QStringList m_departments, m_courses;
    QString m_department, m_course, m_errorText;
    QString m_statusMessage;
    bool m_loading = false;
    bool m_authFailure = false;
    bool m_deleteInFlight = false;
    quint64 m_latestAppliedRequestId = 0;
};

#endif // DATABASEVIEWMODEL_H
