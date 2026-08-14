#include "ReportingViewModel.h"

#include <QMap>
#include <QNetworkAccessManager>
#include <algorithm>
#include "reportcontroller.h"
#include "reportdata.h"

ReportingViewModel::ReportingViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new ReportController(m_nam, this))
{
    connect(m_controller, &ReportController::departmentsLoaded,
            this, &ReportingViewModel::onDepartmentsLoaded);
    connect(m_controller, &ReportController::yearsLoaded,
            this, &ReportingViewModel::onYearsLoaded);
    connect(m_controller, &ReportController::coursesLoaded,
            this, &ReportingViewModel::onCoursesLoaded);
    connect(m_controller, &ReportController::reportDataReady,
            this, &ReportingViewModel::onReportDataReady);
    connect(m_controller, &ReportController::reportError,
            this, &ReportingViewModel::onReportError);
    connect(m_controller, &ReportController::loadError,
            this, &ReportingViewModel::onLoadError);
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

QList<BarsModel::Bar> ReportingViewModel::aggregateVisitsByCourse(const QJsonArray &data)
{
    QMap<QString, int> byCourse;   // sorted by key; we re-sort by value below
    for (const QJsonValue &v : data) {
        const QJsonObject o = v.toObject();
        const QString course = o.value("course").toString();
        byCourse[course] += reportVisits(o);
    }
    QList<BarsModel::Bar> bars;
    bars.reserve(byCourse.size());
    for (auto it = byCourse.cbegin(); it != byCourse.cend(); ++it)
        bars.append({ it.key(), double(it.value()) });
    // Rank descending by value; stable so equal totals keep name (key) order.
    std::stable_sort(bars.begin(), bars.end(),
                     [](const BarsModel::Bar &a, const BarsModel::Bar &b) { return a.value > b.value; });
    return bars;
}

ReportingViewModel::Tiles ReportingViewModel::deriveTiles(const QJsonArray &data)
{
    Tiles t;
    t.studentsShown = data.size();
    t.topCourse = QStringLiteral("—");
    if (data.isEmpty())
        return t;
    const QList<BarsModel::Bar> bars = aggregateVisitsByCourse(data);
    for (const BarsModel::Bar &b : bars)
        t.totalVisits += int(b.value);
    if (!bars.isEmpty())
        t.topCourse = bars.first().label;   // already ranked descending
    return t;
}

bool ReportingViewModel::canGenerate() const
{
    if (m_department.isEmpty() || m_loading)
        return false;
    switch (m_durationType) {
    case 0: return parseDate(m_day).isValid();
    case 1: return m_month >= 1 && m_month <= 12 && m_monthYear > 0;
    case 2: return !m_semester.isEmpty() && m_semYear > 0;
    case 3: {
        const QDate s = parseDate(m_customStart), e = parseDate(m_customEnd);
        return s.isValid() && e.isValid() && s <= e;
    }
    default: return false;
    }
}

// --- Stubs filled by later tasks (present so the class links now) ---
void ReportingViewModel::loadDepartments()
{
    m_controller->loadDepartments();
    m_controller->loadYears();
}
void ReportingViewModel::setDepartment(const QString &department)
{
    if (m_department == department) return;
    m_department = department;
    m_course.clear();                 // dependent-clear (finalized in Task 5)
    emit departmentChanged();
    emit courseChanged();
    emit canGenerateChanged();
    if (!department.isEmpty())
        m_controller->loadCourses(department);
}
void ReportingViewModel::setCourse(const QString &course)
{
    if (m_course == course) return;
    m_course = course;
    emit courseChanged();
    emit canGenerateChanged();
}
void ReportingViewModel::setDurationType(int t)
{
    if (m_durationType == t) return;
    m_durationType = t;
    emit durationTypeChanged();
    emit canGenerateChanged();
}
void ReportingViewModel::setDay(const QString &v)
{
    if (m_day == v) return;
    m_day = v; emit dayChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setMonth(int v)
{
    if (m_month == v) return;
    m_month = v; emit monthChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setMonthYear(int v)
{
    if (m_monthYear == v) return;
    m_monthYear = v; emit monthYearChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setSemester(const QString &v)
{
    if (m_semester == v) return;
    m_semester = v; emit semesterChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setSemYear(int v)
{
    if (m_semYear == v) return;
    m_semYear = v; emit semYearChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setCustomStart(const QString &v)
{
    if (m_customStart == v) return;
    m_customStart = v; emit customStartChanged(); emit canGenerateChanged();
}
void ReportingViewModel::setCustomEnd(const QString &v)
{
    if (m_customEnd == v) return;
    m_customEnd = v; emit customEndChanged(); emit canGenerateChanged();
}
void ReportingViewModel::generateReport()
{
    if (!canGenerate())          // includes the !loading gate → single-in-flight
        return;
    setError(QString());
    setLoading(true);
    const QJsonObject filters = buildFilters(
        m_department, m_course, m_durationType,
        parseDate(m_day), m_month, m_monthYear,
        m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd));
    m_controller->fetchReportRows(filters);
}

void ReportingViewModel::retry()
{
    generateReport();            // re-runs with the current filter state
}
void ReportingViewModel::onDepartmentsLoaded(const QStringList &departments)
{
    m_departments = departments;
    emit departmentsChanged();
}
void ReportingViewModel::onYearsLoaded(const QStringList &years)
{
    m_years = years;
    emit yearsChanged();
}
void ReportingViewModel::onCoursesLoaded(const QStringList &courses)
{
    m_courses = courses;
    emit coursesChanged();
}
void ReportingViewModel::onReportDataReady(const QJsonArray &data)
{
    setLoading(false);
    applyResult(data);
}

void ReportingViewModel::onReportError(const QString &message, bool /*critical*/)
{
    setLoading(false);
    setError(message.isEmpty() ? QStringLiteral("Report failed. Please try again.") : message);
}

void ReportingViewModel::onLoadError(const QString &/*title*/, const QString &message, bool /*critical*/)
{
    // Bootstrap (departments/years/courses) failures surface in the same banner.
    setError(message.isEmpty() ? QStringLiteral("Failed to load filters. Please try again.") : message);
}
void ReportingViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
    emit canGenerateChanged();   // loading gates canGenerate
}
void ReportingViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}
void ReportingViewModel::applyResult(const QJsonArray &data)
{
    m_rows.setRows(data);
    m_courseBars.setBars(aggregateVisitsByCourse(data));
    const Tiles t = deriveTiles(data);
    m_totalVisits = t.totalVisits;
    m_studentsShown = t.studentsShown;
    m_topCourse = t.topCourse;
    m_hasResult = true;
    setError(QString());
    emit resultChanged();
}
