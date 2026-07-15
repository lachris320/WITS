#include "DashboardViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "apiconfig.h"
#include "dashboardparser.h"

DashboardViewModel::DashboardViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString DashboardViewModel::formatPeakHour(int hour)
{
    if (hour < 0 || hour > 23)
        return QStringLiteral("—");
    const int h12 = (hour % 12 == 0) ? 12 : (hour % 12);
    const QString ap = hour < 12 ? QStringLiteral("AM") : QStringLiteral("PM");
    return QStringLiteral("%1 %2").arg(h12).arg(ap);
}

void DashboardViewModel::refresh()
{
    setError(QString());
    setLoading(true);
    QNetworkReply *reply = m_nam->get(
        QNetworkRequest(ApiConfig::endpoint(QStringLiteral("dashboard_summary.php"))));
    const quint64 seq = nextRequestSeq();
    connect(reply, &QNetworkReply::finished, this, [this, reply, seq]() {
        const bool netErr = reply->error() != QNetworkReply::NoError;
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (!isCurrentRequest(seq)) return;   // superseded — drop
        setLoading(false);
        if (netErr) {
            resetPeakOnError();
            setError(QStringLiteral("Network error. Please try again."));
            return;
        }
        applySummary(body);
    });
}

void DashboardViewModel::applySummary(const QByteArray &raw)
{
    DashboardSummary s;
    QString err;
    if (!DashboardParser::parse(raw, s, err)) {
        resetPeakOnError();
        setError(err);
        return;
    }

    m_today = s.today;
    m_week = s.week;
    m_students = s.students;

    QList<BarsModel::Bar> hourly;
    hourly.reserve(s.hourly.size());
    int peakIdx = -1;
    int peakCount = -1;
    int peakHour = -1;
    for (int i = 0; i < s.hourly.size(); ++i) {
        const HourBucket &h = s.hourly.at(i);
        hourly.append({ formatPeakHour(h.hour), double(h.count) });
        if (h.count > peakCount) {
            peakCount = h.count;
            peakIdx = i;
            peakHour = h.hour;
        }
    }
    m_hourly.setBars(hourly);
    m_peakHourIndex = peakIdx;
    m_peakHourLabel = formatPeakHour(peakHour);

    QList<BarsModel::Bar> depts;
    depts.reserve(s.departments.size());
    for (const DeptBucket &d : s.departments)
        depts.append({ d.name, double(d.count) });
    m_department.setBars(depts);

    setError(QString());
    emit dataChanged();
}

// Shared by both error branches (network-error in refresh()'s reply lambda,
// parse-error in applySummary) so they stay internally consistent: the
// derived peak-hour fields reset rather than keep showing a stale peak from
// a previous successful fetch behind the error banner. Raw stats
// (today/week/students) and the bar models are intentionally left as-is —
// that is the existing, unchanged behavior this refactor preserves, not a
// new decision; widening the reset to those fields is out of scope here.
void DashboardViewModel::resetPeakOnError()
{
    m_peakHourIndex = -1;
    m_peakHourLabel = QStringLiteral("—");
    emit dataChanged();
}

void DashboardViewModel::setLoading(bool v)
{
    if (m_loading == v)
        return;
    m_loading = v;
    emit loadingChanged();
}

void DashboardViewModel::setError(const QString &e)
{
    if (m_errorText == e)
        return;
    m_errorText = e;
    emit errorTextChanged();
}

quint64 DashboardViewModel::nextRequestSeq()
{
    return ++m_requestSeq;
}

bool DashboardViewModel::isCurrentRequest(quint64 seq) const
{
    return seq == m_requestSeq;
}
