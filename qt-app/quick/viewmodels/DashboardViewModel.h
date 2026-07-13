#ifndef DASHBOARDVIEWMODEL_H
#define DASHBOARDVIEWMODEL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <qqml.h>
#include "BarsModel.h"

class QNetworkAccessManager;

// Dashboard screen's only QML-facing object (spec §6.1/§7.3). Fetches
// dashboard_summary.php once on navigation (manual refresh available; no
// polling). Peak hour is derived here from the hourly array (spec §5.1).
class DashboardViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int statToday READ statToday NOTIFY dataChanged)
    Q_PROPERTY(int statWeek READ statWeek NOTIFY dataChanged)
    Q_PROPERTY(int statStudents READ statStudents NOTIFY dataChanged)
    Q_PROPERTY(QString peakHourLabel READ peakHourLabel NOTIFY dataChanged)
    Q_PROPERTY(int peakHourIndex READ peakHourIndex NOTIFY dataChanged)
    Q_PROPERTY(BarsModel *hourlyModel READ hourlyModel CONSTANT)
    Q_PROPERTY(BarsModel *departmentModel READ departmentModel CONSTANT)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

public:
    explicit DashboardViewModel(QObject *parent = nullptr);

    int statToday() const { return m_today; }
    int statWeek() const { return m_week; }
    int statStudents() const { return m_students; }
    QString peakHourLabel() const { return m_peakHourLabel; }
    int peakHourIndex() const { return m_peakHourIndex; }
    BarsModel *hourlyModel() { return &m_hourly; }
    BarsModel *departmentModel() { return &m_department; }
    bool loading() const { return m_loading; }
    QString errorText() const { return m_errorText; }

    Q_INVOKABLE void refresh();

    // Network-free seam (tests + reply handler).
    void applySummary(const QByteArray &raw);

    // Pure: 0..23 -> "8 AM" / "2 PM"; out-of-range -> "—".
    static QString formatPeakHour(int hour);

signals:
    void dataChanged();
    void loadingChanged();
    void errorTextChanged();

private:
    void setLoading(bool v);
    void setError(const QString &e);

    QNetworkAccessManager *m_nam = nullptr;
    BarsModel m_hourly;
    BarsModel m_department;
    int m_today = 0;
    int m_week = 0;
    int m_students = 0;
    QString m_peakHourLabel = QStringLiteral("—");
    int m_peakHourIndex = -1;
    bool m_loading = false;
    QString m_errorText;
};

#endif // DASHBOARDVIEWMODEL_H
