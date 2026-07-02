#include "visitorcontroller.h"
#include "apiconfig.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>

#include "xlsxdocument.h"
#include "xlsxformat.h"

VisitorController::VisitorController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

void VisitorController::fetchVisitors(const VisitorFilter &filter)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("get_visitors.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonObject payload;
    payload[QLatin1String("search")]     = filter.search;
    payload[QLatin1String("date_type")]  = wireDateType(filter.dateType);
    payload[QLatin1String("start_date")] = filter.startDate;
    payload[QLatin1String("end_date")]   = filter.endDate;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());

    // `this` (the controller) as the context object is mandatory: the
    // connection auto-disconnects if the controller is destroyed while the
    // reply — owned by the shared QNetworkAccessManager — is still in flight.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(QStringLiteral("Network Error"), reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QList<VisitorRecord> records;
        int totalCount = 0;
        QString errorMsg;
        if (!parseVisitorsResponse(resp, &records, &totalCount, &errorMsg)) {
            emit fetchError(QStringLiteral("Error"), errorMsg);
            return;
        }

        emit visitorsLoaded(records, totalCount);
    });
}

bool VisitorController::exportToExcel(const QList<VisitorRecord> &records,
                                      const VisitorFilter &filter,
                                      const QString &schoolName,
                                      const QString &filePath)
{
    QXlsx::Document xlsx;

    QXlsx::Format boldCenter;
    boldCenter.setFontBold(true);
    boldCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setPatternBackgroundColor(Qt::lightGray);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    QXlsx::Format normalCenter;
    normalCenter.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    normalCenter.setBorderStyle(QXlsx::Format::BorderThin);

    // The View passes ui->schoolName->text() unmodified; the fallback lives here.
    const QString effectiveSchoolName =
        schoolName.isEmpty() ? QStringLiteral("School Name") : schoolName;
    const QString reportTitle   = QStringLiteral("Library Visitor Logs Report");
    const QString generatedDate =
        QDateTime::currentDateTime().toString(QStringLiteral("MMMM dd, yyyy hh:mm AP"));
    const QString datePart = datePartForFilter(filter);

    xlsx.mergeCells("A1:E1");
    xlsx.mergeCells("A2:E2");
    xlsx.write("A1", effectiveSchoolName, boldCenter);
    xlsx.write("A2", reportTitle, boldCenter);

    xlsx.write("A4", "Generated On:", boldCenter);
    xlsx.write("B4", generatedDate, normalCenter);

    // Written only when datePart != "All"; the Month datePart keeps its
    // underscore ("January_2026"), exactly as today (adminwindow.cpp:3842-3844).
    if (datePart != QLatin1String("All")) {
        xlsx.write("A5", "Filter Applied:", boldCenter);
        xlsx.write("B5", datePart, normalCenter);
    }

    // Written only when a search term exists. The underscore→space replace
    // preserves the pre-existing quirk (adminwindow.cpp:3846-3848): the old
    // code space→underscored the term for the filename and underscored→spaced
    // it back for this cell, which also turned any ORIGINAL underscores into
    // spaces.
    if (!filter.search.isEmpty()) {
        QString keyword = filter.search;
        keyword.replace(QLatin1Char('_'), QLatin1Char(' '));
        xlsx.write("A6", "Search Keyword:", boldCenter);
        xlsx.write("B6", keyword, normalCenter);
    }

    const int startRow = 8;
    const QStringList headers = {"Name", "Company", "Purpose", "Date", "Time"};
    for (int col = 0; col < headers.size(); ++col)
        xlsx.write(startRow, col + 1, headers.at(col), headerFormat);

    for (int row = 0; row < records.size(); ++row) {
        const VisitorRecord &rec = records.at(row);
        xlsx.write(startRow + row + 1, 1, rec.name,    normalCenter);
        xlsx.write(startRow + row + 1, 2, rec.company, normalCenter);
        xlsx.write(startRow + row + 1, 3, rec.purpose, normalCenter);
        xlsx.write(startRow + row + 1, 4, rec.date,    normalCenter);
        xlsx.write(startRow + row + 1, 5, rec.time,    normalCenter);
    }

    for (int col = 1; col <= 5; ++col)
        xlsx.setColumnWidth(col, 25);

    // Row count, not the server "count" field — matching adminwindow.cpp:3873.
    const int lastRow = startRow + records.size() + 2;
    xlsx.write(lastRow, 1, "Total Visitors:", boldCenter);
    xlsx.write(lastRow, 2, records.size(), normalCenter);

    return xlsx.saveAs(filePath);
}

QString VisitorController::wireDateType(DateFilterType t)
{
    // Legacy wire strings, verbatim — the combo box display text used to be
    // sent as-is (adminwindow.cpp:681, 700-702). Do not "normalize" these.
    switch (t) {
    case DateFilterType::SpecificDay: return QStringLiteral("Specific Day");
    case DateFilterType::Month:       return QStringLiteral("Month");
    case DateFilterType::DateRange:   return QStringLiteral("Date Range");
    case DateFilterType::All:         break;
    }
    return QStringLiteral("all");
}

QPair<QString, QString> VisitorController::monthRange(int month, int year)
{
    const QDate firstDay(year, month, 1);
    const QDate lastDay(year, month, firstDay.daysInMonth());
    return { firstDay.toString(QStringLiteral("yyyy-MM-dd")),
             lastDay.toString(QStringLiteral("yyyy-MM-dd")) };
}

QString VisitorController::defaultExportFileName(const VisitorFilter &f)
{
    QString baseName = QStringLiteral("VisitorLogs");
    if (!f.search.isEmpty()) {
        QString searchTerm = f.search;   // already trimmed by the View
        searchTerm.replace(QLatin1Char(' '), QLatin1Char('_'));
        baseName += QLatin1Char('_') + searchTerm;
    }
    return QStringLiteral("%1_%2.xlsx").arg(baseName, datePartForFilter(f));
}

bool VisitorController::parseVisitorsResponse(const QByteArray &json,
                                              QList<VisitorRecord> *out,
                                              int *totalCount,
                                              QString *errorMsg)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        *errorMsg = QStringLiteral("Invalid server response.");
        return false;
    }

    const QJsonObject obj = doc.object();
    if (obj[QLatin1String("status")].toString() != QLatin1String("success")) {
        *errorMsg = obj[QLatin1String("message")].toString();
        return false;
    }

    const QJsonArray logs = obj[QLatin1String("visitors")].toArray();
    *totalCount = obj[QLatin1String("count")].toInt();

    out->clear();
    out->reserve(logs.size());
    for (const QJsonValue &value : logs) {
        const QJsonObject log = value.toObject();

        VisitorRecord rec;
        rec.name    = log[QLatin1String("name")].toString();
        rec.company = log[QLatin1String("company")].toString();
        rec.purpose = log[QLatin1String("purpose")].toString();

        const QString timeIn = log[QLatin1String("time_in")].toString();
        const QDateTime dt =
            QDateTime::fromString(timeIn, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        rec.date = dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd"))
                                : QStringLiteral("N/A");
        rec.time = dt.isValid() ? dt.toString(QStringLiteral("hh:mm AP"))
                                : QStringLiteral("N/A");

        out->append(rec);
    }
    return true;
}

QString VisitorController::datePartForFilter(const VisitorFilter &f)
{
    switch (f.dateType) {
    case DateFilterType::SpecificDay:
        return f.startDate;
    case DateFilterType::Month: {
        // In Month mode startDate is always "yyyy-MM-01". QLocale::c()
        // guarantees the same English month names the combo box hardcodes
        // (adminwindow.cpp:643-644), regardless of the system locale.
        const QDate d = QDate::fromString(f.startDate, QStringLiteral("yyyy-MM-dd"));
        return QStringLiteral("%1_%2")
            .arg(QLocale::c().toString(d, QStringLiteral("MMMM")),
                 QString::number(d.year()));
    }
    case DateFilterType::DateRange:
        return QStringLiteral("Range_%1_to_%2").arg(f.startDate, f.endDate);
    case DateFilterType::All:
        break;
    }
    return QStringLiteral("All");
}
