#ifndef STUDENTCONTROLLER_H
#define STUDENTCONTROLLER_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include "studentdata.h"

class QNetworkAccessManager;

class StudentController : public QObject
{
    Q_OBJECT
public:
    explicit StudentController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeFilter(const QString &comboText);
    static SearchOutcome parseSearchResponse(const QByteArray &raw,
                                             QList<StudentRecord> &outRecords,
                                             QString &outMessage,
                                             QString &outSearchTerm);
    static BulkUpdateResult parseBulkUpdateResponse(const QByteArray &raw);
    static QStringList parseDepartments(const QByteArray &raw);
    static QStringList parseCourses(const QByteArray &raw);
    // Returns true when status=="success"; on failure sets outMessage to the
    // server "message" field (empty on success).
    static bool parseDeleteResponse(const QByteArray &raw, QString &outMessage);

    // Async — result arrives via searchFinished / searchFailed, both of which
    // echo back the returned request id. StudentController fires an
    // independent QNetworkReply per call and does not serialize/abort a prior
    // in-flight search, so racing calls (e.g. rapid course-chip clicks) can
    // resolve out of request order; the id lets a consumer that cares (see
    // SearchViewModel) tell a superseded reply from the current one. Legacy
    // callers that ignore the id (adminwindow.cpp) are unaffected — Qt
    // permits connecting a signal to a slot with fewer trailing parameters.
    quint64 searchStudents(const QString &search,
                        const QString &department,
                        const QString &course);

    // Async — result arrives via bulkUpdateFinished / bulkUpdateFailed.
    void bulkUpdateStudents(const QList<StudentRecord> &updates);

    // Async — result arrives via deleteFinished / deleteFailed.
    void deleteStudents(const QStringList &schoolIds);

    // Async — result arrives via departmentsLoaded (empty on error/!success).
    void loadDepartments();

    // Async — result arrives via coursesLoaded (empty on error/!success).
    // Posts a JSON body {"department": <normalized department>} to
    // get_courses_by_department.php (NOT get_courses.php — that endpoint is
    // GET-only and errors on an empty department). An empty department (or
    // "All"/placeholder text, which normalizeFilter reduces to empty) is
    // treated server-side as "every course" — that's how the initial,
    // no-department-selected course-chip load is scoped.
    void loadCourses(const QString &department);

signals:
    void searchFinished(SearchOutcome outcome,
                        const QList<StudentRecord> &records,
                        const QString &message,
                        const QString &searchTerm,
                        quint64 requestId);
    void searchFailed(const QString &errorString, quint64 requestId);
    void bulkUpdateFinished(const BulkUpdateResult &result);
    void bulkUpdateFailed(const QString &errorString);
    // ok=true on success; requestedCount echoes how many ids were submitted
    // (the View needs it for the success overlay). message is the server
    // message on failure. deleteFailed fires only on a transport error.
    void deleteFinished(bool ok, int requestedCount, const QString &message);
    void deleteFailed(const QString &errorString);
    void departmentsLoaded(const QStringList &departments);
    void coursesLoaded(const QStringList &courses);

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
    quint64 m_searchRequestSeq = 0; // monotonic id source for searchStudents()
};

#endif // STUDENTCONTROLLER_H
