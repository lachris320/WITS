#ifndef REPORTANALYTICS_H
#define REPORTANALYTICS_H

#include <QJsonArray>
#include <QList>
#include <QString>

// KPI band values ("How much?"). Presentation-agnostic — no formatting, no QML.
struct ReportKpis {
    int     totalVisits         = 0;   // Σ visits
    int     uniqueVisitors      = 0;   // count of distinct school_id groups
    double  avgVisitsPerVisitor = 0.0; // totalVisits / uniqueVisitors (0 when none)
    QString topDepartment;             // busiest department ("" when no data)
    int     topDepartmentVisits = 0;
    bool    hasData             = false; // false when zero rows -> empty-states
};

// One ranked row ("Who?"/"Which?").
struct RankingEntry {
    int     rank           = 0;   // 1-based
    QString label;                // student name / course / department
    QString sublabel;             // students: their course; else ""
    int     visits         = 0;
    double  percentOfTotal = 0.0; // visits / totalVisits * 100 (0 when total 0)
};

struct ReportAnalytics {
    ReportKpis          kpis;
    QList<RankingEntry> topStudents;
    QList<RankingEntry> topCourses;
    QList<RankingEntry> topDepartments;

    // The one entry point. `normalizedRows` MUST carry NUMERIC `visits`
    // (4b-ii's normalizeExportRows is the normalizer). A raw-string `visits`
    // is a caller error and counts as 0. Aggregation is BY KEY, summing visits:
    // students group on school_id, courses on course, departments on department.
    // Blank/whitespace name/course/department normalize to "(Unspecified)".
    static ReportAnalytics compute(const QJsonArray &normalizedRows, int topN = 10);
};

#endif // REPORTANALYTICS_H
