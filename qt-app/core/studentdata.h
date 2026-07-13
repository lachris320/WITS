#ifndef STUDENTDATA_H
#define STUDENTDATA_H

#include <QString>
#include <QStringList>

// One search-result row. Field names track the JSON keys the backend
// returns/consumes: code, school_id, name, course, department, year_level,
// gender, status, visits (search direction only, not serialized back). A
// single type serves both the search-result direction (fill the table) and
// the bulk-update direction (serialize back).
struct StudentRecord
{
    QString code;
    QString schoolId;
    QString name;
    QString course;
    QString department;
    QString yearLevel;
    QString gender;
    QString status;
    int     visits = 0;   // lifetime library visit count (search read-only, §5.3)
};

// Decoded bulk_update_students.php response.
struct BulkUpdateResult
{
    bool        ok = false;
    int         updatedCount = 0;
    QStringList errors;
    QString     message;
};

// The five distinct branches of performStudentSearch's reply handler, so the
// View can show the exact original overlay for each.
enum class SearchOutcome
{
    Results,          // status success, students non-empty
    Empty,            // status success, students empty
    NotSuccess,       // obj["status"] != "success"
    InvalidResponse,  // body is not a JSON object
    NetworkError      // reply->error() != NoError (routed via searchFailed, not searchFinished)
};

#endif // STUDENTDATA_H
