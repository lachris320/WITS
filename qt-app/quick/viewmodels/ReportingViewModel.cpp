#include "ReportingViewModel.h"

#include <QNetworkAccessManager>
#include "reportcontroller.h"
#include "reportdata.h"

ReportingViewModel::ReportingViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new ReportController(m_nam, this))
{
    // Signal wiring added in Task 5/6.
}

QJsonObject ReportingViewModel::buildFilters(const QString &department, const QString &course,
                                             int durationType,
                                             const QDate &day,
                                             int month, int monthYear,
                                             const QString &semester, int semYear,
                                             const QDate &customStart, const QDate &customEnd)
{
    QJsonObject f;
    f["department"] = department;
    f["course"] = course;
    switch (durationType) {
    case 0: {   // Day
        f["durationType"] = "day";
        const DateRange r = ReportController::computeDateRange(0, day, 0, 0, QString(), 0, QDate(), QDate());
        f["start"] = r.start;
        f["end"] = r.end;
        break;
    }
    case 1: {   // Month
        f["durationType"] = "month";
        const DateRange r = ReportController::computeDateRange(1, QDate(), month, monthYear, QString(), 0, QDate(), QDate());
        f["start"] = r.start;
        f["end"] = r.end;
        break;
    }
    case 2: {   // Semester — server-side ranging; send components, NOT a client range
        f["durationType"] = "semester";
        f["year"] = semYear;
        f["semester"] = semester;
        break;
    }
    case 3: {   // Custom
        f["durationType"] = "custom";
        const DateRange r = ReportController::computeDateRange(3, QDate(), 0, 0, QString(), 0, customStart, customEnd);
        f["start"] = r.start;
        f["end"] = r.end;
        break;
    }
    default:
        break;
    }
    return f;
}

// --- Stubs filled by later tasks (present so the class links now) ---
QList<BarsModel::Bar> ReportingViewModel::aggregateVisitsByCourse(const QJsonArray &) { return {}; }
ReportingViewModel::Tiles ReportingViewModel::deriveTiles(const QJsonArray &) { return {}; }
bool ReportingViewModel::canGenerate() const { return false; }
void ReportingViewModel::loadDepartments() {}
void ReportingViewModel::setDepartment(const QString &) {}
void ReportingViewModel::setCourse(const QString &) {}
void ReportingViewModel::setDurationType(int) {}
void ReportingViewModel::setDay(const QString &) {}
void ReportingViewModel::setMonth(int) {}
void ReportingViewModel::setMonthYear(int) {}
void ReportingViewModel::setSemester(const QString &) {}
void ReportingViewModel::setSemYear(int) {}
void ReportingViewModel::setCustomStart(const QString &) {}
void ReportingViewModel::setCustomEnd(const QString &) {}
void ReportingViewModel::generateReport() {}
void ReportingViewModel::retry() {}
void ReportingViewModel::onDepartmentsLoaded(const QStringList &) {}
void ReportingViewModel::onYearsLoaded(const QStringList &) {}
void ReportingViewModel::onCoursesLoaded(const QStringList &) {}
void ReportingViewModel::onReportDataReady(const QJsonArray &) {}
void ReportingViewModel::onReportError(const QString &, bool) {}
void ReportingViewModel::onLoadError(const QString &, const QString &, bool) {}
void ReportingViewModel::setLoading(bool) {}
void ReportingViewModel::setError(const QString &) {}
void ReportingViewModel::applyResult(const QJsonArray &) {}
