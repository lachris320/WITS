#include "reportanalytics.h"

#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace {
QString orUnspecified(const QString &s) {
    const QString t = s.trimmed();
    return t.isEmpty() ? QStringLiteral("(Unspecified)") : t;
}
// A pre-ranking group: a display label (+ optional sublabel) and summed visits.
struct Group { QString label; QString sublabel; int visits = 0; };

// Rank a group list into `out`: descending by visits, ties broken alphabetically
// by label (deterministic), keep the top `topN`, assign 1-based rank + percent.
void rankInto(QList<RankingEntry> &out, QList<Group> groups, int totalVisits, int topN) {
    std::stable_sort(groups.begin(), groups.end(), [](const Group &a, const Group &b) {
        if (a.visits != b.visits) return a.visits > b.visits;
        return a.label < b.label;
    });
    const int n = qMin(topN, int(groups.size()));
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        RankingEntry e;
        e.rank = i + 1;
        e.label = groups[i].label;
        e.sublabel = groups[i].sublabel;
        e.visits = groups[i].visits;
        e.percentOfTotal = totalVisits > 0 ? (100.0 * groups[i].visits / totalVisits) : 0.0;
        out.append(e);
    }
}
} // namespace

ReportAnalytics ReportAnalytics::compute(const QJsonArray &rows, int topN) {
    ReportAnalytics a;
    if (rows.isEmpty())
        return a;                 // hasData=false, all lists empty
    a.kpis.hasData = true;

    QMap<QString, Group> students;      // key: school_id
    QMap<QString, int>   courses;       // key: course label
    QMap<QString, int>   departments;   // key: department label
    QSet<QString>        ids;
    int total = 0;

    for (const QJsonValue &v : rows) {
        const QJsonObject o = v.toObject();
        const int visits = o.value("visits").toInt();   // numeric per contract; string -> 0
        total += visits;

        const QString id = o.value("school_id").toString();
        ids.insert(id);
        Group &g = students[id];
        if (g.label.isEmpty()) {
            g.label = orUnspecified(o.value("name").toString());
            g.sublabel = orUnspecified(o.value("course").toString());
        }
        g.visits += visits;

        courses[orUnspecified(o.value("course").toString())] += visits;
        departments[orUnspecified(o.value("department").toString())] += visits;
    }

    a.kpis.totalVisits = total;
    a.kpis.uniqueVisitors = ids.size();
    a.kpis.avgVisitsPerVisitor = ids.isEmpty() ? 0.0 : double(total) / ids.size();

    // Rankings (Task 2 asserts these in detail; computed here so the KPI
    // top-department can reuse the tie-broken department ranking).
    QList<Group> stu = students.values();
    QList<Group> crs, dep;
    for (auto it = courses.cbegin(); it != courses.cend(); ++it)
        crs.append({ it.key(), QString(), it.value() });
    for (auto it = departments.cbegin(); it != departments.cend(); ++it)
        dep.append({ it.key(), QString(), it.value() });

    rankInto(a.topStudents, stu, total, topN);
    rankInto(a.topCourses, crs, total, topN);
    rankInto(a.topDepartments, dep, total, topN);

    if (!a.topDepartments.isEmpty()) {
        a.kpis.topDepartment = a.topDepartments.first().label;
        a.kpis.topDepartmentVisits = a.topDepartments.first().visits;
    }
    return a;
}
