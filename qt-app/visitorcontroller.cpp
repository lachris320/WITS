#include "visitorcontroller.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

VisitorController::VisitorController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

void VisitorController::fetchVisitors(const VisitorFilter &filter)
{
    Q_UNUSED(filter);   // Stub — real network fetch lands in Task 2, Step 3.
}

bool VisitorController::exportToExcel(const QList<VisitorRecord> &records,
                                      const VisitorFilter &filter,
                                      const QString &schoolName,
                                      const QString &filePath)
{
    Q_UNUSED(records);
    Q_UNUSED(filter);
    Q_UNUSED(schoolName);
    Q_UNUSED(filePath);
    return false;       // Stub — real QXlsx export lands in Task 2, Step 4.
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
