#ifndef REPORTCONTROLLER_H
#define REPORTCONTROLLER_H

#include <QByteArray>
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "reportdata.h"

class QNetworkAccessManager;

class ReportController : public QObject
{
    Q_OBJECT
public:
    explicit ReportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static ReportPalette getPalette(const QString &choice);
    static QStringList  parseDepartments(const QByteArray &raw);   // get_departments.php
    static QStringList  parseYears(const QByteArray &raw);         // get_years.php
    static QStringList  parseCourses(const QByteArray &raw);       // get_courses.php
    static ReportDataOutcome parseReportData(const QByteArray &raw,
                                             QJsonArray &outData,
                                             QString &outMessage); // get_report_data.php
    static QJsonArray   parsePreviewData(const QByteArray &raw);   // api.php/reports/data

    // Pure duration -> date-range math, extracted from collectReportFilters.
    // durationType: 0=day, 1=month, 2=semester, 3=custom (matches durationTypeBox).
    static DateRange computeDateRange(int durationType,
                                      const QDate &day,                     // case 0
                                      int month, int monthYear,             // case 1 (month 1..12)
                                      const QString &semester, int semYear, // case 2
                                      const QDate &customStart,             // case 3
                                      const QDate &customEnd);

    // Async network methods (implemented in Task 2).
    void loadDepartments();                      // GET get_departments.php
    void loadYears();                            // GET get_years.php
    void loadCourses(const QString &department); // GET get_courses.php?department=..&include_all=true
    void fetchReportRows(const QJsonObject &filters);  // POST get_report_data.php
    void fetchPreviewData(const QJsonObject &filters); // POST api.php/reports/data

signals:
    void departmentsLoaded(const QStringList &departments);
    void yearsLoaded(const QStringList &years);
    void coursesLoaded(const QStringList &courses);
    void reportDataReady(const QJsonArray &data);
    void reportError(const QString &message, bool critical);
    void previewDataReady(const QJsonArray &data);
    void loadError(const QString &title, const QString &message, bool critical);

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // REPORTCONTROLLER_H
