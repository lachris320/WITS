#include "ReportingViewModel.h"

#include <QFileInfo>
#include <QMap>
#include <QNetworkAccessManager>
#include <QPageSize>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QUrl>
#include <algorithm>
#include "appsettings.h"
#include "reportanalytics.h"
#include "reportcontroller.h"
#include "reportdata.h"
#include "reportrenderer.h"
#include "xlsxdocument.h"

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
    connect(m_controller, &ReportController::timeAnalyticsReady,
            this, &ReportingViewModel::onTimeAnalyticsReady);
    connect(m_controller, &ReportController::timeAnalyticsError,
            this, &ReportingViewModel::onTimeAnalyticsError);

    // Auto-clear the validation prompt the moment filters become complete —
    // every filter setter emits canGenerateChanged. A real fetch/bootstrap
    // error (m_validationError == false) is left untouched by filter changes.
    connect(this, &ReportingViewModel::canGenerateChanged, this, [this]() {
        if (m_validationError && filtersComplete()) {
            m_validationError = false;
            setError(QString());
        }
    });
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

QList<BarsModel::Bar> ReportingViewModel::buildHourlyBars(const QList<int> &hourly)
{
    QList<BarsModel::Bar> bars;
    if (hourly.size() != 24)
        return bars;
    bars.reserve(24);
    for (int h = 0; h < 24; ++h) {
        // Interval x-labels are DATA-DRIVEN (spec §5.4): only hours 0/3/6.. carry a
        // label; the rest are blank so LBarChart (a Text under EVERY bar) shows
        // ~every-3h ticks. The chart has no thinning logic of its own.
        const QString label = (h % 3 == 0) ? hourTick(h) : QString();
        bars.append({ label, double(hourly.at(h)) });
    }
    return bars;
}

QList<BarsModel::Bar> ReportingViewModel::buildWeekdayBars(const QList<int> &weekdayMonFirst)
{
    QList<BarsModel::Bar> bars;
    if (weekdayMonFirst.size() != 7)
        return bars;
    static const char *const kShort[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    bars.reserve(7);
    for (int d = 0; d < 7; ++d)
        bars.append({ QString::fromLatin1(kShort[d]), double(weekdayMonFirst.at(d)) });
    return bars;
}

QString ReportingViewModel::hourTick(int hour)
{
    const int h12 = (hour % 12 == 0) ? 12 : (hour % 12);
    const QChar suffix = (hour < 12) ? QLatin1Char('A') : QLatin1Char('P');
    return QStringLiteral("%1%2").arg(h12).arg(suffix);
}

QString ReportingViewModel::formatHourRange(int hour)
{
    const int startH = hour;
    const int endH = (hour + 1) % 24;
    const auto to12 = [](int h) { return (h % 12 == 0) ? 12 : (h % 12); };
    const auto ampm = [](int h) { return h < 12 ? QStringLiteral("AM") : QStringLiteral("PM"); };
    // Same meridiem -> one suffix ("2–3 PM"); otherwise annotate both ("11 PM–12 AM").
    if (ampm(startH) == ampm(endH))
        return QStringLiteral("%1–%2 %3").arg(to12(startH)).arg(to12(endH)).arg(ampm(startH));
    return QStringLiteral("%1 %2–%3 %4")
            .arg(to12(startH)).arg(ampm(startH)).arg(to12(endH)).arg(ampm(endH));
}

QString ReportingViewModel::weekdayName(int monFirstIndex)
{
    static const char *const kNames[7] = { "Monday", "Tuesday", "Wednesday",
                                           "Thursday", "Friday", "Saturday", "Sunday" };
    if (monFirstIndex < 0 || monFirstIndex >= 7)
        return QString();
    return QString::fromLatin1(kNames[monFirstIndex]);
}

QJsonArray ReportingViewModel::normalizeExportRows(const QJsonArray &data)
{
    QJsonArray out;
    for (const QJsonValue &v : data) {
        QJsonObject o = v.toObject();
        o["visits"] = reportVisits(o);   // robust string-or-number -> int
        out.append(o);
    }
    return out;
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

DateRange ReportingViewModel::semesterWindow(const QString &semester, int year)
{
    DateRange r;
    if (year <= 0)
        return r;   // invalid
    const QString s = semester.toLower();
    if (s.contains(QStringLiteral("first"))) {
        r.start = QStringLiteral("%1-06-01").arg(year);
        r.end   = QStringLiteral("%1-10-31").arg(year);
        r.valid = true;
    } else if (s.contains(QStringLiteral("second"))) {
        r.start = QStringLiteral("%1-11-01").arg(year);
        r.end   = QStringLiteral("%1-03-31").arg(year + 1);
        r.valid = true;
    } else if (s.contains(QStringLiteral("summer"))) {
        r.start = QStringLiteral("%1-04-01").arg(year);
        r.end   = QStringLiteral("%1-05-31").arg(year);
        r.valid = true;
    }
    return r;   // unknown label -> valid stays false
}

QJsonObject ReportingViewModel::buildExportFilters(
    const QString &department, const QString &course, int durationType,
    const QDate &day, int month, int monthYear,
    const QString &semester, int semYear,
    const QDate &customStart, const QDate &customEnd,
    const QString &chartType)
{
    QJsonObject f;
    f["department"] = department.isEmpty() ? QStringLiteral("All Departments") : department;
    f["course"]     = course.isEmpty()     ? QStringLiteral("All Courses")     : course;
    f["chartType"]  = chartType;

    DateRange r;
    QString schoolYear;
    switch (durationType) {
    case 0:  // Day
        r = ReportController::computeDateRange(0, day, 0, 0, QString(), 0, QDate(), QDate());
        schoolYear = QString::number(day.year());
        break;
    case 1:  // Month
        r = ReportController::computeDateRange(1, QDate(), month, monthYear, QString(), 0, QDate(), QDate());
        schoolYear = QString::number(monthYear);
        break;
    case 2:  // Semester — display range from the server-matched window
        r = semesterWindow(semester, semYear);
        schoolYear = QString::number(semYear);
        break;
    case 3:  // Custom
        r = ReportController::computeDateRange(3, QDate(), 0, 0, QString(), 0, customStart, customEnd);
        schoolYear = QString::number(customStart.year());
        break;
    default:
        break;
    }
    f["start"]      = r.valid ? r.start : QString();
    f["end"]        = r.valid ? r.end   : QString();
    f["schoolYear"] = schoolYear;
    return f;
}

bool ReportingViewModel::filtersComplete() const
{
    // Department is OPTIONAL (empty = all departments; the backend treats an
    // empty department as no filter). Only a complete duration is required.
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

bool ReportingViewModel::operationInFlight() const
{
    return !(m_reportRowsSettled && m_timeAnalyticsSettled);
}

bool ReportingViewModel::canGenerate() const
{
    return filtersComplete() && !operationInFlight();
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
    if (operationInFlight())          // single-in-flight = ONE operation
        return;
    if (!filtersComplete()) {
        setError(QStringLiteral("Complete the selected duration before generating a report."));
        m_validationError = true;
        return;
    }
    m_validationError = false;
    setError(QString());

    // Reset ALL When-section state BEFORE the fetches (staleness guard, spec §5.1):
    // without this, a re-run would show the previous run's captions/bars in flight.
    resetTimeSection();

    // Two child requests, ONE logical Generate operation.
    m_reportRowsSettled = false;
    m_timeAnalyticsSettled = false;
    setLoading(true);                 // rows loading -> main preview dim; also emits canGenerateChanged()
    setTimeLoading(true);             // section spinner

    const QJsonObject filters = buildFilters(
        m_department, m_course, m_durationType,
        parseDate(m_day), m_month, m_monthYear,
        m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd));
    m_controller->fetchReportRows(filters);
    m_controller->fetchTimeAnalytics(filters);   // parallel, same filters
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
    // The server list is requested with include_all=true, so it already
    // carries its own "All"/"All Courses" entry; LCascadingSelect prepends
    // its own "All" on top of whatever this list holds. Strip the server's
    // entry so the cascade doesn't show "All" twice.
    m_courses.clear();
    m_courses.reserve(courses.size());
    for (const QString &c : courses) {
        const QString trimmed = c.trimmed();
        if (trimmed.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0
            || trimmed.compare(QStringLiteral("All Courses"), Qt::CaseInsensitive) == 0)
            continue;
        m_courses.append(c);
    }
    emit coursesChanged();
}
void ReportingViewModel::onReportDataReady(const QJsonArray &data)
{
    m_reportRowsSettled = true;
    setLoading(false);
    applyResult(data);
}

void ReportingViewModel::onReportError(const QString &message, bool /*critical*/)
{
    m_reportRowsSettled = true;
    setLoading(false);
    setError(message.isEmpty() ? QStringLiteral("Report failed. Please try again.") : message);
    m_validationError = false;   // a real fetch error, not a validation prompt
}

void ReportingViewModel::onLoadError(const QString &/*title*/, const QString &message, bool /*critical*/)
{
    // Bootstrap (departments/years/courses) failures surface in the same banner.
    setError(message.isEmpty() ? QStringLiteral("Failed to load filters. Please try again.") : message);
    m_validationError = false;   // a real bootstrap error, not a validation prompt
}
void ReportingViewModel::onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday)
{
    m_timeAnalytics = TimeAnalytics::compute(byHour, byWeekday);

    m_hourlyBars.setBars(buildHourlyBars(m_timeAnalytics.hourly));
    m_weekdayBars.setBars(buildWeekdayBars(m_timeAnalytics.weekdayMonFirst));

    m_hasTimeData = m_timeAnalytics.hasData;
    emit hasTimeDataChanged();

    // Captions ONLY when there is data — peak indices default to 0 ("12–1 AM" /
    // "Monday") on an all-zero range and would mislead (spec §6).
    m_busiestHourLabel = m_timeAnalytics.hasData ? formatHourRange(m_timeAnalytics.peakHour)
                                                 : QString();
    emit busiestHourLabelChanged();
    m_busiestDayLabel = m_timeAnalytics.hasData ? weekdayName(m_timeAnalytics.peakWeekdayMonFirst)
                                                : QString();
    emit busiestDayLabelChanged();

    if (!m_timeError.isEmpty()) { m_timeError.clear(); emit timeErrorChanged(); }

    setTimeLoading(false);
    m_timeAnalyticsSettled = true;
    emit canGenerateChanged();
}

void ReportingViewModel::onTimeAnalyticsError(const QString &message)
{
    // Localized failure (spec §5.2): set ONLY m_timeError; NEVER m_errorText,
    // which would blank the whole preview + block export.
    m_timeError = message.isEmpty() ? QStringLiteral("Couldn't load visit times.") : message;
    emit timeErrorChanged();
    if (m_hasTimeData) { m_hasTimeData = false; emit hasTimeDataChanged(); }
    setTimeLoading(false);
    m_timeAnalyticsSettled = true;
    emit canGenerateChanged();
}

void ReportingViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    if (v) m_exportRows = QJsonArray();   // new fetch starting -> drop the previous export rows
    emit loadingChanged();
    emit canGenerateChanged();   // loading gates canGenerate
    emit canExportChanged();     // loading (and cleared rows) gate canExport
}
void ReportingViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
    emit canExportChanged();
}
void ReportingViewModel::setTimeLoading(bool v)
{
    if (m_timeLoading == v) return;
    m_timeLoading = v;
    emit timeLoadingChanged();
}
void ReportingViewModel::resetTimeSection()
{
    m_timeAnalytics = TimeAnalytics();
    m_hourlyBars.setBars({});
    m_weekdayBars.setBars({});
    if (m_hasTimeData)              { m_hasTimeData = false;      emit hasTimeDataChanged(); }
    if (!m_busiestHourLabel.isEmpty()) { m_busiestHourLabel.clear(); emit busiestHourLabelChanged(); }
    if (!m_busiestDayLabel.isEmpty())  { m_busiestDayLabel.clear();  emit busiestDayLabelChanged(); }
    if (!m_timeError.isEmpty())    { m_timeError.clear();        emit timeErrorChanged(); }
}
void ReportingViewModel::applyResult(const QJsonArray &data)
{
    m_rows.setRows(data);
    m_exportRows = normalizeExportRows(data);   // reused by export (numeric visits, all 8 cols)
    m_courseBars.setBars(aggregateVisitsByCourse(data));
    const Tiles t = deriveTiles(data);
    m_totalVisits = t.totalVisits;
    m_studentsShown = t.studentsShown;
    m_topCourse = t.topCourse;
    m_analytics = ReportAnalytics::compute(m_exportRows);
    m_uniqueVisitors = m_analytics.kpis.uniqueVisitors;
    m_avgVisitsPerVisitor = m_analytics.kpis.avgVisitsPerVisitor;
    m_topDepartment = m_analytics.kpis.hasData ? m_analytics.kpis.topDepartment : QStringLiteral("—");
    m_topDepartmentVisits = m_analytics.kpis.topDepartmentVisits;
    m_topStudents.setEntries(m_analytics.topStudents);
    m_topCourses.setEntries(m_analytics.topCourses);
    m_topDepartments.setEntries(m_analytics.topDepartments);
    m_hasResult = true;
    m_validationError = false;
    setError(QString());
    emit resultChanged();
    emit canExportChanged();
}

bool ReportingViewModel::canExport() const
{
    return m_hasResult && !m_loading && !m_exporting
           && m_errorText.isEmpty() && m_rows.count() > 0;
}
void ReportingViewModel::setPalette(const QString &p)
{
    if (m_palette == p) return;
    m_palette = p; emit paletteChanged();
}
void ReportingViewModel::setChartType(const QString &c)
{
    if (m_chartType == c) return;
    m_chartType = c; emit chartTypeChanged();
}
void ReportingViewModel::setIncludeRosterInExport(bool v)
{
    if (m_includeRosterInExport == v) return;
    m_includeRosterInExport = v;
    emit includeRosterInExportChanged();
}
void ReportingViewModel::setExporting(bool v)
{
    if (m_exporting == v) return;
    m_exporting = v; emit exportingChanged(); emit canExportChanged();
}
void ReportingViewModel::setExportStatus(const QString &s)
{
    m_exportStatus = s; emit exportStatusChanged();
}
void ReportingViewModel::setExportError(const QString &e)
{
    m_exportError = e; emit exportErrorChanged();
}

ReportHeaderInfo ReportingViewModel::headerInfo() const
{
    AppSettings s;   // mandated scope — matches what Phase 4c Settings wrote
    ReportHeaderInfo info;
    info.schoolName = s.value(QStringLiteral("school/name"), QStringLiteral("Your School Name")).toString();
    info.address    = s.value(QStringLiteral("school/address"), QStringLiteral("Your Address")).toString();
    info.logoPath   = s.value(QStringLiteral("school/logoPath"), QString()).toString();
    info.librarian  = s.value(QStringLiteral("admin/name"), QString()).toString();
    info.position   = s.value(QStringLiteral("admin/position"), QString()).toString();
    info.openHour   = s.value(QStringLiteral("library/openHour"), 7).toInt();
    info.closeHour  = s.value(QStringLiteral("library/closeHour"), 21).toInt();
    return info;
}

bool ReportingViewModel::renderToDevice(QPagedPaintDevice *dev, int resolution)
{
    const QJsonObject filters = currentExportFilters();
    const ReportPalette pal = ReportController::getPalette(m_palette);
    return ReportRenderer::paintReport(dev, resolution, m_exportRows, filters, pal,
                                       headerInfo(), m_analytics, m_includeRosterInExport);
}

bool ReportingViewModel::beginFileExport(const QUrl &fileUrl, QString *outPath)
{
    if (m_exporting)
        return false;
    if (m_exportRows.isEmpty()) {
        setExportError(tr("No data to export. Adjust the filters and generate a report with results."));
        return false;
    }
    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        setExportError(tr("Couldn't export — choose a local file location."));
        return false;
    }
    setExportError(QString());
    setExporting(true);
    *outPath = path;
    return true;
}

QJsonObject ReportingViewModel::currentExportFilters() const
{
    return buildExportFilters(m_department, m_course, m_durationType,
        parseDate(m_day), m_month, m_monthYear,
        m_semester, m_semYear, parseDate(m_customStart), parseDate(m_customEnd),
        m_chartType);
}

void ReportingViewModel::exportPdf(const QUrl &fileUrl)
{
    QString path;
    if (!beginFileExport(fileUrl, &path))
        return;
    // Defer one turn so the busy overlay paints before the blocking render.
    QMetaObject::invokeMethod(this, [this, path]() {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        const bool ok = renderToDevice(&writer, writer.resolution()) && QFileInfo::exists(path);
        if (ok)
            setExportStatus(tr("Saved %1").arg(QFileInfo(path).fileName()));
        else
            setExportError(tr("Couldn't write %1 — choose a different location.").arg(QFileInfo(path).fileName()));
        setExporting(false);
    }, Qt::QueuedConnection);
}

void ReportingViewModel::printReport()
{
    if (m_exporting) return;
    if (m_exportRows.isEmpty()) {
        setExportError(tr("No data to export. Adjust the filters and generate a report with results."));
        return;
    }
    // Opening the dialog is NOT "exporting" — the normal UI stays live.
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer);
    if (dlg.exec() != QDialog::Accepted)
        return;   // cancelled -> no-op, no error, no busy state
    setExportError(QString());
    setExporting(true);   // now rendering begins
    const bool ok = renderToDevice(&printer, printer.resolution());
    if (ok)
        setExportStatus(tr("Sent to printer"));
    else
        setExportError(tr("Couldn't print the report."));
    setExporting(false);
}

void ReportingViewModel::exportExcel(const QUrl &fileUrl)
{
    QString path;
    if (!beginFileExport(fileUrl, &path))
        return;
    QMetaObject::invokeMethod(this, [this, path]() {
        QXlsx::Document doc;
        const bool ok = ReportRenderer::writeReportToXlsx(
                            doc, m_exportRows, currentExportFilters(), headerInfo(),
                            m_analytics, m_includeRosterInExport)
                        && doc.saveAs(path);
        if (ok)
            setExportStatus(tr("Saved %1").arg(QFileInfo(path).fileName()));
        else
            setExportError(tr("Couldn't write %1 — choose a different location.").arg(QFileInfo(path).fileName()));
        setExporting(false);
    }, Qt::QueuedConnection);
}
