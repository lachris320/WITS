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

    // Async — result arrives via searchFinished / searchFailed.
    void searchStudents(const QString &search,
                        const QString &department,
                        const QString &course);

    // Async — result arrives via bulkUpdateFinished / bulkUpdateFailed.
    void bulkUpdateStudents(const QList<StudentRecord> &updates);

    // Async — result arrives via deleteFinished / deleteFailed.
    void deleteStudents(const QStringList &schoolIds);

    // Async — result arrives via departmentsLoaded (empty on error/!success).
    void loadDepartments();

    // Async — result arrives via coursesLoaded (empty on error/!success).
    void loadCourses(const QString &department);

signals:
    void searchFinished(SearchOutcome outcome,
                        const QList<StudentRecord> &records,
                        const QString &message,
                        const QString &searchTerm);
    void searchFailed(const QString &errorString);
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
};

#endif // STUDENTCONTROLLER_H
