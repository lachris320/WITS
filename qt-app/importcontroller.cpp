#include "importcontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ImportController::ImportController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

QString ImportController::normalizeHeader(const QString &raw)
{
    // Direct port of the free function at adminwindow.cpp:1376-1382.
    QString s = raw.trimmed().toLower();
    s.remove(' ');
    s.remove('_');
    s.remove('-');
    return s;
}

void ImportController::mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut)
{
    // Unifies the two duplicated if/else chains at adminwindow.cpp:1424-1443
    // and 1500-1519 into one function. First match wins, else-chain order.
    for (int tableCol = 0; tableCol < headers.size(); ++tableCol) {
        const QString n = normalizeHeader(headers.at(tableCol));

        if (n.contains("schoolid") || n.contains("studentid") || (n == "id"))
            indexOut["school_id"] = tableCol;
        else if (n.contains("name") || n.contains("fullname") || n.contains("full"))
            indexOut["name"] = tableCol;
        else if (n.contains("code") || n.contains("studentcode"))
            indexOut["code"] = tableCol;
        else if (n.contains("course"))
            indexOut["course"] = tableCol;
        else if (n.contains("year"))
            indexOut["year_level"] = tableCol;
        else if (n.contains("department") || n.contains("dept"))
            indexOut["department"] = tableCol;
        else if (n.contains("gender"))
            indexOut["gender"] = tableCol;
        else if (n.contains("status"))
            indexOut["status"] = tableCol;
        else if (n.contains("visit"))
            indexOut["visits"] = tableCol;
        else
            indexOut[QString("col_%1").arg(tableCol)] = tableCol;
    }
}

ParsedTable ImportController::parseCsv(const QString &rawText)
{
    // Pure port of loadCSVtoTable (adminwindow.cpp:1465-1542), minus file I/O.
    ParsedTable table;

    QStringList lines = rawText.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
    }

    // Header line (matches lines.first().split(",", Qt::SkipEmptyParts), line 1488-1490).
    QStringList headers = lines.first().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString &h : headers)
        h = h.trimmed();

    if (headers.isEmpty())
        return table;   // empty ParsedTable — View shows "CSV file has no headers." (dead-in-practice)

    table.headers = headers;
    mapHeaders(headers, table.headerIndex);

    for (int i = 1; i < lines.size(); ++i) {
        // Data rows split WITHOUT Qt::SkipEmptyParts (line 1524) — deliberate
        // asymmetry from the header split.
        const QStringList rowData = lines.at(i).split(QLatin1Char(','));

        if (rowData.isEmpty() || (rowData.size() == 1 && rowData.first().trimmed().isEmpty()))
            continue;   // matches the empty-line guard at lines 1527-1529

        QStringList row;
        row.reserve(rowData.size());
        for (const QString &cell : rowData)
            row << cell.trimmed();
        table.rows << row;   // ragged rows preserved as-is — no padding/truncation here
    }

    return table;
}

ParsedTable ImportController::parseExcel(const QString &filePath, ExcelParseError *errorOut)
{
    Q_UNUSED(filePath);
    if (errorOut)
        *errorOut = ExcelParseError::OpenFailed;
    return {};          // Stub — real QXlsx parse lands in Task 2, Step 4.
}

void ImportController::checkDuplicates(const QStringList &schoolIds)
{
    Q_UNUSED(schoolIds);   // Stub — real network call lands in Task 2, Step 2.
}

void ImportController::uploadStudents(const QString &excelPath, const QString &zipPath,
                                      const QStringList &skipIds)
{
    Q_UNUSED(excelPath);
    Q_UNUSED(zipPath);
    Q_UNUSED(skipIds);     // Stub — real network call lands in Task 2, Step 3.
}

bool ImportController::parseDuplicateResponse(const QByteArray &raw,
                                              QStringList *duplicatesOut,
                                              QString *errorOut)
{
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        *errorOut = QStringLiteral("Invalid duplicate check response.");
        return false;
    }

    const QJsonObject obj = doc.object();
    if (obj[QLatin1String("status")].toString() != QLatin1String("success")) {
        *errorOut = QStringLiteral("Duplicate check failed.");
        return false;
    }

    const QJsonArray dupArray = obj[QLatin1String("duplicates")].toArray();
    duplicatesOut->clear();
    for (const QJsonValue &v : dupArray)
        *duplicatesOut << v.toString();

    return true;
}

UploadResult ImportController::parseUploadResponse(const QByteArray &raw)
{
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        UploadResult result;
        result.plainText = true;
        result.rawText   = QString::fromUtf8(raw);
        return result;
    }

    const QJsonObject obj = doc.object();
    const QString status       = obj[QLatin1String("status")].toString();
    const QString message      = obj[QLatin1String("message")].toString();
    const int successCount     = obj[QLatin1String("success_count")].toInt();
    const int errorCount       = obj[QLatin1String("error_count")].toInt();

    UploadResult result;
    result.message      = message;
    result.successCount = successCount;
    result.errorCount   = errorCount;
    result.plainText    = false;
    result.ok           = (status == QLatin1String("success"));
    return result;
}
