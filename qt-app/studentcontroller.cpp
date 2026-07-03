#include "studentcontroller.h"
#include "apiconfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

StudentController::StudentController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

// Reproduces the placeholder->"" normalization at adminwindow.cpp:3171-3172.
// The union of both legacy predicate lists; no real value equals a placeholder,
// so applying this to department reproduces 3171 and to course reproduces 3172.
QString StudentController::normalizeFilter(const QString &comboText)
{
    if (comboText == "All" || comboText == "All Departments"
        || comboText == "Select Department" || comboText == "All Courses"
        || comboText == "Select Course")
        return QString();
    return comboText;
}

// Pure port of the reply body of performStudentSearch (adminwindow.cpp:3194-3242),
// minus the overlay/table side effects (which stay View-side).
SearchOutcome StudentController::parseSearchResponse(const QByteArray &raw,
                                                     QList<StudentRecord> &outRecords,
                                                     QString &outMessage,
                                                     QString &outSearchTerm)
{
    outRecords.clear();
    outMessage.clear();
    outSearchTerm.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return SearchOutcome::InvalidResponse;         // line 3195

    const QJsonObject obj = doc.object();

    if (obj["status"].toString() != "success") {       // line 3206
        outMessage = obj.contains("message") ? obj["message"].toString()
                                             : QStringLiteral("No students found.");  // line 3207
        return SearchOutcome::NotSuccess;
    }

    const QJsonArray students = obj["students"].toArray();
    if (students.isEmpty())                            // line 3224
        return SearchOutcome::Empty;

    for (const QJsonValue &val : students) {
        const QJsonObject s = val.toObject();
        StudentRecord rec;
        rec.code       = s["code"].toString();
        rec.schoolId   = s["school_id"].toString();
        rec.name       = s["name"].toString();
        rec.course     = s["course"].toString();
        rec.department = s["department"].toString();
        rec.yearLevel  = s["year_level"].toString();
        rec.gender     = s["gender"].toString();
        rec.status     = s["status"].toString();
        outRecords.append(rec);
    }
    outSearchTerm = obj["searchTerm"].toString();      // line 3242
    return SearchOutcome::Results;
}

// Pure port of the reply body of bulkUpdateStudents (adminwindow.cpp:3017-3046).
BulkUpdateResult StudentController::parseBulkUpdateResponse(const QByteArray &raw)
{
    BulkUpdateResult result;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();

    if (obj["status"].toString() == "success") {
        result.ok = true;
        result.updatedCount = obj["updated"].toInt();          // line 3021
        if (obj.contains("errors") && !obj["errors"].toArray().isEmpty()) {  // line 3030
            const QJsonArray errs = obj["errors"].toArray();
            for (const QJsonValue &e : errs)
                result.errors << e.toString();
        }
    } else {
        result.message = obj["message"].toString();            // line 3045
    }
    return result;
}

// Pure port of the departments parse in populateFilters (adminwindow.cpp:3375-3382).
// Returns only the parsed entries (View prepends "Select Department"); skips
// "all" case-insensitively; empty on !success.
QStringList StudentController::parseDepartments(const QByteArray &raw)
{
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["departments"].toArray();
        for (const QJsonValue &val : arr) {
            const QString dept = val.toString();
            if (dept.toLower() != "all")               // line 3379
                out << dept;
        }
    }
    return out;
}

// Pure port of the courses parse in onDepartmentFilterChanged (adminwindow.cpp:3497-3503).
// Returns only the parsed entries (View prepends "Select Course"); empty on !success.
QStringList StudentController::parseCourses(const QByteArray &raw)
{
    QStringList out;
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success") {
        const QJsonArray arr = obj["courses"].toArray();
        for (const QJsonValue &val : arr)
            out << val.toString();
    }
    return out;
}

// --- Network methods: stubbed this task, implemented in Task 2 ---

void StudentController::searchStudents(const QString &, const QString &, const QString &)
{
    // Implemented in Task 2.
}

void StudentController::bulkUpdateStudents(const QList<StudentRecord> &)
{
    // Implemented in Task 2.
}

void StudentController::loadDepartments()
{
    // Implemented in Task 2.
}

void StudentController::loadCourses(const QString &)
{
    // Implemented in Task 2.
}
