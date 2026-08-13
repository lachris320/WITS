#include "importcontroller.h"
#include "apiconfig.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "studentcontroller.h"
#include "xlsxdocument.h"
#include "xlsxcellrange.h"

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

QByteArray ImportController::serializeRows(const ParsedTable &table)
{
    static const QStringList kKeys = {
        QStringLiteral("school_id"), QStringLiteral("name"),
        QStringLiteral("course"),    QStringLiteral("department"),
        QStringLiteral("year_level"),QStringLiteral("gender"),
        QStringLiteral("status")
    };

    QJsonArray out;
    for (const QStringList &row : table.rows) {
        QJsonObject obj;
        for (const QString &key : kKeys) {
            QString value;
            if (table.headerIndex.contains(key)) {
                const int col = table.headerIndex.value(key);
                if (col >= 0 && col < row.size())
                    value = row.at(col).trimmed();
            }
            obj[key] = value;   // absent/out-of-range column -> "" (key still present)
        }
        out.append(obj);
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

ParsedTable ImportController::parseExcel(const QString &filePath, ExcelParseError *errorOut)
{
    ParsedTable table;

    QXlsx::Document xlsx(filePath);
    if (!xlsx.isLoadPackage()) {
        if (errorOut)
            *errorOut = ExcelParseError::OpenFailed;
        return table;
    }

    const QStringList sheets = xlsx.sheetNames();
    if (sheets.isEmpty()) {
        if (errorOut)
            *errorOut = ExcelParseError::NoSheets;
        return table;
    }
    xlsx.selectSheet(sheets.first());

    const QXlsx::CellRange rng = xlsx.dimension();
    if (!rng.isValid()) {
        if (errorOut)
            *errorOut = ExcelParseError::EmptySheet;
        return table;
    }

    const int firstRow = rng.firstRow();
    const int lastRow  = rng.lastRow();
    const int firstCol = rng.firstColumn();
    const int lastCol  = rng.lastColumn();

    QStringList headers;
    for (int c = firstCol; c <= lastCol; ++c)
        headers << xlsx.read(firstRow, c).toString();

    table.headers = headers;
    mapHeaders(headers, table.headerIndex);   // tableCol = c - firstCol, matching line 1422

    // rows = lastRow - firstRow (excludes header row) — off-by-one convention
    // preserved verbatim from line 1446, NOT lastRow - firstRow + 1.
    for (int r = firstRow + 1; r <= lastRow; ++r) {
        QStringList row;
        for (int c = firstCol; c <= lastCol; ++c)
            row << xlsx.read(r, c).toString();
        table.rows << row;
    }

    if (errorOut)
        *errorOut = ExcelParseError::None;
    return table;
}

void ImportController::checkDuplicates(const QStringList &schoolIds, const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("check_duplicates.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    // school_ids as a JSON-array string in one form field (mirrors how
    // bulk_update_students sends `students`), admin_key as a sibling field.
    QJsonArray idsArray;
    for (const QString &id : schoolIds)
        idsArray.append(id);
    const QByteArray idsJson = QJsonDocument(idsArray).toJson(QJsonDocument::Compact);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("school_ids"), QString::fromUtf8(idsJson));
    body.addQueryItem(QStringLiteral("admin_key"), adminKey);

    QNetworkReply *reply =
        m_nam->post(request, body.toString(QUrl::FullyEncoded).toUtf8());

    // `this` as the context object: auto-disconnect if the controller dies
    // while the reply (owned by the shared NAM) is still in flight.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray resp = reply->readAll();
        const bool hadError = reply->error() != QNetworkReply::NoError;
        const QString errorString = reply->errorString();
        const QVariant statusAttr =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        reply->deleteLater();

        if (!StudentController::replyIsServerAnswer(hadError, httpStatus, resp)) {
            // Genuine transport failure (no server answer). Legacy title "Error".
            emit importError(QStringLiteral("Error"), errorString, ImportSeverity::Critical);
            return;
        }

        QStringList duplicates;
        QString errorMsg;
        if (!parseDuplicateResponse(resp, &duplicates, &errorMsg)) {
            // A 401-with-body is a server answer; surface it as a clear auth error.
            if (httpStatus == 401)
                emit importError(QStringLiteral("Authentication"),
                                 QStringLiteral("Admin authentication required — re-enter via admin login."),
                                 ImportSeverity::Critical);
            else
                emit importError(QStringLiteral("Error"), errorMsg, ImportSeverity::Warning);
            return;
        }

        emit duplicatesResolved(duplicates);   // empty list = no duplicates found
    });
}

void ImportController::uploadStudents(const ParsedTable &table, const QString &zipPath,
                                      const QStringList &skipIds, const QString &adminKey)
{
    QNetworkRequest uploadRequest(ApiConfig::endpoint(QStringLiteral("upload_students_zip.php")));
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto addText = [multiPart](const QString &name, const QByteArray &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"%1\"").arg(name)));
        part.setBody(value);
        multiPart->append(part);
    };

    // rows: the client-parsed, header-mapped payload (the ONLY serializeRows
    // call site). Replaces the old raw-Excel file part.
    addText(QStringLiteral("rows"), serializeRows(table));
    addText(QStringLiteral("admin_key"), adminKey.toUtf8());   // guard field — never logged

    // skip_ids — only appended when non-empty (comma-joined; preserved).
    if (!skipIds.isEmpty())
        addText(QStringLiteral("skip_ids"), skipIds.join(QLatin1Char(',')).toUtf8());

    // photos_zip (optional, non-fatal on open failure).
    if (!zipPath.isEmpty()) {
        QHttpPart zipPart;
        zipPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"photos_zip\"; filename=\"" +
                                   QFileInfo(zipPath).fileName() + "\""));
        QFile *zipFile = new QFile(zipPath);
        if (!zipFile->open(QIODevice::ReadOnly)) {
            emit importError(QStringLiteral("Warning"),
                             QStringLiteral("Cannot open ZIP file. Proceeding without photos."),
                             ImportSeverity::Warning);
            delete zipFile;
        } else {
            zipPart.setBodyDevice(zipFile);
            zipFile->setParent(multiPart);
            multiPart->append(zipPart);
        }
    }

    // The client already parsed the file, so there is no fatal open branch:
    // fire uploadStarted immediately before the POST.
    emit uploadStarted();

    QNetworkReply *uploadReply = m_nam->post(uploadRequest, multiPart);
    multiPart->setParent(uploadReply);

    connect(uploadReply, &QNetworkReply::uploadProgress, this,
            [this](qint64 bytesSent, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    const int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
                    emit uploadProgress(percent);
                }
            });

    connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply]() {
        const QByteArray response = uploadReply->readAll();
        const bool hadError = uploadReply->error() != QNetworkReply::NoError;
        const QString errorString = uploadReply->errorString();
        const QVariant statusAttr =
            uploadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpStatus = statusAttr.isValid() ? statusAttr.toInt() : 0;
        uploadReply->deleteLater();

        if (!StudentController::replyIsServerAnswer(hadError, httpStatus, response)) {
            emit uploadFailed(errorString);                    // genuine transport failure
            return;
        }
        if (httpStatus == 401) {
            emit uploadFailed(QStringLiteral(
                "Admin authentication required — re-enter via admin login."));
            return;
        }
        emit uploadFinished(parseUploadResponse(response));    // real JSON counts
    });
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
    const int skippedCount     = obj[QLatin1String("skipped_count")].toInt();
    const int errorCount       = obj[QLatin1String("error_count")].toInt();

    UploadResult result;
    result.message      = message;
    result.successCount = successCount;
    result.skippedCount = skippedCount;
    result.errorCount   = errorCount;
    result.plainText    = false;
    result.ok           = (status == QLatin1String("success"));
    return result;
}
