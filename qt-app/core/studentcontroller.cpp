#include "studentcontroller.h"
#include "apiconfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

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
        rec.visits     = s["visits"].toInt();
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

// Pure port of the reply body of the legacy deleteStudents (master
// adminwindow.cpp:3309-3324): success => true; otherwise the server message.
bool StudentController::parseDeleteResponse(const QByteArray &raw, QString &outMessage)
{
    outMessage.clear();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj["status"].toString() == "success")
        return true;
    outMessage = obj["message"].toString();
    return false;
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

// --- Network methods ---

quint64 StudentController::searchStudents(const QString &search,
                                       const QString &department,
                                       const QString &course)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("search_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonObject filters;
    filters["search"]     = search;
    filters["department"] = normalizeFilter(department);   // reproduces adminwindow.cpp:3171
    filters["course"]     = normalizeFilter(course);       // reproduces adminwindow.cpp:3172

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());

    const quint64 requestId = ++m_searchRequestSeq;

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        if (reply->error() != QNetworkReply::NoError) {
            const QString errorString = reply->errorString();
            reply->deleteLater();
            emit searchFailed(errorString, requestId);      // View gates overlay row 11 on flag
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QList<StudentRecord> records;
        QString message, searchTerm;
        const SearchOutcome outcome =
            parseSearchResponse(resp, records, message, searchTerm);
        emit searchFinished(outcome, records, message, searchTerm, requestId);
    });

    return requestId;
}

void StudentController::bulkUpdateStudents(const QList<StudentRecord> &updates)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("bulk_update_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonArray studentsArray;
    for (const StudentRecord &rec : updates) {
        QJsonObject student;
        student["school_id"]  = rec.schoolId;
        student["code"]       = rec.code;
        student["name"]       = rec.name;
        student["department"] = rec.department;
        student["course"]     = rec.course;
        student["year_level"] = rec.yearLevel;
        student["gender"]     = rec.gender;
        student["status"]     = rec.status;
        studentsArray.append(student);
    }

    QJsonObject data;
    data["students"] = studentsArray;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(data).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit bulkUpdateFailed(reply->errorString());   // View: overlay row 4 after UI reset
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        emit bulkUpdateFinished(parseBulkUpdateResponse(resp));
    });
}

void StudentController::deleteStudents(const QStringList &schoolIds)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("delete_students.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonArray ids;
    for (const QString &id : schoolIds)
        ids.append(id);
    QJsonObject data;
    data["school_ids"] = ids;

    const int requestedCount = schoolIds.size();
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(data).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestedCount]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit deleteFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QString message;
        const bool ok = parseDeleteResponse(resp, message);
        emit deleteFinished(ok, requestedCount, message);
    });
}

void StudentController::loadDepartments()
{
    QNetworkReply *reply =
        m_nam->get(QNetworkRequest(ApiConfig::endpoint(QStringLiteral("get_departments.php"))));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // Legacy shows no dialog on error or !success — an empty list plus the
        // View's always-prepended placeholder is the identical visible result.
        const QStringList departments =
            (reply->error() == QNetworkReply::NoError)
                ? parseDepartments(reply->readAll())
                : QStringList();
        reply->deleteLater();
        emit departmentsLoaded(departments);
    });
}

void StudentController::loadCourses(const QString &department)
{
    // get_courses_by_department.php reads a JSON request body
    // (json_decode(file_get_contents("php://input")), key "department") —
    // NOT a query string. A GET or form-POST falls through its own
    // empty/absent-department branch and silently returns every course
    // regardless of what was asked for, so this must be a JSON POST.
    QNetworkRequest request(
        ApiConfig::endpoint(QStringLiteral("get_courses_by_department.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonObject payload;
    payload["department"] = normalizeFilter(department);   // placeholder/empty -> "" -> "all courses"

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QStringList courses =
            (reply->error() == QNetworkReply::NoError)
                ? parseCourses(reply->readAll())
                : QStringList();
        reply->deleteLater();
        emit coursesLoaded(courses);
    });
}
