#include "visitlogparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool VisitLogParser::parse(const QByteArray &raw, QList<StudentVisitRecord> &out,
                           int &outCount, QString &outError)
{
    out.clear();
    outCount = 0;
    outError.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        outError = QStringLiteral("Invalid server response.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        outError = QStringLiteral("Could not load visit logs.");
        return false;
    }

    const QJsonArray visits = obj.value(QStringLiteral("visits")).toArray();
    for (const QJsonValue &v : visits) {
        const QJsonObject o = v.toObject();
        StudentVisitRecord rec;
        rec.date       = o.value(QStringLiteral("date")).toString();
        rec.name       = o.value(QStringLiteral("name")).toString();
        rec.course     = o.value(QStringLiteral("course")).toString();
        rec.department = o.value(QStringLiteral("department")).toString();
        rec.timeIn     = o.value(QStringLiteral("time_in")).toString();
        out.append(rec);
    }
    // Trust the array length for the count; fall back to the "count" field only
    // if the array is absent but a count was supplied.
    outCount = obj.contains(QStringLiteral("visits"))
                   ? out.size()
                   : obj.value(QStringLiteral("count")).toInt();
    return true;
}
