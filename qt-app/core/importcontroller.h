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

    // Synchronous QXlsx parse. Requires a QGuiApplication (see Testing).
    // errorOut (when non-null) reports which of the three legacy failure
    // cases occurred so the View can show the exact original dialog.
    ParsedTable parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr);

    // Async — result arrives via duplicatesResolved / importError. adminKey is
    // sent as a form field (check_duplicates.php is requireAdminAuth-guarded);
    // a 401-with-body routes to importError with a clear auth message.
    void checkDuplicates(const QStringList &schoolIds, const QString &adminKey);

    // Async — result arrives via uploadStarted / uploadProgress / uploadFinished / uploadFailed.
    void uploadStudents(const QString &excelPath, const QString &zipPath,
                        const QStringList &skipIds);

    // Pure response parsers
    static bool parseDuplicateResponse(const QByteArray &raw,
                                       QStringList *duplicatesOut,
                                       QString *errorOut);
    static UploadResult parseUploadResponse(const QByteArray &raw);

signals:
    void duplicatesResolved(const QStringList &duplicates);   // empty = none found
    void importError(const QString &title, const QString &message, ImportSeverity severity);
    void uploadStarted();                        // excel file opened OK; request about to post
    void uploadProgress(int percent);
    void uploadFinished(const UploadResult &result);
    void uploadFailed(const QString &message);   // always critical, title "Upload Failed"

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};

#endif // IMPORTCONTROLLER_H
