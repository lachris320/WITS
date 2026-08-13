#ifndef IMPORTCONTROLLER_H
#define IMPORTCONTROLLER_H
#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include "importdata.h"

class QNetworkAccessManager;

class ImportController : public QObject
{
    Q_OBJECT
public:
    explicit ImportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeHeader(const QString &raw);
    static void    mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut);
    static ParsedTable parseCsv(const QString &rawText);

    // Pure serializer (4a.3): the parsed table -> a compact JSON array of
    // 7-key row objects (school_id/name/course/department/year_level/gender/
    // status), mapped through table.headerIndex. code/visits/unrecognized
    // columns are excluded; each value is trimmed; a missing/out-of-range
    // column contributes "". Empty table -> "[]". Primary upload-body seam.
    static QByteArray serializeRows(const ParsedTable &table);

    // Pure client-side validation (4a.3, spec §7). Returns "" when importable;
    // else a friendly message. Requires school_id + name columns (else names
    // the found columns); each data row must have a non-empty school_id (empty
    // ones are appended to *badRowsOut as "Row N", 1-based over data rows).
    // Extra columns are ignored, never an error.
    static QString validateForImport(const ParsedTable &table,
                                     QStringList *badRowsOut = nullptr);

    // Pure sample-CSV generator for the "Download Template" button (spec §7).
    // Header line of the recognized columns + one synthetic example row (opaque
    // hyphenated school_id like 21-1-0001; no real PII). UTF-8, no BOM.
    static QByteArray importTemplateCsv();

    // Synchronous QXlsx parse. Requires a QGuiApplication (see Testing).
    // errorOut (when non-null) reports which of the three legacy failure
    // cases occurred so the View can show the exact original dialog.
    ParsedTable parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr);

    // Async — result arrives via duplicatesResolved / importError. adminKey is
    // sent as a form field (check_duplicates.php is requireAdminAuth-guarded);
    // a 401-with-body routes to importError with a clear auth message.
    void checkDuplicates(const QStringList &schoolIds, const QString &adminKey);

    // Async — result arrives via uploadStarted / uploadProgress /
    // uploadFinished / uploadFailed. Serializes `table` to a `rows` JSON form
    // field (via serializeRows) + admin_key + optional skip_ids + optional
    // photos_zip file. A 401-with-body routes to uploadFailed(auth message).
    void uploadStudents(const ParsedTable &table, const QString &zipPath,
                        const QStringList &skipIds, const QString &adminKey);

    // Pure response parsers
    static bool parseDuplicateResponse(const QByteArray &raw,
                                       QStringList *duplicatesOut,
                                       QString *errorOut);
    static UploadResult parseUploadResponse(const QByteArray &raw);

signals:
    void duplicatesResolved(const QStringList &duplicates);   // empty = none found
    void importError(const QString &title, const QString &message, ImportSeverity severity);
    void uploadStarted();                        // request about to post
    void uploadProgress(int percent);
    void uploadFinished(const UploadResult &result);
    void uploadFailed(const QString &message);   // always critical, title "Upload Failed"

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // IMPORTCONTROLLER_H
