#include "VisitLogsViewModel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include "apiconfig.h"
#include "visitlogparser.h"
#include "visitorcontroller.h"
#include "visitordata.h"

VisitLogsViewModel::VisitLogsViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_rangeLabel = computeRangeLabel();
}

QString VisitLogsViewModel::formatWeekLabel(const QDate &monday)
{
    const QDate sunday = monday.addDays(6);
    return QStringLiteral("%1 – %2")
        .arg(monday.toString(QStringLiteral("MMM d")),
             sunday.toString(QStringLiteral("MMM d, yyyy")));
}

QString VisitLogsViewModel::computeRangeLabel() const
{
    const QDate today = QDate::currentDate();
    if (m_range == Week) {
        // Qt: dayOfWeek() Monday==1. Step back to this week's Monday.
        const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
        return formatWeekLabel(monday);
    }
    return QStringLiteral("Today, %1").arg(today.toString(QStringLiteral("MMM d, yyyy")));
}

void VisitLogsViewModel::setMode(Mode m)
{
    if (m_mode == m)
        return;
    m_mode = m;
    emit modeChanged();
    refresh();
}

void VisitLogsViewModel::setRange(Range r)
{
    if (m_range == r)
        return;
    m_range = r;
    emit rangeChanged();
    refresh();
}

void VisitLogsViewModel::refresh()
{
    setError(QString());
    setLoading(true);

    if (m_mode == Student) {
        QUrl url = ApiConfig::endpoint(QStringLiteral("get_library_visits.php"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("range"),
                       m_range == Week ? QStringLiteral("week") : QStringLiteral("today"));
        url.setQuery(q);
        QNetworkReply *reply = m_nam->get(QNetworkRequest(url));
        const quint64 seq = nextRequestSeq();
        connect(reply, &QNetworkReply::finished, this, [this, reply, seq]() {
            const bool netErr = reply->error() != QNetworkReply::NoError;
            const QByteArray body = reply->readAll();
            reply->deleteLater();
            if (!isCurrentRequest(seq)) return;   // superseded — drop
            setLoading(false);
            if (netErr) { setError(QStringLiteral("Network error. Please try again.")); return; }
            applyStudentVisits(body);
        });
        return;
    }

    // Guest (§5.4): POST get_visitors.php with APP-GENERATED ISO dates only —
    // never free-text on the date path. range -> date_type + start/end.
    //
    // CRITICAL: get_visitors.php reads its params via
    // json_decode(file_get_contents("php://input")) — i.e. a JSON REQUEST BODY,
    // NOT form-urlencoded. So this must post an application/json body (matching
    // VisitorController::fetchVisitors), NOT HttpForm::submit — a form body
    // fails json_decode, the endpoint falls back to date_type="all", and the
    // range filter is silently ignored (returns every guest row as "success").
    const QDate today = QDate::currentDate();
    QString dateType, startDate, endDate;
    if (m_range == Week) {
        const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
        dateType  = QStringLiteral("date range");
        startDate = monday.toString(Qt::ISODate);
        endDate   = monday.addDays(6).toString(Qt::ISODate);
    } else {
        dateType  = QStringLiteral("specific day");
        startDate = today.toString(Qt::ISODate);
        endDate   = today.toString(Qt::ISODate);
    }
    QJsonObject payload;
    payload[QStringLiteral("date_type")]  = dateType;
    payload[QStringLiteral("start_date")] = startDate;
    payload[QStringLiteral("end_date")]   = endDate;

    QNetworkRequest req(ApiConfig::endpoint(QStringLiteral("get_visitors.php")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(payload).toJson());
    const quint64 seq = nextRequestSeq();
    connect(reply, &QNetworkReply::finished, this, [this, reply, seq]() {
        const bool netErr = reply->error() != QNetworkReply::NoError;
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (!isCurrentRequest(seq)) return;   // superseded — drop
        setLoading(false);
        if (netErr) { setError(QStringLiteral("Network error. Please try again.")); return; }
        applyGuestVisits(body);
    });
}

void VisitLogsViewModel::applyStudentVisits(const QByteArray &raw)
{
    QList<StudentVisitRecord> parsed;
    int count = 0;
    QString err;
    if (!VisitLogParser::parse(raw, parsed, count, err)) {
        setError(err);
        m_rows.setRows({});
        setCount(0);
        emit dataChanged();
        return;
    }
    QList<VisitLogRowsModel::Row> rows;
    rows.reserve(parsed.size());
    for (const StudentVisitRecord &r : parsed) {
        VisitLogRowsModel::Row row;
        row.date       = r.date;
        row.name       = r.name;
        row.course     = r.course;
        row.department = r.department;
        row.timeIn     = r.timeIn;
        row.timeOut    = QStringLiteral("—");   // login-only (§6.3)
        rows.append(row);
    }
    m_rows.setRows(rows);
    setCount(rows.size());
    m_rangeLabel = computeRangeLabel();
    setError(QString());
    emit dataChanged();
}

void VisitLogsViewModel::applyGuestVisits(const QByteArray &raw)
{
    QList<VisitorRecord> parsed;
    int total = 0;
    QString err;
    if (!VisitorController::parseVisitorsResponse(raw, &parsed, &total, &err)) {
        setError(err.isEmpty() ? QStringLiteral("Could not load guest logs.") : err);
        m_rows.setRows({});
        setCount(0);
        emit dataChanged();
        return;
    }
    QList<VisitLogRowsModel::Row> rows;
    rows.reserve(parsed.size());
    for (const VisitorRecord &v : parsed) {
        VisitLogRowsModel::Row row;
        row.name    = v.name;
        row.company = v.company;
        row.purpose = v.purpose;
        row.date    = v.date;
        row.timeIn  = v.time;
        // timeOut/course/department stay empty — not in the guest column set.
        rows.append(row);
    }
    m_rows.setRows(rows);
    setCount(rows.size());
    m_rangeLabel = computeRangeLabel();
    setError(QString());
    emit dataChanged();
}

void VisitLogsViewModel::setLoading(bool v)
{
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void VisitLogsViewModel::setError(const QString &e)
{
    if (m_errorText == e) return;
    m_errorText = e;
    emit errorTextChanged();
}

void VisitLogsViewModel::setCount(int c)
{
    m_count = c;   // emitted via dataChanged by callers
}

quint64 VisitLogsViewModel::nextRequestSeq()
{
    return ++m_requestSeq;
}

bool VisitLogsViewModel::isCurrentRequest(quint64 seq) const
{
    return seq == m_requestSeq;
}
