#ifndef REPORTINGVIEWMODEL_H
#define REPORTINGVIEWMODEL_H

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <qqml.h>                 // QML_ELEMENT — AdminScreen instantiates this type
#include "BarsModel.h"
#include "ReportRowsModel.h"
#include "reportdata.h"            // DateRange — return type of semesterWindow()

class QNetworkAccessManager;
class ReportController;

// Reporting screen VM (spec 4b-i). Wraps the witscore ReportController (no new
// endpoint). Only QML-facing C++ for the reporting screen. Single-in-flight:
// generateReport() is a no-op while loading; canGenerate is false while loading.
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

    // Public slots (network-free test seam) — wired to ReportController in Task 5/6.
    void onDepartmentsLoaded(const QStringList &departments);
    void onYearsLoaded(const QStringList &years);
    void onCoursesLoaded(const QStringList &courses);
    void onReportDataReady(const QJsonArray &data);
    void onReportError(const QString &message, bool critical);
    void onLoadError(const QString &title, const QString &message, bool critical);

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

private:
    void setLoading(bool v);
    void setError(const QString &e);
    void applyResult(const QJsonArray &data);    // Task 6
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
};

#endif // REPORTINGVIEWMODEL_H
