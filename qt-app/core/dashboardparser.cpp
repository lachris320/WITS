#include "dashboardparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool DashboardParser::parse(const QByteArray &raw, DashboardSummary &out, QString &outError)
{
    out = DashboardSummary();
    outError.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        outError = QStringLiteral("Invalid server response.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        outError = QStringLiteral("Could not load dashboard data.");
        return false;
    }

    out.today    = obj.value(QStringLiteral("today")).toInt();
    out.week     = obj.value(QStringLiteral("week")).toInt();
    out.students = obj.value(QStringLiteral("students")).toInt();

    const QJsonArray hours = obj.value(QStringLiteral("hourly")).toArray();
    for (const QJsonValue &v : hours) {
        const QJsonObject h = v.toObject();
        out.hourly.append(HourBucket{ h.value(QStringLiteral("hour")).toInt(),
                                      h.value(QStringLiteral("count")).toInt() });
    }

    const QJsonArray depts = obj.value(QStringLiteral("departments")).toArray();
    for (const QJsonValue &v : depts) {
        const QJsonObject d = v.toObject();
        out.departments.append(DeptBucket{ d.value(QStringLiteral("name")).toString(),
                                           d.value(QStringLiteral("count")).toInt() });
    }
    return true;
}
