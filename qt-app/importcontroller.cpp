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

void ImportController::checkDuplicates(const QStringList &schoolIds)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("check_duplicates.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    QJsonArray idsArray;
    for (const QString &id : schoolIds)
        idsArray.append(id);

    QJsonObject payload;
    payload[QLatin1String("school_ids")] = idsArray;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());

    // `this` (the controller) as the context object is mandatory: the
    // connection auto-disconnects if the controller is destroyed while the
    // reply — owned by the shared QNetworkAccessManager — is still in flight.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            // Legacy uses "Error" as the title here, not "Network Error"
            // (adminwindow.cpp:1180) — preserved verbatim, not unified with
            // VisitorController's "Network Error" convention.
            emit importError(QStringLiteral("Error"), reply->errorString(),
                             ImportSeverity::Critical);
            reply->deleteLater();
            return;
        }

        const QByteArray resp = reply->readAll();
        reply->deleteLater();

        QStringList duplicates;
        QString errorMsg;
        if (!parseDuplicateResponse(resp, &duplicates, &errorMsg)) {
            emit importError(QStringLiteral("Error"), errorMsg, ImportSeverity::Warning);
            return;
        }

        emit duplicatesResolved(duplicates);   // empty list = no duplicates found
    });
}

void ImportController::uploadStudents(const QString &excelPath, const QString &zipPath,
                                      const QStringList &skipIds)
{
    QNetworkRequest uploadRequest(ApiConfig::endpoint(QStringLiteral("upload_students_zip.php")));
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // Excel part (mandatory, fatal on failure).
    QHttpPart excelPart;
    excelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"excel\"; filename=\"" +
                                 QFileInfo(excelPath).fileName() + "\""));
    QFile *excelFile = new QFile(excelPath);
    if (!excelFile->open(QIODevice::ReadOnly)) {
        // Fatal, exactly as legacy (adminwindow.cpp:1270-1278): free what was
        // already allocated and abort before sending anything. uploadStarted
        // never fires here, so the View never sets progress/button state —
        // nothing to revert, reproducing legacy's set-then-revert net effect.
        emit importError(QStringLiteral("Error"), QStringLiteral("Cannot open Excel file."),
                         ImportSeverity::Critical);
        delete excelFile;
        delete multiPart;
        return;
    }
    excelPart.setBodyDevice(excelFile);
    excelFile->setParent(multiPart);
    multiPart->append(excelPart);

    // The excel file opened OK — this is the exact point legacy passed line
    // 1270 and had already set the progress/button state at lines 1251-1258.
    // The View sets that state in its onUploadStarted slot.
    emit uploadStarted();

    // ZIP part (optional, non-fatal on failure).
    if (!zipPath.isEmpty()) {
        QHttpPart zipPart;
        zipPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"photos_zip\"; filename=\"" +
                                   QFileInfo(zipPath).fileName() + "\""));
        QFile *zipFile = new QFile(zipPath);
        if (!zipFile->open(QIODevice::ReadOnly)) {
            // Non-fatal as legacy (adminwindow.cpp:1291): warn and continue
            // the upload without the ZIP part. The delete below fixes a
            // pre-existing zipFile leak (legacy never freed it on this
            // branch); no observable behavior change.
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

    // skip_ids part — only appended when non-empty (adminwindow.cpp:1300-1306).
    if (!skipIds.isEmpty()) {
        QHttpPart dupPart;
        dupPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"skip_ids\""));
        dupPart.setBody(skipIds.join(",").toUtf8());
        multiPart->append(dupPart);
    }

    QNetworkReply *uploadReply = m_nam->post(uploadRequest, multiPart);
    multiPart->setParent(uploadReply);

    connect(uploadReply, &QNetworkReply::uploadProgress, this,
            [this](qint64 bytesSent, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    const int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
                    emit uploadProgress(percent);
                }
            });

    // `this` (the controller) as the context object — same rationale as
    // checkDuplicates and VisitorController::fetchVisitors.
    connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply]() {
        if (uploadReply->error() != QNetworkReply::NoError) {
            emit uploadFailed(uploadReply->errorString());
            uploadReply->deleteLater();
            return;
        }

        const QByteArray response = uploadReply->readAll();
        uploadReply->deleteLater();

        emit uploadFinished(parseUploadResponse(response));
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
    const int errorCount       = obj[QLatin1String("error_count")].toInt();

    UploadResult result;
    result.message      = message;
    result.successCount = successCount;
    result.errorCount   = errorCount;
    result.plainText    = false;
    result.ok           = (status == QLatin1String("success"));
    return result;
}
