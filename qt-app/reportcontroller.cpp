#include "reportcontroller.h"
#include "apiconfig.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QUrl>
#include <QUrlQuery>

ReportController::ReportController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

// Ported verbatim from adminWindow::getPalette (adminwindow.cpp:2478-2508).
ReportPalette ReportController::getPalette(const QString &choice) {
    if (choice == "Blue") {
        return {
            QColor("#2E86C1"), Qt::white, QColor("#EBF5FB"), Qt::white, Qt::black,
            { QColor("#0D47A1"), QColor("#1565C0"), QColor("#1976D2"),
             QColor("#1E88E5"), QColor("#42A5F5"), QColor("#64B5F6"),
             QColor("#90CAF9"), QColor("#BBDEFB") }
        };
    } else if (choice == "Green") {
        return {
            QColor("#27AE60"), Qt::white, QColor("#E9F7EF"), Qt::white, Qt::black,
            { QColor("#1B5E20"), QColor("#2E7D32"), QColor("#388E3C"),
             QColor("#43A047"), QColor("#66BB6A"), QColor("#81C784"),
             QColor("#A5D6A7"), QColor("#C8E6C9") }
        };
    } else if (choice == "Red") {
        return {
            QColor("#C0392B"), Qt::white, QColor("#FDEDEC"), Qt::white, Qt::black,
            { QColor("#7B241C"), QColor("#922B21"), QColor("#A93226"),
             QColor("#C0392B"), QColor("#CD6155"), QColor("#E6B0AA"),
             QColor("#F5B7B1"), QColor("#FADBD8") }
        };
    } else { // Default (multi-color)
        return {
            QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
            { QColor("#1f77b4"), QColor("#ff7f0e"), QColor("#2ca02c"),
             QColor("#d62728"), QColor("#9467bd"), QColor("#8c564b"),
             QColor("#e377c2"), QColor("#7f7f7f"), QColor("#bcbd22"), QColor("#17becf") }
        };
    }
}

// Ported from adminWindow::loadFilterDepartments (adminwindow.cpp:1818-1853):
// pure JSON-parsing half only, network I/O and UI population stay in adminwindow.cpp.
QStringList ReportController::parseDepartments(const QByteArray &raw) {
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return QStringList();

    QJsonObject obj = doc.object();
    if (obj["status"].toString() != "success")
        return QStringList();

    QStringList departments;
    const QJsonArray deptArray = obj["departments"].toArray();
    for (const auto &v : deptArray)
        departments << v.toString();
    return departments;
}

// Ported from adminWindow::loadAvailableYears (adminwindow.cpp:1855-1888).
QStringList ReportController::parseYears(const QByteArray &raw) {
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return QStringList();

    QJsonObject obj = doc.object();
    if (obj["status"].toString() != "success")
        return QStringList();

    QStringList years;
    const QJsonArray yearsArray = obj["years"].toArray();
    for (const auto &v : yearsArray)
        years << QString::number(v.toInt());
    return years;
}

// Ported from adminWindow::onFilterDepartmentBoxCurrentIndexChanged
// (adminwindow.cpp:1890-1937).
QStringList ReportController::parseCourses(const QByteArray &raw) {
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return QStringList();

    QJsonObject obj = doc.object();
    if (obj["status"].toString() != "success")
        return QStringList();

    QStringList courses;
    const QJsonArray coursesArray = obj["courses"].toArray();
    for (const auto &v : coursesArray)
        courses << v.toString();
    return courses;
}

// Ported from adminWindow::fetchReportData (adminwindow.cpp:1939-1966).
ReportDataOutcome ReportController::parseReportData(const QByteArray &raw,
                                                    QJsonArray &outData,
                                                    QString &outMessage) {
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return ReportDataOutcome::InvalidResponse;

    QJsonObject obj = doc.object();
    if (obj["status"].toString() != "success") {
        outMessage = obj["message"].toString();
        return ReportDataOutcome::NotSuccess;
    }

    outData = obj["data"].toArray();
    return ReportDataOutcome::Success;
}

// Ported from the api.php/reports/data preview handler (adminwindow.cpp:2728-2749).
QJsonArray ReportController::parsePreviewData(const QByteArray &raw) {
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return QJsonArray();

    QJsonObject obj = doc.object();
    if (obj["status"].toString() != "success")
        return QJsonArray();

    return obj["data"].toArray();
}

// Pure date-range math extracted from adminWindow::collectReportFilters's
// duration switch (adminwindow.cpp:1732-1802). Month is taken as 1..12
// directly (the 0-based combobox-index adjustment stays in adminwindow.cpp).
DateRange ReportController::computeDateRange(int durationType,
                                             const QDate &day,
                                             int month, int monthYear,
                                             const QString &semester, int semYear,
                                             const QDate &customStart,
                                             const QDate &customEnd) {
    DateRange range;

    switch (durationType) {
    case 0: { // Day
        if (!day.isValid())
            return DateRange();
        range.start = day.toString("yyyy-MM-dd");
        range.end   = day.toString("yyyy-MM-dd");
        range.valid = true;
        break;
    }
    case 1: { // Month
        QDate start(monthYear, month, 1);
        if (!start.isValid())
            return DateRange();
        QDate end = start.addMonths(1).addDays(-1);
        range.start = start.toString("yyyy-MM-dd");
        range.end   = end.toString("yyyy-MM-dd");
        range.valid = true;
        break;
    }
    case 2: { // Semester
        if (semester.contains("1")) {
            range.start = QString("%1-01-01").arg(semYear);
            range.end   = QString("%1-06-30").arg(semYear);
        } else {
            range.start = QString("%1-07-01").arg(semYear);
            range.end   = QString("%1-12-31").arg(semYear);
        }
        range.valid = true;
        break;
    }
    case 3: { // Custom
        if (!customStart.isValid() || !customEnd.isValid() || customStart > customEnd)
            return DateRange();
        range.start = customStart.toString("yyyy-MM-dd");
        range.end   = customEnd.toString("yyyy-MM-dd");
        range.valid = true;
        break;
    }
    default:
        return DateRange();
    }

    return range;
}

// ---- Network methods ----

// Ported from adminWindow::loadFilterDepartments (adminwindow.cpp:1818-1853).
void ReportController::loadDepartments() {
    QUrl url = ApiConfig::endpoint("get_departments.php");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loadError("Error", reply->errorString(), true);
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            emit loadError("Error", "Invalid server response.", false);
            return;
        }
        if (doc.object()["status"].toString() != "success") {
            emit loadError("Error", "Failed to load departments.", false);
            return;
        }
        emit departmentsLoaded(parseDepartments(resp));
    });
}

// Ported from adminWindow::loadAvailableYears (adminwindow.cpp:1855-1888).
// Non-object response is silent (cpp:1872) — no dialog, no signal.
void ReportController::loadYears() {
    QUrl url = ApiConfig::endpoint("get_years.php");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loadError("Error", reply->errorString(), true);
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject())
            return;
        if (doc.object()["status"].toString() != "success") {
            emit loadError("Error", doc.object()["message"].toString(), false);
            return;
        }
        emit yearsLoaded(parseYears(resp));
    });
}

// Ported from the network half of adminWindow::onFilterDepartmentBoxCurrentIndexChanged
// (adminwindow.cpp:1898-1936). The index <= 0 reset stays View-side.
void ReportController::loadCourses(const QString &department) {
    QUrl url = ApiConfig::endpoint("get_courses.php");
    QUrlQuery query;
    query.addQueryItem("department", department);
    query.addQueryItem("include_all", "true");
    url.setQuery(query);
    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loadError("Error", reply->errorString(), true);
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (!doc.isObject()) {
            emit loadError("Error", "Invalid response from server.", false);
            return;
        }
        if (doc.object()["status"].toString() != "success") {
            emit loadError("Error", doc.object()["message"].toString(), false);
            return;
        }
        emit coursesLoaded(parseCourses(resp));
    });
}

// Ported from adminWindow::postReportData (adminwindow.cpp:1646-1675), routed
// through the parseReportData outcome switch instead of inline JSON checks.
void ReportController::fetchReportRows(const QJsonObject &filters) {
    QUrl url = ApiConfig::endpoint("get_report_data.php");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit reportError(reply->errorString(), true);
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        QJsonArray data;
        QString message;
        switch (parseReportData(resp, data, message)) {
        case ReportDataOutcome::InvalidResponse:
            emit reportError("Invalid response from server.", false);
            break;
        case ReportDataOutcome::NotSuccess:
            emit reportError(message, false);
            break;
        case ReportDataOutcome::Success:
            emit reportDataReady(data);
            break;
        }
    });
}

// Ported from adminWindow::fetchPreviewData (adminwindow.cpp:2726-2750): a
// distinct API-router endpoint, silent on error (qDebug only), and emits only
// on a success object — including when the data array is empty (the View
// then clears the preview, as today). Deliberately NOT routed through
// parsePreviewData: that parser can't distinguish silent-failure from
// success-with-empty-data, and this method must.
void ReportController::fetchPreviewData(const QJsonObject &filters) {
    QUrl url = ApiConfig::endpoint("api.php/reports/data");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(filters).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Preview fetch error:" << reply->errorString();
            reply->deleteLater();
            return;
        }
        const QByteArray resp = reply->readAll();
        reply->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (doc.isObject() && doc.object()["status"].toString() == "success")
            emit previewDataReady(doc.object()["data"].toArray());
    });
}
