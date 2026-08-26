#ifndef REPORTINGVIEWMODEL_H
#define REPORTINGVIEWMODEL_H

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqml.h>                 // QML_ELEMENT — AdminScreen instantiates this type
#include "BarsModel.h"
#include "RankingModel.h"
#include "ReportRowsModel.h"
#include "reportanalytics.h"   // ReportAnalytics — cached from applyResult, reused by the export path
#include "reportdata.h"            // DateRange — return type of semesterWindow()
#include "timeanalytics.h"        // TimeAnalytics — computed in applyResult's sibling path

class QNetworkAccessManager;
class QPagedPaintDevice;
class ReportController;

// Reporting screen VM (spec 4b-i). Wraps the witscore ReportController (no new
// endpoint). Only QML-facing C++ for the reporting screen. Single-in-flight:
// generateReport() is a no-op while operationInFlight() is true — i.e. while
// EITHER the report-rows fetch or the time-analytics fetch is still pending;
// canGenerate() stays false across that whole both-settle window, not just
// while the primary rows fetch is loading.
class ReportingViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QStringList courses READ courses NOTIFY coursesChanged)
    Q_PROPERTY(QStringList years READ years NOTIFY yearsChanged)
    Q_PROPERTY(QString department READ department NOTIFY departmentChanged)
    Q_PROPERTY(QString course READ course NOTIFY courseChanged)
    Q_PROPERTY(int durationType READ durationType WRITE setDurationType NOTIFY durationTypeChanged)
    Q_PROPERTY(QString day READ day WRITE setDay NOTIFY dayChanged)
    Q_PROPERTY(int month READ month WRITE setMonth NOTIFY monthChanged)
    Q_PROPERTY(int monthYear READ monthYear WRITE setMonthYear NOTIFY monthYearChanged)
    Q_PROPERTY(QString semester READ semester WRITE setSemester NOTIFY semesterChanged)
    Q_PROPERTY(int semYear READ semYear WRITE setSemYear NOTIFY semYearChanged)
    Q_PROPERTY(QString customStart READ customStart WRITE setCustomStart NOTIFY customStartChanged)
    Q_PROPERTY(QString customEnd READ customEnd WRITE setCustomEnd NOTIFY customEndChanged)
    Q_PROPERTY(bool canGenerate READ canGenerate NOTIFY canGenerateChanged)
    Q_PROPERTY(bool filtersComplete READ filtersComplete NOTIFY canGenerateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY resultChanged)
    Q_PROPERTY(ReportRowsModel *rows READ rows CONSTANT)
    Q_PROPERTY(BarsModel *courseBars READ courseBars CONSTANT)
    Q_PROPERTY(int totalVisits READ totalVisits NOTIFY resultChanged)
    Q_PROPERTY(int studentsShown READ studentsShown NOTIFY resultChanged)
    Q_PROPERTY(QString topCourse READ topCourse NOTIFY resultChanged)
    Q_PROPERTY(int uniqueVisitors READ uniqueVisitors NOTIFY resultChanged)
    Q_PROPERTY(double avgVisitsPerVisitor READ avgVisitsPerVisitor NOTIFY resultChanged)
    Q_PROPERTY(QString topDepartment READ topDepartment NOTIFY resultChanged)
    Q_PROPERTY(int topDepartmentVisits READ topDepartmentVisits NOTIFY resultChanged)
    Q_PROPERTY(RankingModel *topStudents READ topStudents CONSTANT)
    Q_PROPERTY(RankingModel *topCourses READ topCourses CONSTANT)
    Q_PROPERTY(RankingModel *topDepartments READ topDepartments CONSTANT)
    Q_PROPERTY(QStringList palettes READ palettes CONSTANT)
    Q_PROPERTY(QString palette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(QStringList chartTypes READ chartTypes CONSTANT)
    Q_PROPERTY(QString chartType READ chartType WRITE setChartType NOTIFY chartTypeChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    Q_PROPERTY(bool canExport READ canExport NOTIFY canExportChanged)
    Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStatusChanged)
    Q_PROPERTY(QString exportError READ exportError NOTIFY exportErrorChanged)
    Q_PROPERTY(bool includeRosterInExport READ includeRosterInExport
               WRITE setIncludeRosterInExport NOTIFY includeRosterInExportChanged)
    Q_PROPERTY(bool hasTimeData READ hasTimeData NOTIFY hasTimeDataChanged)
    Q_PROPERTY(QString timeError READ timeError NOTIFY timeErrorChanged)
    Q_PROPERTY(bool timeLoading READ timeLoading NOTIFY timeLoadingChanged)
    Q_PROPERTY(QString busiestHourLabel READ busiestHourLabel NOTIFY busiestHourLabelChanged)
    Q_PROPERTY(QString busiestDayLabel READ busiestDayLabel NOTIFY busiestDayLabelChanged)
    Q_PROPERTY(BarsModel *hourlyBars READ hourlyBars CONSTANT)
    Q_PROPERTY(BarsModel *weekdayBars READ weekdayBars CONSTANT)
public:
    explicit ReportingViewModel(QObject *parent = nullptr);

    // Pure statics (network-free test seams).
    static QJsonObject buildFilters(const QString &department, const QString &course,
                                    int durationType,
                                    const QDate &day,
                                    int month, int monthYear,
                                    const QString &semester, int semYear,
                                    const QDate &customStart, const QDate &customEnd);
    static QList<BarsModel::Bar> aggregateVisitsByCourse(const QJsonArray &data); // Task 3
    struct Tiles { int totalVisits = 0; int studentsShown = 0; QString topCourse; };
    static Tiles deriveTiles(const QJsonArray &data);                             // Task 3
    static QJsonArray normalizeExportRows(const QJsonArray &data);   // visits string -> number
    // Display-only Period for a semester, matching get_report_data.php's server windows.
    static DateRange semesterWindow(const QString &semester, int year);
    // Builds the JSON keys the export renderer (paintReport/writeReportToXlsx) reads:
    // department, course, start, end, schoolYear, chartType.
    static QJsonObject buildExportFilters(
        const QString &department, const QString &course, int durationType,
        const QDate &day, int month, int monthYear,
        const QString &semester, int semYear,
        const QDate &customStart, const QDate &customEnd,
        const QString &chartType);

    QStringList departments() const { return m_departments; }
    QStringList courses() const { return m_courses; }
    QStringList years() const { return m_years; }
    QString department() const { return m_department; }
    QString course() const { return m_course; }
    int durationType() const { return m_durationType; }
    QString day() const { return m_day; }
    int month() const { return m_month; }
    int monthYear() const { return m_monthYear; }
    QString semester() const { return m_semester; }
    int semYear() const { return m_semYear; }
    QString customStart() const { return m_customStart; }
    QString customEnd() const { return m_customEnd; }
    bool canGenerate() const;                    // Task 4
    bool filtersComplete() const;                // GUI-smoke bug 3: dept + valid duration, ignoring loading
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }
    bool hasResult() const { return m_hasResult; }
    ReportRowsModel *rows() { return &m_rows; }
    BarsModel *courseBars() { return &m_courseBars; }
    int totalVisits() const { return m_totalVisits; }
    int studentsShown() const { return m_studentsShown; }
    QString topCourse() const { return m_topCourse; }
    int uniqueVisitors() const { return m_uniqueVisitors; }
    double avgVisitsPerVisitor() const { return m_avgVisitsPerVisitor; }
    QString topDepartment() const { return m_topDepartment; }
    int topDepartmentVisits() const { return m_topDepartmentVisits; }
    RankingModel *topStudents() { return &m_topStudents; }
    RankingModel *topCourses() { return &m_topCourses; }
    RankingModel *topDepartments() { return &m_topDepartments; }
    QStringList palettes() const { return { QStringLiteral("Default"), QStringLiteral("Blue"),
                                            QStringLiteral("Green"), QStringLiteral("Red") }; }
    QString palette() const { return m_palette; }
    QStringList chartTypes() const { return { QStringLiteral("Bar"), QStringLiteral("Pie") }; }
    QString chartType() const { return m_chartType; }
    bool exporting() const { return m_exporting; }
    bool canExport() const;
    QString exportStatus() const { return m_exportStatus; }
    QString exportError() const { return m_exportError; }
    bool includeRosterInExport() const { return m_includeRosterInExport; }
    bool hasTimeData() const { return m_hasTimeData; }
    QString timeError() const { return m_timeError; }
    bool timeLoading() const { return m_timeLoading; }
    QString busiestHourLabel() const { return m_busiestHourLabel; }
    QString busiestDayLabel() const { return m_busiestDayLabel; }
    BarsModel *hourlyBars() { return &m_hourlyBars; }
    BarsModel *weekdayBars() { return &m_weekdayBars; }

    Q_INVOKABLE void setPalette(const QString &p);
    Q_INVOKABLE void setChartType(const QString &c);
    Q_INVOKABLE void setIncludeRosterInExport(bool v);

    Q_INVOKABLE void loadDepartments();          // bootstrap: departments + years (Task 5)
    Q_INVOKABLE void setDepartment(const QString &department);   // Task 5
    Q_INVOKABLE void setCourse(const QString &course);           // Task 5
    Q_INVOKABLE void setDurationType(int t);     // Task 4
    Q_INVOKABLE void setDay(const QString &v);
    Q_INVOKABLE void setMonth(int v);
    Q_INVOKABLE void setMonthYear(int v);
    Q_INVOKABLE void setSemester(const QString &v);
    Q_INVOKABLE void setSemYear(int v);
    Q_INVOKABLE void setCustomStart(const QString &v);
    Q_INVOKABLE void setCustomEnd(const QString &v);
    Q_INVOKABLE void generateReport();           // Task 6
    Q_INVOKABLE void retry();                     // Task 6
    Q_INVOKABLE void exportPdf(const QUrl &fileUrl);
    Q_INVOKABLE void exportExcel(const QUrl &fileUrl);
    Q_INVOKABLE void printReport();

    // Assembles the presentation-ready carrier from cached time state (spec §7.1).
    // PUBLIC because tst_reportingviewmodel asserts it directly; also consumed by
    // the two export seams. Pure w.r.t. member state; performs no fetch.
    ReportTimeExport buildTimeExport() const;

    // Public slots (network-free test seam) — wired to ReportController in Task 5/6.
    void onDepartmentsLoaded(const QStringList &departments);
    void onYearsLoaded(const QStringList &years);
    void onCoursesLoaded(const QStringList &courses);
    void onReportDataReady(const QJsonArray &data);
    void onReportError(const QString &message, bool critical);
    void onLoadError(const QString &title, const QString &message, bool critical);
    void onTimeAnalyticsReady(const QList<int> &byHour, const QList<int> &byWeekday);
    void onTimeAnalyticsError(const QString &message);

signals:
    void departmentsChanged();
    void coursesChanged();
    void yearsChanged();
    void departmentChanged();
    void courseChanged();
    void durationTypeChanged();
    void dayChanged();
    void monthChanged();
    void monthYearChanged();
    void semesterChanged();
    void semYearChanged();
    void customStartChanged();
    void customEndChanged();
    void canGenerateChanged();
    void loadingChanged();
    void errorTextChanged();
    void resultChanged();
    void paletteChanged();
    void chartTypeChanged();
    void exportingChanged();
    void canExportChanged();
    void exportStatusChanged();
    void exportErrorChanged();
    void includeRosterInExportChanged();
    void hasTimeDataChanged();
    void timeErrorChanged();
    void timeLoadingChanged();
    void busiestHourLabelChanged();
    void busiestDayLabelChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);
    void setTimeLoading(bool v);
    void resetTimeSection();          // clears ALL When-section state at Generate start
    // Presentation shaping for the "When?" section (formatting lives HERE, not in
    // core — spec §5.4). Static + pure so they are directly unit-testable.
    static QList<BarsModel::Bar> buildHourlyBars(const QList<int> &hourly);        // 24, label blanked off-3h
    static QList<BarsModel::Bar> buildWeekdayBars(const QList<int> &weekdayMonFirst); // 7, Mon-first
    static QString hourTick(int hour);          // 0..23 -> "12A","3A",...,"9P"
    static QString formatHourRange(int hour);   // 14 -> "2–3 PM"
    static QString weekdayName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Monday".."Sunday"
    static QString weekdayShortName(int monFirstIndex); // 0..6 (Mon..Sun) -> "Mon".."Sun"
    bool operationInFlight() const;   // true until BOTH children settle
    void applyResult(const QJsonArray &data);    // Task 6
    void setExporting(bool v);
    void setExportStatus(const QString &s);
    void setExportError(const QString &e);
    ReportHeaderInfo headerInfo() const;
    bool renderToDevice(QPagedPaintDevice *dev, int resolution);
    // Shared preamble for the file exports: false (with exportError set) if an export is
    // in flight, there are no rows, or the URL is not a local file; on success sets *outPath,
    // clears exportError, and flips exporting on.
    bool beginFileExport(const QUrl &fileUrl, QString *outPath);
    // The export filters for the current VM state (keys read by ReportRenderer).
    QJsonObject currentExportFilters() const;
    static QDate parseDate(const QString &s) { return QDate::fromString(s, QStringLiteral("yyyy-MM-dd")); }

    QNetworkAccessManager *m_nam = nullptr;
    ReportController *m_controller = nullptr;
    ReportRowsModel m_rows;
    BarsModel m_courseBars;

    QStringList m_departments, m_courses, m_years;
    QString m_department, m_course;
    int m_durationType = 0;      // 0=Day default
    QString m_day, m_customStart, m_customEnd;
    int m_month = 0, m_monthYear = 0, m_semYear = 0;
    QString m_semester;

    bool m_loading = false;
    QString m_errorText;
    bool m_validationError = false;   // true only while errorText holds an incomplete-filters prompt
    bool m_hasResult = false;
    int m_totalVisits = 0, m_studentsShown = 0;
    QString m_topCourse = QStringLiteral("—");
    int m_uniqueVisitors = 0;
    double m_avgVisitsPerVisitor = 0.0;
    QString m_topDepartment = QStringLiteral("—");
    int m_topDepartmentVisits = 0;
    RankingModel m_topStudents, m_topCourses, m_topDepartments;

    QString m_palette = QStringLiteral("Default");
    QString m_chartType = QStringLiteral("Bar");
    bool m_exporting = false;
    QString m_exportStatus;
    QString m_exportError;
    QJsonArray m_exportRows;
    ReportAnalytics m_analytics;   // computed once in applyResult; reused by the export renderer
    bool m_includeRosterInExport = false;   // spec §9: export roster is opt-in, default OFF

    BarsModel m_hourlyBars;
    BarsModel m_weekdayBars;
    TimeAnalytics m_timeAnalytics;
    QString m_busiestHourLabel;
    QString m_busiestDayLabel;
    bool m_hasTimeData = false;
    QString m_timeError;
    bool m_timeLoading = false;
    // Settle flags start TRUE = "no operation pending", so canGenerate works
    // before the first Generate. generateReport() sets both false; each child
    // flips its own true on settle (success OR failure).
    bool m_reportRowsSettled = true;
    bool m_timeAnalyticsSettled = true;
};

#endif // REPORTINGVIEWMODEL_H
